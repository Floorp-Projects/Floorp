/* -*- Mode: C; tab-width: 8 -*-
 * Copyright © 1996, 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

#ifndef prdtoa_h___
#define prdtoa_h___
/*
 * Public interface to portable double-precision floating point to string
 * and back conversion package.
 */

#include "jscompat.h"

PR_BEGIN_EXTERN_C

/*
 * PR_strtod() returns as a double-precision floating-point number
 * the  value represented by the character string pointed to by
 * s00.  The string is scanned up to  the  first  unrecognized
 * character.
 * If the value of se is not (char **)NULL,  a  pointer  to
 * the  character terminating the scan is returned in the location pointed
 * to by se.  If no number can be  formed, se is set to s00r, and
 * zero is returned.
 */
extern PR_PUBLIC_API(double)
PR_strtod(const char *s00, char **se);

/*
 * PR_cnvtf()
 * conversion routines for floating point
 * prcsn - number of digits of precision to generate floating
 * point value.
 */
extern PR_PUBLIC_API(void)
PR_cnvtf(char *buf, intN bufsz, intN prcsn, double dval);

PR_END_EXTERN_C

#endif /* prdtoa_h___ */
