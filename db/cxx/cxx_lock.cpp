/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)cxx_lock.cpp	10.7 (Sleepycat) 4/10/98";
#endif /* not lint */

#include "db_cxx.h"
#include "cxx_int.h"
#include <errno.h>

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbLockTab                               //
//                                                                    //
////////////////////////////////////////////////////////////////////////

DbLockTab::DbLockTab()
:   imp_(0)
{
}

DbLockTab::~DbLockTab()
{
}

int DbLockTab::close()
{
    DB_LOCKTAB *locktab = unwrap(this);
    int err;

    if (!locktab) {
        return EINVAL;        // handle never assigned
    }

    if ((err = lock_close(locktab)) != 0) {
        DB_ERROR("DbLockTab::close", err);
        return err;
    }
    imp_ = 0;                   // extra safety

    // This may seem weird, but is legal as long as we don't access
    // any data before returning.
    //
    delete this;
    return 0;
}

int DbLockTab::detect(u_int32_t flags, int atype)
{
    DB_LOCKTAB *locktab = unwrap(this);

    if (!locktab) {
        return EINVAL;        // handle never assigned
    }

    int err = 0;
    if ((err = lock_detect(locktab, flags, atype)) != 0) {
        DB_ERROR("DbLockTab::detect", err);
        return err;
    }
    return err;
}

int DbLockTab::get(u_int32_t locker, u_int32_t flags, const Dbt *obj,
                   db_lockmode_t lock_mode, DbLock *lock)
{
    DB_LOCKTAB *locktab = unwrap(this);

    if (!locktab) {
        return EINVAL;        // handle never assigned
    }

    int err = 0;
    if ((err = lock_get(locktab, locker, flags, obj,
                        lock_mode, &lock->lock_)) != 0) {
        DB_ERROR("DbLockTab::get", err);
        return err;
    }
    return err;
}

int DbLockTab::id(u_int32_t *idp)
{
    DB_LOCKTAB *locktab = unwrap(this);

    if (!locktab) {
        return EINVAL;        // handle never assigned
    }

    int err;
    if ((err = lock_id(locktab, idp)) != 0) {
        DB_ERROR("DbLockTab::id", err);
    }
    return err;
}

int DbLockTab::vec(u_int32_t locker, u_int32_t flags,
                   DB_LOCKREQ list[],
                   int nlist, DB_LOCKREQ **elist_returned)
{
    DB_LOCKTAB *locktab = unwrap(this);

    if (!locktab) {
        return EINVAL;        // handle never assigned
    }

    int err;
    if ((err = lock_vec(locktab, locker, flags, list,
                        nlist, elist_returned)) != 0) {
        DB_ERROR("DbLockTab::vec", err);
        return err;
    }
    return err;
}

// static method
int DbLockTab::open(const char *dir, u_int32_t flags, int mode,
                  DbEnv *dbenv, DbLockTab **regionp)
{
    *regionp = 0;
    DB_LOCKTAB *result = 0;
    int err;
    if ((err = lock_open(dir, flags, mode, dbenv, &result)) != 0) {
        DB_ERROR("DbLockTab::open", err);
        return err;
    }

    *regionp = new DbLockTab();
    (*regionp)->imp_ = wrap(result);
    return 0;
}

// static method
int DbLockTab::unlink(const char *dir, int force, DbEnv *dbenv)
{
    int err;
    if ((err = lock_unlink(dir, force, dbenv)) != 0) {
        DB_ERROR("DbLockTab::unlink", err);
        return err;
    }
    return err;
}

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbLock                                  //
//                                                                    //
////////////////////////////////////////////////////////////////////////

DbLock::DbLock(unsigned int value)
:   lock_(value)
{
}

DbLock::DbLock()
:   lock_(0)
{
}

DbLock::DbLock(const DbLock &that)
:   lock_(that.lock_)
{
}

DbLock &DbLock::operator = (const DbLock &that)
{
    lock_ = that.lock_;
    return *this;
}

unsigned int DbLock::get_lock_id()
{
    return lock_;
}

void DbLock::set_lock_id(unsigned int value)
{
    lock_ = value;
}

int DbLock::put(DbLockTab *locktab)
{
    DB_LOCKTAB *db_locktab = unwrap(locktab);

    if (!db_locktab) {
        return EINVAL;        // handle never assigned
    }

    int err;
    if ((err = lock_put(db_locktab, lock_)) != 0) {
        DB_ERROR("DbLock::put", err);
    }
    return err;
}
