/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)java_DbMpoolFStat.cpp	10.2 (Sleepycat) 4/10/98";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"
#include "com_sleepycat_db_DbMpoolFStat.h"

JAVADB_RO_ACCESS_STRING(DbMpoolFStat, file_1name, DB_MPOOL_FSTAT, file_name, 0)
JAVADB_RO_ACCESS(DbMpoolFStat, jlong, st_1pagesize, DB_MPOOL_FSTAT, st_pagesize)
JAVADB_RO_ACCESS(DbMpoolFStat, jlong, st_1cache_1hit, DB_MPOOL_FSTAT, st_cache_hit)
JAVADB_RO_ACCESS(DbMpoolFStat, jlong, st_1cache_1miss, DB_MPOOL_FSTAT, st_cache_miss)
JAVADB_RO_ACCESS(DbMpoolFStat, jlong, st_1map, DB_MPOOL_FSTAT, st_map)
JAVADB_RO_ACCESS(DbMpoolFStat, jlong, st_1page_1create, DB_MPOOL_FSTAT, st_page_create)
JAVADB_RO_ACCESS(DbMpoolFStat, jlong, st_1page_1in, DB_MPOOL_FSTAT, st_page_in)
JAVADB_RO_ACCESS(DbMpoolFStat, jlong, st_1page_1out, DB_MPOOL_FSTAT, st_page_out)

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbMpoolFStat_finalize
  (JNIEnv *jnienv, jobject jthis)
{
    DB_MPOOL_FSTAT *db_mpool_fstat = get_DB_MPOOL_FSTAT(jnienv, jthis);
    if (db_mpool_fstat) {
        // Free any data related to DB_MPOOL_FSTAT here
        free(db_mpool_fstat);
    }
}
