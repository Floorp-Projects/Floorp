/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)java_DbTxn.cpp	10.2 (Sleepycat) 4/10/98";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"
#include "com_sleepycat_db_DbTxn.h"

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbTxn_abort
  (JNIEnv *jnienv, jobject jthis)
{
    int err;
    DB_TXN *dbtxn = get_DB_TXN(jnienv, jthis);
    if (!verify_non_null(jnienv, dbtxn))
        return;

    err = txn_abort(dbtxn);
    verify_return(jnienv, err);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbTxn_commit
  (JNIEnv *jnienv, jobject jthis)
{
    int err;
    DB_TXN *dbtxn = get_DB_TXN(jnienv, jthis);
    if (!verify_non_null(jnienv, dbtxn))
        return;

    err = txn_commit(dbtxn);
    verify_return(jnienv, err);
}

JNIEXPORT jint JNICALL Java_com_sleepycat_db_DbTxn_id
  (JNIEnv *jnienv, jobject jthis)
{
    int retval = 0;
    DB_TXN *dbtxn = get_DB_TXN(jnienv, jthis);
    if (!verify_non_null(jnienv, dbtxn))
        return -1;

    // No error to check for from txn_id
    retval = txn_id(dbtxn);
    return retval;
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbTxn_prepare
  (JNIEnv *jnienv, jobject jthis)
{
    int err;
    DB_TXN *dbtxn = get_DB_TXN(jnienv, jthis);
    if (!verify_non_null(jnienv, dbtxn))
        return;

    err = txn_prepare(dbtxn);
    verify_return(jnienv, err);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbTxn_finalize
  (JNIEnv *jnienv, jobject jthis)
{
    DB_TXN *dbtxn = get_DB_TXN(jnienv, jthis);
    if (dbtxn) {
        // Free any data related to DB_TXN here
        // Note: we don't make a policy of doing
        // a commit or abort here.  The txnmgr
        // should be closed, and DB will clean up.
    }
}
