/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)java_DbLogStat.cpp	10.1 (Sleepycat) 5/2/98";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"
#include "com_sleepycat_db_DbLogStat.h"

JAVADB_RO_ACCESS(DbLogStat, jint, st_1magic, DB_LOG_STAT, st_magic)
JAVADB_RO_ACCESS(DbLogStat, jint, st_1version, DB_LOG_STAT, st_version)
JAVADB_RO_ACCESS(DbLogStat, jint, st_1mode, DB_LOG_STAT, st_mode)
JAVADB_RO_ACCESS(DbLogStat, jint, st_1lg_1max, DB_LOG_STAT, st_lg_max)
JAVADB_RO_ACCESS(DbLogStat, jint, st_1w_1bytes, DB_LOG_STAT, st_w_bytes)
JAVADB_RO_ACCESS(DbLogStat, jint, st_1w_1mbytes, DB_LOG_STAT, st_w_mbytes)
JAVADB_RO_ACCESS(DbLogStat, jint, st_1wc_1bytes, DB_LOG_STAT, st_wc_bytes)
JAVADB_RO_ACCESS(DbLogStat, jint, st_1wc_1mbytes, DB_LOG_STAT, st_wc_mbytes)
JAVADB_RO_ACCESS(DbLogStat, jint, st_1wcount, DB_LOG_STAT, st_wcount)
JAVADB_RO_ACCESS(DbLogStat, jint, st_1scount, DB_LOG_STAT, st_scount)
JAVADB_RO_ACCESS(DbLogStat, jint, st_1region_1wait, DB_LOG_STAT, st_region_wait)
JAVADB_RO_ACCESS(DbLogStat, jint, st_1region_1nowait, DB_LOG_STAT, st_region_nowait)
JAVADB_RO_ACCESS(DbLogStat, jint, st_1cur_1file, DB_LOG_STAT, st_cur_file)
JAVADB_RO_ACCESS(DbLogStat, jint, st_1cur_1offset, DB_LOG_STAT, st_cur_offset)

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbLogStat_finalize
  (JNIEnv *jnienv, jobject jthis)
{
    DB_LOG_STAT *db_log_stat = get_DB_LOG_STAT(jnienv, jthis);
    if (db_log_stat) {
        // Free any data related to DB_LOG_STAT here
        free(db_log_stat);
    }
}
