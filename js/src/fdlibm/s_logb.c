/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Sun Microsystems,
 * Inc.  Portions created by Netscape are Copyright (C) 1998 Netscape
 * Communications Corporation.  All Rights Reserved.
 */

/* @(#)s_logb.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/*
 * double logb(x)
 * IEEE 754 logb. Included to pass IEEE test suite. Not recommend.
 * Use ilogb instead.
 */

#include "fdlibm.h"

#ifdef __STDC__
	double fd_logb(double x)
#else
	double fd_logb(x)
	double x;
#endif
{
	int lx,ix;
	ix = (__HI(x))&0x7fffffff;	/* high |x| */
	lx = __LO(x);			/* low x */
	if((ix|lx)==0) return -1.0/fd_fabs(x);
	if(ix>=0x7ff00000) return x*x;
	if((ix>>=20)==0) 			/* IEEE 754 logb */
		return -1022.0; 
	else
		return (double) (ix-1023); 
}
