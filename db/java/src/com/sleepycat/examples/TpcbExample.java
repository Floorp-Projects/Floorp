/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)TpcbExample.java	10.4 (Sleepycat) 4/20/98
 */

package com.sleepycat.examples;

import com.sleepycat.db.*;
import java.util.Calendar;
import java.util.Date;
import java.util.Random;
import java.util.GregorianCalendar;
import java.math.BigDecimal;

//
// This program implements a basic TPC/B driver program.  To create the
// TPC/B database, run with the -i (init) flag.  The number of records
// with which to populate the account, history, branch, and teller tables
// is specified by the a, s, b, and t flags respectively.  To run a TPC/B
// test, use the n flag to indicate a number of transactions to run (note
// that you can run many of these processes in parallel to simulate a
// multiuser test run).
//
class TpcbExample extends DbEnv
{
    // XXX Margo, check these ratios and record sizes.
    //
    public static final int TELLERS_PER_BRANCH = 100;
    public static final int ACCOUNTS_PER_TELLER = 1000;
    public static final int ACCOUNTS = 1000000;
    public static final int BRANCHES = 10;
    public static final int TELLERS = 1000;
    public static final int HISTORY = 1000000;
    public static final int HISTORY_LEN = 100;
    public static final int RECLEN = 100;
    public static final int BEGID = 1000000;

    // used by random_id()
    public static final int ACCOUNT = 0;
    public static final int BRANCH = 1;
    public static final int TELLER = 2;

    private static boolean verbose = false;
    private static final String progname = "TpcbExample";    // Program name.

    // Note: the constructor uses the default DbEnv() constructor,
    // which means that appinit() can be called after all options
    // have been set in the DbEnv.
    //
    public TpcbExample()
    {
        super();
    }

    //
    // Initialize the database to the specified number of accounts, branches,
    // history records, and tellers.
    //
    // Note: num_h was unused in the original ex_tpcb.c example.
    //
    public void
    populate(int num_a, int num_b, int num_h, int num_t)
    {
        Db dbp = null;

        int err;
        int balance, idnum;
        int end_anum, end_bnum, end_tnum;
        int start_anum, start_bnum, start_tnum;

        idnum = BEGID;
        balance = 500000;

        DbInfo dbi = new DbInfo();

        dbi.set_h_nelem((int)num_a);

        try {
            dbp = Db.open("account",
                          Db.DB_HASH, Db.DB_CREATE | Db.DB_TRUNCATE, 0644, this, dbi);
        }
        catch (DbException dbe) {
            errExit(dbe, "Open of account file failed");
        }

        start_anum = idnum;
        populateTable(dbp, idnum, balance, dbi.get_h_nelem(), "account");
        idnum += dbi.get_h_nelem();
        end_anum = idnum - 1;
        try {
            dbp.close(0);
        }
        catch (DbException dbe2) {
            errExit(dbe2, "Account file close failed");
        }

        if (verbose)
            System.out.println("Populated accounts: "
                 + String.valueOf(start_anum) + " - " + String.valueOf(end_anum));

        //
        // Since the number of branches is very small, we want to use very
        // small pages and only 1 key per page.  This is the poor-man's way
        // of getting key locking instead of page locking.
        //
        dbi.set_h_nelem((int)num_b);
        dbi.set_h_ffactor(1);
        dbi.set_pagesize(512);

        try {
            dbp = Db.open("branch",
                          Db.DB_HASH, Db.DB_CREATE | Db.DB_TRUNCATE, 0644,
                          this, dbi);
        }
        catch (DbException dbe3) {
            errExit(dbe3, "Branch file create failed");
        }
        start_bnum = idnum;
        populateTable(dbp, idnum, balance, dbi.get_h_nelem(), "branch");
        idnum += dbi.get_h_nelem();
        end_bnum = idnum - 1;

        try {
            dbp.close(0);
        }
        catch (DbException dbe4) {
            errExit(dbe4, "Close of branch file failed");
        }

        if (verbose)
            System.out.println("Populated branches: "
                 + String.valueOf(start_bnum) + " - " + String.valueOf(end_bnum));

        //
        // In the case of tellers, we also want small pages, but we'll let
        // the fill factor dynamically adjust itself.
        //
        dbi.set_h_nelem((int)num_t);
        dbi.set_h_ffactor(0);
        dbi.set_pagesize(512);

        try {
            dbp = Db.open("teller",
                          Db.DB_HASH, Db.DB_CREATE | Db.DB_TRUNCATE, 0644, this, dbi);
        }
        catch (DbException dbe5) {
            errExit(dbe5, "Teller file create failed");
        }

        start_tnum = idnum;
        populateTable(dbp, idnum, balance, dbi.get_h_nelem(), "teller");
        idnum += dbi.get_h_nelem();
        end_tnum = idnum - 1;

        try {
            dbp.close(0);
        }
        catch (DbException dbe6) {
            errExit(dbe6, "Close of teller file failed");
        }

        if (verbose)
            System.out.println("Populated tellers: "
                 + String.valueOf(start_tnum) + " - " + String.valueOf(end_tnum));

        // start with a fresh DbInfo
        //
        DbInfo histDbi = new DbInfo();

        histDbi.set_re_len(HISTORY_LEN);
        histDbi.set_flags(Db.DB_FIXEDLEN | Db.DB_RENUMBER);
        try {
            dbp = Db.open("history",
                          Db.DB_RECNO, Db.DB_CREATE | Db.DB_TRUNCATE, 0644,
                          this, histDbi);
        }
        catch (DbException dbe7) {
            errExit(dbe7, "Create of history file failed");
        }

        populateHistory(dbp, num_h, num_a, num_b, num_t);

        try {
            dbp.close(0);
        }
        catch (DbException dbe8) {
            errExit(dbe8, "Close of history file failed");
        }
    }

    public void
    populateTable(Db dbp,
                  int start_id, int balance,
                  int nrecs, String msg)
    {
        Defrec drec = new Defrec();

        Dbt kdbt = new Dbt(drec.data);
        kdbt.set_size(4);                  // sizeof(int)
        Dbt ddbt = new Dbt(drec.data);
        ddbt.set_size(drec.data.length);   // uses whole array

        try {
            for (int i = 0; i < nrecs; i++) {
                kdbt.set_recno_key_data(start_id + (int)i);
                drec.set_balance(balance);
                dbp.put(null, kdbt, ddbt, Db.DB_NOOVERWRITE);
            }
        }
        catch (DbException dbe) {
            System.err.println("Failure initializing " + msg + " file: " +
                               dbe.toString());
            System.exit(1);
        }
    }

    public void
    populateHistory(Db dbp, int nrecs,
                                 int anum, int bnum, int tnum)
    {
        Histrec hrec = new Histrec();
        hrec.set_amount(10);

        byte arr[] = new byte[4];                  // sizeof(int)
        int i;
        Dbt kdbt = new Dbt(arr);
        kdbt.set_size(arr.length);
        Dbt ddbt = new Dbt(hrec.data);
        ddbt.set_size(hrec.data.length);

        try {
            for (i = 1; i <= nrecs; i++) {
                kdbt.set_recno_key_data(i);

                hrec.set_aid(random_id(ACCOUNT, anum, bnum, tnum));
                hrec.set_bid(random_id(BRANCH, anum, bnum, tnum));
                hrec.set_tid(random_id(TELLER, anum, bnum, tnum));

                dbp.put(null, kdbt, ddbt, Db.DB_NOOVERWRITE);
            }
        }
        catch (DbException dbe) {
            errExit(dbe, "Failure initializing history file");
        }
    }

    static Random rand = new Random();

    public static int
    random_int(int lo, int hi)
    {
        int ret;
        int t;

        t = rand.nextInt();
        if (t < 0)
            t = -t;
        ret = (int)(((double)t / ((double)(Integer.MAX_VALUE) + 1)) *
                          (hi - lo + 1));
        ret += lo;
        return (ret);
    }

    public static int
    random_id(int type, int accounts, int branches, int tellers)
    {
        int min, max, num;

        max = min = BEGID;
        num = accounts;
        switch(type) {
        case TELLER:
            min += branches;
            num = tellers;
            // Fallthrough
        case BRANCH:
            if (type == BRANCH)
                num = branches;
            min += accounts;
            // Fallthrough
        case ACCOUNT:
            max = min + num - 1;
        }
        return (random_int(min, max));
    }

    public void
    run(int n, int accounts, int branches, int tellers)
    {
        Db adb = null;
        Db bdb = null;
        Db hdb = null;
        Db tdb = null;
        double gtps, itps;
        int failed, ifailed, ret, txns;
        long gus, ius;
        long starttime, curtime, lasttime;

        //
        // Open the database files.
        //
        int err;
        try {
            adb = Db.open("account", Db.DB_UNKNOWN, 0, 0, this, null);
            bdb = Db.open("branch", Db.DB_UNKNOWN, 0, 0, this, null);
            tdb = Db.open("teller", Db.DB_UNKNOWN, 0, 0, this, null);
            hdb = Db.open("history", Db.DB_UNKNOWN, 0, 0, this, null);
        }
        catch (DbException dbe) {
            errExit(dbe, "Open of db files failed");
        }

        txns = failed = ifailed = 0;
        starttime = (new Date()).getTime();
        lasttime = starttime;
        while (n-- >= 0) {
            txns++;
            DbTxnMgr txnmgr = get_tx_info();
            ret = txn(txnmgr, adb, bdb, tdb, hdb, accounts, branches, tellers);
            if (ret != 0) {
                failed++;
                ifailed++;
            }
            if (n % 1000 == 0) {
                curtime = (new Date()).getTime();
                gus = (curtime - starttime) * 1000;
                ius = (curtime - lasttime) * 1000;
                gtps = (double)(txns - failed) /
                    ((double)gus / 1000000);
                itps = (double)(1000 - ifailed) /
                    ((double)ius / 1000000);
                System.out.print(String.valueOf(txns) + " txns " +
                                 String.valueOf(failed) + " failed ");
                System.out.println(showRounded(gtps, 2) + " TPS (gross) " +
                                   showRounded(itps, 2) + " TPS (interval)");
                lasttime = curtime;
                ifailed = 0;
            }
        }

        try {
            adb.close(0);
            bdb.close(0);
            tdb.close(0);
            hdb.close(0);
        }
        catch (DbException dbe2) {
            errExit(dbe2, "Close of db files failed");
        }

        System.out.println((long)txns + " transactions begun "
             + String.valueOf(failed) + " failed");

    }

    //
    // XXX Figure out the appropriate way to pick out IDs.
    //
    public int
    txn(DbTxnMgr txmgr,
        Db adb, Db bdb, Db tdb, Db hdb,
        int anum, int bnum, int tnum)
    {
        Dbc acurs = null;
        Dbc bcurs = null;
        Dbc hcurs = null;
        Dbc tcurs = null;
        DbTxn t = null;

        Defrec rec = new Defrec();
        Histrec hrec = new Histrec();
        int account, branch, teller;

        Dbt d_dbt = new Dbt();
        Dbt d_histdbt = new Dbt();
        Dbt k_dbt = new Dbt();
        Dbt k_histdbt = new Dbt();

        account = random_id(ACCOUNT, anum, bnum, tnum);
        branch = random_id(BRANCH, anum, bnum, tnum);
        teller = random_id(TELLER, anum, bnum, tnum);

        // The history key will not actually be retrieved,
        // but it does need to be set to something.
        byte hist_key[] = new byte[4];
	k_histdbt.set_data(hist_key);
	k_histdbt.set_size(4 /* == sizeof(int)*/);

        byte key_bytes[] = new byte[4];
        k_dbt.set_data(key_bytes);
        k_dbt.set_size(4 /* == sizeof(int)*/);

        d_dbt.set_flags(Db.DB_DBT_USERMEM);
        d_dbt.set_data(rec.data);
        d_dbt.set_ulen(rec.length());

        hrec.set_aid(account);
        hrec.set_bid(branch);
        hrec.set_tid(teller);
        hrec.set_amount(10);
        // Request 0 bytes since we're just positioning.
        d_histdbt.set_flags(Db.DB_DBT_PARTIAL);

        // START TIMING

        try {
            t = txmgr.begin(null);

            acurs = adb.cursor(t);
            bcurs = bdb.cursor(t);
            tcurs = tdb.cursor(t);
            hcurs = hdb.cursor(t);

            // Account record
            k_dbt.set_recno_key_data(account);
            if (acurs.get(k_dbt, d_dbt, Db.DB_SET) != 0)
                throw new TpcbException("acurs get failed");
            rec.set_balance(rec.get_balance() + 10);
            acurs.put(k_dbt, d_dbt, Db.DB_CURRENT);

            // Branch record
            k_dbt.set_recno_key_data(branch);
            if (bcurs.get(k_dbt, d_dbt, Db.DB_SET) != 0)
                throw new TpcbException("bcurs get failed");
            rec.set_balance(rec.get_balance() + 10);
            bcurs.put(k_dbt, d_dbt, Db.DB_CURRENT);

            // Teller record
            k_dbt.set_recno_key_data(teller);
            if (tcurs.get(k_dbt, d_dbt, Db.DB_SET) != 0)
                throw new TpcbException("ccurs get failed");
            rec.set_balance(rec.get_balance() + 10);
            tcurs.put(k_dbt, d_dbt, Db.DB_CURRENT);

            // History record
            d_histdbt.set_flags(0);
            d_histdbt.set_data(hrec.data);
            d_histdbt.set_ulen(hrec.length());
            if (hdb.put(t, k_histdbt, d_histdbt, Db.DB_APPEND) != 0)
		throw(new DbException("put failed"));

            acurs.close();
            bcurs.close();
            tcurs.close();
            hcurs.close();

            t.commit();

            // END TIMING
            return (0);

        }
        catch (Exception e) {
            try {
                if (acurs != null)
                    acurs.close();
                if (bcurs != null)
                    bcurs.close();
                if (tcurs != null)
                    tcurs.close();
                if (hcurs != null)
                    hcurs.close();
                if (t != null)
                    t.abort();
            }
            catch (DbException dbe) {
                // not much we can do here.
            }

            if (verbose) {
                System.out.println("Transaction A=" + String.valueOf(account)
                                   + " B=" + String.valueOf(branch)
                                   + " T=" + String.valueOf(teller) + " failed");
                System.out.println("Reason: " + e.toString());
            }
            return (-1);
        }
    }

    static void errExit(DbException err, String s)
    {
        System.err.print(progname + ": ");
        if (s != null) {
            System.err.print(s + ": ");
        }
        System.err.println(err.toString());
        System.exit(1);
    }


    public static void main(String argv[])
    {
        long seed;
        int accounts, branches, tellers, history;
        boolean iflag;
        int mpool, ntxns;
        String home, endarg;

        home = null;
        accounts = branches = history = tellers = 0;
        mpool = ntxns = 0;
        verbose = false;
        iflag = false;
        seed = (new GregorianCalendar()).get(Calendar.SECOND);

        for (int i = 0; i < argv.length; ++i)
        {
            if (argv[i].equals("-a")) {
                // Number of account records
                if ((accounts = Integer.parseInt(argv[++i])) <= 0)
                    invarg(argv[i]);
            }
            else if (argv[i].equals("-b")) {
                // Number of branch records
                if ((branches = Integer.parseInt(argv[++i])) <= 0)
                    invarg(argv[i]);
            }
            else if (argv[i].equals("-h")) {
                // DB  home.
                home = argv[++i];
            }
            else if (argv[i].equals("-i")) {
                // Initialize the test.
                iflag = true;
            }
            else if (argv[i].equals("-m")) {
                // Bytes in buffer pool
                if ((mpool = Integer.parseInt(argv[++i])) <= 0)
                    invarg(argv[i]);
            }
            else if (argv[i].equals("-n")) {
                // Number of transactions
                if ((ntxns = Integer.parseInt(argv[++i])) <= 0)
                    invarg(argv[i]);
            }
            else if (argv[i].equals("-S")) {
                // Random number seed.
                seed = Long.parseLong(argv[++i]);
                if (seed <= 0)
                    invarg(argv[i]);
            }
            else if (argv[i].equals("-s")) {
                // Number of history records
                if ((history = Integer.parseInt(argv[++i])) <= 0)
                    invarg(argv[i]);
            }
            else if (argv[i].equals("-t")) {
                // Number of teller records
                if ((tellers = Integer.parseInt(argv[++i])) <= 0)
                    invarg(argv[i]);
            }
            else if (argv[i].equals("-v")) {
                // Verbose option.
                verbose = true;
            }
            else
            {
                usage();
            }
        }

        rand.setSeed((int)seed);

        accounts = accounts == 0 ? ACCOUNTS : accounts;
        branches = branches == 0 ? BRANCHES : branches;
        tellers = tellers == 0 ? TELLERS : tellers;
        history = history == 0 ? HISTORY : history;

        if (verbose)
            System.out.println((long)accounts + " Accounts "
                 + String.valueOf(branches) + " Branches "
                 + String.valueOf(tellers) + " Tellers "
                 + String.valueOf(history) + " History");

        // Declaring and setting options does not need
        // to be done in a try block, as it will never
        // raise an exception.
        //
        TpcbExample app = new TpcbExample();
        int flags = Db.DB_CREATE | Db.DB_INIT_MPOOL;

        app.set_error_stream(System.err);
        app.set_errpfx("TpcbExample");

        if (mpool == 0) {
            mpool = 4 * 1024 * 1024;
        }
        app.set_mp_size(mpool);

        if (!iflag) {
            flags |= Db.DB_INIT_TXN | Db.DB_INIT_LOCK | Db.DB_INIT_LOG;
        }

        // Initialize the database environment.
        // Must be done in within a try block, unless you
        // change the error model in the options.
        //
        try {
            app.appinit(home, null, flags);
        }
        catch (DbException dbe1) {
            errExit(dbe1, "appinit failed");
        }

        if (iflag && ntxns != 0) {
            System.err.println("specify only one of -i and -n");
            System.exit(1);
        }
        if (iflag)
            app.populate(accounts, branches, history, tellers);
        if (ntxns != 0)
            app.run(ntxns, accounts, branches, tellers);

        // Shut down the application.

        try {
            app.appexit();
        }
        catch (DbException dbe2) {
            errExit(dbe2, "appexit failed");
        }

        System.exit(0);
    }

    private static void invarg(String str)
    {
        System.err.println("TpcbExample: invalid argument: " + str);
        System.exit(1);
    }

    private static void usage()
    {
        System.err.println("usage: TpcbExample "
             + "[-iv] [-a accounts] [-b branches] [-h home] [-m mpool_size] "
             + "[-n transactions ] [-S seed] [-s history] [-t tellers] ");
        System.exit(1);
    }

    // round 'd' to 'scale' digits, and return result as string
    private String showRounded(double d, int scale)
    {
        return new BigDecimal(d).
            setScale(scale, BigDecimal.ROUND_HALF_DOWN).toString();
    }

    // The byte order is our choice.
    //
    static long get_int_in_array(byte[] array, int offset)
    {
        return
            ((0xff & array[offset+0]) << 0)  |
            ((0xff & array[offset+1]) << 8)  |
            ((0xff & array[offset+2]) << 16) |
            ((0xff & array[offset+3]) << 24);
    }

    // Note: Value needs to be long to avoid sign extension
    static void set_int_in_array(byte[] array, int offset, long value)
    {
        array[offset+0] = (byte)((value >> 0) & 0x0ff);
        array[offset+1] = (byte)((value >> 8) & 0x0ff);
        array[offset+2] = (byte)((value >> 16) & 0x0ff);
        array[offset+3] = (byte)((value >> 24) & 0x0ff);
    }

};

// Simulate the following C struct:
// struct Defrec {
//     u_int32_t   id;
//     u_int32_t   balance;
//     u_int8_t    pad[RECLEN - sizeof(int) - sizeof(int)];
// };

class Defrec
{
    public Defrec()
    {
        data = new byte[TpcbExample.RECLEN];
    }

    public int length()
    {
        return TpcbExample.RECLEN;
    }

    public long get_id()
    {
        return TpcbExample.get_int_in_array(data, 0);
    }

    public void set_id(long value)
    {
        TpcbExample.set_int_in_array(data, 0, value);
    }

    public long get_balance()
    {
        return TpcbExample.get_int_in_array(data, 4);
    }

    public void set_balance(long value)
    {
        TpcbExample.set_int_in_array(data, 4, value);
    }

    static {
        Defrec d = new Defrec();
        d.set_balance(500000);
    }

    public byte[] data;
}

// Simulate the following C struct:
// struct Histrec {
//     u_int32_t   aid;
//     u_int32_t   bid;
//     u_int32_t   tid;
//     u_int32_t   amount;
//     u_int8_t    pad[RECLEN - 4 * sizeof(u_int32_t)];
// };

class Histrec
{
    public Histrec()
    {
        data = new byte[TpcbExample.RECLEN];
    }

    public int length()
    {
        return TpcbExample.RECLEN;
    }

    public long get_aid()
    {
        return TpcbExample.get_int_in_array(data, 0);
    }

    public void set_aid(long value)
    {
        TpcbExample.set_int_in_array(data, 0, value);
    }

    public long get_bid()
    {
        return TpcbExample.get_int_in_array(data, 4);
    }

    public void set_bid(long value)
    {
        TpcbExample.set_int_in_array(data, 4, value);
    }

    public long get_tid()
    {
        return TpcbExample.get_int_in_array(data, 8);
    }

    public void set_tid(long value)
    {
        TpcbExample.set_int_in_array(data, 8, value);
    }

    public long get_amount()
    {
        return TpcbExample.get_int_in_array(data, 12);
    }

    public void set_amount(long value)
    {
        TpcbExample.set_int_in_array(data, 12, value);
    }

    public byte[] data;
}

class TpcbException extends Exception
{
    TpcbException()
    {
        super();
    }

    TpcbException(String s)
    {
        super(s);
    }
}
