/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)cxx_app.cpp	10.14 (Sleepycat) 4/10/98";
#endif /* not lint */

#include "db_cxx.h"
#include "cxx_int.h"

#include <string.h>
#include <iostream.h>
#include <fstream.h>
#include <stdio.h>              // needed for setErrorStream

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbEnv                                   //
//                                                                    //
////////////////////////////////////////////////////////////////////////

static DbEnv *currentApp = 0;

ostream *DbEnv::error_stream_ = 0;

DbEnv::DbEnv(const char *homeDir, char *const *db_config, u_int32_t flags_arg)
:   error_model_(Exception)
{
    DB_ENV *env = this;
    memset(env, 0, sizeof(DB_ENV));

    int err;

    if ((err = db_appinit(homeDir, db_config, env, flags_arg)) != 0) {
        DB_ERROR("DbEnv::DbEnv", err);
    }
    currentApp = this;
}

DbEnv::DbEnv()
:   error_model_(Exception)
{
    DB_ENV *env = this;
    memset(env, 0, sizeof(DB_ENV));
}

DbEnv::~DbEnv()
{
    if (currentApp == this)
        currentApp = 0;
    DB_ENV *env = this;

    // having a zeroed environment is a signal that
    // appexit() has already been done.
    //
    DB_ENV zeroed;
    memset(&zeroed, 0, sizeof(DB_ENV));
    if (memcmp(&zeroed, env, sizeof(DB_ENV)) != 0) {
        (void)appexit();        // ignore error return
    }
}

int DbEnv::appinit(const char *homeDir, char *const *db_config, u_int32_t flags_arg)
{
    DB_ENV *env = this;

    int err;

    if ((err = db_appinit(homeDir, db_config, env, flags_arg)) != 0) {
        DB_ERROR("DbEnv::appinit", err);
    }
    currentApp = this;
    return err;
}

int DbEnv::appexit()
{
    DB_ENV *env = this;

    int err;

    if ((err = db_appexit(env)) != 0) {
        DB_ERROR("DbEnv::appexit", err);
    }
    memset(env, 0, sizeof(DB_ENV));
    currentApp = 0;
    return err;
}

void DbEnv::set_error_model(ErrorModel model)
{
    error_model_ = model;
}

int DbEnv::runtime_error(const char *caller, int err, int in_destructor)
{
    int throwit = (!currentApp ||
                   (currentApp && currentApp->error_model_ == Exception));

    if (throwit && !in_destructor) {
        throw DbException(caller, err);
    }
    return err;
}

// Note: This actually behaves a bit like a static function,
// since DB_ENV.db_errcall has no information about which
// db_env triggered the call.  A user that has multiple DB_ENVs
// will simply not be able to have different streams for each one.
//
void DbEnv::set_error_stream(class ostream *stream)
{
    error_stream_ = stream;

    db_errcall = stream_error_function;
}

ostream *DbEnv::get_error_stream() const
{
    return error_stream_;
}

void DbEnv::stream_error_function(const char *prefix, char *message)
{
    if (error_stream_) {
        if (prefix) {
            (*error_stream_) << prefix << ": ";
        }
        if (message) {
            (*error_stream_) << message;
        }
        (*error_stream_) << "\n";
    }
}

DB_RW_ACCESS(DbEnv, int, lorder, db_lorder)
DB_RW_ACCESS(DbEnv, DbEnv::db_errcall_fcn, errcall, db_errcall)
DB_RW_ACCESS(DbEnv, FILE *, errfile, db_errfile)
DB_RW_ACCESS(DbEnv, const char *, errpfx, db_errpfx)
DB_RW_ACCESS(DbEnv, int, verbose, db_verbose)
DB_RW_ACCESS(DbEnv, char *, home, db_home)
DB_RW_ACCESS(DbEnv, char *, log_dir, db_log_dir)
DB_RW_ACCESS(DbEnv, char *, tmp_dir, db_tmp_dir)
DB_RW_ACCESS(DbEnv, char **, data_dir, db_data_dir)
DB_RW_ACCESS(DbEnv, int, data_cnt, data_cnt)
DB_RW_ACCESS(DbEnv, int, data_next, data_next)
DB_RW_ACCESS(DbEnv, u_int8_t *, lk_conflicts, lk_conflicts)
DB_RW_ACCESS(DbEnv, int, lk_modes, lk_modes)
DB_RW_ACCESS(DbEnv, unsigned int, lk_max, lk_max)
DB_RW_ACCESS(DbEnv, u_int32_t, lk_detect, lk_detect)
DB_RW_ACCESS(DbEnv, u_int32_t, lg_max, lg_max)
DB_RW_ACCESS(DbEnv, size_t, mp_mmapsize, mp_mmapsize)
DB_RW_ACCESS(DbEnv, size_t, mp_size, mp_size)
DB_RW_ACCESS(DbEnv, unsigned int, tx_max, tx_max)
DB_RW_ACCESS(DbEnv, DbEnv::tx_recover_fcn, tx_recover, tx_recover)
DB_RW_ACCESS(DbEnv, u_int32_t, flags, flags)

// These access methods require construction of
// wrapper options DB_FOO* to DbFoo* .
//

DbLockTab *DbEnv::get_lk_info() const
{
    if (!lk_info)
        return 0;
    DbLockTab *result = new DbLockTab();
    result->imp_ = wrap(lk_info);
    return result;
}

DbLog *DbEnv::get_lg_info() const
{
    if (!lg_info)
        return 0;
    DbLog *result = new DbLog();
    result->imp_ = wrap(lg_info);
    return result;
}

DbMpool *DbEnv::get_mp_info() const
{
    if (!mp_info)
        return 0;
    DbMpool *result = new DbMpool();
    result->imp_ = wrap(mp_info);
    return result;
}

DbTxnMgr *DbEnv::get_tx_info() const
{
    if (!tx_info)
        return 0;
    DbTxnMgr *result = new DbTxnMgr();
    result->imp_ = wrap(tx_info);
    return result;
}

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbInfo                                  //
//                                                                    //
////////////////////////////////////////////////////////////////////////

// Note: in theory, the db_home and db_*_dir fields will always be zero
// when managed by DbInfo.  That's because they are set by
// db_appinit, not by the user, and we make a copy of the db_env used by
// the application.
//

DbInfo::DbInfo()
{
    DB_INFO *info = this;
    memset(info, 0, sizeof(DB_INFO));
}

DbInfo::~DbInfo()
{
}

DbInfo::DbInfo(const DbInfo &that)
{
    DB_INFO *to = this;
    const DB_INFO *from = &that;
    memcpy(to, from, sizeof(DB_INFO));
}

DbInfo &DbInfo::operator = (const DbInfo &that)
{
    DB_INFO *to = this;
    const DB_INFO *from = &that;
    memcpy(to, from, sizeof(DB_INFO));
    return *this;
}

DB_RW_ACCESS(DbInfo, int, lorder, db_lorder)
DB_RW_ACCESS(DbInfo, size_t, cachesize, db_cachesize)
DB_RW_ACCESS(DbInfo, size_t, pagesize, db_pagesize)
DB_RW_ACCESS(DbInfo, DbInfo::db_malloc_fcn, malloc, db_malloc)
DB_RW_ACCESS(DbInfo, int, bt_maxkey, bt_maxkey)
DB_RW_ACCESS(DbInfo, int, bt_minkey, bt_minkey)
DB_RW_ACCESS(DbInfo, DbInfo::bt_compare_fcn, bt_compare, bt_compare)
DB_RW_ACCESS(DbInfo, DbInfo::bt_prefix_fcn, bt_prefix, bt_prefix)
DB_RW_ACCESS(DbInfo, unsigned int, h_ffactor, h_ffactor)
DB_RW_ACCESS(DbInfo, unsigned int, h_nelem, h_nelem)
DB_RW_ACCESS(DbInfo, DbInfo::h_hash_fcn, h_hash, h_hash)
DB_RW_ACCESS(DbInfo, int, re_pad, re_pad)
DB_RW_ACCESS(DbInfo, int, re_delim, re_delim)
DB_RW_ACCESS(DbInfo, u_int32_t, re_len, re_len)
DB_RW_ACCESS(DbInfo, char *, re_source, re_source)
DB_RW_ACCESS(DbInfo, u_int32_t, flags, flags)
