/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)DbMpoolFStat.java	10.2 (Sleepycat) 4/10/98
 */

package com.sleepycat.db;

/**
 *
 * Models the DB DB_MPOOL_FSTAT struct.
 * @author Donald D. Anderson
 */
public class DbMpoolFStat
{
    // methods
    //
    protected native void finalize()
         throws Throwable;

    // get/set methods
    //

    // File name.
    public native String get_file_name();

    // Page size.
    public native /*size_t*/ long get_st_pagesize();

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

    // private data
    //
    private long private_info_ = 0;

    static {
        Db.load_db();
    }
}

// end of DbMpoolFStat.java
