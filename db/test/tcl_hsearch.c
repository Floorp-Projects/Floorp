/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)tcl_hsearch.c	10.6 (Sleepycat) 4/27/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif
#include <tcl.h>

#define	DB_DBM_HSEARCH	1
#include "db_int.h"

#include "dbtest.h"
#include "test_ext.h"

/*
 * hcreate_cmd --
 *	Implements hcreate for dbtest.  hcreate creates an in-memory
 * database compatible with the historical hsearch interface.
 */
#define HCREATE_USAGE "hcreate nelem"
int
hcreate_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	int nelem;

	notused = NULL;

	/* Check number of arguments. */
	USAGE_GE(argc, 2, HCREATE_USAGE, 0);

	if (Tcl_GetInt(interp, argv[1], &nelem) != TCL_OK) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "\nUsage: ", HCREATE_USAGE, NULL);
		return (TCL_OK);
	}
	debug_check();

	/* Call hcreate. */
	if (hcreate(nelem) == 0) {
		Tcl_SetResult(interp, "hcreate: ", TCL_STATIC);
		Tcl_AppendResult(interp, Tcl_PosixError(interp), NULL);
		return (TCL_OK);
	}

	Tcl_SetResult(interp, "0", TCL_STATIC);
	return (TCL_OK);
}

#define HSEARCH_USAGE "hsearch key data find/enter"
int
hsearch_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	ENTRY item, *result;
	ACTION action;

	USAGE(argc, 4, HSEARCH_USAGE, 0);
	notused = NULL;

	item.key = argv[1];
	item.data = argv[2];
	if (strcmp(argv[3], "find") == 0)
		action = FIND;
	else if (strcmp(argv[3], "enter") == 0)
		action = ENTER;
	else
		USAGE(0, 4, HSEARCH_USAGE, 0);

	debug_check();

	if ((result = hsearch(item, action)) == NULL)
		Tcl_SetResult(interp, "-1", TCL_STATIC);
	else if (action == FIND)
		Tcl_SetResult(interp, (char *)result->data, TCL_STATIC);
	else
		/* action is ENTER */
		Tcl_SetResult(interp, "0", TCL_STATIC);

	return (TCL_OK);
}

#define HDESTROY_USAGE "hdestroy"
int
hdestroy_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	COMPQUIET(argv, NULL);

	USAGE_GE(argc, 1, HDESTROY_USAGE, 0);
	notused = NULL;

	debug_check();
	hdestroy();
	Tcl_SetResult(interp, "0", TCL_STATIC);
	return (TCL_OK);
}
