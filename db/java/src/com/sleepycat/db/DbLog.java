/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)DbLog.java	10.4 (Sleepycat) 5/2/98
 */

package com.sleepycat.db;

/**
 *
 * @author Donald D. Anderson
 */
public class DbLog
{
    // methods
    //

    public native String[] archive(int flags)
         throws DbException;

    public native void close()
         throws DbException;

    public native static int compare(DbLsn lsn0, DbLsn lsn1);

    public native String file(DbLsn lsn)
         throws DbException;

    public native void flush(DbLsn lsn)
         throws DbException;

    public native void get(DbLsn lsn, Dbt data, int flags)
         throws DbException;

    public native void put(DbLsn lsn, Dbt data, int flags)
         throws DbException;

    public native DbLogStat stat()
         throws DbException;

    public native /*u_int32_t fidp*/ int db_register(Db dbp, String name,
                                                     int dbtype)
         throws DbException;

    public native void db_unregister(/*u_int32_t*/ int fid)
         throws DbException;

    // Create or remove new log files
    //

    public native static DbLog open(String dir, int flags, int mode,
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

// end of DbLog.java
