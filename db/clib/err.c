/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)err.c	10.4 (Sleepycat) 4/10/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#endif

#include "db.h"
#include "clib_ext.h"

extern char *progname;

/*
 * err, warn --
 *	Error routines.
 *
 * PUBLIC: #ifdef __STDC__
 * PUBLIC: void err __P((int eval, const char *, ...));
 * PUBLIC: #else
 * PUBLIC: void err();
 * PUBLIC: #endif
 */
void
#ifdef __STDC__
err(int eval, const char *fmt, ...)
#else
err(eval, fmt, va_alist)
	int eval;
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;
	int sverrno;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif

	sverrno = errno;
	(void)fprintf(stderr, "%s: ", progname);
	if (fmt != NULL) {
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": ");
	}
	(void)fprintf(stderr, "%s\n", strerror(sverrno));

	exit(eval);
}

/*
 * PUBLIC: #ifdef __STDC__
 * PUBLIC: void errx __P((int eval, const char *, ...));
 * PUBLIC: #else
 * PUBLIC: void errx();
 * PUBLIC: #endif
 */
void
#ifdef __STDC__
errx(int eval, const char *fmt, ...)
#else
errx(eval, fmt, va_alist)
	int eval;
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif

	(void)fprintf(stderr, "%s: ", progname);
	if (fmt != NULL)
		(void)vfprintf(stderr, fmt, ap);
	(void)fprintf(stderr, "\n");

	exit(eval);
}

/*
 * PUBLIC: #ifdef __STDC__
 * PUBLIC: void warn __P((const char *, ...));
 * PUBLIC: #else
 * PUBLIC: void warn();
 * PUBLIC: #endif
 */
void
#ifdef __STDC__
warn(const char *fmt, ...)
#else
warn(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;
	int sverrno;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	sverrno = errno;

	(void)fprintf(stderr, "%s: ", progname);
	if (fmt != NULL) {
		(void)vfprintf(stderr, fmt, ap);
		(void)fprintf(stderr, ": ");
	}
	(void)fprintf(stderr, "%s\n", strerror(sverrno));
	va_end(ap);
}

/*
 * PUBLIC: #ifdef __STDC__
 * PUBLIC: void warnx __P((const char *, ...));
 * PUBLIC: #else
 * PUBLIC: void warnx();
 * PUBLIC: #endif
 */
void
#ifdef __STDC__
warnx(const char *fmt, ...)
#else
warnx(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif

	(void)fprintf(stderr, "%s: ", progname);
	if (fmt != NULL)
		(void)vfprintf(stderr, fmt, ap);
	(void)fprintf(stderr, "\n");
	va_end(ap);
}
