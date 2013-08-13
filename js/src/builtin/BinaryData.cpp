/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/BinaryData.h"

#include "jscompartment.h"
#include "jsfun.h"
#include "jsobj.h"
#include "jsutil.h"

#include "builtin/TypeRepresentation.h"
#include "gc/Marking.h"
#include "js/Vector.h"
#include "vm/GlobalObject.h"
#include "vm/ObjectImpl.h"
#include "vm/String.h"
#include "vm/StringBuffer.h"
#include "vm/TypedArrayObject.h"

#include "jsatominlines.h"
#include "jsobjinlines.h"

using mozilla::DebugOnly;

using namespace js;

/*
 * Reify() converts a binary value into a JS Object.
 *
 * The type of the value to be converted is described by `typeRepr`,
 * and `type` is an associated type descriptor object with that
 * same representation (note that there may be many descriptors
 * that all share the same `typeRepr`).
 *
 * The value is located at the offset `offset` in the binary block
 * associated with `owner`, which must be a BinaryBlock object.
 *
 * Upon success, the result is stored in `vp`.
 */
static bool Reify(JSContext *cx, TypeRepresentation *typeRepr, HandleObject type,
                  HandleObject owner, size_t offset, MutableHandleValue vp);

/*
 * ConvertAndCopyTo() converts `from` to type `type` and stores the result in
 * `mem`, which MUST be pre-allocated to the appropriate size for instances of
 * `type`.
 */
static bool ConvertAndCopyTo(JSContext *cx, TypeRepresentation *typeRepr,
                             HandleValue from, uint8_t *mem);

static bool
TypeThrowError(JSContext *cx, unsigned argc, Value *vp)
{
    return ReportIsNotFunction(cx, *vp);
}

static bool
DataThrowError(JSContext *cx, unsigned argc, Value *vp)
{
    return ReportIsNotFunction(cx, *vp);
}

static void
ReportCannotConvertTo(JSContext *cx, HandleValue fromValue, const char *toType)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_CONVERT_TO,
                         InformalValueTypeName(fromValue), toType);
}

static void
ReportCannotConvertTo(JSContext *cx, HandleObject fromObject, const char *toType)
{
    RootedValue fromValue(cx, ObjectValue(*fromObject));
    ReportCannotConvertTo(cx, fromValue, toType);
}

static void
ReportCannotConvertTo(JSContext *cx, HandleValue fromValue, HandleString toType)
{
    const char *fnName = JS_EncodeString(cx, toType);
    if (!fnName)
        return;
    ReportCannotConvertTo(cx, fromValue, fnName);
    JS_free(cx, (void *) fnName);
}

static void
ReportCannotConvertTo(JSContext *cx, HandleValue fromValue, TypeRepresentation *toType)
{
    StringBuffer contents(cx);
    if (!toType->appendString(cx, contents))
        return;

    RootedString result(cx, contents.finishString());
    if (!result)
        return;

    ReportCannotConvertTo(cx, fromValue, result);
}

static int32_t
Clamp(int32_t value, int32_t min, int32_t max)
{
    JS_ASSERT(min < max);
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

static inline JSObject *
ToObjectIfObject(HandleValue value)
{
    if (!value.isObject())
        return NULL;

    return &value.toObject();
}

static inline bool
IsNumericType(HandleObject type)
{
    return type &&
           &NumericTypeClasses[0] <= type->getClass() &&
           type->getClass() < &NumericTypeClasses[ScalarTypeRepresentation::TYPE_MAX];
}

static inline bool
IsArrayType(HandleObject type)
{
    return type && type->hasClass(&ArrayType::class_);
}

static inline bool
IsStructType(HandleObject type)
{
    return type && type->hasClass(&StructType::class_);
}

static inline bool
IsComplexType(HandleObject type)
{
    return IsArrayType(type) || IsStructType(type);
}

static inline bool
IsBinaryType(HandleObject type)
{
    return IsNumericType(type) || IsComplexType(type);
}

static inline bool
IsBlock(HandleObject obj)
{
    return obj && obj->hasClass(&BinaryBlock::class_);
}

static inline uint8_t *
BlockMem(HandleObject val)
{
    JS_ASSERT(IsBlock(val));
    return (uint8_t*) val->getPrivate();
}

/*
 * Given a user-visible type descriptor object, returns the
 * owner object for the TypeRepresentation* that we use internally.
 */
static JSObject *
typeRepresentationOwnerObj(HandleObject typeObj)
{
    JS_ASSERT(IsBinaryType(typeObj));
    return &typeObj->getFixedSlot(SLOT_TYPE_REPR).toObject();
}

/*
 * Given a user-visible type descriptor object, returns the
 * TypeRepresentation* that we use internally.
 *
 * Note: this pointer is valid only so long as `typeObj` remains rooted.
 */
static TypeRepresentation *
typeRepresentation(HandleObject typeObj)
{
    return TypeRepresentation::fromOwnerObject(typeRepresentationOwnerObj(typeObj));
}

static inline JSObject *
GetType(HandleObject block)
{
    JS_ASSERT(IsBlock(block));
    return &block->getFixedSlot(SLOT_DATATYPE).toObject();
}

/*
 * Overwrites the contents of `block` with the converted form of `val`
 */
static bool
ConvertAndCopyTo(JSContext *cx, HandleValue val, HandleObject block)
{
    uint8_t *memory = BlockMem(block);
    RootedObject type(cx, GetType(block));
    TypeRepresentation *typeRepr = typeRepresentation(type);
    return ConvertAndCopyTo(cx, typeRepr, val, memory);
}

static inline size_t
TypeSize(HandleObject type)
{
    return typeRepresentation(type)->size();
}

static inline size_t
BlockSize(JSContext *cx, HandleObject val)
{
    RootedObject type(cx, GetType(val));
    return TypeSize(type);
}

static inline bool
IsBlockOfKind(JSContext *cx, HandleObject obj, TypeRepresentation::Kind kind)
{
    if (!IsBlock(obj))
        return false;
    RootedObject objType(cx, GetType(obj));
    TypeRepresentation *repr = typeRepresentation(objType);
    return repr->kind() == kind;
}

static inline bool
IsBinaryArray(JSContext *cx, HandleObject obj)
{
    return IsBlockOfKind(cx, obj, TypeRepresentation::Array);
}

static inline bool
IsBinaryStruct(JSContext *cx, HandleObject obj)
{
    return IsBlockOfKind(cx, obj, TypeRepresentation::Struct);
}

Class js::DataClass = {
    "Data",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Data),
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

Class js::TypeClass = {
    "Type",
    JSCLASS_HAS_CACHED_PROTO(JSProto_Type),
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

static bool
TypeEquivalent(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject thisObj(cx, ToObjectIfObject(args.thisv()));
    if (!thisObj || !IsBinaryType(thisObj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_INCOMPATIBLE_PROTO,
                             "Type", "equivalent",
                             InformalValueTypeName(args.thisv()));
        return false;
    }

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             "Type.equivalent", "1", "s");
        return false;
    }

    RootedObject otherObj(cx, ToObjectIfObject(args[0]));
    if (!otherObj || !IsBinaryType(otherObj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BINARYDATA_NOT_TYPE_OBJECT);
        return false;
    }

    TypeRepresentation *thisRepr = typeRepresentation(thisObj);
    TypeRepresentation *otherRepr = typeRepresentation(otherObj);
    args.rval().setBoolean(thisRepr == otherRepr);
    return true;
}

#define BINARYDATA_NUMERIC_CLASSES(constant_, type_, name_)                   \
{                                                                             \
    #name_,                                                                   \
    JSCLASS_HAS_RESERVED_SLOTS(1) |                                           \
    JSCLASS_HAS_CACHED_PROTO(JSProto_##name_),                                \
    JS_PropertyStub,       /* addProperty */                                  \
    JS_DeletePropertyStub, /* delProperty */                                  \
    JS_PropertyStub,       /* getProperty */                                  \
    JS_StrictPropertyStub, /* setProperty */                                  \
    JS_EnumerateStub,                                                         \
    JS_ResolveStub,                                                           \
    JS_ConvertStub,                                                           \
    NULL,                                                                     \
    NULL,                                                                     \
    NumericType<constant_, type_>::call,                                      \
    NULL,                                                                     \
    NULL,                                                                     \
    NULL                                                                      \
},

Class js::NumericTypeClasses[ScalarTypeRepresentation::TYPE_MAX] = {
    JS_FOR_EACH_SCALAR_TYPE_REPR(BINARYDATA_NUMERIC_CLASSES)
};

template <typename Domain, typename Input>
bool
InRange(Input x)
{
    return std::numeric_limits<Domain>::min() <= x &&
           x <= std::numeric_limits<Domain>::max();
}

template <>
bool
InRange<float, int>(int x)
{
    return -std::numeric_limits<float>::max() <= x &&
           x <= std::numeric_limits<float>::max();
}

template <>
bool
InRange<double, int>(int x)
{
    return -std::numeric_limits<double>::max() <= x &&
           x <= std::numeric_limits<double>::max();
}

template <>
bool
InRange<float, double>(double x)
{
    return -std::numeric_limits<float>::max() <= x &&
           x <= std::numeric_limits<float>::max();
}

template <>
bool
InRange<double, double>(double x)
{
    return -std::numeric_limits<double>::max() <= x &&
           x <= std::numeric_limits<double>::max();
}

#define BINARYDATA_TYPE_TO_CLASS(constant_, type_, name_)                     \
    template <>                                                               \
    Class *                                                                   \
    NumericType<constant_, type_>::typeToClass()                              \
    {                                                                         \
        return &NumericTypeClasses[constant_];                                \
    }

/**
 * This namespace declaration is required because of a weird 'specialization in
 * different namespace' error that happens in gcc, only on type specialized
 * template functions.
 */
namespace js {
JS_FOR_EACH_SCALAR_TYPE_REPR(BINARYDATA_TYPE_TO_CLASS);

template <ScalarTypeRepresentation::Type type, typename T>
JS_ALWAYS_INLINE
bool NumericType<type, T>::reify(JSContext *cx, void *mem, MutableHandleValue vp)
{
    vp.setNumber((double)*((T*)mem) );
    return true;
}

template <ScalarTypeRepresentation::Type type, typename T>
bool
NumericType<type, T>::convert(JSContext *cx, HandleValue val, T* converted)
{
    if (val.isInt32()) {
        *converted = T(val.toInt32());
        return true;
    }

    double d;
    if (!ToDoubleForTypedArray(cx, val, &d))
        return false;

    if (TypeIsFloatingPoint<T>()) {
        *converted = T(d);
    } else if (TypeIsUnsigned<T>()) {
        uint32_t n = ToUint32(d);
        *converted = T(n);
    } else {
        int32_t n = ToInt32(d);
        *converted = T(n);
    }

    return true;
}

template <>
bool
NumericType<ScalarTypeRepresentation::TYPE_UINT8_CLAMPED, uint8_t>::convert(
    JSContext *cx, HandleValue val, uint8_t* converted)
{
    double d;
    if (val.isInt32()) {
        d = val.toInt32();
    } else {
        if (!ToDoubleForTypedArray(cx, val, &d))
            return false;
    }
    *converted = ClampDoubleToUint8(d);
    return true;
}


} // namespace js

static bool
ConvertAndCopyScalarTo(JSContext *cx, ScalarTypeRepresentation *typeRepr,
                       HandleValue from, uint8_t *mem)
{
#define CONVERT_CASES(constant_, type_, name_)                                \
    case constant_: {                                                         \
        type_ temp;                                                           \
        if (!NumericType<constant_, type_>::convert(cx, from, &temp))         \
            return false;                                                     \
        memcpy(mem, &temp, sizeof(type_));                                    \
        return true; }

    switch (typeRepr->type()) {
        JS_FOR_EACH_SCALAR_TYPE_REPR(CONVERT_CASES)
    }
#undef CONVERT_CASES
    return false;
}

static bool
ReifyScalar(JSContext *cx, ScalarTypeRepresentation *typeRepr, HandleObject type,
            HandleObject owner, size_t offset, MutableHandleValue to)
{
    JS_ASSERT(&NumericTypeClasses[0] <= type->getClass() &&
              type->getClass() <= &NumericTypeClasses[ScalarTypeRepresentation::TYPE_MAX]);

    switch (typeRepr->type()) {
#define REIFY_CASES(constant_, type_, name_)                                  \
        case constant_:                                                       \
          return NumericType<constant_, type_>::reify(                        \
                cx, BlockMem(owner) + offset, to);
        JS_FOR_EACH_SCALAR_TYPE_REPR(REIFY_CASES)
    }
#undef REIFY_CASES

    MOZ_ASSUME_UNREACHABLE("Invalid type");
    return false;
}

template <ScalarTypeRepresentation::Type type, typename T>
bool
NumericType<type, T>::call(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             args.callee().getClass()->name, "0", "s");
        return false;
    }

    RootedValue arg(cx, args[0]);
    T answer;
    if (!convert(cx, arg, &answer))
        return false; // convert() raises TypeError.

    RootedValue reified(cx);
    if (!NumericType<type, T>::reify(cx, &answer, &reified)) {
        return false;
    }

    args.rval().set(reified);
    return true;
}

template<ScalarTypeRepresentation::Type N>
bool
NumericTypeToString(JSContext *cx, unsigned int argc, Value *vp)
{
    JS_STATIC_ASSERT(N < ScalarTypeRepresentation::TYPE_MAX);
    CallArgs args = CallArgsFromVp(argc, vp);
    JSString *s = JS_NewStringCopyZ(cx, ScalarTypeRepresentation::typeName(N));
    if (!s)
        return false;
    args.rval().set(StringValue(s));
    return true;
}

/*
 * When creating:
 *   var A = new ArrayType(uint8, 10)
 * or
 *   var S = new StructType({...})
 *
 * A.prototype.__proto__ === ArrayType.prototype.prototype (and similar for
 * StructType).
 *
 * This function takes a reference to either ArrayType or StructType and
 * returns a JSObject which can be set as A.prototype.
 */
JSObject *
SetupAndGetPrototypeObjectForComplexTypeInstance(JSContext *cx,
                                                 HandleObject complexTypeGlobal)
{
    RootedObject global(cx, cx->compartment()->maybeGlobal());
    RootedValue complexTypePrototypeVal(cx);
    RootedValue complexTypePrototypePrototypeVal(cx);

    if (!JSObject::getProperty(cx, complexTypeGlobal, complexTypeGlobal,
                               cx->names().classPrototype, &complexTypePrototypeVal))
        return NULL;

    JS_ASSERT(complexTypePrototypeVal.isObject()); // immutable binding
    RootedObject complexTypePrototypeObj(cx,
        &complexTypePrototypeVal.toObject());

    if (!JSObject::getProperty(cx, complexTypePrototypeObj,
                               complexTypePrototypeObj,
                               cx->names().classPrototype,
                               &complexTypePrototypePrototypeVal))
        return NULL;

    RootedObject prototypeObj(cx,
        NewObjectWithGivenProto(cx, &JSObject::class_, NULL, global));

    JS_ASSERT(complexTypePrototypePrototypeVal.isObject()); // immutable binding
    RootedObject proto(cx, &complexTypePrototypePrototypeVal.toObject());
    if (!JS_SetPrototype(cx, prototypeObj, proto))
        return NULL;

    return prototypeObj;
}

Class ArrayType::class_ = {
    "ArrayType",
    JSCLASS_HAS_RESERVED_SLOTS(ARRAY_TYPE_RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_ArrayType),
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,
    NULL,
    NULL,
    NULL,
    BinaryBlock::construct,
    NULL
};

static JSObject *
ArrayElementType(HandleObject array)
{
    JS_ASSERT(IsArrayType(array));
    return &array->getFixedSlot(SLOT_ARRAY_ELEM_TYPE).toObject();
}

static bool
ConvertAndCopyArrayTo(JSContext *cx, ArrayTypeRepresentation *typeRepr,
                      HandleValue from, uint8_t *mem)
{
    if (!from.isObject()) {
        ReportCannotConvertTo(cx, from, typeRepr);
        return false;
    }

    // Check for the fast case, where we can just memcpy:
    RootedObject fromObject(cx, &from.toObject());
    if (IsBlock(fromObject)) {
        RootedObject fromType(cx, GetType(fromObject));
        TypeRepresentation *fromTypeRepr = typeRepresentation(fromType);
        if (typeRepr == fromTypeRepr) {
            memcpy(mem, BlockMem(fromObject), typeRepr->size());
            return true;
        }
    }

    // Otherwise, use a structural comparison:
    RootedValue fromLenVal(cx);
    if (!JSObject::getProperty(cx, fromObject, fromObject,
                               cx->names().length, &fromLenVal))
        return false;

    if (!fromLenVal.isInt32()) {
        ReportCannotConvertTo(cx, from, typeRepr);
        return false;
    }

    int32_t fromLenInt32 = fromLenVal.toInt32();
    if (fromLenInt32 < 0) {
        ReportCannotConvertTo(cx, from, typeRepr);
        return false;
    }

    size_t fromLen = (size_t) fromLenInt32;
    if (typeRepr->length() != fromLen) {
        ReportCannotConvertTo(cx, from, typeRepr);
        return false;
    }

    TypeRepresentation *elementType = typeRepr->element();
    uint8_t *p = mem;

    RootedValue fromElem(cx);
    for (size_t i = 0; i < fromLen; i++) {
        if (!JSObject::getElement(cx, fromObject, fromObject, i, &fromElem))
            return false;

        if (!ConvertAndCopyTo(cx, elementType, fromElem, p))
            return false;

        p += elementType->size();
    }

    return true;
}

static bool
DataInstanceUpdate(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             NULL, JSMSG_MORE_ARGS_NEEDED,
                             "update()", "0", "s");
        return false;
    }

    RootedObject thisObj(cx, ToObjectIfObject(args.thisv()));
    if (!thisObj || !IsBlock(thisObj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_INCOMPATIBLE_PROTO,
                             "Data", "update",
                             InformalValueTypeName(args.thisv()));
        return false;
    }

    RootedValue val(cx, args[0]);
    if (!ConvertAndCopyTo(cx, val, thisObj))
        return false;

    args.rval().setUndefined();
    return true;
}

static bool
FillBinaryArrayWithValue(JSContext *cx, HandleObject array, HandleValue val)
{
    JS_ASSERT(IsBinaryArray(cx, array));

    // set array[0] = [[Convert]](val)
    RootedObject type(cx, GetType(array));
    ArrayTypeRepresentation *typeRepr = typeRepresentation(type)->asArray();
    uint8_t *base = BlockMem(array);
    if (!ConvertAndCopyTo(cx, typeRepr->element(), val, base))
        return false;

    // copy a[0] into remaining indices.
    size_t elementSize = typeRepr->element()->size();
    for (size_t i = 1; i < typeRepr->length(); i++) {
        uint8_t *dest = base + elementSize * i;
        memcpy(dest, base, elementSize);
    }

    return true;
}

static bool
ArrayRepeat(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             NULL, JSMSG_MORE_ARGS_NEEDED,
                             "repeat()", "0", "s");
        return false;
    }

    RootedObject thisObj(cx, ToObjectIfObject(args.thisv()));
    if (!thisObj || !IsArrayType(thisObj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_INCOMPATIBLE_PROTO,
                             "ArrayType", "repeat",
                             InformalValueTypeName(args.thisv()));
        return false;
    }

    RootedObject binaryArray(cx, BinaryBlock::createZeroed(cx, thisObj));
    if (!binaryArray)
        return false;

    RootedValue val(cx, args[0]);
    if (!FillBinaryArrayWithValue(cx, binaryArray, val))
        return false;

    args.rval().setObject(*binaryArray);
    return true;
}

bool
ArrayType::toSource(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject thisObj(cx, ToObjectIfObject(args.thisv()));
    if (!thisObj || !IsArrayType(thisObj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "ArrayType", "toSource",
                             InformalValueTypeName(args.thisv()));
        return false;
    }

    StringBuffer contents(cx);
    if (!typeRepresentation(thisObj)->appendString(cx, contents))
        return false;

    RootedString result(cx, contents.finishString());
    if (!result)
        return false;

    args.rval().setString(result);
    return true;
}

/**
 * The subarray function first creates an ArrayType instance
 * which will act as the elementType for the subarray.
 *
 * var MA = new ArrayType(elementType, 10);
 * var mb = MA.repeat(val);
 *
 * mb.subarray(begin, end=mb.length) => (Only for +ve)
 *     var internalSA = new ArrayType(elementType, end-begin);
 *     var ret = new internalSA()
 *     for (var i = begin; i < end; i++)
 *         ret[i-begin] = ret[i]
 *     return ret
 *
 * The range specified by the begin and end values is clamped to the valid
 * index range for the current array. If the computed length of the new
 * TypedArray would be negative, it is clamped to zero.
 * see: http://www.khronos.org/registry/typedarray/specs/latest/#7
 *
 */
static bool
ArraySubarray(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             NULL, JSMSG_MORE_ARGS_NEEDED,
                             "subarray()", "0", "s");
        return false;
    }

    if (!args[0].isInt32()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BINARYDATA_SUBARRAY_INTEGER_ARG, "1");
        return false;
    }

    RootedObject thisObj(cx, &args.thisv().toObject());
    if (!IsBinaryArray(cx, thisObj)) {
        ReportCannotConvertTo(cx, thisObj, "binary array");
        return false;
    }

    RootedObject type(cx, GetType(thisObj));
    ArrayTypeRepresentation *typeRepr = typeRepresentation(type)->asArray();
    size_t length = typeRepr->length();

    int32_t begin = args[0].toInt32();
    int32_t end = length;

    if (args.length() >= 2) {
        if (!args[1].isInt32()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                    JSMSG_BINARYDATA_SUBARRAY_INTEGER_ARG, "2");
            return false;
        }

        end = args[1].toInt32();
    }

    if (begin < 0)
        begin = length + begin;
    if (end < 0)
        end = length + end;

    begin = Clamp(begin, 0, length);
    end = Clamp(end, 0, length);

    int32_t sublength = end - begin; // end exclusive
    sublength = Clamp(sublength, 0, length);

    RootedObject globalObj(cx, cx->compartment()->maybeGlobal());
    JS_ASSERT(globalObj);
    Rooted<GlobalObject*> global(cx, &globalObj->as<GlobalObject>());
    RootedObject arrayTypeGlobal(cx, global->getOrCreateArrayTypeObject(cx));

    RootedObject elementType(cx, ArrayElementType(type));
    RootedObject subArrayType(cx, ArrayType::create(cx, arrayTypeGlobal,
                                                    elementType, sublength));
    if (!subArrayType)
        return false;

    int32_t elementSize = typeRepr->element()->size();
    size_t offset = elementSize * begin;

    RootedObject subarray(cx, BinaryBlock::createDerived(cx, subArrayType, thisObj, offset));
    if (!subarray)
        return false;

    args.rval().setObject(*subarray);
    return true;
}

static bool
ArrayFillSubarray(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             NULL, JSMSG_MORE_ARGS_NEEDED,
                             "fill()", "0", "s");
        return false;
    }

    RootedObject thisObj(cx, ToObjectIfObject(args.thisv()));
    if (!thisObj || !IsBinaryArray(cx, thisObj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "ArrayType", "fill",
                             InformalValueTypeName(args.thisv()));
        return false;
    }

    Value funArrayTypeVal = GetFunctionNativeReserved(&args.callee(), 0);
    JS_ASSERT(funArrayTypeVal.isObject());

    RootedObject type(cx, GetType(thisObj));
    TypeRepresentation *typeRepr = typeRepresentation(type);
    RootedObject funArrayType(cx, &funArrayTypeVal.toObject());
    TypeRepresentation *funArrayTypeRepr = typeRepresentation(funArrayType);
    if (typeRepr != funArrayTypeRepr) {
        RootedValue thisObjVal(cx, ObjectValue(*thisObj));
        ReportCannotConvertTo(cx, thisObjVal, funArrayTypeRepr);
        return false;
    }

    args.rval().setUndefined();
    RootedValue val(cx, args[0]);
    return FillBinaryArrayWithValue(cx, thisObj, val);
}

static bool
InitializeCommonTypeDescriptorProperties(JSContext *cx,
                                         HandleObject obj,
                                         HandleObject typeReprOwnerObj)
{
    TypeRepresentation *typeRepr =
        TypeRepresentation::fromOwnerObject(typeReprOwnerObj);

    // byteLength
    RootedValue typeByteLength(cx, NumberValue(typeRepr->size()));
    if (!JSObject::defineProperty(cx, obj, cx->names().byteLength,
                                  typeByteLength,
                                  NULL, NULL,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return false;

    // byteAlignment
    RootedValue typeByteAlignment(cx, NumberValue(typeRepr->alignment()));
    if (!JSObject::defineProperty(cx, obj, cx->names().byteAlignment,
                                  typeByteAlignment,
                                  NULL, NULL,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return false;

    // variable -- always false since we do not yet support variable-size types
    RootedValue variable(cx, JSVAL_FALSE);
    if (!JSObject::defineProperty(cx, obj, cx->names().variable,
                                  variable,
                                  NULL, NULL,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return false;

    return true;
}

JSObject *
ArrayType::create(JSContext *cx, HandleObject arrayTypeGlobal,
                  HandleObject elementType, size_t length)
{
    JS_ASSERT(elementType);
    JS_ASSERT(IsBinaryType(elementType));

    TypeRepresentation *elementTypeRepr = typeRepresentation(elementType);
    RootedObject typeReprObj(
        cx,
        ArrayTypeRepresentation::Create(cx, elementTypeRepr, length));
    if (!typeReprObj)
        return NULL;

    RootedObject obj(cx, NewBuiltinClassInstance(cx, &ArrayType::class_));
    if (!obj)
        return NULL;
    obj->setFixedSlot(SLOT_TYPE_REPR, ObjectValue(*typeReprObj));

    RootedValue elementTypeVal(cx, ObjectValue(*elementType));
    if (!JSObject::defineProperty(cx, obj, cx->names().elementType,
                                  elementTypeVal, NULL, NULL,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return NULL;

    obj->setFixedSlot(SLOT_ARRAY_ELEM_TYPE, elementTypeVal);

    RootedValue lengthVal(cx, Int32Value(length));
    if (!JSObject::defineProperty(cx, obj, cx->names().length,
                                  lengthVal, NULL, NULL,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return NULL;

    if (!InitializeCommonTypeDescriptorProperties(cx, obj, typeReprObj))
        return NULL;

    RootedObject prototypeObj(cx,
        SetupAndGetPrototypeObjectForComplexTypeInstance(cx, arrayTypeGlobal));

    if (!prototypeObj)
        return NULL;

    if (!LinkConstructorAndPrototype(cx, obj, prototypeObj))
        return NULL;

    JSFunction *fillFun = DefineFunctionWithReserved(cx, prototypeObj, "fill", ArrayFillSubarray, 1, 0);
    if (!fillFun)
        return NULL;

    // This is important
    // so that A.prototype.fill.call(b, val)
    // where b.type != A raises an error
    SetFunctionNativeReserved(fillFun, 0, ObjectValue(*obj));

    return obj;
}

bool
ArrayType::construct(JSContext *cx, unsigned argc, Value *vp)
{
    if (!JS_IsConstructing(cx, vp)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_NOT_FUNCTION, "ArrayType");
        return false;
    }

    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc != 2 ||
        !args[0].isObject() ||
        !args[1].isNumber() ||
        args[1].toNumber() < 0)
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BINARYDATA_ARRAYTYPE_BAD_ARGS);
        return false;
    }

    RootedObject arrayTypeGlobal(cx, &args.callee());
    RootedObject elementType(cx, &args[0].toObject());

    if (!IsBinaryType(elementType)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BINARYDATA_ARRAYTYPE_BAD_ARGS);
        return false;
    }

    if (!args[1].isInt32()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BINARYDATA_ARRAYTYPE_BAD_ARGS);
        return false;
    }

    int32_t length = args[1].toInt32();
    if (length < 0) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BINARYDATA_ARRAYTYPE_BAD_ARGS);
        return false;
    }

    RootedObject obj(cx, create(cx, arrayTypeGlobal, elementType, length));
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

/*********************************
 * Structs
 *********************************/
Class StructType::class_ = {
    "StructType",
    JSCLASS_HAS_RESERVED_SLOTS(STRUCT_TYPE_RESERVED_SLOTS) |
    JSCLASS_HAS_PRIVATE | // used to store FieldList
    JSCLASS_HAS_CACHED_PROTO(JSProto_StructType),
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL, /* finalize */
    NULL, /* checkAccess */
    NULL, /* call */
    NULL, /* hasInstance */
    BinaryBlock::construct,
    NULL  /* trace */
};

static bool
ConvertAndCopyStructTo(JSContext *cx,
                       StructTypeRepresentation *typeRepr,
                       HandleValue from,
                       uint8_t *mem)
{
    if (!from.isObject()) {
        ReportCannotConvertTo(cx, from, typeRepr);
        return false;
    }

    // Check for the fast case, where we can just memcpy:
    RootedObject fromObject(cx, &from.toObject());
    if (IsBlock(fromObject)) {
        RootedObject fromType(cx, GetType(fromObject));
        TypeRepresentation *fromTypeRepr = typeRepresentation(fromType);
        if (typeRepr == fromTypeRepr) {
            memcpy(mem, BlockMem(fromObject), typeRepr->size());
            return true;
        }
    }

    // Otherwise, convert the properties one by one.
    RootedId fieldId(cx);
    RootedValue fromProp(cx);
    for (size_t i = 0; i < typeRepr->fieldCount(); i++) {
        const StructField &field = typeRepr->field(i);

        fieldId = field.id;
        if (!JSObject::getGeneric(cx, fromObject, fromObject, fieldId, &fromProp))
            return false;

        if (!ConvertAndCopyTo(cx, field.typeRepr, fromProp,
                              mem + field.offset))
            return false;
    }
    return true;
}

/*
 * NOTE: layout() does not check for duplicates in fields since the arguments
 * to StructType are currently passed as an object literal. Fix this if it
 * changes to taking an array of arrays.
 */
bool
StructType::layout(JSContext *cx, HandleObject structType, HandleObject fields)
{
    AutoIdVector ids(cx);
    if (!GetPropertyNames(cx, fields, JSITER_OWNONLY, &ids))
        return false;

    AutoValueVector fieldTypeObjs(cx);
    AutoObjectVector fieldTypeReprObjs(cx);

    RootedValue fieldTypeVal(cx);
    RootedId id(cx);
    for (unsigned int i = 0; i < ids.length(); i++) {
        id = ids[i];

        if (!JSObject::getGeneric(cx, fields, fields, id, &fieldTypeVal))
            return false;

        uint32_t index;
        if (js_IdIsIndex(id, &index)) {
            RootedValue idValue(cx, IdToJsval(id));
            ReportCannotConvertTo(cx, idValue, "StructType field name");
            return false;
        }

        RootedObject fieldType(cx, ToObjectIfObject(fieldTypeVal));
        if (!fieldType || !IsBinaryType(fieldType)) {
            ReportCannotConvertTo(cx, fieldTypeVal, "StructType field specifier");
            return false;
        }

        if (!fieldTypeObjs.append(fieldTypeVal)) {
            js_ReportOutOfMemory(cx);
            return false;
        }

        if (!fieldTypeReprObjs.append(typeRepresentationOwnerObj(fieldType))) {
            js_ReportOutOfMemory(cx);
            return false;
        }
    }

    // Construct the `TypeRepresentation*`.
    RootedObject typeReprObj(
        cx,
        StructTypeRepresentation::Create(cx, ids, fieldTypeReprObjs));
    if (!typeReprObj)
        return false;
    StructTypeRepresentation *typeRepr =
        TypeRepresentation::fromOwnerObject(typeReprObj)->asStruct();
    structType->setFixedSlot(SLOT_TYPE_REPR, ObjectValue(*typeReprObj));

    // Construct for internal use an array with the type object for each field.
    RootedObject fieldTypeVec(
        cx,
        NewDenseCopiedArray(cx, fieldTypeObjs.length(), fieldTypeObjs.begin()));
    if (!fieldTypeVec)
        return false;

    structType->setFixedSlot(SLOT_STRUCT_FIELD_TYPES,
                             ObjectValue(*fieldTypeVec));

    // Construct the fieldNames vector
    AutoValueVector fieldNameValues(cx);
    for (unsigned int i = 0; i < ids.length(); i++) {
        RootedValue value(cx, IdToValue(ids[i]));
        if (!fieldNameValues.append(value))
            return false;
    }
    RootedObject fieldNamesVec(
        cx,
        NewDenseCopiedArray(cx, fieldNameValues.length(),
                            fieldNameValues.begin()));
    if (!fieldNamesVec)
        return false;
    RootedValue fieldNamesVecValue(cx, ObjectValue(*fieldNamesVec));
    if (!JSObject::defineProperty(cx, structType, cx->names().fieldNames,
                                  fieldNamesVecValue, NULL, NULL,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return false;

    // Construct the fieldNames, fieldOffsets and fieldTypes objects:
    // fieldNames : [ string ]
    // fieldOffsets : { string: integer, ... }
    // fieldTypes : { string: Type, ... }
    RootedObject fieldOffsets(
        cx,
        NewObjectWithClassProto(cx, &JSObject::class_, NULL, NULL));
    RootedObject fieldTypes(
        cx,
        NewObjectWithClassProto(cx, &JSObject::class_, NULL, NULL));
    for (size_t i = 0; i < typeRepr->fieldCount(); i++) {
        const StructField &field = typeRepr->field(i);
        RootedId fieldId(cx, field.id);

        // fieldOffsets[id] = offset
        RootedValue offset(cx, NumberValue(field.offset));
        if (!JSObject::defineGeneric(cx, fieldOffsets, fieldId,
                                     offset, NULL, NULL,
                                     JSPROP_READONLY | JSPROP_PERMANENT))
            return false;

        // fieldTypes[id] = typeObj
        if (!JSObject::defineGeneric(cx, fieldTypes, fieldId,
                                     fieldTypeObjs.handleAt(i), NULL, NULL,
                                     JSPROP_READONLY | JSPROP_PERMANENT))
            return false;
    }

    RootedValue fieldOffsetsValue(cx, ObjectValue(*fieldOffsets));
    if (!JSObject::defineProperty(cx, structType, cx->names().fieldOffsets,
                                  fieldOffsetsValue, NULL, NULL,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return false;

    RootedValue fieldTypesValue(cx, ObjectValue(*fieldTypes));
    if (!JSObject::defineProperty(cx, structType, cx->names().fieldTypes,
                                  fieldTypesValue, NULL, NULL,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return false;

    return true;
}

JSObject *
StructType::create(JSContext *cx, HandleObject structTypeGlobal,
                   HandleObject fields)
{
    RootedObject obj(cx, NewBuiltinClassInstance(cx, &StructType::class_));
    if (!obj)
        return NULL;

    if (!StructType::layout(cx, obj, fields))
        return NULL;

    RootedObject fieldsProto(cx);
    if (!JSObject::getProto(cx, fields, &fieldsProto))
        return NULL;

    RootedObject typeReprObj(cx, typeRepresentationOwnerObj(obj));
    if (!InitializeCommonTypeDescriptorProperties(cx, obj, typeReprObj))
        return NULL;

    RootedObject prototypeObj(cx,
        SetupAndGetPrototypeObjectForComplexTypeInstance(cx, structTypeGlobal));

    if (!prototypeObj)
        return NULL;

    if (!LinkConstructorAndPrototype(cx, obj, prototypeObj))
        return NULL;

    return obj;
}

bool
StructType::construct(JSContext *cx, unsigned int argc, Value *vp)
{
    if (!JS_IsConstructing(cx, vp)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_NOT_FUNCTION, "StructType");
        return false;
    }

    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc >= 1 && args[0].isObject()) {
        RootedObject structTypeGlobal(cx, &args.callee());
        RootedObject fields(cx, &args[0].toObject());
        RootedObject obj(cx, create(cx, structTypeGlobal, fields));
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                         JSMSG_BINARYDATA_STRUCTTYPE_BAD_ARGS);
    return false;
}

bool
StructType::toSource(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject thisObj(cx, ToObjectIfObject(args.thisv()));
    if (!thisObj || !IsStructType(thisObj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "StructType", "toSource",
                             InformalValueTypeName(args.thisv()));
        return false;
    }

    StringBuffer contents(cx);
    if (!typeRepresentation(thisObj)->appendString(cx, contents))
        return false;

    RootedString result(cx, contents.finishString());
    if (!result)
        return false;

    args.rval().setString(result);
    return true;
}

static bool
ConvertAndCopyTo(JSContext *cx,
                 TypeRepresentation *typeRepr,
                 HandleValue from,
                 uint8_t *mem)
{
    switch (typeRepr->kind()) {
      case TypeRepresentation::Scalar:
        return ConvertAndCopyScalarTo(cx, typeRepr->asScalar(), from, mem);

      case TypeRepresentation::Array:
        return ConvertAndCopyArrayTo(cx, typeRepr->asArray(), from, mem);

      case TypeRepresentation::Struct:
        return ConvertAndCopyStructTo(cx, typeRepr->asStruct(), from, mem);
    }

    MOZ_ASSUME_UNREACHABLE("Invalid kind");
    return false;
}

///////////////////////////////////////////////////////////////////////////

static bool
ReifyComplex(JSContext *cx, HandleObject type, HandleObject owner,
             size_t offset, MutableHandleValue to)
{
    RootedObject obj(cx, BinaryBlock::createDerived(cx, type, owner, offset));
    if (!obj)
        return false;
    to.setObject(*obj);
    return true;
}

static bool
Reify(JSContext *cx, TypeRepresentation *typeRepr, HandleObject type,
      HandleObject owner, size_t offset, MutableHandleValue to)
{
    JS_ASSERT(typeRepr == typeRepresentation(type));
    switch (typeRepr->kind()) {
      case TypeRepresentation::Scalar:
        return ReifyScalar(cx, typeRepr->asScalar(), type, owner, offset, to);
      case TypeRepresentation::Struct:
      case TypeRepresentation::Array:
        return ReifyComplex(cx, type, owner, offset, to);
    }
    MOZ_ASSUME_UNREACHABLE("Invalid typeRepr kind");
}

bool
GlobalObject::initDataObject(JSContext *cx, Handle<GlobalObject *> global)
{
    RootedObject DataProto(cx);
    DataProto = NewObjectWithGivenProto(cx, &DataClass,
                                        global->getOrCreateObjectPrototype(cx),
                                        global, SingletonObject);
    if (!DataProto)
        return false;

    RootedAtom DataName(cx, ClassName(JSProto_Data, cx));
    RootedFunction DataCtor(cx,
            global->createConstructor(cx, DataThrowError, DataName,
                                      1, JSFunction::ExtendedFinalizeKind));

    if (!DataCtor)
        return false;

    if (!JS_DefineFunction(cx, DataProto, "update", DataInstanceUpdate, 1, 0))
        return false;

    if (!LinkConstructorAndPrototype(cx, DataCtor, DataProto))
        return false;

    if (!DefineConstructorAndPrototype(cx, global, JSProto_Data,
                                       DataCtor, DataProto))
        return false;

    global->setReservedSlot(JSProto_Data, ObjectValue(*DataCtor));
    return true;
}

bool
GlobalObject::initTypeObject(JSContext *cx, Handle<GlobalObject *> global)
{
    RootedObject TypeProto(cx, global->getOrCreateDataObject(cx));
    if (!TypeProto)
        return false;

    RootedAtom TypeName(cx, ClassName(JSProto_Type, cx));
    RootedFunction TypeCtor(cx,
            global->createConstructor(cx, TypeThrowError, TypeName,
                                      1, JSFunction::ExtendedFinalizeKind));
    if (!TypeCtor)
        return false;

    if (!LinkConstructorAndPrototype(cx, TypeCtor, TypeProto))
        return false;

    if (!DefineConstructorAndPrototype(cx, global, JSProto_Type,
                                       TypeCtor, TypeProto))
        return false;

    global->setReservedSlot(JSProto_Type, ObjectValue(*TypeCtor));
    return true;
}

bool
GlobalObject::initArrayTypeObject(JSContext *cx, Handle<GlobalObject *> global)
{
    RootedFunction ctor(cx,
        global->createConstructor(cx, ArrayType::construct,
                                  cx->names().ArrayType, 2));

    global->setReservedSlot(JSProto_ArrayTypeObject, ObjectValue(*ctor));
    return true;
}

static JSObject *
SetupComplexHeirarchy(JSContext *cx, Handle<GlobalObject*> global, JSProtoKey protoKey,
                      HandleObject complexObject, MutableHandleObject proto,
                      MutableHandleObject protoProto)
{
    // get the 'Type' constructor
    RootedObject TypeObject(cx, global->getOrCreateTypeObject(cx));
    if (!TypeObject)
        return NULL;

    // Set complexObject.__proto__ = Type
    if (!JS_SetPrototype(cx, complexObject, TypeObject))
        return NULL;

    RootedObject DataObject(cx, global->getOrCreateDataObject(cx));
    if (!DataObject)
        return NULL;

    RootedValue DataProtoVal(cx);
    if (!JSObject::getProperty(cx, DataObject, DataObject,
                               cx->names().classPrototype, &DataProtoVal))
        return NULL;

    RootedObject DataProto(cx, &DataProtoVal.toObject());
    if (!DataProto)
        return NULL;

    RootedObject prototypeObj(cx,
        NewObjectWithGivenProto(cx, &JSObject::class_, NULL, global));
    if (!prototypeObj)
        return NULL;
    if (!LinkConstructorAndPrototype(cx, complexObject, prototypeObj))
        return NULL;
    if (!DefineConstructorAndPrototype(cx, global, protoKey,
                                       complexObject, prototypeObj))
        return NULL;

    // Set complexObject.prototype.__proto__ = Data
    if (!JS_SetPrototype(cx, prototypeObj, DataObject))
        return NULL;

    proto.set(prototypeObj);

    // Set complexObject.prototype.prototype.__proto__ = Data.prototype
    RootedObject prototypePrototypeObj(cx, JS_NewObject(cx, NULL, NULL,
                                       global));

    if (!LinkConstructorAndPrototype(cx, prototypeObj,
                                     prototypePrototypeObj))
        return NULL;

    if (!JS_SetPrototype(cx, prototypePrototypeObj, DataProto))
        return NULL;

    protoProto.set(prototypePrototypeObj);

    return complexObject;
}

static bool
InitType(JSContext *cx, HandleObject globalObj)
{
    JS_ASSERT(globalObj->isNative());
    Rooted<GlobalObject*> global(cx, &globalObj->as<GlobalObject>());
    RootedObject ctor(cx, global->getOrCreateTypeObject(cx));
    if (!ctor)
        return false;

    RootedValue protoVal(cx);
    if (!JSObject::getProperty(cx, ctor, ctor,
                               cx->names().classPrototype, &protoVal))
        return false;

    JS_ASSERT(protoVal.isObject());
    RootedObject protoObj(cx, &protoVal.toObject());

    if (!JS_DefineFunction(cx, protoObj, "equivalent", TypeEquivalent, 0, 0))
        return false;

    return true;
}

static bool
InitArrayType(JSContext *cx, HandleObject globalObj)
{
    JS_ASSERT(globalObj->isNative());
    Rooted<GlobalObject*> global(cx, &globalObj->as<GlobalObject>());
    RootedObject ctor(cx, global->getOrCreateArrayTypeObject(cx));
    if (!ctor)
        return false;

    RootedObject proto(cx);
    RootedObject protoProto(cx);
    if (!SetupComplexHeirarchy(cx, global, JSProto_ArrayType,
                               ctor, &proto, &protoProto))
        return false;

    if (!JS_DefineFunction(cx, proto, "repeat", ArrayRepeat, 1, 0))
        return false;

    if (!JS_DefineFunction(cx, proto, "toSource", ArrayType::toSource, 0, 0))
        return false;

    RootedObject arrayProto(cx);
    if (!FindProto(cx, &ArrayObject::class_, &arrayProto))
        return false;

    RootedValue forEachFunVal(cx);
    RootedAtom forEachAtom(cx, Atomize(cx, "forEach", 7));
    RootedId forEachId(cx, AtomToId(forEachAtom));
    if (!JSObject::getProperty(cx, arrayProto, arrayProto, forEachAtom->asPropertyName(), &forEachFunVal))
        return false;

    if (!JSObject::defineGeneric(cx, protoProto, forEachId, forEachFunVal, NULL, NULL, 0))
        return false;

    if (!JS_DefineFunction(cx, protoProto, "subarray",
                           ArraySubarray, 1, 0))
        return false;

    return true;
}

static bool
InitStructType(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->isNative());
    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());
    RootedFunction ctor(cx,
        global->createConstructor(cx, StructType::construct,
                                  cx->names().StructType, 1));

    if (!ctor)
        return false;

    RootedObject proto(cx);
    RootedObject protoProto(cx);
    if (!SetupComplexHeirarchy(cx, global, JSProto_StructType,
                               ctor, &proto, &protoProto))
        return false;

    if (!JS_DefineFunction(cx, proto, "toSource", StructType::toSource, 0, 0))
        return false;

    return true;
}

template<ScalarTypeRepresentation::Type type>
static bool
DefineNumericClass(JSContext *cx,
                   HandleObject global,
                   const char *name)
{
    RootedObject globalProto(cx, JS_GetFunctionPrototype(cx, global));
    RootedObject numFun(
        cx,
        JS_DefineObject(cx, global, name,
                        (JSClass *) &NumericTypeClasses[type],
                        globalProto, 0));
    if (!numFun)
        return false;

    RootedObject typeReprObj(cx, ScalarTypeRepresentation::Create(cx, type));
    if (!typeReprObj)
        return false;

    numFun->setFixedSlot(SLOT_TYPE_REPR, ObjectValue(*typeReprObj));

    if (!InitializeCommonTypeDescriptorProperties(cx, numFun, typeReprObj))
        return false;

    if (!JS_DefineFunction(cx, numFun, "toString",
                           NumericTypeToString<type>, 0, 0))
        return false;

    if (!JS_DefineFunction(cx, numFun, "toSource",
                           NumericTypeToString<type>, 0, 0))
        return false;

    return true;
}

JSObject *
js_InitBinaryDataClasses(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->is<GlobalObject>());
    Rooted<GlobalObject *> global(cx, &obj->as<GlobalObject>());

    if (!InitType(cx, obj))
        return NULL;

#define BINARYDATA_NUMERIC_DEFINE(constant_, type_, name_)                    \
    if (!DefineNumericClass<constant_>(cx, global, #name_))                   \
        return NULL;
    JS_FOR_EACH_SCALAR_TYPE_REPR(BINARYDATA_NUMERIC_DEFINE)
#undef BINARYDATA_NUMERIC_DEFINE

    if (!InitArrayType(cx, obj))
        return NULL;

    if (!InitStructType(cx, obj))
        return NULL;

    return global;
}

///////////////////////////////////////////////////////////////////////////
// Binary blocks

Class BinaryBlock::class_ = {
    "BinaryBlock",
    Class::NON_NATIVE |
    JSCLASS_HAS_RESERVED_SLOTS(BLOCK_RESERVED_SLOTS) |
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_ArrayType),
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    BinaryBlock::obj_finalize,
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* hasInstance */
    BinaryBlock::obj_trace,
    JS_NULL_CLASS_EXT,
    {
        BinaryBlock::obj_lookupGeneric,
        BinaryBlock::obj_lookupProperty,
        BinaryBlock::obj_lookupElement,
        BinaryBlock::obj_lookupSpecial,
        BinaryBlock::obj_defineGeneric,
        BinaryBlock::obj_defineProperty,
        BinaryBlock::obj_defineElement,
        BinaryBlock::obj_defineSpecial,
        BinaryBlock::obj_getGeneric,
        BinaryBlock::obj_getProperty,
        BinaryBlock::obj_getElement,
        BinaryBlock::obj_getElementIfPresent,
        BinaryBlock::obj_getSpecial,
        BinaryBlock::obj_setGeneric,
        BinaryBlock::obj_setProperty,
        BinaryBlock::obj_setElement,
        BinaryBlock::obj_setSpecial,
        BinaryBlock::obj_getGenericAttributes,
        BinaryBlock::obj_getPropertyAttributes,
        BinaryBlock::obj_getElementAttributes,
        BinaryBlock::obj_getSpecialAttributes,
        BinaryBlock::obj_setGenericAttributes,
        BinaryBlock::obj_setPropertyAttributes,
        BinaryBlock::obj_setElementAttributes,
        BinaryBlock::obj_setSpecialAttributes,
        BinaryBlock::obj_deleteProperty,
        BinaryBlock::obj_deleteElement,
        BinaryBlock::obj_deleteSpecial,
        BinaryBlock::obj_enumerate,
        NULL, /* thisObject */
    }
};

static bool
ReportBlockTypeError(JSContext *cx,
                     const unsigned errorNumber,
                     HandleObject obj)
{
    JS_ASSERT(IsBlock(obj));

    RootedObject type(cx, GetType(obj));
    TypeRepresentation *typeRepr = typeRepresentation(type);

    StringBuffer contents(cx);
    if (!typeRepr->appendString(cx, contents))
        return false;

    RootedString result(cx, contents.finishString());
    if (!result)
        return false;

    char *typeReprStr = JS_EncodeString(cx, result.get());
    if (!typeReprStr)
        return false;

    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                         errorNumber, typeReprStr);

    JS_free(cx, (void *) typeReprStr);
    return false;
}

/*static*/ void
BinaryBlock::obj_trace(JSTracer *trace, JSObject *object)
{
    JS_ASSERT(object->hasClass(&class_));

    for (size_t i = 0; i < BLOCK_RESERVED_SLOTS; i++)
        gc::MarkSlot(trace, &object->getReservedSlotRef(i), "BinaryBlockSlot");
}

/*static*/ void
BinaryBlock::obj_finalize(js::FreeOp *op, JSObject *obj)
{
    if (!obj->getFixedSlot(SLOT_BLOCKREFOWNER).isNull())
        return;

    if (void *mem = obj->getPrivate())
        op->free_(mem);
}

/*static*/ JSObject *
BinaryBlock::createNull(JSContext *cx, HandleObject type, HandleValue owner)
{
    JS_ASSERT(IsBinaryType(type));

    RootedValue protoVal(cx);
    if (!JSObject::getProperty(cx, type, type,
                               cx->names().classPrototype, &protoVal))
        return NULL;

    RootedObject obj(cx,
        NewObjectWithClassProto(cx, &class_, &protoVal.toObject(), NULL));
    obj->setFixedSlot(SLOT_DATATYPE, ObjectValue(*type));
    obj->setFixedSlot(SLOT_BLOCKREFOWNER, owner);

    return obj;
}

/*static*/ JSObject *
BinaryBlock::createZeroed(JSContext *cx, HandleObject type)
{
    RootedValue owner(cx, NullValue());
    RootedObject obj(cx, createNull(cx, type, owner));
    if (!obj)
        return NULL;

    TypeRepresentation *typeRepr = typeRepresentation(type);
    size_t memsize = typeRepr->size();
    void *memory = JS_malloc(cx, memsize);
    if (!memory)
        return NULL;
    memset(memory, 0, memsize);
    obj->setPrivate(memory);
    return obj;
}

/*static*/ JSObject *
BinaryBlock::createDerived(JSContext *cx, HandleObject type,
                           HandleObject owner, size_t offset)
{
    JS_ASSERT(IsBlock(owner));
    JS_ASSERT(offset <= BlockSize(cx, owner));
    JS_ASSERT(offset + TypeSize(type) <= BlockSize(cx, owner));

    RootedValue ownerValue(cx, ObjectValue(*owner));
    RootedObject obj(cx, createNull(cx, type, ownerValue));
    if (!obj)
        return NULL;

    obj->setPrivate(BlockMem(owner) + offset);
    return obj;
}

/*static*/ bool
BinaryBlock::construct(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject callee(cx, &args.callee());
    JS_ASSERT(IsBinaryType(callee));

    RootedObject obj(cx, createZeroed(cx, callee));
    if (!obj)
        return false;

    if (argc == 1) {
        RootedValue initial(cx, args[0]);
        if (!ConvertAndCopyTo(cx, initial, obj))
            return false;
    }

    args.rval().setObject(*obj);
    return true;
}

bool
BinaryBlock::obj_lookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                                MutableHandleObject objp, MutableHandleShape propp)
{
    JS_ASSERT(IsBlock(obj));

    RootedObject type(cx, GetType(obj));
    TypeRepresentation *typeRepr = typeRepresentation(type);

    switch (typeRepr->kind()) {
      case TypeRepresentation::Scalar:
        break;

      case TypeRepresentation::Array: {
        uint32_t index;
        if (js_IdIsIndex(id, &index))
            return obj_lookupElement(cx, obj, index, objp, propp);

        if (JSID_IS_ATOM(id, cx->names().length)) {
            MarkNonNativePropertyFound(propp);
            objp.set(obj);
            return true;
        }
        break;
      }

      case TypeRepresentation::Struct: {
        RootedObject type(cx, GetType(obj));
        JS_ASSERT(IsStructType(type));
        StructTypeRepresentation *typeRepr =
            typeRepresentation(type)->asStruct();
        const StructField *field = typeRepr->fieldNamed(id);
        if (field) {
            MarkNonNativePropertyFound(propp);
            objp.set(obj);
            return true;
        }
        break;
      }
    }

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        objp.set(NULL);
        propp.set(NULL);
        return true;
    }

    return JSObject::lookupGeneric(cx, proto, id, objp, propp);
}

bool
BinaryBlock::obj_lookupProperty(JSContext *cx,
                                HandleObject obj,
                                HandlePropertyName name,
                                MutableHandleObject objp,
                                MutableHandleShape propp)
{
    RootedId id(cx, NameToId(name));
    return obj_lookupGeneric(cx, obj, id, objp, propp);
}

bool
BinaryBlock::obj_lookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                                MutableHandleObject objp, MutableHandleShape propp)
{
    JS_ASSERT(IsBlock(obj));
    MarkNonNativePropertyFound(propp);
    objp.set(obj);
    return true;
}

static bool
ReportPropertyError(JSContext *cx,
                    const unsigned errorNumber,
                    HandleId id)
{
    RootedString str(cx, IdToString(cx, id));
    if (!str)
        return false;

    char *propName = JS_EncodeString(cx, str);
    if (!propName)
        return false;

    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                         errorNumber, propName);

    JS_free(cx, propName);
    return false;
}

bool
BinaryBlock::obj_lookupSpecial(JSContext *cx, HandleObject obj,
                               HandleSpecialId sid, MutableHandleObject objp,
                               MutableHandleShape propp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return obj_lookupGeneric(cx, obj, id, objp, propp);
}

bool
BinaryBlock::obj_defineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue v,
                               PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    return ReportPropertyError(cx, JSMSG_UNDEFINED_PROP, id);
}

bool
BinaryBlock::obj_defineProperty(JSContext *cx, HandleObject obj,
                                HandlePropertyName name, HandleValue v,
                                PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, NameToId(name));
    return obj_defineGeneric(cx, obj, id, v, getter, setter, attrs);
}

bool
BinaryBlock::obj_defineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue v,
                               PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);

    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::DefineElement(cx, delegate, index, v, getter, setter, attrs);
}

bool
BinaryBlock::obj_defineSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, HandleValue v,
                               PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return obj_defineGeneric(cx, obj, id, v, getter, setter, attrs);
}

bool
BinaryBlock::obj_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver,
                             HandleId id, MutableHandleValue vp)
{
    JS_ASSERT(IsBlock(obj));

    // Dispatch elements to obj_getElement:
    uint32_t index;
    if (js_IdIsIndex(id, &index))
        return obj_getElement(cx, obj, receiver, index, vp);

    // Handle everything else here:

    RootedObject type(cx, GetType(obj));
    TypeRepresentation *typeRepr = typeRepresentation(type);
    switch (typeRepr->kind()) {
      case TypeRepresentation::Scalar:
        break;

      case TypeRepresentation::Array:
        if (JSID_IS_ATOM(id, cx->names().length)) {
            vp.setInt32(typeRepr->asArray()->length());
            return true;
        }
        break;

      case TypeRepresentation::Struct: {
        StructTypeRepresentation *structTypeRepr = typeRepr->asStruct();
        const StructField *field = structTypeRepr->fieldNamed(id);
        if (!field)
            break;

        // Recover the original type object here (`field` contains
        // only its canonical form). The difference is observable,
        // e.g. in a program like:
        //
        //     var Point1 = new StructType({x:uint8, y:uint8});
        //     var Point2 = new StructType({x:uint8, y:uint8});
        //     var Line1 = new StructType({start:Point1, end: Point1});
        //     var Line2 = new StructType({start:Point2, end: Point2});
        //     var line1 = new Line1(...);
        //     var line2 = new Line2(...);
        //
        // In this scenario, line1.start.type() === Point1 and
        // line2.start.type() === Point2.
        RootedObject fieldTypes(
            cx,
            &type->getFixedSlot(SLOT_STRUCT_FIELD_TYPES).toObject());
        RootedValue fieldTypeVal(cx);
        if (!JSObject::getElement(cx, fieldTypes, fieldTypes,
                                  field->index, &fieldTypeVal))
            return false;

        RootedObject fieldType(cx, &fieldTypeVal.toObject());
        return Reify(cx, field->typeRepr, fieldType, obj, field->offset, vp);
      }
    }

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        vp.setUndefined();
        return true;
    }

    return JSObject::getGeneric(cx, proto, receiver, id, vp);
}

bool
BinaryBlock::obj_getProperty(JSContext *cx, HandleObject obj, HandleObject receiver,
                              HandlePropertyName name, MutableHandleValue vp)
{
    RootedId id(cx, NameToId(name));
    return obj_getGeneric(cx, obj, receiver, id, vp);
}

bool
BinaryBlock::obj_getElement(JSContext *cx, HandleObject obj, HandleObject receiver,
                             uint32_t index, MutableHandleValue vp)
{
    bool present;
    return obj_getElementIfPresent(cx, obj, receiver, index, vp, &present);
}

bool
BinaryBlock::obj_getElementIfPresent(JSContext *cx, HandleObject obj,
                                     HandleObject receiver, uint32_t index,
                                     MutableHandleValue vp, bool *present)
{
    RootedObject type(cx, GetType(obj));
    TypeRepresentation *typeRepr = typeRepresentation(type);

    switch (typeRepr->kind()) {
      case TypeRepresentation::Scalar:
      case TypeRepresentation::Struct:
        break;

      case TypeRepresentation::Array: {
        *present = true;
        ArrayTypeRepresentation *arrayTypeRepr = typeRepr->asArray();

        if (index >= arrayTypeRepr->length()) {
            vp.setUndefined();
            return true;
        }

        RootedObject elementType(cx, ArrayElementType(type));
        size_t offset = arrayTypeRepr->element()->size() * index;
        return Reify(cx, arrayTypeRepr->element(), elementType, obj, offset, vp);
      }
    }

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        *present = false;
        vp.setUndefined();
        return true;
    }

    return JSObject::getElementIfPresent(cx, proto, receiver, index, vp, present);
}

bool
BinaryBlock::obj_getSpecial(JSContext *cx, HandleObject obj,
                            HandleObject receiver, HandleSpecialId sid,
                            MutableHandleValue vp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return obj_getGeneric(cx, obj, receiver, id, vp);
}

bool
BinaryBlock::obj_setGeneric(JSContext *cx, HandleObject obj, HandleId id,
                             MutableHandleValue vp, bool strict)
{
    uint32_t index;
    if (js_IdIsIndex(id, &index))
        return obj_setElement(cx, obj, index, vp, strict);

    RootedObject type(cx, GetType(obj));
    TypeRepresentation *typeRepr = typeRepresentation(type);

    switch (typeRepr->kind()) {
      case ScalarTypeRepresentation::Scalar:
        break;

      case ScalarTypeRepresentation::Array:
        if (JSID_IS_ATOM(id, cx->names().length)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                 NULL, JSMSG_CANT_REDEFINE_ARRAY_LENGTH);
            return false;
        }
        break;

      case ScalarTypeRepresentation::Struct: {
        const StructField *field = typeRepr->asStruct()->fieldNamed(id);
        if (!field)
            break;

        uint8_t *loc = BlockMem(obj) + field->offset;
        return ConvertAndCopyTo(cx, field->typeRepr, vp, loc);
      }
    }

    return ReportBlockTypeError(cx, JSMSG_OBJECT_NOT_EXTENSIBLE, obj);
}

bool
BinaryBlock::obj_setProperty(JSContext *cx, HandleObject obj,
                             HandlePropertyName name, MutableHandleValue vp,
                             bool strict)
{
    RootedId id(cx, NameToId(name));
    return obj_setGeneric(cx, obj, id, vp, strict);
}

bool
BinaryBlock::obj_setElement(JSContext *cx, HandleObject obj, uint32_t index,
                             MutableHandleValue vp, bool strict)
{
    RootedObject type(cx, GetType(obj));
    TypeRepresentation *typeRepr = typeRepresentation(type);

    switch (typeRepr->kind()) {
      case ScalarTypeRepresentation::Scalar:
      case ScalarTypeRepresentation::Struct:
        break;

      case ScalarTypeRepresentation::Array: {
        ArrayTypeRepresentation *arrayTypeRepr = typeRepr->asArray();

        if (index >= arrayTypeRepr->length()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                 NULL, JSMSG_BINARYDATA_BINARYARRAY_BAD_INDEX);
            return false;
        }

        size_t offset = arrayTypeRepr->element()->size() * index;
        uint8_t *mem = BlockMem(obj) + offset;
        return ConvertAndCopyTo(cx, arrayTypeRepr->element(), vp, mem);
      }
    }

    return ReportBlockTypeError(cx, JSMSG_OBJECT_NOT_EXTENSIBLE, obj);
}

bool
BinaryBlock::obj_setSpecial(JSContext *cx, HandleObject obj,
                             HandleSpecialId sid, MutableHandleValue vp,
                             bool strict)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return obj_setGeneric(cx, obj, id, vp, strict);
}

bool
BinaryBlock::obj_getGenericAttributes(JSContext *cx, HandleObject obj,
                                       HandleId id, unsigned *attrsp)
{
    uint32_t index;
    RootedObject type(cx, GetType(obj));
    TypeRepresentation *typeRepr = typeRepresentation(type);

    switch (typeRepr->kind()) {
      case TypeRepresentation::Scalar:
        break;

      case TypeRepresentation::Array:
        if (js_IdIsIndex(id, &index)) {
            *attrsp = JSPROP_ENUMERATE | JSPROP_PERMANENT;
            return true;
        }
        if (JSID_IS_ATOM(id, cx->names().length)) {
            *attrsp = JSPROP_READONLY | JSPROP_PERMANENT;
            return true;
        }
        break;

      case TypeRepresentation::Struct:
        if (typeRepr->asStruct()->fieldNamed(id) != NULL) {
            *attrsp = JSPROP_ENUMERATE | JSPROP_PERMANENT;
            return true;
        }
        break;
    }

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        *attrsp = 0;
        return true;
    }

    return JSObject::getGenericAttributes(cx, proto, id, attrsp);
}

bool
BinaryBlock::obj_getPropertyAttributes(JSContext *cx, HandleObject obj,
                                        HandlePropertyName name,
                                        unsigned *attrsp)
{
    RootedId id(cx, NameToId(name));
    return obj_getGenericAttributes(cx, obj, id, attrsp);
}

bool
BinaryBlock::obj_getElementAttributes(JSContext *cx, HandleObject obj,
                                       uint32_t index, unsigned *attrsp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return obj_getGenericAttributes(cx, obj, id, attrsp);
}

bool
BinaryBlock::obj_getSpecialAttributes(JSContext *cx, HandleObject obj,
                                       HandleSpecialId sid, unsigned *attrsp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return obj_getGenericAttributes(cx, obj, id, attrsp);
}

static bool
IsOwnId(JSContext *cx, HandleObject obj, HandleId id)
{
    uint32_t index;
    RootedObject type(cx, GetType(obj));
    TypeRepresentation *typeRepr = typeRepresentation(type);

    switch (typeRepr->kind()) {
      case TypeRepresentation::Scalar:
        return false;

      case TypeRepresentation::Array:
        return js_IdIsIndex(id, &index) || JSID_IS_ATOM(id, cx->names().length);

      case TypeRepresentation::Struct:
        return typeRepr->asStruct()->fieldNamed(id) != NULL;
    }

    return false;
}

bool
BinaryBlock::obj_setGenericAttributes(JSContext *cx, HandleObject obj,
                                       HandleId id, unsigned *attrsp)
{
    RootedObject type(cx, GetType(obj));

    if (IsOwnId(cx, obj, id))
        return ReportPropertyError(cx, JSMSG_CANT_REDEFINE_PROP, id);

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        *attrsp = 0;
        return true;
    }

    return JSObject::setGenericAttributes(cx, proto, id, attrsp);
}

bool
BinaryBlock::obj_setPropertyAttributes(JSContext *cx, HandleObject obj,
                                        HandlePropertyName name,
                                        unsigned *attrsp)
{
    RootedId id(cx, NameToId(name));
    return obj_setGenericAttributes(cx, obj, id, attrsp);
}

bool
BinaryBlock::obj_setElementAttributes(JSContext *cx, HandleObject obj,
                                       uint32_t index, unsigned *attrsp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return obj_setGenericAttributes(cx, obj, id, attrsp);
}

bool
BinaryBlock::obj_setSpecialAttributes(JSContext *cx, HandleObject obj,
                                      HandleSpecialId sid, unsigned *attrsp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return obj_setGenericAttributes(cx, obj, id, attrsp);
}

bool
BinaryBlock::obj_deleteProperty(JSContext *cx, HandleObject obj,
                                HandlePropertyName name, bool *succeeded)
{
    Rooted<jsid> id(cx, NameToId(name));
    if (IsOwnId(cx, obj, id))
        return ReportPropertyError(cx, JSMSG_CANT_DELETE, id);

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        *succeeded = false;
        return true;
    }

    return JSObject::deleteProperty(cx, proto, name, succeeded);
}

bool
BinaryBlock::obj_deleteElement(JSContext *cx, HandleObject obj, uint32_t index,
                               bool *succeeded)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;

    if (IsOwnId(cx, obj, id))
        return ReportPropertyError(cx, JSMSG_CANT_DELETE, id);

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        *succeeded = false;
        return true;
    }

    return JSObject::deleteElement(cx, proto, index, succeeded);
}

bool
BinaryBlock::obj_deleteSpecial(JSContext *cx, HandleObject obj,
                               HandleSpecialId sid, bool *succeeded)
{
    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        *succeeded = false;
        return true;
    }

    return JSObject::deleteSpecial(cx, proto, sid, succeeded);
}

bool
BinaryBlock::obj_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
                           MutableHandleValue statep, MutableHandleId idp)
{
    uint32_t index;

    RootedObject type(cx, GetType(obj));
    TypeRepresentation *typeRepr = typeRepresentation(type);

    switch (typeRepr->kind()) {
      case TypeRepresentation::Scalar:
        switch (enum_op) {
          case JSENUMERATE_INIT_ALL:
          case JSENUMERATE_INIT:
            statep.setInt32(0);
            idp.set(INT_TO_JSID(0));

          case JSENUMERATE_NEXT:
          case JSENUMERATE_DESTROY:
            statep.setNull();
            break;
        }
        break;

      case TypeRepresentation::Array:
        switch (enum_op) {
          case JSENUMERATE_INIT_ALL:
          case JSENUMERATE_INIT:
            statep.setInt32(0);
            idp.set(INT_TO_JSID(typeRepr->asArray()->length() + 1));
            break;

          case JSENUMERATE_NEXT:
            index = static_cast<int32_t>(statep.toInt32());

            if (index < typeRepr->asArray()->length()) {
                idp.set(INT_TO_JSID(index));
                statep.setInt32(index + 1);
            } else if (index == typeRepr->asArray()->length()) {
                idp.set(NameToId(cx->names().length));
                statep.setInt32(index + 1);
            } else {
                JS_ASSERT(index == typeRepr->asArray()->length() + 1);
                statep.setNull();
            }

            break;

          case JSENUMERATE_DESTROY:
            statep.setNull();
            break;
        }
        break;

      case TypeRepresentation::Struct:
        switch (enum_op) {
          case JSENUMERATE_INIT_ALL:
          case JSENUMERATE_INIT:
            statep.setInt32(0);
            idp.set(INT_TO_JSID(typeRepr->asStruct()->fieldCount()));
            break;

          case JSENUMERATE_NEXT:
            index = static_cast<uint32_t>(statep.toInt32());

            if (index < typeRepr->asStruct()->fieldCount()) {
                idp.set(typeRepr->asStruct()->field(index).id);
                statep.setInt32(index + 1);
            } else {
                statep.setNull();
            }

            break;

          case JSENUMERATE_DESTROY:
            statep.setNull();
            break;
        }
        break;
    }

    return true;
}
