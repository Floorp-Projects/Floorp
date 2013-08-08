/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/BinaryData.h"

#include "mozilla/FloatingPoint.h"

#include "jscompartment.h"
#include "jsfun.h"
#include "jsobj.h"
#include "jsutil.h"

#include "gc/Marking.h"
#include "js/Vector.h"
#include "vm/GlobalObject.h"
#include "vm/String.h"
#include "vm/StringBuffer.h"
#include "vm/TypedArrayObject.h"

#include "jsatominlines.h"
#include "jsobjinlines.h"

using mozilla::DebugOnly;

using namespace js;

/*
 * Reify() takes a complex binary data object `owner` and an offset and tries to
 * convert the value of type `type` at that offset to a JS Value stored in
 * `vp`.
 *
 * NOTE: `type` is NOT the type of `owner`, but the type of `owner.elementType` in
 * case of BinaryArray or `owner[field].type` in case of BinaryStruct.
 */
static bool Reify(JSContext *cx, HandleObject type, HandleObject owner,
                  size_t offset, MutableHandleValue vp);

/*
 * ConvertAndCopyTo() converts `from` to type `type` and stores the result in
 * `mem`, which MUST be pre-allocated to the appropriate size for instances of
 * `type`.
 */
static bool ConvertAndCopyTo(JSContext *cx, HandleObject type,
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
ReportTypeError(JSContext *cx, HandleValue fromValue, const char *toType)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_CONVERT_TO,
                         InformalValueTypeName(fromValue), toType);
}

static void
ReportTypeError(JSContext *cx, HandleValue fromValue, JSString *toType)
{
    const char *fnName = JS_EncodeString(cx, toType);
    ReportTypeError(cx, fromValue, fnName);
    JS_free(cx, (void *) fnName);
}

// The false return value allows callers to just return as soon as this is
// called.
// So yes this call is with side effects.
static bool
ReportTypeError(JSContext *cx, HandleValue fromValue, HandleObject exemplar)
{
    RootedValue v(cx, ObjectValue(*exemplar));
    ReportTypeError(cx, fromValue, ToString<CanGC>(cx, v));
    return false;
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

static inline bool
IsNumericType(HandleObject type)
{
    return type && &NumericTypeClasses[NUMERICTYPE_UINT8] <= type->getClass() &&
                   type->getClass() <= &NumericTypeClasses[NUMERICTYPE_FLOAT64];
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
IsBinaryArray(HandleObject type)
{
    return type && type->hasClass(&BinaryArray::class_);
}

static inline bool
IsBinaryStruct(HandleObject type)
{
    return type && type->hasClass(&BinaryStruct::class_);
}

static inline bool
IsBlock(HandleObject type)
{
    return type->hasClass(&BinaryArray::class_) ||
           type->hasClass(&BinaryStruct::class_);
}

static inline JSObject *
GetType(HandleObject block)
{
    JS_ASSERT(IsBlock(block));
    return block->getFixedSlot(SLOT_DATATYPE).toObjectOrNull();
}

static size_t
GetMemSize(JSContext *cx, HandleObject type)
{
    if (IsComplexType(type))
        return type->getFixedSlot(SLOT_MEMSIZE).toInt32();

    RootedObject typeObj(cx, type);
    RootedValue val(cx);
    JS_ASSERT(IsNumericType(type));
    JSObject::getProperty(cx, typeObj, typeObj, cx->names().bytes, &val);
    return val.toInt32();
}

static size_t
GetAlign(JSContext *cx, HandleObject type)
{
    if (IsComplexType(type))
        return type->getFixedSlot(SLOT_ALIGN).toInt32();

    RootedObject typeObj(cx, type);
    RootedValue val(cx);
    JS_ASSERT(&NumericTypeClasses[NUMERICTYPE_UINT8] <= type->getClass() &&
              type->getClass() <= &NumericTypeClasses[NUMERICTYPE_FLOAT64]);
    JSObject::getProperty(cx, typeObj, typeObj, cx->names().bytes, &val);
    return val.toInt32();
}

struct FieldInfo
{
    RelocatableId name;
    RelocatablePtrObject type;
    size_t offset;

    FieldInfo() : offset(0) {}

    FieldInfo(const FieldInfo &o)
        : name(o.name.get()), type(o.type), offset(o.offset)
    {
    }
};

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

Class js::NumericTypeClasses[NUMERICTYPES] = {
    BINARYDATA_FOR_EACH_NUMERIC_TYPES(BINARYDATA_NUMERIC_CLASSES)
};

typedef Vector<FieldInfo> FieldList;

static
FieldList *
GetStructTypeFieldList(HandleObject obj)
{
    JS_ASSERT(IsStructType(obj));
    return static_cast<FieldList *>(obj->getPrivate());
}

static
bool
LookupFieldList(FieldList *list, jsid fieldName, FieldInfo **out)
{
    for (uint32_t i = 0; i < list->length(); ++i) {
        FieldInfo *info = &(*list)[i];
        if (info->name == fieldName) {
            *out = info;
            return true;
        }
    }
    return false;
}

static bool
IsSameBinaryDataType(JSContext *cx, HandleObject type1, HandleObject type2);

static bool
IsSameArrayType(JSContext *cx, HandleObject type1, HandleObject type2)
{
    JS_ASSERT(IsArrayType(type1) && IsArrayType(type2));
    if (ArrayType::length(cx, type1) != ArrayType::length(cx, type2))
        return false;

    RootedObject elementType1(cx, ArrayType::elementType(cx, type1));
    RootedObject elementType2(cx, ArrayType::elementType(cx, type2));
    return IsSameBinaryDataType(cx, elementType1, elementType2);
}

static bool
IsSameStructType(JSContext *cx, HandleObject type1, HandleObject type2)
{
    JS_ASSERT(IsStructType(type1) && IsStructType(type2));

    FieldList *fieldList1 = GetStructTypeFieldList(type1);
    FieldList *fieldList2 = GetStructTypeFieldList(type2);

    if (fieldList1->length() != fieldList2->length())
        return false;

    // Names and layout should be the same.
    for (uint32_t i = 0; i < fieldList1->length(); ++i) {
        FieldInfo *fieldInfo1 = &(*fieldList1)[i];
        FieldInfo *fieldInfo2 = &(*fieldList2)[i];

        if (fieldInfo1->name.get() != fieldInfo2->name.get())
            return false;

        if (fieldInfo1->offset != fieldInfo2->offset)
            return false;

        RootedObject fieldType1(cx, fieldInfo1->type);
        RootedObject fieldType2(cx, fieldInfo2->type);
        if (!IsSameBinaryDataType(cx, fieldType1, fieldType2))
            return false;
    }

    return true;
}

static bool
IsSameBinaryDataType(JSContext *cx, HandleObject type1, HandleObject type2)
{
    JS_ASSERT(IsBinaryType(type1));
    JS_ASSERT(IsBinaryType(type2));

    if (IsNumericType(type1)) {
        return type1->hasClass(type2->getClass());
    } else if (IsArrayType(type1) && IsArrayType(type2)) {
        return IsSameArrayType(cx, type1, type2);
    } else if (IsStructType(type1) && IsStructType(type2)) {
        return IsSameStructType(cx, type1, type2);
    }

    return false;
}

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

template <typename T>
Class *
js::NumericType<T>::typeToClass()
{
    abort();
    return NULL;
}

#define BINARYDATA_TYPE_TO_CLASS(constant_, type_)\
    template <>\
    Class *\
    NumericType<type_##_t>::typeToClass()\
    {\
        return &NumericTypeClasses[constant_];\
    }

/**
 * This namespace declaration is required because of a weird 'specialization in
 * different namespace' error that happens in gcc, only on type specialized
 * template functions.
 */
namespace js {
    BINARYDATA_FOR_EACH_NUMERIC_TYPES(BINARYDATA_TYPE_TO_CLASS);
}

template <typename T>
bool
NumericType<T>::convert(JSContext *cx, HandleValue val, T* converted)
{
    if (val.isInt32()) {
        *converted = T(val.toInt32());
        return true;
    }

    double d;
    if (!ToDoubleForTypedArray(cx, val, &d)) {
        Class *typeClass = typeToClass();
        ReportTypeError(cx, val, typeClass->name);
        return false;
    }

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

template <typename T>
bool
NumericType<T>::call(JSContext *cx, unsigned argc, Value *vp)
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
    if (!NumericType<T>::reify(cx, &answer, &reified)) {
        return false;
    }

    args.rval().set(reified);
    return true;
}

template<unsigned int N>
bool
NumericTypeToString(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(NUMERICTYPE_UINT8 <= N && N <= NUMERICTYPE_FLOAT64);
    JSString *s = JS_NewStringCopyZ(cx, NumericTypeClasses[N].name);
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

    RootedObject complexTypePrototypeObj(cx,
        complexTypePrototypeVal.toObjectOrNull());

    if (!JSObject::getProperty(cx, complexTypePrototypeObj,
                               complexTypePrototypeObj,
                               cx->names().classPrototype,
                               &complexTypePrototypePrototypeVal))
        return NULL;

    RootedObject prototypeObj(cx,
        NewObjectWithGivenProto(cx, &JSObject::class_, NULL, global));

    RootedObject proto(cx, complexTypePrototypePrototypeVal.toObjectOrNull());
    if (!JS_SetPrototype(cx, prototypeObj, proto))
        return NULL;

    return prototypeObj;
}

Class ArrayType::class_ = {
    "ArrayType",
    JSCLASS_HAS_RESERVED_SLOTS(TYPE_RESERVED_SLOTS) |
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
    BinaryArray::construct,
    NULL
};

Class BinaryArray::class_ = {
    "BinaryArray",
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
    BinaryArray::finalize,
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* hasInstance */
    BinaryArray::obj_trace,
    JS_NULL_CLASS_EXT,
    {
        BinaryArray::obj_lookupGeneric,
        BinaryArray::obj_lookupProperty,
        BinaryArray::obj_lookupElement,
        BinaryArray::obj_lookupSpecial,
        NULL,
        NULL,
        NULL,
        NULL,
        BinaryArray::obj_getGeneric,
        BinaryArray::obj_getProperty,
        BinaryArray::obj_getElement,
        BinaryArray::obj_getElementIfPresent,
        BinaryArray::obj_getSpecial,
        BinaryArray::obj_setGeneric,
        BinaryArray::obj_setProperty,
        BinaryArray::obj_setElement,
        BinaryArray::obj_setSpecial,
        BinaryArray::obj_getGenericAttributes,
        BinaryArray::obj_getPropertyAttributes,
        BinaryArray::obj_getElementAttributes,
        BinaryArray::obj_getSpecialAttributes,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        BinaryArray::obj_enumerate,
        NULL,
    }
};

inline uint32_t
ArrayType::length(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj && IsArrayType(obj));
    RootedValue vp(cx, UndefinedValue());
    if (!JSObject::getProperty(cx, obj, obj, cx->names().length, &vp))
        return -1;
    JS_ASSERT(vp.isInt32());
    JS_ASSERT(vp.toInt32() >= 0);
    return (uint32_t) vp.toInt32();
}

inline JSObject *
ArrayType::elementType(JSContext *cx, HandleObject array)
{
    JS_ASSERT(IsArrayType(array));
    RootedObject arr(cx, array);
    RootedValue elementTypeVal(cx);
    if (!JSObject::getProperty(cx, arr, arr,
                               cx->names().elementType, &elementTypeVal))

    JS_ASSERT(elementTypeVal.isObject());
    return elementTypeVal.toObjectOrNull();
}

bool
ArrayType::convertAndCopyTo(JSContext *cx, HandleObject exemplar,
                            HandleValue from, uint8_t *mem)
{
    if (!from.isObject()) {
        return ReportTypeError(cx, from, exemplar);
    }

    RootedObject val(cx, from.toObjectOrNull());
    if (IsBlock(val)) {
        RootedObject type(cx, GetType(val));
        if (IsSameBinaryDataType(cx, exemplar, type)) {
            uint8_t *priv = static_cast<uint8_t*>(val->getPrivate());
            memcpy(mem, priv, GetMemSize(cx, exemplar));
            return true;
        }
        return ReportTypeError(cx, from, exemplar);
    }

    RootedObject valRooted(cx, val);
    RootedValue fromLenVal(cx);
    if (!JSObject::getProperty(cx, valRooted, valRooted,
                               cx->names().length, &fromLenVal) ||
        !fromLenVal.isInt32())
    {
        return ReportTypeError(cx, from, exemplar);
    }

    uint32_t fromLen = fromLenVal.toInt32();

    if (ArrayType::length(cx, exemplar) != fromLen) {
        return ReportTypeError(cx, from, exemplar);
    }

    RootedObject elementType(cx, ArrayType::elementType(cx, exemplar));

    uint32_t offsetMult = GetMemSize(cx, elementType);

    for (uint32_t i = 0; i < fromLen; i++) {
        RootedValue fromElem(cx);
        if (!JSObject::getElement(cx, valRooted, valRooted, i, &fromElem))
            return ReportTypeError(cx, from, exemplar);

        if (!ConvertAndCopyTo(cx, elementType, fromElem, mem + (offsetMult * i)))
            return false;
    }

    return true;
}

inline bool
ArrayType::reify(JSContext *cx, HandleObject type,
                 HandleObject owner, size_t offset, MutableHandleValue to)
{
    JSObject *obj = BinaryArray::create(cx, type, owner, offset);
    if (!obj)
        return false;
    to.setObject(*obj);
    return true;
}

JSObject *
ArrayType::create(JSContext *cx, HandleObject arrayTypeGlobal,
                  HandleObject elementType, uint32_t length)
{
    JS_ASSERT(elementType);
    JS_ASSERT(IsBinaryType(elementType));

    RootedObject obj(cx, NewBuiltinClassInstance(cx, &ArrayType::class_));
    if (!obj)
        return NULL;

    RootedValue elementTypeVal(cx, ObjectValue(*elementType));
    if (!JSObject::defineProperty(cx, obj, cx->names().elementType,
                                  elementTypeVal, NULL, NULL,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return NULL;

    RootedValue lengthVal(cx, Int32Value(length));
    if (!JSObject::defineProperty(cx, obj, cx->names().length,
                                  lengthVal, NULL, NULL,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return NULL;

    RootedValue elementTypeBytes(cx);
    if (!JSObject::getProperty(cx, elementType, elementType, cx->names().bytes, &elementTypeBytes))
        return NULL;

    JS_ASSERT(elementTypeBytes.isInt32());

    /* since this is the JS visible size and maybe not
     * the actual size in terms of memory layout, it is
     * always elementType.bytes * length */
    RootedValue typeBytes(cx, NumberValue(elementTypeBytes.toInt32() * length));
    if (!JSObject::defineProperty(cx, obj, cx->names().bytes,
                                  typeBytes,
                                  NULL, NULL, JSPROP_READONLY | JSPROP_PERMANENT))
        return NULL;

    RootedValue slotMemsizeVal(cx, Int32Value(::GetMemSize(cx, elementType) * length));
    obj->setFixedSlot(SLOT_MEMSIZE, slotMemsizeVal);

    RootedValue slotAlignVal(cx, Int32Value(::GetAlign(cx, elementType)));
    obj->setFixedSlot(SLOT_ALIGN, slotAlignVal);

    RootedObject prototypeObj(cx,
        SetupAndGetPrototypeObjectForComplexTypeInstance(cx, arrayTypeGlobal));

    if (!prototypeObj)
        return NULL;

    if (!LinkConstructorAndPrototype(cx, obj, prototypeObj))
        return NULL;

    JSFunction *fillFun = DefineFunctionWithReserved(cx, prototypeObj, "fill", BinaryArray::fill, 1, 0);
    if (!fillFun)
        return NULL;

    // This is important
    // so that A.prototype.fill.call(b, val)
    // where b.type != A raises an error
    SetFunctionNativeReserved(fillFun, 0, ObjectValue(*obj));

    RootedId id(cx, NON_INTEGER_ATOM_TO_JSID(cx->names().length));
    unsigned flags = JSPROP_SHARED | JSPROP_GETTER | JSPROP_PERMANENT;

    RootedObject global(cx, cx->compartment()->maybeGlobal());
    JSObject *getter =
        NewFunction(cx, NullPtr(), BinaryArray::lengthGetter,
                    0, JSFunction::NATIVE_FUN, global, NullPtr());
    if (!getter)
        return NULL;

    RootedValue value(cx);
    if (!DefineNativeProperty(cx, prototypeObj, id, value,
                                JS_DATA_TO_FUNC_PTR(PropertyOp, getter), NULL,
                                flags, 0, 0))
        return NULL;
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
    RootedObject elementType(cx, args[0].toObjectOrNull());

    if (!IsBinaryType(elementType)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BINARYDATA_ARRAYTYPE_BAD_ARGS);
        return false;
    }

    JSObject *obj = create(cx, arrayTypeGlobal, elementType, args[1].toInt32());
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

bool
DataInstanceUpdate(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             NULL, JSMSG_MORE_ARGS_NEEDED,
                             "update()", "0", "s");
        return false;
    }

    RootedObject thisObj(cx, args.thisv().isObject() ?
                                args.thisv().toObjectOrNull() : NULL);
    if (!IsBlock(thisObj)) {
        RootedValue thisObjVal(cx, ObjectValue(*thisObj));
        ReportTypeError(cx, thisObjVal, "BinaryData block");
        return false;
    }

    RootedValue val(cx, args[0]);
    uint8_t *memory = (uint8_t*) thisObj->getPrivate();
    RootedObject type(cx, GetType(thisObj));
    if (!ConvertAndCopyTo(cx, type, val, memory)) {
        ReportTypeError(cx, val, type);
        return false;
    }

    args.rval().setUndefined();
    return true;
}

static bool
FillBinaryArrayWithValue(JSContext *cx, HandleObject array, HandleValue val)
{
    JS_ASSERT(IsBinaryArray(array));

    RootedObject type(cx, GetType(array));
    RootedObject elementType(cx, ArrayType::elementType(cx, type));

    uint8_t *base = (uint8_t *) array->getPrivate();

    // set array[0] = [[Convert]](val)
    if (!ConvertAndCopyTo(cx, elementType, val, base)) {
        ReportTypeError(cx, val, elementType);
        return false;
    }

    size_t elementSize = GetMemSize(cx, elementType);
    // Copy a[0] into remaining indices.
    for (uint32_t i = 1; i < ArrayType::length(cx, type); i++) {
        uint8_t *dest = base + elementSize * i;
        memcpy(dest, base, elementSize);
    }

    return true;
}

bool
ArrayType::repeat(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             NULL, JSMSG_MORE_ARGS_NEEDED,
                             "repeat()", "0", "s");
        return false;
    }

    RootedObject thisObj(cx, args.thisv().isObject() ?
                                args.thisv().toObjectOrNull() : NULL);
    if (!IsArrayType(thisObj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "ArrayType", "repeat", InformalValueTypeName(args.thisv()));
        return false;
    }

    RootedObject binaryArray(cx, BinaryArray::create(cx, thisObj));
    if (!binaryArray)
        return false;

    RootedValue val(cx, args[0]);
    if (!FillBinaryArrayWithValue(cx, binaryArray, val))
        return false;

    args.rval().setObject(*binaryArray);
    return true;
}

bool
ArrayType::toString(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject thisObj(cx, args.thisv().isObject() ?
                                args.thisv().toObjectOrNull() : NULL);
    if (!IsArrayType(thisObj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "ArrayType", "toString", InformalValueTypeName(args.thisv()));
        return false;
    }

    StringBuffer contents(cx);
    contents.append("ArrayType(");

    RootedValue elementTypeVal(cx, ObjectValue(*elementType(cx, thisObj)));
    JSString *str = ToString<CanGC>(cx, elementTypeVal);

    contents.append(str);
    contents.append(", ");

    Value len = NumberValue(length(cx, thisObj));
    contents.append(JS_ValueToString(cx, len));
    contents.append(")");
    args.rval().setString(contents.finishString());
    return true;
}

JSObject *
BinaryArray::createEmpty(JSContext *cx, HandleObject type)
{
    JS_ASSERT(IsArrayType(type));
    RootedObject typeRooted(cx, type);

    RootedValue protoVal(cx);
    if (!JSObject::getProperty(cx, typeRooted, typeRooted,
                               cx->names().classPrototype, &protoVal))
        return NULL;

    RootedObject obj(cx,
        NewObjectWithClassProto(cx, &BinaryArray::class_,
                                protoVal.toObjectOrNull(), NULL));
    obj->setFixedSlot(SLOT_DATATYPE, ObjectValue(*type));
    obj->setFixedSlot(SLOT_BLOCKREFOWNER, NullValue());
    return obj;
}

JSObject *
BinaryArray::create(JSContext *cx, HandleObject type)
{
    RootedObject obj(cx, createEmpty(cx, type));
    if (!obj)
        return NULL;

    int32_t memsize = GetMemSize(cx, type);
    void *memory = JS_malloc(cx, memsize);
    if (!memory)
        return NULL;
    memset(memory, 0, memsize);
    obj->setPrivate(memory);
    return obj;
}

JSObject *
BinaryArray::create(JSContext *cx, HandleObject type, HandleValue initial)
{
    RootedObject obj(cx, create(cx, type));
    if (!obj)
        return NULL;

    uint8_t *memory = (uint8_t*) obj->getPrivate();
    if (!ConvertAndCopyTo(cx, type, initial, memory))
        return NULL;

    return obj;
}

JSObject *
BinaryArray::create(JSContext *cx, HandleObject type,
                    HandleObject owner, size_t offset)
{
    JS_ASSERT(IsBlock(owner));
    RootedObject obj(cx, createEmpty(cx, type));
    if (!obj)
        return NULL;

    obj->setPrivate(((uint8_t *) owner->getPrivate()) + offset);
    obj->setFixedSlot(SLOT_BLOCKREFOWNER, ObjectValue(*owner));
    return obj;
}

bool
BinaryArray::construct(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject callee(cx, &args.callee());

    if (!IsArrayType(callee)) {
        ReportTypeError(cx, args.calleev(), "is not an ArrayType");
        return false;
    }

    JSObject *obj = NULL;
    if (argc == 1) {
        RootedValue v(cx, args[0]);
        obj = create(cx, callee, v);
    } else {
        obj = create(cx, callee);
    }

    if (obj)
        args.rval().setObject(*obj);

    return obj != NULL;
}

void
BinaryArray::finalize(js::FreeOp *op, JSObject *obj)
{
    if (obj->getFixedSlot(SLOT_BLOCKREFOWNER).isNull())
        op->free_(obj->getPrivate());
}

void
BinaryArray::obj_trace(JSTracer *tracer, JSObject *obj)
{
    Value val = obj->getFixedSlot(SLOT_BLOCKREFOWNER);
    if (val.isObject()) {
        HeapPtrObject owner(val.toObjectOrNull());
        MarkObject(tracer, &owner, "binaryarray.blockRefOwner");
    }
}

bool
BinaryArray::lengthGetter(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedObject thisObj(cx, args.thisv().toObjectOrNull());
    JS_ASSERT(IsBinaryArray(thisObj));

    RootedObject type(cx, GetType(thisObj));
    vp->setInt32(ArrayType::length(cx, type));
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
bool
BinaryArray::subarray(JSContext *cx, unsigned int argc, Value *vp)
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

    RootedObject thisObj(cx, args.thisv().isObject() ?
                                args.thisv().toObjectOrNull() : NULL);
    if (!IsBinaryArray(thisObj)) {
        ReportTypeError(cx, args.thisv(), "binary array");
        return false;
    }

    RootedObject type(cx, GetType(thisObj));
    RootedObject elementType(cx, ArrayType::elementType(cx, type));
    uint32_t length = ArrayType::length(cx, type);

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

    RootedObject subArrayType(cx, ArrayType::create(cx, arrayTypeGlobal,
                                                    elementType, sublength));
    if (!subArrayType)
        return false;

    int32_t elementSize = GetMemSize(cx, elementType);
    size_t offset = elementSize * begin;

    RootedObject subarray(cx, BinaryArray::create(cx, subArrayType, thisObj, offset));
    if (!subarray)
        return false;

    args.rval().setObject(*subarray);
    return true;
}

bool
BinaryArray::fill(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             NULL, JSMSG_MORE_ARGS_NEEDED,
                             "fill()", "0", "s");
        return false;
    }

    RootedObject thisObj(cx, args.thisv().isObject() ?
                                args.thisv().toObjectOrNull() : NULL);
    if (!IsBinaryArray(thisObj)) {
        ReportTypeError(cx, args.thisv(), "binary array");
        return false;
    }

    Value funArrayTypeVal = GetFunctionNativeReserved(&args.callee(), 0);
    JS_ASSERT(funArrayTypeVal.isObject());

    RootedObject type(cx, GetType(thisObj));
    RootedObject funArrayType(cx, funArrayTypeVal.toObjectOrNull());
    if (!IsSameBinaryDataType(cx, funArrayType, type)) {
        RootedValue thisObjVal(cx, ObjectValue(*thisObj));
        ReportTypeError(cx, thisObjVal, funArrayType);
        return false;
    }

    args.rval().setUndefined();
    RootedValue val(cx, args[0]);
    return FillBinaryArrayWithValue(cx, thisObj, val);
}

JSBool
BinaryArray::obj_lookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                                MutableHandleObject objp, MutableHandleShape propp)
{
    JS_ASSERT(IsBinaryArray(obj));
    RootedObject type(cx, GetType(obj));

    uint32_t index;
    if (js_IdIsIndex(id, &index) &&
        index < ArrayType::length(cx, type)) {
        MarkNonNativePropertyFound(propp);
        objp.set(obj);
        return true;
    }

    if (JSID_IS_ATOM(id, cx->names().length)) {
        MarkNonNativePropertyFound(propp);
        objp.set(obj);
        return true;
    }

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        objp.set(NULL);
        propp.set(NULL);
        return true;
    }

    return JSObject::lookupGeneric(cx, proto, id, objp, propp);
}

JSBool
BinaryArray::obj_lookupProperty(JSContext *cx,
                                HandleObject obj,
                                HandlePropertyName name,
                                MutableHandleObject objp,
                                MutableHandleShape propp)
{
    RootedId id(cx, NameToId(name));
    return obj_lookupGeneric(cx, obj, id, objp, propp);
}

JSBool
BinaryArray::obj_lookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                                MutableHandleObject objp, MutableHandleShape propp)
{
    JS_ASSERT(IsBinaryArray(obj));
    RootedObject type(cx, GetType(obj));

    if (index < ArrayType::length(cx, type)) {
        MarkNonNativePropertyFound(propp);
        objp.set(obj);
        return true;
    }

    RootedObject proto(cx, obj->getProto());
    if (proto)
        return JSObject::lookupElement(cx, proto, index, objp, propp);

    objp.set(NULL);
    propp.set(NULL);
    return true;
}

JSBool
BinaryArray::obj_lookupSpecial(JSContext *cx, HandleObject obj,
                               HandleSpecialId sid, MutableHandleObject objp,
                               MutableHandleShape propp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return obj_lookupGeneric(cx, obj, id, objp, propp);
}

JSBool
BinaryArray::obj_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver,
                             HandleId id, MutableHandleValue vp)
{
    uint32_t index;
    if (js_IdIsIndex(id, &index)) {
        return obj_getElement(cx, obj, receiver, index, vp);
    }

    RootedValue idValue(cx, IdToValue(id));
    Rooted<PropertyName*> name(cx, ToAtom<CanGC>(cx, idValue)->asPropertyName());
    return obj_getProperty(cx, obj, receiver, name, vp);
}

JSBool
BinaryArray::obj_getProperty(JSContext *cx, HandleObject obj, HandleObject receiver,
                              HandlePropertyName name, MutableHandleValue vp)
{
    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        vp.setUndefined();
        return true;
    }

    return JSObject::getProperty(cx, proto, receiver, name, vp);
}

JSBool
BinaryArray::obj_getElement(JSContext *cx, HandleObject obj, HandleObject receiver,
                             uint32_t index, MutableHandleValue vp)
{
    RootedObject type(cx, GetType(obj));

    if (index < ArrayType::length(cx, type)) {
        RootedObject elementType(cx, ArrayType::elementType(cx, type));
        size_t offset = GetMemSize(cx, elementType) * index;
        return Reify(cx, elementType, obj, offset, vp);
    }

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        vp.setUndefined();
        return true;
    }

    return JSObject::getElement(cx, proto, receiver, index, vp);
}

JSBool
BinaryArray::obj_getElementIfPresent(JSContext *cx, HandleObject obj,
                                     HandleObject receiver, uint32_t index,
                                     MutableHandleValue vp, bool *present)
{
    RootedObject type(cx, GetType(obj));

    if (index < ArrayType::length(cx, type)) {
        *present = true;
        return obj_getElement(cx, obj, receiver, index, vp);
    }

    *present = false;
    vp.setUndefined();
    return true;
}

JSBool
BinaryArray::obj_getSpecial(JSContext *cx, HandleObject obj,
                            HandleObject receiver, HandleSpecialId sid,
                            MutableHandleValue vp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return obj_getGeneric(cx, obj, receiver, id, vp);
}

JSBool
BinaryArray::obj_setGeneric(JSContext *cx, HandleObject obj, HandleId id,
                             MutableHandleValue vp, JSBool strict)
{
	uint32_t index;
	if (js_IdIsIndex(id, &index)) {
	    return obj_setElement(cx, obj, index, vp, strict);
    }

    if (JSID_IS_ATOM(id, cx->names().length)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             NULL, JSMSG_CANT_REDEFINE_ARRAY_LENGTH);
        return false;
    }

	return true;
}

JSBool
BinaryArray::obj_setProperty(JSContext *cx, HandleObject obj,
                             HandlePropertyName name, MutableHandleValue vp,
                             JSBool strict)
{
    RootedId id(cx, NameToId(name));
    return obj_setGeneric(cx, obj, id, vp, strict);
}

JSBool
BinaryArray::obj_setElement(JSContext *cx, HandleObject obj, uint32_t index,
                             MutableHandleValue vp, JSBool strict)
{
    RootedObject type(cx, GetType(obj));
    if (index >= ArrayType::length(cx, type)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             NULL, JSMSG_BINARYDATA_BINARYARRAY_BAD_INDEX);
        return false;
    }

    RootedValue elementTypeVal(cx);
    if (!JSObject::getProperty(cx, type, type, cx->names().elementType,
                               &elementTypeVal))
        return false;

    RootedObject elementType(cx, elementTypeVal.toObjectOrNull());
    uint32_t offset = GetMemSize(cx, elementType) * index;

    bool result =
        ConvertAndCopyTo(cx, elementType, vp,
                         ((uint8_t*) obj->getPrivate()) + offset );

    if (!result) {
        return false;
    }

    return true;
}

JSBool
BinaryArray::obj_setSpecial(JSContext *cx, HandleObject obj,
                             HandleSpecialId sid, MutableHandleValue vp,
                             JSBool strict)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return obj_setGeneric(cx, obj, id, vp, strict);
}

JSBool
BinaryArray::obj_getGenericAttributes(JSContext *cx, HandleObject obj,
                                       HandleId id, unsigned *attrsp)
{
    uint32_t index;
    RootedObject type(cx, GetType(obj));

    if (js_IdIsIndex(id, &index) &&
        index < ArrayType::length(cx, type)) {
        *attrsp = JSPROP_ENUMERATE | JSPROP_PERMANENT; // should we report JSPROP_INDEX?
        return true;
    }

    if (JSID_IS_ATOM(id, cx->names().length)) {
        *attrsp = JSPROP_READONLY | JSPROP_PERMANENT;
        return true;
    }

    return false;
}

JSBool
BinaryArray::obj_getPropertyAttributes(JSContext *cx, HandleObject obj,
                                        HandlePropertyName name,
                                        unsigned *attrsp)
{
    RootedId id(cx, NameToId(name));
    return obj_getGenericAttributes(cx, obj, id, attrsp);
}

JSBool
BinaryArray::obj_getElementAttributes(JSContext *cx, HandleObject obj,
                                       uint32_t index, unsigned *attrsp)
{
    RootedId id(cx, INT_TO_JSID(index));
    return obj_getGenericAttributes(cx, obj, id, attrsp);
}

JSBool
BinaryArray::obj_getSpecialAttributes(JSContext *cx, HandleObject obj,
                                       HandleSpecialId sid, unsigned *attrsp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return obj_getGenericAttributes(cx, obj, id, attrsp);
}

JSBool
BinaryArray::obj_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
                            MutableHandleValue statep, MutableHandleId idp)
{
    JS_ASSERT(IsBinaryArray(obj));

    RootedObject type(cx, GetType(obj));

    uint32_t index;
    switch (enum_op) {
        case JSENUMERATE_INIT_ALL:
        case JSENUMERATE_INIT:
            statep.setInt32(0);
            idp.set(INT_TO_JSID(ArrayType::length(cx, type)));
            break;

        case JSENUMERATE_NEXT:
            index = static_cast<uint32_t>(statep.toInt32());

            if (index < ArrayType::length(cx, type)) {
                idp.set(INT_TO_JSID(index));
                statep.setInt32(index + 1);
            } else {
                JS_ASSERT(index == ArrayType::length(cx, type));
                statep.setNull();
            }

            break;

        case JSENUMERATE_DESTROY:
            statep.setNull();
            break;
    }

	return true;
}

/*********************************
 * Structs
 *********************************/
Class StructType::class_ = {
    "StructType",
    JSCLASS_HAS_RESERVED_SLOTS(TYPE_RESERVED_SLOTS) |
    JSCLASS_HAS_PRIVATE | // used to store FieldList
    JSCLASS_HAS_CACHED_PROTO(JSProto_StructType),
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    StructType::finalize,
    NULL, /* checkAccess */
    NULL, /* call */
    NULL, /* hasInstance */
    BinaryStruct::construct,
    StructType::trace
};

Class BinaryStruct::class_ = {
    "BinaryStruct",
    Class::NON_NATIVE |
    JSCLASS_HAS_RESERVED_SLOTS(BLOCK_RESERVED_SLOTS) |
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_StructType),
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    BinaryStruct::finalize,
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* hasInstance */
    NULL,           /* construct   */
    BinaryStruct::obj_trace,
    JS_NULL_CLASS_EXT,
    {
        NULL, /* lookupGeneric */
        NULL, /* lookupProperty */
        NULL, /* lookupElement */
        NULL, /* lookupSpecial */
        NULL, /* defineGeneric */
        NULL, /* defineProperty */
        NULL, /* defineElement */
        NULL, /* defineSpecial */
        BinaryStruct::obj_getGeneric,
        BinaryStruct::obj_getProperty,
        NULL, /* getElement */
        NULL, /* getElementIfPresent */
        BinaryStruct::obj_getSpecial,
        BinaryStruct::obj_setGeneric,
        BinaryStruct::obj_setProperty,
        NULL, /* setElement */
        NULL, /* setSpecial */
        NULL, /* getGenericAttributes */
        NULL, /* getPropertyAttributes */
        NULL, /* getElementAttributes */
        NULL, /* getSpecialAttributes */
        NULL, /* setGenericAttributes */
        NULL, /* setPropertyAttributes */
        NULL, /* setElementAttributes */
        NULL, /* setSpecialAttributes */
        NULL, /* deleteProperty */
        NULL, /* deleteElement */
        NULL, /* deleteSpecial */
        BinaryStruct::obj_enumerate,
        NULL, /* thisObject */
    }
};

/*
 * NOTE: layout() does not check for duplicates in fields since the arguments
 * to StructType are currently passed as an object literal. Fix this if it
 * changes to taking an array of arrays.
 */
bool
StructType::layout(JSContext *cx, HandleObject structType, HandleObject fields)
{
    AutoIdVector fieldProps(cx);
    if (!GetPropertyNames(cx, fields, JSITER_OWNONLY, &fieldProps))
        return false;

    if (fieldProps.length() <= 0) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BINARYDATA_STRUCTTYPE_EMPTY_DESCRIPTOR);
        return false;
    }

    // All error branches from here onwards should |goto error;| to free this list.
    FieldList *fieldList = js_new<FieldList>(cx);
    fieldList->resize(fieldProps.length());

    uint32_t structAlign = 0;
    uint32_t structMemSize = 0;
    uint32_t structByteSize = 0;
    size_t structTail = 0;

    for (unsigned int i = 0; i < fieldProps.length(); i++) {
        RootedValue fieldTypeVal(cx);
        RootedId id(cx, fieldProps[i]);
        if (!JSObject::getGeneric(cx, fields, fields, id, &fieldTypeVal))
            goto error;

        if (!fieldTypeVal.isObject()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BINARYDATA_STRUCTTYPE_BAD_FIELD, JSID_TO_STRING(id));
            goto error;
        }

        RootedObject fieldType(cx, fieldTypeVal.toObjectOrNull());
        if (!IsBinaryType(fieldType)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BINARYDATA_STRUCTTYPE_BAD_FIELD, JSID_TO_STRING(id));
            goto error;
        }

        size_t fieldMemSize = GetMemSize(cx, fieldType);
        size_t fieldAlign = GetAlign(cx, fieldType);
        size_t fieldOffset = AlignBytes(structMemSize, fieldAlign);

        structMemSize = fieldOffset + fieldMemSize;

        if (fieldAlign > structAlign)
            structAlign = fieldAlign;

        // If the field type is a BinaryType and we can't get its bytes, we have a problem.
        RootedValue fieldTypeBytes(cx);
        DebugOnly<bool> r = JSObject::getProperty(cx, fieldType, fieldType, cx->names().bytes, &fieldTypeBytes);
        JS_ASSERT(r);

        JS_ASSERT(fieldTypeBytes.isInt32());
        structByteSize += fieldTypeBytes.toInt32();

        (*fieldList)[i].name = fieldProps[i];
        (*fieldList)[i].type = fieldType.get();
        (*fieldList)[i].offset = fieldOffset;
    }

    structTail = AlignBytes(structMemSize, structAlign);
    JS_ASSERT(structTail >= structMemSize);
    structMemSize = structTail;

    structType->setFixedSlot(SLOT_MEMSIZE, Int32Value(structMemSize));
    structType->setFixedSlot(SLOT_ALIGN, Int32Value(structAlign));
    structType->setPrivate(fieldList);

    if (!JS_DefineProperty(cx, structType, "bytes",
                           Int32Value(structByteSize), NULL, NULL,
                           JSPROP_READONLY | JSPROP_PERMANENT))
        goto error;

    return true;

error:
    delete fieldList;
    return false;
}

bool
StructType::convertAndCopyTo(JSContext *cx, HandleObject exemplar,
                             HandleValue from, uint8_t *mem)
{

    if (!from.isObject()) {
        return ReportTypeError(cx, from, exemplar);
    }

    RootedObject val(cx, from.toObjectOrNull());
    if (IsBlock(val)) {
        RootedObject type(cx, GetType(val));
        if (IsSameBinaryDataType(cx, exemplar, type)) {
            uint8_t *priv = (uint8_t*) val->getPrivate();
            memcpy(mem, priv, GetMemSize(cx, exemplar));
            return true;
        }
        return ReportTypeError(cx, from, exemplar);
    }

    RootedObject valRooted(cx, val);
    AutoIdVector ownProps(cx);
    if (!GetPropertyNames(cx, valRooted, JSITER_OWNONLY, &ownProps))
        return ReportTypeError(cx, from, exemplar);

    FieldList *fieldList = GetStructTypeFieldList(exemplar);

    if (ownProps.length() != fieldList->length()) {
        return ReportTypeError(cx, from, exemplar);
    }

    for (unsigned int i = 0; i < ownProps.length(); i++) {
        FieldInfo *info = NULL;
        if (!LookupFieldList(fieldList, ownProps[i], &info)) {
            return ReportTypeError(cx, from, exemplar);
        }
    }

    for (uint32_t i = 0; i < fieldList->length(); ++i) {
        FieldInfo *info = &(*fieldList)[i];
        RootedPropertyName fieldName(cx, JSID_TO_ATOM(info->name)->asPropertyName());

        RootedValue fromProp(cx);
        if (!JSObject::getProperty(cx, valRooted, valRooted, fieldName, &fromProp))
            return ReportTypeError(cx, from, exemplar);

        RootedObject fieldType(cx, info->type);
        if (!ConvertAndCopyTo(cx, fieldType, fromProp, mem + info->offset))
            return false;
    }
    return true;
}

bool
StructType::reify(JSContext *cx, HandleObject type, HandleObject owner,
                  size_t offset, MutableHandleValue to) {
    JSObject *obj = BinaryStruct::create(cx, type, owner, offset);
    if (!obj)
        return false;
    to.setObject(*obj);
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

    RootedObject clone(cx, CloneObject(cx, fields, fieldsProto, NullPtr()));
    if (!clone)
        return NULL;

    if (!JS_DefineProperty(cx, obj, "fields",
                           ObjectValue(*fields), NULL, NULL,
                           JSPROP_READONLY | JSPROP_PERMANENT))
        return NULL;

    JSObject *prototypeObj =
        SetupAndGetPrototypeObjectForComplexTypeInstance(cx, structTypeGlobal);

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
        RootedObject fields(cx, args[0].toObjectOrNull());
        JSObject *obj = create(cx, structTypeGlobal, fields);
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                         JSMSG_BINARYDATA_STRUCTTYPE_BAD_ARGS);
    return false;
}

void
StructType::finalize(FreeOp *op, JSObject *obj)
{
    FieldList *fieldList = static_cast<FieldList *>(obj->getPrivate());
    js_delete(fieldList);
}

void
StructType::trace(JSTracer *tracer, JSObject *obj)
{
    FieldList *fieldList = static_cast<FieldList *>(obj->getPrivate());
    JS_ASSERT(fieldList);
    for (uint32_t i = 0; i < fieldList->length(); ++i) {
        FieldInfo *info = &(*fieldList)[i];
        gc::MarkId(tracer, &(info->name), "structtype.field.name");
        MarkObject(tracer, &(info->type), "structtype.field.type");
    }
}

bool
StructType::toString(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.thisv().isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "StructType", "toString", InformalValueTypeName(args.thisv()));
        return false;
    }

    RootedObject thisObj(cx, args.thisv().toObjectOrNull());

    if (!IsStructType(thisObj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "StructType", "toString", InformalValueTypeName(args.thisv()));
        return false;
    }

    StringBuffer contents(cx);
    contents.append("StructType({");

    FieldList *fieldList = GetStructTypeFieldList(thisObj);
    JS_ASSERT(fieldList);

    for (uint32_t i = 0; i < fieldList->length(); ++i) {
        FieldInfo *info = &(*fieldList)[i];
        if (i != 0)
            contents.append(", ");

        contents.append(IdToString(cx, info->name));
        contents.append(": ");

        RootedValue fieldStringVal(cx);
        if (!JS_CallFunctionName(cx, info->type,
                                 "toString", 0, NULL, fieldStringVal.address()))
            return false;

        contents.append(fieldStringVal.toString());
    }

    contents.append("})");

    args.rval().setString(contents.finishString());
    return true;
}

JSObject *
BinaryStruct::createEmpty(JSContext *cx, HandleObject type)
{
    JS_ASSERT(IsStructType(type));
    RootedObject typeRooted(cx, type);

    RootedValue protoVal(cx);
    if (!JSObject::getProperty(cx, typeRooted, typeRooted,
                               cx->names().classPrototype, &protoVal))
        return NULL;

    RootedObject obj(cx,
        NewObjectWithClassProto(cx, &BinaryStruct::class_,
                                protoVal.toObjectOrNull(), NULL));

    obj->setFixedSlot(SLOT_DATATYPE, ObjectValue(*type));
    obj->setFixedSlot(SLOT_BLOCKREFOWNER, NullValue());
    return obj;
}

JSObject *
BinaryStruct::create(JSContext *cx, HandleObject type)
{
    RootedObject obj(cx, createEmpty(cx, type));
    if (!obj)
        return NULL;

    int32_t memsize = GetMemSize(cx, type);
    void *memory = JS_malloc(cx, memsize);
    if (!memory)
        return NULL;
    memset(memory, 0, memsize);
    obj->setPrivate(memory);
    return obj;
}

JSObject *
BinaryStruct::create(JSContext *cx, HandleObject type,
                     HandleObject owner, size_t offset)
{
    JS_ASSERT(IsBlock(owner));
    RootedObject obj(cx, createEmpty(cx, type));
    if (!obj)
        return NULL;

    obj->setPrivate(static_cast<uint8_t*>(owner->getPrivate()) + offset);
    obj->setFixedSlot(SLOT_BLOCKREFOWNER, ObjectValue(*owner));
    return obj;
}

bool
BinaryStruct::construct(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject callee(cx, &args.callee());

    if (!IsStructType(callee)) {
        ReportTypeError(cx, args.calleev(), "is not an StructType");
        return false;
    }

    RootedObject obj(cx, create(cx, callee));

    if (obj)
        args.rval().setObject(*obj);

    return obj != NULL;
}

void
BinaryStruct::finalize(js::FreeOp *op, JSObject *obj)
{
    if (obj->getFixedSlot(SLOT_BLOCKREFOWNER).isNull())
        op->free_(obj->getPrivate());
}

void
BinaryStruct::obj_trace(JSTracer *tracer, JSObject *obj)
{
    Value val = obj->getFixedSlot(SLOT_BLOCKREFOWNER);
    if (val.isObject()) {
        HeapPtrObject owner(val.toObjectOrNull());
        MarkObject(tracer, &owner, "binarystruct.blockRefOwner");
    }

    HeapPtrObject type(obj->getFixedSlot(SLOT_DATATYPE).toObjectOrNull());
    MarkObject(tracer, &type, "binarystruct.type");
}

JSBool
BinaryStruct::obj_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
                            MutableHandleValue statep, MutableHandleId idp)
{
    JS_ASSERT(IsBinaryStruct(obj));

    RootedObject type(cx, GetType(obj));

    FieldList *fieldList = GetStructTypeFieldList(type);
    JS_ASSERT(fieldList);

    uint32_t index;
    switch (enum_op) {
        case JSENUMERATE_INIT_ALL:
        case JSENUMERATE_INIT:
            statep.setInt32(0);
            idp.set(INT_TO_JSID(fieldList->length()));
            break;

        case JSENUMERATE_NEXT:
            index = static_cast<uint32_t>(statep.toInt32());

            if (index < fieldList->length()) {
                FieldInfo *info = &(*fieldList)[index];
                idp.set(info->name);
                statep.setInt32(index + 1);
            } else {
                statep.setNull();
            }

            break;

        case JSENUMERATE_DESTROY:
            statep.setNull();
            break;
    }

    return true;
}

JSBool
BinaryStruct::obj_getGeneric(JSContext *cx, HandleObject obj,
                             HandleObject receiver, HandleId id,
                             MutableHandleValue vp)
{
    if (!IsBinaryStruct(obj)) {
        char *valueStr = JS_EncodeString(cx, JS_ValueToString(cx, ObjectValue(*obj)));
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSMSG_BINARYDATA_NOT_BINARYSTRUCT, valueStr);
        JS_free(cx, (void *) valueStr);
        return false;
    }

    RootedObject type(cx, GetType(obj));
    JS_ASSERT(IsStructType(type));

    FieldList *fieldList = GetStructTypeFieldList(type);
    JS_ASSERT(fieldList);

    FieldInfo *fieldInfo = NULL;
    if (!LookupFieldList(fieldList, id, &fieldInfo)) {
        RootedObject proto(cx, obj->getProto());
        if (!proto) {
            vp.setUndefined();
            return true;
        }

        return JSObject::getGeneric(cx, proto, receiver, id, vp);
    }

    RootedObject fieldType(cx, fieldInfo->type);
    return Reify(cx, fieldType, obj, fieldInfo->offset, vp);
}

JSBool
BinaryStruct::obj_getProperty(JSContext *cx, HandleObject obj,
                              HandleObject receiver, HandlePropertyName name,
                              MutableHandleValue vp)
{
    RootedId id(cx, NON_INTEGER_ATOM_TO_JSID(&(*name)));
    return obj_getGeneric(cx, obj, receiver, id, vp);
}

JSBool
BinaryStruct::obj_getSpecial(JSContext *cx, HandleObject obj,
                             HandleObject receiver, HandleSpecialId sid,
                             MutableHandleValue vp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return obj_getGeneric(cx, obj, receiver, id, vp);
}

JSBool
BinaryStruct::obj_setGeneric(JSContext *cx, HandleObject obj, HandleId id,
                             MutableHandleValue vp, JSBool strict)
{
    if (!IsBinaryStruct(obj)) {
        char *valueStr = JS_EncodeString(cx, JS_ValueToString(cx, ObjectValue(*obj)));
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                JSMSG_BINARYDATA_NOT_BINARYSTRUCT, valueStr);
        JS_free(cx, (void *) valueStr);
        return false;
    }

    RootedObject type(cx, GetType(obj));
    JS_ASSERT(IsStructType(type));

    FieldList *fieldList = GetStructTypeFieldList(type);
    JS_ASSERT(fieldList);

    FieldInfo *fieldInfo = NULL;
    if (!LookupFieldList(fieldList, id, &fieldInfo)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_UNDEFINED_PROP, IdToString(cx, id));
        return false;
    }

    uint8_t *loc = static_cast<uint8_t*>(obj->getPrivate()) + fieldInfo->offset;

    RootedObject fieldType(cx, fieldInfo->type);
    if (!ConvertAndCopyTo(cx, fieldType, vp, loc))
        return false;

    return true;
}

JSBool
BinaryStruct::obj_setProperty(JSContext *cx, HandleObject obj,
                              HandlePropertyName name, MutableHandleValue vp,
                              JSBool strict)
{
    RootedId id(cx, NON_INTEGER_ATOM_TO_JSID(&(*name)));
    return obj_setGeneric(cx, obj, id, vp, strict);
}

static bool
Reify(JSContext *cx, HandleObject type,
      HandleObject owner, size_t offset, MutableHandleValue to)
{
    if (IsArrayType(type)) {
        return ArrayType::reify(cx, type, owner, offset, to);
    } else if (IsStructType(type)) {
        return StructType::reify(cx, type, owner, offset, to);
    }

    JS_ASSERT(&NumericTypeClasses[NUMERICTYPE_UINT8] <= type->getClass() &&
              type->getClass() <= &NumericTypeClasses[NUMERICTYPE_FLOAT64]);

#define REIFY_CASES(constant_, type_)\
        case constant_:\
            return NumericType<type_##_t>::reify(cx,\
                    ((uint8_t *) owner->getPrivate()) + offset, to);

    switch(type->getFixedSlot(SLOT_DATATYPE).toInt32()) {
        BINARYDATA_FOR_EACH_NUMERIC_TYPES(REIFY_CASES);
        default:
            abort();
    }
#undef REIFY_CASES
    return false;
}

static bool
ConvertAndCopyTo(JSContext *cx, HandleObject type, HandleValue from, uint8_t *mem)
{
    if (IsComplexType(type)) {
        if (IsArrayType(type)) {
            if (!ArrayType::convertAndCopyTo(cx, type, from, mem))
                return false;
        } else if (IsStructType(type)) {
            if (!StructType::convertAndCopyTo(cx, type, from, mem))
                return false;
        } else {
            MOZ_ASSUME_UNREACHABLE("Unexpected complex BinaryData type!");
        }

        return true;
    }

    JS_ASSERT(&NumericTypeClasses[NUMERICTYPE_UINT8] <= type->getClass() &&
              type->getClass() <= &NumericTypeClasses[NUMERICTYPE_FLOAT64]);

#define CONVERT_CASES(constant_, type_)\
        case constant_:\
                       {\
            type_##_t temp;\
            bool ok = NumericType<type_##_t>::convert(cx, from, &temp);\
            if (!ok)\
                return false;\
            memcpy(mem, &temp, sizeof(type_##_t));\
            return true; }

    switch(type->getFixedSlot(0).toInt32()) {
        BINARYDATA_FOR_EACH_NUMERIC_TYPES(CONVERT_CASES);
        default:
            abort();
    }
#undef CONVERT_CASES
    return false;
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
SetupComplexHeirarchy(JSContext *cx, HandleObject obj, JSProtoKey protoKey,
                      HandleObject complexObject, MutableHandleObject proto,
                      MutableHandleObject protoProto)
{
    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());
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

    RootedObject DataProto(cx, DataProtoVal.toObjectOrNull());
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

static JSObject *
InitArrayType(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->isNative());
    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());
    RootedObject ctor(cx, global->getOrCreateArrayTypeObject(cx));
    if (!ctor)
        return NULL;

    RootedObject proto(cx);
    RootedObject protoProto(cx);
    if (!SetupComplexHeirarchy(cx, obj, JSProto_ArrayType,
                               ctor, &proto, &protoProto))
        return NULL;

    if (!JS_DefineFunction(cx, proto, "repeat", ArrayType::repeat, 1, 0))
        return NULL;

    if (!JS_DefineFunction(cx, proto, "toString", ArrayType::toString, 0, 0))
        return NULL;

    RootedObject arrayProto(cx);
    if (!FindProto(cx, &ArrayObject::class_, &arrayProto))
        return NULL;

    RootedValue forEachFunVal(cx);
    RootedAtom forEachAtom(cx, Atomize(cx, "forEach", 7));
    RootedId forEachId(cx, AtomToId(forEachAtom));
    if (!JSObject::getProperty(cx, arrayProto, arrayProto, forEachAtom->asPropertyName(), &forEachFunVal))
        return NULL;

    if (!JSObject::defineGeneric(cx, protoProto, forEachId, forEachFunVal, NULL, NULL, 0))
        return NULL;

    if (!JS_DefineFunction(cx, protoProto, "subarray",
                           BinaryArray::subarray, 1, 0))
        return NULL;

    return proto;
}

static JSObject *
InitStructType(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->isNative());
    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());
    RootedFunction ctor(cx,
        global->createConstructor(cx, StructType::construct,
                                  cx->names().StructType, 1));

    if (!ctor)
        return NULL;

    RootedObject proto(cx);
    RootedObject protoProto(cx);
    if (!SetupComplexHeirarchy(cx, obj, JSProto_StructType,
                               ctor, &proto, &protoProto))
        return NULL;

    if (!JS_DefineFunction(cx, proto, "toString", StructType::toString, 0, 0))
        return NULL;

    return proto;
}

JSObject *
js_InitBinaryDataClasses(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->is<GlobalObject>());
    Rooted<GlobalObject *> global(cx, &obj->as<GlobalObject>());

    RootedObject funProto(cx, JS_GetFunctionPrototype(cx, global));
#define BINARYDATA_NUMERIC_DEFINE(constant_, type_)\
    do {\
        RootedObject numFun(cx, JS_DefineObject(cx, global, #type_,\
                    (JSClass *) &NumericTypeClasses[constant_], funProto, 0));\
\
        if (!numFun)\
            return NULL;\
\
        numFun->setFixedSlot(SLOT_DATATYPE, Int32Value(constant_));\
\
        RootedValue sizeVal(cx, NumberValue(sizeof(type_##_t)));\
        if (!JSObject::defineProperty(cx, numFun, cx->names().bytes,\
                                      sizeVal,\
                                      NULL, NULL,\
                                      JSPROP_READONLY | JSPROP_PERMANENT))\
            return NULL;\
\
        if (!JS_DefineFunction(cx, numFun, "toString",\
                               NumericTypeToString<constant_>, 0, 0))\
            return NULL;\
    } while(0);
    BINARYDATA_FOR_EACH_NUMERIC_TYPES(BINARYDATA_NUMERIC_DEFINE)
#undef BINARYDATA_NUMERIC_DEFINE

    if (!InitArrayType(cx, obj))
        return NULL;

    if (!InitStructType(cx, obj))
        return NULL;

    return global;
}
