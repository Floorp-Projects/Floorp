/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_open.c	10.26 (Sleepycat) 5/4/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "db_int.h"

/*
 * __db_open --
 *	Open a file descriptor.
 *
 * PUBLIC: int __db_open __P((const char *, u_int32_t, u_int32_t, int, int *));
 */
int
__db_open(name, arg_flags, ok_flags, mode, fdp)
	const char *name;
	u_int32_t arg_flags, ok_flags;
	int mode, *fdp;
{
	int fd, flags;

	if (arg_flags & ~ok_flags)
		return (EINVAL);

	flags = 0;

	/*
	 * DB requires the semantic that two files opened at the same time
	 * with O_CREAT and O_EXCL set will return failure in at least one.
	 */
	if (arg_flags & DB_CREATE)
		flags |= O_CREAT;

	if (arg_flags & DB_EXCL)
		flags |= O_EXCL;

	if (arg_flags & DB_RDONLY)
		flags |= O_RDONLY;
	else
		flags |= O_RDWR;

#if defined(_WIN32) || defined(WIN16)
#ifdef _MSC_VER
	if (arg_flags & DB_SEQUENTIAL)
		flags |= _O_SEQUENTIAL;
	else
		flags |= _O_RANDOM;

	if (arg_flags & DB_TEMPORARY)
		flags |= _O_TEMPORARY;
#endif
	flags |= O_BINARY | O_NOINHERIT;
#endif

	if (arg_flags & DB_TRUNCATE)
		flags |= O_TRUNC;

	/* Open the file. */
	if ((fd = __os_open(name, flags, mode)) == -1)
		return (errno);

#ifndef _WIN32
	/* Delete any temporary file; done for Win32 by _O_TEMPORARY. */
	if (arg_flags & DB_TEMPORARY)
		(void)__os_unlink(name);
#endif

#if !defined(_WIN32) && !defined(WIN16)
	/*
	 * Deny access to any child process; done for Win32 by O_NOINHERIT,
	 * MacOS has neither child processes nor fd inheritance.
	 */
	if (fcntl(fd, F_SETFD, 1) == -1) {
		int ret = errno;

		(void)__os_close(fd);
		return (ret);
	}
#endif
	*fdp = fd;
	return (0);
}

/*
 * __db_close --
 *	Close a file descriptor.
 *
 * PUBLIC: int __db_close __P((int));
 */
int
__db_close(fd)
	int fd;
{
	return (__os_close(fd) ? errno : 0);
}
