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

/* @(#)s_scalbn.c 1.3 95/01/18 */
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
 * scalbn (double x, int n)
 * scalbn(x,n) returns x* 2**n  computed by  exponent  
 * manipulation rather than by actually performing an 
 * exponentiation or a multiplication.
 */

#include "fdlibm.h"

#ifdef __STDC__
static const double
#else
static double
#endif
two54   =  1.80143985094819840000e+16, /* 0x43500000, 0x00000000 */
twom54  =  5.55111512312578270212e-17, /* 0x3C900000, 0x00000000 */
huge   = 1.0e+300,
tiny   = 1.0e-300;

#ifdef __STDC__
	double fd_scalbn (double x, int n)
#else
	double fd_scalbn (x,n)
	double x; int n;
#endif
{
	int  k,hx,lx;
	hx = __HI(x);
	lx = __LO(x);
        k = (hx&0x7ff00000)>>20;		/* extract exponent */
        if (k==0) {				/* 0 or subnormal x */
            if ((lx|(hx&0x7fffffff))==0) return x; /* +-0 */
	    x *= two54; 
	    hx = __HI(x);
	    k = ((hx&0x7ff00000)>>20) - 54; 
            if (n< -50000) return tiny*x; 	/*underflow*/
	    }
        if (k==0x7ff) return x+x;		/* NaN or Inf */
        k = k+n; 
        if (k >  0x7fe) return huge*fd_copysign(huge,x); /* overflow  */
        if (k > 0) 				/* normal result */
	    {__HI(x) = (hx&0x800fffff)|(k<<20); return x;}
        if (k <= -54)
            if (n > 50000) 	/* in case integer overflow in n+k */
		return huge*fd_copysign(huge,x);	/*overflow*/
	    else return tiny*fd_copysign(tiny,x); 	/*underflow*/
        k += 54;				/* subnormal result */
        __HI(x) = (hx&0x800fffff)|(k<<20);
        return x*twom54;
}
