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

/* @(#)s_nextafter.c 1.3 95/01/18 */
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

/* IEEE functions
 *	nextafter(x,y)
 *	return the next machine floating-point number of x in the
 *	direction toward y.
 *   Special cases:
 */

#include "fdlibm.h"

#ifdef __STDC__
	double fd_nextafter(double x, double y)
#else
	double fd_nextafter(x,y)
	double x,y;
#endif
{
	int	hx,hy,ix,iy;
	unsigned lx,ly;

	hx = __HI(x);		/* high word of x */
	lx = __LO(x);		/* low  word of x */
	hy = __HI(y);		/* high word of y */
	ly = __LO(y);		/* low  word of y */
	ix = hx&0x7fffffff;		/* |x| */
	iy = hy&0x7fffffff;		/* |y| */

	if(((ix>=0x7ff00000)&&((ix-0x7ff00000)|lx)!=0) ||   /* x is nan */ 
	   ((iy>=0x7ff00000)&&((iy-0x7ff00000)|ly)!=0))     /* y is nan */ 
	   return x+y;				
	if(x==y) return x;		/* x=y, return x */
	if((ix|lx)==0) {			/* x == 0 */
	    __HI(x) = hy&0x80000000;	/* return +-minsubnormal */
	    __LO(x) = 1;
	    y = x*x;
	    if(y==x) return y; else return x;	/* raise underflow flag */
	} 
	if(hx>=0) {				/* x > 0 */
	    if(hx>hy||((hx==hy)&&(lx>ly))) {	/* x > y, x -= ulp */
		if(lx==0) hx -= 1;
		lx -= 1;
	    } else {				/* x < y, x += ulp */
		lx += 1;
		if(lx==0) hx += 1;
	    }
	} else {				/* x < 0 */
	    if(hy>=0||hx>hy||((hx==hy)&&(lx>ly))){/* x < y, x -= ulp */
		if(lx==0) hx -= 1;
		lx -= 1;
	    } else {				/* x > y, x += ulp */
		lx += 1;
		if(lx==0) hx += 1;
	    }
	}
	hy = hx&0x7ff00000;
	if(hy>=0x7ff00000) return x+x;	/* overflow  */
	if(hy<0x00100000) {		/* underflow */
	    y = x*x;
	    if(y!=x) {		/* raise underflow flag */
		__HI(y) = hx; __LO(y) = lx;
		return y;
	    }
	}
	__HI(x) = hx; __LO(x) = lx;
	return x;
}
