/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)DbInfo.java	10.3 (Sleepycat) 4/10/98
 */

package com.sleepycat.db;

/**
 *
 * @author Donald D. Anderson
 */
public class DbInfo
{
    // methods
    //
    public DbInfo()
    {
        init_from(null);
    }

    public DbInfo(DbInfo that)
    {
        init_from(that);
    }

    private native void init_from(DbInfo that);

    protected native void finalize()
         throws Throwable;

    // get/set methods
    //

    // Byte order.
    public native int get_lorder();
    public native void set_lorder(int lorder);

    // Underlying cache size.
    public native /*size_t*/ long get_cachesize();
    public native void set_cachesize(/*size_t*/ long cachesize);

    // Underlying page size.
    public native /*size_t*/ long get_pagesize();
    public native void set_pagesize(/*size_t*/ long pagesize);

    // Local heap allocation.
    // Note: there is no access to the underlying "malloc" field, since
    // it is a C function that makes little sense in the Java world.

    ////////////////////////////////////////////////////////////////
    // Btree access method.

    // Maximum keys per page.
    public native int get_bt_maxkey();
    public native void set_bt_maxkey(int bt_maxkey);

    // Minimum keys per page.
    public native int get_bt_minkey();
    public native void set_bt_minkey(int bt_minkey);

    // Note: this callback is not implemented
    // Comparison function.
    // public native DbCompare get_bt_compare();
    // public native void set_bt_compare(DbCompare bt_compare);

    // Note: this callback is not implemented
    // Prefix function.
    // public native DbPrefix get_bt_prefix();
    // public native void set_bt_prefix(DbPrefix bt_prefix);

    ////////////////////////////////////////////////////////////////
    // Hash access method.

    // Fill factor.
    public native /*unsigned*/ int get_h_ffactor();
    public native void set_h_ffactor(/*unsigned*/ int h_ffactor);

    // Number of elements.
    public native /*unsigned*/ int get_h_nelem();
    public native void set_h_nelem(/*unsigned*/ int h_nelem);

    // Note: this callback is not implemented
    // Hash function.
    // public native DbHash get_h_hash();
    // public native void set_h_hash(DbHash h_hash);

    ////////////////////////////////////////////////////////////////
    // Recno access method.

    // Fixed-length padding byte.
    public native int get_re_pad();
    public native void set_re_pad(int re_pad);

    // Variable-length delimiting byte.
    public native int get_re_delim();
    public native void set_re_delim(int re_delim);

    // Length for fixed-length records.
    public native /*u_int32_t*/ int get_re_len();
    public native void set_re_len(/*u_int32_t*/ int re_len);

    // Source file name.
    public native String get_re_source();
    public native void set_re_source(String re_source);

    // Flags.
    public native /*u_int32_t*/ int get_flags();
    public native void set_flags(/*u_int32_t*/ int flags);

    // private data
    //
    private long private_info_ = 0;

    static {
        Db.load_db();
    }
}

// end of DbInfo.java
