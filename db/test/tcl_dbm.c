/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)tcl_dbm.c	10.8 (Sleepycat) 4/27/98";
#endif /* not lint */

/*
 * This file is divided up into 2 sets of functions:
 * 1. The dbminit command.
 * 3. The dbm support functions (e.g. fetch, store, delete)
 */
#include <string.h>
#include <tcl.h>

#define	DB_DBM_HSEARCH	1
#include <db.h>

#include "dbtest.h"
#include "test_ext.h"

/*
 * dbminit_cmd --
 *	Implements dbminit for dbtest.  Dbminit creates a widget that
 * implements all the commands found off in the historical dbm interface.
 */


#define DBMINIT_USAGE "dbminit file"

int
dbminit_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	char *name;
	int ret;

	notused = NULL;

	/* Check number of arguments. */
	USAGE_GE(argc, 2, DBMINIT_USAGE, 0);

	name = strcmp(argv[1], "NULL") == 0 ? NULL : argv[1];

	debug_check();

	/* Call dbminit. */
	ret = dbminit(name);
	if (ret != 0) {
		Tcl_SetResult(interp, "dbminit: ", TCL_STATIC);
		Tcl_AppendResult(interp, Tcl_PosixError(interp), NULL);
		return (TCL_OK);
	}

	Tcl_SetResult(interp, "0", TCL_STATIC);
	return (TCL_OK);
}

/* DBM support functions. */

#define DBMDEL_USAGE "delete key"
int
dbm_delete_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	datum key;

	USAGE(argc, 2, DBMDEL_USAGE, 0);
	notused = NULL;

	key.dptr = argv[1];
	key.dsize = (int)strlen(argv[1]) + 1;	/* Add NULL on end. */

	debug_check();

	if (delete(key) == 0)
		Tcl_SetResult(interp, "0", TCL_STATIC);
	else
		Tcl_SetResult(interp, "-1", TCL_STATIC);

	return (TCL_OK);
}

#define DBMFETCH_USAGE "fetch key"
int
dbm_fetch_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	datum data, key;

	USAGE_GE(argc, 2, DBMFETCH_USAGE, 0);
	notused = NULL;

	key.dptr = argv[1];
	key.dsize = strlen(argv[1]) + 1;	/* Add Null on end */

	debug_check();

	data = fetch(key);
	if (data.dptr == NULL)
		Tcl_SetResult(interp, "-1", TCL_STATIC);
	else
		Tcl_SetResult(interp, data.dptr, TCL_VOLATILE);
	return (TCL_OK);
}


#define DBMSTORE_USAGE "store key data"

int
dbm_store_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	datum data, key;

	USAGE_GE(argc, 3, DBMSTORE_USAGE, 0);
	notused = NULL;

	key.dptr = argv[1];
	key.dsize = strlen(argv[1]) + 1;	/* Add Null on end */

	data.dptr = argv[2];
	data.dsize = strlen(argv[2]) + 1;

	debug_check();

	if (store(key, data) == 0)
		Tcl_SetResult(interp, "0", TCL_STATIC);
	else
		Tcl_SetResult(interp, "-1", TCL_STATIC);
	return (TCL_OK);
}

#define DBMFIRST_USAGE "firstkey"
int
dbm_first_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	datum key;

	USAGE(argc, 1, DBMFIRST_USAGE, 0);
	argv = argv;
	notused = NULL;

	key = firstkey();

	if (key.dptr != 0)
		Tcl_SetResult(interp, key.dptr, TCL_VOLATILE);
	else
		Tcl_SetResult(interp, "-1", TCL_STATIC);

	return (TCL_OK);
}

#define DBMNEXT_USAGE "nextkey key"
int
dbm_next_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	datum nkey, key;

	USAGE(argc, 2, DBMNEXT_USAGE, 0);
	notused = NULL;

	key.dptr = argv[1];
	key.dsize = strlen(argv[1]) + 1;

	nkey = nextkey(key);

	if (nkey.dptr != 0)
		Tcl_SetResult(interp, nkey.dptr, TCL_VOLATILE);
	else
		Tcl_SetResult(interp, "-1", TCL_STATIC);

	return (TCL_OK);
}
