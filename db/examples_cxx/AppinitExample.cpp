/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)AppinitExample.cpp	10.6 (Sleepycat) 4/10/98
 */

#include "config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <iostream.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include <db_cxx.h>

#define	DATABASE_HOME	"/home/database"

DB_ENV *db_setup(FILE *);

char *progname = "AppinitExample";			/* Program name. */

/*
 * An example of a program using DbEnv to configure its DB
 * environment.
 */
DbEnv *
init()
{
    DbEnv *dbenv = new DbEnv();
    char *config[2];

    // Output errors to the application's log.
    //
    dbenv->set_error_stream(&cerr);
    dbenv->set_errpfx(progname);

    //
    // All of the shared database files live in /home/database,
    // but data files live in /database.
    //
    config[0] = "DB_DATA_DIR /database/files";
    config[1] = NULL;

    //
    // We want to specify the shared memory buffer pool cachesize,
    // but everything else is the default.
    //
    dbenv->set_mp_size(64 * 1024);

    //
    // We have multiple processes reading/writing these files, so
    // we need concurrency control and a shared buffer pool, but
    // not logging or transactions.
    //
    if ((errno = dbenv->appinit(DATABASE_HOME,
	    config, DB_CREATE | DB_INIT_LOCK | DB_INIT_MPOOL)) != 0) {
        cerr << progname << ": db_appinit: " << strerror(errno);
        exit (1);
    }
    return dbenv;
}

int
main(int, char **)
{
    //
    // Note: it may be easiest to put all DB operations in a try block, as
    // seen here.  Alternatively, you can change the ErrorModel in the DbEnv
    // so that exceptions are never thrown and check error returns from all
    // methods.
    //
    try
    {
        DbEnv *dbenv = init();

        // ... your application here ...

        delete dbenv;
        return 0;
    }
    catch (DbException &dbe)
    {
        cerr << "AccessExample: " << dbe.what() << "\n";
        return 1;
    }
}
