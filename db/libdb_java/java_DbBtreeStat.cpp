/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)java_DbBtreeStat.cpp	10.2 (Sleepycat) 4/10/98";
#endif /* not lint */

#include <jni.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "java_util.h"
#include "com_sleepycat_db_DbBtreeStat.h"

JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1flags, DB_BTREE_STAT, bt_flags)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1maxkey, DB_BTREE_STAT, bt_maxkey)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1minkey, DB_BTREE_STAT, bt_minkey)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1re_1len, DB_BTREE_STAT, bt_re_len)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1re_1pad, DB_BTREE_STAT, bt_re_pad)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1pagesize, DB_BTREE_STAT, bt_pagesize)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1levels, DB_BTREE_STAT, bt_levels)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1nrecs, DB_BTREE_STAT, bt_nrecs)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1int_1pg, DB_BTREE_STAT, bt_int_pg)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1leaf_1pg, DB_BTREE_STAT, bt_leaf_pg)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1dup_1pg, DB_BTREE_STAT, bt_dup_pg)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1over_1pg, DB_BTREE_STAT, bt_over_pg)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1free, DB_BTREE_STAT, bt_free)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1freed, DB_BTREE_STAT, bt_freed)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1int_1pgfree, DB_BTREE_STAT, bt_int_pgfree)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1leaf_1pgfree, DB_BTREE_STAT, bt_leaf_pgfree)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1dup_1pgfree, DB_BTREE_STAT, bt_dup_pgfree)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1over_1pgfree, DB_BTREE_STAT, bt_over_pgfree)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1pfxsaved, DB_BTREE_STAT, bt_pfxsaved)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1split, DB_BTREE_STAT, bt_split)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1rootsplit, DB_BTREE_STAT, bt_rootsplit)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1fastsplit, DB_BTREE_STAT, bt_fastsplit)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1added, DB_BTREE_STAT, bt_added)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1deleted, DB_BTREE_STAT, bt_deleted)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1get, DB_BTREE_STAT, bt_get)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1cache_1hit, DB_BTREE_STAT, bt_cache_hit)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1cache_1miss, DB_BTREE_STAT, bt_cache_miss)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1magic, DB_BTREE_STAT, bt_magic)
JAVADB_RO_ACCESS(DbBtreeStat, jlong, bt_1version, DB_BTREE_STAT, bt_version)

JNIEXPORT void JNICALL Java_com_sleepycat_db_DbBtreeStat_finalize
  (JNIEnv *jnienv, jobject jthis)
{
    DB_BTREE_STAT *db_btree_stat = get_DB_BTREE_STAT(jnienv, jthis);
    if (db_btree_stat) {
        // Free any data related to DB_BTREE_STAT here
        free(db_btree_stat);
    }
}
