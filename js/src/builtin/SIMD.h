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

#define FLOAT32X4_NULLARY_FUNCTION_LIST(V)                                                                        \
  V(zero, (FuncZero<Float32x4>), 0, 0, Zero)

#define FLOAT32X4_UNARY_FUNCTION_LIST(V)                                                                          \
  V(abs, (Func<Float32x4, Abs<float, Float32x4>, Float32x4>), 1, 0, Abs)                                          \
  V(bitsToInt32x4, (FuncConvertBits<Float32x4, Int32x4>), 1, 0, BitsToInt32x4)                                    \
  V(neg, (Func<Float32x4, Neg<float, Float32x4>, Float32x4>), 1, 0, Neg)                                          \
  V(reciprocal, (Func<Float32x4, Rec<float, Float32x4>, Float32x4>), 1, 0, Reciprocal)                            \
  V(reciprocalSqrt, (Func<Float32x4, RecSqrt<float, Float32x4>, Float32x4>), 1, 0, ReciprocalSqrt)                \
  V(splat, (FuncSplat<Float32x4>), 1, 0, Splat)                                                                   \
  V(sqrt, (Func<Float32x4, Sqrt<float, Float32x4>, Float32x4>), 1, 0, Sqrt)                                       \
  V(toInt32x4, (FuncConvert<Float32x4, Int32x4>), 1, 0, ToInt32x4)

#define FLOAT32X4_BINARY_FUNCTION_LIST(V)                                                                         \
  V(add, (Func<Float32x4, Add<float, Float32x4>, Float32x4>), 2, 0, Add)                                          \
  V(div, (Func<Float32x4, Div<float, Float32x4>, Float32x4>), 2, 0, Div)                                          \
  V(equal, (Func<Float32x4, Equal<float, Int32x4>, Int32x4>), 2, 0, Equal)                                        \
  V(greaterThan, (Func<Float32x4, GreaterThan<float, Int32x4>, Int32x4>), 2, 0, GreaterThan)                      \
  V(greaterThanOrEqual, (Func<Float32x4, GreaterThanOrEqual<float, Int32x4>, Int32x4>), 2, 0, GreaterThanOrEqual) \
  V(lessThan, (Func<Float32x4, LessThan<float, Int32x4>, Int32x4>), 2, 0, LessThan)                               \
  V(lessThanOrEqual, (Func<Float32x4, LessThanOrEqual<float, Int32x4>, Int32x4>), 2, 0, LessThanOrEqual)          \
  V(max, (Func<Float32x4, Maximum<float, Float32x4>, Float32x4>), 2, 0, Max)                                      \
  V(min, (Func<Float32x4, Minimum<float, Float32x4>, Float32x4>), 2, 0, Min)                                      \
  V(mul, (Func<Float32x4, Mul<float, Float32x4>, Float32x4>), 2, 0, Mul)                                          \
  V(notEqual, (Func<Float32x4, NotEqual<float, Int32x4>, Int32x4>), 2, 0, NotEqual)                               \
  V(shuffle, (FuncShuffle<Float32x4, Shuffle<float, Float32x4>, Float32x4>), 2, 0, Shuffle)                       \
  V(scale, (FuncWith<Float32x4, Scale<float, Float32x4>, Float32x4>), 2, 0, Scale)                                \
  V(sub, (Func<Float32x4, Sub<float, Float32x4>, Float32x4>), 2, 0, Sub)                                          \
  V(withX, (FuncWith<Float32x4, WithX<float, Float32x4>, Float32x4>), 2, 0, WithX)                                \
  V(withY, (FuncWith<Float32x4, WithY<float, Float32x4>, Float32x4>), 2, 0, WithY)                                \
  V(withZ, (FuncWith<Float32x4, WithZ<float, Float32x4>, Float32x4>), 2, 0, WithZ)                                \
  V(withW, (FuncWith<Float32x4, WithW<float, Float32x4>, Float32x4>), 2, 0, WithW)

#define FLOAT32X4_TERNARY_FUNCTION_LIST(V)                                                                        \
  V(clamp, Float32x4Clamp, 3, 0, Clamp)                                                                           \
  V(shuffleMix, (FuncShuffle<Float32x4, Shuffle<float, Float32x4>, Float32x4>), 3, 0, ShuffleMix)

#define FLOAT32X4_FUNCTION_LIST(V)                                                                                \
  FLOAT32X4_NULLARY_FUNCTION_LIST(V)                                                                              \
  FLOAT32X4_UNARY_FUNCTION_LIST(V)                                                                                \
  FLOAT32X4_BINARY_FUNCTION_LIST(V)                                                                               \
  FLOAT32X4_TERNARY_FUNCTION_LIST(V)

#define INT32X4_NULLARY_FUNCTION_LIST(V)                                                                          \
  V(zero, (FuncZero<Int32x4>), 0, 0, Zero)

#define INT32X4_UNARY_FUNCTION_LIST(V)                                                                            \
  V(bitsToFloat32x4, (FuncConvertBits<Int32x4, Float32x4>), 1, 0, BitsToFloat32x4)                                \
  V(neg, (Func<Int32x4, Neg<int32_t, Int32x4>, Int32x4>), 1, 0, Neg)                                              \
  V(not, (Func<Int32x4, Not<int32_t, Int32x4>, Int32x4>), 1, 0, Not)                                              \
  V(splat, (FuncSplat<Int32x4>), 0, 0, Splat)                                                                     \
  V(toFloat32x4, (FuncConvert<Int32x4, Float32x4>), 1, 0, ToFloat32x4)

#define INT32X4_BINARY_FUNCTION_LIST(V)                                                                           \
  V(add, (Func<Int32x4, Add<int32_t, Int32x4>, Int32x4>), 2, 0, Add)                                              \
  V(and, (Func<Int32x4, And<int32_t, Int32x4>, Int32x4>), 2, 0, And)                                              \
  V(mul, (Func<Int32x4, Mul<int32_t, Int32x4>, Int32x4>), 2, 0, Mul)                                              \
  V(or, (Func<Int32x4, Or<int32_t, Int32x4>, Int32x4>), 2, 0, Or)                                                 \
  V(sub, (Func<Int32x4, Sub<int32_t, Int32x4>, Int32x4>), 2, 0, Sub)                                              \
  V(shuffle, (FuncShuffle<Int32x4, Shuffle<int32_t, Int32x4>, Int32x4>), 2, 0, Shuffle)                           \
  V(withFlagX, (FuncWith<Int32x4, WithFlagX<int32_t, Int32x4>, Int32x4>), 2, 0, WithFlagX)                        \
  V(withFlagY, (FuncWith<Int32x4, WithFlagY<int32_t, Int32x4>, Int32x4>), 2, 0, WithFlagY)                        \
  V(withFlagZ, (FuncWith<Int32x4, WithFlagZ<int32_t, Int32x4>, Int32x4>), 2, 0, WithFlagZ)                        \
  V(withFlagW, (FuncWith<Int32x4, WithFlagW<int32_t, Int32x4>, Int32x4>), 2, 0, WithFlagW)                        \
  V(withX, (FuncWith<Int32x4, WithX<int32_t, Int32x4>, Int32x4>), 2, 0, WithX)                                    \
  V(withY, (FuncWith<Int32x4, WithY<int32_t, Int32x4>, Int32x4>), 2, 0, WithY)                                    \
  V(withZ, (FuncWith<Int32x4, WithZ<int32_t, Int32x4>, Int32x4>), 2, 0, WithZ)                                    \
  V(withW, (FuncWith<Int32x4, WithW<int32_t, Int32x4>, Int32x4>), 2, 0, WithW)                                    \
  V(xor, (Func<Int32x4, Xor<int32_t, Int32x4>, Int32x4>), 2, 0, Xor)

#define INT32X4_TERNARY_FUNCTION_LIST(V)                                                                          \
  V(select, Int32x4Select, 3, 0, Select)                                                                          \
  V(shuffleMix, (FuncShuffle<Int32x4, Shuffle<int32_t, Int32x4>, Int32x4>), 3, 0, ShuffleMix)

#define INT32X4_QUARTERNARY_FUNCTION_LIST(V)                                                                      \
  V(bool, Int32x4Bool, 4, 0, Bool)

#define INT32X4_FUNCTION_LIST(V)                                                                                  \
  INT32X4_NULLARY_FUNCTION_LIST(V)                                                                                \
  INT32X4_UNARY_FUNCTION_LIST(V)                                                                                  \
  INT32X4_BINARY_FUNCTION_LIST(V)                                                                                 \
  INT32X4_TERNARY_FUNCTION_LIST(V)                                                                                \
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
    static const int32_t lanes = 4;
    static const X4TypeDescr::Type type =
        X4TypeDescr::TYPE_FLOAT32;

    static TypeDescr &GetTypeDescr(GlobalObject &global) {
        return global.float32x4TypeDescr().as<TypeDescr>();
    }
    static Elem toType(Elem a) {
        return a;
    }
    static void toType2(JSContext *cx, JS::Handle<JS::Value> v, Elem *out) {
        *out = v.toNumber();
    }
    static void setReturn(CallArgs &args, float value) {
        args.rval().setDouble(JS::CanonicalizeNaN(value));
    }
};

struct Int32x4 {
    typedef int32_t Elem;
    static const int32_t lanes = 4;
    static const X4TypeDescr::Type type =
        X4TypeDescr::TYPE_INT32;

    static TypeDescr &GetTypeDescr(GlobalObject &global) {
        return global.int32x4TypeDescr().as<TypeDescr>();
    }
    static Elem toType(Elem a) {
        return ToInt32(a);
    }
    static void toType2(JSContext *cx, JS::Handle<JS::Value> v, Elem *out) {
        ToInt32(cx,v,out);
    }
    static void setReturn(CallArgs &args, int32_t value) {
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
