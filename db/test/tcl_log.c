/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)tcl_log.c	10.13 (Sleepycat) 4/10/98";
#endif /* not lint */

/*
 * This file is divided up into 5 sets of functions:
 * 1. The log command and its support functions.
 * 2. The logunlink command.
 * 3. The log manager widget commands.
 */
#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#endif
#include <tcl.h>

#include "db_int.h"
#include "dbtest.h"
#include "test_ext.h"

/* Internal functions */
static int get_lsn __P((Tcl_Interp *, char *, DB_LSN *));
static int log_compare_cmd __P((Tcl_Interp *interp, int, char *argv[]));
static int log_file_cmd __P((DB_LOG *, Tcl_Interp *interp, int, char *argv[]));
static int log_flush_cmd __P((DB_LOG *, Tcl_Interp *interp, int, char *argv[]));
static int log_get_cmd __P((DB_LOG *, Tcl_Interp *interp, int, char *argv[]));
static int log_put_cmd __P((DB_LOG *, Tcl_Interp *interp, int, char *argv[]));
static int log_reg_cmd __P((DB_LOG *, Tcl_Interp *interp, int, char *argv[]));

/*
 * log_cmd --
 *	Implements log_open for dbtest.  Log_open creates a log manager
 * and all the necessary files in the file system.  It then creates
 * a command that implements the other log functions.
 */

#define LOG_USAGE "log path flags mode [options]\n\toptions:\n"

typedef struct _log_data {
	DB_LOG *logp;
	DB_ENV *env;
} log_data;

int
log_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	static int log_number = 0;
	DB_LOG *lp;
	DB_ENV *env;
	log_data *ld;
	u_int32_t flags;
	int mode, tclint;
	char logname[50];

	notused = NULL;

	debug_check();

	/* Check number of arguments. */
	USAGE_GE(argc, 4, LOG_USAGE, DO_ENV);
	if (Tcl_GetInt(interp, argv[2], &tclint) != TCL_OK)
		return (TCL_ERROR);
	flags = (u_int32_t)tclint;

	if (Tcl_GetInt(interp, argv[3], &mode) != TCL_OK)
		return (TCL_ERROR);

	if (process_env_options(interp, argc, argv, &env))
		return (TCL_ERROR);

	if (F_ISSET(env, DB_ENV_STANDALONE))
		lp = env->lg_info;
	else if (log_open(argv[1], flags, mode, env, &lp) != 0) {
		Tcl_SetResult(interp, "NULL", TCL_STATIC);
		return (TCL_OK);
	} else
		env->lg_info = lp;

	if ((ld = (log_data *)malloc(sizeof(log_data))) == NULL) {
		if (!F_ISSET(env, DB_ENV_STANDALONE)) {
			(void)db_appexit(env);
			if (env->lk_conflicts)
				free(env->lk_conflicts);
			free(env);
		}
		Tcl_SetResult(interp, "lock_open: ", TCL_STATIC);
		errno = ENOMEM;
		Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
		return (TCL_ERROR);
	}
	ld->logp = lp;
	ld->env = env;

	/* Create new command name. */
	sprintf(&logname[0], "log%d", log_number);
	log_number++;

	/* Create widget command. */
	Tcl_CreateCommand(interp, logname, logwidget_cmd, (int *)ld, NULL);
	Tcl_SetResult(interp, logname, TCL_VOLATILE);
	return (TCL_OK);
}

/*
 * logunlink_cmd --
 *	Implements txn_unlink for dbtest.
 */

#define LOGUNLINK_USAGE "log_unlink path force"

int
logunlink_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	int force;

	notused = NULL;

	debug_check();

	USAGE_GE(argc, 3, LOGUNLINK_USAGE, 0);

	if (Tcl_GetInt(interp, argv[2], &force) != TCL_OK)
		return (TCL_ERROR);

	if (log_unlink(argv[1], force, NULL) != 0)
		Tcl_SetResult(interp, "-1", TCL_STATIC);
	else
		Tcl_SetResult(interp, "0", TCL_STATIC);

	return (TCL_OK);
}

/*
 * logwidget --
 * This is that command that implements the log widget.  If we
 * ever add new "methods" we add new widget commands here.
 */
#define LOGWIDGET_USAGE "logN option ?arg arg ...?"
#define LOGCLOSE_USAGE "logN close"
#define LOGFLUSH_USAGE "logN flush lsn"
#define LOGGET_USAGE "logN get lsn flags"
#define LOGCMP_USAGE "logN compare lsn1 lsn2"
#define LOGFILE_USAGE "logN file lsn"
#define LOGLAST_USAGE "logN last"
#define LOGPUT_USAGE "logN put record flags"
#define LOGREG_USAGE "logN register db name type"
#define LOG_UNREG "logN unregister id"

int
logwidget_cmd(cd_lp, interp, argc, argv)
	ClientData cd_lp;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	DB_LOG *lp;
	DB_ENV *env;
	DB_LSN lsn;
	DBT rec;
	int id, ret;
	char fstr[16], dstr[16];

	debug_check();

	lp = ((log_data *)cd_lp)->logp;

	USAGE_GE(argc, 2, LOGWIDGET_USAGE, 0);

	if (strcmp(argv[1], "close") == 0) {
		USAGE(argc, 2, LOGCLOSE_USAGE, 0);
		env = ((log_data *)cd_lp)->env;
		if (!F_ISSET(env, DB_ENV_STANDALONE)) {
			(void)db_appexit(env);
			if (env->lk_conflicts)
				free(env->lk_conflicts);
			free(env);
		}
		ret = Tcl_DeleteCommand(interp, argv[0]);
	} else if (strcmp(argv[1], "flush") == 0) {
		ret =  log_flush_cmd(lp, interp, argc, argv);
	} else if (strcmp(argv[1], "get") == 0) {
		return (log_get_cmd(lp, interp, argc, argv));
	} else if (strcmp(argv[1], "last") == 0) {
		memset(&rec, 0, sizeof(rec));
		ret = log_get(lp, &lsn, &rec, DB_LAST);
		if (ret == 0) {
			sprintf(fstr, "%lu", (u_long)lsn.file);
			sprintf(dstr, "%lu", (u_long)lsn.offset);
			Tcl_ResetResult(interp);
			Tcl_AppendResult(interp, fstr, " ", dstr, 0);
			return (TCL_OK);
		}
	} else if (strcmp(argv[1], "compare") == 0) {
		return (log_compare_cmd(interp, argc, argv));
	} else if (strcmp(argv[1], "file") == 0) {
		return (log_file_cmd(lp, interp, argc, argv));
	} else if (strcmp(argv[1], "put") == 0) {
		return (log_put_cmd(lp, interp, argc, argv));
	} else if (strcmp(argv[1], "register") == 0) {
		return (log_reg_cmd(lp, interp, argc, argv));
	} else if (strcmp(argv[1], "unregister") == 0) {
		if (Tcl_GetInt(interp, argv[2], &id) != TCL_OK)
			return (TCL_ERROR);
		ret = log_unregister(lp, id);
	} else {
		Tcl_SetResult(interp, LOGWIDGET_USAGE, TCL_STATIC);
		return (TCL_ERROR);
	}

	if (ret != 0) {
		Tcl_SetResult(interp, "log_cmd: ", TCL_STATIC);
		errno = ret;
		Tcl_AppendResult(interp, argv[2], " ",
		    Tcl_PosixError(interp), 0);
	} else
		Tcl_SetResult(interp, "0", TCL_STATIC);
	return (TCL_OK);
}

/*
 * Implementation of all the individual widget commands.
 */
static int
log_compare_cmd(interp, argc, argv)
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	DB_LSN lsn1, lsn2;
	char resbuf[32];
	int cmp;

	USAGE(argc, 4, LOGCMP_USAGE, 0);
	if (get_lsn(interp, argv[2], &lsn1) != TCL_OK ||
	    get_lsn(interp, argv[3], &lsn2) != TCL_OK)
		return (TCL_ERROR);

	cmp = log_compare(&lsn1, &lsn2);
	sprintf(resbuf, "%d", cmp);
	Tcl_SetResult(interp, resbuf, TCL_VOLATILE);
	return (TCL_OK);
}

static int
log_flush_cmd(lp, interp, argc, argv)
	DB_LOG *lp;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	DB_LSN lsn;
	int ret;

	USAGE(argc, 3, LOGFLUSH_USAGE, 0);
	if (get_lsn(interp, argv[2], &lsn) != TCL_OK)
		return (TCL_ERROR);
	if (lsn.file == 0 && lsn.offset == 0)
		ret = log_flush(lp, NULL);
	else
		ret = log_flush(lp, &lsn);
	if (ret != 0)
		return (TCL_ERROR);

	Tcl_SetResult(interp, "0", TCL_STATIC);
	return (TCL_OK);
}

static int
log_file_cmd(lp, interp, argc, argv)
	DB_LOG *lp;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	DB_LSN lsn;
	int ret;
	char name[1024];

	USAGE(argc, 3, LOGFILE_USAGE, 0);
	if (get_lsn(interp, argv[2], &lsn) != TCL_OK)
		return (TCL_ERROR);
	if ((ret = log_file(lp, &lsn, name, sizeof(name))) != 0) {
		Tcl_SetResult(interp, "log_file: ", TCL_STATIC);
		errno = ret;
		Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
		return (TCL_OK);
	}
	Tcl_SetResult(interp, name, TCL_VOLATILE);
	return (TCL_OK);
}

static int
log_get_cmd(lp, interp, argc, argv)
	DB_LOG *lp;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	DB_LSN lsn;
	DBT data;
	u_int32_t flags;
	int ret, tclint;

	USAGE(argc, 4, LOGGET_USAGE, 0);
	if (get_lsn(interp, argv[2], &lsn) != TCL_OK)
		return (TCL_ERROR);
	if (Tcl_GetInt(interp, argv[3], &tclint) != TCL_OK)
		return (TCL_ERROR);
	flags = (u_int32_t)tclint;

	memset(&data, 0, sizeof(data));
	switch (ret = log_get(lp, &lsn, &data, flags)) {
	case 0:	Tcl_SetResult(interp, data.data, TCL_VOLATILE);
		break;
	case DB_NOTFOUND: Tcl_ResetResult(interp);
		break;
	default:
		Tcl_SetResult(interp, "log get: ", TCL_STATIC);
		errno = ret;
		Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
		break;
	}
	return (TCL_OK);
}

static int
log_put_cmd(lp, interp, argc, argv)
	DB_LOG *lp;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	DB_LSN lsn;
	DBT data;
	u_int32_t flags;
	int ret, tclint;
	char resbuf[64];

	USAGE(argc, 4, LOGPUT_USAGE, 0);

	if (Tcl_GetInt(interp, argv[3], &tclint) != TCL_OK)
		return (TCL_ERROR);
	flags = (u_int32_t)tclint;

	memset(&data, 0, sizeof(data));
	data.data = argv[2];
	data.size = strlen(argv[2]) + 1;
	if ((ret = log_put(lp, &lsn, &data, flags)) != 0) {
		errno = ret;
		Tcl_SetResult(interp, "ERROR: ", TCL_STATIC);
		Tcl_AppendResult(interp, Tcl_PosixError(interp), NULL);
		return (TCL_ERROR);
	}

	sprintf(resbuf, "%lu %lu", (unsigned long)lsn.offset,
	    (unsigned long)lsn.file);
	Tcl_SetResult(interp, resbuf, TCL_VOLATILE);
	return (TCL_OK);
}

static int
log_reg_cmd(lp, interp, argc, argv)
	DB_LOG *lp;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	DB *dbp;
	DBTYPE type;
	Tcl_CmdInfo cmd_info;
	u_int32_t regid;
	int ret, tclint;

	USAGE(argc, 5, LOGREG_USAGE, 0);
	if (Tcl_GetInt(interp, argv[4], &tclint) != TCL_OK)
		return (TCL_ERROR);
	type = (DBTYPE)tclint;

	if (Tcl_GetCommandInfo(interp, argv[2], &cmd_info) != TCL_OK)
		dbp = NULL;
	else
		dbp = (DB *)cmd_info.clientData;
	ret = log_register(lp, dbp, argv[3], type, &regid);
	if (ret != 0) {
		errno = ret;
		Tcl_SetResult(interp, "ERROR: ", TCL_STATIC);
		Tcl_AppendResult(interp, Tcl_PosixError(interp), NULL);
		return (TCL_ERROR);
	}
	sprintf(interp->result, "%lu", (unsigned long)regid);
	return (TCL_OK);
}

static int
get_lsn(interp, str, lsnp)
	Tcl_Interp *interp;
	char *str;
	DB_LSN *lsnp;
{
	int largc, tclint;
	char **largv;

	if (Tcl_SplitList(interp, str, &largc, &largv) != TCL_OK || largc != 2)
		return (TCL_ERROR);
	if (Tcl_GetInt(interp, largv[0], &tclint) != TCL_OK)
		return (TCL_ERROR);
	lsnp->offset = (u_int32_t)tclint;
	if (Tcl_GetInt(interp, largv[1], &tclint) != TCL_OK)
		return (TCL_ERROR);
	lsnp->file = (u_int32_t)tclint;
	return (TCL_OK);
}
