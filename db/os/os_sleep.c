/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_sleep.c	10.10 (Sleepycat) 4/27/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include <errno.h>
#ifndef HAVE_SYS_TIME_H
#include <time.h>
#endif
#include <unistd.h>
#endif

#include "db_int.h"

/*
 * __os_sleep --
 *	Yield the processor for a period of time.
 *
 * PUBLIC: int __os_sleep __P((u_long, u_long));
 */
int
__os_sleep(secs, usecs)
	u_long secs, usecs;		/* Seconds and microseconds. */
{
	struct timeval t;

	/* Don't require that the values be normalized. */
	for (; usecs >= 1000000; ++secs, usecs -= 1000000)
		;

	/*
	 * It's important that we yield the processor here so that other
	 * processes or threads are permitted to run.
	 */
	t.tv_sec = secs;
	t.tv_usec = usecs;
	return (select(0, NULL, NULL, NULL, &t) == -1 ? errno : 0);
}
