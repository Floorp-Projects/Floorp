/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)java_DbTxnMgr.cpp	10.2 (Sleepycat) 4/10/98";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"
#include "com_sleepycat_db_DbTxnMgr.h"

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_DbTxnMgr_begin
  (JNIEnv *jnienv, jobject jthis, /*DbTxn*/ jobject pid)
{
    int err;
    DB_TXNMGR *dbtxnmgr = get_DB_TXNMGR(jnienv, jthis);
    if (!verify_non_null(jnienv, dbtxnmgr))
        return 0;
    DB_TXN *dbpid = get_DB_TXN(jnienv, pid);
    DB_TXN *result = 0;

    err = txn_begin(dbtxnmgr, dbpid, &result);
    if (!verify_return(jnienv, err))
        return 0;
    return get_DbTxn(jnienv, result);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbTxnMgr_checkpoint
  (JNIEnv *jnienv, jobject jthis, jint kbyte, jint min)
{
    int err;
    DB_TXNMGR *dbtxnmgr = get_DB_TXNMGR(jnienv, jthis);

    if (!verify_non_null(jnienv, dbtxnmgr))
        return;
    err = txn_checkpoint(dbtxnmgr, kbyte, min);
    verify_return(jnienv, err);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbTxnMgr_close
  (JNIEnv *jnienv, jobject jthis)
{
    int err;
    DB_TXNMGR *dbtxnmgr = get_DB_TXNMGR(jnienv, jthis);

    if (!verify_non_null(jnienv, dbtxnmgr))
        return;
    err = txn_close(dbtxnmgr);
    if (verify_return(jnienv, err))
    {
        set_private_info(jnienv, name_DB_TXNMGR, jthis, 0);
    }
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_DbTxnMgr_stat
  (JNIEnv *jnienv, jobject jthis)
{
    int err;
    DB_TXNMGR *dbtxnmgr = get_DB_TXNMGR(jnienv, jthis);
    DB_TXN_STAT *statp = 0;

    err = txn_stat(dbtxnmgr, &statp, 0);
    if (!verify_return(jnienv, err))
        return 0;
    return get_DbTxnStat(jnienv, statp);
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_DbTxnMgr_open
  (JNIEnv *jnienv, jclass jthis_class, jstring dir,
   jint flags, jint mode, jobject dbenv)
{
    int err;
    jobject retval = NULL;
    DB_TXNMGR *dbtxnmgr;
    DB_ENV *db_dbenv = get_DB_ENV(jnienv, dbenv);
    LockedString dbdir(jnienv, dir);

    if (verify_non_null(jnienv, db_dbenv)) {
        err = txn_open(dbdir.string, flags, mode, db_dbenv, &dbtxnmgr);
        if (verify_return(jnienv, err)) {
            retval = create_default_object(jnienv, name_DB_TXNMGR);
            set_private_info(jnienv, name_DB_TXNMGR, retval, dbtxnmgr);
        }
    }
    return retval;
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbTxnMgr_unlink
  (JNIEnv *jnienv, jclass jthis_class, jstring dir, jint force,
   /*DbEnv*/ jobject dbenv)
{
    int err;
    DB_ENV *db_dbenv = get_DB_ENV(jnienv, dbenv);
    LockedString dbdir(jnienv, dir);

    if (verify_non_null(jnienv, db_dbenv)) {
        err = txn_unlink(dbdir.string, force, db_dbenv);
        verify_return(jnienv, err);
    }
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbTxnMgr_finalize
  (JNIEnv *jnienv, jobject jthis)
{
    DB_TXNMGR *dbtxnmgr = get_DB_TXNMGR(jnienv, jthis);
    if (dbtxnmgr) {
        // Free any data related to DB_TXNMGR here
    }
}
