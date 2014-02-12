/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_TypedObjectSimple_h
#define builtin_TypedObjectSimple_h

#include "jsobj.h"

#include "builtin/TypedObjectConstants.h"

/*
 * Simple Typed Objects
 * --------------------
 *
 * Contains definitions for the "simple" type descriptors like scalar
 * and reference types. These are pulled apart from the rest of the
 * typed object primarily so that this file can be included from
 * TypedArrayObject.h, but it also helps to reduce the monolithic
 * nature of TypedObject.h.
 */

namespace js {

class TypeRepresentation;
class ScalarTypeRepresentation;
class ReferenceTypeRepresentation;
class X4TypeRepresentation;
class StructTypeDescr;

/*
 * Helper method for converting a double into other scalar
 * types in the same way that JavaScript would. In particular,
 * simple C casting from double to int32_t gets things wrong
 * for values like 0xF0000000.
 */
template <typename T>
static T ConvertScalar(double d)
{
    if (TypeIsFloatingPoint<T>()) {
        return T(d);
    } else if (TypeIsUnsigned<T>()) {
        uint32_t n = ToUint32(d);
        return T(n);
    } else {
        int32_t n = ToInt32(d);
        return T(n);
    }
}

class TypeDescr : public JSObject
{
  public:
    enum Kind {
        Scalar = JS_TYPEREPR_SCALAR_KIND,
        Reference = JS_TYPEREPR_REFERENCE_KIND,
        X4 = JS_TYPEREPR_X4_KIND,
        Struct = JS_TYPEREPR_STRUCT_KIND,
        SizedArray = JS_TYPEREPR_SIZED_ARRAY_KIND,
        UnsizedArray = JS_TYPEREPR_UNSIZED_ARRAY_KIND,
    };

    static bool isSized(Kind kind) {
        return kind > JS_TYPEREPR_MAX_UNSIZED_KIND;
    }

    JSObject &typeRepresentationOwnerObj() const {
        return getReservedSlot(JS_DESCR_SLOT_TYPE_REPR).toObject();
    }

    TypeRepresentation *typeRepresentation() const;

    TypeDescr::Kind kind() const;

    bool opaque() const;

    size_t alignment() {
        return getReservedSlot(JS_DESCR_SLOT_ALIGNMENT).toInt32();
    }
};

typedef Handle<TypeDescr*> HandleTypeDescr;

class SizedTypeDescr : public TypeDescr
{
  public:
    size_t size() {
        return getReservedSlot(JS_DESCR_SLOT_SIZE).toInt32();
    }
};

typedef Handle<SizedTypeDescr*> HandleSizedTypeDescr;

class SimpleTypeDescr : public SizedTypeDescr
{
};

// Type for scalar type constructors like `uint8`. All such type
// constructors share a common js::Class and JSFunctionSpec. Scalar
// types are non-opaque (their storage is visible unless combined with
// an opaque reference type.)
class ScalarTypeDescr : public SimpleTypeDescr
{
  public:
    // Must match order of JS_FOR_EACH_SCALAR_TYPE_REPR below
    enum Type {
        TYPE_INT8 = JS_SCALARTYPEREPR_INT8,
        TYPE_UINT8 = JS_SCALARTYPEREPR_UINT8,
        TYPE_INT16 = JS_SCALARTYPEREPR_INT16,
        TYPE_UINT16 = JS_SCALARTYPEREPR_UINT16,
        TYPE_INT32 = JS_SCALARTYPEREPR_INT32,
        TYPE_UINT32 = JS_SCALARTYPEREPR_UINT32,
        TYPE_FLOAT32 = JS_SCALARTYPEREPR_FLOAT32,
        TYPE_FLOAT64 = JS_SCALARTYPEREPR_FLOAT64,

        /*
         * Special type that's a uint8_t, but assignments are clamped to 0 .. 255.
         * Treat the raw data type as a uint8_t.
         */
        TYPE_UINT8_CLAMPED = JS_SCALARTYPEREPR_UINT8_CLAMPED,
    };
    static const int32_t TYPE_MAX = TYPE_UINT8_CLAMPED + 1;

    static size_t size(Type t);
    static size_t alignment(Type t);
    static const char *typeName(Type type);

    static const Class class_;
    static const JSFunctionSpec typeObjectMethods[];
    typedef ScalarTypeRepresentation TypeRepr;

    ScalarTypeDescr::Type type() const {
        return (ScalarTypeDescr::Type) getReservedSlot(JS_DESCR_SLOT_TYPE).toInt32();
    }

    static bool call(JSContext *cx, unsigned argc, Value *vp);
};

// Enumerates the cases of ScalarTypeDescr::Type which have
// unique C representation. In particular, omits Uint8Clamped since it
// is just a Uint8.
#define JS_FOR_EACH_UNIQUE_SCALAR_TYPE_REPR_CTYPE(macro_)                     \
    macro_(ScalarTypeDescr::TYPE_INT8,    int8_t,   int8)            \
    macro_(ScalarTypeDescr::TYPE_UINT8,   uint8_t,  uint8)           \
    macro_(ScalarTypeDescr::TYPE_INT16,   int16_t,  int16)           \
    macro_(ScalarTypeDescr::TYPE_UINT16,  uint16_t, uint16)          \
    macro_(ScalarTypeDescr::TYPE_INT32,   int32_t,  int32)           \
    macro_(ScalarTypeDescr::TYPE_UINT32,  uint32_t, uint32)          \
    macro_(ScalarTypeDescr::TYPE_FLOAT32, float,    float32)         \
    macro_(ScalarTypeDescr::TYPE_FLOAT64, double,   float64)

// Must be in same order as the enum ScalarTypeDescr::Type:
#define JS_FOR_EACH_SCALAR_TYPE_REPR(macro_)                                    \
    JS_FOR_EACH_UNIQUE_SCALAR_TYPE_REPR_CTYPE(macro_)                           \
    macro_(ScalarTypeDescr::TYPE_UINT8_CLAMPED, uint8_t, uint8Clamped)

// Type for reference type constructors like `Any`, `String`, and
// `Object`. All such type constructors share a common js::Class and
// JSFunctionSpec. All these types are opaque.
class ReferenceTypeDescr : public SimpleTypeDescr
{
  public:
    // Must match order of JS_FOR_EACH_REFERENCE_TYPE_REPR below
    enum Type {
        TYPE_ANY = JS_REFERENCETYPEREPR_ANY,
        TYPE_OBJECT = JS_REFERENCETYPEREPR_OBJECT,
        TYPE_STRING = JS_REFERENCETYPEREPR_STRING,
    };
    static const int32_t TYPE_MAX = TYPE_STRING + 1;
    static const char *typeName(Type type);

    static const Class class_;
    static const JSFunctionSpec typeObjectMethods[];
    typedef ReferenceTypeRepresentation TypeRepr;

    ReferenceTypeDescr::Type type() const {
        return (ReferenceTypeDescr::Type) getReservedSlot(JS_DESCR_SLOT_TYPE).toInt32();
    }

    static bool call(JSContext *cx, unsigned argc, Value *vp);
};

#define JS_FOR_EACH_REFERENCE_TYPE_REPR(macro_)                    \
    macro_(ReferenceTypeDescr::TYPE_ANY,    HeapValue, Any)        \
    macro_(ReferenceTypeDescr::TYPE_OBJECT, HeapPtrObject, Object) \
    macro_(ReferenceTypeDescr::TYPE_STRING, HeapPtrString, string)

/*
 * Type descriptors `float32x4` and `int32x4`
 */
class X4TypeDescr : public SizedTypeDescr
{
  public:
    enum Type {
        TYPE_INT32 = JS_X4TYPEREPR_INT32,
        TYPE_FLOAT32 = JS_X4TYPEREPR_FLOAT32,
    };

    static const Class class_;
    typedef X4TypeRepresentation TypeRepr;

    X4TypeDescr::Type type() const {
        return (X4TypeDescr::Type) getReservedSlot(JS_DESCR_SLOT_TYPE).toInt32();
    }

    static bool call(JSContext *cx, unsigned argc, Value *vp);
    static bool is(const Value &v);
};

#define JS_FOR_EACH_X4_TYPE_REPR(macro_)                             \
    macro_(X4TypeDescr::TYPE_INT32, int32_t, int32)                  \
    macro_(X4TypeDescr::TYPE_FLOAT32, float, float32)

bool IsTypedDatumClass(const Class *clasp); // Defined in TypedArrayObject.h

} // namespace js

#endif
