/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)os_alloc.c	10.6 (Sleepycat) 5/2/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <string.h>
#endif

#include "db_int.h"

/*
 * __db_strdup --
 *	The strdup(3) function for DB.
 *
 * PUBLIC: char *__db_strdup __P((const char *));
 */
char *
__db_strdup(str)
	const char *str;
{
	size_t len;
	char *copy;

	len = strlen(str) + 1;
	if ((copy = __db_malloc(len)) == NULL)
		return (NULL);

	memcpy(copy, str, len);
	return (copy);
}

/*
 * XXX
 * Correct for systems that return NULL when you allocate 0 bytes of memory.
 * There are several places in DB where we allocate the number of bytes held
 * by the key/data item, and it can be 0.  Correct here so that malloc never
 * returns a NULL for that reason (which behavior is permitted by ANSI).  We
 * could make these calls macros on non-Alpha architectures (that's where we
 * saw the problem), but it's probably not worth the autoconf complexity.
 *
 *	Out of memory.
 *	We wish to hold the whole sky,
 *	But we never will.
 */
/*
 * __db_calloc --
 *	The calloc(3) function for DB.
 *
 * PUBLIC: void *__db_calloc __P((size_t, size_t));
 */
void *
__db_calloc(num, size)
	size_t num, size;
{
	void *p;

	size *= num;
	if ((p = __db_jump.j_malloc(size == 0 ? 1 : size)) != NULL)
		memset(p, 0, size);
	return (p);
}

/*
 * __db_malloc --
 *	The malloc(3) function for DB.
 *
 * PUBLIC: void *__db_malloc __P((size_t));
 */
void *
__db_malloc(size)
	size_t size;
{
#ifdef DIAGNOSTIC
	void *p;

	p = __db_jump.j_malloc(size == 0 ? 1 : size);
	memset(p, 0xff, size == 0 ? 1 : size);
	return (p);
#else
	return (__db_jump.j_malloc(size == 0 ? 1 : size));
#endif
}

/*
 * __db_realloc --
 *	The realloc(3) function for DB.
 *
 * PUBLIC: void *__db_realloc __P((void *, size_t));
 */
void *
__db_realloc(ptr, size)
	void *ptr;
	size_t size;
{
	return (__db_jump.j_realloc(ptr, size == 0 ? 1 : size));
}
