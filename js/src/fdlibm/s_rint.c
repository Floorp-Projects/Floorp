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

/* @(#)s_rint.c 1.3 95/01/18 */
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
 * rint(x)
 * Return x rounded to integral value according to the prevailing
 * rounding mode.
 * Method:
 *	Using floating addition.
 * Exception:
 *	Inexact flag raised if x not equal to rint(x).
 */

#include "fdlibm.h"

#ifdef __STDC__
static const double
#else
static double 
#endif
TWO52[2]={
  4.50359962737049600000e+15, /* 0x43300000, 0x00000000 */
 -4.50359962737049600000e+15, /* 0xC3300000, 0x00000000 */
};

#ifdef __STDC__
	double fd_rint(double x)
#else
	double fd_rint(x)
	double x;
#endif
{
	int i0,j0,sx;
	unsigned i,i1;
	double w,t;
	i0 =  __HI(x);
	sx = (i0>>31)&1;
	i1 =  __LO(x);
	j0 = ((i0>>20)&0x7ff)-0x3ff;
	if(j0<20) {
	    if(j0<0) { 	
		if(((i0&0x7fffffff)|i1)==0) return x;
		i1 |= (i0&0x0fffff);
		i0 &= 0xfffe0000;
		i0 |= ((i1|-(int)i1)>>12)&0x80000;
		__HI(x)=i0;
	        w = TWO52[sx]+x;
	        t =  w-TWO52[sx];
	        i0 = __HI(t);
	        __HI(t) = (i0&0x7fffffff)|(sx<<31);
	        return t;
	    } else {
		i = (0x000fffff)>>j0;
		if(((i0&i)|i1)==0) return x; /* x is integral */
		i>>=1;
		if(((i0&i)|i1)!=0) {
		    if(j0==19) i1 = 0x40000000; else
		    i0 = (i0&(~i))|((0x20000)>>j0);
		}
	    }
	} else if (j0>51) {
	    if(j0==0x400) return x+x;	/* inf or NaN */
	    else return x;		/* x is integral */
	} else {
	    i = ((unsigned)(0xffffffff))>>(j0-20);
	    if((i1&i)==0) return x;	/* x is integral */
	    i>>=1;
	    if((i1&i)!=0) i1 = (i1&(~i))|((0x40000000)>>(j0-20));
	}
	__HI(x) = i0;
	__LO(x) = i1;
	w = TWO52[sx]+x;
	return w-TWO52[sx];
}
