/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)tcl_ndbm.c	10.12 (Sleepycat) 4/27/98";
#endif /* not lint */

/*
 * This file is divided up into 2 sets of functions:
 * 1. The ndbmopen command and its support functions.
 * 2. The ndbmwidget commands.
 */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#endif

#include <tcl.h>

#define	DB_DBM_HSEARCH	1
#include "db_int.h"

#include "dbtest.h"
#include "test_ext.h"

/*
 * ndbmopen_cmd --
 *	Implements dbm_open (the ndbm interface) for dbtest.  ndbm_open
 * creates a widget that implements all the commands found off in the
 * historical ndbm interface.
 */


#define NDBMOPEN_USAGE "ndbm_open file flags mode"

int
ndbmopen_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	static int db_number = 0;
	DBM *dbm;
	u_int32_t flags;
	int mode, oflags, tclint;
	char dbname[50], *name;

	notused = NULL;

	/* Check number of arguments. */
	USAGE_GE(argc, 4, NDBMOPEN_USAGE, 0);

	name = strcmp(argv[1], "NULL") == 0 ? NULL : argv[1];

	debug_check();

	/* Check flags and mode. */
	if (Tcl_GetInt(interp, argv[2], &tclint) != TCL_OK)
		goto usage;
	flags = (u_int32_t)tclint;
	if (Tcl_GetInt(interp, argv[3], &mode) != TCL_OK) {
usage:		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "\nUsage: ", NDBMOPEN_USAGE, NULL);
		return (TCL_OK);
	}
	oflags = 0;
	if (flags & DB_RDONLY)
		oflags |= O_RDONLY;
	else
		oflags |= O_RDWR;
	if (flags & DB_CREATE)
		oflags |= O_CREAT;
	if (flags & DB_TRUNCATE)
		oflags |= O_TRUNC;

	/* Call dbm_open. */
	if ((dbm = dbm_open(name, oflags, mode)) == NULL) {
		Tcl_SetResult(interp, "ndbm_open: ", TCL_STATIC);
		Tcl_AppendResult(interp, Tcl_PosixError(interp), NULL);
		return (TCL_OK);
	}

	/* Create widget command. */
	/* Create new command name. */
	sprintf(&dbname[0], "ndbm%d", db_number);
	db_number++;

	Tcl_CreateCommand(interp, dbname, ndbmwidget_cmd,
	    (ClientData)dbm, NULL);
	Tcl_SetResult(interp, dbname, TCL_VOLATILE);
	return (TCL_OK);
}


/*
 * ndbmwidget --
 *	Since ndbm_open creates a widget, we need a command that then
 * handles all the widget commands.  This is that command.  If we
 * ever add new "methods" we add new widget commands here.
 */

#define NDBMWIDGET_USAGE "ndbmN option ?arg arg ...?"
#define NDBMCLEAR_USAGE "ndbmN clearerr"
#define NDBMCLOSE_USAGE "ndbmN close"
#define NDBMDEL_USAGE "ndbmN delete key"
#define NDBMDIRFNO_USAGE "ndbmM dirfno"
#define NDBMERROR_USAGE "ndbmN error"
#define NDBMFETCH_USAGE "ndbmN fetch key"
#define NDBMFIRST_USAGE "ndbmN firstkey"
#define NDBMNEXT_USAGE "ndbmN nextkey"
#define NDBMPAGFNO_USAGE "ndbmM pagfno"
#define NDBMRDONLY_USAGE "ndbmM rdonly"
#define NDBMSTORE_USAGE "ndbmN store key datum flags"

int
ndbmwidget_cmd(cd_dbm, interp, argc, argv)
	ClientData cd_dbm;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	DBM *dbm;
	datum data, key;
	int flags, ret;
	char *cmd, fdval[16];

	COMPQUIET(cmd, NULL);

	USAGE_GE(argc, 2, NDBMWIDGET_USAGE, 0);

	Tcl_SetResult(interp, "0", TCL_STATIC);

	dbm = (DBM *)cd_dbm;

	if (strcmp(argv[1], "clearerr") == 0) {
		USAGE(argc, 2, NDBMCLEAR_USAGE, 0);
		debug_check();
		ret = dbm_clearerr(dbm);
		cmd = "dbm_clearerr:";
	} else if (strcmp(argv[1], "close") == 0) {
		USAGE(argc, 2, NDBMCLOSE_USAGE, 0);
		debug_check();
		dbm_close(dbm);
		(void)Tcl_DeleteCommand(interp, argv[0]);
		ret = 0;
	} else if (strcmp(argv[1], "delete") == 0) {
		USAGE(argc, 3, NDBMDEL_USAGE, 0);
		key.dptr = argv[2];
		key.dsize = strlen(argv[2]) + 1;
		debug_check();
		ret = dbm_delete(dbm, key);
		cmd = "dbm_delete:";
	} else if (strcmp(argv[1], "dirfno") == 0) {
		USAGE(argc, 2, NDBMDIRFNO_USAGE, 0);
		ret = dbm_dirfno(dbm);
		sprintf(fdval, "%d", ret);
		Tcl_SetResult(interp, fdval, TCL_VOLATILE);
		return (TCL_OK);
	} else if (strcmp(argv[1], "error") == 0) {
		USAGE(argc, 2, NDBMERROR_USAGE, 0);
		debug_check();
		errno = dbm_error(dbm);
		Tcl_SetResult(interp, Tcl_PosixError(interp), TCL_STATIC);
		return (TCL_OK);
	} else if (strcmp(argv[1], "fetch") == 0) {
		USAGE(argc, 3, NDBMFETCH_USAGE, 0);
		key.dptr = argv[2];
		key.dsize = strlen(argv[2]) + 1;
		debug_check();
		data = dbm_fetch(dbm, key);
		if (data.dptr == NULL)
			Tcl_SetResult(interp, "-1", TCL_STATIC);
		else
			Tcl_SetResult(interp, data.dptr, TCL_VOLATILE);
		return (TCL_OK);
	} else if (strcmp(argv[1], "firstkey") == 0) {
		USAGE(argc, 2, NDBMFIRST_USAGE, 0);
		debug_check();
		key = dbm_firstkey(dbm);
		if (key.dptr == NULL)
			Tcl_SetResult(interp, "-1", TCL_STATIC);
		else
			Tcl_SetResult(interp, key.dptr, TCL_VOLATILE);
		return (TCL_OK);
	} else if (strcmp(argv[1], "nextkey") == 0) {
		USAGE(argc, 2, NDBMNEXT_USAGE, 0);
		debug_check();
		data = dbm_nextkey(dbm);
		if (data.dptr == NULL)
			Tcl_SetResult(interp, "-1", TCL_STATIC);
		else
			Tcl_SetResult(interp, data.dptr, TCL_VOLATILE);
		return (TCL_OK);
	} else if (strcmp(argv[1], "pagfno") == 0) {
		USAGE(argc, 2, NDBMPAGFNO_USAGE, 0);
		ret = dbm_pagfno(dbm);
		sprintf(fdval, "%d", ret);
		Tcl_SetResult(interp, fdval, TCL_VOLATILE);
		return (TCL_OK);
	} else if (strcmp(argv[1], "rdonly") == 0) {
		USAGE(argc, 2, NDBMRDONLY_USAGE, 0);
		if (dbm_rdonly(dbm))
			Tcl_SetResult(interp, "1", TCL_STATIC);
		else
			Tcl_SetResult(interp, "0", TCL_STATIC);
		return (TCL_OK);
	} else if (strcmp(argv[1], "store") == 0) {
		USAGE(argc, 5, NDBMSTORE_USAGE, 0);
		if (Tcl_GetInt(interp, argv[4], &flags) != TCL_OK)
			return (TCL_ERROR);
		key.dptr = argv[2];
		key.dsize = strlen(argv[2]) + 1;
		data.dptr = argv[3];
		data.dsize = strlen(argv[3]) + 1;
		debug_check();
		ret = dbm_store(dbm, key, data, flags);
		cmd = "dbm_store:";
	} else {
		Tcl_SetResult(interp, NDBMWIDGET_USAGE, TCL_STATIC);
		return (TCL_ERROR);
	}

	if (ret == 0)
		Tcl_SetResult(interp, "0", TCL_STATIC);
	else if (ret > 0)
		Tcl_SetResult(interp, "Not found", TCL_STATIC);
	else {
		Tcl_SetResult(interp, cmd, TCL_STATIC);
		Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
	}
	return (TCL_OK);
}
