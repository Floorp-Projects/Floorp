/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* BigInt. */

#ifndef js_BigInt_h
#define js_BigInt_h

#include "mozilla/Span.h"  // mozilla::Span

#include <limits>       // std::numeric_limits
#include <stdint.h>     // int64_t, uint64_t
#include <type_traits>  // std::enable_if_t, std::{true,false}_type, std::is_{integral,signed,unsigned}_v

#include "jstypes.h"  // JS_PUBLIC_API
#include "js/TypeDecls.h"

namespace mozilla {
template <typename T>
class Range;
}

namespace JS {

class JS_PUBLIC_API BigInt;

namespace detail {

extern JS_PUBLIC_API BigInt* BigIntFromInt64(JSContext* cx, int64_t num);
extern JS_PUBLIC_API BigInt* BigIntFromUint64(JSContext* cx, uint64_t num);
extern JS_PUBLIC_API BigInt* BigIntFromBool(JSContext* cx, bool b);

template <typename T, typename = void>
struct NumberToBigIntConverter;

template <typename SignedIntT>
struct NumberToBigIntConverter<
    SignedIntT,
    std::enable_if_t<std::is_integral_v<SignedIntT> &&
                     std::is_signed_v<SignedIntT> &&
                     std::numeric_limits<SignedIntT>::digits <= 64>> {
  static BigInt* convert(JSContext* cx, SignedIntT num) {
    return BigIntFromInt64(cx, num);
  }
};

template <typename UnsignedIntT>
struct NumberToBigIntConverter<
    UnsignedIntT,
    std::enable_if_t<std::is_integral_v<UnsignedIntT> &&
                     std::is_unsigned_v<UnsignedIntT> &&
                     std::numeric_limits<UnsignedIntT>::digits <= 64>> {
  static BigInt* convert(JSContext* cx, UnsignedIntT num) {
    return BigIntFromUint64(cx, num);
  }
};

template <>
struct NumberToBigIntConverter<bool> {
  static BigInt* convert(JSContext* cx, bool b) {
    return BigIntFromBool(cx, b);
  }
};

}  // namespace detail

/**
 * Create a BigInt from an integer value. All integral types not larger than 64
 * bits in size are supported.
 */
template <typename NumericT>
extern JS_PUBLIC_API BigInt* NumberToBigInt(JSContext* cx, NumericT val) {
  return detail::NumberToBigIntConverter<NumericT>::convert(cx, val);
}

/**
 * Create a BigInt from a floating-point value. If the number isn't integral
 * (that is, if it's NaN, an infinity, or contains a fractional component),
 * this function returns null and throws an exception.
 *
 * Passing -0.0 will produce the bigint 0n.
 */
extern JS_PUBLIC_API BigInt* NumberToBigInt(JSContext* cx, double num);

/**
 * Create a BigInt by parsing a string using the ECMAScript StringToBigInt
 * algorithm (https://tc39.es/ecma262/#sec-stringtobigint). Latin1 and two-byte
 * character ranges are supported. It may be convenient to use
 * JS::ConstLatin1Chars or JS::ConstTwoByteChars.
 *
 * (StringToBigInt performs parsing similar to that performed by the |Number|
 * global function when passed a string, but it doesn't allow infinities,
 * decimal points, or exponential notation, and neither algorithm allows numeric
 * separators or an 'n' suffix character. This fast-and-loose description is
 * offered purely as a convenience to the reader: see the specification
 * algorithm for exact behavior.)
 *
 * If parsing fails, this function returns null and throws an exception.
 */
extern JS_PUBLIC_API BigInt* StringToBigInt(
    JSContext* cx, mozilla::Range<const Latin1Char> chars);

extern JS_PUBLIC_API BigInt* StringToBigInt(
    JSContext* cx, mozilla::Range<const char16_t> chars);

/**
 * Create a BigInt by parsing a string consisting of an optional sign character
 * followed by one or more alphanumeric ASCII digits in the provided radix.
 *
 * If the radix is not in the range [2, 36], or the string fails to parse, this
 * function returns null and throws an exception.
 */
extern JS_PUBLIC_API BigInt* SimpleStringToBigInt(
    JSContext* cx, mozilla::Span<const char> chars, unsigned radix);

/**
 * Convert a JS::Value to a BigInt using the ECMAScript ToBigInt algorithm
 * (https://tc39.es/ecma262/#sec-tobigint).
 *
 * (Note in particular that this will throw if passed a value whose type is
 * 'number'. To convert a number to a BigInt, use one of the overloads of
 * JS::NumberToBigInt().)
 */
extern JS_PUBLIC_API BigInt* ToBigInt(JSContext* cx, Handle<Value> val);

/**
 * Convert the given BigInt, modulo 2**64, to a signed 64-bit integer.
 */
extern JS_PUBLIC_API int64_t ToBigInt64(BigInt* bi);

/**
 * Convert the given BigInt, modulo 2**64, to an unsigned 64-bit integer.
 */
extern JS_PUBLIC_API uint64_t ToBigUint64(BigInt* bi);

}  // namespace JS

#endif /* js_BigInt_h */
