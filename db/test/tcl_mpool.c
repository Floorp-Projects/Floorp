/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)tcl_mpool.c	10.15 (Sleepycat) 5/4/98";
#endif /* not lint */

/*
 * This file is divided up into 5 sets of functions:
 * 1. The memp command and its support functions.
 * 2. The memp_unlink command.
 * 3. The memp widget commands.
 * 4. The mool file commands.
 * 5. The memp support functions (e.g. get, put, sync)
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

typedef struct _mp_data {
	DB_MPOOL *mp;
	DB_ENV *env;
} mp_data;

typedef struct _mpfinfo {
	DB_MPOOLFILE *mpf;
	size_t pgsize;
} mpfinfo;

typedef struct _pginfo {
	void *addr;
	db_pgno_t pgno;
	size_t pgsize;
	DB_MPOOLFILE *mpf;
} pginfo;

/*
 * memp_cmd --
 *	Implements memp_open for dbtest.  Mpool_open creates an memp
 * and creates an memp widget and command.
 */

#define MPOOL_USAGE "memp path mode flags options [options]\n\toptions:\n"

int
memp_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	static int mp_number = 0;
	DB_ENV *env;
	DB_MPOOL *mp;
	mp_data *md;
	u_int32_t flags;
	int mode, tclint;
	char mpname[50];

	notused = NULL;
	debug_check();

	/* Check number of arguments. */
	USAGE_GE(argc, 4, MPOOL_USAGE, DO_ENV);

	if (Tcl_GetInt(interp, argv[2], &mode) != TCL_OK)
		return (TCL_ERROR);
	if (Tcl_GetInt(interp, argv[3], &tclint) != TCL_OK)
		return (TCL_ERROR);
	flags = (u_int32_t)tclint;

	/* Call memp_open. */
	if (process_env_options(interp, argc, argv, &env) != TCL_OK) {
		/*
		 * Special case an EINVAL return which we want to pass
		 * back to the caller.
		 */
		if (strcmp(interp->result, "EINVAL") != 0)
			Tcl_SetResult(interp, "NULL", TCL_STATIC);
		return (TCL_OK);
	}
	if (F_ISSET(env, DB_ENV_STANDALONE))
		mp = env->mp_info;
	else if (memp_open(argv[1], flags, mode, env, &mp) != 0) {
		Tcl_SetResult(interp, "NULL", TCL_STATIC);
		return (TCL_OK);
	} else
		env->mp_info = mp;

	md = (mp_data *)malloc(sizeof(mp_data));
	if (md == NULL) {
		if (!F_ISSET(env, DB_ENV_STANDALONE)) {
			(void)db_appexit(env);
			if (env->lk_conflicts)
				free(env->lk_conflicts);
			free(env);
		}
		Tcl_SetResult(interp, "mp_open: ", TCL_STATIC);
		errno = ENOMEM;
		Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
		return (TCL_ERROR);
	}
	md->mp = mp;
	md->env = env;
	/* Create new command name. */
	sprintf(&mpname[0], "mp%d", mp_number);
	mp_number++;

	/* Create widget command. */
	Tcl_CreateCommand(interp, mpname, mpwidget_cmd, (int *)md, NULL);
	Tcl_SetResult(interp, mpname, TCL_VOLATILE);
	return (TCL_OK);
}

/*
 * mempunlink_cmd --
 *	Implements memp_unlink for dbtest.
 */

#define MPOOLUNLINK_USAGE "memp_unlink path force"

int
mempunlink_cmd(notused, interp, argc, argv)
	ClientData notused;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	u_int32_t flags;
	int ret, tclint;

	notused = NULL;
	debug_check();

	USAGE_GE(argc, 3, MPOOLUNLINK_USAGE, 0);

	if (Tcl_GetInt(interp, argv[2], &tclint) != TCL_OK)
		return (TCL_ERROR);
	flags = (u_int32_t)tclint;

	if ((ret = memp_unlink(argv[1], flags, NULL)) != 0) {
		Tcl_SetResult(interp, "memp_unlink: ", TCL_STATIC);
		errno = ret;
		Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
	} else
		Tcl_SetResult(interp, "0", TCL_STATIC);
	return (TCL_OK);
}

/*
 * mpwidget --
 * This is that command that implements the memp widget.  If we
 * ever add new "methods" we add new widget commands here.
 */
#define MPWIDGET_USAGE "mpN option ?arg arg ...?"
#define MPCLOSE_USAGE "mpN close"
#define MPFOPEN_USAGE "mpN open path pagesize flags mode options"
#define MPSTAT_USAGE "mpN stat"

int
mpwidget_cmd(cd_mp, interp, argc, argv)
	ClientData cd_mp;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	static int mpf_id = 0;
	DB_ENV *env;
	DB_MPOOL *mp;
	DB_MPOOLFILE *mpf;
	mpfinfo *mfi;
	size_t pagesize;
	u_int32_t flags;
	int mode, ret, tclint;
	char mpfname[128];

	debug_check();

	mp = ((mp_data *)cd_mp)->mp;

	USAGE_GE(argc, 2, MPWIDGET_USAGE, 0);

	if (strcmp(argv[1], "close") == 0) {
		USAGE(argc, 2, MPCLOSE_USAGE, 0);
		env = ((mp_data *)cd_mp)->env;
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
	} else if (strcmp(argv[1], "open") == 0) {
		USAGE(argc, 6, MPFOPEN_USAGE, 0);

		if (Tcl_GetInt(interp, argv[3], &tclint) != TCL_OK)
			return (TCL_ERROR);
		pagesize = (size_t)tclint;
		if (Tcl_GetInt(interp, argv[4], &tclint) != TCL_OK)
			return (TCL_ERROR);
		flags = (u_int32_t)tclint;
		if (Tcl_GetInt(interp, argv[5], &mode) != TCL_OK)
			return (TCL_ERROR);

		if ((ret = memp_fopen(mp,
		    argv[2], flags, mode, pagesize, NULL, &mpf)) != 0) {
			errno = ret;
			Tcl_SetResult(interp,
			    Tcl_PosixError(interp), TCL_STATIC);
		}

		sprintf(&mpfname[0], "%s.mpf%d", argv[0], mpf_id);
		if ((mfi = (mpfinfo *)malloc(sizeof(mpfinfo))) == NULL) {
			Tcl_SetResult(interp, "mp open: ", TCL_STATIC);
			errno = ENOMEM;
			Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
			return (TCL_ERROR);
		}
		mfi->mpf = mpf;
		mfi->pgsize = pagesize;

		mpf_id++;
		Tcl_CreateCommand(interp, mpfname, mpf_cmd, (int *)mfi, NULL);
		Tcl_SetResult(interp, mpfname, TCL_VOLATILE);
		return (TCL_OK);
	} else if (strcmp(argv[1], "stat") == 0) {
		/*
		 * XXX
		 * THIS DOESN'T CURRENTLY WORK.
		memp_stat(mp, stdout);
		 */
		return (TCL_OK);
	} else {
		Tcl_SetResult(interp, MPWIDGET_USAGE, TCL_STATIC);
		return (TCL_ERROR);
	}
}

#define MPF_USAGE "mpN.mpfM cmd ?arg arg ...?"
#define MPFCLOSE_USAGE "mpN.mpfM close"
#define MPFGET_USAGE "mpN.mpfM get pgno flags"
#define MPFSYNC_USAGE "mpN.mpfM sync"
int
mpf_cmd(cd_mfi, interp, argc, argv)
	ClientData cd_mfi;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	static int pg_id = 0;
	DB_MPOOLFILE *mpf;
	db_pgno_t pgno;
	pginfo *pinfo;
	void *paddr;
	u_int32_t flags;
	int ret, tclint;
	char pgname[128];

	debug_check();

	USAGE_GE(argc, 2, MPF_USAGE, 0);

	mpf = ((mpfinfo *)cd_mfi)->mpf;
	if (strcmp(argv[1], "close") == 0) {
		USAGE(argc, 2, MPFCLOSE_USAGE, 0);
		(void)free(cd_mfi);
		ret = memp_fclose(mpf);
		if (ret == 0)
			ret = Tcl_DeleteCommand(interp, argv[0]);
		if (ret)
			Tcl_SetResult(interp, "-1", TCL_STATIC);
		else
			Tcl_SetResult(interp, "0", TCL_STATIC);
		return (ret);
	} else if (strcmp(argv[1], "get") == 0) {
		USAGE(argc, 4, MPFGET_USAGE, 0);
		if (Tcl_GetInt(interp, argv[2], (int *)&pgno) != TCL_OK)
			return (TCL_ERROR);
		if (Tcl_GetInt(interp, argv[3], &tclint) != TCL_OK)
			return (TCL_ERROR);
		flags = (u_int32_t)tclint;
		ret = memp_fget(mpf, &pgno, flags, &paddr);
		if (ret != 0 ||
		    (pinfo = (pginfo *)malloc(sizeof(pginfo))) == NULL) {
			Tcl_SetResult(interp, "mpf_cmd: ", TCL_STATIC);
			if (ret != 0)
				errno = ret;
			Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
			return (TCL_ERROR);
		}

		/* Now create page widget. */
		pinfo->addr = paddr;
		pinfo->pgno = pgno;
		pinfo->pgsize = ((mpfinfo *)cd_mfi)->pgsize;
		pinfo->mpf = ((mpfinfo *)cd_mfi)->mpf;
		sprintf(&pgname[0], "page%d", pg_id);
		pg_id++;
		Tcl_CreateCommand(interp, pgname, pgwidget_cmd, (int *)pinfo,
		    NULL);
		Tcl_SetResult(interp, &pgname[0], TCL_VOLATILE);
		return (TCL_OK);

	} else if (strcmp(argv[1], "sync") == 0) {
		USAGE(argc, 2, MPFSYNC_USAGE, 0);
		if ((ret = memp_fsync(mpf)) == 0)
			return (TCL_OK);
		else {
			Tcl_SetResult(interp, "mpf sync: ", TCL_STATIC);
			errno = ret;
			Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
			return (TCL_ERROR);
		}
	} else {
		Tcl_SetResult(interp, MPF_USAGE, TCL_STATIC);
		return (TCL_ERROR);
	}
}

/*
 * pagewidget --
 * This is that command that implements the memp page widget.   This
 * is returned from the get function of an mpf.
 */
#define PAGE_USAGE "pageN option ?arg arg ...?"
#define PAGEPUT_USAGE "pageN put flags"
#define PAGEINIT_USAGE "pageN init str"
#define PAGECHECK_USAGE "pageN check str"
#define PAGEGET_USAGE "pageN get"

int
pgwidget_cmd(cd_page, interp, argc, argv)
	ClientData cd_page;
	Tcl_Interp *interp;
	int argc;
	char *argv[];
{
	pginfo *pinfo;
	size_t i, len;
	u_int32_t flags;
	int *ip, ret, tclint;
	char *p, intbuf[50];

	debug_check();

	pinfo = (pginfo *)cd_page;
	if (strcmp(argv[1], "put") == 0) {
		USAGE(argc, 3, PAGEPUT_USAGE, 0);
		if (Tcl_GetInt(interp, argv[2], &tclint) != TCL_OK)
			return (TCL_ERROR);
		flags = (u_int32_t)tclint;
		if ((ret = memp_fput(pinfo->mpf, pinfo->addr, flags)) != 0) {
			Tcl_SetResult(interp, "page put: ", TCL_STATIC);
			errno = ret;
			Tcl_AppendResult(interp, Tcl_PosixError(interp), 0);
			return (TCL_ERROR);
		}
		free (cd_page);
		Tcl_DeleteCommand(interp, argv[0]);
		Tcl_SetResult(interp, "0", TCL_STATIC);
		return (TCL_OK);
	} else if (strcmp(argv[1], "init") == 0) {
		USAGE(argc, 3, PAGEINIT_USAGE, 0);
		len = strlen(argv[2]);
		for (p = (char *)pinfo->addr,
		    i = 0; i < pinfo->pgsize; i += len, p += len) {
			if (i + len > pinfo->pgsize)
				len = pinfo->pgsize - i;
			memcpy(p, argv[2], len);
		}
		Tcl_SetResult(interp, "0", TCL_STATIC);
		return (TCL_OK);
	} else if (strcmp(argv[1], "check") == 0) {
		USAGE(argc, 3, PAGECHECK_USAGE, 0);
		len = strlen(argv[2]);

		/* Special case, 0'd pages. */
		ret = 0;
		if (strcmp(argv[2], "nul") == 0) {
			for (ip = (int *)pinfo->addr,
			    i = 0; i < pinfo->pgsize; i += sizeof(int), ip++)
				if ((ret = (*ip != 0)) != 0)
					break;
		} else {
			for (p = (char *)pinfo->addr,
			    i = 0; i < pinfo->pgsize; i += len, p += len) {
				if (i + len > pinfo->pgsize)
					len = pinfo->pgsize - i;
				if ((ret = memcmp(p, argv[2], len)) != 0)
					break;
			}
		}

		if (ret)	/* MISMATCH */
			Tcl_SetResult(interp, "1", TCL_STATIC);
		else
			Tcl_SetResult(interp, "0", TCL_STATIC);
		return (TCL_OK);
	} else if (strcmp(argv[1], "get") == 0) {
		USAGE(argc, 2, PAGEGET_USAGE, 0);
		sprintf(intbuf, "%lu", (unsigned long)pinfo->pgno);
		Tcl_SetResult(interp, intbuf, TCL_VOLATILE);
		return (TCL_OK);
	} else {
		Tcl_SetResult(interp, PAGE_USAGE, TCL_STATIC);
		return (TCL_ERROR);
	}
}
