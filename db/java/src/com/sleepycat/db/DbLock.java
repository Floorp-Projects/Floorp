/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)DbLock.java	10.2 (Sleepycat) 4/10/98
 */

package com.sleepycat.db;

/**
 *
 * @author Donald D. Anderson
 */
public class DbLock
{
    // NOTE: the type used here is int, the type in DB is unsigned int.

    protected native void finalize()
         throws Throwable;

    // methods
    //
    public DbLock(int lockid)
         throws DbException
    {
        set_lock_id(lockid);
    }

    public DbLock()
         throws DbException
    {
        set_lock_id(0);
    }

    public native void put(DbLockTab locktab)
         throws DbException;

    // get/set methods
    //
    public native int get_lock_id()
         throws DbException;

    public native void set_lock_id(int lockid)
         throws DbException;

    // private data
    //
    private long private_info_ = 0;

    static {
        Db.load_db();
    }
}

// end of DbLock.java
