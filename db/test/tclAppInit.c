/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)tclAppInit.c	10.7 (Sleepycat) 4/27/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#endif
#include <tcl.h>

#include "db_int.h"
#include "test_ext.h"

/*
 * XXX
 * This is necessary so we can load with the additional DB C routines.
 */
const char
	*progname = "dbtest";				/* Program name. */

/*
 * XXX
 * This is necessary to use Sun shared libraries with Tcl.
 */
#ifndef _WIN32
extern int matherr();
int *tclDummyMathPtr = (int *)matherr;
#endif

#ifdef _WIN32
extern void setup_terminate();
#endif

/*
 * Global debugging variables.
 */
int debug_on, debug_print, debug_test;

/*
 * Tcl_AppInit --
 *	General initialization.
 */
int
Tcl_AppInit(interp)
	Tcl_Interp *interp;
{
	/* Initialize the interpreter. */
	if (Tcl_Init(interp) == TCL_ERROR)
		return (TCL_ERROR);

	/* Create the commands. */
	Tcl_CreateCommand(interp, "args", args_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "db_version", dbversion_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "dbenv", dbenv_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "dbopen", dbopen_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "debug_check", debugcheck_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "lock_open", lockmgr_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "lock_unlink", lockunlink_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "log", log_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "log_unlink", logunlink_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "memp", memp_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "memp_unlink", mempunlink_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "mutex_init", mutex_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "mutex_unlink", mutexunlink_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "rand", rand_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "random_int", randomint_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "srand", srand_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "timestamp", stamp_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "txn", txnmgr_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "txn_unlink", txnunlink_cmd, NULL, NULL);

	/* Historic dbm functions */
	Tcl_CreateCommand(interp, "dbminit", dbminit_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "fetch", dbm_fetch_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "store", dbm_store_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "delete", dbm_delete_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "firstkey", dbm_first_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "nextkey", dbm_next_cmd, NULL, NULL);

	/* Historic ndbm functions */
	Tcl_CreateCommand(interp, "ndbm_open", ndbmopen_cmd, NULL, NULL);

	/* Historic hsearch functions */
	Tcl_CreateCommand(interp, "hcreate", hcreate_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "hdestroy", hdestroy_cmd, NULL, NULL);
	Tcl_CreateCommand(interp, "hsearch", hsearch_cmd, NULL, NULL);

	/* Create shared global variables. */
	Tcl_LinkVar(interp, "debug_on", (char *)&debug_on, TCL_LINK_INT);
	Tcl_LinkVar(interp, "debug_print", (char *)&debug_print, TCL_LINK_INT);
	Tcl_LinkVar(interp, "debug_test", (char *)&debug_test, TCL_LINK_INT);

	/* Initialize shared global variables. */
	debug_on = debug_print = debug_test = 0;

	/* Specify the user startup file. */
#if ((TCL_MAJOR_VERSION > 7) || \
    ((TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION > 4)))
	Tcl_SetVar(interp, "tcl_rcFileName", ".dbtestrc", TCL_GLOBAL_ONLY);
#else
	tcl_RcFileName = ".dbtestrc";
#endif
	return (TCL_OK);
}

#if ((TCL_MAJOR_VERSION > 7) || \
    ((TCL_MAJOR_VERSION == 7) && (TCL_MINOR_VERSION >= 4)))
int
#ifdef _WIN32
// Must let C++ have main in order to trap errors.
main2(argc, argv)
#else
main(argc, argv)
#endif
	int argc;
	char *argv[];
{
	Tcl_Main(argc, argv, Tcl_AppInit);

	/* NOTREACHED */
	return (0);
}
#endif
