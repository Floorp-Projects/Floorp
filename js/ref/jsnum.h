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

#ifndef jsnum_h___
#define jsnum_h___
/*
 * JS number (IEEE double) interface.
 *
 * JS numbers are optimistically stored in the top 31 bits of 32-bit integers,
 * but floating point literals, results that overflow 31 bits, and division and
 * modulus operands and results require a 64-bit IEEE double.  These are GC'ed
 * and pointed to by 32-bit jsvals on the stack and in object properties.
 *
 * When a JS number is treated as an object (followed by . or []), the runtime
 * wraps it with a JSObject whose valueOf method returns the unwrapped number.
 */

PR_BEGIN_EXTERN_C

#ifdef IS_LITTLE_ENDIAN
#define JSDOUBLE_HI32(x)        (((uint32 *)&(x))[1])
#define JSDOUBLE_LO32(x)        (((uint32 *)&(x))[0])
#else
#define JSDOUBLE_HI32(x)        (((uint32 *)&(x))[0])
#define JSDOUBLE_LO32(x)        (((uint32 *)&(x))[1])
#endif
#define JSDOUBLE_HI32_SIGNBIT   0x80000000
#define JSDOUBLE_HI32_EXPMASK   0x7ff00000
#define JSDOUBLE_HI32_MANTMASK  0x000fffff

#define JSDOUBLE_IS_NaN(x)                                                    \
    ((JSDOUBLE_HI32(x) & JSDOUBLE_HI32_EXPMASK) == JSDOUBLE_HI32_EXPMASK &&   \
     (JSDOUBLE_LO32(x) || (JSDOUBLE_HI32(x) & JSDOUBLE_HI32_MANTMASK)))

#define JSDOUBLE_IS_INFINITE(x)                                               \
    ((JSDOUBLE_HI32(x) & ~JSDOUBLE_HI32_SIGNBIT) == JSDOUBLE_HI32_EXPMASK &&   \
     !JSDOUBLE_LO32(x))

#define JSDOUBLE_IS_FINITE(x)                                                 \
    ((JSDOUBLE_HI32(x) & JSDOUBLE_HI32_EXPMASK) != JSDOUBLE_HI32_EXPMASK)

#define JSDOUBLE_IS_NEGZERO(d)  (JSDOUBLE_HI32(d) == JSDOUBLE_HI32_SIGNBIT && \
				 JSDOUBLE_LO32(d) == 0)

#define JSDOUBLE_IS_INT_2(d, i)	(!JSDOUBLE_IS_NEGZERO(d) && (jsdouble)i == d)

#ifdef XP_PC
/* XXX MSVC miscompiles NaN floating point comparisons for ==, !=, <, and <= */
#define JSDOUBLE_IS_INT(d, i)	(!JSDOUBLE_IS_NaN(d) && JSDOUBLE_IS_INT_2(d, i))
#else
#define JSDOUBLE_IS_INT(d, i)	JSDOUBLE_IS_INT_2(d, i)
#endif

/* Initialize the Number class, returning its prototype object. */
extern JSObject *
js_InitNumberClass(JSContext *cx, JSObject *obj);

/* GC-allocate a new JS number. */
extern jsdouble *
js_NewDouble(JSContext *cx, jsdouble d);

extern void
js_FinalizeDouble(JSContext *cx, jsdouble *dp);

extern JSBool
js_NewDoubleValue(JSContext *cx, jsdouble d, jsval *rval);

extern JSBool
js_NewNumberValue(JSContext *cx, jsdouble d, jsval *rval);

/* Construct a Number instance that wraps around d. */
extern JSObject *
js_NumberToObject(JSContext *cx, jsdouble d);

/* Convert a number to a GC'ed string. */
extern JSString *
js_NumberToString(JSContext *cx, jsdouble d);

/*
 * Convert a value to a number, returning false after reporting any error,
 * otherwise returning true with *dp set.
 */
extern JSBool
js_ValueToNumber(JSContext *cx, jsval v, jsdouble *dp);

/*
 * Convert a value or a double to an int32, according to the ECMA rules
 * for ToInt32.
 */
extern JSBool
js_ValueToECMAInt32(JSContext *cx, jsval v, int32 *ip);

extern JSBool
js_DoubleToECMAInt32(JSContext *cx, jsdouble d, int32 *ip);

/*
 * Convert a value or a double to a uint32, according to the ECMA rules
 * for ToUint32.
 */
extern JSBool
js_ValueToECMAUint32(JSContext *cx, jsval v, uint32 *ip);

extern JSBool
js_DoubleToECMAUint32(JSContext *cx, jsdouble d, uint32 *ip);

/*
 * Convert a value to a number, then to an int32 if it fits by rounding to
 * nearest; but failing with an error report if the double is out of range
 * or unordered.
 */
extern JSBool
js_ValueToInt32(JSContext *cx, jsval v, int32 *ip);

/*
 * Convert a value to a number, then to a uint16 according to the ECMA rules
 * for ToUint16.
 */
extern JSBool
js_ValueToUint16(JSContext *cx, jsval v, uint16 *ip);

/*
 * Convert a jsdouble to an integral number, stored in a jsdouble.
 * If d is NaN, return 0.  If d is an infinity, return it without conversion.
 */
extern jsdouble
js_DoubleToInteger(jsdouble d);

/*
 * Similar to strtod except that replaces overflows with infinities of the correct
 * sign and underflows with zeros of the correct sign.  Guaranteed to return the
 * closest double number to the given input in dp.
 * Also allows inputs of the form [+|-]Infinity, which produce an infinity of the
 * appropriate sign.  The case of the "Infinity" string must match.
 * If the string does not have a number in it, set *ep to s and return 0.0 in dp.
 * Return false if out of memory.
 */
extern JSBool
js_strtod(JSContext *cx, const jschar *s, const jschar **ep, jsdouble *dp);

/*
 * Similar to strtol except that handles integers of arbitrary size.  Guaranteed to
 * return the closest double number to the given input when radix is 10 or a power of 2.
 * May experience roundoff errors for very large numbers of a different radix.
 * If the string does not have a number in it, set *ep to s and return 0.0 in dp.
 * Return false if out of memory.
 */
extern JSBool
js_strtointeger(JSContext *cx, const jschar *s, const jschar **ep, jsint radix, jsdouble *dp);

PR_END_EXTERN_C

#endif /* jsnum_h___ */
