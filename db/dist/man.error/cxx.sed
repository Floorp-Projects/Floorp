# @(#)cxx.sed	10.1 (Sleepycat) 9/18/97
#
# Corrections for the C++ wrapper for DB.

# DB
s/DB->/Db::/
s/DBcursor->c_/Dbc::/

s/db_appexit/DbEnv::appexit/
s/db_appinit/DbEnv::appinit/

# Lock
s/lock_detect/DbLock::detect/
s/lock_get/DbLock::get/
s/lock_put/DbLock::put/
s/lock_unlink/DbLock::unlink/

s/lock_close/DbLockTab::close/
s/lock_id/DbLockTab::id/
s/lock_open/DbLockTab::open/
s/lock_vec/DbLockTab::vec/

# Log
s/log_archive/DbLog::archive/
s/log_close/DbLog::close/
s/log_compare/DbLog::compare/
s/log_file/DbLog::file/
s/log_flush/DbLog::flush/
s/log_get/DbLog::get/
s/log_open/DbLog::open/
s/log_put/DbLog::put/
s/log_register/DbLog::db_register/
s/log_unlink/DbLog::unlink/
s/log_unregister/DbLog::db_register/

# Mpool
s/memp_close/DbMpool::close/
s/memp_open/DbMpool::open/
s/memp_register/DbMpool::db_register/
s/memp_stat/DbMpool::stat/
s/memp_sync/DbMpool::sync/
s/memp_unlink/DbMpool::unlink/

s/memp_fclose/DbMpoolFile::close/
s/memp_fget/DbMpoolFile::get/
s/memp_fopen/DbMpoolFile::open/
s/memp_fput/DbMpoolFile::put/
s/memp_fset/DbMpoolFile::set/
s/memp_fsync/DbMpoolFile::sync/

# Txn
s/txn_abort/DbTxn::abort/
s/txn_commit/DbTxn::commit/
s/txn_id/DbTxn::id/
s/txn_prepare/DbTxn::prepare/

s/txn_begin/DbTxnMgr::begin/
s/txn_checkpoint/DbTxnMgr::checkpoint/
s/txn_close/DbTxnMgr::close/
s/txn_open/DbTxnMgr::open/
s/txn_stat/DbTxnMgr::stat/
s/txn_unlink/DbTxnMgr::unlink/
