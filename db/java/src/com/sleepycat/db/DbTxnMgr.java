/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)DbTxnMgr.java	10.2 (Sleepycat) 4/10/98
 */

package com.sleepycat.db;

/**
 *
 * @author Donald D. Anderson
 */
public class DbTxnMgr
{
    // methods
    //
    public native DbTxn begin(DbTxn pid)
         throws DbException;

    public native void checkpoint(int kbyte, int min)
         throws DbException;

    public native void close()
         throws DbException;

    public native DbTxnStat stat()
         throws DbException;

    // Create or remove new txnmgr files
    //
    public native static DbTxnMgr open(String dir, int flags, int mode,
                                       DbEnv dbenv)
         throws DbException;

    public native static void unlink(String dir, int force, DbEnv dbenv)
         throws DbException;

    protected native void finalize()
         throws Throwable;

    // get/set methods
    //

    // private data
    //
    private long private_info_ = 0;

    static {
        Db.load_db();
    }
}

// end of DbTxnMgr.java
