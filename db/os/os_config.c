/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_config.c	10.26 (Sleepycat) 5/23/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#endif

#include "db_int.h"

/*
 * XXX
 * We provide our own extern declarations so that we don't collide with
 * systems that get them wrong, e.g., SunOS.
 */
#ifdef _WIN32
#define fsync		_commit
#define imported	__declspec(dllimport)
#else
#define imported
#endif

/*
 * XXX
 * HP/UX MPE doesn't have fsync, but you can build one using FCONTROL.
 */
#ifdef __hp3000s900
#define	fsync	__mpe_fsync
#endif

imported extern int	 close __P((int));
imported extern void	 free __P((void *));
imported extern int	 fsync __P((int));
imported extern void    *malloc __P((size_t));
imported extern int	 open __P((const char *, int, ...));
imported extern ssize_t	 read __P((int, void *, size_t));
imported extern void    *realloc __P((void *, size_t));
imported extern int	 unlink __P((const char *));
imported extern ssize_t	 write __P((int, const void *, size_t));

/*
 * __db_jump --
 *	This list of interfaces that applications can replace.  In some
 *	cases, the user is permitted to replace the standard ANSI C or
 *	POSIX 1003.1 call, e.g., malloc or read.  In others, we provide
 *	a local interface to the functionality, e.g., __os_ioinfo.
 */
struct __db_jumptab __db_jump = {
	close,				/* DB_FUNC_CLOSE */
	__os_dirfree,			/* DB_FUNC_DIRFREE */
	__os_dirlist,			/* DB_FUNC_DIRLIST */
	__os_exists,			/* DB_FUNC_EXISTS */
	free,				/* DB_FUNC_FREE */
	fsync,				/* DB_FUNC_FSYNC */
	__os_ioinfo,			/* DB_FUNC_IOINFO */
	malloc,				/* DB_FUNC_MALLOC */
	NULL,				/* DB_FUNC_MAP */
	open,				/* DB_FUNC_OPEN */
	read,				/* DB_FUNC_READ */
	realloc,			/* DB_FUNC_REALLOC */
	NULL,				/* DB_FUNC_RUNLINK */
	__os_seek,			/* DB_FUNC_SEEK */
	__os_sleep,			/* DB_FUNC_SLEEP */
	unlink,				/* DB_FUNC_UNLINK */
	NULL,				/* DB_FUNC_UNMAP */
	write,				/* DB_FUNC_WRITE */
	NULL				/* DB_FUNC_YIELD */
};

DB_GLOBALS __db_global_values = {
	1,				/* DB_MUTEXLOCKS */
	0,				/* DB_REGION_ANON, DB_REGION_NAME */
	0,				/* DB_REGION_INIT */
	0,				/* DB_TSL_SPINS */
	0				/* DB_PAGEYIELD */
};

/*
 * db_jump_set --
 *	Replace functions for the DB package.
 */
int
db_jump_set(func, which)
	void *func;
	int which;
{
	switch (which) {
	case DB_FUNC_CLOSE:
		__db_jump.j_close = (int (*) __P((int)))func;
		break;
	case DB_FUNC_DIRFREE:
		__db_jump.j_dirfree = (void (*) __P((char **, int)))func;
		break;
	case DB_FUNC_DIRLIST:
		__db_jump.j_dirlist =
		    (int (*) __P((const char *, char ***, int *)))func;
		break;
	case DB_FUNC_EXISTS:
		__db_jump.j_exists = (int (*) __P((const char *, int *)))func;
		break;
	case DB_FUNC_FREE:
		__db_jump.j_free = (void (*) __P((void *)))func;
		break;
	case DB_FUNC_FSYNC:
		__db_jump.j_fsync = (int (*) __P((int)))func;
		break;
	case DB_FUNC_IOINFO:
		__db_jump.j_ioinfo = (int (*) __P((const char *,
		    int, u_int32_t *, u_int32_t *, u_int32_t *)))func;
		break;
	case DB_FUNC_MALLOC:
		__db_jump.j_malloc = (void *(*) __P((size_t)))func;
		break;
	case DB_FUNC_MAP:
		__db_jump.j_map = (int (*)
		    __P((char *, int, size_t, int, int, int, void **)))func;
		break;
	case DB_FUNC_OPEN:
		__db_jump.j_open = (int (*) __P((const char *, int, ...)))func;
		break;
	case DB_FUNC_READ:
		__db_jump.j_read =
		    (ssize_t (*) __P((int, void *, size_t)))func;
		break;
	case DB_FUNC_REALLOC:
		__db_jump.j_realloc = (void *(*) __P((void *, size_t)))func;
		break;
	case DB_FUNC_RUNLINK:
		__db_jump.j_runlink = (int (*) __P((char *)))func;
		break;
	case DB_FUNC_SEEK:
		__db_jump.j_seek = (int (*)
		    __P((int, size_t, db_pgno_t, u_int32_t, int, int)))func;
		break;
	case DB_FUNC_SLEEP:
		__db_jump.j_sleep = (int (*) __P((u_long, u_long)))func;
		break;
	case DB_FUNC_UNLINK:
		__db_jump.j_unlink = (int (*) __P((const char *)))func;
		break;
	case DB_FUNC_UNMAP:
		__db_jump.j_unmap = (int (*) __P((void *, size_t)))func;
		break;
	case DB_FUNC_WRITE:
		__db_jump.j_write =
		    (ssize_t (*) __P((int, const void *, size_t)))func;
		break;
	case DB_FUNC_YIELD:
		__db_jump.j_yield = (int (*) __P((void)))func;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

/*
 * db_value_set --
 *	Replace values for the DB package.
 */
int
db_value_set(value, which)
	int value, which;
{
	int ret;

	switch (which) {
	case DB_MUTEXLOCKS:
		DB_GLOBAL(db_mutexlocks) = value;
		break;
	case DB_PAGEYIELD:
		DB_GLOBAL(db_pageyield) = value;
		break;
	case DB_REGION_ANON:
		if (value != 0 && (ret = __db_mapanon_ok(0)) != 0)
			return (ret);
		DB_GLOBAL(db_region_anon) = value;
		break;
	case DB_REGION_INIT:
		DB_GLOBAL(db_region_init) = value;
		break;
	case DB_REGION_NAME:
		if (value != 0 && (ret = __db_mapanon_ok(1)) != 0)
			return (ret);
		DB_GLOBAL(db_region_anon) = value;
		break;
	case DB_TSL_SPINS:
		if (value <= 0)
			return (EINVAL);
		DB_GLOBAL(db_tsl_spins) = value;
		break;
	default:
		return (EINVAL);
	}
	return (0);
}
