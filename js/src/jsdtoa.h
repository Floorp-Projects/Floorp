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
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef jsdtoa_h___
#define jsdtoa_h___
/*
 * Public interface to portable double-precision floating point to string
 * and back conversion package.
 */

#include "jscompat.h"

JS_BEGIN_EXTERN_C

/*
 * JS_strtod() returns as a double-precision floating-point number
 * the  value represented by the character string pointed to by
 * s00.  The string is scanned up to  the  first  unrecognized
 * character.
 * If the value of se is not (char **)NULL,  a  pointer  to
 * the  character terminating the scan is returned in the location pointed
 * to by se.  If no number can be  formed, se is set to s00r, and
 * zero is returned.
 */
JS_FRIEND_API(double)
JS_strtod(const char *s00, char **se);

/*
 * JS_cnvtf()
 * conversion routines for floating point
 * prcsn - number of digits of precision to generate floating
 * point value.
 */
JS_FRIEND_API(void)
JS_cnvtf(char *buf, size_t bufsz, int prcsn, double dval);

JS_END_EXTERN_C

#endif /* jsdtoa_h___ */
