/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)java_DbTxnStat.cpp	10.2 (Sleepycat) 4/10/98";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"
#include "com_sleepycat_db_DbTxnStat.h"

JAVADB_RO_ACCESS(DbTxnStat, jint, time_1ckp, DB_TXN_STAT, st_time_ckp)
JAVADB_RO_ACCESS(DbTxnStat, jint, last_1txnid, DB_TXN_STAT, st_last_txnid)
JAVADB_RO_ACCESS(DbTxnStat, jint, maxtxns, DB_TXN_STAT, st_maxtxns)
JAVADB_RO_ACCESS(DbTxnStat, jint, naborts, DB_TXN_STAT, st_naborts)
JAVADB_RO_ACCESS(DbTxnStat, jint, nbegins, DB_TXN_STAT, st_nbegins)
JAVADB_RO_ACCESS(DbTxnStat, jint, ncommits, DB_TXN_STAT, st_ncommits)

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_DbTxnStat_get_1last_1ckp
  (JNIEnv *jnienv, jobject jthis)
{
    DB_TXN_STAT *dbtxnstat = get_DB_TXN_STAT(jnienv, jthis);
    if (!verify_non_null(jnienv, dbtxnstat))
        return 0;

    return get_DbLsn(jnienv, dbtxnstat->st_last_ckp);
}

JNIEXPORT jobject JNICALL Java_com_sleepycat_db_DbTxnStat_get_1pending_1ckp
  (JNIEnv *jnienv, jobject jthis)
{
    DB_TXN_STAT *dbtxnstat = get_DB_TXN_STAT(jnienv, jthis);
    if (!verify_non_null(jnienv, dbtxnstat))
        return 0;

    return get_DbLsn(jnienv, dbtxnstat->st_pending_ckp);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbTxnStat_finalize
  (JNIEnv *jnienv, jobject jthis)
{
    DB_TXN_STAT *dbtxnstat = get_DB_TXN_STAT(jnienv, jthis);
    if (dbtxnstat) {
        // Free any data related to DB_TXN_STAT here
        free(dbtxnstat);
    }
}
