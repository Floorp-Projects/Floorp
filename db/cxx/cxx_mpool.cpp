/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)cxx_mpool.cpp	10.9 (Sleepycat) 5/2/98";
#endif /* not lint */

#include "db_cxx.h"
#include "cxx_int.h"
#include <errno.h>

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbMpoolFile                             //
//                                                                    //
////////////////////////////////////////////////////////////////////////

DbMpoolFile::DbMpoolFile()
:   imp_(0)
{
}

DbMpoolFile::~DbMpoolFile()
{
}

int DbMpoolFile::open(DbMpool *mp, const char *file,
                      u_int32_t flags, int mode, size_t pagesize,
                      DB_MPOOL_FINFO *finfop, DbMpoolFile **result)
{
    int err;

    DB_MPOOLFILE *mpf;
    if ((err = memp_fopen(unwrap(mp), file, flags, mode, pagesize,
                          finfop, &mpf)) != 0) {
        DB_ERROR("DbMpoolFile::open", err);
        return err;
    }
    *result = new DbMpoolFile();
    (*result)->imp_ = wrap(mpf);
    return 0;
}

int DbMpoolFile::close()
{
    DB_MPOOLFILE *mpf = unwrap(this);
    int err = 0;
    if (!mpf) {
        err = EINVAL;
    }
    else if ((err = memp_fclose(mpf)) != 0) {
        DB_ERROR("DbMpoolFile::close", err);
        return err;
    }
    imp_ = 0;                   // extra safety

    // This may seem weird, but is legal as long as we don't access
    // any data before returning.
    //
    delete this;
    return 0;
}

int DbMpoolFile::get(db_pgno_t *pgnoaddr, u_int32_t flags, void *pagep)
{
    DB_MPOOLFILE *mpf = unwrap(this);
    int err = 0;
    if (!mpf) {
        err = EINVAL;
    }
    else if ((err = memp_fget(mpf, pgnoaddr, flags, pagep)) != 0) {
        DB_ERROR("DbMpoolFile::get", err);
    }
    return err;
}

int DbMpoolFile::put(void *pgaddr, u_int32_t flags)
{
    DB_MPOOLFILE *mpf = unwrap(this);
    int err = 0;
    if (!mpf) {
        err = EINVAL;
    }
    else if ((err = memp_fput(mpf, pgaddr, flags)) != 0) {
        DB_ERROR("DbMpoolFile::put", err);
    }
    return err;
}

int DbMpoolFile::set(void *pgaddr, u_int32_t flags)
{
    DB_MPOOLFILE *mpf = unwrap(this);
    int err = 0;
    if (!mpf) {
        err = EINVAL;
    }
    else if ((err = memp_fset(mpf, pgaddr, flags)) != 0) {
        DB_ERROR("DbMpoolFile::set", err);
    }
    return err;
}

int DbMpoolFile::sync()
{
    DB_MPOOLFILE *mpf = unwrap(this);
    int err = 0;
    if (!mpf) {
        err = EINVAL;
    }
    else if ((err = memp_fsync(mpf)) != 0) {
        DB_ERROR("DbMpoolFile::sync", err);
    }
    return err;
}

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbMpool                                 //
//                                                                    //
////////////////////////////////////////////////////////////////////////

DbMpool::DbMpool()
:   imp_(0)
{
}

DbMpool::~DbMpool()
{
}

int DbMpool::close()
{
    DB_MPOOL *mpool = unwrap(this);
    int err;

    if (!mpool) {
        return EINVAL;        // already closed
    }

    if ((err = memp_close(mpool)) != 0) {
        DB_ERROR("DbMpool::close", err);
        return err;
    }
    imp_ = 0;                   // extra safety

    // This may seem weird, but is legal as long as we don't access
    // any data before returning.
    //
    delete this;
    return 0;
}

int DbMpool::db_register(int ftype,
                     int (*pgin)(db_pgno_t pgno, void *pgaddr, DBT *pgcookie),
                     int (*pgout)(db_pgno_t pgno, void *pgaddr, DBT *pgcookie))
{
    DB_MPOOL *mpool = unwrap(this);
    int err = 0;
    if (!mpool) {
        err = EINVAL;
    }
    else if ((err = memp_register(mpool, ftype, pgin, pgout)) != 0) {
        DB_ERROR("DbMpool::db_register", err);
        return err;
    }
    return err;
}

int DbMpool::stat(DB_MPOOL_STAT **gsp, DB_MPOOL_FSTAT ***fsp,
                     void *(*alternate_malloc)(size_t))
{
    DB_MPOOL *mpool = unwrap(this);
    int err = 0;
    if (!mpool) {
        err = EINVAL;
    }
    else if ((err = memp_stat(mpool, gsp, fsp, alternate_malloc)) != 0) {
        DB_ERROR("DbMpool::stat", err);
        return err;
    }
    return err;
}

int DbMpool::sync(DbLsn *sn)
{
    DB_MPOOL *mpool = unwrap(this);
    int err = 0;
    if (!mpool) {
        err = EINVAL;
    }
    else if ((err = memp_sync(mpool, sn)) != 0) {
        DB_ERROR("DbMpool::sync", err);
        return err;
    }
    return err;
}

int DbMpool::trickle(int pct, int *nwrotep)
{
    DB_MPOOL *mpool = unwrap(this);
    int err = 0;
    if (!mpool) {
        err = EINVAL;
    }
    else if ((err = memp_trickle(mpool, pct, nwrotep)) != 0) {
        DB_ERROR("DbMpool::trickle", err);
        return err;
    }
    return err;
}
// static method
int DbMpool::open(const char *dir, u_int32_t flags, int mode,
                  DbEnv *dbenv, DbMpool **regionp)
{
    *regionp = 0;
    DB_MPOOL *result = 0;
    int err;
    if ((err = memp_open(dir, flags, mode, dbenv, &result)) != 0) {
        DB_ERROR("DbMpool::open", err);
        return err;
    }

    *regionp = new DbMpool();
    (*regionp)->imp_ = wrap(result);
    return 0;
}

// static method
int DbMpool::unlink(const char *dir, int force, DbEnv *dbenv)
{
    int err;
    if ((err = memp_unlink(dir, force, dbenv)) != 0) {
        DB_ERROR("DbMpool::unlink", err);
        return err;
    }
    return err;
}
