/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)TpcbExample.cpp	10.8 (Sleepycat) 4/10/98
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

#ifdef _WIN32
#include <sys/types.h>
#include <sys/timeb.h>
#endif

#include <db_cxx.h>

typedef enum { ACCOUNT, BRANCH, TELLER } FTYPE;

void errExit(int err, const char *);  // show err as errno and exit

void	  invarg(int, char *);
u_int32_t random_id(FTYPE, u_int32_t, u_int32_t, u_int32_t);
u_int32_t random_int(u_int32_t, u_int32_t);
static void	  usage(void);

int verbose;
char *progname = "TpcbExample";                            // Program name.

#ifdef _WIN32
/* Simulation of gettimeofday */

struct timeval {
    long tv_sec;
    long tv_usec;
};

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

int gettimeofday(struct timeval *tp, struct timezone *tzp)
{
    struct _timeb tb;

    _ftime(&tb);
    if (tp != 0) {
        tp->tv_sec = tb.time;
        tp->tv_usec = tb.millitm * 1000;
    }
    if (tzp != 0) {
        tzp->tz_minuteswest = tb.timezone;
        tzp->tz_dsttime = tb.dstflag;
    }
    return 0;
}

#endif

class TpcbExample : public DbEnv
{
public:
    void populate(int, int, int, int);
    void run(int, int, int, int);
    int txn(DbTxnMgr *,
            Db *, Db *, Db *, Db *,
            int, int, int);
    void populateHistory(Db *, unsigned int, u_int32_t,
                         u_int32_t, u_int32_t);
    void populateTable(Db *, u_int32_t, u_int32_t, unsigned int, char *);

    // Note: the constructor uses the default DbEnv() constructor,
    // which means that appinit() can be called after all options
    // have been set in the DbEnv.
    //
    TpcbExample() : DbEnv() {}

private:
    static const char FileName[];

    // no need for copy and assignment
    TpcbExample(const TpcbExample &);
    operator = (const TpcbExample &);
};

//
// This program implements a basic TPC/B driver program.  To create the
// TPC/B database, run with the -i (init) flag.  The number of records
// with which to populate the account, history, branch, and teller tables
// is specified by the a, s, b, and t flags respectively.  To run a TPC/B
// test, use the n flag to indicate a number of transactions to run (note
// that you can run many of these processes in parallel to simulate a
// multiuser test run).
//
// XXX Margo, check these ratios and record sizes.
//
#define TELLERS_PER_BRANCH      100
#define ACCOUNTS_PER_TELLER     1000

#define ACCOUNTS 1000000
#define	BRANCHES 10
#define TELLERS 1000
#define HISTORY 1000000
#define HISTORY_LEN 100
#define RECLEN 100
#define BEGID   1000000

struct Defrec {
    u_int32_t   id;
    u_int32_t   balance;
    u_int8_t    pad[RECLEN - sizeof(u_int32_t) - sizeof(u_int32_t)];
};

struct Histrec {
    u_int32_t   aid;
    u_int32_t   bid;
    u_int32_t   tid;
    u_int32_t   amount;
    u_int8_t    pad[RECLEN - 4 * sizeof(u_int32_t)];
};

int
main(int argc, char *argv[])
{
    unsigned long seed;
    int accounts, branches, tellers, history;
    int iflag, mpool, ntxns;
    char *home, *endarg;

    home = NULL;
    accounts = branches = history = tellers = 0;
    mpool = ntxns = 0;
    verbose = 0;
    iflag = 0;
    seed = (unsigned long)getpid();

    for (int i = 1; i < argc; ++i)
    {

        if (strcmp(argv[i], "-a") == 0) {
            // Number of account records
            if ((accounts = atoi(argv[++i])) <= 0)
                invarg('a', argv[i]);
        }
        else if (strcmp(argv[i], "-b") == 0) {
            // Number of branch records
            if ((branches = atoi(argv[++i])) <= 0)
                invarg('b', argv[i]);
        }
        else if (strcmp(argv[i], "-h") == 0) {
            // DB  home.
            home = argv[++i];
        }
        else if (strcmp(argv[i], "-i") == 0) {
            // Initialize the test.
            iflag = 1;
        }
        else if (strcmp(argv[i], "-m") == 0) {
            // Bytes in buffer pool
            if ((mpool = atoi(argv[++i])) <= 0)
                invarg('m', argv[i]);
        }
        else if (strcmp(argv[i], "-n") == 0) {
            // Number of transactions
            if ((ntxns = atoi(argv[++i])) <= 0)
                invarg('n', argv[i]);
        }
        else if (strcmp(argv[i], "-S") == 0) {
            // Random number seed.
            seed = strtoul(argv[++i], &endarg, 0);
            if (*endarg != '\0')
                invarg('S', argv[i]);
        }
        else if (strcmp(argv[i], "-s") == 0) {
            // Number of history records
            if ((history = atoi(argv[++i])) <= 0)
                invarg('s', argv[i]);
        }
        else if (strcmp(argv[i], "-t") == 0) {
            // Number of teller records
            if ((tellers = atoi(argv[++i])) <= 0)
                invarg('t', argv[i]);
        }
        else if (strcmp(argv[i], "-v") == 0) {
            // Verbose option.
            verbose = 1;
        }
        else
        {
            usage();
        }
    }

    srand((unsigned int)seed);

    accounts = accounts == 0 ? ACCOUNTS : accounts;
    branches = branches == 0 ? BRANCHES : branches;
    tellers = tellers == 0 ? TELLERS : tellers;
    history = history == 0 ? HISTORY : history;

    if (verbose)
        cout << (long)accounts << " Accounts "
             << (long)branches << " Branches "
             << (long)tellers << " Tellers "
             << (long)history << " History\n";

    // Declaring and setting options does not need
    // to be done in a try block, as it will never
    // raise an exception.
    //
    TpcbExample app;
    u_int32_t flags = DB_CREATE | DB_INIT_MPOOL;

    app.set_error_stream(&cerr);
    app.set_errpfx("TpcbExample");

    if (mpool == 0) {
        mpool = 4 * 1024 * 1024;
    }
    app.set_mp_size(mpool);

    if (!iflag) {
        flags |= DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG;
    }

    try {
        // Initialize the database environment.
        // Must be done in within a try block, unless you
        // change the error model in the options.
        //
        app.appinit(home, NULL, flags);

        if (iflag && ntxns != 0) {
            cerr << "specify only one of -i and -n\n";
            exit(1);
        }
        if (iflag)
            app.populate(accounts, branches, history, tellers);
        if (ntxns != 0)
            app.run(ntxns, accounts, branches, tellers);

        return 0;
    }
    catch (DbException &dbe)
    {
        cerr << "TpcbExample: " << dbe.what() << "\n";
        return 1;
    }
}

void
invarg(int arg, char *str)
{
    cerr << "TpcbExample: invalid argument for -"
         << (char)arg << ": " << str << "\n";
    exit(1);
}

static void
usage()
{
    cerr << "usage: TpcbExample "
         << "[-iv] [-a accounts] [-b branches] [-h home] [-m mpool_size] "
         << "[-n transactions ] [-S seed] [-s history] [-t tellers] \n";
    exit(1);
}

//
// Initialize the database to the specified number of accounts, branches,
// history records, and tellers.
//
// Note: num_h was unused in the original ex_tpcb.c example.
//
void
TpcbExample::populate(int num_a, int num_b, int num_h, int num_t)
{
    Db *dbp;

    int err;
    u_int32_t balance, idnum;
    u_int32_t end_anum, end_bnum, end_tnum;
    u_int32_t start_anum, start_bnum, start_tnum;

    idnum = BEGID;
    balance = 500000;

    DbInfo dbi;

    dbi.set_h_nelem((unsigned int)num_a);

    if ((err = Db::open("account",
         DB_HASH, DB_CREATE | DB_TRUNCATE, 0644, this, &dbi, &dbp)) != 0) {
        errExit(err, "Open of account file failed");
    }

    start_anum = idnum;
    populateTable(dbp, idnum, balance, dbi.get_h_nelem(), "account");
    idnum += dbi.get_h_nelem();
    end_anum = idnum - 1;
    if ((err = dbp->close(0)) != 0) {
        errExit(err, "Account file close failed");
    }
    if (verbose)
        cout << "Populated accounts: "
             << (long)start_anum << " - " << (long)end_anum << "\n";

    //
    // Since the number of branches is very small, we want to use very
    // small pages and only 1 key per page.  This is the poor-man's way
    // of getting key locking instead of page locking.
    //
    dbi.set_h_nelem((unsigned int)num_b);
    dbi.set_h_ffactor(1);
    dbi.set_pagesize(512);

    if ((err = Db::open("branch",
	  DB_HASH, DB_CREATE | DB_TRUNCATE, 0644, this, &dbi, &dbp)) != 0) {
        errExit(err, "Branch file create failed");
    }
    start_bnum = idnum;
    populateTable(dbp, idnum, balance, dbi.get_h_nelem(), "branch");
    idnum += dbi.get_h_nelem();
    end_bnum = idnum - 1;
    if ((err = dbp->close(0)) != 0) {
        errExit(err, "Close of branch file failed");
    }

    if (verbose)
        cout << "Populated branches: "
             << (long)start_bnum << " - " << (long)end_bnum << "\n";

    //
    // In the case of tellers, we also want small pages, but we'll let
    // the fill factor dynamically adjust itself.
    //
    dbi.set_h_nelem((unsigned int)num_t);
    dbi.set_h_ffactor(0);
    dbi.set_pagesize(512);

    if ((err = Db::open("teller",
         DB_HASH, DB_CREATE | DB_TRUNCATE, 0644, this, &dbi, &dbp)) != 0) {
        errExit(err, "Teller file create failed");
    }

    start_tnum = idnum;
    populateTable(dbp, idnum, balance, dbi.get_h_nelem(), "teller");
    idnum += dbi.get_h_nelem();
    end_tnum = idnum - 1;
    if ((err = dbp->close(0)) != 0) {
        errExit(err, "Close of teller file failed");
    }
    if (verbose)
        cout << "Populated tellers: "
             << (long)start_tnum << " - " << (long)end_tnum << "\n";

    // start with a fresh DbInfo
    //
    DbInfo histDbi;

    histDbi.set_re_len(HISTORY_LEN);
    histDbi.set_flags(DB_FIXEDLEN | DB_RENUMBER);
    if ((err = Db::open("history", DB_RECNO, DB_CREATE | DB_TRUNCATE, 0644,
         this, &histDbi, &dbp)) != 0) {
        errExit(err, "Create of history file failed");
    }

    populateHistory(dbp, num_h, num_a, num_b, num_t);
    if ((err = dbp->close(0)) != 0) {
        errExit(err, "Close of history file failed");
    }
}

void
TpcbExample::populateTable(Db *dbp,
                           u_int32_t start_id, u_int32_t balance,
                           unsigned int nrecs, char *msg)
{
    Defrec drec;
    memset(&drec.pad[0], 1, sizeof(drec.pad));

    Dbt kdbt(&drec.id, sizeof(u_int32_t));
    Dbt ddbt(&drec, sizeof(drec));

    for (unsigned int i = 0; i < nrecs; i++) {
        drec.id = start_id + (u_int32_t)i;
        drec.balance = balance;
        int err;
        if ((err =
             dbp->put(NULL, &kdbt, &ddbt, DB_NOOVERWRITE)) != 0) {
            cerr << "Failure initializing " << msg << " file: "
                 << strerror(err) << "\n";
            exit(1);
        }
    }
}

void
TpcbExample::populateHistory(Db *dbp, unsigned int nrecs,
                             u_int32_t anum, u_int32_t bnum, u_int32_t tnum)
{
    Histrec hrec;
    memset(&hrec.pad[0], 1, sizeof(hrec.pad));
    hrec.amount = 10;

    u_int32_t i;
    Dbt kdbt(&i, sizeof(u_int32_t));
    Dbt ddbt(&hrec, sizeof(hrec));

    for (i = 1; i <= nrecs; i++) {
        hrec.aid = random_id(ACCOUNT, anum, bnum, tnum);
        hrec.bid = random_id(BRANCH, anum, bnum, tnum);
        hrec.tid = random_id(TELLER, anum, bnum, tnum);

        int err;
        if ((err = dbp->put(NULL, &kdbt, &ddbt, DB_NOOVERWRITE)) != 0) {
            errExit(err, "Failure initializing history file");
	}
    }
}

u_int32_t
random_int(u_int32_t lo, u_int32_t hi)
{
    u_int32_t ret;
    int t;

    t = rand();
    ret = (u_int32_t)(((double)t / ((double)(RAND_MAX) + 1)) *
                      (hi - lo + 1));
    ret += lo;
    return (ret);
}

u_int32_t
random_id(FTYPE type, u_int32_t accounts, u_int32_t branches, u_int32_t tellers)
{
    u_int32_t min, max, num;

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

void
TpcbExample::run(int n, int accounts, int branches, int tellers)
{
    Db *adb, *bdb, *hdb, *tdb;
    double gtps, itps;
    int failed, ifailed, ret, txns, gus, ius;
    struct timeval starttime, curtime, lasttime;

    //
    // Open the database files.
    //
    int err;
    if ((err = Db::open("account", DB_UNKNOWN, 0, 0, this, NULL, &adb)) != 0)
        errExit(err, "Open of account file failed");

    if ((err = Db::open("branch", DB_UNKNOWN, 0, 0, this, NULL, &bdb)) != 0)
        errExit(err, "Open of branch file failed");

    if ((err = Db::open("teller", DB_UNKNOWN, 0, 0, this, NULL, &tdb)) != 0)
        errExit(err, "Open of teller file failed");

    if ((err = Db::open("history", DB_UNKNOWN, 0, 0, this, NULL, &hdb)) != 0)
        errExit(err, "Open of history file failed");

    txns = failed = ifailed = 0;
    (void)gettimeofday(&starttime, NULL);
    lasttime = starttime;
    while (n-- >= 0) {
        txns++;
        DbTxnMgr *txnmgr = get_tx_info();
        ret = txn(txnmgr, adb, bdb, tdb, hdb, accounts, branches, tellers);
        if (ret != 0) {
            failed++;
            ifailed++;
        }
        if (n % 1000 == 0) {
            (void)gettimeofday(&curtime, NULL);
            gus = curtime.tv_usec >= starttime.tv_usec ?
                curtime.tv_usec - starttime.tv_usec +
                1000000 * (curtime.tv_sec - starttime.tv_sec) :
                1000000 + curtime.tv_usec - starttime.tv_usec +
                1000000 * (curtime.tv_sec - starttime.tv_sec - 1);
            ius = curtime.tv_usec >= lasttime.tv_usec ?
                curtime.tv_usec - lasttime.tv_usec +
                1000000 * (curtime.tv_sec - lasttime.tv_sec) :
                1000000 + curtime.tv_usec - lasttime.tv_usec +
                1000000 * (curtime.tv_sec - lasttime.tv_sec - 1);
            gtps = (double)(txns - failed) /
                ((double)gus / 1000000);
            itps = (double)(1000 - ifailed) /
                ((double)ius / 1000000);
            printf("%d txns %d failed ", txns, failed);
            printf("%6.2f TPS (gross) %6.2f TPS (interval)\n",
                   gtps, itps);
            lasttime = curtime;
            ifailed = 0;
        }
    }

    (void)adb->close(0);
    (void)bdb->close(0);
    (void)tdb->close(0);
    (void)hdb->close(0);

    cout << (long)txns << " transactions begun "
         << (long)failed << " failed\n";
}

//
// XXX Figure out the appropriate way to pick out IDs.
//
int
TpcbExample::txn(DbTxnMgr *txmgr,
                 Db *adb, Db *bdb, Db *tdb, Db *hdb,
                 int anum, int bnum, int tnum)
{
    Dbc *acurs = NULL;
    Dbc *bcurs = NULL;
    Dbc *hcurs = NULL;
    Dbc *tcurs = NULL;
    DbTxn *t = NULL;

    Defrec rec;
    Histrec hrec;
    int account, branch, teller;

    Dbt d_dbt;
    Dbt d_histdbt;
    Dbt k_dbt;
    Dbt k_histdbt;

    //
    // XXX We could move a lot of this into the driver to make this
    // faster.
    //
    account = random_id(ACCOUNT, anum, bnum, tnum);
    branch = random_id(BRANCH, anum, bnum, tnum);
    teller = random_id(TELLER, anum, bnum, tnum);

    k_dbt.set_size(sizeof(int));

    d_dbt.set_flags(DB_DBT_USERMEM);
    d_dbt.set_data(&rec);
    d_dbt.set_ulen(sizeof(rec));

    hrec.aid = account;
    hrec.bid = branch;
    hrec.tid = teller;
    hrec.amount = 10;
    // Request 0 bytes since we're just positioning.
    d_histdbt.set_flags(DB_DBT_PARTIAL);

    // START TIMING
    if (txmgr->begin(NULL, &t) != 0)
        goto err;

    if (adb->cursor(t, &acurs) != 0 ||
        bdb->cursor(t, &bcurs) != 0 ||
        tdb->cursor(t, &tcurs) != 0 ||
        hdb->cursor(t, &hcurs) != 0)
        goto err;

    // Account record
    k_dbt.set_data(&account);
    if (acurs->get(&k_dbt, &d_dbt, DB_SET) != 0)
        goto err;
    rec.balance += 10;
    if (acurs->put(&k_dbt, &d_dbt, DB_CURRENT) != 0)
        goto err;

    // Branch record
    k_dbt.set_data(&branch);
    if (bcurs->get(&k_dbt, &d_dbt, DB_SET) != 0)
        goto err;
    rec.balance += 10;
    if (bcurs->put(&k_dbt, &d_dbt, DB_CURRENT) != 0)
        goto err;

    // Teller record
    k_dbt.set_data(&teller);
    if (tcurs->get(&k_dbt, &d_dbt, DB_SET) != 0)
        goto err;
    rec.balance += 10;
    if (tcurs->put(&k_dbt, &d_dbt, DB_CURRENT) != 0)
        goto err;

    // History record
    if (hcurs->get(&k_histdbt, &d_histdbt, DB_LAST) != 0)
        goto err;
    d_histdbt.set_flags(0);
    d_histdbt.set_data(&hrec);
    d_histdbt.set_ulen(sizeof(hrec));
    if (hcurs->put(&k_histdbt, &d_histdbt, DB_AFTER) != 0)
        goto err;

    if (acurs->close() != 0 || bcurs->close() != 0 ||
        tcurs->close() != 0 || hcurs->close() != 0)
        goto err;

    if (t->commit() != 0)
        goto err;

    // END TIMING
    return (0);

  err:
    if (acurs != NULL)
        (void)acurs->close();
    if (bcurs != NULL)
        (void)bcurs->close();
    if (tcurs != NULL)
        (void)tcurs->close();
    if (hcurs != NULL)
        (void)hcurs->close();
    if (t != NULL)
        (void)t->abort();

    if (verbose)
        cout << "Transaction A=" << (long)account
             << " B=" << (long)branch
             << " T=" << (long)teller << " failed\n";
    return (-1);
}

void errExit(int err, const char *s)
{
    cerr << progname << ": ";
    if (s != NULL) {
        cerr << s << ": ";
    }
    cerr << strerror(err) << "\n";
    exit(1);
}
