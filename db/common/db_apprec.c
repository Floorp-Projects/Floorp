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
static const char sccsid[] = "@(#)db_apprec.c	10.30 (Sleepycat) 5/3/98";
#endif

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <string.h>
#include <time.h>
#endif

#include "db_int.h"
#include "shqueue.h"
#include "db_page.h"
#include "db_dispatch.h"
#include "db_am.h"
#include "log.h"
#include "txn.h"
#include "common_ext.h"

/*
 * __db_apprec --
 *	Perform recovery.
 *
 * PUBLIC: int __db_apprec __P((DB_ENV *, u_int32_t));
 */
int
__db_apprec(dbenv, flags)
	DB_ENV *dbenv;
	u_int32_t flags;
{
	DBT data;
	DB_LOG *lp;
	DB_LSN ckp_lsn, first_lsn, lsn;
	time_t now;
	u_int32_t is_thread;
	int ret;
	void *txninfo;

	lp = dbenv->lg_info;

	/* Initialize the transaction list. */
	if ((ret = __db_txnlist_init(&txninfo)) != 0)
		return (ret);

	/*
	 * Save the state of the thread flag -- we don't need it on at the
	 * moment because we're single-threaded until recovery is complete.
	 */
	is_thread = F_ISSET(lp, DB_AM_THREAD);
	F_CLR(lp, DB_AM_THREAD);

	/*
	 * Recovery is done in three passes:
	 * Pass #1:
	 *	Read forward through the log from the last checkpoint to the
	 *	end of the log, opening and closing files so that at the end
	 *	of the log we have the "current" set of files open.
	 * Pass #2:
	 *	Read backward through the log undoing any uncompleted TXNs.
	 *	If doing catastrophic recovery, we read to the beginning of
	 *	the log, otherwise, to the most recent checkpoint that occurs
	 *	before the most recent checkpoint LSN, which is returned by
	 *	__log_findckp().  During this pass, checkpoint file information
	 *	is ignored, and file openings and closings are undone.
	 * Pass #3:
	 *	Read forward through the log from the LSN found in pass #2,
	 *	redoing any committed TXNs.  During this pass, checkpoint
	 *	file information is ignored, and file openings and closings
	 *	are redone.
	 */

	/*
	 * Find the last checkpoint in the log.  This is the point from which
	 * we want to begin pass #1 (the TXN_OPENFILES pass).
	 */
	memset(&data, 0, sizeof(data));
	if ((ret = log_get(lp, &ckp_lsn, &data, DB_CHECKPOINT)) != 0) {
		/*
		 * If we don't find a checkpoint, start from the beginning.
		 * If that fails, we're done.  Note, we do not require that
		 * there be log records if we're performing recovery.
		 */
		if ((ret = log_get(lp, &ckp_lsn, &data, DB_FIRST)) != 0) {
			if (ret == DB_NOTFOUND)
				ret = 0;
			else
				__db_err(dbenv, "First log record not found");
			goto out;
		}
	}

	/*
	 * Now, ckp_lsn is either the lsn of the last checkpoint or the lsn
	 * of the first record in the log.  Begin the TXN_OPENFILES pass from
	 * that lsn, and proceed to the end of the log.
	 */
	lsn = ckp_lsn;
	for (;;) {
		ret = __db_dispatch(lp, &data, &lsn, TXN_OPENFILES, txninfo);
		if (ret != 0 && ret != DB_TXN_CKP)
			goto msgerr;
		if ((ret = log_get(lp, &lsn, &data, DB_NEXT)) != 0) {
			if (ret == DB_NOTFOUND)
				break;
			goto out;
		}
	}

	/*
	 * Pass #2.
	 *
	 * Before we can begin pass #2, backward roll phase, we determine how
	 * far back in the log to recover.  If we are doing catastrophic
	 * recovery, then we go as far back as we have files.  If we are
	 * doing normal recovery, we go as back to the most recent checkpoint
	 * that occurs before the most recent checkpoint LSN.
	 */
	if (LF_ISSET(DB_RECOVER_FATAL)) {
		ZERO_LSN(first_lsn);
	} else
		if ((ret = __log_findckp(lp, &first_lsn)) == DB_NOTFOUND) {
			/*
			 * We don't require that log files exist if recovery
			 * was specified.
			 */
			ret = 0;
			goto out;
		}

	if (dbenv->db_verbose)
		__db_err(lp->dbenv, "Recovery starting from [%lu][%lu]",
		    (u_long)first_lsn.file, (u_long)first_lsn.offset);

	for (ret = log_get(lp, &lsn, &data, DB_LAST);
	    ret == 0 && log_compare(&lsn, &first_lsn) > 0;
	    ret = log_get(lp, &lsn, &data, DB_PREV)) {
		ret = __db_dispatch(lp,
		    &data, &lsn, TXN_BACKWARD_ROLL, txninfo);
		if (ret != 0)
			if (ret != DB_TXN_CKP)
				goto msgerr;
			else
				ret = 0;
	}
	if (ret != 0 && ret != DB_NOTFOUND)
		goto out;

	/*
	 * Pass #3.
	 */
	for (ret = log_get(lp, &lsn, &data, DB_NEXT);
	    ret == 0; ret = log_get(lp, &lsn, &data, DB_NEXT)) {
		ret = __db_dispatch(lp, &data, &lsn, TXN_FORWARD_ROLL, txninfo);
		if (ret != 0)
			if (ret != DB_TXN_CKP)
				goto msgerr;
			else
				ret = 0;
	}
	if (ret != DB_NOTFOUND)
		goto out;

	/* Now close all the db files that are open. */
	__log_close_files(lp);

	/*
	 * Now set the last checkpoint lsn and the current time,
	 * take a checkpoint, and reset the txnid.
	 */
	(void)time(&now);
	dbenv->tx_info->region->last_ckp = ckp_lsn;
	dbenv->tx_info->region->time_ckp = (u_int32_t)now;
	if ((ret = txn_checkpoint(dbenv->tx_info, 0, 0)) != 0)
		goto out;
	dbenv->tx_info->region->last_txnid = TXN_MINIMUM;

	if (dbenv->db_verbose) {
		__db_err(lp->dbenv, "Recovery complete at %.24s", ctime(&now));
		__db_err(lp->dbenv, "%s %lx %s [%lu][%lu]",
		    "Maximum transaction id",
		    ((DB_TXNHEAD *)txninfo)->maxid,
		    "Recovery checkpoint",
		    (u_long)dbenv->tx_info->region->last_ckp.file,
		    (u_long)dbenv->tx_info->region->last_ckp.offset);
	}

	if (0) {
msgerr:		__db_err(dbenv, "Recovery function for LSN %lu %lu failed",
		    (u_long)lsn.file, (u_long)lsn.offset);
	}

out:	F_SET(lp, is_thread);
	__db_txnlist_end(txninfo);

	return (ret);
}
