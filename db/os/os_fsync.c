/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_fsync.c	10.5 (Sleepycat) 4/19/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <errno.h>
#include <unistd.h>
#endif

#include "db_int.h"

/*
 * __db_fsync --
 *	Flush a file descriptor.
 *
 * PUBLIC: int __db_fsync __P((int));
 */
int
__db_fsync(fd)
	int fd;
{
	return (__os_fsync(fd) ? errno : 0);
}

#ifdef __hp3000s900
#include <fcntl.h>

int
__mpe_fsync(fd)
	int fd;
{
	extern FCONTROL(short, short, void *);

	FCONTROL(_MPE_FILENO(fd), 2, NULL);	/* Flush the buffers */
	FCONTROL(_MPE_FILENO(fd), 6, NULL);	/* Write the EOF */
	return (0);
}
#endif
