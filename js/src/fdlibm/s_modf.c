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
        fd_twoints u;
        u.d = x;
	i0 =  __HI(u);		/* high x */
	i1 =  __LO(u);		/* low  x */
	j0 = ((i0>>20)&0x7ff)-0x3ff;	/* exponent of x */
	if(j0<20) {			/* integer part in high x */
	    if(j0<0) {			/* |x|<1 */
                u.d = *iptr;
		__HI(u) = i0&0x80000000;
		__LO(u) = 0;		/* *iptr = +-0 */
                *iptr = u.d;
		return x;
	    } else {
		i = (0x000fffff)>>j0;
		if(((i0&i)|i1)==0) {		/* x is integral */
		    *iptr = x;
                    u.d = x;
		    __HI(u) &= 0x80000000;
		    __LO(u)  = 0;	/* return +-0 */
                    x = u.d;
		    return x;
		} else {
                    u.d = *iptr;
		    __HI(u) = i0&(~i);
		    __LO(u) = 0;
                    *iptr = u.d;
		    return x - *iptr;
		}
	    }
	} else if (j0>51) {		/* no fraction part */
	    *iptr = x*one;
            u.d = x;
	    __HI(u) &= 0x80000000;
	    __LO(u)  = 0;	/* return +-0 */
            x = u.d;
	    return x;
	} else {			/* fraction part in low x */
	    i = ((unsigned)(0xffffffff))>>(j0-20);
	    if((i1&i)==0) { 		/* x is integral */
		*iptr = x;
                u.d = x;
		__HI(u) &= 0x80000000;
		__LO(u)  = 0;	/* return +-0 */
                x = u.d;
		return x;
	    } else {
                u.d = *iptr;
		__HI(u) = i0;
		__LO(u) = i1&(~i);
                *iptr = u.d;
		return x - *iptr;
	    }
	}
}
