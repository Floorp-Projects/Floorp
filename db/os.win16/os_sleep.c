/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1998
 *	Sleepycat Software.  All rights reserved.
 *
 * This code is derived from software contributed to Sleepycat Software by
 * Frederick G.M. Roeber of Netscape Communications Corp.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_sleep.c	10.3 (Sleepycat) 4/27/98";
#endif /* not lint */

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
	/* Don't require that the values be normalized. */
	for (; usecs >= 1000000; ++secs, usecs -= 1000000)
		;

	/* unsigned *long* number of seconds?  Okaaay... */
	for( ; secs >= 65535; secs -= 65535)
		sleep((u_int)65535);		/* About 18 hours. */

	sleep(secs);
	delay(usecs/1000);
	return (0);
}
