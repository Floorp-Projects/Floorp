/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)getlong.c	10.3 (Sleepycat) 4/10/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#endif

#include "db.h"
#include "clib_ext.h"

/*
 * get_long --
 *	Return a long value inside of basic parameters.
 *
 * PUBLIC: void get_long __P((char *, long, long, long *));
 */
void
get_long(p, min, max, storep)
	char *p;
	long min, max, *storep;
{
	long val;
	char *end;

	errno = 0;
	val = strtol(p, &end, 10);
	if ((val == LONG_MIN || val == LONG_MAX) && errno == ERANGE)
		err(1, "%s", p);
	if (p[0] == '\0' || end[0] != '\0')
		errx(1, "%s: Invalid numeric argument", p);
	if (val < min)
		errx(1, "%s: Less than minimum value (%ld)", p, min);
	if (val > max)
		errx(1, "%s: Greater than maximum value (%ld)", p, max);
	*storep = val;
}
