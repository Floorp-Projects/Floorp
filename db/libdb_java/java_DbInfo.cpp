/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)java_DbInfo.cpp	10.3 (Sleepycat) 4/10/98";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"
#include "com_sleepycat_db_DbInfo.h"

JAVADB_RW_ACCESS(DbInfo, jint, lorder, DB_INFO, db_lorder)
JAVADB_RW_ACCESS(DbInfo, jlong, cachesize, DB_INFO, db_cachesize)
JAVADB_RW_ACCESS(DbInfo, jlong, pagesize, DB_INFO, db_pagesize)
JAVADB_RW_ACCESS(DbInfo, jint, bt_1maxkey, DB_INFO, bt_maxkey)
JAVADB_RW_ACCESS(DbInfo, jint, bt_1minkey, DB_INFO, bt_minkey)
JAVADB_RW_ACCESS(DbInfo, jint, h_1ffactor, DB_INFO, h_ffactor)
JAVADB_RW_ACCESS(DbInfo, jint, h_1nelem, DB_INFO, h_nelem)
JAVADB_RW_ACCESS(DbInfo, jint, re_1pad, DB_INFO, re_pad)
JAVADB_RW_ACCESS(DbInfo, jint, re_1delim, DB_INFO, re_delim)
JAVADB_RW_ACCESS(DbInfo, jint, re_1len, DB_INFO, re_len)
// TODO: JAVADB_RW_ACCESS_STRING(DbInfo, re_1source, DB_INFO, re_source)
JAVADB_RW_ACCESS(DbInfo, jint, flags, DB_INFO, flags)


/* Initialize one DbInfo object from another.
 * If jthat is null, make this a default initialization.
 */
JNIEXPORT void JNICALL Java_com_sleepycat_db_DbInfo_init_1from
  (JNIEnv *jnienv, jobject jthis, /*DbInfo*/ jobject jthat)
{
    DB_INFO *dbthis = NEW(DB_INFO);
    DB_INFO *dbthat = get_DB_INFO(jnienv, jthat);
    if (dbthat != 0) {
        *dbthis = *dbthat;
    }
    else {
        memset(dbthis, 0, sizeof(DB_INFO));
        dbthis->db_malloc = java_db_malloc;
    }
    set_private_info(jnienv, name_DB_INFO, jthis, dbthis);
}

JNIEXPORT jstring JNICALL Java_com_sleepycat_db_DbInfo_get_1re_1source
  (JNIEnv *jnienv, jobject jthis)
{
    DB_INFO *dbinfo = get_DB_INFO(jnienv, jthis);
    if (!verify_non_null(jnienv, dbinfo))
        return 0;

    if (!dbinfo->re_source)
        return 0;

    return jnienv->NewStringUTF(dbinfo->re_source);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbInfo_set_1re_1source
  (JNIEnv *jnienv, jobject jthis, jstring value)
{
    DB_INFO *dbinfo = get_DB_INFO(jnienv, jthis);
    if (!verify_non_null(jnienv, dbinfo))
        return;

    LockedString re_source(jnienv, value);
    dbinfo->re_source = dup_string(re_source.string);
}

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbInfo_finalize
  (JNIEnv *jnienv, jobject jthis)
{
    DB_INFO *dbinfo = get_DB_INFO(jnienv, jthis);
    if (dbinfo) {
        if (dbinfo->re_source)
            DELETE(dbinfo->re_source);

        // Free any data related to DB_INFO here
        DELETE(dbinfo);
    }
}
