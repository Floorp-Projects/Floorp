/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_SIMD_h
#define builtin_SIMD_h

#include "jsapi.h"
#include "jsobj.h"
#include "builtin/TypedObject.h"
#include "vm/GlobalObject.h"

/*
 * JS SIMD functions.
 * Spec matching polyfill:
 * https://github.com/johnmccutchan/ecmascript_simd/blob/master/src/ecmascript_simd.js
 */

#define FLOAT32X4_NULLARY_FUNCTION_LIST(V)                                                        \
  V(zero, (FuncZero<Float32x4>), 0, 0, Zero)

#define FLOAT32X4_UNARY_FUNCTION_LIST(V)                                                          \
  V(abs, (Func<Float32x4, Abs, Float32x4>), 1, 0, Abs)                                            \
  V(fromInt32x4Bits, (FuncConvertBits<Int32x4, Float32x4>), 1, 0, FromInt32x4Bits)                \
  V(neg, (Func<Float32x4, Neg, Float32x4>), 1, 0, Neg)                                            \
  V(not, (CoercedFunc<Float32x4, Int32x4, Not, Float32x4>), 1, 0, Not)                            \
  V(reciprocal, (Func<Float32x4, Rec, Float32x4>), 1, 0, Reciprocal)                              \
  V(reciprocalSqrt, (Func<Float32x4, RecSqrt, Float32x4>), 1, 0, ReciprocalSqrt)                  \
  V(splat, (FuncSplat<Float32x4>), 1, 0, Splat)                                                   \
  V(sqrt, (Func<Float32x4, Sqrt, Float32x4>), 1, 0, Sqrt)                                         \
  V(fromInt32x4, (FuncConvert<Int32x4, Float32x4> ), 1, 0, FromInt32x4)

#define FLOAT32X4_BINARY_FUNCTION_LIST(V)                                                         \
  V(add, (Func<Float32x4, Add, Float32x4>), 2, 0, Add)                                            \
  V(and, (CoercedFunc<Float32x4, Int32x4, And, Float32x4>), 2, 0, And)                            \
  V(div, (Func<Float32x4, Div, Float32x4>), 2, 0, Div)                                            \
  V(equal, (Func<Float32x4, Equal, Int32x4>), 2, 0, Equal)                                        \
  V(greaterThan, (Func<Float32x4, GreaterThan, Int32x4>), 2, 0, GreaterThan)                      \
  V(greaterThanOrEqual, (Func<Float32x4, GreaterThanOrEqual, Int32x4>), 2, 0, GreaterThanOrEqual) \
  V(lessThan, (Func<Float32x4, LessThan, Int32x4>), 2, 0, LessThan)                               \
  V(lessThanOrEqual, (Func<Float32x4, LessThanOrEqual, Int32x4>), 2, 0, LessThanOrEqual)          \
  V(max, (Func<Float32x4, Maximum, Float32x4>), 2, 0, Max)                                        \
  V(min, (Func<Float32x4, Minimum, Float32x4>), 2, 0, Min)                                        \
  V(mul, (Func<Float32x4, Mul, Float32x4>), 2, 0, Mul)                                            \
  V(notEqual, (Func<Float32x4, NotEqual, Int32x4>), 2, 0, NotEqual)                               \
  V(shuffle, FuncShuffle<Float32x4>, 2, 0, Shuffle)                                               \
  V(or, (CoercedFunc<Float32x4, Int32x4, Or, Float32x4>), 2, 0, Or)                               \
  V(scale, (FuncWith<Float32x4, Scale, Float32x4>), 2, 0, Scale)                                  \
  V(sub, (Func<Float32x4, Sub, Float32x4>), 2, 0, Sub)                                            \
  V(withX, (FuncWith<Float32x4, WithX, Float32x4>), 2, 0, WithX)                                  \
  V(withY, (FuncWith<Float32x4, WithY, Float32x4>), 2, 0, WithY)                                  \
  V(withZ, (FuncWith<Float32x4, WithZ, Float32x4>), 2, 0, WithZ)                                  \
  V(withW, (FuncWith<Float32x4, WithW, Float32x4>), 2, 0, WithW)                                  \
  V(xor, (CoercedFunc<Float32x4, Int32x4, Xor, Float32x4>), 2, 0, Xor)

#define FLOAT32X4_TERNARY_FUNCTION_LIST(V)                                                        \
  V(clamp, Float32x4Clamp, 3, 0, Clamp)                                                           \
  V(shuffleMix, FuncShuffle<Float32x4>, 3, 0, ShuffleMix)

#define FLOAT32X4_FUNCTION_LIST(V)                                                                \
  FLOAT32X4_NULLARY_FUNCTION_LIST(V)                                                              \
  FLOAT32X4_UNARY_FUNCTION_LIST(V)                                                                \
  FLOAT32X4_BINARY_FUNCTION_LIST(V)                                                               \
  FLOAT32X4_TERNARY_FUNCTION_LIST(V)

#define INT32X4_NULLARY_FUNCTION_LIST(V)                                                          \
  V(zero, (FuncZero<Int32x4>), 0, 0, Zero)

#define INT32X4_UNARY_FUNCTION_LIST(V)                                                            \
  V(fromFloat32x4Bits, (FuncConvertBits<Float32x4, Int32x4>), 1, 0, FromFloat32x4Bits)            \
  V(neg, (Func<Int32x4, Neg, Int32x4>), 1, 0, Neg)                                                \
  V(not, (Func<Int32x4, Not, Int32x4>), 1, 0, Not)                                                \
  V(splat, (FuncSplat<Int32x4>), 0, 0, Splat)                                                     \
  V(fromFloat32x4, (FuncConvert<Float32x4, Int32x4>), 1, 0, FromFloat32x4)

#define INT32X4_BINARY_FUNCTION_LIST(V)                                                           \
  V(add, (Func<Int32x4, Add, Int32x4>), 2, 0, Add)                                                \
  V(and, (Func<Int32x4, And, Int32x4>), 2, 0, And)                                                \
  V(equal, (Func<Int32x4, Equal, Int32x4>), 2, 0, Equal)                                          \
  V(greaterThan, (Func<Int32x4, GreaterThan, Int32x4>), 2, 0, GreaterThan)                        \
  V(lessThan, (Func<Int32x4, LessThan, Int32x4>), 2, 0, LessThan)                                 \
  V(mul, (Func<Int32x4, Mul, Int32x4>), 2, 0, Mul)                                                \
  V(or, (Func<Int32x4, Or, Int32x4>), 2, 0, Or)                                                   \
  V(sub, (Func<Int32x4, Sub, Int32x4>), 2, 0, Sub)                                                \
  V(shiftLeft, (Int32x4BinaryScalar<ShiftLeft>), 2, 0, ShiftLeft)                                 \
  V(shiftRight, (Int32x4BinaryScalar<ShiftRight>), 2, 0, ShiftRight)                              \
  V(shiftRightLogical, (Int32x4BinaryScalar<ShiftRightLogical>), 2, 0, ShiftRightLogical)         \
  V(shuffle, FuncShuffle<Int32x4>, 2, 0, Shuffle)                                                 \
  V(withFlagX, (FuncWith<Int32x4, WithFlagX, Int32x4>), 2, 0, WithFlagX)                          \
  V(withFlagY, (FuncWith<Int32x4, WithFlagY, Int32x4>), 2, 0, WithFlagY)                          \
  V(withFlagZ, (FuncWith<Int32x4, WithFlagZ, Int32x4>), 2, 0, WithFlagZ)                          \
  V(withFlagW, (FuncWith<Int32x4, WithFlagW, Int32x4>), 2, 0, WithFlagW)                          \
  V(withX, (FuncWith<Int32x4, WithX, Int32x4>), 2, 0, WithX)                                      \
  V(withY, (FuncWith<Int32x4, WithY, Int32x4>), 2, 0, WithY)                                      \
  V(withZ, (FuncWith<Int32x4, WithZ, Int32x4>), 2, 0, WithZ)                                      \
  V(withW, (FuncWith<Int32x4, WithW, Int32x4>), 2, 0, WithW)                                      \
  V(xor, (Func<Int32x4, Xor, Int32x4>), 2, 0, Xor)

#define INT32X4_TERNARY_FUNCTION_LIST(V)                                                          \
  V(select, Int32x4Select, 3, 0, Select)                                                          \
  V(shuffleMix, FuncShuffle<Int32x4>, 3, 0, ShuffleMix)

#define INT32X4_QUARTERNARY_FUNCTION_LIST(V)                                                      \
  V(bool, Int32x4Bool, 4, 0, Bool)

#define INT32X4_FUNCTION_LIST(V)                                                                  \
  INT32X4_NULLARY_FUNCTION_LIST(V)                                                                \
  INT32X4_UNARY_FUNCTION_LIST(V)                                                                  \
  INT32X4_BINARY_FUNCTION_LIST(V)                                                                 \
  INT32X4_TERNARY_FUNCTION_LIST(V)                                                                \
  INT32X4_QUARTERNARY_FUNCTION_LIST(V)

namespace js {

class SIMDObject : public JSObject
{
  public:
    static const Class class_;
    static JSObject* initClass(JSContext *cx, Handle<GlobalObject *> global);
    static bool toString(JSContext *cx, unsigned int argc, jsval *vp);
};

// These classes exist for use with templates below.

struct Float32x4 {
    typedef float Elem;
    static const unsigned lanes = 4;
    static const X4TypeDescr::Type type = X4TypeDescr::TYPE_FLOAT32;

    static TypeDescr &GetTypeDescr(GlobalObject &global) {
        return global.float32x4TypeDescr().as<TypeDescr>();
    }
    static Elem toType(Elem a) {
        return a;
    }
    static bool toType(JSContext *cx, JS::HandleValue v, Elem *out) {
        *out = v.toNumber();
        return true;
    }
    static void setReturn(CallArgs &args, Elem value) {
        args.rval().setDouble(JS::CanonicalizeNaN(value));
    }
};

struct Int32x4 {
    typedef int32_t Elem;
    static const unsigned lanes = 4;
    static const X4TypeDescr::Type type = X4TypeDescr::TYPE_INT32;

    static TypeDescr &GetTypeDescr(GlobalObject &global) {
        return global.int32x4TypeDescr().as<TypeDescr>();
    }
    static Elem toType(Elem a) {
        return ToInt32(a);
    }
    static bool toType(JSContext *cx, JS::HandleValue v, Elem *out) {
        return ToInt32(cx, v, out);
    }
    static void setReturn(CallArgs &args, Elem value) {
        args.rval().setInt32(value);
    }
};

template<typename V>
JSObject *Create(JSContext *cx, typename V::Elem *data);

#define DECLARE_SIMD_FLOAT32X4_FUNCTION(Name, Func, Operands, Flags, MIRId)                                       \
extern bool                                                                                                       \
simd_float32x4_##Name(JSContext *cx, unsigned argc, Value *vp);
FLOAT32X4_FUNCTION_LIST(DECLARE_SIMD_FLOAT32X4_FUNCTION)
#undef DECLARE_SIMD_FLOAT32X4_FUNCTION

#define DECLARE_SIMD_INT32x4_FUNCTION(Name, Func, Operands, Flags, MIRId)                                         \
extern bool                                                                                                       \
simd_int32x4_##Name(JSContext *cx, unsigned argc, Value *vp);
INT32X4_FUNCTION_LIST(DECLARE_SIMD_INT32x4_FUNCTION)
#undef DECLARE_SIMD_INT32x4_FUNCTION

}  /* namespace js */

JSObject *
js_InitSIMDClass(JSContext *cx, js::HandleObject obj);

#endif /* builtin_SIMD_h */
