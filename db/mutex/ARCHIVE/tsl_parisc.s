/*
 * Copyright (c) 1996-1997 The University of Utah and the Computer Systems
 * Laboratory at the University of Utah (CSL).  All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the Computer
 * Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *	@(#)tsl_parisc.s	8.4 (Sleepycat) 1/18/97
 */

/*
 * Spin locks for the PA-RISC.  Based on Bob Wheeler's Mach implementation.
 */
	.SPACE	$TEXT$
	.SUBSPA $CODE$

/*
 * int tsl_set(tsl_t *tsl)
 *
 * Try to acquire a lock, return 1 if successful, 0 if not.
 */
	.EXPORT	tsl_set,ENTRY
tsl_set
	.PROC
	.CALLINFO FRAME=0,NO_CALLS
	.ENTRY
	ldo	15(%r26),%r26
	depi	0,31,4,%r26
	ldcws	0(%r26),%r28
	subi,=	0,%r28,%r0
	ldi	1,%r28
	bv,n	0(%r2)
	.EXIT
	.PROCEND


/*
 * void tsl_unset(tsl_t *tsl)
 *
 * Release a lock.
 */
	.EXPORT	tsl_unset,ENTRY
tsl_unset
	.PROC
	.CALLINFO FRAME=0,NO_CALLS
	.ENTRY
	ldo	15(%r26),%r26
	ldi	-1,%r19
	depi	0,31,4,%r26
	bv	0(%r2)
	stw	%r19,0(%r26)
	.EXIT
	.PROCEND
