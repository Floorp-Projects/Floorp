/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1996, 1997, 1998\n\
	Sleepycat Software Inc.  All rights reserved.\n";
static const char sccsid[] = "@(#)db_stat.c	8.38 (Sleepycat) 5/30/98";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_shash.h"
#include "lock.h"
#include "mp.h"
#include "clib_ext.h"

typedef enum { T_NOTSET, T_DB, T_LOCK, T_LOG, T_MPOOL, T_TXN } test_t;

int	argcheck __P((char *, const char *));
void	btree_stats __P((DB *));
DB_ENV *db_init __P((char *, test_t));
void	dl __P((const char *, u_long));
void	hash_stats __P((DB *));
int	lock_ok __P((char *));
void	lock_stats __P((DB_ENV *));
void	log_stats __P((DB_ENV *));
int	main __P((int, char *[]));
int	mpool_ok __P((char *));
void	mpool_stats __P((DB_ENV *));
void	onint __P((int));
void	prflags __P((u_int32_t, const FN *));
int	txn_compare __P((const void *, const void *));
void	txn_stats __P((DB_ENV *));
void	usage __P((void));

int	 interrupted;
char	*internal;
const char
	*progname = "db_stat";				/* Program name. */

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	DB *dbp;
	DB_ENV *dbenv;
	test_t ttype;
	int ch;
	char *db, *home;

	ttype = T_NOTSET;
	db = home = NULL;
	while ((ch = getopt(argc, argv, "C:cd:h:lM:mNt")) != EOF)
		switch (ch) {
		case 'C':
			ttype = T_LOCK;
			if (!argcheck(internal = optarg, "Acflmo"))
				usage();
			break;
		case 'c':
			ttype = T_LOCK;
			break;
		case 'd':
			db = optarg;
			ttype = T_DB;
			break;
		case 'h':
			home = optarg;
			break;
		case 'l':
			ttype = T_LOG;
			break;
		case 'M':
			ttype = T_MPOOL;
			if (!argcheck(internal = optarg, "Ahlm"))
				usage();
			break;
		case 'm':
			ttype = T_MPOOL;
			break;
		case 'N':
			(void)db_value_set(0, DB_MUTEXLOCKS);
			break;
		case 't':
			ttype = T_TXN;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 0 || ttype == T_NOTSET)
		usage();

	dbenv = db_init(home, ttype);

	(void)signal(SIGINT, onint);

	switch (ttype) {
	case T_DB:
		if ((errno = db_open(db, DB_UNKNOWN,
		    DB_RDONLY, 0, dbenv, NULL, &dbp)) != 0)
			return (1);
		switch (dbp->type) {
		case DB_BTREE:
		case DB_RECNO:
			btree_stats(dbp);
			break;
		case DB_HASH:
			hash_stats(dbp);
			break;
		case DB_UNKNOWN:
			abort();		/* Impossible. */
			/* NOTREACHED */
		}
		(void)dbp->close(dbp, 0);
		break;
	case T_LOCK:
		lock_stats(dbenv);
		break;
	case T_LOG:
		log_stats(dbenv);
		break;
	case T_MPOOL:
		mpool_stats(dbenv);
		break;
	case T_TXN:
		txn_stats(dbenv);
		break;
	case T_NOTSET:
		abort();			/* Impossible. */
		/* NOTREACHED */
	}

	(void)db_appexit(dbenv);

	if (interrupted) {
		(void)signal(SIGINT, SIG_DFL);
		(void)raise(SIGINT);
		/* NOTREACHED */
	}
	return (0);
}

/*
 * btree_stats --
 *	Display btree/recno statistics.
 */
void
btree_stats(dbp)
	DB *dbp;
{
	static const FN fn[] = {
		{ DB_DUP,	"DB_DUP" },
		{ DB_FIXEDLEN,	"DB_FIXEDLEN" },
		{ DB_RECNUM,	"DB_RECNUM" },
		{ DB_RENUMBER,	"DB_RENUMBER" },
		{ 0 }
	};
	DB_BTREE_STAT *sp;

	if (dbp->stat(dbp, &sp, NULL, 0))
		err(1, "dbp->stat");

#define	PCT(f, t)							\
    (t == 0 ? 0 :							\
    (((double)((t * sp->bt_pagesize) - f) / (t * sp->bt_pagesize)) * 100))

	printf("%#lx\tBtree magic number.\n", (u_long)sp->bt_magic);
	printf("%lu\tBtree version number.\n", (u_long)sp->bt_version);
	prflags(sp->bt_flags, fn);
	if (dbp->type == DB_BTREE) {
#ifdef NOT_IMPLEMENTED
		dl("Maximum keys per-page.\n", (u_long)sp->bt_maxkey);
#endif
		dl("Minimum keys per-page.\n", (u_long)sp->bt_minkey);
	}
	if (dbp->type == DB_RECNO) {
		dl("Fixed-length record size.\n", (u_long)sp->bt_re_len);
		if (isprint(sp->bt_re_pad))
			printf("%c\tFixed-length record pad.\n",
			    (int)sp->bt_re_pad);
		else
			printf("0x%x\tFixed-length record pad.\n",
			    (int)sp->bt_re_pad);
	}
	dl("Underlying tree page size.\n", (u_long)sp->bt_pagesize);
	dl("Number of levels in the tree.\n", (u_long)sp->bt_levels);
	dl("Number of keys in the tree.\n", (u_long)sp->bt_nrecs);
	dl("Number of tree internal pages.\n", (u_long)sp->bt_int_pg);
	dl("Number of tree leaf pages.\n", (u_long)sp->bt_leaf_pg);
	dl("Number of tree duplicate pages.\n", (u_long)sp->bt_dup_pg);
	dl("Number of tree overflow pages.\n", (u_long)sp->bt_over_pg);
	dl("Number of pages on the free list.\n", (u_long)sp->bt_free);
	dl("Number of pages freed for reuse.\n", (u_long)sp->bt_freed);
	dl("Number of bytes free in tree internal pages",
	    (u_long)sp->bt_int_pgfree);
	printf(" (%.0f%% ff).\n", PCT(sp->bt_int_pgfree, sp->bt_int_pg));
	dl("Number of bytes free in tree leaf pages",
	    (u_long)sp->bt_leaf_pgfree);
	printf(" (%.0f%% ff).\n", PCT(sp->bt_leaf_pgfree, sp->bt_leaf_pg));
	dl("Number of bytes free in tree duplicate pages",
	    (u_long)sp->bt_dup_pgfree);
	printf(" (%.0f%% ff).\n", PCT(sp->bt_dup_pgfree, sp->bt_dup_pg));
	dl("Number of bytes free in tree overflow pages",
	    (u_long)sp->bt_over_pgfree);
	printf(" (%.0f%% ff).\n", PCT(sp->bt_over_pgfree, sp->bt_over_pg));
	dl("Number of bytes saved by prefix compression.\n",
	    (u_long)sp->bt_pfxsaved);
	dl("Total number of tree page splits.\n", (u_long)sp->bt_split);
	dl("Number of root page splits.\n", (u_long)sp->bt_rootsplit);
	dl("Number of fast splits.\n", (u_long)sp->bt_fastsplit);
	dl("Number of hits in tree fast-insert code.\n",
	    (u_long)sp->bt_cache_hit);
	dl("Number of misses in tree fast-insert code.\n",
	    (u_long)sp->bt_cache_miss);
	dl("Number of keys added.\n", (u_long)sp->bt_added);
	dl("Number of keys deleted.\n", (u_long)sp->bt_deleted);
}

/*
 * hash_stats --
 *	Display hash statistics.
 */
void
hash_stats(dbp)
	DB *dbp;
{
	COMPQUIET(dbp, NULL);

	printf("Hash statistics not currently available.\n");
	return;
}

/*
 * lock_stats --
 *	Display lock statistics.
 */
void
lock_stats(dbenv)
	DB_ENV *dbenv;
{
	DB_LOCK_STAT *sp;

	if (internal != NULL) {
		__lock_dump_region(dbenv->lk_info, internal, stdout);
		return;
	}

	if (lock_stat(dbenv->lk_info, &sp, NULL))
		err(1, NULL);

	printf("%#lx\tLock magic number.\n", (u_long)sp->st_magic);
	printf("%lu\tLock version number.\n", (u_long)sp->st_version);
	dl("Lock region reference count.\n", (u_long)sp->st_refcnt);
	dl("Lock region size.\n", (u_long)sp->st_regsize);
	dl("Maximum number of locks.\n", (u_long)sp->st_maxlocks);
	dl("Number of lock modes.\n", (u_long)sp->st_nmodes);
	dl("Number of lock objects.\n", (u_long)sp->st_numobjs);
	dl("Number of lockers.\n", (u_long)sp->st_nlockers);
	dl("Number of lock conflicts.\n", (u_long)sp->st_nconflicts);
	dl("Number of lock requests.\n", (u_long)sp->st_nrequests);
	dl("Number of lock releases.\n", (u_long)sp->st_nreleases);
	dl("Number of deadlocks.\n", (u_long)sp->st_ndeadlocks);
	dl("The number of region locks granted without waiting.\n",
	    (u_long)sp->st_region_nowait);
	dl("The number of region locks granted after waiting.\n",
	    (u_long)sp->st_region_wait);
}

/*
 * log_stats --
 *	Display log statistics.
 */
void
log_stats(dbenv)
	DB_ENV *dbenv;
{
	DB_LOG_STAT *sp;

	if (log_stat(dbenv->lg_info, &sp, NULL))
		err(1, NULL);

	printf("%#lx\tLog magic number.\n", (u_long)sp->st_magic);
	printf("%lu\tLog version number.\n", (u_long)sp->st_version);
	dl("Log region reference count.\n", (u_long)sp->st_refcnt);
	dl("Log region size.\n", (u_long)sp->st_regsize);
	printf("%#o\tLog file mode.\n", sp->st_mode);
	if (sp->st_lg_max % MEGABYTE == 0)
		printf("%luMb\tLog file size.\n",
		    (u_long)sp->st_lg_max / MEGABYTE);
	else if (sp->st_lg_max % 1024 == 0)
		printf("%luKb\tLog file size.\n", (u_long)sp->st_lg_max / 1024);
	else
		printf("%lu\tLog file size.\n", (u_long)sp->st_lg_max);
	printf("%luMb\tLog bytes written (+%lu bytes).\n",
	    (u_long)sp->st_w_mbytes, (u_long)sp->st_w_bytes);
	printf("%luMb\tLog bytes written since last checkpoint (+%lu bytes).\n",
	    (u_long)sp->st_wc_mbytes, (u_long)sp->st_wc_bytes);
	dl("Total log file writes.\n", (u_long)sp->st_wcount);
	dl("Total log file flushes.\n", (u_long)sp->st_scount);
	printf("%lu\tCurrent log file number.\n", (u_long)sp->st_cur_file);
	printf("%lu\tCurrent log file offset.\n", (u_long)sp->st_cur_offset);
	dl("The number of region locks granted without waiting.\n",
	    (u_long)sp->st_region_nowait);
	dl("The number of region locks granted after waiting.\n",
	    (u_long)sp->st_region_wait);
}

/*
 * mpool_stats --
 *	Display mpool statistics.
 */
void
mpool_stats(dbenv)
	DB_ENV *dbenv;
{
	DB_MPOOL_FSTAT **fsp;
	DB_MPOOL_STAT *gsp;

	if (internal != NULL) {
		__memp_dump_region(dbenv->mp_info, internal, stdout);
		return;
	}

	if (memp_stat(dbenv->mp_info, &gsp, &fsp, NULL))
		err(1, NULL);

	dl("Pool region reference count.\n", (u_long)gsp->st_refcnt);
	dl("Pool region size.\n", (u_long)gsp->st_regsize);
	dl("Cache size", (u_long)gsp->st_cachesize);
	printf(" (%luK).\n", (u_long)gsp->st_cachesize / 1024);
	dl("Requested pages found in the cache", (u_long)gsp->st_cache_hit);
	if (gsp->st_cache_hit + gsp->st_cache_miss != 0)
		printf(" (%.0f%%)", ((double)gsp->st_cache_hit /
		    (gsp->st_cache_hit + gsp->st_cache_miss)) * 100);
	printf(".\n");
	dl("Requested pages mapped into the process' address space.\n",
	    (u_long)gsp->st_map);
	dl("Requested pages not found in the cache.\n",
	    (u_long)gsp->st_cache_miss);
	dl("Pages created in the cache.\n", (u_long)gsp->st_page_create);
	dl("Pages read into the cache.\n", (u_long)gsp->st_page_in);
	dl("Pages written from the cache to the backing file.\n",
	    (u_long)gsp->st_page_out);
	dl("Clean pages forced from the cache.\n",
	    (u_long)gsp->st_ro_evict);
	dl("Dirty pages forced from the cache.\n",
	    (u_long)gsp->st_rw_evict);
	dl("Dirty buffers written by trickle-sync thread.\n",
	    (u_long)gsp->st_page_trickle);
	dl("Current clean buffer count.\n",
	    (u_long)gsp->st_page_clean);
	dl("Current dirty buffer count.\n",
	    (u_long)gsp->st_page_dirty);
	dl("Number of hash buckets used for page location.\n",
	    (u_long)gsp->st_hash_buckets);
	dl("Total number of times hash chains searched for a page.\n",
	    (u_long)gsp->st_hash_searches);
	dl("The longest hash chain searched for a page.\n",
	    (u_long)gsp->st_hash_longest);
	dl("Total number of hash buckets examined for page location.\n",
	    (u_long)gsp->st_hash_examined);
	dl("The number of region locks granted without waiting.\n",
	    (u_long)gsp->st_region_nowait);
	dl("The number of region locks granted after waiting.\n",
	    (u_long)gsp->st_region_wait);

	for (; fsp != NULL && *fsp != NULL; ++fsp) {
		printf("%s\n", DB_LINE);
		printf("%s\n", (*fsp)->file_name);
		dl("Page size.\n", (u_long)(*fsp)->st_pagesize);
		dl("Requested pages found in the cache",
		    (u_long)(*fsp)->st_cache_hit);
		if ((*fsp)->st_cache_hit + (*fsp)->st_cache_miss != 0)
			printf(" (%.0f%%)", ((double)(*fsp)->st_cache_hit /
			    ((*fsp)->st_cache_hit + (*fsp)->st_cache_miss)) *
			    100);
		printf(".\n");
		dl("Requested pages mapped into the process' address space.\n",
		    (u_long)(*fsp)->st_map);
		dl("Requested pages not found in the cache.\n",
		    (u_long)(*fsp)->st_cache_miss);
		dl("Pages created in the cache.\n",
		    (u_long)(*fsp)->st_page_create);
		dl("Pages read into the cache.\n",
		    (u_long)(*fsp)->st_page_in);
		dl("Pages written from the cache to the backing file.\n",
		    (u_long)(*fsp)->st_page_out);
	}
}

/*
 * txn_stats --
 *	Display transaction statistics.
 */
void
txn_stats(dbenv)
	DB_ENV *dbenv;
{
	DB_TXN_STAT *sp;
	u_int32_t i;
	const char *p;

	if (txn_stat(dbenv->tx_info, &sp, NULL))
		err(1, NULL);

	dl("Txn region reference count.\n", (u_long)sp->st_refcnt);
	dl("Txn region size.\n", (u_long)sp->st_regsize);
	p = sp->st_last_ckp.file == 0 ?
	    "No checkpoint LSN." : "File/offset for last checkpoint LSN.";
	printf("%lu/%lu\t%s\n",
	    (u_long)sp->st_last_ckp.file, (u_long)sp->st_last_ckp.offset, p);
	p = sp->st_pending_ckp.file == 0 ?
	    "No pending checkpoint LSN." :
	    "File/offset for last pending checkpoint LSN.";
	printf("%lu/%lu\t%s\n",
	    (u_long)sp->st_pending_ckp.file,
	    (u_long)sp->st_pending_ckp.offset, p);
	if (sp->st_time_ckp == 0)
		printf("0\tNo checkpoint timestamp.\n");
	else
		printf("%.24s\tCheckpoint timestamp.\n",
		    ctime(&sp->st_time_ckp));
	printf("%lx\tLast transaction ID allocated.\n",
	    (u_long)sp->st_last_txnid);
	dl("Maximum number of active transactions.\n", (u_long)sp->st_maxtxns);
	dl("Number of transactions begun.\n", (u_long)sp->st_nbegins);
	dl("Number of transactions aborted.\n", (u_long)sp->st_naborts);
	dl("Number of transactions committed.\n", (u_long)sp->st_ncommits);
	dl("The number of region locks granted without waiting.\n",
	    (u_long)sp->st_region_nowait);
	dl("The number of region locks granted after waiting.\n",
	    (u_long)sp->st_region_wait);
	dl("Active transactions.\n", (u_long)sp->st_nactive);
	qsort(sp->st_txnarray,
	    sp->st_nactive, sizeof(sp->st_txnarray[0]), txn_compare);
	for (i = 0; i < sp->st_nactive; ++i)
		printf("\tid: %lx; initial LSN file/offest %lu/%lu\n",
		    (u_long)sp->st_txnarray[i].txnid,
		    (u_long)sp->st_txnarray[i].lsn.file,
		    (u_long)sp->st_txnarray[i].lsn.offset);
}

int
txn_compare(a1, b1)
	const void *a1, *b1;
{
	const DB_TXN_ACTIVE *a, *b;

	a = a1;
	b = b1;

	if (a->txnid > b->txnid)
		return (1);
	if (a->txnid < b->txnid)
		return (-1);
	return (0);
}

/*
 * dl --
 *	Display a big value.
 */
void
dl(msg, value)
	const char *msg;
	u_long value;
{
	/*
	 * Two formats: if less than 10 million, display as the number, if
	 * greater than 10 million display as ###M.
	 */
	if (value < 10000000)
		printf("%lu\t%s", value, msg);
	else
		printf("%luM\t%s", value / 1000000, msg);
}

/*
 * prflags --
 *	Print out flag values.
 */
void
prflags(flags, fnp)
	u_int32_t flags;
	const FN *fnp;
{
	const char *sep;

	sep = " ";
	printf("Flags:");
	for (; fnp->mask != 0; ++fnp)
		if (fnp->mask & flags) {
			printf("%s%s", sep, fnp->name);
			sep = ", ";
		}
	printf("\n");
}

/*
 * db_init --
 *	Initialize the environment.
 */
DB_ENV *
db_init(home, ttype)
	char *home;
	test_t ttype;
{
	DB_ENV *dbenv;
	u_int32_t flags;

	if ((dbenv = (DB_ENV *)malloc(sizeof(DB_ENV))) == NULL) {
		errno = ENOMEM;
		err(1, NULL);
	}

	/*
	 * Try and use the shared regions when reporting statistics on the
	 * DB databases, so our information is as up-to-date as possible,
	 * even if the mpool cache hasn't been flushed.  If that fails, we
	 * turn off the DB_INIT_MPOOL flag and try again.
	 */
	flags = DB_USE_ENVIRON;
	switch (ttype) {
	case T_DB:
	case T_MPOOL:
		LF_SET(DB_INIT_MPOOL);
		break;
	case T_LOCK:
		LF_SET(DB_INIT_LOCK);
		break;
	case T_LOG:
		LF_SET(DB_INIT_LOG);
		break;
	case T_TXN:
		LF_SET(DB_INIT_TXN);
		break;
	case T_NOTSET:
		abort();
		/* NOTREACHED */
	}

	/*
	 * If it works, we're done.  Set the error output options so that
	 * future errors are correctly reported.
	 */
	memset(dbenv, 0, sizeof(*dbenv));
	if ((errno = db_appinit(home, NULL, dbenv, flags)) == 0) {
		dbenv->db_errfile = stderr;
		dbenv->db_errpfx = progname;
		return (dbenv);
	}

	/* Turn off the DB_INIT_MPOOL flag if it's a database. */
	if (ttype == T_DB)
		LF_CLR(DB_INIT_MPOOL);

	/* Set the error output options -- this time we want a message. */
	memset(dbenv, 0, sizeof(*dbenv));
	dbenv->db_errfile = stderr;
	dbenv->db_errpfx = progname;

	/* Try again, and it's fatal if we fail. */
	if ((errno = db_appinit(home, NULL, dbenv, flags)) != 0)
		err(1, "db_appinit");

	return (dbenv);
}

/*
 * argcheck --
 *	Return if argument flags are okay.
 */
int
argcheck(arg, ok_args)
	char *arg;
	const char *ok_args;
{
	for (; *arg != '\0'; ++arg)
		if (strchr(ok_args, *arg) == NULL)
			return (0);
	return (1);
}

/*
 * oninit --
 *	Interrupt signal handler.
 */
void
onint(signo)
	int signo;
{
	COMPQUIET(signo, 0);

	interrupted = 1;
}

void
usage()
{
	fprintf(stderr,
    "usage: db_stat [-clmNt] [-C Acflmo] [-d file] [-h home] [-M Ahlm]\n");
	exit (1);
}
