/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1996, 1997, 1998
 *	Sleepycat Software.  All rights reserved.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)lock_conflict.c	10.3 (Sleepycat) 4/10/98";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>
#endif

#include "db_int.h"

/*
 * The conflict arrays are set up such that the row is the lock you
 * are holding and the column is the lock that is desired.
 */
const u_int8_t db_rw_conflicts[] = {
	/*		N   R   W */
	/*   N */	0,  0,  0,
	/*   R */	0,  0,  1,
	/*   W */	0,  1,  1
};

const u_int8_t db_riw_conflicts[] = {
	/*		N   	S   	X  	IS  	IX	SIX */
	/*   N */	0,	0,	0,	0,	0,	0,
	/*   S */	0,	0,	1,	0,	1,	1,
	/*   X */	1,	1,	1,	1,	1,	1,
	/*  IS */	0,	0,	1,	0,	0,	0,
	/*  IX */	0,	1,	1,	0,	0,	0,
	/* SIX */	0,	1,	1,	0,	0,	0
};
