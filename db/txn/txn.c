/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */
/*
 * Copyright (c) 1995, 1996
 *	The President and Fellows of Harvard University.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Margo Seltzer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)txn.c	10.58 (Sleepycat) 5/31/98";
#endif /* not lint */


#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#include <time.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_page.h"
#include "db_shash.h"
#include "txn.h"
#include "db_dispatch.h"
#include "lock.h"
#include "log.h"
#include "db_am.h"
#include "common_ext.h"

static int __txn_check_running __P((const DB_TXN *));
static int __txn_end __P((DB_TXN *, int));
static int __txn_grow_region __P((DB_TXNMGR *));
static int __txn_init __P((DB_TXNREGION *));
static int __txn_undo __P((DB_TXN *));
static int __txn_validate_region __P((DB_TXNMGR *));

/*
 * This file contains the top level routines of the transaction library.
 * It assumes that a lock manager and log manager that conform to the db_log(3)
 * and db_lock(3) interfaces exist.
 *
 * Initialize a transaction region in shared memory.
 * Return 0 on success, errno on failure.
 */
static int
__txn_init(txn_region)
	DB_TXNREGION *txn_region;
{
	time_t now;

	(void)time(&now);

	/* maxtxns is already initialized. */
	txn_region->magic = DB_TXNMAGIC;
	txn_region->version = DB_TXNVERSION;
	txn_region->last_txnid = TXN_MINIMUM;
	/* XXX If we ever do more types of locking and logging, this changes. */
	txn_region->logtype = 0;
	txn_region->locktype = 0;
	txn_region->time_ckp = now;
	ZERO_LSN(txn_region->last_ckp);
	ZERO_LSN(txn_region->pending_ckp);
	SH_TAILQ_INIT(&txn_region->active_txn);
	__db_shalloc_init((void *)&txn_region[1],
	    TXN_REGION_SIZE(txn_region->maxtxns) - sizeof(DB_TXNREGION));

	return (0);
}

int
txn_open(path, flags, mode, dbenv, mgrpp)
	const char *path;
	u_int32_t flags;
	int mode;
	DB_ENV *dbenv;
	DB_TXNMGR **mgrpp;
{
	DB_TXNMGR *tmgrp;
	u_int32_t maxtxns;
	int ret;

	/* Validate arguments. */
	if (dbenv == NULL)
		return (EINVAL);
#ifdef HAVE_SPINLOCKS
#define	OKFLAGS	(DB_CREATE | DB_THREAD | DB_TXN_NOSYNC)
#else
#define	OKFLAGS	(DB_CREATE | DB_TXN_NOSYNC)
#endif
	if ((ret = __db_fchk(dbenv, "txn_open", flags, OKFLAGS)) != 0)
		return (ret);

	maxtxns = dbenv->tx_max != 0 ? dbenv->tx_max : 20;

	/* Now, create the transaction manager structure and set its fields. */
	if ((tmgrp = (DB_TXNMGR *)__db_calloc(1, sizeof(DB_TXNMGR))) == NULL) {
		__db_err(dbenv, "txn_open: %s", strerror(ENOMEM));
		return (ENOMEM);
	}

	/* Initialize the transaction manager structure. */
	tmgrp->mutexp = NULL;
	tmgrp->dbenv = dbenv;
	tmgrp->recover =
	    dbenv->tx_recover == NULL ? __db_dispatch : dbenv->tx_recover;
	tmgrp->flags = LF_ISSET(DB_TXN_NOSYNC | DB_THREAD);
	TAILQ_INIT(&tmgrp->txn_chain);

	/* Join/create the txn region. */
	tmgrp->reginfo.dbenv = dbenv;
	tmgrp->reginfo.appname = DB_APP_NONE;
	if (path == NULL)
		tmgrp->reginfo.path = NULL;
	else
		if ((tmgrp->reginfo.path = (char *)__db_strdup(path)) == NULL)
			goto err;
	tmgrp->reginfo.file = DEFAULT_TXN_FILE;
	tmgrp->reginfo.mode = mode;
	tmgrp->reginfo.size = TXN_REGION_SIZE(maxtxns);
	tmgrp->reginfo.dbflags = flags;
	tmgrp->reginfo.addr = NULL;
	tmgrp->reginfo.fd = -1;
	tmgrp->reginfo.flags = dbenv->tx_max == 0 ? REGION_SIZEDEF : 0;
	if ((ret = __db_rattach(&tmgrp->reginfo)) != 0)
		goto err;

	/* Fill in region-related fields. */
	tmgrp->region = tmgrp->reginfo.addr;
	tmgrp->mem = &tmgrp->region[1];

	if (F_ISSET(&tmgrp->reginfo, REGION_CREATED)) {
		tmgrp->region->maxtxns = maxtxns;
		if ((ret = __txn_init(tmgrp->region)) != 0)
			goto err;

	} else if (tmgrp->region->magic != DB_TXNMAGIC) {
		/* Check if valid region. */
		__db_err(dbenv, "txn_open: Bad magic number");
		ret = EINVAL;
		goto err;
	}

	if (LF_ISSET(DB_THREAD)) {
		if ((ret = __db_shalloc(tmgrp->mem, sizeof(db_mutex_t),
		    MUTEX_ALIGNMENT, &tmgrp->mutexp)) == 0)
			/*
			 * Since we only get here if threading is turned on, we
			 * know that we have spinlocks, so the offset is going
			 * to be ignored.  We put 0 here as a valid placeholder.
			 */
			__db_mutex_init(tmgrp->mutexp, 0);
		if (ret != 0)
			goto err;
	}

	UNLOCK_TXNREGION(tmgrp);
	*mgrpp = tmgrp;
	return (0);

err:	if (tmgrp->reginfo.addr != NULL) {
		if (tmgrp->mutexp != NULL)
			__db_shalloc_free(tmgrp->mem, tmgrp->mutexp);

		UNLOCK_TXNREGION(tmgrp);
		(void)__db_rdetach(&tmgrp->reginfo);
		if (F_ISSET(&tmgrp->reginfo, REGION_CREATED))
			(void)txn_unlink(path, 1, dbenv);
	}

	if (tmgrp->reginfo.path != NULL)
		FREES(tmgrp->reginfo.path);
	FREE(tmgrp, sizeof(*tmgrp));
	return (ret);
}

/*
 * Internally, we use TXN_DETAIL structures, but we allocate and return
 * DB_TXN structures that provide access to the transaction ID and the
 * offset in the transaction region of the TXN_DETAIL structure.
 */
int
txn_begin(tmgrp, parent, txnpp)
	DB_TXNMGR *tmgrp;
	DB_TXN *parent;
	DB_TXN **txnpp;
{
	DB_LSN begin_lsn;
	DB_TXN *retp;
	TXN_DETAIL *txnp;
	size_t off;
	u_int32_t id;
	int ret;

	txnp = NULL;
	*txnpp = NULL;

	if ((retp = (DB_TXN *)__db_malloc(sizeof(DB_TXN))) == NULL) {
		__db_err(tmgrp->dbenv, "txn_begin : %s", strerror(ENOMEM));
		return (ENOMEM);
	}

	/*
	 * We do not have to write begin records (and if we do not, then we
	 * need never write records for read-only transactions).  However,
	 * we do need to find the current LSN so that we can store it in the
	 * transaction structure, so we can know where to take checkpoints.
	 */
	if (tmgrp->dbenv->lg_info != NULL && (ret =
	    log_put(tmgrp->dbenv->lg_info, &begin_lsn, NULL, DB_CURLSN)) != 0)
		goto err2;

	LOCK_TXNREGION(tmgrp);

	/* Make sure that last_txnid is not going to wrap around. */
	if (tmgrp->region->last_txnid == TXN_INVALID) {
		__db_err(tmgrp->dbenv, "txn_begin: %s  %s",
		    "Transaction ID wrapping.",
		    "Snapshot your database and start a new log.");
		ret = EINVAL;
		goto err1;
	}

	if ((ret = __txn_validate_region(tmgrp)) != 0)
		goto err1;

	/* Allocate a new transaction detail structure. */
	if ((ret = __db_shalloc(tmgrp->mem, sizeof(TXN_DETAIL), 0, &txnp)) != 0
	    && ret == ENOMEM && (ret = __txn_grow_region(tmgrp)) == 0)
	    	ret = __db_shalloc(tmgrp->mem, sizeof(TXN_DETAIL), 0, &txnp);
	if (ret != 0)
		goto err1;

	/* Place transaction on active transaction list. */
	SH_TAILQ_INSERT_HEAD(&tmgrp->region->active_txn,
	    txnp, links, __txn_detail);

	id = ++tmgrp->region->last_txnid;
	tmgrp->region->nbegins++;

	txnp->txnid = id;
	txnp->begin_lsn = begin_lsn;
	ZERO_LSN(txnp->last_lsn);
	txnp->last_lock = 0;
	txnp->status = TXN_RUNNING;
	off = (u_int8_t *)txnp - (u_int8_t *)tmgrp->region;
	UNLOCK_TXNREGION(tmgrp);

	ZERO_LSN(retp->last_lsn);
	retp->txnid = id;
	retp->parent = parent;
	retp->mgrp = tmgrp;
	retp->off = off;

	LOCK_TXNTHREAD(tmgrp);
	TAILQ_INSERT_TAIL(&tmgrp->txn_chain, retp, links);
	UNLOCK_TXNTHREAD(tmgrp);

	*txnpp = retp;
	return (0);

err1:	UNLOCK_TXNREGION(tmgrp);

err2:	__db_free(retp);
	return (ret);
}

/*
 * txn_commit --
 *	Commit a transaction.
 */
int
txn_commit(txnp)
	DB_TXN *txnp;
{
	DB_LOG *logp;
	int ret;

	if ((ret = __txn_check_running(txnp)) != 0)
		return (ret);

	/*
	 * If there are any log records, write a log record and sync
	 * the log, else do no log writes.
	 */
	if ((logp = txnp->mgrp->dbenv->lg_info) != NULL &&
	    !IS_ZERO_LSN(txnp->last_lsn) &&
	    (ret = __txn_regop_log(logp, txnp, &txnp->last_lsn,
	    F_ISSET(txnp->mgrp, DB_TXN_NOSYNC) ? 0 : DB_FLUSH,
	    TXN_COMMIT)) != 0)
		return (ret);

	return (__txn_end(txnp, 1));
}

/*
 * txn_abort --
 *	Abort a transcation.
 */
int
txn_abort(txnp)
	DB_TXN *txnp;
{
	int ret;

	if ((ret = __txn_check_running(txnp)) != 0)
		return (ret);

	if ((ret = __txn_undo(txnp)) != 0) {
		__db_err(txnp->mgrp->dbenv,
		    "txn_abort: Log undo failed %s", strerror(ret));
		return (ret);
	}
	return (__txn_end(txnp, 0));
}

/*
 * Flush the log so a future commit is guaranteed to succeed.
 */
int
txn_prepare(txnp)
	DB_TXN *txnp;
{
	TXN_DETAIL *tp;
	int ret;

	if ((ret = __txn_check_running(txnp)) != 0)
		return (ret);

	if (txnp->mgrp->dbenv->lg_info != NULL) {
		if ((ret = log_flush(txnp->mgrp->dbenv->lg_info,
		    &txnp->last_lsn)) != 0)
			__db_err(txnp->mgrp->dbenv,
			    "txn_prepare: log_flush failed %s\n",
			    strerror(ret));
		return (ret);
	}

	LOCK_TXNTHREAD(txnp->mgrp);
	tp = (TXN_DETAIL *)((u_int8_t *)txnp->mgrp->region + txnp->off);
	tp->status = TXN_PREPARED;
	UNLOCK_TXNTHREAD(txnp->mgrp);
	return (ret);
}

/*
 * Return the transaction ID associated with a particular transaction
 */
u_int32_t
txn_id(txnp)
	DB_TXN *txnp;
{
	return (txnp->txnid);
}

/*
 * txn_close --
 *	Close the transaction region, does not imply a checkpoint.
 */
int
txn_close(tmgrp)
	DB_TXNMGR *tmgrp;
{
	DB_TXN *txnp;
	int ret, t_ret;

	ret = 0;

	/*
	 * This function had better only be called once per process
	 * (i.e., not per thread), so there should be no synchronization
	 * required.
	 */
	while ((txnp =
	    TAILQ_FIRST(&tmgrp->txn_chain)) != TAILQ_END(&tmgrp->txn_chain))
		if ((t_ret = txn_abort(txnp)) != 0) {
			__txn_end(txnp, 0);
			if (ret == 0)
				ret = t_ret;
		}

	if (tmgrp->dbenv->lg_info &&
	    (t_ret = log_flush(tmgrp->dbenv->lg_info, NULL)) != 0 && ret == 0)
		ret = t_ret;

	if (tmgrp->mutexp != NULL) {
		LOCK_TXNREGION(tmgrp);
		__db_shalloc_free(tmgrp->mem, tmgrp->mutexp);
		UNLOCK_TXNREGION(tmgrp);
	}

	if ((t_ret = __db_rdetach(&tmgrp->reginfo)) != 0 && ret == 0)
		ret = t_ret;

	if (tmgrp->reginfo.path != NULL)
		FREES(tmgrp->reginfo.path);
	FREE(tmgrp, sizeof(*tmgrp));

	return (ret);
}

/*
 * txn_unlink --
 *	Remove the transaction region.
 */
int
txn_unlink(path, force, dbenv)
	const char *path;
	int force;
	DB_ENV *dbenv;
{
	REGINFO reginfo;
	int ret;

	memset(&reginfo, 0, sizeof(reginfo));
	reginfo.dbenv = dbenv;
	reginfo.appname = DB_APP_NONE;
	if (path != NULL && (reginfo.path = (char *)__db_strdup(path)) == NULL)
		return (ENOMEM);
	reginfo.file = DEFAULT_TXN_FILE;
	ret = __db_runlink(&reginfo, force);
	if (reginfo.path != NULL)
		FREES(reginfo.path);
	return (ret);
}

/* Internal routines. */

/*
 * Return 0 if the txnp is reasonable, otherwise returns EINVAL.
 */
static int
__txn_check_running(txnp)
	const DB_TXN *txnp;
{
	TXN_DETAIL *tp;

	tp = NULL;
	if (txnp != NULL && txnp->mgrp != NULL && txnp->mgrp->region != NULL) {
		tp = (TXN_DETAIL *)((u_int8_t *)txnp->mgrp->region + txnp->off);
		if (tp->status != TXN_RUNNING)
			tp = NULL;
	}

	return (tp == NULL ? EINVAL : 0);
}

static int
__txn_end(txnp, is_commit)
	DB_TXN *txnp;
	int is_commit;
{
	DB_TXNMGR *mgr;
	TXN_DETAIL *tp;
	DB_LOCKREQ request;
	int ret;
	u_int32_t locker;

	mgr = txnp->mgrp;

	LOCK_TXNTHREAD(mgr);
	TAILQ_REMOVE(&mgr->txn_chain, txnp, links);
	UNLOCK_TXNTHREAD(mgr);

	/* Release the locks. */
	locker = txnp->txnid;
	request.op = DB_LOCK_PUT_ALL;

	if (mgr->dbenv->lk_info) {
		ret = lock_vec(mgr->dbenv->lk_info, locker, 0,
		    &request, 1, NULL);
		if (ret != 0 && (ret != DB_LOCK_DEADLOCK || is_commit)) {
			__db_err(mgr->dbenv, "%s: release locks failed %s",
			    is_commit ? "txn_commit" : "txn_abort",
			    strerror(ret));
			return (ret);
		}
	}

	/* End the transaction. */
	LOCK_TXNREGION(mgr);
	tp = (TXN_DETAIL *)((u_int8_t *)mgr->region + txnp->off);
	SH_TAILQ_REMOVE(&mgr->region->active_txn, tp, links, __txn_detail);
	__db_shalloc_free(mgr->mem, tp);
	if (is_commit)
		mgr->region->ncommits++;
	else
		mgr->region->naborts++;
	UNLOCK_TXNREGION(mgr);

	FREE(txnp, sizeof(*txnp));

	return (0);
}


/*
 * __txn_undo --
 *	Undo the transaction with id txnid.  Returns 0 on success and
 *	errno on failure.
 */
static int
__txn_undo(txnp)
	DB_TXN *txnp;
{
	DBT rdbt;
	DB_LOG *logp;
	DB_LSN key_lsn;
	DB_TXNMGR *mgr;
	int ret;

	mgr = txnp->mgrp;
	logp = mgr->dbenv->lg_info;
	if (logp == NULL)
		return (0);

	/*
	 * This is the simplest way to code this, but if the mallocs during
	 * recovery turn out to be a performance issue, we can do the
	 * allocation here and use DB_DBT_USERMEM.
	 */
	memset(&rdbt, 0, sizeof(rdbt));
	if (F_ISSET(logp, DB_AM_THREAD))
		F_SET(&rdbt, DB_DBT_MALLOC);

	key_lsn = txnp->last_lsn;		/* structure assignment */
	for (ret = 0; ret == 0 && !IS_ZERO_LSN(key_lsn);) {
		/*
		 * The dispatch routine returns the lsn of the record
		 * before the current one in the key_lsn argument.
		 */
		if ((ret = log_get(logp, &key_lsn, &rdbt, DB_SET)) == 0) {
			ret =
			    mgr->recover(logp, &rdbt, &key_lsn, TXN_UNDO, NULL);
			if (F_ISSET(logp, DB_AM_THREAD) && rdbt.data != NULL) {
				__db_free(rdbt.data);
				rdbt.data = NULL;
			}
		}
		if (ret != 0)
			return (ret);
	}

	return (ret);
}

/*
 * Transaction checkpoint.
 * If either kbytes or minutes is non-zero, then we only take the checkpoint
 * more than "minutes" minutes have passed since the last checkpoint or if
 * more than "kbytes" of log data have been written since the last checkpoint.
 * When taking a checkpoint, find the oldest active transaction and figure out
 * its first LSN.  This is the lowest LSN we can checkpoint, since any record
 * written after since that point may be involved in a transaction and may
 * therefore need to be undone in the case of an abort.
 */
int
txn_checkpoint(mgr, kbytes, minutes)
	const DB_TXNMGR *mgr;
	u_int32_t kbytes, minutes;
{
	TXN_DETAIL *txnp;
	DB_LSN ckp_lsn, last_ckp;
	DB_LOG *dblp;
	u_int32_t kbytes_written;
	time_t last_ckp_time, now;
	int ret;

	/*
	 * Check if we need to run recovery.
	 */
	ZERO_LSN(ckp_lsn);
	if (minutes != 0) {
		(void)time(&now);

		LOCK_TXNREGION(mgr);
		last_ckp_time = mgr->region->time_ckp;
		UNLOCK_TXNREGION(mgr);

		if (now - last_ckp_time >= (time_t)(minutes * 60))
			goto do_ckp;
	}

	if (kbytes != 0) {
		dblp = mgr->dbenv->lg_info;
		LOCK_LOGREGION(dblp);
		kbytes_written =
		    dblp->lp->stat.st_wc_mbytes * 1024 +
		    dblp->lp->stat.st_wc_bytes / 1024;
		ckp_lsn = dblp->lp->lsn;
		UNLOCK_LOGREGION(dblp);
		if (kbytes_written >= (u_int32_t)kbytes)
			goto do_ckp;
	}

	/*
	 * If we checked time and data and didn't go to checkpoint,
	 * we're done.
	 */
	if (minutes != 0 || kbytes != 0)
		return (0);

do_ckp:
	if (IS_ZERO_LSN(ckp_lsn)) {
		dblp = mgr->dbenv->lg_info;
		LOCK_LOGREGION(dblp);
		ckp_lsn = dblp->lp->lsn;
		UNLOCK_LOGREGION(dblp);
	}

	/*
	 * We have to find an LSN such that all transactions begun
	 * before that LSN are complete.
	 */
	LOCK_TXNREGION(mgr);

	if (!IS_ZERO_LSN(mgr->region->pending_ckp))
		ckp_lsn = mgr->region->pending_ckp;
	else
		for (txnp =
		    SH_TAILQ_FIRST(&mgr->region->active_txn, __txn_detail);
		    txnp != NULL;
		    txnp = SH_TAILQ_NEXT(txnp, links, __txn_detail)) {

			/*
			 * Look through the active transactions for the
			 * lowest begin lsn.
			 */
			if (!IS_ZERO_LSN(txnp->begin_lsn) &&
			    log_compare(&txnp->begin_lsn, &ckp_lsn) < 0)
				ckp_lsn = txnp->begin_lsn;
		}

	mgr->region->pending_ckp = ckp_lsn;
	UNLOCK_TXNREGION(mgr);

	if (mgr->dbenv->mp_info != NULL &&
	    (ret = memp_sync(mgr->dbenv->mp_info, &ckp_lsn)) != 0) {
		/*
		 * ret == DB_INCOMPLETE means that there are still buffers to
		 * flush, the checkpoint is not complete.  Wait and try again.
		 */
		if (ret > 0)
			__db_err(mgr->dbenv,
			    "txn_checkpoint: system failure in memp_sync %s\n",
			    strerror(ret));
		return (ret);
	}
	if (mgr->dbenv->lg_info != NULL) {
		LOCK_TXNREGION(mgr);
		last_ckp = mgr->region->last_ckp;
		ZERO_LSN(mgr->region->pending_ckp);
		UNLOCK_TXNREGION(mgr);

		if ((ret = __txn_ckp_log(mgr->dbenv->lg_info,
		   NULL, &ckp_lsn, DB_CHECKPOINT, &ckp_lsn, &last_ckp)) != 0) {
			__db_err(mgr->dbenv,
			    "txn_checkpoint: log failed at LSN [%ld %ld] %s\n",
			    (long)ckp_lsn.file, (long)ckp_lsn.offset,
			    strerror(ret));
			return (ret);
		}

		LOCK_TXNREGION(mgr);
		mgr->region->last_ckp = ckp_lsn;
		(void)time(&mgr->region->time_ckp);
		UNLOCK_TXNREGION(mgr);
	}
	return (0);
}

/*
 * __txn_validate_region --
 *	Called at every interface to verify if the region has changed size,
 *	and if so, to remap the region in and reset the process' pointers.
 */
static int
__txn_validate_region(tp)
	DB_TXNMGR *tp;
{
	int ret;

	if (tp->reginfo.size == tp->region->hdr.size)
		return (0);

	/* Detach/reattach the region. */
	if ((ret = __db_rreattach(&tp->reginfo, tp->region->hdr.size)) != 0)
		return (ret);

	/* Reset region information. */
	tp->region = tp->reginfo.addr;
	tp->mem = &tp->region[1];

	return (0);
}

static int
__txn_grow_region(tp)
	DB_TXNMGR *tp;
{
	size_t incr, oldsize;
	u_int32_t mutex_offset, oldmax;
	u_int8_t *curaddr;
	int ret;

	oldmax = tp->region->maxtxns;
	incr = oldmax * sizeof(DB_TXN);
	mutex_offset = tp->mutexp != NULL ?
	    (u_int8_t *)tp->mutexp - (u_int8_t *)tp->region : 0;

	oldsize = tp->reginfo.size;
	if ((ret = __db_rgrow(&tp->reginfo, oldsize + incr)) != 0)
		return (ret);
	tp->region = tp->reginfo.addr;

	/* Throw the new space on the free list. */
	curaddr = (u_int8_t *)tp->region + oldsize;
	tp->mem = &tp->region[1];
	tp->mutexp = mutex_offset != 0 ?
	    (db_mutex_t *)((u_int8_t *)tp->region + mutex_offset) : NULL;

	*((size_t *)curaddr) = incr - sizeof(size_t);
	curaddr += sizeof(size_t);
	__db_shalloc_free(tp->mem, curaddr);

	tp->region->maxtxns = 2 * oldmax;

	return (0);
}

int
txn_stat(mgr, statp, db_malloc)
	DB_TXNMGR *mgr;
	DB_TXN_STAT **statp;
	void *(*db_malloc) __P((size_t));
{
	DB_TXN_STAT *stats;
	TXN_DETAIL *txnp;
	size_t nbytes;
	u_int32_t nactive, ndx;

	LOCK_TXNREGION(mgr);
	nactive = mgr->region->nbegins -
	    mgr->region->naborts - mgr->region->ncommits;
	UNLOCK_TXNREGION(mgr);

	/*
	 * Allocate a bunch of extra active structures to handle any
	 * that have been created since we unlocked the region.
	 */
	nbytes = sizeof(DB_TXN_STAT) + sizeof(DB_TXN_ACTIVE) * (nactive + 200);
	if (db_malloc == NULL)
		stats = (DB_TXN_STAT *)__db_malloc(nbytes);
	else
		stats = (DB_TXN_STAT *)db_malloc(nbytes);

	if (stats == NULL)
		return (ENOMEM);

	LOCK_TXNREGION(mgr);
	stats->st_last_txnid = mgr->region->last_txnid;
	stats->st_last_ckp = mgr->region->last_ckp;
	stats->st_maxtxns = mgr->region->maxtxns;
	stats->st_naborts = mgr->region->naborts;
	stats->st_nbegins = mgr->region->nbegins;
	stats->st_ncommits = mgr->region->ncommits;
	stats->st_pending_ckp = mgr->region->pending_ckp;
	stats->st_time_ckp = mgr->region->time_ckp;
	stats->st_nactive = stats->st_nbegins -
	    stats->st_naborts - stats->st_ncommits;
	if (stats->st_nactive > nactive + 200)
		stats->st_nactive = nactive + 200;
	stats->st_txnarray = (DB_TXN_ACTIVE *)&stats[1];

	ndx = 0;
	for (txnp = SH_TAILQ_FIRST(&mgr->region->active_txn, __txn_detail);
	    txnp != NULL;
	    txnp = SH_TAILQ_NEXT(txnp, links, __txn_detail)) {
		stats->st_txnarray[ndx].txnid = txnp->txnid;
		stats->st_txnarray[ndx].lsn = txnp->begin_lsn;
		ndx++;

		if (ndx >= stats->st_nactive)
			break;
	}

	stats->st_region_wait = mgr->region->hdr.lock.mutex_set_wait;
	stats->st_region_nowait = mgr->region->hdr.lock.mutex_set_nowait;
	stats->st_refcnt = mgr->region->hdr.refcnt;
	stats->st_regsize = mgr->region->hdr.size;

	UNLOCK_TXNREGION(mgr);
	*statp = stats;
	return (0);
}
