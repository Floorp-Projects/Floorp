/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsnum_h
#define jsnum_h

#include "mozilla/FloatingPoint.h"

#include "jscntxt.h"

#include "vm/NumericConversions.h"

extern double js_PositiveInfinity;
extern double js_NegativeInfinity;

namespace js {

extern bool
InitRuntimeNumberState(JSRuntime *rt);

#if !ENABLE_INTL_API
extern void
FinishRuntimeNumberState(JSRuntime *rt);
#endif

} /* namespace js */

/* Initialize the Number class, returning its prototype object. */
extern JSObject *
js_InitNumberClass(JSContext *cx, js::HandleObject obj);

/*
 * String constants for global function names, used in jsapi.c and jsnum.c.
 */
extern const char js_isNaN_str[];
extern const char js_isFinite_str[];
extern const char js_parseFloat_str[];
extern const char js_parseInt_str[];

class JSString;

/*
 * When base == 10, this function implements ToString() as specified by
 * ECMA-262-5 section 9.8.1; but note that it handles integers specially for
 * performance.  See also js::NumberToCString().
 */
template <js::AllowGC allowGC>
extern JSString *
js_NumberToString(JSContext *cx, double d);

namespace js {

template <AllowGC allowGC>
extern JSFlatString *
Int32ToString(JSContext *cx, int32_t i);

/*
 * Convert an integer or double (contained in the given value) to a string and
 * append to the given buffer.
 */
extern bool JS_FASTCALL
NumberValueToStringBuffer(JSContext *cx, const Value &v, StringBuffer &sb);

/* Same as js_NumberToString, different signature. */
extern JSFlatString *
NumberToString(JSContext *cx, double d);

extern JSFlatString *
IndexToString(JSContext *cx, uint32_t index);

/*
 * Usually a small amount of static storage is enough, but sometimes we need
 * to dynamically allocate much more.  This struct encapsulates that.
 * Dynamically allocated memory will be freed when the object is destroyed.
 */
struct ToCStringBuf
{
    /*
     * The longest possible result that would need to fit in sbuf is
     * (-0x80000000).toString(2), which has length 33.  Longer cases are
     * possible, but they'll go in dbuf.
     */
    static const size_t sbufSize = 34;
    char sbuf[sbufSize];
    char *dbuf;

    ToCStringBuf();
    ~ToCStringBuf();
};

/*
 * Convert a number to a C string.  When base==10, this function implements
 * ToString() as specified by ECMA-262-5 section 9.8.1.  It handles integral
 * values cheaply.  Return NULL if we ran out of memory.  See also
 * js_NumberToCString().
 */
extern char *
NumberToCString(JSContext *cx, ToCStringBuf *cbuf, double d, int base = 10);

/*
 * The largest positive integer such that all positive integers less than it
 * may be precisely represented using the IEEE-754 double-precision format.
 */
const double DOUBLE_INTEGRAL_PRECISION_LIMIT = uint64_t(1) << 53;

/*
 * Parse a decimal number encoded in |chars|.  The decimal number must be
 * sufficiently small that it will not overflow the integrally-precise range of
 * the double type -- that is, the number will be smaller than
 * DOUBLE_INTEGRAL_PRECISION_LIMIT
 */
extern double
ParseDecimalNumber(const JS::TwoByteChars chars);

/*
 * Compute the positive integer of the given base described immediately at the
 * start of the range [start, end) -- no whitespace-skipping, no magical
 * leading-"0" octal or leading-"0x" hex behavior, no "+"/"-" parsing, just
 * reading the digits of the integer.  Return the index one past the end of the
 * digits of the integer in *endp, and return the integer itself in *dp.  If
 * base is 10 or a power of two the returned integer is the closest possible
 * double; otherwise extremely large integers may be slightly inaccurate.
 *
 * If [start, end) does not begin with a number with the specified base,
 * *dp == 0 and *endp == start upon return.
 */
extern bool
GetPrefixInteger(JSContext *cx, const jschar *start, const jschar *end, int base,
                 const jschar **endp, double *dp);

/* ES5 9.3 ToNumber, overwriting *vp with the appropriate number value. */
JS_ALWAYS_INLINE bool
ToNumber(JSContext *cx, Value *vp)
{
#ifdef DEBUG
    {
        SkipRoot skip(cx, vp);
        MaybeCheckStackRoots(cx);
    }
#endif

    if (vp->isNumber())
        return true;
    double d;
    extern bool ToNumberSlow(JSContext *cx, js::Value v, double *dp);
    if (!ToNumberSlow(cx, *vp, &d))
        return false;

    vp->setNumber(d);
    return true;
}

JSBool
num_parseInt(JSContext *cx, unsigned argc, Value *vp);

}  /* namespace js */

/*
 * Similar to strtod except that it replaces overflows with infinities of the
 * correct sign, and underflows with zeros of the correct sign.  Guaranteed to
 * return the closest double number to the given input in dp.
 *
 * Also allows inputs of the form [+|-]Infinity, which produce an infinity of
 * the appropriate sign.  The case of the "Infinity" string must match exactly.
 * If the string does not contain a number, set *ep to s and return 0.0 in dp.
 * Return false if out of memory.
 */
extern JSBool
js_strtod(JSContext *cx, const jschar *s, const jschar *send,
          const jschar **ep, double *dp);

extern JSBool
js_num_valueOf(JSContext *cx, unsigned argc, js::Value *vp);

namespace js {

static JS_ALWAYS_INLINE bool
ValueFitsInInt32(const Value &v, int32_t *pi)
{
    if (v.isInt32()) {
        *pi = v.toInt32();
        return true;
    }
    return v.isDouble() && mozilla::DoubleIsInt32(v.toDouble(), pi);
}

/*
 * Returns true if the given value is definitely an index: that is, the value
 * is a number that's an unsigned 32-bit integer.
 *
 * This method prioritizes common-case speed over accuracy in every case.  It
 * can produce false negatives (but not false positives): some values which are
 * indexes will be reported not to be indexes by this method.  Users must
 * consider this possibility when using this method.
 */
static JS_ALWAYS_INLINE bool
IsDefinitelyIndex(const Value &v, uint32_t *indexp)
{
    if (v.isInt32() && v.toInt32() >= 0) {
        *indexp = v.toInt32();
        return true;
    }

    int32_t i;
    if (v.isDouble() && mozilla::DoubleIsInt32(v.toDouble(), &i) && i >= 0) {
        *indexp = uint32_t(i);
        return true;
    }

    return false;
}

/* ES5 9.4 ToInteger. */
static inline bool
ToInteger(JSContext *cx, const js::Value &v, double *dp)
{
#ifdef DEBUG
    {
        SkipRoot skip(cx, &v);
        MaybeCheckStackRoots(cx);
    }
#endif

    if (v.isInt32()) {
        *dp = v.toInt32();
        return true;
    }
    if (v.isDouble()) {
        *dp = v.toDouble();
    } else {
        extern bool ToNumberSlow(JSContext *cx, Value v, double *dp);
        if (!ToNumberSlow(cx, v, dp))
            return false;
    }
    *dp = ToInteger(*dp);
    return true;
}

inline bool
SafeAdd(int32_t one, int32_t two, int32_t *res)
{
    *res = one + two;
    int64_t ores = (int64_t)one + (int64_t)two;
    return ores == (int64_t)*res;
}

inline bool
SafeSub(int32_t one, int32_t two, int32_t *res)
{
    *res = one - two;
    int64_t ores = (int64_t)one - (int64_t)two;
    return ores == (int64_t)*res;
}

inline bool
SafeMul(int32_t one, int32_t two, int32_t *res)
{
    *res = one * two;
    int64_t ores = (int64_t)one * (int64_t)two;
    return ores == (int64_t)*res;
}

} /* namespace js */

#endif /* jsnum_h */
