/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)ex_tpcb.c	10.24 (Sleepycat) 4/10/98
 */

#include "config.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <sys/types.h>
#include <sys/timeb.h>
#endif


#include <db.h>

typedef enum { ACCOUNT, BRANCH, TELLER } FTYPE;

DB_ENV	 *db_init(char *, int, int, int);
void	  hpopulate(DB *, u_int, u_int32_t, u_int32_t, u_int32_t);
void	  invarg(int, char *);
int	  main __P((int, char *[]));
void	  populate(DB *, u_int32_t, u_int32_t, u_int, char *);
u_int32_t random_id(FTYPE, u_int32_t, u_int32_t, u_int32_t);
u_int32_t random_int(u_int32_t, u_int32_t);
void	  tp_populate(DB_ENV *, int, int, int, int);
void	  tp_run(DB_ENV *, int, int, int, int);
int	  tp_txn(DB_TXNMGR *, DB *, DB *, DB *, DB *, int, int, int);
void	  usage(void);

int verbose;
const char
	*progname = "ex_tpcb";				/* Program name. */

/*
 * This program implements a basic TPC/B driver program.  To create the
 * TPC/B database, run with the -i (init) flag.  The number of records
 * with which to populate the account, history, branch, and teller tables
 * is specified by the a, s, b, and t flags respectively.  To run a TPC/B
 * test, use the n flag to indicate a number of transactions to run (note
 * that you can run many of these processes in parallel to simulate a
 * multiuser test run).
 */
#define	TELLERS_PER_BRANCH	10
#define	ACCOUNTS_PER_TELLER	10000
#define	HISTORY_PER_BRANCH	2592000

/*
 * The default configuration that adheres to TPCB scaling rules requires
 * nearly 3 GB of space.  To avoid requiring that much space for testing,
 * we set the parameters much lower.  If you want to run a valid 10 TPS
 * configuration, define VALID_SCALING.
 */
#ifdef VALID_SCALING
#define	ACCOUNTS	 1000000
#define	BRANCHES	      10
#define	TELLERS		     100
#define HISTORY		25920000
#else
#define	ACCOUNTS	  100000
#define	BRANCHES	      10
#define	TELLERS		     100
#define HISTORY		  259200
#endif

#define HISTORY_LEN	    100
#define	RECLEN		    100
#define	BEGID		1000000

typedef struct _defrec {
	u_int32_t	id;
	u_int32_t	balance;
	u_int8_t	pad[RECLEN - sizeof(u_int32_t) - sizeof(u_int32_t)];
} defrec;

typedef struct _histrec {
	u_int32_t	aid;
	u_int32_t	bid;
	u_int32_t	tid;
	u_int32_t	amount;
	u_int8_t	pad[RECLEN - 4 * sizeof(u_int32_t)];
} histrec;

#ifdef _WIN32
/* Simulation of UNIX gettimeofday(2). */
struct timeval {
	long tv_sec;
	long tv_usec;
};

struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};

int
gettimeofday(tp, tzp)
	struct timeval *tp;
	struct timezone *tzp;
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
	return (0);
}
#endif

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	DB_ENV *dbenv;
	u_long seed;
	int accounts, branches, tellers, history;
	int ch, iflag, mpool, ntxns, txn_no_sync;
	char *home, *endarg;

	home = NULL;
	accounts = branches = history = tellers = 0;
	txn_no_sync = 0;
	mpool = ntxns = 0;
	verbose = 0;
	iflag = 0;
	seed = (u_long)getpid();
	while ((ch = getopt(argc, argv, "a:b:fh:im:n:S:s:t:v")) != EOF)
		switch (ch) {
		case 'a':			/* Number of account records */
			if ((accounts = atoi(optarg)) <= 0)
				invarg(ch, optarg);
			break;
		case 'b':			/* Number of branch records */
			if ((branches = atoi(optarg)) <= 0)
				invarg(ch, optarg);
			break;
		case 'c':			/* Cachesize in bytes */
			if ((mpool = atoi(optarg)) <= 0)
				invarg(ch, optarg);
			break;
		case 'f':			/* Fast mode: no txn sync. */
			txn_no_sync = 1;
			break;
		case 'h':			/* DB  home. */
			home = optarg;
			break;
		case 'i':			/* Initialize the test. */
			iflag = 1;
			break;
		case 'n':			/* Number of transactions */
			if ((ntxns = atoi(optarg)) <= 0)
				invarg(ch, optarg);
			break;
		case 'S':			/* Random number seed. */
			seed = strtoul(optarg, &endarg, 0);
			if (*endarg != '\0')
				invarg(ch, optarg);
			break;
		case 's':			/* Number of history records */
			if ((history = atoi(optarg)) <= 0)
				invarg(ch, optarg);
			break;
		case 't':			/* Number of teller records */
			if ((tellers = atoi(optarg)) <= 0)
				invarg(ch, optarg);
			break;
		case 'v':			/* Verbose option. */
			verbose = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	srand((u_int)seed);

	/* Initialize the database environment. */
	dbenv = db_init(home, mpool, iflag, txn_no_sync ? DB_TXN_NOSYNC : 0);

	accounts = accounts == 0 ? ACCOUNTS : accounts;
	branches = branches == 0 ? BRANCHES : branches;
	tellers = tellers == 0 ? TELLERS : tellers;
	history = history == 0 ? HISTORY : history;

	if (verbose)
		printf("%ld Accounts, %ld Branches, %ld Tellers, %ld History\n",
		    (long)accounts, (long)branches,
		    (long)tellers, (long)history);

	if (iflag) {
		if (ntxns != 0)
			usage();
		tp_populate(dbenv, accounts, branches, history, tellers);
	} else {
		if (ntxns == 0)
			usage();
		tp_run(dbenv, ntxns, accounts, branches, tellers);
	}

	if ((errno = db_appexit(dbenv)) != 0) {
		fprintf(stderr, "%s: %s\n", progname, strerror(errno));
		exit (1);
	}

	return (0);
}

/*
 * db_init --
 *	Initialize the environment.
 */
DB_ENV *
db_init(home, mp_size, initializing, flags)
	char *home;
	int mp_size;
	int initializing;
	int flags;
{
	DB_ENV *dbenv;
	u_int32_t local_flags;

	/* Rely on calloc to initialize the structure. */
	if ((dbenv = (DB_ENV *)calloc(sizeof(DB_ENV), 1)) == NULL) {
		fprintf(stderr, "%s: ", progname);
		if (errno < 0)
			fprintf(stderr, "returned %ld\n", (long)errno);
		else
			fprintf(stderr, "%s\n", strerror(errno));
		exit (1);
	}

	dbenv->db_errfile = stderr;
	dbenv->db_errpfx = progname;
	dbenv->mp_size = mp_size == 0 ? 4 * 1024 * 1024 : (size_t)mp_size;

	local_flags = flags | DB_CREATE | (initializing ? DB_INIT_MPOOL :
	    DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL);
	if ((errno = db_appinit(home, NULL, dbenv, local_flags)) != 0) {
		fprintf(stderr, "%s: db_appinit: ", progname);
		if (errno < 0)
			fprintf(stderr, "returned %ld\n", (long)errno);
		else
			fprintf(stderr, "%s\n", strerror(errno));
		exit (1);
	}

	return (dbenv);
}

void
invarg(arg, str)
	int arg;
	char *str;
{
	(void)fprintf(stderr,
	    "%s: invalid argument for -%c: %s\n", progname, arg, str);
	exit (1);
}

void
usage()
{
	char *a1, *a2;

	a1 = "[-v] [-a accounts] [-b branches] [-h home]\n";
	a2 = "          [-m mpool_size] [-S seed] [-s history] [-t tellers]";
	(void)fprintf(stderr, "usage: %s -i %s %s\n", progname, a1, a2);
	(void)fprintf(stderr,
	    "       %s -n transactions %s %s\n", progname, a1, a2);
	exit(1);
}

/*
 * Initialize the database to the specified number of accounts, branches,
 * history records, and tellers.
 */
void
tp_populate(env, num_a, num_b, num_h, num_t)
	DB_ENV *env;
	int num_a, num_b, num_h, num_t;
{
	DB *dbp;
	DB_INFO dbi;
	u_int32_t balance, idnum;
	u_int32_t end_anum, end_bnum, end_tnum;
	u_int32_t start_anum, start_bnum, start_tnum;

	idnum = BEGID;
	balance = 500000;

	memset(&dbi, 0, sizeof(dbi));

	dbi.h_nelem = (u_int)num_a;
	if ((errno = db_open("account",
	    DB_HASH, DB_CREATE | DB_TRUNCATE, 0644, env, &dbi, &dbp)) != 0) {
		fprintf(stderr, "%s: Account file open failed: ", progname);
		if (errno < 0)
			fprintf(stderr, "returned %ld\n", (long)errno);
		else
			fprintf(stderr, "%s\n", strerror(errno));
		exit (1);
	}

	start_anum = idnum;
	populate(dbp, idnum, balance, dbi.h_nelem, "account");
	idnum += dbi.h_nelem;
	end_anum = idnum - 1;
	if ((errno = dbp->close(dbp, 0)) != 0) {
		fprintf(stderr, "%s: Account file close failed: ", progname);
		if (errno < 0)
			fprintf(stderr, "returned %ld\n", (long)errno);
		else
			fprintf(stderr, "%s\n", strerror(errno));
		exit (1);
	}
	if (verbose)
		printf("Populated accounts: %ld - %ld\n",
		    (long)start_anum, (long)end_anum);


	/*
	 * Since the number of branches is very small, we want to use very
	 * small pages and only 1 key per page.  This is the poor-man's way
	 * of getting key locking instead of page locking.
	 */
	dbi.h_nelem = (u_int)num_b;
	dbi.h_ffactor = 1;
	dbi.db_pagesize = 512;
	if ((errno = db_open("branch",
	    DB_HASH, DB_CREATE | DB_TRUNCATE, 0644, env, &dbi, &dbp)) != 0) {
		fprintf(stderr, "%s: Branch file create failed: ", progname);
		if (errno < 0)
			fprintf(stderr, "returned %ld\n", (long)errno);
		else
			fprintf(stderr, "%s\n", strerror(errno));
		exit (1);
	}
	start_bnum = idnum;
	populate(dbp, idnum, balance, dbi.h_nelem, "branch");
	idnum += dbi.h_nelem;
	end_bnum = idnum - 1;
	if ((errno = dbp->close(dbp, 0)) != 0) {
		fprintf(stderr, "%s: Close of branch file failed: ", progname);
		if (errno < 0)
			fprintf(stderr, "returned %ld\n", (long)errno);
		else
			fprintf(stderr, "%s\n", strerror(errno));
		exit (1);
	}

	if (verbose)
		printf("Populated branches: %ld - %ld\n",
		    (long)start_bnum, (long)end_bnum);


	/*
	 * In the case of tellers, we also want small pages, but we'll let
	 * the fill factor dynamically adjust itself.
	 */
	dbi.h_nelem = (u_int)num_t;
	dbi.h_ffactor = 0;
	dbi.db_pagesize = 512;
	if ((errno = db_open("teller",
	    DB_HASH, DB_CREATE | DB_TRUNCATE, 0644, env, &dbi, &dbp)) != 0) {
		fprintf(stderr, "%s: Teller file create failed: ", progname);
		if (errno < 0)
			fprintf(stderr, "returned %ld\n", (long)errno);
		else
			fprintf(stderr, "%s\n", strerror(errno));
		exit (1);
	}

	start_tnum = idnum;
	populate(dbp, idnum, balance, dbi.h_nelem, "teller");
	idnum += dbi.h_nelem;
	end_tnum = idnum - 1;
	if ((errno = dbp->close(dbp, 0)) != 0) {
		fprintf(stderr, "%s: Teller file close failed: ", progname);
		if (errno < 0)
			fprintf(stderr, "returned %ld\n", (long)errno);
		else
			fprintf(stderr, "%s\n", strerror(errno));
		exit (1);
	}
	if (verbose)
		printf("Populated tellers: %ld - %ld\n",
		    (long)start_tnum, (long)end_tnum);

	memset(&dbi, 0, sizeof(dbi));
	dbi.re_len = HISTORY_LEN;
	dbi.flags = DB_FIXEDLEN;
	if ((errno = db_open("history",
	    DB_RECNO, DB_CREATE | DB_TRUNCATE, 0644, env, &dbi, &dbp)) != 0) {
		fprintf(stderr, "%s: History file create failed: ", progname);
		if (errno < 0)
			fprintf(stderr, "returned %ld\n", (long)errno);
		else
			fprintf(stderr, "%s\n", strerror(errno));
		exit (1);
	}

	hpopulate(dbp, num_h, num_a, num_b, num_t);
	if ((errno = dbp->close(dbp, 0)) != 0) {
		fprintf(stderr, "%s: History file close failed: ", progname);
		if (errno < 0)
			fprintf(stderr, "returned %ld\n", (long)errno);
		else
			fprintf(stderr, "%s\n", strerror(errno));
		exit (1);
	}
}

void
populate(dbp, start_id, balance, nrecs, msg)
	DB *dbp;
	u_int32_t start_id, balance;
	u_int nrecs;
	char *msg;
{
	DBT kdbt, ddbt;
	defrec drec;
	u_int i;

	kdbt.flags = 0;
	kdbt.data = &drec.id;
	kdbt.size = sizeof(u_int32_t);
	ddbt.flags = 0;
	ddbt.data = &drec;
	ddbt.size = sizeof(drec);
	memset(&drec.pad[0], 1, sizeof(drec.pad));

	for (i = 0; i < nrecs; i++) {
		drec.id = start_id + (u_int32_t)i;
		drec.balance = balance;
		if ((errno =
		    (dbp->put)(dbp, NULL, &kdbt, &ddbt, DB_NOOVERWRITE)) != 0) {
			fprintf(stderr, "%s: Failure initializing %s file\n",
			    progname, msg);
			exit (1);
		}
	}
}

void
hpopulate(dbp, nrecs, anum, bnum, tnum)
	DB *dbp;
	u_int nrecs;
	u_int32_t anum, bnum, tnum;
{
	DBT kdbt, ddbt;
	histrec hrec;
	db_recno_t key;
	u_int32_t i;

	memset(&kdbt, 0, sizeof(kdbt));
	memset(&ddbt, 0, sizeof(ddbt));
	ddbt.data = &hrec;
	ddbt.size = sizeof(hrec);
	kdbt.data = &key;
	kdbt.size = sizeof(key);
	memset(&hrec.pad[0], 1, sizeof(hrec.pad));
	hrec.amount = 10;

	for (i = 1; i <= nrecs; i++) {
		hrec.aid = random_id(ACCOUNT, anum, bnum, tnum);
		hrec.bid = random_id(BRANCH, anum, bnum, tnum);
		hrec.tid = random_id(TELLER, anum, bnum, tnum);
		if ((errno = dbp->put(dbp,
		    NULL, &kdbt, &ddbt, DB_APPEND | DB_NOOVERWRITE)) != 0) {
			fprintf(stderr,
			    "%s: Failure initializing history file: ",
			    progname);
			if (errno < 0)
				fprintf(stderr, "returned %ld\n", (long)errno);
			else
				fprintf(stderr, "%s\n", strerror(errno));
			exit (1);
		}
	}
}

u_int32_t
random_int(lo, hi)
	u_int32_t lo, hi;
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
random_id(type, accounts, branches, tellers)
	FTYPE type;
	u_int32_t accounts, branches, tellers;
{
	u_int32_t min, max, num;

	max = min = BEGID;
	num = accounts;
	switch(type) {
	case TELLER:
		min += branches;
		num = tellers;
		/* FALLTHROUGH */
	case BRANCH:
		if (type == BRANCH)
			num = branches;
		min += accounts;
		/* FALLTHROUGH */
	case ACCOUNT:
		max = min + num - 1;
	}
	return (random_int(min, max));
}

void
tp_run(dbenv, n, accounts, branches, tellers)
	DB_ENV *dbenv;
	int n, accounts, branches, tellers;
{
	DB *adb, *bdb, *hdb, *tdb;
	double gtps, itps;
	int failed, ifailed, ret, txns, gus, ius;
	struct timeval starttime, curtime, lasttime;
#ifndef _WIN32
	pid_t pid;

	pid = getpid();
#else
	int pid;

	pid = 0;
#endif

	/*
	 * Open the database files.
	 */
	if ((errno =
	    db_open("account", DB_UNKNOWN, 0, 0, dbenv, NULL, &adb)) != 0) {
		fprintf(stderr, "%s: Open of account file failed: ", progname);
		if (errno < 0)
			fprintf(stderr, "returned %ld\n", (long)errno);
		else
			fprintf(stderr, "%s\n", strerror(errno));
		exit (1);
	}
	if ((errno =
	    db_open("branch", DB_UNKNOWN, 0, 0, dbenv, NULL, &bdb)) != 0) {
		fprintf(stderr, "%s: Open of branch file failed: ", progname);
		if (errno < 0)
			fprintf(stderr, "returned %ld\n", (long)errno);
		else
			fprintf(stderr, "%s\n", strerror(errno));
		exit (1);
	}
	if ((errno =
	    db_open("teller", DB_UNKNOWN, 0, 0, dbenv, NULL, &tdb)) != 0) {
		fprintf(stderr, "%s: Open of teller file failed: ", progname);
		if (errno < 0)
			fprintf(stderr, "returned %ld\n", (long)errno);
		else
			fprintf(stderr, "%s\n", strerror(errno));
		exit (1);
	}
	if ((errno =
	    db_open("history", DB_UNKNOWN, 0, 0, dbenv, NULL, &hdb)) != 0) {
		fprintf(stderr, "%s: Open of history file failed: ", progname);
		if (errno < 0)
			fprintf(stderr, "returned %ld\n", (long)errno);
		else
			fprintf(stderr, "%s\n", strerror(errno));
		exit (1);
	}

	txns = failed = ifailed = 0;
	(void)gettimeofday(&starttime, NULL);
	lasttime = starttime;
	while (n-- >= 0) {
		txns++;
		ret = tp_txn(dbenv->tx_info,
		    adb, bdb, tdb, hdb, accounts, branches, tellers);
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
			printf("[%d] %d txns %d failed ", (int)pid,
			    txns, failed);
			printf("%6.2f TPS (gross) %6.2f TPS (interval)\n",
			   gtps, itps);
			lasttime = curtime;
			ifailed = 0;
		}
	}

	(void)adb->close(adb, 0);
	(void)bdb->close(bdb, 0);
	(void)tdb->close(tdb, 0);
	(void)hdb->close(hdb, 0);

	printf("%ld transactions begun %ld failed\n", (long)txns, (long)failed);
}

/*
 * XXX Figure out the appropriate way to pick out IDs.
 */
int
tp_txn(txmgr, adb, bdb, tdb, hdb, anum, bnum, tnum)
	DB_TXNMGR *txmgr;
	DB *adb, *bdb, *tdb, *hdb;
	int anum, bnum, tnum;
{
	DBC *acurs, *bcurs, *tcurs;
	DBT d_dbt, d_histdbt, k_dbt, k_histdbt;
	DB_TXN *t;
	db_recno_t key;
	defrec rec;
	histrec hrec;
	int account, branch, teller;

	t = NULL;
	acurs = bcurs = tcurs = NULL;

	/*
	 * XXX We could move a lot of this into the driver to make this
	 * faster.
	 */
	account = random_id(ACCOUNT, anum, bnum, tnum);
	branch = random_id(BRANCH, anum, bnum, tnum);
	teller = random_id(TELLER, anum, bnum, tnum);

	memset(&d_histdbt, 0, sizeof(d_histdbt));

	memset(&k_histdbt, 0, sizeof(k_histdbt));
	k_histdbt.data = &key;
	k_histdbt.size = sizeof(key);

	memset(&k_dbt, 0, sizeof(k_dbt));
	k_dbt.size = sizeof(int);

	memset(&d_dbt, 0, sizeof(d_dbt));
	d_dbt.flags = DB_DBT_USERMEM;
	d_dbt.data = &rec;
	d_dbt.ulen = sizeof(rec);

	hrec.aid = account;
	hrec.bid = branch;
	hrec.tid = teller;
	hrec.amount = 10;
	/* Request 0 bytes since we're just positioning. */
	d_histdbt.flags = DB_DBT_PARTIAL;

	/* START TIMING */
	if (txn_begin(txmgr, NULL, &t) != 0)
		goto err;

	if (adb->cursor(adb, t, &acurs) != 0 ||
	    bdb->cursor(bdb, t, &bcurs) != 0 ||
	    tdb->cursor(tdb, t, &tcurs) != 0)
		goto err;

	/* Account record */
	k_dbt.data = &account;
	if (acurs->c_get(acurs, &k_dbt, &d_dbt, DB_SET) != 0)
		goto err;
	rec.balance += 10;
	if (acurs->c_put(acurs, &k_dbt, &d_dbt, DB_CURRENT) != 0)
		goto err;

	/* Branch record */
	k_dbt.data = &branch;
	if (bcurs->c_get(bcurs, &k_dbt, &d_dbt, DB_SET) != 0)
		goto err;
	rec.balance += 10;
	if (bcurs->c_put(bcurs, &k_dbt, &d_dbt, DB_CURRENT) != 0)
		goto err;

	/* Teller record */
	k_dbt.data = &teller;
	if (tcurs->c_get(tcurs, &k_dbt, &d_dbt, DB_SET) != 0)
		goto err;
	rec.balance += 10;
	if (tcurs->c_put(tcurs, &k_dbt, &d_dbt, DB_CURRENT) != 0)
		goto err;

	/* History record */
	d_histdbt.flags = 0;
	d_histdbt.data = &hrec;
	d_histdbt.ulen = sizeof(hrec);
	if (hdb->put(hdb, t, &k_histdbt, &d_histdbt, DB_APPEND) != 0)
		goto err;

	if (acurs->c_close(acurs) != 0 || bcurs->c_close(bcurs) != 0 ||
	    tcurs->c_close(tcurs) != 0)
		goto err;

	if (txn_commit(t) != 0)
		goto err;

	/* END TIMING */
	return (0);

err:	if (acurs != NULL)
		(void)acurs->c_close(acurs);
	if (bcurs != NULL)
		(void)bcurs->c_close(bcurs);
	if (tcurs != NULL)
		(void)tcurs->c_close(tcurs);
	if (t != NULL)
		(void)txn_abort(t);

	if (verbose)
		printf("Transaction A=%ld B=%ld T=%ld failed\n",
		    (long)account, (long)branch, (long)teller);
	return (-1);
}
