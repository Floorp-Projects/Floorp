/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)DbMpoolStat.java	10.2 (Sleepycat) 4/10/98
 */

package com.sleepycat.db;

/**
 *
 * Models the DB DB_MPOOL_STAT struct.
 * @author Donald D. Anderson
 */
public class DbMpoolStat
{
    // methods
    //

    protected native void finalize()
         throws Throwable;

    // get/set methods
    //

    // Cache size.
    public native /*size_t*/ long get_st_cachesize();

    // Pages found in the cache.
    public native /*unsigned*/ long get_st_cache_hit();

    // Pages not found in the cache.
    public native /*unsigned*/ long get_st_cache_miss();

    // Pages from mapped files.
    public native /*unsigned*/ long get_st_map();

    // Pages created in the cache.
    public native /*unsigned*/ long get_st_page_create();

    // Pages read in.
    public native /*unsigned*/ long get_st_page_in();

    // Pages written out.
    public native /*unsigned*/ long get_st_page_out();

    // Read-only pages evicted.
    public native /*unsigned*/ long get_st_ro_evict();

    // Read-write pages evicted.
    public native /*unsigned*/ long get_st_rw_evict();

    // Number of hash buckets.
    public native /*unsigned*/ long get_st_hash_buckets();

    // Total hash chain searches.
    public native /*unsigned*/ long get_st_hash_searches();

    // Longest hash chain searched.
    public native /*unsigned*/ long get_st_hash_longest();

    // Total hash entries searched.
    public native /*unsigned*/ long get_st_hash_examined();

    // private data
    //
    private long private_info_ = 0;

    static {
        Db.load_db();
    }
}

// end of DbMpoolStat.java
