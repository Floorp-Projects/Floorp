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

/* @(#)s_modf.c 1.3 95/01/18 */
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
 * modf(double x, double *iptr) 
 * return fraction part of x, and return x's integral part in *iptr.
 * Method:
 *	Bit twiddling.
 *
 * Exception:
 *	No exception.
 */

#include "fdlibm.h"

#ifdef __STDC__
static const double one = 1.0;
#else
static double one = 1.0;
#endif

#ifdef __STDC__
	double fd_modf(double x, double *iptr)
#else
	double fd_modf(x, iptr)
	double x,*iptr;
#endif
{
	int i0,i1,j0;
	unsigned i;
	i0 =  __HI(x);		/* high x */
	i1 =  __LO(x);		/* low  x */
	j0 = ((i0>>20)&0x7ff)-0x3ff;	/* exponent of x */
	if(j0<20) {			/* integer part in high x */
	    if(j0<0) {			/* |x|<1 */
		__HIp(iptr) = i0&0x80000000;
		__LOp(iptr) = 0;		/* *iptr = +-0 */
		return x;
	    } else {
		i = (0x000fffff)>>j0;
		if(((i0&i)|i1)==0) {		/* x is integral */
		    *iptr = x;
		    __HI(x) &= 0x80000000;
		    __LO(x)  = 0;	/* return +-0 */
		    return x;
		} else {
		    __HIp(iptr) = i0&(~i);
		    __LOp(iptr) = 0;
		    return x - *iptr;
		}
	    }
	} else if (j0>51) {		/* no fraction part */
	    *iptr = x*one;
	    __HI(x) &= 0x80000000;
	    __LO(x)  = 0;	/* return +-0 */
	    return x;
	} else {			/* fraction part in low x */
	    i = ((unsigned)(0xffffffff))>>(j0-20);
	    if((i1&i)==0) { 		/* x is integral */
		*iptr = x;
		__HI(x) &= 0x80000000;
		__LO(x)  = 0;	/* return +-0 */
		return x;
	    } else {
		__HIp(iptr) = i0;
		__LOp(iptr) = i1&(~i);
		return x - *iptr;
	    }
	}
}
