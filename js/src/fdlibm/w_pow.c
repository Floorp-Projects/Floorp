/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */


/* @(#)w_pow.c 1.3 95/01/18 */
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
 * wrapper pow(x,y) return x**y
 */

#include "fdlibm.h"


#ifdef __STDC__
	double fd_pow(double x, double y)	/* wrapper pow */
#else
	double fd_pow(x,y)			/* wrapper pow */
	double x,y;
#endif
{
#ifdef _IEEE_LIBM
	return  __ieee754_pow(x,y);
#else
	double z;
    int err;
	z=__ieee754_pow(x,y);
	if(_LIB_VERSION == _IEEE_|| fd_isnan(y)) return z;
	if(fd_isnan(x)) {
	    if(y==0.0) 
	        return __kernel_standard(x,y,42,&err); /* pow(NaN,0.0) */
	    else 
		return z;
	}
	if(x==0.0){ 
	    if(y==0.0)
	        return __kernel_standard(x,y,20,&err); /* pow(0.0,0.0) */
	    if(fd_finite(y)&&y<0.0)
	        return __kernel_standard(x,y,23,&err); /* pow(0.0,negative) */
	    return z;
	}
	if(!fd_finite(z)) {
	    if(fd_finite(x)&&fd_finite(y)) {
	        if(fd_isnan(z))
	            return __kernel_standard(x,y,24,&err); /* pow neg**non-int */
	        else 
	            return __kernel_standard(x,y,21,&err); /* pow overflow */
	    }
	} 
	if(z==0.0&&fd_finite(x)&&fd_finite(y))
	    return __kernel_standard(x,y,22,&err); /* pow underflow */
	return z;
#endif
}
