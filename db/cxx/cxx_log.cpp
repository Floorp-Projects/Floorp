/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)cxx_log.cpp	10.7 (Sleepycat) 4/10/98";
#endif /* not lint */

#include "db_cxx.h"
#include "cxx_int.h"
#include <errno.h>

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbLog                                   //
//                                                                    //
////////////////////////////////////////////////////////////////////////

DbLog::DbLog()
:   imp_(0)
{
}

DbLog::~DbLog()
{
}

int DbLog::archive(char **list[], u_int32_t flags, void *(*db_malloc)(size_t))
{
    int err;
    DB_LOG *log = unwrap(this);
    if ((err = log_archive(log, list, flags, db_malloc)) != 0) {
        DB_ERROR("DbLog::archive", err);
        return err;
    }
    return 0;
}

int DbLog::close()
{
    int err;
    DB_LOG *log = unwrap(this);
    if ((err = log_close(log)) != 0) {
        DB_ERROR("DbLog::close", err);
        return err;
    }
    imp_ = 0;                   // extra safety

    // This may seem weird, but is legal as long as we don't access
    // any data before returning.
    //
    delete this;
    return 0;
}

int DbLog::compare(const DbLsn *lsn0, const DbLsn *lsn1)
{
    return log_compare(lsn0, lsn1);
}

int DbLog::file(DbLsn *lsn, char *namep, int len)
{
    int err;
    DB_LOG *log = unwrap(this);
    if ((err = log_file(log, lsn, namep, len)) != 0) {
        DB_ERROR("DbLog::file", err);
        return err;
    }
    return 0;
}

int DbLog::flush(const DbLsn *lsn)
{
    int err;
    DB_LOG *log = unwrap(this);
    if ((err = log_flush(log, lsn)) != 0) {
        DB_ERROR("DbLog::flush", err);
        return err;
    }
    return 0;
}

int DbLog::get(DbLsn *lsn, Dbt *data, u_int32_t flags)
{
    int err;
    DB_LOG *log = unwrap(this);
    if ((err = log_get(log, lsn, data, flags)) != 0) {
        DB_ERROR("DbLog::get", err);
        return err;
    }
    return 0;
}

int DbLog::put(DbLsn *lsn, const Dbt *data, u_int32_t flags)
{
    int err = 0;
    DB_LOG *log = unwrap(this);
    if ((err = log_put(log, lsn, data, flags)) != 0) {
        DB_ERROR("DbLog::put", err);
        return err;
    }
    return 0;
}

int DbLog::db_register(Db *dbp, const char *name, DBTYPE type, u_int32_t *fidp)
{
    int err = 0;
    DB_LOG *log = unwrap(this);
    if ((err = log_register(log, unwrap(dbp), name, type, fidp)) != 0) {
        DB_ERROR("DbLog::db_register", err);
        return err;
    }
    return 0;
}

int DbLog::db_unregister(u_int32_t fid)
{
    int err;
    DB_LOG *log = unwrap(this);
    if ((err = log_unregister(log, fid)) != 0) {
        DB_ERROR("DbLog::db_unregister", err);
        return err;
    }
    return 0;
}

// static method
int DbLog::open(const char *dir, u_int32_t flags, int mode,
                  DbEnv *dbenv, DbLog **regionp)
{
    *regionp = 0;
    DB_LOG *result = 0;
    int err;
    if ((err = log_open(dir, flags, mode, dbenv, &result)) != 0) {
        DB_ERROR("DbLog::open", err);
        return err;
    }
    *regionp = new DbLog();
    (*regionp)->imp_ = wrap(result);
    return err;
}

// static method
int DbLog::unlink(const char *dir, int force, DbEnv *dbenv)
{
    int err;
    if ((err = log_unlink(dir, force, dbenv)) != 0) {
        DB_ERROR("DbLog::unlink", err);
        return err;
    }
    return err;
}
