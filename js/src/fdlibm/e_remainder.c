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

/* @(#)e_remainder.c 1.3 95/01/18 */
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

/* __ieee754_remainder(x,p)
 * Return :                  
 * 	returns  x REM p  =  x - [x/p]*p as if in infinite 
 * 	precise arithmetic, where [x/p] is the (infinite bit) 
 *	integer nearest x/p (in half way case choose the even one).
 * Method : 
 *	Based on fmod() return x-[x/p]chopped*p exactlp.
 */

#include "fdlibm.h"

#ifdef __STDC__
static const double zero = 0.0;
#else
static double zero = 0.0;
#endif


#ifdef __STDC__
	double __ieee754_remainder(double x, double p)
#else
	double __ieee754_remainder(x,p)
	double x,p;
#endif
{
	int hx,hp;
	unsigned sx,lx,lp;
	double p_half;

	hx = __HI(x);		/* high word of x */
	lx = __LO(x);		/* low  word of x */
	hp = __HI(p);		/* high word of p */
	lp = __LO(p);		/* low  word of p */
	sx = hx&0x80000000;
	hp &= 0x7fffffff;
	hx &= 0x7fffffff;

    /* purge off exception values */
	if((hp|lp)==0) return (x*p)/(x*p); 	/* p = 0 */
	if((hx>=0x7ff00000)||			/* x not finite */
	  ((hp>=0x7ff00000)&&			/* p is NaN */
	  (((hp-0x7ff00000)|lp)!=0)))
	    return (x*p)/(x*p);


	if (hp<=0x7fdfffff) x = __ieee754_fmod(x,p+p);	/* now x < 2p */
	if (((hx-hp)|(lx-lp))==0) return zero*x;
	x  = fd_fabs(x);
	p  = fd_fabs(p);
	if (hp<0x00200000) {
	    if(x+x>p) {
		x-=p;
		if(x+x>=p) x -= p;
	    }
	} else {
	    p_half = 0.5*p;
	    if(x>p_half) {
		x-=p;
		if(x>=p_half) x -= p;
	    }
	}
	__HI(x) ^= sx;
	return x;
}
