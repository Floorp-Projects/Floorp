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

/* @(#)s_ldexp.c 1.3 95/01/18 */
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

#include "fdlibm.h"
#include <errno.h>

#ifdef __STDC__
	double fd_ldexp(double value, int exp)
#else
	double fd_ldexp(value, exp)
	double value; int exp;
#endif
{
	if(!fd_finite(value)||value==0.0) return value;
	value = fd_scalbn(value,exp);
	if(!fd_finite(value)||value==0.0) errno = ERANGE;
	return value;
}
