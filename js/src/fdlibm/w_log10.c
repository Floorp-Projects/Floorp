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

/* @(#)w_log10.c 1.3 95/01/18 */
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
 * wrapper log10(X)
 */

#include "fdlibm.h"


#ifdef __STDC__
	double fd_log10(double x)		/* wrapper log10 */
#else
	double fd_log10(x)			/* wrapper log10 */
	double x;
#endif
{
#ifdef _IEEE_LIBM
	return __ieee754_log10(x);
#else
	double z;
	z = __ieee754_log10(x);
	if(_LIB_VERSION == _IEEE_ || fd_isnan(x)) return z;
	if(x<=0.0) {
	    if(x==0.0)
	        return __kernel_standard(x,x,18); /* log10(0) */
	    else 
	        return __kernel_standard(x,x,19); /* log10(x<0) */
	} else
	    return z;
#endif
}
