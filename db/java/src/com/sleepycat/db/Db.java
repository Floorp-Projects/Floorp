/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)Db.java	10.5 (Sleepycat) 6/2/98
 */

package com.sleepycat.db;

/**
 *
 * @author Donald D. Anderson
 */
public class Db
{
    // All constant and flag values used with Db* classes are defined here.

    // Collectively, these constants are known by the name
    // "DBTYPE" in the documentation.
    //
    public static final int DB_BTREE = 1; // B+tree
    public static final int DB_HASH  = 2; // Extended Linear Hashing.
    public static final int DB_RECNO = 3; // Fixed and variable-length records.
    public static final int DB_UNKNOWN = 4; // Figure it out on open.

    // Flags understood by Db.open(), DbEnv.appinit(), DbEnv.DbEnv().
    //
    public static final int DB_CREATE; // O_CREAT: create file as necessary.
    public static final int DB_NOMMAP; // Don't mmap underlying file.
    public static final int DB_THREAD; // Free-thread DB package handles.

    //
    // Flags understood by db_open(3).
    //
    public static final int DB_EXCL; // O_EXCL: exclusive open.
    public static final int DB_RDONLY; // O_RDONLY: read-only.
    public static final int DB_SEQUENTIAL; // Indicate sequential access.
    public static final int DB_TEMPORARY; // Remove on last close.
    public static final int DB_TRUNCATE; // O_TRUNCATE: replace existing DB.


    //
    // DB (user visible) error return codes.
    //
    public static final int DB_INCOMPLETE = -1; // Sync didn't finish.
    public static final int DB_KEYEMPTY = -2;   // The key/data pair was deleted or
                                                // was never created by the user.
    public static final int DB_KEYEXIST = -3;   // The key/data pair already exists.
    public static final int DB_LOCK_DEADLOCK = -4; // Locker killed to resolve deadlock.
    public static final int DB_LOCK_NOTGRANTED = -5; // Lock unavailable, no-wait set.
    public static final int DB_LOCK_NOTHELD = -6; // Lock not held by locker.
    public static final int DB_NOTFOUND = -7;   // Key/data pair not found (EOF).

    //
    // Flags used by DbEnv.appinit()
    //
    public static final int DB_INIT_LOCK;        // Initialize locking.
    public static final int DB_INIT_LOG;         // Initialize logging.
    public static final int DB_INIT_MPOOL;       // Initialize mpool.
    public static final int DB_INIT_TXN;         // Initialize transactions.
    public static final int DB_MPOOL_PRIVATE;    // Mpool: private memory pool.
    public static final int DB_RECOVER;          // Run normal recovery.
    public static final int DB_RECOVER_FATAL;    // Run catastrophic recovery.
    public static final int DB_TXN_NOSYNC;       // Do not sync log on commit.
    public static final int DB_USE_ENVIRON;      // Use the environment.
    public static final int DB_USE_ENVIRON_ROOT; // Use the environment if root.

    //
    // Deadlock detector modes; used in the DBENV structure to configure the
    // locking subsystem.
    //
    public static final int DB_LOCK_NORUN;
    public static final int DB_LOCK_DEFAULT;
    public static final int DB_LOCK_OLDEST;
    public static final int DB_LOCK_RANDOM;
    public static final int DB_LOCK_YOUNGEST;

    //
    // Values for DbInfo flags
    //
    public static final int DB_DELIMITER; // Recno: re_delim set.
    public static final int DB_DUP;       // Btree, Hash: duplicate keys.
    public static final int DB_FIXEDLEN;  // Recno: fixed-length records.
    public static final int DB_PAD;       // Recno: re_pad set.
    public static final int DB_RECNUM;    // Btree: record numbers.
    public static final int DB_RENUMBER;  // Recno: renumber on insert/delete.
    public static final int DB_SNAPSHOT;  // Recno: snapshot the input.

    // Collectively, these constants are known by the name
    // "db_lockmode_t" in the documentation.
    //
    public static final int DB_LOCK_NG = 0;	// Not granted.
    public static final int DB_LOCK_READ = 1;	// Shared/read.
    public static final int DB_LOCK_WRITE = 2;	// Exclusive/write.
    public static final int DB_LOCK_IREAD = 3;	// Intent to share/read.
    public static final int DB_LOCK_IWRITE = 4;	// Intent exclusive/write.
    public static final int DB_LOCK_IWR = 5;	// Intent to read and write.

    // Collectively, these constants are known by the name
    // "db_lockop_t" in the documentation.
    //
    public static final int DB_LOCK_DUMP = 0;	// Display held locks.
    public static final int DB_LOCK_GET = 1;	// Get the lock.
    public static final int DB_LOCK_PUT = 2;	// Release the lock.
    public static final int DB_LOCK_PUT_ALL = 3;// Release locker's locks.
    public static final int DB_LOCK_PUT_OBJ = 4;// Release locker's locks on obj.

    // Flag values for DbLock.vec()
    public static final int DB_LOCK_NOWAIT;  // Don't wait on unavailable lock.

    // Flag values for DbLock.detect()
    public static final int DB_LOCK_CONFLICT; // Run on any conflict.

    //
    // Flag values for DbLog.archive()
    //
    public static final int DB_ARCH_ABS;      // Absolute pathnames.
    public static final int DB_ARCH_DATA;     // Data files.
    public static final int DB_ARCH_LOG;      // Log files.

    //
    // DB access method and cursor operation codes.  These are implemented as
    // bit fields for future flexibility, but currently only a single one may
    // be specified to any function.
    ///
    public static final int DB_AFTER;      // Dbc.put()
    public static final int DB_APPEND;     // Db.put()
    public static final int DB_BEFORE;     // Dbc.put()
    public static final int DB_CHECKPOINT; // DbLog.put(), DbLog.get()
    public static final int DB_CURRENT;    // Dbc.get(), Dbc.put(), DbLog.get()
    public static final int DB_FIRST;      // Dbc.get(), DbLog.get()
    public static final int DB_FLUSH;      // DbLog.put()
    public static final int DB_GET_RECNO;  // Db.get(), Dbc.get()
    public static final int DB_KEYFIRST;   // Dbc.put()
    public static final int DB_KEYLAST;    // Dbc.put()
    public static final int DB_LAST;       // Dbc.get(), DbLog.get()
    public static final int DB_NEXT;       // Dbc.get(), DbLog.get()
    public static final int DB_NOOVERWRITE;// Db.put()
    public static final int DB_NOSYNC;     // Db.close()
    public static final int DB_PREV;       // Dbc.get(), DbLog.get()
    public static final int DB_RECORDCOUNT;// Db.stat()
    public static final int DB_SET;        // Dbc.get(), DbLog.get()
    public static final int DB_SET_RANGE;  // Dbc.get()
    public static final int DB_SET_RECNO;  // Dbc.get()

    // Collectively, these values are used for Dbt flags
    //
    // Perform any mallocs using regular malloc, not the user's malloc.
    public static final int DB_DBT_INTERNAL;

    // Return in allocated memory.
    public static final int DB_DBT_MALLOC;

    // Partial put/get.
    public static final int DB_DBT_PARTIAL;

    // Return in user's memory.
    public static final int DB_DBT_USERMEM;



    // methods
    //

    public native void close(int flags)
         throws DbException;

    public native Dbc cursor(DbTxn txnid)
         throws DbException;

    public native void del(DbTxn txnid, Dbt key, int flags)
         throws DbException;

    public native int fd()
         throws DbException;

    // returns: 0, DB_NOTFOUND, or throws error
    public native int get(DbTxn txnid, Dbt key, Dbt data, int flags)
         throws DbException;

    // returns: 0, DB_KEYEXIST, or throws error
    public native int put(DbTxn txnid, Dbt key, Dbt data, int flags)
         throws DbException;

    public native DbBtreeStat stat(int flags)
         throws DbException;

    public native void sync(int flags)
         throws DbException;

    public native /*DBTYPE*/ int get_type();

    public native static Db open(String fname, /*DBTYPE*/ int type, int flags,
                                 int mode, DbEnv dbenv, DbInfo info)
         throws DbException;

    protected native void finalize()
         throws Throwable;

    // get/set methods
    //

    // private data
    //
    private long private_info_ = 0;

    private static boolean already_loaded_ = false;

    public static void load_db()
    {
        if (already_loaded_)
            return;

        String os = System.getProperty("os.name");
        if (os != null && os.startsWith("Windows")) {
            // called libdb.dll, libdb_java.dll on Win/*
            System.loadLibrary("libdb");
            System.loadLibrary("libdb_java");
        }
        else {
            // called libdb.so, libdb_java.so on UNIX
            System.loadLibrary("db");
            System.loadLibrary("db_java");
        }

        already_loaded_ = true;
    }

    static private void check_constant(int c1, int c2)
    {
        if (c1 != c2) {
            System.err.println("Db: constant mismatch");
            System.exit(1);
        }
    }

    static {
        Db.load_db();

        // Note: constant values are stored in DbConstants, which
        // is automatically generated.  Initializing constants in
        // static code insulates users from the possibility of
        // changing constants.
        //
        DB_CREATE = DbConstants.DB_CREATE;
        DB_NOMMAP = DbConstants.DB_NOMMAP;
        DB_THREAD = DbConstants.DB_THREAD;

        DB_EXCL = DbConstants.DB_EXCL;
        DB_RDONLY = DbConstants.DB_RDONLY;
        DB_SEQUENTIAL = DbConstants.DB_SEQUENTIAL;
        DB_TEMPORARY = DbConstants.DB_TEMPORARY;
        DB_TRUNCATE = DbConstants.DB_TRUNCATE;

        // These constants are not assigned, but rather checked.
        // Having initialized constants for these values allows
        // them to be used as case values in switch statements.
        //
        check_constant(DB_INCOMPLETE, DbConstants.DB_INCOMPLETE);
        check_constant(DB_KEYEMPTY, DbConstants.DB_KEYEMPTY);
        check_constant(DB_KEYEXIST, DbConstants.DB_KEYEXIST);
        check_constant(DB_LOCK_DEADLOCK, DbConstants.DB_LOCK_DEADLOCK);
        check_constant(DB_LOCK_NOTGRANTED, DbConstants.DB_LOCK_NOTGRANTED);
        check_constant(DB_LOCK_NOTHELD, DbConstants.DB_LOCK_NOTHELD);
        check_constant(DB_NOTFOUND, DbConstants.DB_NOTFOUND);

        DB_INIT_LOCK = DbConstants.DB_INIT_LOCK;
        DB_INIT_LOG = DbConstants.DB_INIT_LOG;
        DB_INIT_MPOOL = DbConstants.DB_INIT_MPOOL;
        DB_INIT_TXN = DbConstants.DB_INIT_TXN;
        DB_MPOOL_PRIVATE = DbConstants.DB_MPOOL_PRIVATE;
        DB_RECOVER = DbConstants.DB_RECOVER;
        DB_RECOVER_FATAL = DbConstants.DB_RECOVER_FATAL;
        DB_TXN_NOSYNC = DbConstants.DB_TXN_NOSYNC;
        DB_USE_ENVIRON = DbConstants.DB_USE_ENVIRON;
        DB_USE_ENVIRON_ROOT = DbConstants.DB_USE_ENVIRON_ROOT;

        DB_LOCK_NORUN = DbConstants.DB_LOCK_NORUN;
        DB_LOCK_DEFAULT = DbConstants.DB_LOCK_DEFAULT;
        DB_LOCK_OLDEST = DbConstants.DB_LOCK_OLDEST;
        DB_LOCK_RANDOM = DbConstants.DB_LOCK_RANDOM;
        DB_LOCK_YOUNGEST = DbConstants.DB_LOCK_YOUNGEST;

        DB_DELIMITER = DbConstants.DB_DELIMITER;
        DB_DUP = DbConstants.DB_DUP;
        DB_FIXEDLEN = DbConstants.DB_FIXEDLEN;
        DB_PAD = DbConstants.DB_PAD;
        DB_RECNUM = DbConstants.DB_RECNUM;
        DB_RENUMBER = DbConstants.DB_RENUMBER;
        DB_SNAPSHOT = DbConstants.DB_SNAPSHOT;

        DB_LOCK_NOWAIT = DbConstants.DB_LOCK_NOWAIT;
        DB_LOCK_CONFLICT = DbConstants.DB_LOCK_CONFLICT;

        DB_ARCH_ABS = DbConstants.DB_ARCH_ABS;
        DB_ARCH_DATA = DbConstants.DB_ARCH_DATA;
        DB_ARCH_LOG = DbConstants.DB_ARCH_LOG;

        DB_AFTER = DbConstants.DB_AFTER;
        DB_APPEND = DbConstants.DB_APPEND;
        DB_BEFORE = DbConstants.DB_BEFORE;
        DB_CHECKPOINT = DbConstants.DB_CHECKPOINT;
        DB_CURRENT = DbConstants.DB_CURRENT;
        DB_FIRST = DbConstants.DB_FIRST;
        DB_FLUSH = DbConstants.DB_FLUSH;
        DB_GET_RECNO = DbConstants.DB_GET_RECNO;
        DB_KEYFIRST = DbConstants.DB_KEYFIRST;
        DB_KEYLAST = DbConstants.DB_KEYLAST;
        DB_LAST = DbConstants.DB_LAST;
        DB_NEXT = DbConstants.DB_NEXT;
        DB_NOOVERWRITE = DbConstants.DB_NOOVERWRITE;
        DB_NOSYNC = DbConstants.DB_NOSYNC;
        DB_PREV = DbConstants.DB_PREV;
        DB_RECORDCOUNT = DbConstants.DB_RECORDCOUNT;
        DB_SET = DbConstants.DB_SET;
        DB_SET_RANGE = DbConstants.DB_SET_RANGE;
        DB_SET_RECNO = DbConstants.DB_SET_RECNO;

        DB_DBT_INTERNAL = DbConstants.DB_DBT_INTERNAL;
        DB_DBT_MALLOC = DbConstants.DB_DBT_MALLOC;
        DB_DBT_PARTIAL = DbConstants.DB_DBT_PARTIAL;
        DB_DBT_USERMEM = DbConstants.DB_DBT_USERMEM;
    }
}

// end of Db.java
