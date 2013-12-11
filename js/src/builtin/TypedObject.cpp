/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/TypedObject.h"

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

#include "vm/Shape-inl.h"

using mozilla::DebugOnly;

using namespace js;

const Class js::TypedObjectClass = {
    "TypedObject",
    JSCLASS_HAS_CACHED_PROTO(JSProto_TypedObject),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

static const JSFunctionSpec TypedObjectMethods[] = {
    JS_SELF_HOSTED_FN("objectType", "TypeOfTypedDatum", 1, 0),
    JS_FS_END
};

static void
ReportCannotConvertTo(JSContext *cx, HandleValue fromValue, const char *toType)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
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
        return nullptr;

    return &value.toObject();
}

static inline bool
IsScalarTypeObject(JSObject &type)
{
    return type.hasClass(&ScalarType::class_);
}

static inline bool
IsReferenceTypeObject(JSObject &type)
{
    return type.hasClass(&ReferenceType::class_);
}

static inline bool
IsSimpleTypeObject(JSObject &type)
{
    return IsScalarTypeObject(type) || IsReferenceTypeObject(type);
}

static inline bool
IsArrayTypeObject(JSObject &type)
{
    return type.hasClass(&ArrayType::class_);
}

static inline bool
IsStructTypeObject(JSObject &type)
{
    return type.hasClass(&StructType::class_);
}

static inline bool
IsX4TypeObject(JSObject &type)
{
    return type.hasClass(&X4Type::class_);
}

static inline bool
IsComplexTypeObject(JSObject &type)
{
    return IsArrayTypeObject(type) || IsStructTypeObject(type);
}

static inline bool
IsTypeObject(JSObject &type)
{
    return IsScalarTypeObject(type) ||
           IsReferenceTypeObject(type) ||
           IsX4TypeObject(type) ||
           IsComplexTypeObject(type);
}

static inline bool
IsTypedDatum(JSObject &obj)
{
    return obj.is<TypedObject>() || obj.is<TypedHandle>();
}

static inline uint8_t *
TypedMem(JSObject &val)
{
    JS_ASSERT(IsTypedDatum(val));
    return (uint8_t*) val.getPrivate();
}

/*
 * Given a user-visible type descriptor object, returns the
 * owner object for the TypeRepresentation* that we use internally.
 */
static JSObject *
typeRepresentationOwnerObj(JSObject &typeObj)
{
    JS_ASSERT(IsTypeObject(typeObj));
    return &typeObj.getReservedSlot(JS_TYPEOBJ_SLOT_TYPE_REPR).toObject();
}

/*
 * Given a user-visible type descriptor object, returns the
 * TypeRepresentation* that we use internally.
 *
 * Note: this pointer is valid only so long as `typeObj` remains rooted.
 */
static TypeRepresentation *
typeRepresentation(JSObject &typeObj)
{
    return TypeRepresentation::fromOwnerObject(*typeRepresentationOwnerObj(typeObj));
}

static inline JSObject *
GetType(JSObject &datum)
{
    JS_ASSERT(IsTypedDatum(datum));
    return &datum.getReservedSlot(JS_DATUM_SLOT_TYPE_OBJ).toObject();
}

static bool
TypeObjectToSource(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject thisObj(cx, ToObjectIfObject(args.thisv()));
    if (!thisObj || !IsTypeObject(*thisObj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             nullptr, JSMSG_TYPEDOBJECT_HANDLE_BAD_ARGS,
                             "this", "type object");
        return false;
    }

    StringBuffer contents(cx);
    if (!typeRepresentation(*thisObj)->appendString(cx, contents))
        return false;

    RootedString result(cx, contents.finishString());
    if (!result)
        return false;

    args.rval().setString(result);
    return true;
}

/*
 * Overwrites the contents of `datum` at offset `offset` with `val`
 * converted to the type `typeObj`. This is done by delegating to
 * self-hosted code. This is used for assignments and initializations.
 *
 * For example, consider the final assignment in this snippet:
 *
 *    var Point = new StructType({x: float32, y: float32});
 *    var Line = new StructType({from: Point, to: Point});
 *    var line = new Line();
 *    line.to = {x: 22, y: 44};
 *
 * This would result in a call to `ConvertAndCopyTo`
 * where:
 * - typeObj = Point
 * - datum = line
 * - offset = sizeof(Point) == 8
 * - val = {x: 22, y: 44}
 * This would result in loading the value of `x`, converting
 * it to a float32, and hen storing it at the appropriate offset,
 * and then doing the same for `y`.
 *
 * Note that the type of `typeObj` may not be the
 * type of `datum` but rather some subcomponent of `datum`.
 */
static bool
ConvertAndCopyTo(JSContext *cx,
                 HandleObject typeObj,
                 HandleObject datum,
                 int32_t offset,
                 HandleValue val)
{
    JS_ASSERT(IsTypeObject(*typeObj));
    JS_ASSERT(IsTypedDatum(*datum));

    RootedFunction func(
        cx, SelfHostedFunction(cx, cx->names().ConvertAndCopyTo));
    if (!func)
        return false;

    InvokeArgs args(cx);
    if (!args.init(5))
        return false;

    args.setCallee(ObjectValue(*func));
    args[0].setObject(*typeRepresentationOwnerObj(*typeObj));
    args[1].setObject(*typeObj);
    args[2].setObject(*datum);
    args[3].setInt32(offset);
    args[4].set(val);

    return Invoke(cx, args);
}

static bool
ConvertAndCopyTo(JSContext *cx, HandleObject datum, HandleValue val)
{
    RootedObject type(cx, GetType(*datum));
    return ConvertAndCopyTo(cx, type, datum, 0, val);
}

/*
 * Overwrites the contents of `datum` at offset `offset` with `val`
 * converted to the type `typeObj`
 */
static bool
Reify(JSContext *cx,
      TypeRepresentation *typeRepr,
      HandleObject type,
      HandleObject datum,
      size_t offset,
      MutableHandleValue to)
{
    JS_ASSERT(IsTypeObject(*type));
    JS_ASSERT(IsTypedDatum(*datum));

    RootedFunction func(cx, SelfHostedFunction(cx, cx->names().Reify));
    if (!func)
        return false;

    InvokeArgs args(cx);
    if (!args.init(4))
        return false;

    args.setCallee(ObjectValue(*func));
    args[0].setObject(*typeRepr->ownerObject());
    args[1].setObject(*type);
    args[2].setObject(*datum);
    args[3].setInt32(offset);

    if (!Invoke(cx, args))
        return false;

    to.set(args.rval());
    return true;
}

static inline size_t
TypeSize(JSObject &type)
{
    return typeRepresentation(type)->size();
}

static inline size_t
DatumSize(JSObject &val)
{
    return TypeSize(*GetType(val));
}

static inline bool
IsTypedDatumOfKind(JSObject &obj, TypeRepresentation::Kind kind)
{
    if (!IsTypedDatum(obj))
        return false;
    TypeRepresentation *repr = typeRepresentation(*GetType(obj));
    return repr->kind() == kind;
}

/***************************************************************************
 * Scalar type objects
 *
 * Scalar type objects like `uint8`, `uint16`, are all instances of
 * the ScalarType class. Like all type objects, they have a reserved
 * slot pointing to a TypeRepresentation object, which is used to
 * distinguish which scalar type object this actually is.
 */

const Class js::ScalarType::class_ = {
    "Scalar",
    JSCLASS_HAS_RESERVED_SLOTS(JS_TYPEOBJ_SCALAR_SLOTS),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,
    nullptr,
    ScalarType::call,
    nullptr,
    nullptr,
    nullptr
};

const JSFunctionSpec js::ScalarType::typeObjectMethods[] = {
    JS_FN("toSource", TypeObjectToSource, 0, 0),
    {"handle", {nullptr, nullptr}, 2, 0, "HandleCreate"},
    {"equivalent", {nullptr, nullptr}, 1, 0, "TypeObjectEquivalent"},
    JS_FS_END
};

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

bool
ScalarType::call(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             args.callee().getClass()->name, "0", "s");
        return false;
    }

    ScalarTypeRepresentation *typeRepr = typeRepresentation(args.callee())->asScalar();
    ScalarTypeRepresentation::Type type = typeRepr->type();

    double number;
    if (!ToNumber(cx, args[0], &number))
        return false;

    if (type == ScalarTypeRepresentation::TYPE_UINT8_CLAMPED)
        number = ClampDoubleToUint8(number);

    switch (type) {
#define SCALARTYPE_CALL(constant_, type_, name_)                             \
      case constant_: {                                                       \
          type_ converted = ConvertScalar<type_>(number);                     \
          args.rval().setNumber((double) converted);                          \
          return true;                                                        \
      }

        JS_FOR_EACH_SCALAR_TYPE_REPR(SCALARTYPE_CALL)
#undef SCALARTYPE_CALL

    }
    return true;
}

/***************************************************************************
 * Reference type objects
 *
 * Reference type objects like `Any` or `Object` basically work the
 * same way that the scalar type objects do. There is one class with
 * many instances, and each instance has a reserved slot with a
 * TypeRepresentation object, which is used to distinguish which
 * reference type object this actually is.
 */

const Class js::ReferenceType::class_ = {
    "Reference",
    JSCLASS_HAS_RESERVED_SLOTS(JS_TYPEOBJ_REFERENCE_SLOTS),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,
    nullptr,
    ReferenceType::call,
    nullptr,
    nullptr,
    nullptr
};

const JSFunctionSpec js::ReferenceType::typeObjectMethods[] = {
    JS_FN("toSource", TypeObjectToSource, 0, 0),
    {"handle", {nullptr, nullptr}, 2, 0, "HandleCreate"},
    {"equivalent", {nullptr, nullptr}, 1, 0, "TypeObjectEquivalent"},
    JS_FS_END
};

bool
js::ReferenceType::call(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JS_ASSERT(IsReferenceTypeObject(args.callee()));
    ReferenceTypeRepresentation *typeRepr =
        typeRepresentation(args.callee())->asReference();

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_MORE_ARGS_NEEDED,
                             typeRepr->typeName(), "0", "s");
        return false;
    }

    switch (typeRepr->type()) {
      case ReferenceTypeRepresentation::TYPE_ANY:
        args.rval().set(args[0]);
        return true;

      case ReferenceTypeRepresentation::TYPE_OBJECT:
      {
        RootedObject obj(cx, ToObject(cx, args[0]));
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
      }

      case ReferenceTypeRepresentation::TYPE_STRING:
      {
        RootedString obj(cx, ToString<CanGC>(cx, args[0]));
        if (!obj)
            return false;
        args.rval().setString(&*obj);
        return true;
      }
    }

    MOZ_ASSUME_UNREACHABLE("Unhandled Reference type");
}

/***************************************************************************
 * ArrayType class
 */

/*
 * For code like:
 *
 *   var A = new TypedObject.ArrayType(uint8, 10);
 *   var S = new TypedObject.StructType({...});
 *
 * As usual, the [[Prototype]] of A is
 * TypedObject.ArrayType.prototype.  This permits adding methods to
 * all ArrayType types, by setting
 * TypedObject.ArrayType.prototype.methodName = function() { ... }.
 * The same holds for S with respect to TypedObject.StructType.
 *
 * We may also want to add methods to *instances* of an ArrayType:
 *
 *   var a = new A();
 *   var s = new S();
 *
 * As usual, the [[Prototype]] of a is A.prototype.  What's
 * A.prototype?  It's an empty object, and you can set
 * A.prototype.methodName = function() { ... } to add a method to all
 * A instances.  (And the same with respect to s and S.)
 *
 * But what if you want to add a method to all ArrayType instances,
 * not just all A instances?  (Or to all StructType instances.)  The
 * [[Prototype]] of the A.prototype empty object is
 * TypedObject.ArrayType.prototype.prototype (two .prototype levels!).
 * So just set TypedObject.ArrayType.prototype.prototype.methodName =
 * function() { ... } to add a method to all ArrayType instances.
 * (And, again, same with respect to s and S.)
 *
 * This function creates the A.prototype/S.prototype object.  It takes
 * as an argument either the TypedObject.ArrayType or the
 * TypedObject.StructType constructor function, then returns an empty
 * object with the .prototype.prototype object as its [[Prototype]].
 */
static JSObject *
CreateComplexTypeInstancePrototype(JSContext *cx,
                                   HandleObject typeObjectCtor)
{
    RootedValue ctorPrototypeVal(cx);
    if (!JSObject::getProperty(cx, typeObjectCtor, typeObjectCtor,
                               cx->names().prototype,
                               &ctorPrototypeVal))
    {
        return nullptr;
    }
    JS_ASSERT(ctorPrototypeVal.isObject()); // immutable binding
    RootedObject ctorPrototypeObj(cx, &ctorPrototypeVal.toObject());

    RootedValue ctorPrototypePrototypeVal(cx);
    if (!JSObject::getProperty(cx, ctorPrototypeObj, ctorPrototypeObj,
                               cx->names().prototype,
                               &ctorPrototypePrototypeVal))
    {
        return nullptr;
    }

    JS_ASSERT(ctorPrototypePrototypeVal.isObject()); // immutable binding
    RootedObject proto(cx, &ctorPrototypePrototypeVal.toObject());
    return NewObjectWithGivenProto(cx, &JSObject::class_, proto,
                                   &typeObjectCtor->global());
}

const Class ArrayType::class_ = {
    "ArrayType",
    JSCLASS_HAS_RESERVED_SLOTS(JS_TYPEOBJ_ARRAY_SLOTS),
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    TypedObject::construct,
    nullptr
};

const JSPropertySpec ArrayType::typeObjectProperties[] = {
    JS_PS_END
};

const JSFunctionSpec ArrayType::typeObjectMethods[] = {
    {"handle", {nullptr, nullptr}, 2, 0, "HandleCreate"},
    JS_FN("repeat", ArrayType::repeat, 1, 0),
    JS_FN("toSource", TypeObjectToSource, 0, 0),
    {"equivalent", {nullptr, nullptr}, 1, 0, "TypeObjectEquivalent"},
    JS_FS_END
};

const JSPropertySpec ArrayType::typedObjectProperties[] = {
    JS_PS_END
};

const JSFunctionSpec ArrayType::typedObjectMethods[] = {
    JS_FN("subarray", ArrayType::subarray, 2, 0),
    {"forEach", {nullptr, nullptr}, 1, 0, "ArrayForEach"},
    {"redimension", {nullptr, nullptr}, 1, 0, "TypedArrayRedimension"},
    JS_FS_END
};

static JSObject *
ArrayElementType(HandleObject array)
{
    JS_ASSERT(IsArrayTypeObject(*array));
    return &array->getReservedSlot(JS_TYPEOBJ_SLOT_ARRAY_ELEM_TYPE).toObject();
}

static bool
FillTypedArrayWithValue(JSContext *cx, HandleObject array, HandleValue val)
{
    JS_ASSERT(IsTypedDatumOfKind(*array, TypeRepresentation::Array));

    RootedFunction func(
        cx, SelfHostedFunction(cx, cx->names().FillTypedArrayWithValue));
    if (!func)
        return false;

    InvokeArgs args(cx);
    if (!args.init(2))
        return false;

    args.setCallee(ObjectValue(*func));
    args[0].setObject(*array);
    args[1].set(val);
    return Invoke(cx, args);
}

bool
ArrayType::repeat(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "repeat()", "0", "s");
        return false;
    }

    RootedObject thisObj(cx, ToObjectIfObject(args.thisv()));
    if (!thisObj || !IsArrayTypeObject(*thisObj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_INCOMPATIBLE_PROTO,
                             "ArrayType", "repeat",
                             InformalValueTypeName(args.thisv()));
        return false;
    }

    RootedObject binaryArray(cx, TypedObject::createZeroed(cx, thisObj));
    if (!binaryArray)
        return false;

    RootedValue val(cx, args[0]);
    if (!FillTypedArrayWithValue(cx, binaryArray, val))
        return false;

    args.rval().setObject(*binaryArray);
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
/* static */ bool
ArrayType::subarray(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "subarray()", "0", "s");
        return false;
    }

    if (!args[0].isInt32()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_TYPEDOBJECT_SUBARRAY_INTEGER_ARG, "1");
        return false;
    }

    RootedObject thisObj(cx, &args.thisv().toObject());
    if (!IsTypedDatumOfKind(*thisObj, TypeRepresentation::Array)) {
        ReportCannotConvertTo(cx, thisObj, "binary array");
        return false;
    }

    RootedObject type(cx, GetType(*thisObj));
    ArrayTypeRepresentation *typeRepr = typeRepresentation(*type)->asArray();
    size_t length = typeRepr->length();

    int32_t begin = args[0].toInt32();
    int32_t end = length;

    if (args.length() >= 2) {
        if (!args[1].isInt32()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                    JSMSG_TYPEDOBJECT_SUBARRAY_INTEGER_ARG, "2");
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
    RootedObject arrayTypeGlobal(cx, global->getArrayType(cx));

    RootedObject elementType(cx, ArrayElementType(type));
    RootedObject subArrayType(cx, ArrayType::create(cx, arrayTypeGlobal,
                                                    elementType, sublength));
    if (!subArrayType)
        return false;

    int32_t elementSize = typeRepr->element()->size();
    size_t offset = elementSize * begin;

    RootedObject subarray(
        cx, TypedDatum::createDerived(cx, subArrayType, thisObj, offset));
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
                             nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "fill()", "0", "s");
        return false;
    }

    RootedObject thisObj(cx, ToObjectIfObject(args.thisv()));
    if (!thisObj || !IsTypedDatumOfKind(*thisObj, TypeRepresentation::Array)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             nullptr, JSMSG_INCOMPATIBLE_PROTO,
                             "ArrayType", "fill",
                             InformalValueTypeName(args.thisv()));
        return false;
    }

    Value funArrayTypeVal = GetFunctionNativeReserved(&args.callee(), 0);
    JS_ASSERT(funArrayTypeVal.isObject());

    RootedObject type(cx, GetType(*thisObj));
    TypeRepresentation *typeRepr = typeRepresentation(*type);
    RootedObject funArrayType(cx, &funArrayTypeVal.toObject());
    TypeRepresentation *funArrayTypeRepr = typeRepresentation(*funArrayType);
    if (typeRepr != funArrayTypeRepr) {
        RootedValue thisObjValue(cx, ObjectValue(*thisObj));
        ReportCannotConvertTo(cx, thisObjValue, funArrayTypeRepr);
        return false;
    }

    args.rval().setUndefined();
    RootedValue val(cx, args[0]);
    return FillTypedArrayWithValue(cx, thisObj, val);
}

static bool
InitializeCommonTypeDescriptorProperties(JSContext *cx,
                                         HandleObject obj,
                                         HandleObject typeReprOwnerObj)
{
    TypeRepresentation *typeRepr =
        TypeRepresentation::fromOwnerObject(*typeReprOwnerObj);

    if (typeRepr->transparent()) {
        // byteLength
        RootedValue typeByteLength(cx, NumberValue(typeRepr->size()));
        if (!JSObject::defineProperty(cx, obj, cx->names().byteLength,
                                      typeByteLength,
                                      nullptr, nullptr,
                                      JSPROP_READONLY | JSPROP_PERMANENT))
        {
            return false;
        }

        // byteAlignment
        RootedValue typeByteAlignment(cx, NumberValue(typeRepr->alignment()));
        if (!JSObject::defineProperty(cx, obj, cx->names().byteAlignment,
                                      typeByteAlignment,
                                      nullptr, nullptr,
                                      JSPROP_READONLY | JSPROP_PERMANENT))
        {
            return false;
        }
    }

    // variable -- always false since we do not yet support variable-size types
    RootedValue variable(cx, JSVAL_FALSE);
    if (!JSObject::defineProperty(cx, obj, cx->names().variable,
                                  variable,
                                  nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
    {
        return false;
    }

    return true;
}

JSObject *
ArrayType::create(JSContext *cx,
                  HandleObject metaTypeObject,
                  HandleObject elementType,
                  size_t length)
{
    JS_ASSERT(elementType);
    JS_ASSERT(IsTypeObject(*elementType));

    TypeRepresentation *elementTypeRepr = typeRepresentation(*elementType);
    RootedObject typeReprObj(
        cx, ArrayTypeRepresentation::Create(cx, elementTypeRepr, length));
    if (!typeReprObj)
        return nullptr;

    RootedValue prototypeVal(cx);
    if (!JSObject::getProperty(cx, metaTypeObject, metaTypeObject,
                               cx->names().prototype,
                               &prototypeVal))
    {
        return nullptr;
    }
    JS_ASSERT(prototypeVal.isObject()); // immutable binding

    RootedObject obj(
        cx, NewObjectWithClassProto(cx, &ArrayType::class_,
                                    &prototypeVal.toObject(), cx->global()));
    if (!obj)
        return nullptr;
    obj->initReservedSlot(JS_TYPEOBJ_SLOT_TYPE_REPR, ObjectValue(*typeReprObj));

    RootedValue elementTypeVal(cx, ObjectValue(*elementType));
    if (!JSObject::defineProperty(cx, obj, cx->names().elementType,
                                  elementTypeVal, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return nullptr;

    obj->initReservedSlot(JS_TYPEOBJ_SLOT_ARRAY_ELEM_TYPE, elementTypeVal);

    RootedValue lengthVal(cx, Int32Value(length));
    if (!JSObject::defineProperty(cx, obj, cx->names().length,
                                  lengthVal, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return nullptr;

    if (!InitializeCommonTypeDescriptorProperties(cx, obj, typeReprObj))
        return nullptr;

    RootedObject prototypeObj(
        cx, CreateComplexTypeInstancePrototype(cx, metaTypeObject));
    if (!prototypeObj)
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, obj, prototypeObj))
        return nullptr;

    JSFunction *fillFun = DefineFunctionWithReserved(cx, prototypeObj, "fill", ArrayFillSubarray, 1, 0);
    if (!fillFun)
        return nullptr;

    // This is important
    // so that A.prototype.fill.call(b, val)
    // where b.type != A raises an error
    SetFunctionNativeReserved(fillFun, 0, ObjectValue(*obj));

    return obj;
}

bool
ArrayType::construct(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.isConstructing()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_NOT_FUNCTION, "ArrayType");
        return false;
    }

    if (args.length() != 2 ||
        !args[0].isObject() ||
        !args[1].isNumber() ||
        args[1].toNumber() < 0)
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_TYPEDOBJECT_ARRAYTYPE_BAD_ARGS);
        return false;
    }

    RootedObject arrayTypeGlobal(cx, &args.callee());
    RootedObject elementType(cx, &args[0].toObject());

    if (!IsTypeObject(*elementType)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_TYPEDOBJECT_ARRAYTYPE_BAD_ARGS);
        return false;
    }

    if (!args[1].isInt32()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_TYPEDOBJECT_ARRAYTYPE_BAD_ARGS);
        return false;
    }

    int32_t length = args[1].toInt32();
    if (length < 0) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_TYPEDOBJECT_ARRAYTYPE_BAD_ARGS);
        return false;
    }

    RootedObject obj(cx, create(cx, arrayTypeGlobal, elementType, length));
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

/*********************************
 * StructType class
 */

const Class StructType::class_ = {
    "StructType",
    JSCLASS_HAS_RESERVED_SLOTS(JS_TYPEOBJ_STRUCT_SLOTS) |
    JSCLASS_HAS_PRIVATE, // used to store FieldList
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr, /* finalize */
    nullptr, /* checkAccess */
    nullptr, /* call */
    nullptr, /* hasInstance */
    TypedObject::construct,
    nullptr  /* trace */
};

const JSPropertySpec StructType::typeObjectProperties[] = {
    JS_PS_END
};

const JSFunctionSpec StructType::typeObjectMethods[] = {
    {"handle", {nullptr, nullptr}, 2, 0, "HandleCreate"},
    JS_FN("toSource", TypeObjectToSource, 0, 0),
    {"equivalent", {nullptr, nullptr}, 1, 0, "TypeObjectEquivalent"},
    JS_FS_END
};

const JSPropertySpec StructType::typedObjectProperties[] = {
    JS_PS_END
};

const JSFunctionSpec StructType::typedObjectMethods[] = {
    JS_FS_END
};

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
        if (!fieldType || !IsTypeObject(*fieldType)) {
            ReportCannotConvertTo(cx, fieldTypeVal, "StructType field specifier");
            return false;
        }

        if (!fieldTypeObjs.append(fieldTypeVal)) {
            js_ReportOutOfMemory(cx);
            return false;
        }

        if (!fieldTypeReprObjs.append(typeRepresentationOwnerObj(*fieldType))) {
            js_ReportOutOfMemory(cx);
            return false;
        }
    }

    // Construct the `TypeRepresentation*`.
    RootedObject typeReprObj(
        cx, StructTypeRepresentation::Create(cx, ids, fieldTypeReprObjs));
    if (!typeReprObj)
        return false;
    StructTypeRepresentation *typeRepr =
        TypeRepresentation::fromOwnerObject(*typeReprObj)->asStruct();
    structType->initReservedSlot(JS_TYPEOBJ_SLOT_TYPE_REPR,
                                 ObjectValue(*typeReprObj));

    // Construct for internal use an array with the type object for each field.
    RootedObject fieldTypeVec(
        cx, NewDenseCopiedArray(cx, fieldTypeObjs.length(),
                                fieldTypeObjs.begin()));
    if (!fieldTypeVec)
        return false;

    structType->initReservedSlot(JS_TYPEOBJ_SLOT_STRUCT_FIELD_TYPES,
                                 ObjectValue(*fieldTypeVec));

    // Construct the fieldNames vector
    AutoValueVector fieldNameValues(cx);
    for (unsigned int i = 0; i < ids.length(); i++) {
        RootedValue value(cx, IdToValue(ids[i]));
        if (!fieldNameValues.append(value))
            return false;
    }
    RootedObject fieldNamesVec(
        cx, NewDenseCopiedArray(cx, fieldNameValues.length(),
                                fieldNameValues.begin()));
    if (!fieldNamesVec)
        return false;
    RootedValue fieldNamesVecValue(cx, ObjectValue(*fieldNamesVec));
    if (!JSObject::defineProperty(cx, structType, cx->names().fieldNames,
                                  fieldNamesVecValue, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return false;

    // Construct the fieldNames, fieldOffsets and fieldTypes objects:
    // fieldNames : [ string ]
    // fieldOffsets : { string: integer, ... }
    // fieldTypes : { string: Type, ... }
    RootedObject fieldOffsets(
        cx, NewObjectWithClassProto(cx, &JSObject::class_, nullptr, nullptr));
    RootedObject fieldTypes(
        cx, NewObjectWithClassProto(cx, &JSObject::class_, nullptr, nullptr));
    for (size_t i = 0; i < typeRepr->fieldCount(); i++) {
        const StructField &field = typeRepr->field(i);
        RootedId fieldId(cx, field.id);

        // fieldOffsets[id] = offset
        RootedValue offset(cx, NumberValue(field.offset));
        if (!JSObject::defineGeneric(cx, fieldOffsets, fieldId,
                                     offset, nullptr, nullptr,
                                     JSPROP_READONLY | JSPROP_PERMANENT))
            return false;

        // fieldTypes[id] = typeObj
        if (!JSObject::defineGeneric(cx, fieldTypes, fieldId,
                                     fieldTypeObjs.handleAt(i), nullptr, nullptr,
                                     JSPROP_READONLY | JSPROP_PERMANENT))
            return false;
    }

    RootedValue fieldOffsetsValue(cx, ObjectValue(*fieldOffsets));
    if (!JSObject::defineProperty(cx, structType, cx->names().fieldOffsets,
                                  fieldOffsetsValue, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return false;

    RootedValue fieldTypesValue(cx, ObjectValue(*fieldTypes));
    if (!JSObject::defineProperty(cx, structType, cx->names().fieldTypes,
                                  fieldTypesValue, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return false;

    return true;
}

JSObject *
StructType::create(JSContext *cx, HandleObject metaTypeObject,
                   HandleObject fields)
{
    RootedValue prototypeVal(cx);
    if (!JSObject::getProperty(cx, metaTypeObject, metaTypeObject,
                               cx->names().prototype,
                               &prototypeVal))
        return nullptr;
    JS_ASSERT(prototypeVal.isObject()); // immutable binding

    RootedObject obj(
        cx, NewObjectWithClassProto(cx, &StructType::class_,
                                    &prototypeVal.toObject(), cx->global()));
    if (!obj)
        return nullptr;

    if (!StructType::layout(cx, obj, fields))
        return nullptr;

    RootedObject fieldsProto(cx);
    if (!JSObject::getProto(cx, fields, &fieldsProto))
        return nullptr;

    RootedObject typeReprObj(cx, typeRepresentationOwnerObj(*obj));
    if (!InitializeCommonTypeDescriptorProperties(cx, obj, typeReprObj))
        return nullptr;

    RootedObject prototypeObj(
        cx, CreateComplexTypeInstancePrototype(cx, metaTypeObject));
    if (!prototypeObj)
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, obj, prototypeObj))
        return nullptr;

    return obj;
}

bool
StructType::construct(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.isConstructing()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_NOT_FUNCTION, "StructType");
        return false;
    }

    if (args.length() >= 1 && args[0].isObject()) {
        RootedObject metaTypeObject(cx, &args.callee());
        RootedObject fields(cx, &args[0].toObject());
        RootedObject obj(cx, create(cx, metaTypeObject, fields));
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                         JSMSG_TYPEDOBJECT_STRUCTTYPE_BAD_ARGS);
    return false;
}

/******************************************************************************
 * Creating the TypedObject "module"
 *
 * We create one global, `TypedObject`, which contains the following
 * members:
 *
 * 1. uint8, uint16, etc
 * 2. ArrayType
 * 3. StructType
 *
 * Each of these is a function and hence their prototype is
 * `Function.__proto__` (in terms of the JS Engine, they are not
 * JSFunctions but rather instances of their own respective JSClasses
 * which override the call and construct operations).
 *
 * Each type object also has its own `prototype` field. Therefore,
 * using `StructType` as an example, the basic setup is:
 *
 *   StructType --__proto__--> Function.__proto__
 *        |
 *    prototype -- prototype --> { }
 *        |
 *        v
 *       { } -----__proto__--> Function.__proto__
 *
 * When a new type object (e.g., an instance of StructType) is created,
 * it will look as follows:
 *
 *   MyStruct -__proto__-> StructType.prototype -__proto__-> Function.__proto__
 *        |                          |
 *        |                     prototype
 *        |                          |
 *        |                          v
 *    prototype -----__proto__----> { }
 *        |
 *        v
 *       { } --__proto__-> Object.prototype
 *
 * Finally, when an instance of `MyStruct` is created, its
 * structure is as follows:
 *
 *    object -__proto__->
 *      MyStruct.prototype -__proto__->
 *        StructType.prototype.prototype -__proto__->
 *          Objcet.prototype
 */

// Here `T` is either `ScalarType` or `ReferenceType`
template<typename T>
static bool
DefineSimpleTypeObject(JSContext *cx,
                       HandleObject global,
                       HandleObject module,
                       typename T::TypeRepr::Type type,
                       HandlePropertyName className)
{
    RootedObject funcProto(cx, JS_GetFunctionPrototype(cx, global));
    JS_ASSERT(funcProto);

    RootedObject numFun(cx, NewObjectWithClassProto(cx, &T::class_, funcProto, global));
    if (!numFun)
        return false;

    RootedObject typeReprObj(cx, T::TypeRepr::Create(cx, type));
    if (!typeReprObj)
        return false;

    numFun->initReservedSlot(JS_TYPEOBJ_SLOT_TYPE_REPR,
                             ObjectValue(*typeReprObj));

    if (!InitializeCommonTypeDescriptorProperties(cx, numFun, typeReprObj))
        return false;

    if (!JS_DefineFunctions(cx, numFun, T::typeObjectMethods))
        return false;

    RootedValue numFunValue(cx, ObjectValue(*numFun));
    if (!JSObject::defineProperty(cx, module, className,
                                  numFunValue, nullptr, nullptr, 0))
    {
        return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////
// X4

const Class X4Type::class_ = {
    "X4",
    JSCLASS_HAS_RESERVED_SLOTS(JS_TYPEOBJ_X4_SLOTS),
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,
    nullptr,
    call,
    nullptr,
    nullptr,
    nullptr
};

// These classes just exist to group together various properties and so on.
namespace js {
class Int32x4Defn {
  public:
    static const X4TypeRepresentation::Type type = X4TypeRepresentation::TYPE_INT32;
    static const JSPropertySpec TypedObjectProperties[];
    static const JSFunctionSpec TypedObjectMethods[];
};
class Float32x4Defn {
  public:
    static const X4TypeRepresentation::Type type = X4TypeRepresentation::TYPE_FLOAT32;
    static const JSPropertySpec TypedObjectProperties[];
    static const JSFunctionSpec TypedObjectMethods[];
};
} // namespace js

const JSPropertySpec js::Int32x4Defn::TypedObjectProperties[] = {
    JS_SELF_HOSTED_GET("x", "Int32x4Lane0", JSPROP_PERMANENT),
    JS_SELF_HOSTED_GET("y", "Int32x4Lane1", JSPROP_PERMANENT),
    JS_SELF_HOSTED_GET("z", "Int32x4Lane2", JSPROP_PERMANENT),
    JS_SELF_HOSTED_GET("w", "Int32x4Lane3", JSPROP_PERMANENT),
    JS_PS_END
};

const JSFunctionSpec js::Int32x4Defn::TypedObjectMethods[] = {
    JS_SELF_HOSTED_FN("toSource", "X4ToSource", 0, 0),
    JS_FS_END
};

const JSPropertySpec js::Float32x4Defn::TypedObjectProperties[] = {
    JS_SELF_HOSTED_GET("x", "Float32x4Lane0", JSPROP_PERMANENT),
    JS_SELF_HOSTED_GET("y", "Float32x4Lane1", JSPROP_PERMANENT),
    JS_SELF_HOSTED_GET("z", "Float32x4Lane2", JSPROP_PERMANENT),
    JS_SELF_HOSTED_GET("w", "Float32x4Lane3", JSPROP_PERMANENT),
    JS_PS_END
};

const JSFunctionSpec js::Float32x4Defn::TypedObjectMethods[] = {
    JS_SELF_HOSTED_FN("toSource", "X4ToSource", 0, 0),
    JS_FS_END
};

template<typename T>
static JSObject *
CreateX4Class(JSContext *cx, Handle<GlobalObject*> global)
{
    RootedObject funcProto(cx, global->getOrCreateFunctionPrototype(cx));
    if (!funcProto)
        return nullptr;

    // Create type representation

    RootedObject typeReprObj(cx);
    typeReprObj = X4TypeRepresentation::Create(cx, T::type);
    if (!typeReprObj)
        return nullptr;

    // Create prototype property, which inherits from Object.prototype

    RootedObject objProto(cx, global->getOrCreateObjectPrototype(cx));
    if (!objProto)
        return nullptr;
    RootedObject proto(cx);
    proto = NewObjectWithGivenProto(cx, &JSObject::class_, objProto, global, SingletonObject);
    if (!proto)
        return nullptr;

    // Create type constructor itself

    RootedObject x4(cx);
    x4 = NewObjectWithClassProto(cx, &X4Type::class_, funcProto, global);
    if (!x4 ||
        !InitializeCommonTypeDescriptorProperties(cx, x4, typeReprObj) ||
        !DefinePropertiesAndBrand(cx, proto, nullptr, nullptr))
    {
        return nullptr;
    }

    // Link type constructor to the type representation

    x4->initReservedSlot(JS_TYPEOBJ_SLOT_TYPE_REPR, ObjectValue(*typeReprObj));

    // Link constructor to prototype and install properties

    if (!LinkConstructorAndPrototype(cx, x4, proto) ||
        !DefinePropertiesAndBrand(cx, proto, T::TypedObjectProperties,
                                  T::TypedObjectMethods))
    {
        return nullptr;
    }

    return x4;
}

bool
X4Type::call(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    const uint32_t LANES = 4;

    if (args.length() < LANES) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             args.callee().getClass()->name, "3", "s");
        return false;
    }

    double values[LANES];
    for (uint32_t i = 0; i < LANES; i++) {
        if (!ToNumber(cx, args[i], &values[i]))
            return false;
    }

    RootedObject typeObj(cx, &args.callee());
    RootedObject result(cx, TypedObject::createZeroed(cx, typeObj));
    if (!result)
        return false;

    X4TypeRepresentation *typeRepr = typeRepresentation(*typeObj)->asX4();
    switch (typeRepr->type()) {
#define STORE_LANES(_constant, _type, _name)                                  \
      case _constant:                                                         \
      {                                                                       \
        _type *mem = (_type*) TypedMem(*result);                              \
        for (uint32_t i = 0; i < LANES; i++) {                                \
            mem[i] = values[i];                                               \
        }                                                                     \
        break;                                                                \
      }
      JS_FOR_EACH_X4_TYPE_REPR(STORE_LANES)
#undef STORE_LANES
    }
    args.rval().setObject(*result);
    return true;
}

///////////////////////////////////////////////////////////////////////////

template<typename T>
static JSObject *
DefineMetaTypeObject(JSContext *cx,
                     Handle<GlobalObject*> global)
{
    RootedAtom className(cx, Atomize(cx, T::class_.name,
                                     strlen(T::class_.name)));
    if (!className)
        return nullptr;

    RootedObject funcProto(cx, global->getOrCreateFunctionPrototype(cx));
    if (!funcProto)
        return nullptr;

    // Create ctor.prototype, which inherits from Function.__proto__

    RootedObject proto(
        cx, NewObjectWithGivenProto(cx, &JSObject::class_, funcProto,
                                    global, SingletonObject));
    if (!proto)
        return nullptr;

    // Create ctor.prototype.prototype, which inherits from Object.__proto__

    RootedObject objProto(cx, global->getOrCreateObjectPrototype(cx));
    if (!objProto)
        return nullptr;
    RootedObject protoProto(
        cx, NewObjectWithGivenProto(cx, &JSObject::class_, objProto,
                                    global, SingletonObject));
    if (!proto)
        return nullptr;

    RootedValue protoProtoValue(cx, ObjectValue(*protoProto));
    if (!JSObject::defineProperty(cx, proto, cx->names().prototype,
                                  protoProtoValue,
                                  nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return nullptr;

    // Create ctor itself

    const int constructorLength = 2;
    RootedFunction ctor(cx,
                        global->createConstructor(cx, T::construct,
                                                  className, constructorLength));
    if (!ctor ||
        !LinkConstructorAndPrototype(cx, ctor, proto) ||
        !DefinePropertiesAndBrand(cx, proto,
                                  T::typeObjectProperties,
                                  T::typeObjectMethods) ||
        !DefinePropertiesAndBrand(cx, protoProto,
                                  T::typedObjectProperties,
                                  T::typedObjectMethods))
    {
        return nullptr;
    }

    return ctor;
}

JSObject *
js_InitTypedObjectClass(JSContext *cx, HandleObject obj)
{
    /*
     * The initialization strategy for TypedObjects is mildly unusual
     * compared to other classes. Because all of the types are members
     * of a single global, `TypedObject`, we basically make the
     * initialized for the `TypedObject` class populate the
     * `TypedObject` global (which is referred to as "module" herein).
     */

    JS_ASSERT(obj->is<GlobalObject>());
    Rooted<GlobalObject *> global(cx, &obj->as<GlobalObject>());

    RootedObject objProto(cx, global->getOrCreateObjectPrototype(cx));
    if (!objProto)
        return nullptr;

    RootedObject module(cx, NewObjectWithClassProto(cx, &JSObject::class_,
                                                    objProto, global));
    if (!module)
        return nullptr;

    if (!JS_DefineFunctions(cx, module, TypedObjectMethods))
        return nullptr;

    // Define TypedObject global.

    RootedValue moduleValue(cx, ObjectValue(*module));

    // uint8, uint16, any, etc

#define BINARYDATA_SCALAR_DEFINE(constant_, type_, name_)                       \
    if (!DefineSimpleTypeObject<ScalarType>(cx, global, module, constant_,      \
                                            cx->names().name_))                 \
        return nullptr;
    JS_FOR_EACH_SCALAR_TYPE_REPR(BINARYDATA_SCALAR_DEFINE)
#undef BINARYDATA_SCALAR_DEFINE

#define BINARYDATA_REFERENCE_DEFINE(constant_, type_, name_)                    \
    if (!DefineSimpleTypeObject<ReferenceType>(cx, global, module, constant_,   \
                                               cx->names().name_))              \
        return nullptr;
    JS_FOR_EACH_REFERENCE_TYPE_REPR(BINARYDATA_REFERENCE_DEFINE)
#undef BINARYDATA_REFERENCE_DEFINE

    // float32x4

    RootedObject float32x4Object(cx);
    float32x4Object = CreateX4Class<Float32x4Defn>(cx, global);
    if (!float32x4Object)
        return nullptr;

    RootedValue float32x4Value(cx, ObjectValue(*float32x4Object));
    if (!JSObject::defineProperty(cx, module, cx->names().float32x4,
                                  float32x4Value,
                                  nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
    {
        return nullptr;
    }

    // int32x4

    RootedObject int32x4Object(cx);
    int32x4Object = CreateX4Class<Int32x4Defn>(cx, global);
    if (!int32x4Object)
        return nullptr;

    RootedValue int32x4Value(cx, ObjectValue(*int32x4Object));
    if (!JSObject::defineProperty(cx, module, cx->names().int32x4,
                                  int32x4Value,
                                  nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
    {
        return nullptr;
    }

    // ArrayType.

    RootedObject arrayType(cx, DefineMetaTypeObject<ArrayType>(cx, global));
    if (!arrayType)
        return nullptr;

    RootedValue arrayTypeValue(cx, ObjectValue(*arrayType));
    if (!JSObject::defineProperty(cx, module, cx->names().ArrayType,
                                  arrayTypeValue,
                                  nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return nullptr;

    // StructType.

    RootedObject structType(cx, DefineMetaTypeObject<StructType>(cx, global));
    if (!structType)
        return nullptr;

    RootedValue structTypeValue(cx, ObjectValue(*structType));
    if (!JSObject::defineProperty(cx, module, cx->names().StructType,
                                  structTypeValue,
                                  nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return nullptr;

    // Everything is setup, install module on the global object:
    if (!JSObject::defineProperty(cx, global, cx->names().TypedObject,
                                  moduleValue,
                                  nullptr, nullptr,
                                  0))
        return nullptr;
    global->setConstructor(JSProto_TypedObject, moduleValue);
    global->setArrayType(arrayType);

    //  Handle

    RootedObject handle(cx, NewBuiltinClassInstance(cx, &JSObject::class_));
    if (!module)
        return nullptr;

    if (!JS_DefineFunctions(cx, handle, TypedHandle::handleStaticMethods))
        return nullptr;

    RootedValue handleValue(cx, ObjectValue(*handle));
    if (!JSObject::defineProperty(cx, module, cx->names().Handle,
                                  handleValue,
                                  nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
    {
        return nullptr;
    }

    return module;
}

JSObject *
js_InitTypedObjectDummy(JSContext *cx, HandleObject obj)
{
    /*
     * This function is entered into the jsprototypes.h table
     * as the initializer for `TypedObject`. It should not
     * be executed via the `standard_class_atoms` mechanism.
     */

    MOZ_ASSUME_UNREACHABLE("shouldn't be initializing TypedObject via the JSProtoKey initializer mechanism");
}

/******************************************************************************
 * Typed datums
 *
 * Datums represent either typed objects or handles. See comment in
 * TypedObject.h.
 */

template<class T>
/*static*/ T *
TypedDatum::createUnattached(JSContext *cx,
                             HandleObject type)
{
    JS_STATIC_ASSERT(T::IsTypedDatumClass);
    JS_ASSERT(IsTypeObject(*type));

    RootedObject obj(cx, createUnattachedWithClass(cx, &T::class_, type));
    if (!obj)
        return nullptr;

    return &obj->as<T>();
}

/*static*/ TypedDatum *
TypedDatum::createUnattachedWithClass(JSContext *cx,
                                      const Class *clasp,
                                      HandleObject type)
{
    JS_ASSERT(IsTypeObject(*type));
    JS_ASSERT(clasp == &TypedObject::class_ || clasp == &TypedHandle::class_);
    JS_ASSERT(JSCLASS_RESERVED_SLOTS(clasp) == JS_DATUM_SLOTS);
    JS_ASSERT(clasp->hasPrivate());

    RootedObject proto(cx);
    if (IsSimpleTypeObject(*type)) {
        // FIXME Bug 929651 -- What prototype to use?
        proto = type->global().getOrCreateObjectPrototype(cx);
    } else {
        RootedValue protoVal(cx);
        if (!JSObject::getProperty(cx, type, type,
                                   cx->names().prototype, &protoVal))
        {
            return nullptr;
        }
        proto = &protoVal.toObject();
    }

    RootedObject obj(
        cx, NewObjectWithClassProto(cx, clasp, &*proto, nullptr));
    if (!obj)
        return nullptr;

    obj->setPrivate(nullptr);
    obj->initReservedSlot(JS_DATUM_SLOT_TYPE_OBJ, ObjectValue(*type));
    obj->initReservedSlot(JS_DATUM_SLOT_OWNER, NullValue());

    // Tag the type object for this instance with the type
    // representation, if that has not been done already.
    if (cx->typeInferenceEnabled() && !IsSimpleTypeObject(*type)) {
        // FIXME Bug 929651           ^~~~~~~~~~~~~~~~~~~~~~~~~~~
        RootedTypeObject typeObj(cx, obj->getType(cx));
        if (typeObj) {
            TypeRepresentation *typeRepr = typeRepresentation(*type);
            if (!typeObj->addTypedObjectAddendum(cx, typeRepr))
                return nullptr;
        }
    }

    return static_cast<TypedDatum*>(&*obj);
}

/*static*/ void
TypedDatum::attach(uint8_t *memory)
{
    setPrivate(memory);
    setReservedSlot(JS_DATUM_SLOT_OWNER, ObjectValue(*this));
}

/*static*/ void
TypedDatum::attach(JSObject &datum, uint32_t offset)
{
    JS_ASSERT(IsTypedDatum(datum));
    JS_ASSERT(datum.getReservedSlot(JS_DATUM_SLOT_OWNER).isObject());

    // find the location in memory
    uint8_t *mem = TypedMem(datum) + offset;

    // find the owner, which is often but not always `datum`
    JSObject &owner = datum.getReservedSlot(JS_DATUM_SLOT_OWNER).toObject();

    setPrivate(mem);
    setReservedSlot(JS_DATUM_SLOT_OWNER, ObjectValue(owner));
}

/*static*/ TypedDatum *
TypedDatum::createDerived(JSContext *cx, HandleObject type,
                          HandleObject datum, size_t offset)
{
    JS_ASSERT(IsTypedDatum(*datum));
    JS_ASSERT(offset <= DatumSize(*datum));
    JS_ASSERT(offset + TypeSize(*type) <= DatumSize(*datum));

    const js::Class *clasp = datum->getClass();
    Rooted<TypedDatum*> obj(cx, createUnattachedWithClass(cx, clasp, type));
    if (!obj)
        return nullptr;

    obj->attach(*datum, offset);
    return obj;
}

static bool
ReportDatumTypeError(JSContext *cx,
                     const unsigned errorNumber,
                     HandleObject obj)
{
    JS_ASSERT(IsTypedDatum(*obj));

    RootedObject type(cx, GetType(*obj));
    TypeRepresentation *typeRepr = typeRepresentation(*type);

    StringBuffer contents(cx);
    if (!typeRepr->appendString(cx, contents))
        return false;

    RootedString result(cx, contents.finishString());
    if (!result)
        return false;

    char *typeReprStr = JS_EncodeString(cx, result.get());
    if (!typeReprStr)
        return false;

    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                         errorNumber, typeReprStr);

    JS_free(cx, (void *) typeReprStr);
    return false;
}

/*static*/ void
TypedDatum::obj_trace(JSTracer *trace, JSObject *object)
{
    JS_ASSERT(IsTypedDatum(*object));

    for (size_t i = 0; i < JS_DATUM_SLOTS; i++)
        gc::MarkSlot(trace, &object->getReservedSlotRef(i), "TypedObjectSlot");

    uint8_t *mem = TypedMem(*object);
    TypeRepresentation *repr = typeRepresentation(*GetType(*object));
    if (repr->opaque())
        repr->traceInstance(trace, mem);
}

/*static*/ void
TypedDatum::obj_finalize(js::FreeOp *op, JSObject *obj)
{
    // if the obj owns the memory, indicating by the owner slot being
    // set to itself, then we must free it when finalized.

    JS_ASSERT(obj->getReservedSlotRef(JS_DATUM_SLOT_OWNER).isObjectOrNull());

    if (obj->getReservedSlot(JS_DATUM_SLOT_OWNER).isNull())
        return; // Unattached

    if (&obj->getReservedSlot(JS_DATUM_SLOT_OWNER).toObject() == obj) {
        JS_ASSERT(obj->getPrivate());
        op->free_(obj->getPrivate());
    }
}

bool
TypedDatum::obj_lookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                              MutableHandleObject objp, MutableHandleShape propp)
{
    JS_ASSERT(IsTypedDatum(*obj));

    RootedObject type(cx, GetType(*obj));
    TypeRepresentation *typeRepr = typeRepresentation(*type);

    switch (typeRepr->kind()) {
      case TypeRepresentation::Scalar:
      case TypeRepresentation::Reference:
      case TypeRepresentation::X4:
        break;

      case TypeRepresentation::Array:
      {
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

      case TypeRepresentation::Struct:
      {
        RootedObject type(cx, GetType(*obj));
        JS_ASSERT(IsStructTypeObject(*type));
        StructTypeRepresentation *typeRepr =
            typeRepresentation(*type)->asStruct();
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
        objp.set(nullptr);
        propp.set(nullptr);
        return true;
    }

    return JSObject::lookupGeneric(cx, proto, id, objp, propp);
}

bool
TypedDatum::obj_lookupProperty(JSContext *cx,
                                HandleObject obj,
                                HandlePropertyName name,
                                MutableHandleObject objp,
                                MutableHandleShape propp)
{
    RootedId id(cx, NameToId(name));
    return obj_lookupGeneric(cx, obj, id, objp, propp);
}

bool
TypedDatum::obj_lookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                                MutableHandleObject objp, MutableHandleShape propp)
{
    JS_ASSERT(IsTypedDatum(*obj));
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

    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                         errorNumber, propName);

    JS_free(cx, propName);
    return false;
}

bool
TypedDatum::obj_lookupSpecial(JSContext *cx, HandleObject obj,
                              HandleSpecialId sid, MutableHandleObject objp,
                              MutableHandleShape propp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return obj_lookupGeneric(cx, obj, id, objp, propp);
}

bool
TypedDatum::obj_defineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue v,
                              PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    return ReportPropertyError(cx, JSMSG_UNDEFINED_PROP, id);
}

bool
TypedDatum::obj_defineProperty(JSContext *cx, HandleObject obj,
                               HandlePropertyName name, HandleValue v,
                               PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, NameToId(name));
    return obj_defineGeneric(cx, obj, id, v, getter, setter, attrs);
}

bool
TypedDatum::obj_defineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue v,
                               PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);

    RootedObject delegate(cx, ArrayBufferDelegate(cx, obj));
    if (!delegate)
        return false;
    return baseops::DefineElement(cx, delegate, index, v, getter, setter, attrs);
}

bool
TypedDatum::obj_defineSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, HandleValue v,
                              PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return obj_defineGeneric(cx, obj, id, v, getter, setter, attrs);
}

static JSObject *
StructFieldType(JSContext *cx,
                HandleObject type,
                int32_t fieldIndex)
{
    JS_ASSERT(IsStructTypeObject(*type));

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
        cx, &type->getReservedSlot(JS_TYPEOBJ_SLOT_STRUCT_FIELD_TYPES).toObject());
    RootedValue fieldTypeVal(cx);
    if (!JSObject::getElement(cx, fieldTypes, fieldTypes,
                              fieldIndex, &fieldTypeVal))
        return nullptr;

    return &fieldTypeVal.toObject();
}

bool
TypedDatum::obj_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver,
                           HandleId id, MutableHandleValue vp)
{
    JS_ASSERT(IsTypedDatum(*obj));

    // Dispatch elements to obj_getElement:
    uint32_t index;
    if (js_IdIsIndex(id, &index))
        return obj_getElement(cx, obj, receiver, index, vp);

    // Handle everything else here:

    RootedObject type(cx, GetType(*obj));
    TypeRepresentation *typeRepr = typeRepresentation(*type);
    switch (typeRepr->kind()) {
      case TypeRepresentation::Scalar:
      case TypeRepresentation::Reference:
        break;

      case TypeRepresentation::X4:
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

        RootedObject fieldType(cx, StructFieldType(cx, type, field->index));
        if (!fieldType)
            return false;

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
TypedDatum::obj_getProperty(JSContext *cx, HandleObject obj, HandleObject receiver,
                              HandlePropertyName name, MutableHandleValue vp)
{
    RootedId id(cx, NameToId(name));
    return obj_getGeneric(cx, obj, receiver, id, vp);
}

bool
TypedDatum::obj_getElement(JSContext *cx, HandleObject obj, HandleObject receiver,
                             uint32_t index, MutableHandleValue vp)
{
    bool present;
    return obj_getElementIfPresent(cx, obj, receiver, index, vp, &present);
}

bool
TypedDatum::obj_getElementIfPresent(JSContext *cx, HandleObject obj,
                                    HandleObject receiver, uint32_t index,
                                    MutableHandleValue vp, bool *present)
{
    RootedObject type(cx, GetType(*obj));
    TypeRepresentation *typeRepr = typeRepresentation(*type);

    switch (typeRepr->kind()) {
      case TypeRepresentation::Scalar:
      case TypeRepresentation::Reference:
      case TypeRepresentation::X4:
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
TypedDatum::obj_getSpecial(JSContext *cx, HandleObject obj,
                            HandleObject receiver, HandleSpecialId sid,
                            MutableHandleValue vp)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return obj_getGeneric(cx, obj, receiver, id, vp);
}

bool
TypedDatum::obj_setGeneric(JSContext *cx, HandleObject obj, HandleId id,
                            MutableHandleValue vp, bool strict)
{
    uint32_t index;
    if (js_IdIsIndex(id, &index))
        return obj_setElement(cx, obj, index, vp, strict);

    RootedObject type(cx, GetType(*obj));
    TypeRepresentation *typeRepr = typeRepresentation(*type);

    switch (typeRepr->kind()) {
      case ScalarTypeRepresentation::Scalar:
      case TypeRepresentation::Reference:
        break;

      case ScalarTypeRepresentation::X4:
        break;

      case ScalarTypeRepresentation::Array:
        if (JSID_IS_ATOM(id, cx->names().length)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                 nullptr, JSMSG_CANT_REDEFINE_ARRAY_LENGTH);
            return false;
        }
        break;

      case ScalarTypeRepresentation::Struct: {
        const StructField *field = typeRepr->asStruct()->fieldNamed(id);
        if (!field)
            break;

        RootedObject fieldType(cx, StructFieldType(cx, type, field->index));
        if (!fieldType)
            return false;

        return ConvertAndCopyTo(cx, fieldType, obj, field->offset, vp);
      }
    }

    return ReportDatumTypeError(cx, JSMSG_OBJECT_NOT_EXTENSIBLE, obj);
}

bool
TypedDatum::obj_setProperty(JSContext *cx, HandleObject obj,
                             HandlePropertyName name, MutableHandleValue vp,
                             bool strict)
{
    RootedId id(cx, NameToId(name));
    return obj_setGeneric(cx, obj, id, vp, strict);
}

bool
TypedDatum::obj_setElement(JSContext *cx, HandleObject obj, uint32_t index,
                            MutableHandleValue vp, bool strict)
{
    RootedObject type(cx, GetType(*obj));
    TypeRepresentation *typeRepr = typeRepresentation(*type);

    switch (typeRepr->kind()) {
      case TypeRepresentation::Scalar:
      case TypeRepresentation::Reference:
      case TypeRepresentation::X4:
      case TypeRepresentation::Struct:
        break;

      case TypeRepresentation::Array: {
        ArrayTypeRepresentation *arrayTypeRepr = typeRepr->asArray();

        if (index >= arrayTypeRepr->length()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                 nullptr, JSMSG_TYPEDOBJECT_BINARYARRAY_BAD_INDEX);
            return false;
        }

        RootedObject elementType(cx, ArrayElementType(type));
        size_t offset = arrayTypeRepr->element()->size() * index;
        return ConvertAndCopyTo(cx, elementType, obj, offset, vp);
      }
    }

    return ReportDatumTypeError(cx, JSMSG_OBJECT_NOT_EXTENSIBLE, obj);
}

bool
TypedDatum::obj_setSpecial(JSContext *cx, HandleObject obj,
                             HandleSpecialId sid, MutableHandleValue vp,
                             bool strict)
{
    RootedId id(cx, SPECIALID_TO_JSID(sid));
    return obj_setGeneric(cx, obj, id, vp, strict);
}

bool
TypedDatum::obj_getGenericAttributes(JSContext *cx, HandleObject obj,
                                     HandleId id, unsigned *attrsp)
{
    uint32_t index;
    RootedObject type(cx, GetType(*obj));
    TypeRepresentation *typeRepr = typeRepresentation(*type);

    switch (typeRepr->kind()) {
      case TypeRepresentation::Scalar:
      case TypeRepresentation::Reference:
        break;

      case TypeRepresentation::X4:
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
        if (typeRepr->asStruct()->fieldNamed(id) != nullptr) {
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

static bool
IsOwnId(JSContext *cx, HandleObject obj, HandleId id)
{
    uint32_t index;
    RootedObject type(cx, GetType(*obj));
    TypeRepresentation *typeRepr = typeRepresentation(*type);

    switch (typeRepr->kind()) {
      case TypeRepresentation::Scalar:
      case TypeRepresentation::Reference:
      case TypeRepresentation::X4:
        return false;

      case TypeRepresentation::Array:
        return js_IdIsIndex(id, &index) || JSID_IS_ATOM(id, cx->names().length);

      case TypeRepresentation::Struct:
        return typeRepr->asStruct()->fieldNamed(id) != nullptr;
    }

    return false;
}

bool
TypedDatum::obj_setGenericAttributes(JSContext *cx, HandleObject obj,
                                       HandleId id, unsigned *attrsp)
{
    RootedObject type(cx, GetType(*obj));

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
TypedDatum::obj_deleteProperty(JSContext *cx, HandleObject obj,
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
TypedDatum::obj_deleteElement(JSContext *cx, HandleObject obj, uint32_t index,
                               bool *succeeded)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
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
TypedDatum::obj_deleteSpecial(JSContext *cx, HandleObject obj,
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
TypedDatum::obj_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
                           MutableHandleValue statep, MutableHandleId idp)
{
    uint32_t index;

    RootedObject type(cx, GetType(*obj));
    TypeRepresentation *typeRepr = typeRepresentation(*type);

    switch (typeRepr->kind()) {
      case TypeRepresentation::Scalar:
      case TypeRepresentation::Reference:
      case TypeRepresentation::X4:
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
            idp.set(INT_TO_JSID(typeRepr->asArray()->length()));
            break;

          case JSENUMERATE_NEXT:
            index = static_cast<int32_t>(statep.toInt32());

            if (index < typeRepr->asArray()->length()) {
                idp.set(INT_TO_JSID(index));
                statep.setInt32(index + 1);
            } else {
                JS_ASSERT(index == typeRepr->asArray()->length());
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

/* static */ size_t
TypedDatum::dataOffset()
{
    return JSObject::getPrivateDataOffset(JS_DATUM_SLOTS + 1);
}

/******************************************************************************
 * Typed Objects
 */

const Class TypedObject::class_ = {
    "TypedObject",
    Class::NON_NATIVE |
    JSCLASS_HAS_RESERVED_SLOTS(JS_DATUM_SLOTS) |
    JSCLASS_HAS_PRIVATE |
    JSCLASS_IMPLEMENTS_BARRIERS,
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    TypedDatum::obj_finalize,
    nullptr,        /* checkAccess */
    nullptr,        /* call        */
    nullptr,        /* construct   */
    nullptr,        /* hasInstance */
    TypedDatum::obj_trace,
    JS_NULL_CLASS_EXT,
    {
        TypedDatum::obj_lookupGeneric,
        TypedDatum::obj_lookupProperty,
        TypedDatum::obj_lookupElement,
        TypedDatum::obj_lookupSpecial,
        TypedDatum::obj_defineGeneric,
        TypedDatum::obj_defineProperty,
        TypedDatum::obj_defineElement,
        TypedDatum::obj_defineSpecial,
        TypedDatum::obj_getGeneric,
        TypedDatum::obj_getProperty,
        TypedDatum::obj_getElement,
        TypedDatum::obj_getElementIfPresent,
        TypedDatum::obj_getSpecial,
        TypedDatum::obj_setGeneric,
        TypedDatum::obj_setProperty,
        TypedDatum::obj_setElement,
        TypedDatum::obj_setSpecial,
        TypedDatum::obj_getGenericAttributes,
        TypedDatum::obj_setGenericAttributes,
        TypedDatum::obj_deleteProperty,
        TypedDatum::obj_deleteElement,
        TypedDatum::obj_deleteSpecial,
        nullptr, nullptr, // watch/unwatch
        TypedDatum::obj_enumerate,
        nullptr, /* thisObject */
    }
};

/*static*/ JSObject *
TypedObject::createZeroed(JSContext *cx, HandleObject type)
{
    Rooted<TypedObject*> obj(cx, createUnattached<TypedObject>(cx, type));
    if (!obj)
        return nullptr;

    TypeRepresentation *typeRepr = typeRepresentation(*type);
    size_t memsize = typeRepr->size();
    uint8_t *memory = (uint8_t*) cx->malloc_(memsize);
    if (!memory)
        return nullptr;
    typeRepr->initInstance(cx->runtime(), memory);
    obj->attach(memory);
    return obj;
}

/*static*/ bool
TypedObject::construct(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject callee(cx, &args.callee());
    JS_ASSERT(IsTypeObject(*callee));

    RootedObject obj(cx, createZeroed(cx, callee));
    if (!obj)
        return false;

    if (argc == 1) {
        RootedValue initial(cx, args[0]);
        if (!ConvertAndCopyTo(cx, obj, initial))
            return false;
    }

    args.rval().setObject(*obj);
    return true;
}

/******************************************************************************
 * Handles
 */

const Class TypedHandle::class_ = {
    "Handle",
    Class::NON_NATIVE |
    JSCLASS_HAS_RESERVED_SLOTS(JS_DATUM_SLOTS) |
    JSCLASS_HAS_PRIVATE |
    JSCLASS_IMPLEMENTS_BARRIERS,
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    TypedDatum::obj_finalize,
    nullptr,        /* checkAccess */
    nullptr,        /* call        */
    nullptr,        /* construct   */
    nullptr,        /* hasInstance */
    TypedDatum::obj_trace,
    JS_NULL_CLASS_EXT,
    {
        TypedDatum::obj_lookupGeneric,
        TypedDatum::obj_lookupProperty,
        TypedDatum::obj_lookupElement,
        TypedDatum::obj_lookupSpecial,
        TypedDatum::obj_defineGeneric,
        TypedDatum::obj_defineProperty,
        TypedDatum::obj_defineElement,
        TypedDatum::obj_defineSpecial,
        TypedDatum::obj_getGeneric,
        TypedDatum::obj_getProperty,
        TypedDatum::obj_getElement,
        TypedDatum::obj_getElementIfPresent,
        TypedDatum::obj_getSpecial,
        TypedDatum::obj_setGeneric,
        TypedDatum::obj_setProperty,
        TypedDatum::obj_setElement,
        TypedDatum::obj_setSpecial,
        TypedDatum::obj_getGenericAttributes,
        TypedDatum::obj_setGenericAttributes,
        TypedDatum::obj_deleteProperty,
        TypedDatum::obj_deleteElement,
        TypedDatum::obj_deleteSpecial,
        nullptr, nullptr, // watch/unwatch
        TypedDatum::obj_enumerate,
        nullptr, /* thisObject */
    }
};

const JSFunctionSpec TypedHandle::handleStaticMethods[] = {
    {"move", {nullptr, nullptr}, 3, 0, "HandleMove"},
    {"get", {nullptr, nullptr}, 1, 0, "HandleGet"},
    {"set", {nullptr, nullptr}, 2, 0, "HandleSet"},
    {"isHandle", {nullptr, nullptr}, 1, 0, "HandleTest"},
    JS_FS_END
};

/******************************************************************************
 * Intrinsics
 */

bool
js::NewTypedHandle(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(argc == 1);
    JS_ASSERT(args[0].isObject() && IsTypeObject(args[0].toObject()));

    RootedObject typeObj(cx, &args[0].toObject());
    Rooted<TypedHandle*> obj(
        cx, TypedDatum::createUnattached<TypedHandle>(cx, typeObj));
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

bool
js::NewDerivedTypedDatum(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(argc == 3);
    JS_ASSERT(args[0].isObject() && IsTypeObject(args[0].toObject()));
    JS_ASSERT(args[1].isObject() && IsTypedDatum(args[1].toObject()));
    JS_ASSERT(args[2].isInt32());

    RootedObject typeObj(cx, &args[0].toObject());
    RootedObject datum(cx, &args[1].toObject());
    int32_t offset = args[2].toInt32();

    Rooted<TypedDatum*> obj(
        cx, TypedDatum::createDerived(cx, typeObj, datum, offset));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

bool
js::AttachHandle(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(argc == 3);
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedHandle>());
    JS_ASSERT(args[1].isObject() && IsTypedDatum(args[1].toObject()));
    JS_ASSERT(args[2].isInt32());

    TypedHandle &handle = args[0].toObject().as<TypedHandle>();
    handle.attach(args[1].toObject(), args[2].toInt32());
    return true;
}

const JSJitInfo js::AttachHandleJitInfo =
    JS_JITINFO_NATIVE_PARALLEL(
        JSParallelNativeThreadSafeWrapper<js::AttachHandle>);

bool
js::ObjectIsTypeObject(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(argc == 1);
    JS_ASSERT(args[0].isObject());
    args.rval().setBoolean(IsTypeObject(args[0].toObject()));
    return true;
}

const JSJitInfo js::ObjectIsTypeObjectJitInfo =
    JS_JITINFO_NATIVE_PARALLEL(
        JSParallelNativeThreadSafeWrapper<js::ObjectIsTypeObject>);

bool
js::ObjectIsTypeRepresentation(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(argc == 1);
    JS_ASSERT(args[0].isObject());
    args.rval().setBoolean(TypeRepresentation::isOwnerObject(args[0].toObject()));
    return true;
}

const JSJitInfo js::ObjectIsTypeRepresentationJitInfo =
    JS_JITINFO_NATIVE_PARALLEL(
        JSParallelNativeThreadSafeWrapper<js::ObjectIsTypeRepresentation>);

bool
js::ObjectIsTypedHandle(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(argc == 1);
    JS_ASSERT(args[0].isObject());
    args.rval().setBoolean(args[0].toObject().is<TypedHandle>());
    return true;
}

const JSJitInfo js::ObjectIsTypedHandleJitInfo =
    JS_JITINFO_NATIVE_PARALLEL(
        JSParallelNativeThreadSafeWrapper<js::ObjectIsTypedHandle>);

bool
js::ObjectIsTypedObject(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(argc == 1);
    JS_ASSERT(args[0].isObject());
    args.rval().setBoolean(args[0].toObject().is<TypedObject>());
    return true;
}

const JSJitInfo js::ObjectIsTypedObjectJitInfo =
    JS_JITINFO_NATIVE_PARALLEL(
        JSParallelNativeThreadSafeWrapper<js::ObjectIsTypedObject>);

bool
js::IsAttached(ThreadSafeContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args[0].isObject() && IsTypedDatum(args[0].toObject()));
    args.rval().setBoolean(TypedMem(args[0].toObject()) != nullptr);
    return true;
}

const JSJitInfo js::IsAttachedJitInfo =
    JS_JITINFO_NATIVE_PARALLEL(
        JSParallelNativeThreadSafeWrapper<js::IsAttached>);

bool
js::ClampToUint8(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(argc == 1);
    JS_ASSERT(args[0].isNumber());
    args.rval().setNumber(ClampDoubleToUint8(args[0].toNumber()));
    return true;
}

const JSJitInfo js::ClampToUint8JitInfo =
    JS_JITINFO_NATIVE_PARALLEL(
        JSParallelNativeThreadSafeWrapper<js::ClampToUint8>);

bool
js::Memcpy(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 5);
    JS_ASSERT(args[0].isObject() && IsTypedDatum(args[0].toObject()));
    JS_ASSERT(args[1].isInt32());
    JS_ASSERT(args[2].isObject() && IsTypedDatum(args[2].toObject()));
    JS_ASSERT(args[3].isInt32());
    JS_ASSERT(args[4].isInt32());

    uint8_t *target = TypedMem(args[0].toObject()) + args[1].toInt32();
    uint8_t *source = TypedMem(args[2].toObject()) + args[3].toInt32();
    int32_t size = args[4].toInt32();
    memcpy(target, source, size);
    args.rval().setUndefined();
    return true;
}

const JSJitInfo js::MemcpyJitInfo =
    JS_JITINFO_NATIVE_PARALLEL(
        JSParallelNativeThreadSafeWrapper<js::Memcpy>);

bool
js::StandardTypeObjectDescriptors(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<GlobalObject*> global(cx, cx->global());
    args.rval().setObject(global->getTypedObject());
    return true;
}

#define JS_STORE_SCALAR_CLASS_IMPL(_constant, T, _name)                       \
bool                                                                          \
js::StoreScalar##T::Func(ThreadSafeContext *, unsigned argc, Value *vp)       \
{                                                                             \
    CallArgs args = CallArgsFromVp(argc, vp);                                 \
    JS_ASSERT(args.length() == 3);                                            \
    JS_ASSERT(args[0].isObject() && IsTypedDatum(args[0].toObject()));        \
    JS_ASSERT(args[1].isInt32());                                             \
    JS_ASSERT(args[2].isNumber());                                            \
                                                                              \
    int32_t offset = args[1].toInt32();                                       \
                                                                              \
    /* Should be guaranteed by the typed objects API: */                      \
    JS_ASSERT(offset % MOZ_ALIGNOF(T) == 0);                                  \
                                                                              \
    T *target = (T*) (TypedMem(args[0].toObject()) + offset);                 \
    double d = args[2].toNumber();                                            \
    *target = ConvertScalar<T>(d);                                            \
    args.rval().setUndefined();                                               \
    return true;                                                              \
}                                                                             \
                                                                              \
const JSJitInfo                                                               \
js::StoreScalar##T::JitInfo =                                                 \
    JS_JITINFO_NATIVE_PARALLEL(                                               \
        JSParallelNativeThreadSafeWrapper<Func>);

#define JS_STORE_REFERENCE_CLASS_IMPL(_constant, T, _name)                      \
bool                                                                            \
js::StoreReference##T::Func(ThreadSafeContext *, unsigned argc, Value *vp)      \
{                                                                               \
    CallArgs args = CallArgsFromVp(argc, vp);                                   \
    JS_ASSERT(args.length() == 3);                                              \
    JS_ASSERT(args[0].isObject() && IsTypedDatum(args[0].toObject()));          \
    JS_ASSERT(args[1].isInt32());                                               \
                                                                                \
    int32_t offset = args[1].toInt32();                                         \
                                                                                \
    /* Should be guaranteed by the typed objects API: */                        \
    JS_ASSERT(offset % MOZ_ALIGNOF(T) == 0);                                    \
                                                                                \
    uint8_t *mem = TypedMem(args[0].toObject()) + offset;                       \
    T *heap = reinterpret_cast<T*>(mem);                                        \
    store(heap, args[2]);                                                       \
    args.rval().setUndefined();                                                 \
    return true;                                                                \
}                                                                               \
                                                                                \
const JSJitInfo                                                                 \
js::StoreReference##T::JitInfo =                                                \
    JS_JITINFO_NATIVE_PARALLEL(                                                 \
        JSParallelNativeThreadSafeWrapper<Func>);

#define JS_LOAD_SCALAR_CLASS_IMPL(_constant, T, _name)                        \
bool                                                                          \
js::LoadScalar##T::Func(ThreadSafeContext *, unsigned argc, Value *vp)        \
{                                                                             \
    CallArgs args = CallArgsFromVp(argc, vp);                                 \
    JS_ASSERT(args.length() == 2);                                            \
    JS_ASSERT(args[0].isObject() && IsTypedDatum(args[0].toObject()));        \
    JS_ASSERT(args[1].isInt32());                                             \
                                                                              \
    int32_t offset = args[1].toInt32();                                       \
                                                                              \
    /* Should be guaranteed by the typed objects API: */                      \
    JS_ASSERT(offset % MOZ_ALIGNOF(T) == 0);                                  \
                                                                              \
    T *target = (T*) (TypedMem(args[0].toObject()) + offset);                 \
    args.rval().setNumber((double) *target);                                  \
    return true;                                                              \
}                                                                             \
                                                                              \
const JSJitInfo                                                               \
js::LoadScalar##T::JitInfo =                                                  \
    JS_JITINFO_NATIVE_PARALLEL(                                               \
        JSParallelNativeThreadSafeWrapper<Func>);

#define JS_LOAD_REFERENCE_CLASS_IMPL(_constant, T, _name)                     \
bool                                                                          \
js::LoadReference##T::Func(ThreadSafeContext *, unsigned argc, Value *vp)     \
{                                                                             \
    CallArgs args = CallArgsFromVp(argc, vp);                                 \
    JS_ASSERT(args.length() == 2);                                            \
    JS_ASSERT(args[0].isObject() && IsTypedDatum(args[0].toObject()));        \
    JS_ASSERT(args[1].isInt32());                                             \
                                                                              \
    int32_t offset = args[1].toInt32();                                       \
                                                                              \
    /* Should be guaranteed by the typed objects API: */                      \
    JS_ASSERT(offset % MOZ_ALIGNOF(T) == 0);                                  \
                                                                              \
    uint8_t *mem = TypedMem(args[0].toObject()) + offset;                     \
    T *heap = reinterpret_cast<T*>(mem);                                      \
    load(heap, args.rval());                                                  \
    return true;                                                              \
}                                                                             \
                                                                              \
const JSJitInfo                                                               \
js::LoadReference##T::JitInfo =                                               \
    JS_JITINFO_NATIVE_PARALLEL(                                               \
        JSParallelNativeThreadSafeWrapper<Func>);

// Because the precise syntax for storing values/objects/strings
// differs, we abstract it away using specialized variants of the
// private methods `store()` and `load()`.

void
StoreReferenceHeapValue::store(HeapValue *heap, const Value &v)
{
    *heap = v;
}

void
StoreReferenceHeapPtrObject::store(HeapPtrObject *heap, const Value &v)
{
    JS_ASSERT(v.isObjectOrNull()); // or else Store_object is being misused
    *heap = v.toObjectOrNull();
}

void
StoreReferenceHeapPtrString::store(HeapPtrString *heap, const Value &v)
{
    JS_ASSERT(v.isString()); // or else Store_string is being misused
    *heap = v.toString();
}

void
LoadReferenceHeapValue::load(HeapValue *heap,
                             MutableHandleValue v)
{
    v.set(*heap);
}

void
LoadReferenceHeapPtrObject::load(HeapPtrObject *heap,
                                 MutableHandleValue v)
{
    if (*heap)
        v.setObject(**heap);
    else
        v.setNull();
}

void
LoadReferenceHeapPtrString::load(HeapPtrString *heap,
                                 MutableHandleValue v)
{
    v.setString(*heap);
}

// I was using templates for this stuff instead of macros, but ran
// into problems with the Unagi compiler.
JS_FOR_EACH_UNIQUE_SCALAR_TYPE_REPR_CTYPE(JS_STORE_SCALAR_CLASS_IMPL)
JS_FOR_EACH_UNIQUE_SCALAR_TYPE_REPR_CTYPE(JS_LOAD_SCALAR_CLASS_IMPL)
JS_FOR_EACH_REFERENCE_TYPE_REPR(JS_STORE_REFERENCE_CLASS_IMPL)
JS_FOR_EACH_REFERENCE_TYPE_REPR(JS_LOAD_REFERENCE_CLASS_IMPL)
