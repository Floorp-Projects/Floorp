/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)AppinitExample.java	10.3 (Sleepycat) 4/10/98
 */

package com.sleepycat.examples;

import com.sleepycat.db.*;

/*
 * An example of a program using DbEnv to configure its DB
 * environment.
 *
 * For comparison purposes, this example uses a similar structure
 * as examples/ex_appinit.c and examples_cxx/AppinitExample.cpp.
 */
class AppinitExample
{
    private static final String progname = "AppinitExample";
    private static final String DATABASE_HOME = "/home/database";

    private static DbEnv init()
         throws DbException
    {
        DbEnv dbenv = new DbEnv();
        String config[] = new String[1];

        // Output errors to the application's log.
        //
        dbenv.set_error_stream(System.err);
        dbenv.set_errpfx(progname);

        //
        // All of the shared database files live in /home/database,
        // but data files live in /database.
        //
        // In C/C++ we need to allocate two elements in the array
        // and set config[1] to NULL.  This is not necessary in Java.
        //
        config[0] = "DB_DATA_DIR /database/files";

        //
        // We want to specify the shared memory buffer pool cachesize,
        // but everything else is the default.
        //
        dbenv.set_mp_size(64 * 1024);

        //
        // We have multiple processes reading/writing these files, so
        // we need concurrency control and a shared buffer pool, but
        // not logging or transactions.
        //
        // appinit() will throw on error.
        //
        dbenv.appinit(DATABASE_HOME, config,
                      Db.DB_CREATE | Db.DB_INIT_LOCK | Db.DB_INIT_MPOOL);
        return dbenv;
    }

    static public void main(String[] args)
    {
	System.out.println("beginning appinit");
        try {
            DbEnv dbenv = init();

            // ... your application here ...

            dbenv.appexit();
        }
        catch (DbException dbe) {
            System.err.println(progname + ": db_appinit: " + dbe.toString());
            System.exit (1);
        }
	System.out.println("completed appinit");
    }
}
