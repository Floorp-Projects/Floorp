/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)cxx_txn.cpp	10.8 (Sleepycat) 4/10/98";
#endif /* not lint */

#include "db_cxx.h"
#include "cxx_int.h"
#include <errno.h>

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbTxnMgr                                //
//                                                                    //
////////////////////////////////////////////////////////////////////////

DbTxnMgr::DbTxnMgr()
:   imp_(0)
{
}

DbTxnMgr::~DbTxnMgr()
{
}

int DbTxnMgr::begin(DbTxn *pid, DbTxn **tid)
{
    int err;
    DB_TXNMGR *mgr = unwrap(this);
    DB_TXN *txn;

    if ((err = txn_begin(mgr, unwrap(pid), &txn)) != 0) {
        DB_ERROR("DbTxn::begin", err);
        return err;
    }
    DbTxn *result = new DbTxn();
    result->imp_ = wrap(txn);
    *tid = result;
    return err;
}

int DbTxnMgr::checkpoint(u_int32_t kbyte, u_int32_t min) const
{
    int err;
    const DB_TXNMGR *mgr = unwrapConst(this);
    if ((err = txn_checkpoint(mgr, kbyte, min)) != 0) {
        DB_ERROR("DbTxnMgr::checkpoint", err);
        return err;
    }
    return 0;
}

int DbTxnMgr::close()
{
    int err;
    DB_TXNMGR *mgr = unwrap(this);
    if ((err = txn_close(mgr)) != 0) {
        DB_ERROR("DbTxnMgr::close", err);
        return err;
    }
    imp_ = 0;                   // extra safety

    // This may seem weird, but is legal as long as we don't access
    // any data before returning.
    //
    delete this;
    return 0;
}

// static method
int DbTxnMgr::open(const char *dir, u_int32_t flags, int mode,
                  DbEnv *dbenv, DbTxnMgr **regionp)
{
    *regionp = 0;
    DB_TXNMGR *result = 0;
    int err;
    if ((err = txn_open(dir, flags, mode, dbenv, &result)) != 0) {
        DB_ERROR("DbTxnMgr::open", err);
        return err;
    }

    *regionp = new DbTxnMgr();
    (*regionp)->imp_ = wrap(result);
    return 0;
}

int DbTxnMgr::stat(DB_TXN_STAT **statp, void *(*db_malloc)(size_t))
{
    int err;
    DB_TXNMGR *mgr = unwrap(this);
    if ((err = txn_stat(mgr, statp, db_malloc)) != 0) {
        DB_ERROR("DbTxnMgr::stat", err);
        return err;
    }
    return 0;
}

// static method
int DbTxnMgr::unlink(const char *dir, int force, DbEnv *dbenv)
{
    int err;
    if ((err = txn_unlink(dir, force, dbenv)) != 0) {
        DB_ERROR("DbTxnMgr::unlink", err);
        return err;
    }
    return err;
}

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            DbTxn                                   //
//                                                                    //
////////////////////////////////////////////////////////////////////////

DbTxn::DbTxn()
:  imp_(0)
{
}

DbTxn::~DbTxn()
{
}

int DbTxn::abort()
{
    int err;
    DB_TXN *txn = unwrap(this);
    if ((err = txn_abort(txn)) != 0) {
        DB_ERROR("DbTxn::abort", err);
        return err;
    }

    // This may seem weird, but is legal as long as we don't access
    // any data before returning.
    //
    delete this;
    return 0;
}

int DbTxn::commit()
{
    int err;
    DB_TXN *txn = unwrap(this);
    if ((err = txn_commit(txn)) != 0) {
        DB_ERROR("DbTxn::commit", err);
        return err;
    }

    // This may seem weird, but is legal as long as we don't access
    // any data before returning.
    //
    delete this;
    return 0;
}

u_int32_t DbTxn::id()
{
    DB_TXN *txn = unwrap(this);
    return txn_id(txn);         // no error
}

int DbTxn::prepare()
{
    int err;
    DB_TXN *txn = unwrap(this);
    if ((err = txn_prepare(txn)) != 0) {
        DB_ERROR("DbTxn::prepare", err);
        return err;
    }
    return 0;
}
