/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)tcl_txn.c	10.15 (Sleepycat) 5/31/98";
#endif /* not lint */

/*
 * This file is divided up into 5 sets of functions:
 * 1. The txn command and its support functions.
 * 2. The txn_unlink command.
 * 3. The txn manager widget commands.
 * 4. The txn widget commands.
 */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#endif
#include <tcl.h>

#include "db_int.h"
#include "dbtest.h"
#include "test_ext.h"

typedef struct _txn_data {
	DB_ENV *env;
	DB_TXNMGR *txnp;
} txn_data;

/*
 * txn_cmd --
 *	Implements txn_open for dbtest.  Txn_open creates a transaction
 * manager and all the necessary files in the file system.  It then creates
 * a command that implements the other transaction functions.
 */

#define TXNMGR_USAGE "txn path flags mode [options]\n\toptions:\n"

int
txnmgr_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	static int mgr_number = 0;
	DB_ENV *env;
	DB_TXNMGR *mgrp;
	txn_data *td;
	int mode, tclint;
	u_int32_t flags;
	char mgrname[50];

	notused = NULL;
	debug_check();

	/* Check number of arguments. */
	USAGE_GE(argc, 4, TXNMGR_USAGE, DO_ENV);
	if (Tcl_GetInt(interp, argv[2], &tclint) != TCL_OK)
		return (TCL_ERROR);
	flags = (u_int32_t)tclint;

	/* Don't specify DB_THREAD if the architecture can't do spinlocks. */
#ifndef HAVE_SPINLOCKS
	LF_CLR(DB_THREAD);
#endif
	if (Tcl_GetInt(interp, argv[3], &mode) != TCL_OK)
		return (TCL_ERROR);

	/*
	 * Call txn_open.
	 * For now the recovery proc is NULL
	 */
	if (process_env_options(interp, argc, argv, &env)) {
		Tcl_SetResult(interp, "NULL", TCL_STATIC);
		return (TCL_OK);
	}

	if (F_ISSET(env, DB_ENV_STANDALONE))
		mgrp = env->tx_info;
	else if (txn_open(argv[1], flags, mode, env, &mgrp) != 0) {
		Tcl_SetResult(interp, "NULL", TCL_STATIC);
		return (TCL_OK);
	} else
		env->tx_info = mgrp;

	td = (txn_data *)malloc(sizeof(txn_data));
	if (td == NULL) {
		if (!F_ISSET(env, DB_ENV_STANDALONE)) {
			(void)db_appexit(env);
			if (env->lk_conflicts)
				free(env->lk_conflicts);
			free(env);
		}
		Tcl_SetResult(interp, "txn_open: ", TCL_STATIC);
		errno = ENOMEM;
		Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
		return (TCL_ERROR);
	}
	td->txnp = mgrp;
	td->env = env;

	/* Create new command name. */
	sprintf(&mgrname[0], "mgr%d", mgr_number);
	mgr_number++;

	/* Create widget command. */
	Tcl_CreateCommand(interp, mgrname, txnwidget_cmd, (int *)td, NULL);
	Tcl_SetResult(interp, mgrname, TCL_VOLATILE);
	return (TCL_OK);
}

/*
 * txnunlink_cmd --
 *	Implements txn_unlink for dbtest.
 */

#define TXNUNLINK_USAGE "txn_unlink path force"

int
txnunlink_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	int force;

	notused = NULL;
	debug_check();

	USAGE_GE(argc, 3, TXNUNLINK_USAGE, 0);

	if (Tcl_GetInt(interp, argv[2], &force) != TCL_OK)
		return (TCL_ERROR);

	if (txn_unlink(argv[1], force, NULL) != 0)
		Tcl_SetResult(interp, "-1", TCL_STATIC);
	else
		Tcl_SetResult(interp, "0", TCL_STATIC);
	return (TCL_OK);
}

/*
 * txnwidget --
 * This is that command that implements the txn widget.  If we
 * ever add new "methods" we add new widget commands here.
 */
#define TXNWIDGET_USAGE "mgrN option ?arg arg ...?"
#define TXNBEGIN_USAGE "mgrN begin [parent]"
#define TXNCHECK_USAGE "mgrN checkpoint [kbytes] [minutes]"
#define TXNCLOSE_USAGE "mgrN close"
#define TXNSTAT_USAGE "mgrN stat"

int
txnwidget_cmd(cd_mgr, interp, argc, argv)
	ClientData cd_mgr;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	static int id = 0;
	DB_ENV *env;
	DB_TXN *txn, *parent;
	DB_TXNMGR *mgr;
	DB_TXN_STAT *statp;
	Tcl_CmdInfo info;
	u_int32_t i, kbytes, minutes;
	int ret, tclint;
	char *p, *statbuf, txnname[128];

	debug_check();

	mgr = ((txn_data *)cd_mgr)->txnp;

	USAGE_GE(argc, 2, TXNWIDGET_USAGE, 0);

	if (strcmp(argv[1], "close") == 0) {
		USAGE(argc, 2, TXNCLOSE_USAGE, 0);
		env = ((txn_data *)cd_mgr)->env;
		if (!F_ISSET(env, DB_ENV_STANDALONE)) {
			(void)db_appexit(env);
			if (env->lk_conflicts)
				free(env->lk_conflicts);
			free(env);
		}
		ret = Tcl_DeleteCommand(interp, argv[0]);
		if (ret)
			Tcl_SetResult(interp, "-1", TCL_STATIC);
		else
			Tcl_SetResult(interp, "0", TCL_STATIC);
		return (ret);
	} else if (strcmp(argv[1], "begin") == 0) {
		USAGE_GE(argc, 2, TXNBEGIN_USAGE, 0);
		if (argc == 3) {
			if (Tcl_GetCommandInfo(interp, argv[2], &info) == 0) {
				Tcl_SetResult(interp,
				    "txn_begin: Invalid argument ", TCL_STATIC);
				Tcl_AppendResult(interp, argv[2],
				    " not a transaction.", 0);
				return (TCL_ERROR);
			}
			parent = (DB_TXN *)(info.clientData);
		} else {
			USAGE(argc, 2, TXNBEGIN_USAGE, 0);
			parent = NULL;
		}

		if ((ret = txn_begin(mgr, parent, &txn)) != 0) {
			Tcl_SetResult(interp, "NULL", TCL_STATIC);
			return (TCL_OK);
		}

		sprintf(&txnname[0], "%s.txn%d", argv[0], id);
		id++;

		Tcl_CreateCommand(interp, txnname, txn_cmd, (int *)txn, NULL);
		Tcl_SetResult(interp, txnname, TCL_VOLATILE);
	} else if (strcmp(argv[1], "check") == 0) {
		USAGE_GE(argc, 2, TXNCHECK_USAGE, 0);
		if (argc <= 2 ||
		    (Tcl_GetInt(interp, argv[2], &tclint) != TCL_OK))
			tclint = 0;
		kbytes = (u_int32_t)tclint;

		if (argc <= 3 ||
		    (Tcl_GetInt(interp, argv[3], &tclint) != TCL_OK))
			tclint = 0;
		minutes = (u_int32_t)tclint;

		if ((ret = txn_checkpoint(mgr, kbytes, minutes)) != 0) {
			Tcl_SetResult(interp, "txn_checkpoint: ", TCL_STATIC);
			if (ret > 0) {
				errno = ret;
				Tcl_AppendResult(interp,
				    Tcl_PosixError(interp), 0);
			}  else
				Tcl_AppendResult(interp,
				    "Checkpoint pending", 0);
			return (TCL_ERROR);
		}
	} else if (strcmp(argv[1], "stat") == 0) {
		USAGE_GE(argc, 2, TXNSTAT_USAGE, 0);
		if ((ret = txn_stat(mgr, &statp, NULL)) != 0) {
			errno = ret;
			Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
			return (TCL_ERROR);
		}
		/*
		 * Allocate space for return message. Assume every u_int32_t
		 * will be printed out maximum size and with a label of 12
		 * bytes.  Then figure that we need 3 u_int32_t's for each
		 * active transaction. Leave plenty of room for newlines
		 * and space.
		 */
		statbuf = (char *)malloc(sizeof(DB_TXN_STAT) / sizeof(u_int32_t)
		    + (statp->st_nactive * 3) * (24 + 16));
		sprintf(statbuf, "%s %lx\n%s [%lu, %lu]\n%s %lu\n%s %lu\n%s %lu\n%s %lu\n%s %lu\n%s [%lu, %lu]\n",
		    "last txn id  ", (unsigned long)statp->st_last_txnid,
		    "last ckp     ", (unsigned long)statp->st_last_ckp.file,
		    (unsigned long)statp->st_last_ckp.offset,
		    "max txns     ", (unsigned long)statp->st_maxtxns,
		    "Aborted txns ", (unsigned long)statp->st_naborts,
		    "Begun txns   ", (unsigned long)statp->st_nbegins,
		    "Commited txns", (unsigned long)statp->st_ncommits,
		    "Active txns  ", (unsigned long)statp->st_nactive,
		    "Pending ckp  ", (unsigned long)statp->st_pending_ckp.file,
		        (unsigned long)statp->st_pending_ckp.offset);
		Tcl_AppendResult(interp, "last ckp time ",
		    ctime(&statp->st_time_ckp), NULL);
		p = statbuf + strlen(statbuf);
		for (i = 0; i < statp->st_nactive; i++) {
			sprintf(p, "%lx: %lu/%lu\n",
			    (unsigned long)statp->st_txnarray[i].txnid,
			    (unsigned long)statp->st_txnarray[i].lsn.file,
			    (unsigned long)statp->st_txnarray[i].lsn.offset);
			p += strlen(p);
		}
		Tcl_SetResult(interp, statbuf, TCL_DYNAMIC);
	} else {
		Tcl_SetResult(interp, TXNWIDGET_USAGE, TCL_STATIC);
		return (TCL_ERROR);
	}
	return (TCL_OK);
}

#define TXN_USAGE "mgrN.txnM cmd ?arg arg ...?"
#define TXNCOMMIT_USAGE "mgrN.txnM commit"
#define TXNABORT_USAGE "mgrN.txnM abort"
#define TXNID_USAGE "mgrN.txnM id"
#define TXNPREP_USAGE "mgrN.txnM prep"
int
txn_cmd(cd_txn, interp, argc, argv)
	ClientData cd_txn;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	DB_TXN *txn;
	int ret;
	char idbuf[16];

	debug_check();

	USAGE_GE(argc, 2, TXN_USAGE, 0);

	txn = (DB_TXN *)cd_txn;

	if (strcmp(argv[1], "commit") == 0) {
		USAGE(argc, 2, TXNCOMMIT_USAGE, 0);
		Tcl_DeleteCommand(interp, argv[0]);
		ret =  txn_commit(txn);
	} else if (strcmp(argv[1], "abort") == 0) {
		USAGE(argc, 2, TXNABORT_USAGE, 0);
		Tcl_DeleteCommand(interp, argv[0]);
		ret = txn_abort(txn);
	} else if (strcmp(argv[1], "id") == 0) {
		USAGE(argc, 2, TXNID_USAGE, 0);
		ret = txn_id(txn);
		sprintf(idbuf, "%d", ret);
		Tcl_SetResult(interp, idbuf, TCL_VOLATILE);
		return (TCL_OK);
	} else if (strcmp(argv[1], "prepare") == 0) {
		USAGE(argc, 2, TXNPREP_USAGE, 0);
		ret = txn_prepare(txn);
	} else {
		Tcl_SetResult(interp, TXN_USAGE, TCL_STATIC);
		return (TCL_ERROR);
	}
	if (ret)
		Tcl_SetResult(interp, "-1", TCL_STATIC);
	else
		Tcl_SetResult(interp, "0", TCL_STATIC);
	return (TCL_OK);
}
