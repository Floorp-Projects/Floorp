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

/* @(#)e_scalb.c 1.3 95/01/18 */
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
 * __ieee754_scalb(x, fn) is provide for
 * passing various standard test suite. One 
 * should use scalbn() instead.
 */

#include "fdlibm.h"

#ifdef _SCALB_INT
#ifdef __STDC__
	double __ieee754_scalb(double x, int fn)
#else
	double __ieee754_scalb(x,fn)
	double x; int fn;
#endif
#else
#ifdef __STDC__
	double __ieee754_scalb(double x, double fn)
#else
	double __ieee754_scalb(x,fn)
	double x, fn;
#endif
#endif
{
#ifdef _SCALB_INT
	return fd_scalbn(x,fn);
#else
	if (fd_isnan(x)||fd_isnan(fn)) return x*fn;
	if (!fd_finite(fn)) {
	    if(fn>0.0) return x*fn;
	    else       return x/(-fn);
	}
	if (fd_rint(fn)!=fn) return (fn-fn)/(fn-fn);
	if ( fn > 65000.0) return fd_scalbn(x, 65000);
	if (-fn > 65000.0) return fd_scalbn(x,-65000);
	return fd_scalbn(x,(int)fn);
#endif
}
