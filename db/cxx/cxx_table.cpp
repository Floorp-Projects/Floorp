/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)cxx_table.cpp	10.11 (Sleepycat) 4/10/98";
#endif /* not lint */

#include "db_cxx.h"
#include "cxx_int.h"
#include <errno.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            Db                                      //
//                                                                    //
////////////////////////////////////////////////////////////////////////

Db::Db()
:   imp_(0)
{
}

Db::~Db()
{
}

int Db::close(u_int32_t flags)
{
    DB *db = unwrap(this);
    if (!db) {
        DB_ERROR("Db::close", EINVAL);
        return EINVAL;
    }
    int err;
    if ((err = db->close(db, flags)) != 0) {
        DB_ERROR("Db::close", err);
        return err;
    }
    imp_ = 0;                   // extra safety

    // This may seem weird, but is legal as long as we don't access
    // any data before returning.
    //
    delete this;
    return 0;
}

int Db::cursor(DbTxn *txnid, Dbc **cursorp)
{
    DB *db = unwrap(this);
    int err;

    if (!db) {
        DB_ERROR("Db::cursor", EINVAL);
        return EINVAL;
    }
    DBC *dbc = 0;
    if ((err = db->cursor(db, unwrap(txnid), &dbc)) != 0) {
        DB_ERROR("Db::cursor", err);
        return err;
    }

    // The following cast implies that Dbc can be no larger than DBC
    *cursorp = (Dbc*)dbc;
    return 0;
}

int Db::del(DbTxn *txnid, Dbt *key, u_int32_t flags)
{
    DB *db = unwrap(this);
    int err;

    if (!db) {
        DB_ERROR("Db::del", EINVAL);
        return EINVAL;
    }
    if ((err = db->del(db, unwrap(txnid), key, flags)) != 0) {
        DB_ERROR("Db::del", err);
        return err;
    }
    return 0;
}

int Db::fd(int *fdp)
{
    DB *db = unwrap(this);
    if (!db) {
        DB_ERROR("Db::fd", EINVAL);
        return EINVAL;
    }
    int err;
    if ((err = db->fd(db, fdp)) != 0) {
        DB_ERROR("Db::fd", err);
        return err;
    }
    return 0;
}

int Db::get(DbTxn *txnid, Dbt *key, Dbt *value, u_int32_t flags)
{

    DB *db = unwrap(this);
    int err;

    if (!db) {
        DB_ERROR("Db::get", EINVAL);
        return EINVAL;
    }
    if ((err = db->get(db, unwrap(txnid), key, value, flags)) != 0) {
        // DB_NOTFOUND is a "normal" return, so should not be
        // thrown as an error
        //
        if (err != DB_NOTFOUND) {
            DB_ERROR("Db::get", err);
            return err;
        }
    }
    return err;
}

// static method
int Db::open(const char *fname, DBTYPE type, u_int32_t flags,
             int mode, DbEnv *dbenv, DbInfo *info, Db **table_returned)
{
    *table_returned = 0;
    DB *newtable;
    int err;
    if ((err = db_open(fname, type, flags, mode, dbenv,
                       info, &newtable)) != 0) {
        DB_ERROR("Db::open", err);
        return err;
    }
    *table_returned = new Db();
    (*table_returned)->imp_ = wrap(newtable);
    return 0;
}

int Db::put(DbTxn *txnid, Dbt *key, Dbt *value, u_int32_t flags)
{
    DB *db = unwrap(this);
    int err;

    if (!db) {
        DB_ERROR("Db::put", EINVAL);
        return EINVAL;
    }
    if ((err = db->put(db, unwrap(txnid), key, value, flags)) != 0) {

        // DB_KEYEXIST is a "normal" return, so should not be
        // thrown as an error
        //
        if (err != DB_KEYEXIST) {
            DB_ERROR("Db::put", err);
            return err;
        }
    }
    return err;
}

int Db::stat(void *sp, void *(*db_malloc)(size_t), u_int32_t flags)
{
    DB *db = unwrap(this);
    if (!db) {
        DB_ERROR("Db::stat", EINVAL);
        return EINVAL;
    }
    int err;
    if ((err = db->stat(db, sp, db_malloc, flags)) != 0) {
        DB_ERROR("Db::stat", err);
        return err;
    }
    return 0;
}

int Db::sync(u_int32_t flags)
{
    DB *db = unwrap(this);
    if (!db) {
        DB_ERROR("Db::sync", EINVAL);
        return EINVAL;
    }
    int err;
    if ((err = db->sync(db, flags)) != 0) {
        DB_ERROR("Db::sync", err);
        return err;
    }
    return 0;
}

DBTYPE Db::get_type() const
{
    const DB *db = unwrapConst(this);
    return db->type;
}


////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            Dbc                                     //
//                                                                    //
////////////////////////////////////////////////////////////////////////

// It's private, and should never be called, but VC4.0 needs it resolved
//
Dbc::~Dbc()
{
}

int Dbc::close()
{
    DBC *cursor = this;
    int err;

    if ((err = cursor->c_close(cursor)) != 0) {
        DB_ERROR("Db::close", err);
        return err;
    }
    return 0;
}

int Dbc::del(u_int32_t flags)
{
    DBC *cursor = this;
    int err;

    if ((err = cursor->c_del(cursor, flags)) != 0) {
        DB_ERROR("Db::del", err);
        return err;
    }
    return 0;
}

int Dbc::get(Dbt* key, Dbt *data, u_int32_t flags)
{
    DBC *cursor = this;
    int err;

    if ((err = cursor->c_get(cursor, key, data, flags)) != 0) {
        if (err != DB_NOTFOUND) {
            DB_ERROR("Db::get", err);
            return err;
        }
    }
    return err;
}

int Dbc::put(Dbt* key, Dbt *data, u_int32_t flags)
{
    DBC *cursor = this;
    int err;

    if ((err = cursor->c_put(cursor, key, data, flags)) != 0) {
        DB_ERROR("Db::put", err);
        return err;
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////
//                                                                    //
//                            Dbt                                 //
//                                                                    //
////////////////////////////////////////////////////////////////////////

Dbt::Dbt()
{
    DBT *dbt = this;
    memset(dbt, 0, sizeof(DBT));
}

Dbt::Dbt(void *data_arg, size_t size_arg)
{
    DBT *dbt = this;
    memset(dbt, 0, sizeof(DBT));
    set_data(data_arg);
    set_size(size_arg);
}

Dbt::~Dbt()
{
}

Dbt::Dbt(const Dbt &that)
{
    const DBT *from = &that;
    DBT *to = this;
    memcpy(to, from, sizeof(DBT));
}

Dbt &Dbt::operator = (const Dbt &that)
{
    const DBT *from = &that;
    DBT *to = this;
    memcpy(to, from, sizeof(DBT));
    return *this;
}

DB_RW_ACCESS(Dbt, void *, data, data)
DB_RW_ACCESS(Dbt, u_int32_t, size, size)
DB_RW_ACCESS(Dbt, u_int32_t, ulen, ulen)
DB_RW_ACCESS(Dbt, u_int32_t, dlen, dlen)
DB_RW_ACCESS(Dbt, u_int32_t, doff, doff)
DB_RW_ACCESS(Dbt, u_int32_t, flags, flags)
