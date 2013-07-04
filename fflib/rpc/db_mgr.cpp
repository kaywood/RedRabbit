
#include "rpc/db_mgr.h"
using namespace ff;

db_mgr_t::db_mgr_t():
    m_db_index(0)
{
}
db_mgr_t::~db_mgr_t()
{
}

int db_mgr_t::start()
{
    for (size_t i = 0; i < 1; ++i)
    {
        m_tq.push_back(new task_queue_t());
        m_thread.create_thread(task_binder_t::gen(&task_queue_t::run, (m_tq[i]).get()), 1);
    }
    return 0;
}
int db_mgr_t::stop()
{
    for (size_t i = 0; i < m_tq.size(); ++i)
    {
        m_tq[i]->close();
    }
    m_thread.join();
    return 0;
}


long db_mgr_t::connect_db(const string& host_)
{
    shared_ptr_t<ffdb_t> db(new ffdb_t());
    if (db->connect(host_))
    {
        return 0;
    }
    long db_id = long(db.get());
    {
        lock_guard_t lock(m_mutex);
        db_connection_info_t& db_connection_info = m_db_connection[db_id];
        db_connection_info.db = db;
        db_connection_info.tq = m_tq[m_db_index++ % m_tq.size()];
    }
    return db_id;
}

void db_mgr_t::db_query(long db_id_,const string& sql_, ffslot_t::callback_t* callback_)
{
    db_connection_info_t* db_connection_info = NULL;
    {
        lock_guard_t lock(m_mutex);
        db_connection_info = &(m_db_connection[db_id_]);
    }
    if (NULL == db_connection_info)
    {
        db_query_result_t ret;
        callback_->exe(&ret);
        delete callback_;
        return;
    }
    else
    {
        db_connection_info->tq->produce(task_binder_t::gen(&db_mgr_t::db_query_impl, this, db_connection_info, sql_, callback_));
    }
}

void db_mgr_t::db_query_impl(db_connection_info_t* db_connection_info_, const string& sql_, ffslot_t::callback_t* callback_)
{
    db_connection_info_->ret.clear();
    if (0 == db_connection_info_->db->exe_sql(sql_, db_connection_info_->ret.result_data, db_connection_info_->ret.col_names))
    {
        db_connection_info_->ret.ok = true;
    }
    callback_->exe(&(db_connection_info_->ret));
    delete callback_;
}
