/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_SIMD_h
#define builtin_SIMD_h

#include "jsapi.h"
#include "NamespaceImports.h"

#include "builtin/SIMDConstants.h"
#include "jit/IonTypes.h"
#include "js/Conversions.h"

/*
 * JS SIMD functions.
 * Spec matching polyfill:
 * https://github.com/tc39/ecmascript_simd/blob/master/src/ecmascript_simd.js
 */

namespace js {

class GlobalObject;

// These classes implement the concept containing the following constraints:
// - requires typename Elem: this is the scalar lane type, stored in each lane
// of the SIMD vector.
// - requires static const unsigned lanes: this is the number of lanes (length)
// of the SIMD vector.
// - requires static const SimdType type: this is the SimdType enum value
// corresponding to the SIMD type.
// - requires static bool Cast(JSContext*, JS::HandleValue, Elem*): casts a
// given Value to the current scalar lane type and saves it in the Elem
// out-param.
// - requires static Value ToValue(Elem): returns a Value of the right type
// containing the given value.
//
// This concept is used in the templates above to define the functions
// associated to a given type and in their implementations, to avoid code
// redundancy.

struct Float32x4 {
    typedef float Elem;
    static const unsigned lanes = 4;
    static const SimdType type = SimdType::Float32x4;
    static MOZ_MUST_USE bool Cast(JSContext* cx, JS::HandleValue v, Elem* out) {
        double d;
        if (!ToNumber(cx, v, &d))
            return false;
        *out = float(d);
        return true;
    }
    static Value ToValue(Elem value) {
        return DoubleValue(JS::CanonicalizeNaN(value));
    }
};

struct Float64x2 {
    typedef double Elem;
    static const unsigned lanes = 2;
    static const SimdType type = SimdType::Float64x2;
    static MOZ_MUST_USE bool Cast(JSContext* cx, JS::HandleValue v, Elem* out) {
        return ToNumber(cx, v, out);
    }
    static Value ToValue(Elem value) {
        return DoubleValue(JS::CanonicalizeNaN(value));
    }
};

struct Int8x16 {
    typedef int8_t Elem;
    static const unsigned lanes = 16;
    static const SimdType type = SimdType::Int8x16;
    static MOZ_MUST_USE bool Cast(JSContext* cx, JS::HandleValue v, Elem* out) {
        return ToInt8(cx, v, out);
    }
    static Value ToValue(Elem value) {
        return NumberValue(value);
    }
};

struct Int16x8 {
    typedef int16_t Elem;
    static const unsigned lanes = 8;
    static const SimdType type = SimdType::Int16x8;
    static MOZ_MUST_USE bool Cast(JSContext* cx, JS::HandleValue v, Elem* out) {
        return ToInt16(cx, v, out);
    }
    static Value ToValue(Elem value) {
        return NumberValue(value);
    }
};

struct Int32x4 {
    typedef int32_t Elem;
    static const unsigned lanes = 4;
    static const SimdType type = SimdType::Int32x4;
    static MOZ_MUST_USE bool Cast(JSContext* cx, JS::HandleValue v, Elem* out) {
        return ToInt32(cx, v, out);
    }
    static Value ToValue(Elem value) {
        return NumberValue(value);
    }
};

struct Uint8x16 {
    typedef uint8_t Elem;
    static const unsigned lanes = 16;
    static const SimdType type = SimdType::Uint8x16;
    static MOZ_MUST_USE bool Cast(JSContext* cx, JS::HandleValue v, Elem* out) {
        return ToUint8(cx, v, out);
    }
    static Value ToValue(Elem value) {
        return NumberValue(value);
    }
};

struct Uint16x8 {
    typedef uint16_t Elem;
    static const unsigned lanes = 8;
    static const SimdType type = SimdType::Uint16x8;
    static MOZ_MUST_USE bool Cast(JSContext* cx, JS::HandleValue v, Elem* out) {
        return ToUint16(cx, v, out);
    }
    static Value ToValue(Elem value) {
        return NumberValue(value);
    }
};

struct Uint32x4 {
    typedef uint32_t Elem;
    static const unsigned lanes = 4;
    static const SimdType type = SimdType::Uint32x4;
    static MOZ_MUST_USE bool Cast(JSContext* cx, JS::HandleValue v, Elem* out) {
        return ToUint32(cx, v, out);
    }
    static Value ToValue(Elem value) {
        return NumberValue(value);
    }
};

struct Bool8x16 {
    typedef int8_t Elem;
    static const unsigned lanes = 16;
    static const SimdType type = SimdType::Bool8x16;
    static MOZ_MUST_USE bool Cast(JSContext* cx, JS::HandleValue v, Elem* out) {
        *out = ToBoolean(v) ? -1 : 0;
        return true;
    }
    static Value ToValue(Elem value) {
        return BooleanValue(value);
    }
};

struct Bool16x8 {
    typedef int16_t Elem;
    static const unsigned lanes = 8;
    static const SimdType type = SimdType::Bool16x8;
    static MOZ_MUST_USE bool Cast(JSContext* cx, JS::HandleValue v, Elem* out) {
        *out = ToBoolean(v) ? -1 : 0;
        return true;
    }
    static Value ToValue(Elem value) {
        return BooleanValue(value);
    }
};

struct Bool32x4 {
    typedef int32_t Elem;
    static const unsigned lanes = 4;
    static const SimdType type = SimdType::Bool32x4;
    static MOZ_MUST_USE bool Cast(JSContext* cx, JS::HandleValue v, Elem* out) {
        *out = ToBoolean(v) ? -1 : 0;
        return true;
    }
    static Value ToValue(Elem value) {
        return BooleanValue(value);
    }
};

struct Bool64x2 {
    typedef int64_t Elem;
    static const unsigned lanes = 2;
    static const SimdType type = SimdType::Bool64x2;
    static MOZ_MUST_USE bool Cast(JSContext* cx, JS::HandleValue v, Elem* out) {
        *out = ToBoolean(v) ? -1 : 0;
        return true;
    }
    static Value ToValue(Elem value) {
        return BooleanValue(value);
    }
};

// Get the well known name of the SIMD.* object corresponding to type.
PropertyName* SimdTypeToName(const JSAtomState& atoms, SimdType type);

// Check if name is the well known name of a SIMD type.
// Returns true and sets *type iff name is known.
bool IsSimdTypeName(const JSAtomState& atoms, const PropertyName* name, SimdType* type);

const char* SimdTypeToString(SimdType type);

template<typename V>
JSObject* CreateSimd(JSContext* cx, const typename V::Elem* data);

template<typename V>
bool IsVectorObject(HandleValue v);

template<typename V>
MOZ_MUST_USE bool ToSimdConstant(JSContext* cx, HandleValue v, jit::SimdConstant* out);

JSObject*
InitSimdClass(JSContext* cx, Handle<GlobalObject*> global);

namespace jit {

extern const JSJitInfo JitInfo_SimdInt32x4_extractLane;
extern const JSJitInfo JitInfo_SimdFloat32x4_extractLane;

} // namespace jit

#define DECLARE_SIMD_FLOAT32X4_FUNCTION(Name, Func, Operands)   \
extern MOZ_MUST_USE bool                                        \
simd_float32x4_##Name(JSContext* cx, unsigned argc, Value* vp);
FLOAT32X4_FUNCTION_LIST(DECLARE_SIMD_FLOAT32X4_FUNCTION)
#undef DECLARE_SIMD_FLOAT32X4_FUNCTION

#define DECLARE_SIMD_FLOAT64X2_FUNCTION(Name, Func, Operands)   \
extern MOZ_MUST_USE bool                                        \
simd_float64x2_##Name(JSContext* cx, unsigned argc, Value* vp);
FLOAT64X2_FUNCTION_LIST(DECLARE_SIMD_FLOAT64X2_FUNCTION)
#undef DECLARE_SIMD_FLOAT64X2_FUNCTION

#define DECLARE_SIMD_INT8X16_FUNCTION(Name, Func, Operands)     \
extern MOZ_MUST_USE bool                                        \
simd_int8x16_##Name(JSContext* cx, unsigned argc, Value* vp);
INT8X16_FUNCTION_LIST(DECLARE_SIMD_INT8X16_FUNCTION)
#undef DECLARE_SIMD_INT8X16_FUNCTION

#define DECLARE_SIMD_INT16X8_FUNCTION(Name, Func, Operands)     \
extern MOZ_MUST_USE bool                                        \
simd_int16x8_##Name(JSContext* cx, unsigned argc, Value* vp);
INT16X8_FUNCTION_LIST(DECLARE_SIMD_INT16X8_FUNCTION)
#undef DECLARE_SIMD_INT16X8_FUNCTION

#define DECLARE_SIMD_INT32X4_FUNCTION(Name, Func, Operands)     \
extern MOZ_MUST_USE bool                                        \
simd_int32x4_##Name(JSContext* cx, unsigned argc, Value* vp);
INT32X4_FUNCTION_LIST(DECLARE_SIMD_INT32X4_FUNCTION)
#undef DECLARE_SIMD_INT32X4_FUNCTION

#define DECLARE_SIMD_UINT8X16_FUNCTION(Name, Func, Operands)    \
extern MOZ_MUST_USE bool                                        \
simd_uint8x16_##Name(JSContext* cx, unsigned argc, Value* vp);
UINT8X16_FUNCTION_LIST(DECLARE_SIMD_UINT8X16_FUNCTION)
#undef DECLARE_SIMD_UINT8X16_FUNCTION

#define DECLARE_SIMD_UINT16X8_FUNCTION(Name, Func, Operands)    \
extern MOZ_MUST_USE bool                                        \
simd_uint16x8_##Name(JSContext* cx, unsigned argc, Value* vp);
UINT16X8_FUNCTION_LIST(DECLARE_SIMD_UINT16X8_FUNCTION)
#undef DECLARE_SIMD_UINT16X8_FUNCTION

#define DECLARE_SIMD_UINT32X4_FUNCTION(Name, Func, Operands)    \
extern MOZ_MUST_USE bool                                        \
simd_uint32x4_##Name(JSContext* cx, unsigned argc, Value* vp);
UINT32X4_FUNCTION_LIST(DECLARE_SIMD_UINT32X4_FUNCTION)
#undef DECLARE_SIMD_UINT32X4_FUNCTION

#define DECLARE_SIMD_BOOL8X16_FUNCTION(Name, Func, Operands)    \
extern MOZ_MUST_USE bool                                        \
simd_bool8x16_##Name(JSContext* cx, unsigned argc, Value* vp);
BOOL8X16_FUNCTION_LIST(DECLARE_SIMD_BOOL8X16_FUNCTION)
#undef DECLARE_SIMD_BOOL8X16_FUNCTION

#define DECLARE_SIMD_BOOL16X8_FUNCTION(Name, Func, Operands)    \
extern MOZ_MUST_USE bool                                        \
simd_bool16x8_##Name(JSContext* cx, unsigned argc, Value* vp);
BOOL16X8_FUNCTION_LIST(DECLARE_SIMD_BOOL16X8_FUNCTION)
#undef DECLARE_SIMD_BOOL16X8_FUNCTION

#define DECLARE_SIMD_BOOL32X4_FUNCTION(Name, Func, Operands)    \
extern MOZ_MUST_USE bool                                        \
simd_bool32x4_##Name(JSContext* cx, unsigned argc, Value* vp);
BOOL32X4_FUNCTION_LIST(DECLARE_SIMD_BOOL32X4_FUNCTION)
#undef DECLARE_SIMD_BOOL32X4_FUNCTION

#define DECLARE_SIMD_BOOL64X2_FUNCTION(Name, Func, Operands)    \
extern MOZ_MUST_USE bool                                        \
simd_bool64x2_##Name(JSContext* cx, unsigned argc, Value* vp);
BOOL64X2_FUNCTION_LIST(DECLARE_SIMD_BOOL64X2_FUNCTION)
#undef DECLARE_SIMD_BOOL64X2_FUNCTION

}  /* namespace js */

#endif /* builtin_SIMD_h */
