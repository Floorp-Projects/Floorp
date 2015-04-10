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
#include "js/Conversions.h"
#include "vm/GlobalObject.h"

/*
 * JS SIMD functions.
 * Spec matching polyfill:
 * https://github.com/johnmccutchan/ecmascript_simd/blob/master/src/ecmascript_simd.js
 */

#define FLOAT32X4_UNARY_FUNCTION_LIST(V)                                              \
  V(abs, (UnaryFunc<Float32x4, Abs, Float32x4>), 1)                                   \
  V(check, (UnaryFunc<Float32x4, Identity, Float32x4>), 1)                            \
  V(fromFloat64x2, (FuncConvert<Float64x2, Float32x4> ), 1)                           \
  V(fromFloat64x2Bits, (FuncConvertBits<Float64x2, Float32x4>), 1)                    \
  V(fromInt32x4, (FuncConvert<Int32x4, Float32x4> ), 1)                               \
  V(fromInt32x4Bits, (FuncConvertBits<Int32x4, Float32x4>), 1)                        \
  V(neg, (UnaryFunc<Float32x4, Neg, Float32x4>), 1)                                   \
  V(not, (CoercedUnaryFunc<Float32x4, Int32x4, Not, Float32x4>), 1)                   \
  V(reciprocalApproximation, (UnaryFunc<Float32x4, RecApprox, Float32x4>), 1)         \
  V(reciprocalSqrtApproximation, (UnaryFunc<Float32x4, RecSqrtApprox, Float32x4>), 1) \
  V(splat, (FuncSplat<Float32x4>), 1)                                                 \
  V(sqrt, (UnaryFunc<Float32x4, Sqrt, Float32x4>), 1)

#define FLOAT32X4_BINARY_FUNCTION_LIST(V)                                             \
  V(add, (BinaryFunc<Float32x4, Add, Float32x4>), 2)                                  \
  V(and, (CoercedBinaryFunc<Float32x4, Int32x4, And, Float32x4>), 2)                  \
  V(div, (BinaryFunc<Float32x4, Div, Float32x4>), 2)                                  \
  V(equal, (CompareFunc<Float32x4, Equal>), 2)                                        \
  V(greaterThan, (CompareFunc<Float32x4, GreaterThan>), 2)                            \
  V(greaterThanOrEqual, (CompareFunc<Float32x4, GreaterThanOrEqual>), 2)              \
  V(lessThan, (CompareFunc<Float32x4, LessThan>), 2)                                  \
  V(lessThanOrEqual, (CompareFunc<Float32x4, LessThanOrEqual>), 2)                    \
  V(load,    (Load<Float32x4, 4>), 2)                                                 \
  V(loadXYZ, (Load<Float32x4, 3>), 2)                                                 \
  V(loadXY,  (Load<Float32x4, 2>), 2)                                                 \
  V(loadX,   (Load<Float32x4, 1>), 2)                                                 \
  V(max, (BinaryFunc<Float32x4, Maximum, Float32x4>), 2)                              \
  V(maxNum, (BinaryFunc<Float32x4, MaxNum, Float32x4>), 2)                            \
  V(min, (BinaryFunc<Float32x4, Minimum, Float32x4>), 2)                              \
  V(minNum, (BinaryFunc<Float32x4, MinNum, Float32x4>), 2)                            \
  V(mul, (BinaryFunc<Float32x4, Mul, Float32x4>), 2)                                  \
  V(notEqual, (CompareFunc<Float32x4, NotEqual>), 2)                                  \
  V(or, (CoercedBinaryFunc<Float32x4, Int32x4, Or, Float32x4>), 2)                    \
  V(store,    (Store<Float32x4, 4>), 3)                                               \
  V(storeXYZ, (Store<Float32x4, 3>), 3)                                               \
  V(storeXY,  (Store<Float32x4, 2>), 3)                                               \
  V(storeX,   (Store<Float32x4, 1>), 3)                                               \
  V(sub, (BinaryFunc<Float32x4, Sub, Float32x4>), 2)                                  \
  V(withX, (FuncWith<Float32x4, WithX>), 2)                                           \
  V(withY, (FuncWith<Float32x4, WithY>), 2)                                           \
  V(withZ, (FuncWith<Float32x4, WithZ>), 2)                                           \
  V(withW, (FuncWith<Float32x4, WithW>), 2)                                           \
  V(xor, (CoercedBinaryFunc<Float32x4, Int32x4, Xor, Float32x4>), 2)

#define FLOAT32X4_TERNARY_FUNCTION_LIST(V)                                            \
  V(bitselect, BitSelect<Float32x4>, 3)                                               \
  V(clamp, Clamp<Float32x4>, 3)                                                       \
  V(select, Select<Float32x4>, 3)

#define FLOAT32X4_SHUFFLE_FUNCTION_LIST(V)                                            \
  V(swizzle, Swizzle<Float32x4>, 2)                                                   \
  V(shuffle, Shuffle<Float32x4>, 3)

#define FLOAT32X4_FUNCTION_LIST(V)                                                    \
  FLOAT32X4_UNARY_FUNCTION_LIST(V)                                                    \
  FLOAT32X4_BINARY_FUNCTION_LIST(V)                                                   \
  FLOAT32X4_TERNARY_FUNCTION_LIST(V)                                                  \
  FLOAT32X4_SHUFFLE_FUNCTION_LIST(V)

#define FLOAT64X2_UNARY_FUNCTION_LIST(V)                                              \
  V(abs, (UnaryFunc<Float64x2, Abs, Float64x2>), 1)                                   \
  V(check, (UnaryFunc<Float64x2, Identity, Float64x2>), 1)                            \
  V(fromFloat32x4, (FuncConvert<Float32x4, Float64x2> ), 1)                           \
  V(fromFloat32x4Bits, (FuncConvertBits<Float32x4, Float64x2>), 1)                    \
  V(fromInt32x4, (FuncConvert<Int32x4, Float64x2> ), 1)                               \
  V(fromInt32x4Bits, (FuncConvertBits<Int32x4, Float64x2>), 1)                        \
  V(neg, (UnaryFunc<Float64x2, Neg, Float64x2>), 1)                                   \
  V(reciprocalApproximation, (UnaryFunc<Float64x2, RecApprox, Float64x2>), 1)         \
  V(reciprocalSqrtApproximation, (UnaryFunc<Float64x2, RecSqrtApprox, Float64x2>), 1) \
  V(splat, (FuncSplat<Float64x2>), 1)                                                 \
  V(sqrt, (UnaryFunc<Float64x2, Sqrt, Float64x2>), 1)

#define FLOAT64X2_BINARY_FUNCTION_LIST(V)                                             \
  V(add, (BinaryFunc<Float64x2, Add, Float64x2>), 2)                                  \
  V(div, (BinaryFunc<Float64x2, Div, Float64x2>), 2)                                  \
  V(equal, (CompareFunc<Float64x2, Equal>), 2)                                        \
  V(greaterThan, (CompareFunc<Float64x2, GreaterThan>), 2)                            \
  V(greaterThanOrEqual, (CompareFunc<Float64x2, GreaterThanOrEqual>), 2)              \
  V(lessThan, (CompareFunc<Float64x2, LessThan>), 2)                                  \
  V(lessThanOrEqual, (CompareFunc<Float64x2, LessThanOrEqual>), 2)                    \
  V(load,    (Load<Float64x2, 2>), 2)                                                 \
  V(loadX,   (Load<Float64x2, 1>), 2)                                                 \
  V(max, (BinaryFunc<Float64x2, Maximum, Float64x2>), 2)                              \
  V(maxNum, (BinaryFunc<Float64x2, MaxNum, Float64x2>), 2)                            \
  V(min, (BinaryFunc<Float64x2, Minimum, Float64x2>), 2)                              \
  V(minNum, (BinaryFunc<Float64x2, MinNum, Float64x2>), 2)                            \
  V(mul, (BinaryFunc<Float64x2, Mul, Float64x2>), 2)                                  \
  V(notEqual, (CompareFunc<Float64x2, NotEqual>), 2)                                  \
  V(store,    (Store<Float64x2, 2>), 3)                                               \
  V(storeX,   (Store<Float64x2, 1>), 3)                                               \
  V(sub, (BinaryFunc<Float64x2, Sub, Float64x2>), 2)                                  \
  V(withX, (FuncWith<Float64x2, WithX>), 2)                                           \
  V(withY, (FuncWith<Float64x2, WithY>), 2)

#define FLOAT64X2_TERNARY_FUNCTION_LIST(V)                                            \
  V(bitselect, BitSelect<Float64x2>, 3)                                               \
  V(clamp, Clamp<Float64x2>, 3)                                                       \
  V(select, Select<Float64x2>, 3)

#define FLOAT64X2_SHUFFLE_FUNCTION_LIST(V)                                            \
  V(swizzle, Swizzle<Float64x2>, 2)                                                   \
  V(shuffle, Shuffle<Float64x2>, 3)

#define FLOAT64X2_FUNCTION_LIST(V)                                                    \
  FLOAT64X2_UNARY_FUNCTION_LIST(V)                                                    \
  FLOAT64X2_BINARY_FUNCTION_LIST(V)                                                   \
  FLOAT64X2_TERNARY_FUNCTION_LIST(V)                                                  \
  FLOAT64X2_SHUFFLE_FUNCTION_LIST(V)

#define INT32X4_UNARY_FUNCTION_LIST(V)                                                \
  V(check, (UnaryFunc<Int32x4, Identity, Int32x4>), 1)                                \
  V(fromFloat32x4, (FuncConvert<Float32x4, Int32x4>), 1)                              \
  V(fromFloat32x4Bits, (FuncConvertBits<Float32x4, Int32x4>), 1)                      \
  V(fromFloat64x2, (FuncConvert<Float64x2, Int32x4>), 1)                              \
  V(fromFloat64x2Bits, (FuncConvertBits<Float64x2, Int32x4>), 1)                      \
  V(neg, (UnaryFunc<Int32x4, Neg, Int32x4>), 1)                                       \
  V(not, (UnaryFunc<Int32x4, Not, Int32x4>), 1)                                       \
  V(splat, (FuncSplat<Int32x4>), 0)

#define INT32X4_BINARY_FUNCTION_LIST(V)                                               \
  V(add, (BinaryFunc<Int32x4, Add, Int32x4>), 2)                                      \
  V(and, (BinaryFunc<Int32x4, And, Int32x4>), 2)                                      \
  V(equal, (CompareFunc<Int32x4, Equal>), 2)                                          \
  V(greaterThan, (CompareFunc<Int32x4, GreaterThan>), 2)                              \
  V(greaterThanOrEqual, (CompareFunc<Int32x4, GreaterThanOrEqual>), 2)                \
  V(lessThan, (CompareFunc<Int32x4, LessThan>), 2)                                    \
  V(lessThanOrEqual, (CompareFunc<Int32x4, LessThanOrEqual>), 2)                      \
  V(load,    (Load<Int32x4, 4>), 2)                                                   \
  V(loadXYZ, (Load<Int32x4, 3>), 2)                                                   \
  V(loadXY,  (Load<Int32x4, 2>), 2)                                                   \
  V(loadX,   (Load<Int32x4, 1>), 2)                                                   \
  V(mul, (BinaryFunc<Int32x4, Mul, Int32x4>), 2)                                      \
  V(notEqual, (CompareFunc<Int32x4, NotEqual>), 2)                                    \
  V(or, (BinaryFunc<Int32x4, Or, Int32x4>), 2)                                        \
  V(sub, (BinaryFunc<Int32x4, Sub, Int32x4>), 2)                                      \
  V(shiftLeftByScalar, (Int32x4BinaryScalar<ShiftLeft>), 2)                           \
  V(shiftRightArithmeticByScalar, (Int32x4BinaryScalar<ShiftRight>), 2)               \
  V(shiftRightLogicalByScalar, (Int32x4BinaryScalar<ShiftRightLogical>), 2)           \
  V(store,    (Store<Int32x4, 4>), 3)                                                 \
  V(storeXYZ, (Store<Int32x4, 3>), 3)                                                 \
  V(storeXY,  (Store<Int32x4, 2>), 3)                                                 \
  V(storeX,   (Store<Int32x4, 1>), 3)                                                 \
  V(withX, (FuncWith<Int32x4, WithX>), 2)                                             \
  V(withY, (FuncWith<Int32x4, WithY>), 2)                                             \
  V(withZ, (FuncWith<Int32x4, WithZ>), 2)                                             \
  V(withW, (FuncWith<Int32x4, WithW>), 2)                                             \
  V(xor, (BinaryFunc<Int32x4, Xor, Int32x4>), 2)

#define INT32X4_TERNARY_FUNCTION_LIST(V)                                              \
  V(bitselect, BitSelect<Int32x4>, 3)                                                 \
  V(select, Select<Int32x4>, 3)

#define INT32X4_QUARTERNARY_FUNCTION_LIST(V)                                          \
  V(bool, Int32x4Bool, 4)

#define INT32X4_SHUFFLE_FUNCTION_LIST(V)                                              \
  V(swizzle, Swizzle<Int32x4>, 2)                                                     \
  V(shuffle, Shuffle<Int32x4>, 3)

#define INT32X4_FUNCTION_LIST(V)                                                      \
  INT32X4_UNARY_FUNCTION_LIST(V)                                                      \
  INT32X4_BINARY_FUNCTION_LIST(V)                                                     \
  INT32X4_TERNARY_FUNCTION_LIST(V)                                                    \
  INT32X4_QUARTERNARY_FUNCTION_LIST(V)                                                \
  INT32X4_SHUFFLE_FUNCTION_LIST(V)

#define CONVERSION_INT32X4_SIMD_OP(_) \
    _(fromFloat32x4)                  \
    _(fromFloat32x4Bits)
#define FOREACH_INT32X4_SIMD_OP(_)   \
    CONVERSION_INT32X4_SIMD_OP(_)    \
    _(shiftLeftByScalar)             \
    _(shiftRightArithmeticByScalar)  \
    _(shiftRightLogicalByScalar)
#define UNARY_ARITH_FLOAT32X4_SIMD_OP(_) \
    _(abs)                           \
    _(sqrt)                          \
    _(reciprocalApproximation)       \
    _(reciprocalSqrtApproximation)
#define BINARY_ARITH_FLOAT32X4_SIMD_OP(_) \
    _(div)                           \
    _(max)                           \
    _(min)                           \
    _(maxNum)                        \
    _(minNum)
#define FOREACH_FLOAT32X4_SIMD_OP(_) \
    UNARY_ARITH_FLOAT32X4_SIMD_OP(_) \
    BINARY_ARITH_FLOAT32X4_SIMD_OP(_)\
    _(fromInt32x4)                   \
    _(fromInt32x4Bits)
#define ARITH_COMMONX4_SIMD_OP(_)    \
    _(add)                           \
    _(sub)                           \
    _(mul)
#define BITWISE_COMMONX4_SIMD_OP(_)  \
    _(and)                           \
    _(or)                            \
    _(xor)
#define COMP_COMMONX4_TO_INT32X4_SIMD_OP(_) \
    _(lessThan)                      \
    _(lessThanOrEqual)               \
    _(equal)                         \
    _(notEqual)                      \
    _(greaterThan)                   \
    _(greaterThanOrEqual)
#define WITH_COMMONX4_SIMD_OP(_)     \
    _(withX)                         \
    _(withY)                         \
    _(withZ)                         \
    _(withW)
// TODO: remove when all SIMD calls are inlined (bug 1112155)
#define ION_COMMONX4_SIMD_OP(_)      \
    ARITH_COMMONX4_SIMD_OP(_)        \
    BITWISE_COMMONX4_SIMD_OP(_)      \
    WITH_COMMONX4_SIMD_OP(_)         \
    _(bitselect)                     \
    _(select)                        \
    _(splat)                         \
    _(not)                           \
    _(neg)                           \
    _(swizzle)                       \
    _(shuffle)                       \
    _(load)                          \
    _(loadX)                         \
    _(loadXY)                        \
    _(loadXYZ)                       \
    _(store)                         \
    _(storeX)                        \
    _(storeXY)                       \
    _(storeXYZ)                      \
    _(check)
#define ION_ONLY_INT32X4_SIMD_OP(_)  \
    _(bool)
#define FOREACH_COMMONX4_SIMD_OP(_)  \
    ION_COMMONX4_SIMD_OP(_)          \
    COMP_COMMONX4_TO_INT32X4_SIMD_OP(_)
#define FORALL_SIMD_OP(_)            \
    FOREACH_INT32X4_SIMD_OP(_)       \
    FOREACH_FLOAT32X4_SIMD_OP(_)     \
    FOREACH_COMMONX4_SIMD_OP(_)

namespace js {

class SIMDObject : public JSObject
{
  public:
    static const Class class_;
    static JSObject* initClass(JSContext* cx, Handle<GlobalObject*> global);
    static bool toString(JSContext* cx, unsigned int argc, jsval* vp);
};

// These classes exist for use with templates below.

struct Float32x4 {
    typedef float Elem;
    static const unsigned lanes = 4;
    static const SimdTypeDescr::Type type = SimdTypeDescr::Float32x4;

    static TypeDescr& GetTypeDescr(GlobalObject& global) {
        return global.float32x4TypeDescr().as<TypeDescr>();
    }
    static Elem toType(Elem a) {
        return a;
    }
    static bool toType(JSContext* cx, JS::HandleValue v, Elem* out) {
        double d;
        if (!ToNumber(cx, v, &d))
            return false;
        *out = float(d);
        return true;
    }
    static void setReturn(CallArgs& args, Elem value) {
        args.rval().setDouble(JS::CanonicalizeNaN(value));
    }
};

struct Float64x2 {
    typedef double Elem;
    static const unsigned lanes = 2;
    static const SimdTypeDescr::Type type = SimdTypeDescr::Float64x2;

    static TypeDescr& GetTypeDescr(GlobalObject& global) {
        return global.float64x2TypeDescr().as<TypeDescr>();
    }
    static Elem toType(Elem a) {
        return a;
    }
    static bool toType(JSContext* cx, JS::HandleValue v, Elem* out) {
        return ToNumber(cx, v, out);
    }
    static void setReturn(CallArgs& args, Elem value) {
        args.rval().setDouble(JS::CanonicalizeNaN(value));
    }
};

struct Int32x4 {
    typedef int32_t Elem;
    static const unsigned lanes = 4;
    static const SimdTypeDescr::Type type = SimdTypeDescr::Int32x4;

    static TypeDescr& GetTypeDescr(GlobalObject& global) {
        return global.int32x4TypeDescr().as<TypeDescr>();
    }
    static Elem toType(Elem a) {
        return JS::ToInt32(a);
    }
    static bool toType(JSContext* cx, JS::HandleValue v, Elem* out) {
        return ToInt32(cx, v, out);
    }
    static void setReturn(CallArgs& args, Elem value) {
        args.rval().setInt32(value);
    }
};

template<typename V>
JSObject* CreateSimd(JSContext* cx, const typename V::Elem* data);

template<typename V>
bool IsVectorObject(HandleValue v);

template<typename V>
bool ToSimdConstant(JSContext* cx, HandleValue v, jit::SimdConstant* out);

#define DECLARE_SIMD_FLOAT32X4_FUNCTION(Name, Func, Operands)   \
extern bool                                                     \
simd_float32x4_##Name(JSContext* cx, unsigned argc, Value* vp);
FLOAT32X4_FUNCTION_LIST(DECLARE_SIMD_FLOAT32X4_FUNCTION)
#undef DECLARE_SIMD_FLOAT32X4_FUNCTION

#define DECLARE_SIMD_FLOAT64X2_FUNCTION(Name, Func, Operands)   \
extern bool                                                     \
simd_float64x2_##Name(JSContext* cx, unsigned argc, Value* vp);
FLOAT64X2_FUNCTION_LIST(DECLARE_SIMD_FLOAT64X2_FUNCTION)
#undef DECLARE_SIMD_FLOAT64X2_FUNCTION

#define DECLARE_SIMD_INT32x4_FUNCTION(Name, Func, Operands)     \
extern bool                                                     \
simd_int32x4_##Name(JSContext* cx, unsigned argc, Value* vp);
INT32X4_FUNCTION_LIST(DECLARE_SIMD_INT32x4_FUNCTION)
#undef DECLARE_SIMD_INT32x4_FUNCTION

JSObject*
InitSIMDClass(JSContext* cx, HandleObject obj);

}  /* namespace js */

#endif /* builtin_SIMD_h */
