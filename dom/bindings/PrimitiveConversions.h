/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Conversions from jsval to primitive values
 */

#ifndef mozilla_dom_PrimitiveConversions_h
#define mozilla_dom_PrimitiveConversions_h

#include "xpcpublic.h"

namespace mozilla {
namespace dom {

template<typename T>
struct PrimitiveConversionTraits {
};

struct PrimitiveConversionTraits_smallInt {
  // The output of JS::ToInt32 is determined as follows:
  //   1) The value is converted to a double
  //   2) Anything that's not a finite double returns 0
  //   3) The double is rounded towards zero to the nearest integer
  //   4) The resulting integer is reduced mod 2^32.  The output of this
  //      operation is an integer in the range [0, 2^32).
  //   5) If the resulting number is >= 2^31, 2^32 is subtracted from it.
  //
  // The result of all this is a number in the range [-2^31, 2^31)
  //
  // WebIDL conversions for the 8-bit, 16-bit, and 32-bit integer types
  // are defined in the same way, except that step 4 uses reduction mod
  // 2^8 and 2^16 for the 8-bit and 16-bit types respectively, and step 5
  // is only done for the signed types.
  //
  // C/C++ define integer conversion semantics to unsigned types as taking
  // your input integer mod (1 + largest value representable in the
  // unsigned type).  Since 2^32 is zero mod 2^8, 2^16, and 2^32,
  // converting to the unsigned int of the relevant width will correctly
  // perform step 4; in particular, the 2^32 possibly subtracted in step 5
  // will become 0.
  //
  // Once we have step 4 done, we're just going to assume 2s-complement
  // representation and cast directly to the type we really want.
  //
  // So we can cast directly for all unsigned types and for int32_t; for
  // the smaller-width signed types we need to cast through the
  // corresponding unsigned type.
  typedef int32_t jstype;
  typedef int32_t intermediateType;
  static inline bool converter(JSContext* cx, JS::Value v, jstype* retval) {
    return JS::ToInt32(cx, v, retval);
  }
};
template<>
struct PrimitiveConversionTraits<int8_t> : PrimitiveConversionTraits_smallInt {
  typedef uint8_t intermediateType;
};
template<>
struct PrimitiveConversionTraits<uint8_t> : PrimitiveConversionTraits_smallInt {
};
template<>
struct PrimitiveConversionTraits<int16_t> : PrimitiveConversionTraits_smallInt {
  typedef uint16_t intermediateType;
};
template<>
struct PrimitiveConversionTraits<uint16_t> : PrimitiveConversionTraits_smallInt {
};
template<>
struct PrimitiveConversionTraits<int32_t> : PrimitiveConversionTraits_smallInt {
};
template<>
struct PrimitiveConversionTraits<uint32_t> : PrimitiveConversionTraits_smallInt {
};

template<>
struct PrimitiveConversionTraits<bool> {
  typedef JSBool jstype;
  typedef bool intermediateType;
  static inline bool converter(JSContext* /* unused */, JS::Value v, jstype* retval) {
    *retval = JS::ToBoolean(v);
    return true;
  }
};

template<>
struct PrimitiveConversionTraits<int64_t> {
  typedef int64_t jstype;
  typedef int64_t intermediateType;
  static inline bool converter(JSContext* cx, JS::Value v, jstype* retval) {
    return JS::ToInt64(cx, v, retval);
  }
};

template<>
struct PrimitiveConversionTraits<uint64_t> {
  typedef uint64_t jstype;
  typedef uint64_t intermediateType;
  static inline bool converter(JSContext* cx, JS::Value v, jstype* retval) {
    return JS::ToUint64(cx, v, retval);
  }
};

struct PrimitiveConversionTraits_float {
  typedef double jstype;
  typedef double intermediateType;
  static inline bool converter(JSContext* cx, JS::Value v, jstype* retval) {
    return JS::ToNumber(cx, v, retval);
  }
};
template<>
struct PrimitiveConversionTraits<float> : PrimitiveConversionTraits_float {
};
template<>
struct PrimitiveConversionTraits<double> : PrimitiveConversionTraits_float {
};

template<typename T>
bool ValueToPrimitive(JSContext* cx, JS::Value v, T* retval)
{
  typename PrimitiveConversionTraits<T>::jstype t;
  if (!PrimitiveConversionTraits<T>::converter(cx, v, &t))
    return false;

  *retval =
    static_cast<typename PrimitiveConversionTraits<T>::intermediateType>(t);
  return true;
}

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_PrimitiveConversions_h */
