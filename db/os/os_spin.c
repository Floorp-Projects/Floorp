/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_spin.c	10.7 (Sleepycat) 5/20/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <limits.h>
#include <unistd.h>
#endif

#include "db_int.h"

/*
 * __os_spin --
 *	Return the number of default spins before blocking.
 *
 * PUBLIC: int __os_spin __P((void));
 */
int
__os_spin()
{
	static long sys_val;

	/* If the application specified the spins, use its value. */
	if (DB_GLOBAL(db_tsl_spins) != 0)
		return (DB_GLOBAL(db_tsl_spins));

	/* If we've already figured this out, return the value. */
	if (sys_val != 0)
		return (sys_val);

	/*
	 * XXX
	 * Solaris and Linux use _SC_NPROCESSORS_ONLN to return the number of
	 * online processors.  We don't want to repeatedly call sysconf because
	 * it's quite expensive (requiring multiple filesystem accesses) under
	 * Debian Linux.
	 *
	 * Spin 50 times per processor -- we have anecdotal evidence that this
	 * is a reasonable value.
	 */
#if defined(HAVE_SYSCONF) && defined(_SC_NPROCESSORS_ONLN)
	if ((sys_val = sysconf(_SC_NPROCESSORS_ONLN)) > 1)
		sys_val *= 50;
	else
		sys_val = 1;
#else
	sys_val = 1;
#endif
	return (sys_val);
}
