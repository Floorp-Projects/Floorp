/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)LockExample.cpp	10.5 (Sleepycat) 4/10/98
 */

#include "config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <iostream.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include <db_cxx.h>

#define	LOCK_HOME	"/var/tmp/lock"

char *progname = "LockExample";				// Program name.

//
// An example of a program using DBLock and related classes.
//
class LockExample : public DbEnv
{
public:
    void run();

    LockExample(const char *home);

private:
    static const char FileName[];

    // no need for copy and assignment
    LockExample(const LockExample &);
    operator = (const LockExample &);
};

static void usage();          // forward

int
main(int argc, char *argv[])
{
    const char *home = LOCK_HOME;
    int do_unlink = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0) {
            home = argv[++i];
        }
        else if (strcmp(argv[i], "-u") == 0) {
            do_unlink = 1;
        }
        else {
            usage();
        }
    }

    try {
        if (do_unlink) {
            LockExample temp(home);
            DbLockTab::unlink(home, 1, &temp);
        }

        LockExample app(home);
        app.run();
        return 0;
    }
    catch (DbException &dbe) {
        cerr << "AccessExample: " << dbe.what() << "\n";
        return 1;
    }
}

LockExample::LockExample(const char *home)
:   DbEnv(home, 0, DB_CREATE|DB_INIT_LOCK)
{
    set_error_stream(&cerr);
    set_errpfx("LockExample");
}


void LockExample::run()
{
    long held;
    u_int32_t len, locker;
    int did_get, ret;
    char objbuf[1024];
    unsigned int lockid = 0;

    DbLockTab *lockTable = get_lk_info();
    if (!lockTable) {
        cerr << "LockExample: lock table not initialized\n";
        return;
    }

    //
    // Accept lock requests.
    //
    lockTable->id(&locker);
    for (held = 0;;) {
        cout << "Operation get/release [get]> ";
        cout.flush();

        char opbuf[16];
        cin.getline(opbuf, sizeof(opbuf));
        if (cin.eof())
            break;
        if ((len = strlen(opbuf)) <= 1 || strcmp(opbuf, "get") == 0) {
            // Acquire a lock.
            cout << "input object (text string) to lock> ";
            cout.flush();
            cin.getline(objbuf, sizeof(objbuf));
            if (cin.eof())
                break;
            if ((len = strlen(objbuf)) <= 0)
                continue;

            char lockbuf[16];
            do {
                cout << "lock type read/write [read]> ";
                cout.flush();
                cin.getline(lockbuf, sizeof(lockbuf));
                if (cin.eof())
                    break;
                len = strlen(lockbuf);
            } while (len >= 1 &&
                     strcmp(lockbuf, "read") != 0 &&
                     strcmp(lockbuf, "write") != 0);

            db_lockmode_t lock_type;
            if (len <= 1 || strcmp(lockbuf, "read") == 0)
                lock_type = DB_LOCK_READ;
            else
                lock_type = DB_LOCK_WRITE;

            Dbt dbt(objbuf, strlen(objbuf));

            DbLock lock;
            ret = lockTable->get(locker,
                                DB_LOCK_NOWAIT, &dbt, lock_type, &lock);
            lockid = lock.get_lock_id();
            did_get = 1;
        } else {
            // Release a lock.
            do {
                cout << "input lock to release> ";
                cout.flush();
                cin.getline(objbuf, sizeof(objbuf));
                if (cin.eof())
                    break;
            } while ((len = strlen(objbuf)) <= 1);
            lockid = strtoul(objbuf, NULL, 16);
            DbLock lock(lockid);
            ret = lock.put(lockTable);
            did_get = 0;
        }

        switch (ret) {
        case 0:
            cout << "Lock 0x" << hex << lockid << " "
                 <<  (did_get ? "granted" : "released")
                 << "\n";
            held += did_get ? 1 : -1;
            break;
        case DB_LOCK_NOTHELD:
            cout << "You do not hold the lock " << lockid << "\n";
            break;
        case DB_LOCK_NOTGRANTED:
            cout << "Lock not granted\n";
            break;
        case DB_LOCK_DEADLOCK:
            cerr << "LockExample: lock_"
                 << (did_get ? "get" : "put")
                 << ": " << "returned DEADLOCK";
            break;
        default:
            cerr << "LockExample: lock_get: %s",
                strerror(errno);
        }
    }
    cout << "\n";
    cout << "Closing lock region " << held << " locks held\n";
}

static void
usage()
{
    cerr << "usage: LockExample [-u] [-h home]\n";
    exit(1);
}
