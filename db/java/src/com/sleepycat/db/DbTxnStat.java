/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)DbTxnStat.java	10.2 (Sleepycat) 4/10/98
 */

package com.sleepycat.db;

/**
 *
 * @author Donald D. Anderson
 */
public class DbTxnStat
{
    // methods
    //

    protected native void finalize()
         throws Throwable;

    // get/set methods
    //

    // lsn of the last checkpoint
    public native DbLsn get_last_ckp();

    // last checkpoint did not finish
    public native DbLsn get_pending_ckp();

    // time of last checkpoint
    public native /*time_t*/ int get_time_ckp();

    // last transaction id given out
    public native /*u_int32_t*/ int get_last_txnid();

    // maximum number of active txns
    public native /*u_int32_t*/ int get_maxtxns();

    // number of aborted transactions
    public native /*u_int32_t*/ int get_naborts();

    // number of begun transactions
    public native /*u_int32_t*/ int get_nbegins();

    // number of committed transactions
    public native /*u_int32_t*/ int get_ncommits();

    // private data
    //
    private long private_info_ = 0;

    static {
        Db.load_db();
    }
}

// end of DbTxnStat.java
