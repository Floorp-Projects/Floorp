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

#define FLOAT32X4_NULLARY_FUNCTION_LIST(V)                                          \
  V(zero, (FuncZero<Float32x4>), 0, 0)

#define FLOAT32X4_UNARY_FUNCTION_LIST(V)                                            \
  V(abs, (UnaryFunc<Float32x4, Abs, Float32x4>), 1, 0)                              \
  V(fromInt32x4Bits, (FuncConvertBits<Int32x4, Float32x4>), 1, 0)                   \
  V(neg, (UnaryFunc<Float32x4, Neg, Float32x4>), 1, 0)                              \
  V(not, (CoercedUnaryFunc<Float32x4, Int32x4, Not, Float32x4>), 1, 0)              \
  V(reciprocal, (UnaryFunc<Float32x4, Rec, Float32x4>), 1, 0)                       \
  V(reciprocalSqrt, (UnaryFunc<Float32x4, RecSqrt, Float32x4>), 1, 0)               \
  V(splat, (FuncSplat<Float32x4>), 1, 0)                                            \
  V(sqrt, (UnaryFunc<Float32x4, Sqrt, Float32x4>), 1, 0)                            \
  V(fromInt32x4, (FuncConvert<Int32x4, Float32x4> ), 1, 0)

#define FLOAT32X4_BINARY_FUNCTION_LIST(V)                                           \
  V(add, (BinaryFunc<Float32x4, Add, Float32x4>), 2, 0)                             \
  V(and, (CoercedBinaryFunc<Float32x4, Int32x4, And, Float32x4>), 2, 0)             \
  V(div, (BinaryFunc<Float32x4, Div, Float32x4>), 2, 0)                             \
  V(equal, (BinaryFunc<Float32x4, Equal, Int32x4>), 2, 0)                           \
  V(greaterThan, (BinaryFunc<Float32x4, GreaterThan, Int32x4>), 2, 0)               \
  V(greaterThanOrEqual, (BinaryFunc<Float32x4, GreaterThanOrEqual, Int32x4>), 2, 0) \
  V(lessThan, (BinaryFunc<Float32x4, LessThan, Int32x4>), 2, 0)                     \
  V(lessThanOrEqual, (BinaryFunc<Float32x4, LessThanOrEqual, Int32x4>), 2, 0)       \
  V(max, (BinaryFunc<Float32x4, Maximum, Float32x4>), 2, 0)                         \
  V(min, (BinaryFunc<Float32x4, Minimum, Float32x4>), 2, 0)                         \
  V(mul, (BinaryFunc<Float32x4, Mul, Float32x4>), 2, 0)                             \
  V(notEqual, (BinaryFunc<Float32x4, NotEqual, Int32x4>), 2, 0)                     \
  V(shuffle, FuncShuffle<Float32x4>, 2, 0)                                          \
  V(or, (CoercedBinaryFunc<Float32x4, Int32x4, Or, Float32x4>), 2, 0)               \
  V(scale, (FuncWith<Float32x4, Scale>), 2, 0)                                      \
  V(sub, (BinaryFunc<Float32x4, Sub, Float32x4>), 2, 0)                             \
  V(withX, (FuncWith<Float32x4, WithX>), 2, 0)                                      \
  V(withY, (FuncWith<Float32x4, WithY>), 2, 0)                                      \
  V(withZ, (FuncWith<Float32x4, WithZ>), 2, 0)                                      \
  V(withW, (FuncWith<Float32x4, WithW>), 2, 0)                                      \
  V(xor, (CoercedBinaryFunc<Float32x4, Int32x4, Xor, Float32x4>), 2, 0)

#define FLOAT32X4_TERNARY_FUNCTION_LIST(V)                                          \
  V(clamp, Float32x4Clamp, 3, 0)                                                    \
  V(select, Float32x4Select, 3, 0)                                                  \
  V(shuffleMix, FuncShuffle<Float32x4>, 3, 0)

#define FLOAT32X4_FUNCTION_LIST(V)                                                  \
  FLOAT32X4_NULLARY_FUNCTION_LIST(V)                                                \
  FLOAT32X4_UNARY_FUNCTION_LIST(V)                                                  \
  FLOAT32X4_BINARY_FUNCTION_LIST(V)                                                 \
  FLOAT32X4_TERNARY_FUNCTION_LIST(V)

#define INT32X4_NULLARY_FUNCTION_LIST(V)                                            \
  V(zero, (FuncZero<Int32x4>), 0, 0)

#define INT32X4_UNARY_FUNCTION_LIST(V)                                              \
  V(fromFloat32x4Bits, (FuncConvertBits<Float32x4, Int32x4>), 1, 0)                 \
  V(neg, (UnaryFunc<Int32x4, Neg, Int32x4>), 1, 0)                                  \
  V(not, (UnaryFunc<Int32x4, Not, Int32x4>), 1, 0)                                  \
  V(splat, (FuncSplat<Int32x4>), 0, 0)                                              \
  V(fromFloat32x4, (FuncConvert<Float32x4, Int32x4>), 1, 0)

#define INT32X4_BINARY_FUNCTION_LIST(V)                                             \
  V(add, (BinaryFunc<Int32x4, Add, Int32x4>), 2, 0)                                 \
  V(and, (BinaryFunc<Int32x4, And, Int32x4>), 2, 0)                                 \
  V(equal, (BinaryFunc<Int32x4, Equal, Int32x4>), 2, 0)                             \
  V(greaterThan, (BinaryFunc<Int32x4, GreaterThan, Int32x4>), 2, 0)                 \
  V(lessThan, (BinaryFunc<Int32x4, LessThan, Int32x4>), 2, 0)                       \
  V(mul, (BinaryFunc<Int32x4, Mul, Int32x4>), 2, 0)                                 \
  V(or, (BinaryFunc<Int32x4, Or, Int32x4>), 2, 0)                                   \
  V(sub, (BinaryFunc<Int32x4, Sub, Int32x4>), 2, 0)                                 \
  V(shiftLeft, (Int32x4BinaryScalar<ShiftLeft>), 2, 0)                              \
  V(shiftRight, (Int32x4BinaryScalar<ShiftRight>), 2, 0)                            \
  V(shiftRightLogical, (Int32x4BinaryScalar<ShiftRightLogical>), 2, 0)              \
  V(shuffle, FuncShuffle<Int32x4>, 2, 0)                                            \
  V(withFlagX, (FuncWith<Int32x4, WithFlagX>), 2, 0)                                \
  V(withFlagY, (FuncWith<Int32x4, WithFlagY>), 2, 0)                                \
  V(withFlagZ, (FuncWith<Int32x4, WithFlagZ>), 2, 0)                                \
  V(withFlagW, (FuncWith<Int32x4, WithFlagW>), 2, 0)                                \
  V(withX, (FuncWith<Int32x4, WithX>), 2, 0)                                        \
  V(withY, (FuncWith<Int32x4, WithY>), 2, 0)                                        \
  V(withZ, (FuncWith<Int32x4, WithZ>), 2, 0)                                        \
  V(withW, (FuncWith<Int32x4, WithW>), 2, 0)                                        \
  V(xor, (BinaryFunc<Int32x4, Xor, Int32x4>), 2, 0)

#define INT32X4_TERNARY_FUNCTION_LIST(V)                                            \
  V(select, Int32x4Select, 3, 0)                                                    \
  V(shuffleMix, FuncShuffle<Int32x4>, 3, 0)

#define INT32X4_QUARTERNARY_FUNCTION_LIST(V)                                        \
  V(bool, Int32x4Bool, 4, 0)

#define INT32X4_FUNCTION_LIST(V)                                                    \
  INT32X4_NULLARY_FUNCTION_LIST(V)                                                  \
  INT32X4_UNARY_FUNCTION_LIST(V)                                                    \
  INT32X4_BINARY_FUNCTION_LIST(V)                                                   \
  INT32X4_TERNARY_FUNCTION_LIST(V)                                                  \
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
JSObject *CreateSimd(JSContext *cx, typename V::Elem *data);

template<typename V>
bool IsVectorObject(HandleValue v);

template<typename V>
bool ToSimdConstant(JSContext *cx, HandleValue v, jit::SimdConstant *out);

#define DECLARE_SIMD_FLOAT32X4_FUNCTION(Name, Func, Operands, Flags) \
extern bool                                                          \
simd_float32x4_##Name(JSContext *cx, unsigned argc, Value *vp);
FLOAT32X4_FUNCTION_LIST(DECLARE_SIMD_FLOAT32X4_FUNCTION)
#undef DECLARE_SIMD_FLOAT32X4_FUNCTION

#define DECLARE_SIMD_INT32x4_FUNCTION(Name, Func, Operands, Flags)   \
extern bool                                                          \
simd_int32x4_##Name(JSContext *cx, unsigned argc, Value *vp);
INT32X4_FUNCTION_LIST(DECLARE_SIMD_INT32x4_FUNCTION)
#undef DECLARE_SIMD_INT32x4_FUNCTION

}  /* namespace js */

JSObject *
js_InitSIMDClass(JSContext *cx, js::HandleObject obj);

#endif /* builtin_SIMD_h */
