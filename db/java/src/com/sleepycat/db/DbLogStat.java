/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)DbLogStat.java	10.1 (Sleepycat) 5/2/98
 */

package com.sleepycat.db;

/**
 *
 * Models the DB DB_LOG_STAT struct.
 * @author Donald D. Anderson
 */
public class DbLogStat
{
    // methods
    //

    protected native void finalize()
         throws Throwable;

    // get/set methods
    //

    // Log file magic number.
    public native /*unsigned*/ int get_st_magic();

    // Log file version number.
    public native /*unsigned*/ int get_st_version();

    // Log file mode.
    public native int get_st_mode();

    // Maximum log file size.
    public native /*unsigned*/ int get_st_lg_max();

    // Bytes to log.
    public native /*unsigned*/ int get_st_w_bytes();

    // Megabytes to log.
    public native /*unsigned*/ int get_st_w_mbytes();

    // Bytes to log since checkpoint.
    public native /*unsigned*/ int get_st_wc_bytes();

    // Megabytes to log since checkpoint.
    public native /*unsigned*/ int get_st_wc_mbytes();

    // Total syncs to the log.
    public native /*unsigned*/ int get_st_wcount();

    // Total writes to the log.
    public native /*unsigned*/ int get_st_scount();

    // Region lock granted after wait.
    public native /*unsigned*/ int get_st_region_wait();

    // Region lock granted without wait.
    public native /*unsigned*/ int get_st_region_nowait();

    // Current log file number.
    public native /*unsigned*/ int get_st_cur_file();

    // Current log file offset.
    public native /*unsigned*/ int get_st_cur_offset();

    // private data
    //
    private long private_info_ = 0;

    static {
        Db.load_db();
    }
}

// end of DbLogStat.java
