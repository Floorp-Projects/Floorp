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

/* @(#)s_sin.c 1.3 95/01/18 */
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

/* sin(x)
 * Return sine function of x.
 *
 * kernel function:
 *	__kernel_sin		... sine function on [-pi/4,pi/4]
 *	__kernel_cos		... cose function on [-pi/4,pi/4]
 *	__ieee754_rem_pio2	... argument reduction routine
 *
 * Method.
 *      Let S,C and T denote the sin, cos and tan respectively on 
 *	[-PI/4, +PI/4]. Reduce the argument x to y1+y2 = x-k*pi/2 
 *	in [-pi/4 , +pi/4], and let n = k mod 4.
 *	We have
 *
 *          n        sin(x)      cos(x)        tan(x)
 *     ----------------------------------------------------------
 *	    0	       S	   C		 T
 *	    1	       C	  -S		-1/T
 *	    2	      -S	  -C		 T
 *	    3	      -C	   S		-1/T
 *     ----------------------------------------------------------
 *
 * Special cases:
 *      Let trig be any of sin, cos, or tan.
 *      trig(+-INF)  is NaN, with signals;
 *      trig(NaN)    is that NaN;
 *
 * Accuracy:
 *	TRIG(x) returns trig(x) nearly rounded 
 */

#include "fdlibm.h"

#ifdef __STDC__
	double fd_sin(double x)
#else
	double fd_sin(x)
	double x;
#endif
{
	double y[2],z=0.0;
	int n, ix;

    /* High word of x. */
	ix = __HI(x);

    /* |x| ~< pi/4 */
	ix &= 0x7fffffff;
	if(ix <= 0x3fe921fb) return __kernel_sin(x,z,0);

    /* sin(Inf or NaN) is NaN */
	else if (ix>=0x7ff00000) return x-x;

    /* argument reduction needed */
	else {
	    n = __ieee754_rem_pio2(x,y);
	    switch(n&3) {
		case 0: return  __kernel_sin(y[0],y[1],1);
		case 1: return  __kernel_cos(y[0],y[1]);
		case 2: return -__kernel_sin(y[0],y[1],1);
		default:
			return -__kernel_cos(y[0],y[1]);
	    }
	}
}
