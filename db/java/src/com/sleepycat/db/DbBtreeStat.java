/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)DbBtreeStat.java	10.2 (Sleepycat) 4/10/98
 */

package com.sleepycat.db;

/*
 * Models the DB DB_BTREE_STAT struct.
 */
public class DbBtreeStat
{
    // methods
    //

    protected native void finalize()
         throws Throwable;

    // Open flags.
    public native /*unsigned*/ long get_bt_flags();

    // Maxkey value.
    public native /*unsigned*/ long get_bt_maxkey();

    // Minkey value.
    public native /*unsigned*/ long get_bt_minkey();

    // Fixed-length record length.
    public native /*unsigned*/ long get_bt_re_len();

    // Fixed-length record pad.
    public native /*unsigned*/ long get_bt_re_pad();

    // Page size.
    public native /*unsigned*/ long get_bt_pagesize();

    // Tree levels.
    public native /*unsigned*/ long get_bt_levels();

    // Number of records.
    public native /*unsigned*/ long get_bt_nrecs();

    // Internal pages.
    public native /*unsigned*/ long get_bt_int_pg();

    // Leaf pages.
    public native /*unsigned*/ long get_bt_leaf_pg();

    // Duplicate pages.
    public native /*unsigned*/ long get_bt_dup_pg();

    // Overflow pages.
    public native /*unsigned*/ long get_bt_over_pg();

    // Pages on the free list.
    public native /*unsigned*/ long get_bt_free();

    // Pages freed for reuse.
    public native /*unsigned*/ long get_bt_freed();

    // Bytes free in internal pages.
    public native /*unsigned*/ long get_bt_int_pgfree();

    // Bytes free in leaf pages.
    public native /*unsigned*/ long get_bt_leaf_pgfree();

    // Bytes free in duplicate pages.
    public native /*unsigned*/ long get_bt_dup_pgfree();

    // Bytes free in overflow pages.
    public native /*unsigned*/ long get_bt_over_pgfree();

    // Bytes saved by prefix compression.
    public native /*unsigned*/ long get_bt_pfxsaved();

    // Total number of splits.
    public native /*unsigned*/ long get_bt_split();

    // Root page splits.
    public native /*unsigned*/ long get_bt_rootsplit();

    // Fast splits.
    public native /*unsigned*/ long get_bt_fastsplit();

    // Items added.
    public native /*unsigned*/ long get_bt_added();

    // Items deleted.
    public native /*unsigned*/ long get_bt_deleted();

    // Items retrieved.
    public native /*unsigned*/ long get_bt_get();

    // Hits in fast-insert code.
    public native /*unsigned*/ long get_bt_cache_hit();

    // Misses in fast-insert code.
    public native /*unsigned*/ long get_bt_cache_miss();

    // Magic number.
    public native /*unsigned*/ long get_bt_magic();

    // Version number.
    public native /*unsigned*/ long get_bt_version();


    // private data
    //
    private long private_info_ = 0;

    static {
        Db.load_db();
    }
}

// end of DbBtreeStat.java
