/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsnuminlines_h___
#define jsnuminlines_h___

#include "vm/NumericConversions.h"
#include "vm/Unicode.h"

#include "jsstrinlines.h"


namespace js {

template<typename T> struct NumberTraits { };
template<> struct NumberTraits<int32_t> {
  static JS_ALWAYS_INLINE int32_t NaN() { return 0; }
  static JS_ALWAYS_INLINE int32_t toSelfType(int32_t i) { return i; }
  static JS_ALWAYS_INLINE int32_t toSelfType(double d) { return ToUint32(d); }
};
template<> struct NumberTraits<double> {
  static JS_ALWAYS_INLINE double NaN() { return js_NaN; }
  static JS_ALWAYS_INLINE double toSelfType(int32_t i) { return i; }
  static JS_ALWAYS_INLINE double toSelfType(double d) { return d; }
};

template<typename T>
static JS_ALWAYS_INLINE bool
StringToNumberType(JSContext *cx, JSString *str, T *result)
{
    size_t length = str->length();
    const jschar *chars = str->getChars(NULL);
    if (!chars)
        return false;

    if (length == 1) {
        jschar c = chars[0];
        if ('0' <= c && c <= '9') {
            *result = NumberTraits<T>::toSelfType(T(c - '0'));
            return true;
        }
        if (unicode::IsSpace(c)) {
            *result = NumberTraits<T>::toSelfType(T(0));
            return true;
        }
        *result = NumberTraits<T>::NaN();
        return true;
    }

    const jschar *end = chars + length;
    const jschar *bp = SkipSpace(chars, end);

    /* ECMA doesn't allow signed hex numbers (bug 273467). */
    if (end - bp >= 2 && bp[0] == '0' && (bp[1] == 'x' || bp[1] == 'X')) {
        /* Looks like a hex number. */
        const jschar *endptr;
        double d;
        if (!GetPrefixInteger(cx, bp + 2, end, 16, &endptr, &d) || SkipSpace(endptr, end) != end) {
            *result = NumberTraits<T>::NaN();
            return true;
        }
        *result = NumberTraits<T>::toSelfType(d);
        return true;
    }

    /*
     * Note that ECMA doesn't treat a string beginning with a '0' as
     * an octal number here. This works because all such numbers will
     * be interpreted as decimal by js_strtod.  Also, any hex numbers
     * that have made it here (which can only be negative ones) will
     * be treated as 0 without consuming the 'x' by js_strtod.
     */
    const jschar *ep;
    double d;
    if (!js_strtod(cx, bp, end, &ep, &d) || SkipSpace(ep, end) != end) {
        *result = NumberTraits<T>::NaN();
        return true;
    }
    *result = NumberTraits<T>::toSelfType(d);
    return true;
}

} // namespace js

#endif /* jsnuminlines_h___ */
