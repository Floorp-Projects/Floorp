/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 *
 *	@(#)txn.h	10.15 (Sleepycat) 4/21/98
 */
#ifndef	_TXN_H_
#define	_TXN_H_

/*
 * The name of the transaction shared memory region is DEFAULT_TXN_FILE and
 * the region is always created group RW of the group owning the directory.
 */
#define	DEFAULT_TXN_FILE	"__db_txn.share"
/* TXN_MINIMUM = (DB_LOCK_MAXID + 1) but this makes compilers complain. */
#define TXN_MINIMUM		0x80000000
#define	TXN_INVALID           	0xffffffff /* Maximum number of txn ids. */

/*
 * Transaction type declarations.
 */

/*
 * Internal data maintained in shared memory for each transaction.
 */
typedef struct __txn_detail {
	u_int32_t txnid;		/* current transaction id
					   used to link free list also */
	DB_LSN	last_lsn;		/* last lsn written for this txn */
	DB_LSN	begin_lsn;		/* lsn of begin record */
	size_t	last_lock;		/* offset in lock region of last lock
					   for this transaction. */
#define	TXN_UNALLOC	0
#define	TXN_RUNNING	1
#define	TXN_ABORTED	2
#define	TXN_PREPARED	3
	u_int32_t status;		/* status of the transaction */
	SH_TAILQ_ENTRY	links;		/* free/active list */
} TXN_DETAIL;

/*
 * The transaction manager encapsulates the transaction system.  It contains
 * references to the log and lock managers as well as the state that keeps
 * track of the shared memory region.
 */
struct __db_txnmgr {
/* These fields need to be protected for multi-threaded support. */
	db_mutex_t	*mutexp;	/* Synchronization. */
					/* list of active transactions */
	TAILQ_HEAD(_chain, __db_txn)	txn_chain;

/* These fields are not protected. */
	REGINFO		reginfo;	/* Region information. */
	DB_ENV		*dbenv;		/* Environment. */
	int (*recover)			/* Recovery dispatch routine */
	    __P((DB_LOG *, DBT *, DB_LSN *, int, void *));
	u_int32_t	 flags;		/* DB_TXN_NOSYNC, DB_THREAD */
	DB_TXNREGION	*region;	/* address of shared memory region */
	void		*mem;		/* address of the shalloc space */
};

/*
 * Layout of the shared memory region.
 * The region consists of a DB_TXNREGION structure followed by a large
 * pool of shalloc'd memory which is used to hold TXN_DETAIL structures
 * and thread mutexes (which are dynamically allocated).
 */
struct __db_txnregion {
	RLAYOUT		hdr;		/* Shared memory region header. */
	u_int32_t	magic;		/* transaction magic number */
	u_int32_t	version;	/* version number */
	u_int32_t	maxtxns;	/* maximum number of active txns */
	u_int32_t	last_txnid;	/* last transaction id given out */
	DB_LSN		pending_ckp;	/* last checkpoint did not finish */
	DB_LSN		last_ckp;	/* lsn of the last checkpoint */
	time_t		time_ckp;	/* time of last checkpoint */
	u_int32_t	logtype;	/* type of logging */
	u_int32_t	locktype;	/* lock type */
	u_int32_t	naborts;	/* number of aborted transactions */
	u_int32_t	ncommits;	/* number of committed transactions */
	u_int32_t	nbegins;	/* number of begun transactions */
	SH_TAILQ_HEAD(_active) active_txn;	/* active transaction list */
};

/*
 * Make the region large enough to hold N  transaction detail structures
 * plus some space to hold thread handles and the beginning of the shalloc
 * region.
 */
#define	TXN_REGION_SIZE(N)						\
	(sizeof(DB_TXNREGION) + N * sizeof(TXN_DETAIL) + 1000)

/* Macros to lock/unlock the region and threads. */
#define	LOCK_TXNTHREAD(tmgrp)						\
	if (F_ISSET(tmgrp, DB_THREAD))					\
		(void)__db_mutex_lock((tmgrp)->mutexp, -1)
#define	UNLOCK_TXNTHREAD(tmgrp)						\
	if (F_ISSET(tmgrp, DB_THREAD))					\
		(void)__db_mutex_unlock((tmgrp)->mutexp, -1)

#define	LOCK_TXNREGION(tmgrp)						\
	(void)__db_mutex_lock(&(tmgrp)->region->hdr.lock, (tmgrp)->reginfo.fd)
#define	UNLOCK_TXNREGION(tmgrp)						\
	(void)__db_mutex_unlock(&(tmgrp)->region->hdr.lock, (tmgrp)->reginfo.fd)

/*
 * Log record types.
 */
#define	TXN_COMMIT	1
#define	TXN_PREPARE	2
#define	TXN_CHECKPOINT	3

#include "txn_auto.h"
#include "txn_ext.h"
#endif /* !_TXN_H_ */
