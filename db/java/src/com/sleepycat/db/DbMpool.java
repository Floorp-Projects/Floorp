/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)DbMpool.java	10.5 (Sleepycat) 5/5/98
 */

package com.sleepycat.db;

/**
 *
 * @author Donald D. Anderson
 */
public class DbMpool
{
    public native DbMpoolStat stat()
         throws DbException;

    public native DbMpoolFStat[] fstat()
         throws DbException;

    public native int trickle(int pct)
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

// end of DbMpool.java
