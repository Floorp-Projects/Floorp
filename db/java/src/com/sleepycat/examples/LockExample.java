/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)LockExample.java	10.3 (Sleepycat) 4/10/98
 */

package com.sleepycat.examples;

import com.sleepycat.db.*;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.PrintStream;

//
// An example of a program using DbLock and related classes.
//
class LockExample extends DbEnv
{
    private static final String progname = "LockExample";
    private static final String LOCK_HOME = "/var/tmp/lock";

    public LockExample(String home)
         throws DbException
    {
        super(home, null, Db.DB_CREATE|Db.DB_INIT_LOCK);
        set_error_stream(System.err);
        set_errpfx("LockExample");
    }

    // Prompts for a line, and keeps prompting until a non blank
    // line is returned.  Returns null on error.
    //
    static public String askForLine(InputStreamReader reader,
                                    PrintStream out, String prompt)
    {
        String result = "";
        while (result != null && result.length() == 0) {
            out.print(prompt);
            out.flush();
            result = getLine(reader);
        }
        return result;
    }

    // Not terribly efficient, but does the job.
    // Works for reading a line from stdin or a file.
    // Returns null on EOF.  If EOF appears in the middle
    // of a line, returns that line, then null on next call.
    //
    static public String getLine(InputStreamReader reader)
    {
        StringBuffer b = new StringBuffer();
        int c;
        try {
            while ((c = reader.read()) != -1 && c != '\n') {
                if (c != '\r')
                    b.append((char)c);
            }
        }
        catch (IOException ioe) {
            c = -1;
        }

        if (c == -1 && b.length() == 0)
            return null;
        else
            return b.toString();
    }

    public void run()
         throws DbException
    {
        long held;
        int len = 0, locker;
        int ret;
        boolean did_get = false;
        int lockid = 0;
        InputStreamReader in = new InputStreamReader(System.in);

        DbLockTab lockTable = get_lk_info();
        if (lockTable == null) {
            System.err.println("LockExample: lock table not initialized");
            return;
        }

        //
        // Accept lock requests.
        //
        locker = lockTable.id();
        for (held = 0;;) {
            String opbuf = askForLine(in, System.out,
                                      "Operation get/release [get]> ");
            if (opbuf == null)
                break;

            try {
                if (opbuf.equals("get")) {
                    // Acquire a lock.
                    String objbuf = askForLine(in, System.out,
                                   "input object (text string) to lock> ");
                    if (objbuf == null)
                        break;

                    String lockbuf;
                    do {
                        lockbuf = askForLine(in, System.out,
                                             "lock type read/write [read]> ");
                        if (lockbuf == null)
                            break;
                        len = lockbuf.length();
                    } while (len >= 1 &&
                             !lockbuf.equals("read") &&
                             !lockbuf.equals("write"));

                    int lock_type;
                    if (len <= 1 || lockbuf.equals("read"))
                        lock_type = Db.DB_LOCK_READ;
                    else
                        lock_type = Db.DB_LOCK_WRITE;

                    Dbt dbt = new Dbt(objbuf.getBytes());

                    DbLock lock;
                    lock = lockTable.get(locker, Db.DB_LOCK_NOWAIT,
                                         dbt, lock_type);
                    lockid = lock.get_lock_id();
                    did_get = true;
                } else {
                    // Release a lock.
                    String objbuf;
                    objbuf = askForLine(in, System.out,
                                        "input lock to release> ");
                    if (objbuf == null)
                        break;

                    lockid = Integer.parseInt(objbuf, 16);
                    DbLock lock = new DbLock(lockid);
                    lock.put(lockTable);
                    did_get = false;
                }
                // No errors.
                System.out.println("Lock 0x" +
                                   Long.toHexString(lockid) + " " +
                                   (did_get ? "granted" : "released"));
                held += did_get ? 1 : -1;
            }
            catch (DbException dbe) {
                switch (dbe.get_errno()) {
                case Db.DB_LOCK_NOTHELD:
                    System.out.println("You do not hold the lock " +
                                       String.valueOf(lockid));
                    break;
                case Db.DB_LOCK_NOTGRANTED:
                    System.out.println("Lock not granted");
                    break;
                case Db.DB_LOCK_DEADLOCK:
                    System.err.println("LockExample: lock_" +
                                       (did_get ? "get" : "put") +
                                       ": returned DEADLOCK");
                    break;
                default:
                    System.err.println("LockExample: lock_get: " + dbe.toString());
                }
            }
        }
        System.out.println();
        System.out.println("Closing lock region " + String.valueOf(held) +
                           " locks held");
    }

    private static void usage()
    {
        System.err.println("usage: LockExample [-u] [-h home]");
        System.exit(1);
    }

    public static void main(String argv[])
    {
        String home = LOCK_HOME;
        boolean do_unlink = false;

        for (int i = 0; i < argv.length; ++i) {
            if (argv[i].equals("-h")) {
                home = argv[++i];
            }
            else if (argv[i].equals("-u")) {
                do_unlink = true;
            }
            else {
                usage();
            }
        }

        try {
            if (do_unlink) {
                LockExample temp = new LockExample(home);
                DbLockTab.unlink(home, 1, temp);
            }

            LockExample app = new LockExample(home);
            app.run();
            app.appexit();
        }
        catch (DbException dbe) {
            System.err.println("AccessExample: " + dbe.toString());
        }
        catch (Throwable t) {
            System.err.println("AccessExample: " + t.toString());
        }
        System.out.println("LockExample completed");
    }
}
