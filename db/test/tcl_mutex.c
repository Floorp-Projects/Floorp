/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)tcl_mutex.c	10.16 (Sleepycat) 5/4/98";
#endif /* not lint */

/*
 * This file is divided up into 4 sets of functions:
 * 1. The mutex command.
 * 2. The mutex widget commands.
 */
#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif
#include <tcl.h>

#include "db_int.h"
#include "dbtest.h"
#include "common_ext.h"
#include "test_ext.h"


typedef struct _mutex_entry {
	union {
		struct {
			db_mutex_t	real_m;
			u_int32_t	real_val;
		} r;
		/*
		 * This is here to make sure that each of the mutex structures
		 * are 16-byte aligned, which is required on HP architectures.
		 * The db_mutex_t structure might be >32 bytes itself, or the
		 * real_val might push it over the 32 byte boundary.  The best
		 * we can do is use a 48 byte boundary.
		 */
		char c[48];
	} u;
} mutex_entry;
#define	m	u.r.real_m
#define	val	u.r.real_val

typedef struct _mutex_region {
	RLAYOUT		hdr;
	u_int32_t	n_mutex;
} mu_region;

typedef struct _mutex_data {
	DB_ENV		*env;
	REGINFO		 reginfo;
	mutex_entry	*marray;
	mu_region	*region;
	size_t		 size;
} mutex_data;

/*
 * mutex_cmd --
 *	Implements mutex_open for dbtest.  Mutex_open optionally creates
 * a file large enough to hold all the mutexes and then maps it in to
 * the process' address space.
 */

#define MUTEX_USAGE "mutex_init path nitems flags mode"
#define MUTEX_FILE "__mutex.share"
int
mutex_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	static int mut_number = 0;
	DB_ENV *env;
	mu_region *region;
	mutex_data *md;
	mutex_entry *marray;
	u_int32_t flags;
	int i, mode, nitems, tclint;
	char mutname[50];

	notused = NULL;
	debug_check();

	/* Check number of arguments. */
	USAGE_GE(argc, 5, MUTEX_USAGE, 0);
	if (Tcl_GetInt(interp, argv[2], &nitems) != TCL_OK)
		return (TCL_ERROR);
	if (Tcl_GetInt(interp, argv[3], &tclint) != TCL_OK)
		return (TCL_ERROR);
	flags = (u_int32_t)tclint;
	if (Tcl_GetInt(interp, argv[4], &mode) != TCL_OK)
		return (TCL_ERROR);

	if ((md = (mutex_data *)malloc(sizeof(mutex_data))) == NULL)
		goto posixout;

	md->size = ALIGN(sizeof(mu_region), 32) + sizeof(mutex_entry) * nitems;

	/* Create a file of the appropriate size. */
	if (process_env_options(interp, argc, argv, &env))
		goto errout;

	/* Map in the region. */
	memset(&md->reginfo, 0, sizeof(md->reginfo));
	md->reginfo.dbenv = env;
	md->reginfo.appname = DB_APP_NONE;
	md->reginfo.path = argv[1];
	md->reginfo.file = MUTEX_FILE;
	md->reginfo.mode = mode;
	md->reginfo.size = md->size;
	md->reginfo.dbflags = flags;
	md->reginfo.flags = 0;
	if ((errno = __db_rattach(&md->reginfo)) != 0)
		goto posixout;
	md->region = region = md->reginfo.addr;

	/* Initialize a created region. */
	if (F_ISSET(&md->reginfo, REGION_CREATED)) {
		region->n_mutex = nitems;
		marray = (mutex_entry *)((u_int8_t *)region +
		    ALIGN(sizeof(mu_region), 32));
		for (i = 0; i < nitems; i++) {
			marray[i].val = 0;
			__db_mutex_init(&marray[i].m, i);
		}
	}

	md->marray =
	    (mutex_entry *)((u_int8_t *)region + ALIGN(sizeof(mu_region), 32));
	md->env = env;

	(void)__db_mutex_unlock(&region->hdr.lock, md->reginfo.fd);

	/* Create new command name. */
	sprintf(&mutname[0], "mutex%d", mut_number);
	mut_number++;

	/* Create widget command. */
	Tcl_CreateCommand(interp, mutname, mutexwidget_cmd, (int *)md, NULL);
	Tcl_SetResult(interp, mutname, TCL_VOLATILE);
	return (TCL_OK);

posixout:
	Tcl_PosixError(interp);
errout:
	if (md != NULL) {
		if (md->reginfo.addr != NULL)
			(void)__db_rdetach(&md->reginfo);
		free(md);
	}
	if (env != NULL)
		free(env);
	Tcl_SetResult(interp, "NULL", TCL_STATIC);
	return (TCL_OK);
}

#define MUTEXUNLINK_USAGE "mutex_unlink path"
int
mutexunlink_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	DB_ENV *env;
	REGINFO reginfo;

	notused = NULL;
	debug_check();

	USAGE(argc, 3, MUTEXUNLINK_USAGE, 0);

	if (process_env_options(interp, argc, argv, &env)) {
		Tcl_SetResult(interp, "mutex_unlink", TCL_STATIC);
		Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
		return (TCL_ERROR);
	}

	memset(&reginfo, 0, sizeof(reginfo));
	reginfo.dbenv = env;
	reginfo.appname = DB_APP_NONE;
	reginfo.path = argv[1];
	reginfo.file = MUTEX_FILE;
	if (__db_runlink(&reginfo, 1) == 0)
		Tcl_SetResult(interp, "0", TCL_STATIC);
	else
		Tcl_SetResult(interp, "-1", TCL_STATIC);
	return (TCL_OK);
}

/*
 * mutexwidget --
 * This is that command that implements the mutex widget.  If we
 * ever add new "methods" we add new widget commands here.
 */
#define MUTEXWIDGET_USAGE "mutexN option ?arg arg ...?"
#define MUTEXGR_USAGE "mutexN {get,release} id"
#define MUTEXV_USAGE "mutexN {set,get}val id val"
#define MUTEXCLOSE_USAGE "mutexN close"

int
mutexwidget_cmd(cd_md, interp, argc, argv)
	ClientData cd_md;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	mutex_data *md;
	u_int32_t id;
	int ret, tclint;
	char intbuf[50];

	debug_check();

	md = (mutex_data *)cd_md;

	USAGE_GE(argc, 2, MUTEXWIDGET_USAGE, 0);

	if (strcmp(argv[1], "close") == 0) {
		USAGE(argc, 2, MUTEXCLOSE_USAGE, 0);
		(void)__db_rdetach(&md->reginfo);
		Tcl_DeleteCommand(interp, argv[0]);
		Tcl_SetResult(interp, "0", TCL_STATIC);
		return (TCL_OK);
	}
	USAGE_GE(argc, 3, MUTEXGR_USAGE, 0);

	if (Tcl_GetInt(interp, argv[2], &tclint) != TCL_OK) {
		Tcl_PosixError(interp);
		return (TCL_ERROR);
	}
	id = (u_int32_t)tclint;
	if (id >= md->region->n_mutex) {
		Tcl_SetResult(interp, "Invalid mutex id", TCL_STATIC);
		sprintf(intbuf, "%d", id);
		Tcl_AppendResult(interp, intbuf, 0);
		return (TCL_ERROR);
	}

	ret = 0;
	if (strcmp(argv[1], "get") == 0)
		ret = __db_mutex_lock(&md->marray[id].m, md->reginfo.fd);
	else if (strcmp(argv[1], "release") == 0)
		ret = __db_mutex_unlock(&md->marray[id].m, md->reginfo.fd);
	else if (strcmp(argv[1], "getval") == 0) {
		sprintf(intbuf, "%d", md->marray[id].val);
		Tcl_SetResult(interp, intbuf, TCL_VOLATILE);
		return (TCL_OK);
	} else if (strcmp(argv[1], "setval") == 0) {
		USAGE(argc, 4, MUTEXV_USAGE, 0);
		if (Tcl_GetInt(interp, argv[3], &tclint) != TCL_OK) {
			Tcl_PosixError(interp);
			return (TCL_ERROR);
		}
		md->marray[id].val = (u_int32_t)tclint;
	} else {
		Tcl_SetResult(interp, MUTEXWIDGET_USAGE, TCL_STATIC);
		return (TCL_ERROR);
	}

	if (ret)
		Tcl_SetResult(interp, "-1", TCL_STATIC);
	else
		Tcl_SetResult(interp, "0", TCL_STATIC);
	return (TCL_OK);
}
