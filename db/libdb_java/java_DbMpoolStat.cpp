/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)java_DbMpoolStat.cpp	10.2 (Sleepycat) 4/10/98";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"
#include "com_sleepycat_db_DbMpoolStat.h"

JAVADB_RO_ACCESS(DbMpoolStat, jlong, st_1cachesize, DB_MPOOL_STAT, st_cachesize)
JAVADB_RO_ACCESS(DbMpoolStat, jlong, st_1cache_1hit, DB_MPOOL_STAT, st_cache_hit)
JAVADB_RO_ACCESS(DbMpoolStat, jlong, st_1cache_1miss, DB_MPOOL_STAT, st_cache_miss)
JAVADB_RO_ACCESS(DbMpoolStat, jlong, st_1map, DB_MPOOL_STAT, st_map)
JAVADB_RO_ACCESS(DbMpoolStat, jlong, st_1page_1create, DB_MPOOL_STAT, st_page_create)
JAVADB_RO_ACCESS(DbMpoolStat, jlong, st_1page_1in, DB_MPOOL_STAT, st_page_in)
JAVADB_RO_ACCESS(DbMpoolStat, jlong, st_1page_1out, DB_MPOOL_STAT, st_page_out)
JAVADB_RO_ACCESS(DbMpoolStat, jlong, st_1ro_1evict, DB_MPOOL_STAT, st_ro_evict)
JAVADB_RO_ACCESS(DbMpoolStat, jlong, st_1rw_1evict, DB_MPOOL_STAT, st_rw_evict)
JAVADB_RO_ACCESS(DbMpoolStat, jlong, st_1hash_1buckets, DB_MPOOL_STAT, st_hash_buckets)
JAVADB_RO_ACCESS(DbMpoolStat, jlong, st_1hash_1searches, DB_MPOOL_STAT, st_hash_searches)
JAVADB_RO_ACCESS(DbMpoolStat, jlong, st_1hash_1longest, DB_MPOOL_STAT, st_hash_longest)
JAVADB_RO_ACCESS(DbMpoolStat, jlong, st_1hash_1examined, DB_MPOOL_STAT, st_hash_examined)

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbMpoolStat_finalize
  (JNIEnv *jnienv, jobject jthis)
{
    DB_MPOOL_STAT *db_mpool_stat = get_DB_MPOOL_STAT(jnienv, jthis);
    if (db_mpool_stat) {
        // Free any data related to DB_MPOOL_STAT here
        free(db_mpool_stat);
    }
}
