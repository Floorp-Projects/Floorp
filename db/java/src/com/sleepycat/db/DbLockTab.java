/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)DbLockTab.java	10.4 (Sleepycat) 4/10/98
 */

package com.sleepycat.db;

/**
 *
 * @author Donald D. Anderson
 */
public class DbLockTab
{
    // Note: See Db.java for flag values for any of these
    // methods.
    //

    // methods
    //
    public native void close()
         throws DbException;

    public native void detect(int flags, int atype)
         throws DbException;

    public native DbLock get(/*u_int32_t*/ int locker,
                           int flags,
                           Dbt obj,
                           /*db_lockmode_t*/ int lock_mode)
         throws DbException;

    public native /*u_int32_t*/ int id()
         throws DbException;

    // Note: this method is not yet implemented.
    // We expect that users will not use DbLockTab directly,
    // but only use indirectly it as a consequence
    // of other Db activity.

/*
    public native void vec(u_int32_t locker,
                           int flags,
                           DB_LOCKREQ list[],
                           int nlist,
                           DB_LOCKREQ **elistp)
         throws DbException;
 */


    // Create or remove new locktab files
    //

    public native static DbLockTab open(String dir, int flags, int mode,
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

// end of DbLockTab.java
