/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)vsnprintf.c	10.2 (Sleepycat) 4/10/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#endif

/*
 * vsnprintf --
 *	Bounded version of vsprintf.
 *
 * PUBLIC: #ifndef HAVE_VSNPRINTF
 * PUBLIC: int vsnprintf();
 * PUBLIC: #endif
 */
#ifndef HAVE_VSNPRINTF
int
vsnprintf(str, n, fmt, ap)
	char *str;
	size_t n;
	const char *fmt;
	va_list ap;
{
#ifdef SPRINTF_RET_CHARPNT
	(void)vsprintf(str, fmt, ap);
	return (strlen(str));
#else
	return (vsprintf(str, fmt, ap));
#endif
}
#endif
