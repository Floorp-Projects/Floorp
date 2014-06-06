/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/TypedObject.h"

#include "mozilla/Casting.h"
#include "mozilla/CheckedInt.h"

#include "jscompartment.h"
#include "jsfun.h"
#include "jsobj.h"
#include "jsutil.h"

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

using mozilla::CheckedInt32;
using mozilla::DebugOnly;
using mozilla::SafeCast;

using namespace js;

const Class js::TypedObjectModuleObject::class_ = {
    "TypedObject",
    JSCLASS_HAS_RESERVED_SLOTS(SlotCount) |
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
    JS_SELF_HOSTED_FN("objectType", "TypeOfTypedObject", 1, 0),
    JS_SELF_HOSTED_FN("storage", "StorageOfTypedObject", 1, 0),
    JS_FS_END
};

static void
ReportCannotConvertTo(JSContext *cx, HandleValue fromValue, const char *toType)
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                         InformalValueTypeName(fromValue), toType);
}

template<class T>
static inline T*
ToObjectIf(HandleValue value)
{
    if (!value.isObject())
        return nullptr;

    if (!value.toObject().is<T>())
        return nullptr;

    return &value.toObject().as<T>();
}

static inline CheckedInt32 roundUpToAlignment(CheckedInt32 address, int32_t align)
{
    JS_ASSERT(IsPowerOfTwo(align));

    // Note: Be careful to order operators such that we first make the
    // value smaller and then larger, so that we don't get false
    // overflow errors due to (e.g.) adding `align` and then
    // subtracting `1` afterwards when merely adding `align-1` would
    // not have overflowed. Note that due to the nature of two's
    // complement representation, if `address` is already aligned,
    // then adding `align-1` cannot itself cause an overflow.

    return ((address + (align - 1)) / align) * align;
}

/*
 * Overwrites the contents of `typedObj` at offset `offset` with `val`
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
 * - typedObj = line
 * - offset = sizeof(Point) == 8
 * - val = {x: 22, y: 44}
 * This would result in loading the value of `x`, converting
 * it to a float32, and hen storing it at the appropriate offset,
 * and then doing the same for `y`.
 *
 * Note that the type of `typeObj` may not be the
 * type of `typedObj` but rather some subcomponent of `typedObj`.
 */
static bool
ConvertAndCopyTo(JSContext *cx,
                 HandleTypeDescr typeObj,
                 HandleTypedObject typedObj,
                 int32_t offset,
                 HandleValue val)
{
    RootedFunction func(
        cx, SelfHostedFunction(cx, cx->names().ConvertAndCopyTo));
    if (!func)
        return false;

    InvokeArgs args(cx);
    if (!args.init(4))
        return false;

    args.setCallee(ObjectValue(*func));
    args[0].setObject(*typeObj);
    args[1].setObject(*typedObj);
    args[2].setInt32(offset);
    args[3].set(val);

    return Invoke(cx, args);
}

static bool
ConvertAndCopyTo(JSContext *cx, HandleTypedObject typedObj, HandleValue val)
{
    Rooted<TypeDescr*> type(cx, &typedObj->typeDescr());
    return ConvertAndCopyTo(cx, type, typedObj, 0, val);
}

/*
 * Overwrites the contents of `typedObj` at offset `offset` with `val`
 * converted to the type `typeObj`
 */
static bool
Reify(JSContext *cx,
      HandleTypeDescr type,
      HandleTypedObject typedObj,
      size_t offset,
      MutableHandleValue to)
{
    RootedFunction func(cx, SelfHostedFunction(cx, cx->names().Reify));
    if (!func)
        return false;

    InvokeArgs args(cx);
    if (!args.init(3))
        return false;

    args.setCallee(ObjectValue(*func));
    args[0].setObject(*type);
    args[1].setObject(*typedObj);
    args[2].setInt32(offset);

    if (!Invoke(cx, args))
        return false;

    to.set(args.rval());
    return true;
}

// Extracts the `prototype` property from `obj`, throwing if it is
// missing or not an object.
static JSObject *
GetPrototype(JSContext *cx, HandleObject obj)
{
    RootedValue prototypeVal(cx);
    if (!JSObject::getProperty(cx, obj, obj, cx->names().prototype,
                               &prototypeVal))
    {
        return nullptr;
    }
    if (!prototypeVal.isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_INVALID_PROTOTYPE);
        return nullptr;
    }
    return &prototypeVal.toObject();
}

/***************************************************************************
 * Typed Prototypes
 *
 * Every type descriptor has an associated prototype. Instances of
 * that type descriptor use this as their prototype. Per the spec,
 * typed object prototypes cannot be mutated.
 */

const Class js::TypedProto::class_ = {
    "TypedProto",
    JSCLASS_HAS_RESERVED_SLOTS(JS_TYPROTO_SLOTS),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

/***************************************************************************
 * Scalar type objects
 *
 * Scalar type objects like `uint8`, `uint16`, are all instances of
 * the ScalarTypeDescr class. Like all type objects, they have a reserved
 * slot pointing to a TypeRepresentation object, which is used to
 * distinguish which scalar type object this actually is.
 */

const Class js::ScalarTypeDescr::class_ = {
    "Scalar",
    JSCLASS_HAS_RESERVED_SLOTS(JS_DESCR_SLOTS),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,
    ScalarTypeDescr::call,
    nullptr,
    nullptr,
    nullptr
};

const JSFunctionSpec js::ScalarTypeDescr::typeObjectMethods[] = {
    JS_SELF_HOSTED_FN("toSource", "DescrToSource", 0, 0),
    {"array", {nullptr, nullptr}, 1, 0, "ArrayShorthand"},
    {"equivalent", {nullptr, nullptr}, 1, 0, "TypeDescrEquivalent"},
    JS_FS_END
};

int32_t
ScalarTypeDescr::size(Type t)
{
    return Scalar::byteSize(t);
}

int32_t
ScalarTypeDescr::alignment(Type t)
{
    return Scalar::byteSize(t);
}

/*static*/ const char *
ScalarTypeDescr::typeName(Type type)
{
    switch (type) {
#define NUMERIC_TYPE_TO_STRING(constant_, type_, name_) \
        case constant_: return #name_;
        JS_FOR_EACH_SCALAR_TYPE_REPR(NUMERIC_TYPE_TO_STRING)
#undef NUMERIC_TYPE_TO_STRING
      case Scalar::TypeMax:
        MOZ_CRASH();
    }
    MOZ_ASSUME_UNREACHABLE("Invalid type");
}

bool
ScalarTypeDescr::call(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             args.callee().getClass()->name, "0", "s");
        return false;
    }

    Rooted<ScalarTypeDescr *> descr(cx, &args.callee().as<ScalarTypeDescr>());
    ScalarTypeDescr::Type type = descr->type();

    double number;
    if (!ToNumber(cx, args[0], &number))
        return false;

    if (type == Scalar::Uint8Clamped)
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
      case Scalar::TypeMax:
        MOZ_CRASH();
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

const Class js::ReferenceTypeDescr::class_ = {
    "Reference",
    JSCLASS_HAS_RESERVED_SLOTS(JS_DESCR_SLOTS),
    JS_PropertyStub,       /* addProperty */
    JS_DeletePropertyStub, /* delProperty */
    JS_PropertyStub,       /* getProperty */
    JS_StrictPropertyStub, /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,
    ReferenceTypeDescr::call,
    nullptr,
    nullptr,
    nullptr
};

const JSFunctionSpec js::ReferenceTypeDescr::typeObjectMethods[] = {
    JS_SELF_HOSTED_FN("toSource", "DescrToSource", 0, 0),
    {"array", {nullptr, nullptr}, 1, 0, "ArrayShorthand"},
    {"equivalent", {nullptr, nullptr}, 1, 0, "TypeDescrEquivalent"},
    JS_FS_END
};

static int32_t ReferenceSizes[] = {
#define REFERENCE_SIZE(_kind, _type, _name)                        \
    sizeof(_type),
    JS_FOR_EACH_REFERENCE_TYPE_REPR(REFERENCE_SIZE) 0
#undef REFERENCE_SIZE
};

int32_t
ReferenceTypeDescr::size(Type t)
{
    return ReferenceSizes[t];
}

int32_t
ReferenceTypeDescr::alignment(Type t)
{
    return ReferenceSizes[t];
}

/*static*/ const char *
ReferenceTypeDescr::typeName(Type type)
{
    switch (type) {
#define NUMERIC_TYPE_TO_STRING(constant_, type_, name_) \
        case constant_: return #name_;
        JS_FOR_EACH_REFERENCE_TYPE_REPR(NUMERIC_TYPE_TO_STRING)
#undef NUMERIC_TYPE_TO_STRING
    }
    MOZ_ASSUME_UNREACHABLE("Invalid type");
}

bool
js::ReferenceTypeDescr::call(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JS_ASSERT(args.callee().is<ReferenceTypeDescr>());
    Rooted<ReferenceTypeDescr *> descr(cx, &args.callee().as<ReferenceTypeDescr>());

    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_MORE_ARGS_NEEDED,
                             descr->typeName(), "0", "s");
        return false;
    }

    switch (descr->type()) {
      case ReferenceTypeDescr::TYPE_ANY:
        args.rval().set(args[0]);
        return true;

      case ReferenceTypeDescr::TYPE_OBJECT:
      {
        RootedObject obj(cx, ToObject(cx, args[0]));
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
      }

      case ReferenceTypeDescr::TYPE_STRING:
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
 * X4 type objects
 *
 * Note: these are partially defined in SIMD.cpp
 */

static int32_t X4Sizes[] = {
#define X4_SIZE(_kind, _type, _name)                        \
    sizeof(_type) * 4,
    JS_FOR_EACH_X4_TYPE_REPR(X4_SIZE) 0
#undef X4_SIZE
};

int32_t
X4TypeDescr::size(Type t)
{
    return X4Sizes[t];
}

int32_t
X4TypeDescr::alignment(Type t)
{
    return X4Sizes[t];
}

/***************************************************************************
 * ArrayMetaTypeDescr class
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
static TypedProto *
CreatePrototypeObjectForComplexTypeInstance(JSContext *cx,
                                            Handle<TypeDescr*> descr,
                                            HandleObject ctorPrototype)
{
    RootedObject ctorPrototypePrototype(cx, GetPrototype(cx, ctorPrototype));
    if (!ctorPrototypePrototype)
        return nullptr;

    Rooted<TypedProto*> result(cx);
    result = NewObjectWithProto<TypedProto>(cx,
                                            &*ctorPrototypePrototype,
                                            nullptr,
                                            TenuredObject);
    if (!result)
        return nullptr;

    result->initTypeDescrSlot(*descr);
    return result;
}

const Class UnsizedArrayTypeDescr::class_ = {
    "ArrayType",
    JSCLASS_HAS_RESERVED_SLOTS(JS_DESCR_SLOTS),
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
    TypedObject::constructUnsized,
    nullptr
};

const Class SizedArrayTypeDescr::class_ = {
    "ArrayType",
    JSCLASS_HAS_RESERVED_SLOTS(JS_DESCR_SLOTS),
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
    TypedObject::constructSized,
    nullptr
};

const JSPropertySpec ArrayMetaTypeDescr::typeObjectProperties[] = {
    JS_PS_END
};

const JSFunctionSpec ArrayMetaTypeDescr::typeObjectMethods[] = {
    {"array", {nullptr, nullptr}, 1, 0, "ArrayShorthand"},
    JS_FN("dimension", UnsizedArrayTypeDescr::dimension, 1, 0),
    JS_SELF_HOSTED_FN("toSource", "DescrToSource", 0, 0),
    {"equivalent", {nullptr, nullptr}, 1, 0, "TypeDescrEquivalent"},
    JS_SELF_HOSTED_FN("build",    "TypedObjectArrayTypeBuild", 3, 0),
    JS_SELF_HOSTED_FN("buildPar", "TypedObjectArrayTypeBuildPar", 3, 0),
    JS_SELF_HOSTED_FN("from",     "TypedObjectArrayTypeFrom", 3, 0),
    JS_SELF_HOSTED_FN("fromPar",  "TypedObjectArrayTypeFromPar", 3, 0),
    JS_FS_END
};

const JSPropertySpec ArrayMetaTypeDescr::typedObjectProperties[] = {
    JS_PS_END
};

const JSFunctionSpec ArrayMetaTypeDescr::typedObjectMethods[] = {
    {"forEach", {nullptr, nullptr}, 1, 0, "ArrayForEach"},
    {"redimension", {nullptr, nullptr}, 1, 0, "TypedArrayRedimension"},
    JS_SELF_HOSTED_FN("map",        "TypedArrayMap",        2, 0),
    JS_SELF_HOSTED_FN("mapPar",     "TypedArrayMapPar",     2, 0),
    JS_SELF_HOSTED_FN("reduce",     "TypedArrayReduce",     2, 0),
    JS_SELF_HOSTED_FN("reducePar",  "TypedArrayReducePar",  2, 0),
    JS_SELF_HOSTED_FN("scatter",    "TypedArrayScatter",    4, 0),
    JS_SELF_HOSTED_FN("scatterPar", "TypedArrayScatterPar", 4, 0),
    JS_SELF_HOSTED_FN("filter",     "TypedArrayFilter",     1, 0),
    JS_SELF_HOSTED_FN("filterPar",  "TypedArrayFilterPar",  1, 0),
    JS_FS_END
};

bool
js::CreateUserSizeAndAlignmentProperties(JSContext *cx, HandleTypeDescr descr)
{
    // If data is transparent, also store the public slots.
    if (descr->transparent() && descr->is<SizedTypeDescr>()) {
        Rooted<SizedTypeDescr*> sizedDescr(cx, &descr->as<SizedTypeDescr>());

        // byteLength
        RootedValue typeByteLength(cx, Int32Value(sizedDescr->size()));
        if (!JSObject::defineProperty(cx, descr, cx->names().byteLength,
                                      typeByteLength,
                                      nullptr, nullptr,
                                      JSPROP_READONLY | JSPROP_PERMANENT))
        {
            return false;
        }

        // byteAlignment
        RootedValue typeByteAlignment(cx, Int32Value(sizedDescr->alignment()));
        if (!JSObject::defineProperty(cx, descr, cx->names().byteAlignment,
                                      typeByteAlignment,
                                      nullptr, nullptr,
                                      JSPROP_READONLY | JSPROP_PERMANENT))
        {
            return false;
        }
    } else {
        // byteLength
        if (!JSObject::defineProperty(cx, descr, cx->names().byteLength,
                                      UndefinedHandleValue,
                                      nullptr, nullptr,
                                      JSPROP_READONLY | JSPROP_PERMANENT))
        {
            return false;
        }

        // byteAlignment
        if (!JSObject::defineProperty(cx, descr, cx->names().byteAlignment,
                                      UndefinedHandleValue,
                                      nullptr, nullptr,
                                      JSPROP_READONLY | JSPROP_PERMANENT))
        {
            return false;
        }
    }

    // variable -- true for unsized arrays
    RootedValue variable(cx, BooleanValue(!descr->is<SizedTypeDescr>()));
    if (!JSObject::defineProperty(cx, descr, cx->names().variable,
                                  variable,
                                  nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
    {
        return false;
    }

    return true;
}

template<class T>
T *
ArrayMetaTypeDescr::create(JSContext *cx,
                           HandleObject arrayTypePrototype,
                           HandleSizedTypeDescr elementType,
                           HandleAtom stringRepr,
                           int32_t size)
{
    Rooted<T*> obj(cx);
    obj = NewObjectWithProto<T>(cx, arrayTypePrototype, nullptr, TenuredObject);
    if (!obj)
        return nullptr;

    obj->initReservedSlot(JS_DESCR_SLOT_KIND, Int32Value(T::Kind));
    obj->initReservedSlot(JS_DESCR_SLOT_STRING_REPR, StringValue(stringRepr));
    obj->initReservedSlot(JS_DESCR_SLOT_ALIGNMENT, Int32Value(elementType->alignment()));
    obj->initReservedSlot(JS_DESCR_SLOT_SIZE, Int32Value(size));
    obj->initReservedSlot(JS_DESCR_SLOT_OPAQUE, BooleanValue(elementType->opaque()));
    obj->initReservedSlot(JS_DESCR_SLOT_ARRAY_ELEM_TYPE, ObjectValue(*elementType));

    RootedValue elementTypeVal(cx, ObjectValue(*elementType));
    if (!JSObject::defineProperty(cx, obj, cx->names().elementType,
                                  elementTypeVal, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return nullptr;

    if (!CreateUserSizeAndAlignmentProperties(cx, obj))
        return nullptr;

    Rooted<TypedProto*> prototypeObj(cx);
    prototypeObj = CreatePrototypeObjectForComplexTypeInstance(cx, obj, arrayTypePrototype);
    if (!prototypeObj)
        return nullptr;

    obj->initReservedSlot(JS_DESCR_SLOT_TYPROTO, ObjectValue(*prototypeObj));

    if (!LinkConstructorAndPrototype(cx, obj, prototypeObj))
        return nullptr;

    return obj;
}

bool
ArrayMetaTypeDescr::construct(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.isConstructing()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_NOT_FUNCTION, "ArrayType");
        return false;
    }

    RootedObject arrayTypeGlobal(cx, &args.callee());

    // Expect one argument which is a sized type object
    if (args.length() < 1) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             "ArrayType", "0", "");
        return false;
    }

    if (!args[0].isObject() || !args[0].toObject().is<SizedTypeDescr>()) {
        ReportCannotConvertTo(cx, args[0], "ArrayType element specifier");
        return false;
    }

    Rooted<SizedTypeDescr*> elementType(cx);
    elementType = &args[0].toObject().as<SizedTypeDescr>();

    // Construct a canonical string `new ArrayType(<elementType>)`:
    StringBuffer contents(cx);
    contents.append("new ArrayType(");
    contents.append(&elementType->stringRepr());
    contents.append(")");
    RootedAtom stringRepr(cx, contents.finishAtom());
    if (!stringRepr)
        return false;

    // Extract ArrayType.prototype
    RootedObject arrayTypePrototype(cx, GetPrototype(cx, arrayTypeGlobal));
    if (!arrayTypePrototype)
        return false;

    // Create the instance of ArrayType
    Rooted<UnsizedArrayTypeDescr *> obj(cx);
    obj = create<UnsizedArrayTypeDescr>(cx, arrayTypePrototype, elementType,
                                        stringRepr, 0);
    if (!obj)
        return false;

    // Add `length` property, which is undefined for an unsized array.
    if (!JSObject::defineProperty(cx, obj, cx->names().length,
                                  UndefinedHandleValue, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return false;

    args.rval().setObject(*obj);
    return true;
}

/*static*/ bool
UnsizedArrayTypeDescr::dimension(JSContext *cx, unsigned int argc, jsval *vp)
{
    // Expect that the `this` pointer is an unsized array type
    // and the first argument is an integer size.
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() != 1 ||
        !args.thisv().isObject() ||
        !args.thisv().toObject().is<UnsizedArrayTypeDescr>() ||
        !args[0].isInt32() ||
        args[0].toInt32() < 0)
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_TYPEDOBJECT_ARRAYTYPE_BAD_ARGS);
        return false;
    }

    // Extract arguments.
    Rooted<UnsizedArrayTypeDescr*> unsizedTypeDescr(cx);
    unsizedTypeDescr = &args.thisv().toObject().as<UnsizedArrayTypeDescr>();
    int32_t length = args[0].toInt32();
    Rooted<SizedTypeDescr*> elementType(cx, &unsizedTypeDescr->elementType());

    // Compute the size.
    CheckedInt32 size = CheckedInt32(elementType->size()) * length;
    if (!size.isValid()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_TYPEDOBJECT_TOO_BIG);
        return false;
    }

    // Construct a canonical string `new ArrayType(<elementType>).dimension(N)`:
    StringBuffer contents(cx);
    contents.append("new ArrayType(");
    contents.append(&elementType->stringRepr());
    contents.append(").dimension(");
    if (!NumberValueToStringBuffer(cx, NumberValue(length), contents))
        return false;
    contents.append(")");
    RootedAtom stringRepr(cx, contents.finishAtom());
    if (!stringRepr)
        return false;

    // Create the sized type object.
    Rooted<SizedArrayTypeDescr*> obj(cx);
    obj = ArrayMetaTypeDescr::create<SizedArrayTypeDescr>(cx, unsizedTypeDescr,
                                                          elementType,
                                                          stringRepr, size.value());
    if (!obj)
        return false;

    obj->initReservedSlot(JS_DESCR_SLOT_SIZED_ARRAY_LENGTH,
                          Int32Value(length));

    // Add `length` property.
    RootedValue lengthVal(cx, Int32Value(length));
    if (!JSObject::defineProperty(cx, obj, cx->names().length,
                                  lengthVal, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return false;

    // Add `unsized` property, which is a link from the sized
    // array to the unsized array.
    RootedValue unsizedTypeDescrValue(cx, ObjectValue(*unsizedTypeDescr));
    if (!JSObject::defineProperty(cx, obj, cx->names().unsized,
                                  unsizedTypeDescrValue, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return false;

    args.rval().setObject(*obj);
    return true;
}

bool
js::IsTypedObjectArray(JSObject &obj)
{
    if (!obj.is<TypedObject>())
        return false;
    TypeDescr& d = obj.as<TypedObject>().typeDescr();
    return d.is<SizedArrayTypeDescr>() || d.is<UnsizedArrayTypeDescr>();
}

/*********************************
 * StructType class
 */

const Class StructTypeDescr::class_ = {
    "StructType",
    JSCLASS_HAS_RESERVED_SLOTS(JS_DESCR_SLOTS),
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr, /* finalize */
    nullptr, /* call */
    nullptr, /* hasInstance */
    TypedObject::constructSized,
    nullptr  /* trace */
};

const JSPropertySpec StructMetaTypeDescr::typeObjectProperties[] = {
    JS_PS_END
};

const JSFunctionSpec StructMetaTypeDescr::typeObjectMethods[] = {
    {"array", {nullptr, nullptr}, 1, 0, "ArrayShorthand"},
    JS_SELF_HOSTED_FN("toSource", "DescrToSource", 0, 0),
    {"equivalent", {nullptr, nullptr}, 1, 0, "TypeDescrEquivalent"},
    JS_FS_END
};

const JSPropertySpec StructMetaTypeDescr::typedObjectProperties[] = {
    JS_PS_END
};

const JSFunctionSpec StructMetaTypeDescr::typedObjectMethods[] = {
    JS_FS_END
};

JSObject *
StructMetaTypeDescr::create(JSContext *cx,
                            HandleObject metaTypeDescr,
                            HandleObject fields)
{
    // Obtain names of fields, which are the own properties of `fields`
    AutoIdVector ids(cx);
    if (!GetPropertyNames(cx, fields, JSITER_OWNONLY | JSITER_SYMBOLS, &ids))
        return nullptr;

    // Iterate through each field. Collect values for the various
    // vectors below and also track total size and alignment. Be wary
    // of overflow!
    StringBuffer stringBuffer(cx);     // Canonical string repr
    AutoValueVector fieldNames(cx);    // Name of each field.
    AutoValueVector fieldTypeObjs(cx); // Type descriptor of each field.
    AutoValueVector fieldOffsets(cx);  // Offset of each field field.
    RootedObject userFieldOffsets(cx); // User-exposed {f:offset} object
    RootedObject userFieldTypes(cx);   // User-exposed {f:descr} object.
    CheckedInt32 sizeSoFar(0);         // Size of struct thus far.
    int32_t alignment = 1;             // Alignment of struct.
    bool opaque = false;               // Opacity of struct.

    userFieldOffsets = NewObjectWithProto<JSObject>(cx, nullptr, nullptr, TenuredObject);
    if (!userFieldOffsets)
        return nullptr;

    userFieldTypes = NewObjectWithProto<JSObject>(cx, nullptr, nullptr, TenuredObject);
    if (!userFieldTypes)
        return nullptr;

    if (!stringBuffer.append("new StructType({")) {
        js_ReportOutOfMemory(cx);
        return nullptr;
    }

    RootedValue fieldTypeVal(cx);
    RootedId id(cx);
    Rooted<SizedTypeDescr*> fieldType(cx);
    for (unsigned int i = 0; i < ids.length(); i++) {
        id = ids[i];

        // Check that all the property names are non-numeric strings.
        uint32_t unused;
        if (!JSID_IS_ATOM(id) || JSID_TO_ATOM(id)->isIndex(&unused)) {
            RootedValue idValue(cx, IdToValue(id));
            ReportCannotConvertTo(cx, idValue, "StructType field name");
            return nullptr;
        }

        // Load the value for the current field from the `fields` object.
        // The value should be a type descriptor.
        if (!JSObject::getGeneric(cx, fields, fields, id, &fieldTypeVal))
            return nullptr;
        fieldType = ToObjectIf<SizedTypeDescr>(fieldTypeVal);
        if (!fieldType) {
            ReportCannotConvertTo(cx, fieldTypeVal, "StructType field specifier");
            return nullptr;
        }

        // Collect field name and type object
        RootedValue fieldName(cx, IdToValue(id));
        if (!fieldNames.append(fieldName)) {
            js_ReportOutOfMemory(cx);
            return nullptr;
        }
        if (!fieldTypeObjs.append(ObjectValue(*fieldType))) {
            js_ReportOutOfMemory(cx);
            return nullptr;
        }

        // userFieldTypes[id] = typeObj
        if (!JSObject::defineGeneric(cx, userFieldTypes, id,
                                     fieldTypeObjs[i], nullptr, nullptr,
                                     JSPROP_READONLY | JSPROP_PERMANENT))
            return nullptr;

        // Append "f:Type" to the string repr
        if (i > 0 && !stringBuffer.append(", ")) {
            js_ReportOutOfMemory(cx);
            return nullptr;
        }
        if (!stringBuffer.append(JSID_TO_ATOM(id))) {
            js_ReportOutOfMemory(cx);
            return nullptr;
        }
        if (!stringBuffer.append(": ")) {
            js_ReportOutOfMemory(cx);
            return nullptr;
        }
        if (!stringBuffer.append(&fieldType->stringRepr())) {
            js_ReportOutOfMemory(cx);
            return nullptr;
        }

        // Offset of this field is the current total size adjusted for
        // the field's alignment.
        CheckedInt32 offset = roundUpToAlignment(sizeSoFar, fieldType->alignment());
        if (!offset.isValid()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                 JSMSG_TYPEDOBJECT_TOO_BIG);
            return nullptr;
        }
        MOZ_ASSERT(offset.value() >= 0);
        if (!fieldOffsets.append(Int32Value(offset.value()))) {
            js_ReportOutOfMemory(cx);
            return nullptr;
        }

        // userFieldOffsets[id] = offset
        RootedValue offsetValue(cx, Int32Value(offset.value()));
        if (!JSObject::defineGeneric(cx, userFieldOffsets, id,
                                     offsetValue, nullptr, nullptr,
                                     JSPROP_READONLY | JSPROP_PERMANENT))
            return nullptr;

        // Add space for this field to the total struct size.
        sizeSoFar = offset + fieldType->size();
        if (!sizeSoFar.isValid()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                 JSMSG_TYPEDOBJECT_TOO_BIG);
            return nullptr;
        }

        // Struct is opaque if any field is opaque
        if (fieldType->opaque())
            opaque = true;

        // Alignment of the struct is the max of the alignment of its fields.
        alignment = js::Max(alignment, fieldType->alignment());
    }

    // Complete string representation.
    if (!stringBuffer.append("})")) {
        js_ReportOutOfMemory(cx);
        return nullptr;
    }
    RootedAtom stringRepr(cx, stringBuffer.finishAtom());
    if (!stringRepr)
        return nullptr;

    // Adjust the total size to be a multiple of the final alignment.
    CheckedInt32 totalSize = roundUpToAlignment(sizeSoFar, alignment);
    if (!totalSize.isValid()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_TYPEDOBJECT_TOO_BIG);
        return nullptr;
    }

    // Now create the resulting type descriptor.
    RootedObject structTypePrototype(cx, GetPrototype(cx, metaTypeDescr));
    if (!structTypePrototype)
        return nullptr;

    Rooted<StructTypeDescr*> descr(cx);
    descr = NewObjectWithProto<StructTypeDescr>(cx, structTypePrototype, nullptr,
                                                TenuredObject);
    if (!descr)
        return nullptr;

    descr->initReservedSlot(JS_DESCR_SLOT_KIND, Int32Value(type::Struct));
    descr->initReservedSlot(JS_DESCR_SLOT_STRING_REPR, StringValue(stringRepr));
    descr->initReservedSlot(JS_DESCR_SLOT_ALIGNMENT, Int32Value(alignment));
    descr->initReservedSlot(JS_DESCR_SLOT_SIZE, Int32Value(totalSize.value()));
    descr->initReservedSlot(JS_DESCR_SLOT_OPAQUE, BooleanValue(opaque));

    // Construct for internal use an array with the name for each field.
    {
        RootedObject fieldNamesVec(cx);
        fieldNamesVec = NewDenseCopiedArray(cx, fieldNames.length(),
                                            fieldNames.begin(), nullptr,
                                            TenuredObject);
        if (!fieldNamesVec)
            return nullptr;
        descr->initReservedSlot(JS_DESCR_SLOT_STRUCT_FIELD_NAMES,
                                     ObjectValue(*fieldNamesVec));
    }

    // Construct for internal use an array with the type object for each field.
    {
        RootedObject fieldTypeVec(cx);
        fieldTypeVec = NewDenseCopiedArray(cx, fieldTypeObjs.length(),
                                           fieldTypeObjs.begin(), nullptr,
                                           TenuredObject);
        if (!fieldTypeVec)
            return nullptr;
        descr->initReservedSlot(JS_DESCR_SLOT_STRUCT_FIELD_TYPES,
                                     ObjectValue(*fieldTypeVec));
    }

    // Construct for internal use an array with the offset for each field.
    {
        RootedObject fieldOffsetsVec(cx);
        fieldOffsetsVec = NewDenseCopiedArray(cx, fieldOffsets.length(),
                                              fieldOffsets.begin(), nullptr,
                                              TenuredObject);
        if (!fieldOffsetsVec)
            return nullptr;
        descr->initReservedSlot(JS_DESCR_SLOT_STRUCT_FIELD_OFFSETS,
                                     ObjectValue(*fieldOffsetsVec));
    }

    // Create data properties fieldOffsets and fieldTypes
    if (!JSObject::freeze(cx, userFieldOffsets))
        return nullptr;
    if (!JSObject::freeze(cx, userFieldTypes))
        return nullptr;
    RootedValue userFieldOffsetsValue(cx, ObjectValue(*userFieldOffsets));
    if (!JSObject::defineProperty(cx, descr, cx->names().fieldOffsets,
                                  userFieldOffsetsValue, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
    {
        return nullptr;
    }
    RootedValue userFieldTypesValue(cx, ObjectValue(*userFieldTypes));
    if (!JSObject::defineProperty(cx, descr, cx->names().fieldTypes,
                                  userFieldTypesValue, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
    {
        return nullptr;
    }

    if (!CreateUserSizeAndAlignmentProperties(cx, descr))
        return nullptr;

    Rooted<TypedProto*> prototypeObj(cx);
    prototypeObj = CreatePrototypeObjectForComplexTypeInstance(cx, descr, structTypePrototype);
    if (!prototypeObj)
        return nullptr;

    descr->initReservedSlot(JS_DESCR_SLOT_TYPROTO, ObjectValue(*prototypeObj));

    if (!LinkConstructorAndPrototype(cx, descr, prototypeObj))
        return nullptr;

    return descr;
}

bool
StructMetaTypeDescr::construct(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.isConstructing()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_NOT_FUNCTION, "StructType");
        return false;
    }

    if (args.length() >= 1 && args[0].isObject()) {
        RootedObject metaTypeDescr(cx, &args.callee());
        RootedObject fields(cx, &args[0].toObject());
        RootedObject obj(cx, create(cx, metaTypeDescr, fields));
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                         JSMSG_TYPEDOBJECT_STRUCTTYPE_BAD_ARGS);
    return false;
}

size_t
StructTypeDescr::fieldCount() const
{
    return getReservedSlot(JS_DESCR_SLOT_STRUCT_FIELD_NAMES).toObject().getDenseInitializedLength();
}

bool
StructTypeDescr::fieldIndex(jsid id, size_t *out) const
{
    JSObject &fieldNames = getReservedSlot(JS_DESCR_SLOT_STRUCT_FIELD_NAMES).toObject();
    size_t l = fieldNames.getDenseInitializedLength();
    for (size_t i = 0; i < l; i++) {
        JSAtom &a = fieldNames.getDenseElement(i).toString()->asAtom();
        if (JSID_IS_ATOM(id, &a)) {
            *out = i;
            return true;
        }
    }
    return false;
}

JSAtom &
StructTypeDescr::fieldName(size_t index) const
{
    JSObject &fieldNames = getReservedSlot(JS_DESCR_SLOT_STRUCT_FIELD_NAMES).toObject();
    return fieldNames.getDenseElement(index).toString()->asAtom();
}

size_t
StructTypeDescr::fieldOffset(size_t index) const
{
    JSObject &fieldOffsets =
        getReservedSlot(JS_DESCR_SLOT_STRUCT_FIELD_OFFSETS).toObject();
    JS_ASSERT(index < fieldOffsets.getDenseInitializedLength());
    return SafeCast<size_t>(fieldOffsets.getDenseElement(index).toInt32());
}

SizedTypeDescr&
StructTypeDescr::fieldDescr(size_t index) const
{
    JSObject &fieldDescrs =
        getReservedSlot(JS_DESCR_SLOT_STRUCT_FIELD_TYPES).toObject();
    JS_ASSERT(index < fieldDescrs.getDenseInitializedLength());
    return fieldDescrs.getDenseElement(index).toObject().as<SizedTypeDescr>();
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
 *          Object.prototype
 */

// Here `T` is either `ScalarTypeDescr` or `ReferenceTypeDescr`
template<typename T>
static bool
DefineSimpleTypeDescr(JSContext *cx,
                      Handle<GlobalObject *> global,
                      HandleObject module,
                      typename T::Type type,
                      HandlePropertyName className)
{
    RootedObject objProto(cx, global->getOrCreateObjectPrototype(cx));
    if (!objProto)
        return false;

    RootedObject funcProto(cx, global->getOrCreateFunctionPrototype(cx));
    if (!funcProto)
        return false;

    Rooted<T*> descr(cx);
    descr = NewObjectWithProto<T>(cx, funcProto, global, TenuredObject);
    if (!descr)
        return false;

    descr->initReservedSlot(JS_DESCR_SLOT_KIND, Int32Value(T::Kind));
    descr->initReservedSlot(JS_DESCR_SLOT_STRING_REPR, StringValue(className));
    descr->initReservedSlot(JS_DESCR_SLOT_ALIGNMENT, Int32Value(T::alignment(type)));
    descr->initReservedSlot(JS_DESCR_SLOT_SIZE, Int32Value(T::size(type)));
    descr->initReservedSlot(JS_DESCR_SLOT_OPAQUE, BooleanValue(T::Opaque));
    descr->initReservedSlot(JS_DESCR_SLOT_TYPE, Int32Value(type));

    if (!CreateUserSizeAndAlignmentProperties(cx, descr))
        return false;

    if (!JS_DefineFunctions(cx, descr, T::typeObjectMethods))
        return false;

    // Create the typed prototype for the scalar type. This winds up
    // not being user accessible, but we still create one for consistency.
    Rooted<TypedProto*> proto(cx);
    proto = NewObjectWithProto<TypedProto>(cx, objProto, nullptr, TenuredObject);
    if (!proto)
        return false;
    proto->initTypeDescrSlot(*descr);
    descr->initReservedSlot(JS_DESCR_SLOT_TYPROTO, ObjectValue(*proto));

    RootedValue descrValue(cx, ObjectValue(*descr));
    if (!JSObject::defineProperty(cx, module, className,
                                  descrValue, nullptr, nullptr, 0))
    {
        return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////

template<typename T>
static JSObject *
DefineMetaTypeDescr(JSContext *cx,
                    Handle<GlobalObject*> global,
                    HandleObject module,
                    TypedObjectModuleObject::Slot protoSlot)
{
    RootedAtom className(cx, Atomize(cx, T::class_.name,
                                     strlen(T::class_.name)));
    if (!className)
        return nullptr;

    RootedObject funcProto(cx, global->getOrCreateFunctionPrototype(cx));
    if (!funcProto)
        return nullptr;

    // Create ctor.prototype, which inherits from Function.__proto__

    RootedObject proto(cx, NewObjectWithProto<JSObject>(cx, funcProto, global,
                                                        SingletonObject));
    if (!proto)
        return nullptr;

    // Create ctor.prototype.prototype, which inherits from Object.__proto__

    RootedObject objProto(cx, global->getOrCreateObjectPrototype(cx));
    if (!objProto)
        return nullptr;
    RootedObject protoProto(cx);
    protoProto = NewObjectWithProto<JSObject>(cx, objProto,
                                              global, SingletonObject);
    if (!protoProto)
        return nullptr;

    RootedValue protoProtoValue(cx, ObjectValue(*protoProto));
    if (!JSObject::defineProperty(cx, proto, cx->names().prototype,
                                  protoProtoValue,
                                  nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return nullptr;

    // Create ctor itself

    const int constructorLength = 2;
    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, T::construct, className, constructorLength);
    if (!ctor ||
        !LinkConstructorAndPrototype(cx, ctor, proto) ||
        !DefinePropertiesAndFunctions(cx, proto,
                                      T::typeObjectProperties,
                                      T::typeObjectMethods) ||
        !DefinePropertiesAndFunctions(cx, protoProto,
                                      T::typedObjectProperties,
                                      T::typedObjectMethods))
    {
        return nullptr;
    }

    module->initReservedSlot(protoSlot, ObjectValue(*proto));

    return ctor;
}

/*  The initialization strategy for TypedObjects is mildly unusual
 * compared to other classes. Because all of the types are members
 * of a single global, `TypedObject`, we basically make the
 * initializer for the `TypedObject` class populate the
 * `TypedObject` global (which is referred to as "module" herein).
 */
bool
GlobalObject::initTypedObjectModule(JSContext *cx, Handle<GlobalObject*> global)
{
    RootedObject objProto(cx, global->getOrCreateObjectPrototype(cx));
    if (!objProto)
        return false;

    Rooted<TypedObjectModuleObject*> module(cx);
    module = NewObjectWithProto<TypedObjectModuleObject>(cx, objProto, global);
    if (!module)
        return false;

    if (!JS_DefineFunctions(cx, module, TypedObjectMethods))
        return false;

    // uint8, uint16, any, etc

#define BINARYDATA_SCALAR_DEFINE(constant_, type_, name_)                       \
    if (!DefineSimpleTypeDescr<ScalarTypeDescr>(cx, global, module, constant_,      \
                                            cx->names().name_))                 \
        return false;
    JS_FOR_EACH_SCALAR_TYPE_REPR(BINARYDATA_SCALAR_DEFINE)
#undef BINARYDATA_SCALAR_DEFINE

#define BINARYDATA_REFERENCE_DEFINE(constant_, type_, name_)                    \
    if (!DefineSimpleTypeDescr<ReferenceTypeDescr>(cx, global, module, constant_,   \
                                               cx->names().name_))              \
        return false;
    JS_FOR_EACH_REFERENCE_TYPE_REPR(BINARYDATA_REFERENCE_DEFINE)
#undef BINARYDATA_REFERENCE_DEFINE

    // ArrayType.

    RootedObject arrayType(cx);
    arrayType = DefineMetaTypeDescr<ArrayMetaTypeDescr>(
        cx, global, module, TypedObjectModuleObject::ArrayTypePrototype);
    if (!arrayType)
        return false;

    RootedValue arrayTypeValue(cx, ObjectValue(*arrayType));
    if (!JSObject::defineProperty(cx, module, cx->names().ArrayType,
                                  arrayTypeValue,
                                  nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return false;

    // StructType.

    RootedObject structType(cx);
    structType = DefineMetaTypeDescr<StructMetaTypeDescr>(
        cx, global, module, TypedObjectModuleObject::StructTypePrototype);
    if (!structType)
        return false;

    RootedValue structTypeValue(cx, ObjectValue(*structType));
    if (!JSObject::defineProperty(cx, module, cx->names().StructType,
                                  structTypeValue,
                                  nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return false;

    // Everything is setup, install module on the global object:
    RootedValue moduleValue(cx, ObjectValue(*module));
    global->setConstructor(JSProto_TypedObject, moduleValue);
    if (!JSObject::defineProperty(cx, global, cx->names().TypedObject,
                                  moduleValue,
                                  nullptr, nullptr,
                                  0))
    {
        return false;
    }

    return module;
}

JSObject *
js_InitTypedObjectModuleObject(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->is<GlobalObject>());
    Rooted<GlobalObject *> global(cx, &obj->as<GlobalObject>());
    return global->getOrCreateTypedObjectModule(cx);
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
 * Typed objects
 */

/*static*/ TypedObject *
TypedObject::createUnattached(JSContext *cx,
                             HandleTypeDescr descr,
                             int32_t length)
{
    if (descr->opaque())
        return createUnattachedWithClass(cx, &OpaqueTypedObject::class_, descr, length);
    else
        return createUnattachedWithClass(cx, &TransparentTypedObject::class_, descr, length);
}


/*static*/ TypedObject *
TypedObject::createUnattachedWithClass(JSContext *cx,
                                      const Class *clasp,
                                      HandleTypeDescr type,
                                      int32_t length)
{
    JS_ASSERT(clasp == &TransparentTypedObject::class_ ||
              clasp == &OpaqueTypedObject::class_);
    JS_ASSERT(JSCLASS_RESERVED_SLOTS(clasp) == JS_TYPEDOBJ_SLOTS);
    JS_ASSERT(clasp->hasPrivate());

    RootedObject proto(cx);
    if (type->is<SimpleTypeDescr>()) {
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

    RootedObject obj(cx, NewObjectWithClassProto(cx, clasp, &*proto, nullptr));
    if (!obj)
        return nullptr;

    obj->initPrivate(nullptr);
    obj->initReservedSlot(JS_BUFVIEW_SLOT_BYTEOFFSET, Int32Value(0));
    obj->initReservedSlot(JS_BUFVIEW_SLOT_LENGTH, Int32Value(length));
    obj->initReservedSlot(JS_BUFVIEW_SLOT_OWNER, NullValue());
    obj->initReservedSlot(JS_BUFVIEW_SLOT_NEXT_VIEW, PrivateValue(nullptr));

    return static_cast<TypedObject*>(&*obj);
}

void
TypedObject::attach(ArrayBufferObject &buffer, int32_t offset)
{
    JS_ASSERT(offset >= 0);
    JS_ASSERT((size_t) (offset + size()) <= buffer.byteLength());

    buffer.addView(this);
    InitArrayBufferViewDataPointer(this, &buffer, offset);
    setReservedSlot(JS_BUFVIEW_SLOT_BYTEOFFSET, Int32Value(offset));
    setReservedSlot(JS_BUFVIEW_SLOT_OWNER, ObjectValue(buffer));
}

void
TypedObject::attach(TypedObject &typedObj, int32_t offset)
{
    JS_ASSERT(!typedObj.owner().isNeutered());
    JS_ASSERT(typedObj.typedMem() != NULL);

    attach(typedObj.owner(), typedObj.offset() + offset);
}

// Returns a suitable JS_TYPEDOBJ_SLOT_LENGTH value for an instance of
// the type `type`. `type` must not be an unsized array.
static int32_t
TypedObjLengthFromType(TypeDescr &descr)
{
    switch (descr.kind()) {
      case type::Scalar:
      case type::Reference:
      case type::Struct:
      case type::X4:
        return 0;

      case type::SizedArray:
        return descr.as<SizedArrayTypeDescr>().length();

      case type::UnsizedArray:
        MOZ_ASSUME_UNREACHABLE("TypedObjLengthFromType() invoked on unsized type");
    }
    MOZ_ASSUME_UNREACHABLE("Invalid kind");
}

/*static*/ TypedObject *
TypedObject::createDerived(JSContext *cx, HandleSizedTypeDescr type,
                           HandleTypedObject typedObj, int32_t offset)
{
    JS_ASSERT(!typedObj->owner().isNeutered());
    JS_ASSERT(typedObj->typedMem() != NULL);
    JS_ASSERT(offset <= typedObj->size());
    JS_ASSERT(offset + type->size() <= typedObj->size());

    int32_t length = TypedObjLengthFromType(*type);

    const js::Class *clasp = typedObj->getClass();
    Rooted<TypedObject*> obj(cx);
    obj = createUnattachedWithClass(cx, clasp, type, length);
    if (!obj)
        return nullptr;

    obj->attach(*typedObj, offset);
    return obj;
}

/*static*/ TypedObject *
TypedObject::createZeroed(JSContext *cx,
                          HandleTypeDescr descr,
                          int32_t length)
{
    // Create unattached wrapper object.
    Rooted<TypedObject*> obj(cx, createUnattached(cx, descr, length));
    if (!obj)
        return nullptr;

    // Allocate and initialize the memory for this instance.
    // Also initialize the JS_TYPEDOBJ_SLOT_LENGTH slot.
    switch (descr->kind()) {
      case type::Scalar:
      case type::Reference:
      case type::Struct:
      case type::X4:
      case type::SizedArray:
      {
        size_t totalSize = descr->as<SizedTypeDescr>().size();
        Rooted<ArrayBufferObject*> buffer(cx);
        buffer = ArrayBufferObject::create(cx, totalSize);
        if (!buffer)
            return nullptr;
        descr->as<SizedTypeDescr>().initInstances(cx->runtime(), buffer->dataPointer(), 1);
        obj->attach(*buffer, 0);
        return obj;
      }

      case type::UnsizedArray:
      {
        Rooted<SizedTypeDescr*> elementTypeRepr(cx);
        elementTypeRepr = &descr->as<UnsizedArrayTypeDescr>().elementType();

        CheckedInt32 totalSize = CheckedInt32(elementTypeRepr->size()) * length;
        if (!totalSize.isValid()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                 JSMSG_TYPEDOBJECT_TOO_BIG);
            return nullptr;
        }

        Rooted<ArrayBufferObject*> buffer(cx);
        buffer = ArrayBufferObject::create(cx, totalSize.value());
        if (!buffer)
            return nullptr;

        if (length)
            elementTypeRepr->initInstances(cx->runtime(), buffer->dataPointer(), length);
        obj->attach(*buffer, 0);
        return obj;
      }
    }

    MOZ_ASSUME_UNREACHABLE("Bad TypeRepresentation Kind");
}

static bool
ReportTypedObjTypeError(JSContext *cx,
                        const unsigned errorNumber,
                        HandleTypedObject obj)
{
    // Serialize type string of obj
    char *typeReprStr = JS_EncodeString(cx, &obj->typeDescr().stringRepr());
    if (!typeReprStr)
        return false;

    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                         errorNumber, typeReprStr);

    JS_free(cx, (void *) typeReprStr);
    return false;
}

/*static*/ void
TypedObject::obj_trace(JSTracer *trace, JSObject *object)
{
    ArrayBufferViewObject::trace(trace, object);

    JS_ASSERT(object->is<TypedObject>());
    TypedObject &typedObj = object->as<TypedObject>();
    TypeDescr &descr = typedObj.typeDescr();
    if (descr.opaque()) {
        uint8_t *mem = typedObj.typedMem();
        if (!mem)
            return; // partially constructed

        if (typedObj.owner().isNeutered())
            return;

        switch (descr.kind()) {
          case type::Scalar:
          case type::Reference:
          case type::Struct:
          case type::SizedArray:
          case type::X4:
            descr.as<SizedTypeDescr>().traceInstances(trace, mem, 1);
            break;

          case type::UnsizedArray:
            descr.as<UnsizedArrayTypeDescr>().elementType().traceInstances(trace, mem, typedObj.length());
            break;
        }
    }
}

bool
TypedObject::obj_lookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                              MutableHandleObject objp, MutableHandleShape propp)
{
    JS_ASSERT(obj->is<TypedObject>());

    Rooted<TypeDescr*> descr(cx, &obj->as<TypedObject>().typeDescr());
    switch (descr->kind()) {
      case type::Scalar:
      case type::Reference:
      case type::X4:
        break;

      case type::SizedArray:
      case type::UnsizedArray:
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

      case type::Struct:
      {
        StructTypeDescr &structDescr = descr->as<StructTypeDescr>();
        size_t index;
        if (structDescr.fieldIndex(id, &index)) {
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
TypedObject::obj_lookupProperty(JSContext *cx,
                                HandleObject obj,
                                HandlePropertyName name,
                                MutableHandleObject objp,
                                MutableHandleShape propp)
{
    RootedId id(cx, NameToId(name));
    return obj_lookupGeneric(cx, obj, id, objp, propp);
}

bool
TypedObject::obj_lookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                                MutableHandleObject objp, MutableHandleShape propp)
{
    JS_ASSERT(obj->is<TypedObject>());
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
TypedObject::obj_defineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue v,
                              PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    return ReportPropertyError(cx, JSMSG_UNDEFINED_PROP, id);
}

bool
TypedObject::obj_defineProperty(JSContext *cx, HandleObject obj,
                               HandlePropertyName name, HandleValue v,
                               PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    Rooted<jsid> id(cx, NameToId(name));
    return obj_defineGeneric(cx, obj, id, v, getter, setter, attrs);
}

bool
TypedObject::obj_defineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue v,
                               PropertyOp getter, StrictPropertyOp setter, unsigned attrs)
{
    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);
    Rooted<jsid> id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return obj_defineGeneric(cx, obj, id, v, getter, setter, attrs);
}

bool
TypedObject::obj_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver,
                           HandleId id, MutableHandleValue vp)
{
    JS_ASSERT(obj->is<TypedObject>());
    Rooted<TypedObject *> typedObj(cx, &obj->as<TypedObject>());

    // Dispatch elements to obj_getElement:
    uint32_t index;
    if (js_IdIsIndex(id, &index))
        return obj_getElement(cx, obj, receiver, index, vp);

    // Handle everything else here:

    switch (typedObj->typeDescr().kind()) {
      case type::Scalar:
      case type::Reference:
        break;

      case type::X4:
        break;

      case type::SizedArray:
      case type::UnsizedArray:
        if (JSID_IS_ATOM(id, cx->names().length)) {
            if (typedObj->owner().isNeutered() || !typedObj->typedMem()) { // unattached
                JS_ReportErrorNumber(
                    cx, js_GetErrorMessage,
                    nullptr, JSMSG_TYPEDOBJECT_HANDLE_UNATTACHED);
                return false;
            }

            vp.setInt32(typedObj->length());
            return true;
        }
        break;

      case type::Struct: {
        Rooted<StructTypeDescr*> descr(cx, &typedObj->typeDescr().as<StructTypeDescr>());

        size_t fieldIndex;
        if (!descr->fieldIndex(id, &fieldIndex))
            break;

        size_t offset = descr->fieldOffset(fieldIndex);
        Rooted<SizedTypeDescr*> fieldType(cx, &descr->fieldDescr(fieldIndex));
        return Reify(cx, fieldType, typedObj, offset, vp);
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
TypedObject::obj_getProperty(JSContext *cx, HandleObject obj, HandleObject receiver,
                              HandlePropertyName name, MutableHandleValue vp)
{
    RootedId id(cx, NameToId(name));
    return obj_getGeneric(cx, obj, receiver, id, vp);
}

bool
TypedObject::obj_getElement(JSContext *cx, HandleObject obj, HandleObject receiver,
                             uint32_t index, MutableHandleValue vp)
{
    JS_ASSERT(obj->is<TypedObject>());
    Rooted<TypedObject *> typedObj(cx, &obj->as<TypedObject>());
    Rooted<TypeDescr *> descr(cx, &typedObj->typeDescr());

    switch (descr->kind()) {
      case type::Scalar:
      case type::Reference:
      case type::X4:
      case type::Struct:
        break;

      case type::SizedArray:
        return obj_getArrayElement<SizedArrayTypeDescr>(cx, typedObj, descr,
                                                        index, vp);

      case type::UnsizedArray:
        return obj_getArrayElement<UnsizedArrayTypeDescr>(cx, typedObj, descr,
                                                          index, vp);
    }

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        vp.setUndefined();
        return true;
    }

    return JSObject::getElement(cx, proto, receiver, index, vp);
}

template<class T>
/*static*/ bool
TypedObject::obj_getArrayElement(JSContext *cx,
                                Handle<TypedObject*> typedObj,
                                Handle<TypeDescr*> typeDescr,
                                uint32_t index,
                                MutableHandleValue vp)
{
    JS_ASSERT(typeDescr->is<T>());

    if (index >= (size_t) typedObj->length()) {
        vp.setUndefined();
        return true;
    }

    Rooted<SizedTypeDescr*> elementType(cx, &typeDescr->as<T>().elementType());
    size_t offset = elementType->size() * index;
    return Reify(cx, elementType, typedObj, offset, vp);
}

bool
TypedObject::obj_setGeneric(JSContext *cx, HandleObject obj, HandleId id,
                           MutableHandleValue vp, bool strict)
{
    JS_ASSERT(obj->is<TypedObject>());
    Rooted<TypedObject *> typedObj(cx, &obj->as<TypedObject>());

    uint32_t index;
    if (js_IdIsIndex(id, &index))
        return obj_setElement(cx, obj, index, vp, strict);

    switch (typedObj->typeDescr().kind()) {
      case type::Scalar:
      case type::Reference:
        break;

      case type::X4:
        break;

      case type::SizedArray:
      case type::UnsizedArray:
        if (JSID_IS_ATOM(id, cx->names().length)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                 nullptr, JSMSG_CANT_REDEFINE_ARRAY_LENGTH);
            return false;
        }
        break;

      case type::Struct: {
        Rooted<StructTypeDescr*> descr(cx, &typedObj->typeDescr().as<StructTypeDescr>());

        size_t fieldIndex;
        if (!descr->fieldIndex(id, &fieldIndex))
            break;

        size_t offset = descr->fieldOffset(fieldIndex);
        Rooted<SizedTypeDescr*> fieldType(cx, &descr->fieldDescr(fieldIndex));
        return ConvertAndCopyTo(cx, fieldType, typedObj, offset, vp);
      }
    }

    return ReportTypedObjTypeError(cx, JSMSG_OBJECT_NOT_EXTENSIBLE, typedObj);
}

bool
TypedObject::obj_setProperty(JSContext *cx, HandleObject obj,
                             HandlePropertyName name, MutableHandleValue vp,
                             bool strict)
{
    RootedId id(cx, NameToId(name));
    return obj_setGeneric(cx, obj, id, vp, strict);
}

bool
TypedObject::obj_setElement(JSContext *cx, HandleObject obj, uint32_t index,
                           MutableHandleValue vp, bool strict)
{
    JS_ASSERT(obj->is<TypedObject>());
    Rooted<TypedObject *> typedObj(cx, &obj->as<TypedObject>());
    Rooted<TypeDescr *> descr(cx, &typedObj->typeDescr());

    switch (descr->kind()) {
      case type::Scalar:
      case type::Reference:
      case type::X4:
      case type::Struct:
        break;

      case type::SizedArray:
        return obj_setArrayElement<SizedArrayTypeDescr>(cx, typedObj, descr, index, vp);

      case type::UnsizedArray:
        return obj_setArrayElement<UnsizedArrayTypeDescr>(cx, typedObj, descr, index, vp);
    }

    return ReportTypedObjTypeError(cx, JSMSG_OBJECT_NOT_EXTENSIBLE, typedObj);
}

template<class T>
/*static*/ bool
TypedObject::obj_setArrayElement(JSContext *cx,
                                Handle<TypedObject*> typedObj,
                                Handle<TypeDescr*> descr,
                                uint32_t index,
                                MutableHandleValue vp)
{
    JS_ASSERT(descr->is<T>());

    if (index >= (size_t) typedObj->length()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             nullptr, JSMSG_TYPEDOBJECT_BINARYARRAY_BAD_INDEX);
        return false;
    }

    Rooted<SizedTypeDescr*> elementType(cx);
    elementType = &descr->as<T>().elementType();
    size_t offset = elementType->size() * index;
    return ConvertAndCopyTo(cx, elementType, typedObj, offset, vp);
}

bool
TypedObject::obj_getGenericAttributes(JSContext *cx, HandleObject obj,
                                     HandleId id, unsigned *attrsp)
{
    uint32_t index;
    Rooted<TypedObject *> typedObj(cx, &obj->as<TypedObject>());
    switch (typedObj->typeDescr().kind()) {
      case type::Scalar:
      case type::Reference:
        break;

      case type::X4:
        break;

      case type::SizedArray:
      case type::UnsizedArray:
        if (js_IdIsIndex(id, &index)) {
            *attrsp = JSPROP_ENUMERATE | JSPROP_PERMANENT;
            return true;
        }
        if (JSID_IS_ATOM(id, cx->names().length)) {
            *attrsp = JSPROP_READONLY | JSPROP_PERMANENT;
            return true;
        }
        break;

      case type::Struct:
        size_t index;
        if (typedObj->typeDescr().as<StructTypeDescr>().fieldIndex(id, &index)) {
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
    Rooted<TypedObject *> typedObj(cx, &obj->as<TypedObject>());
    switch (typedObj->typeDescr().kind()) {
      case type::Scalar:
      case type::Reference:
      case type::X4:
        return false;

      case type::SizedArray:
      case type::UnsizedArray:
        return js_IdIsIndex(id, &index) || JSID_IS_ATOM(id, cx->names().length);

      case type::Struct:
        size_t index;
        if (typedObj->typeDescr().as<StructTypeDescr>().fieldIndex(id, &index))
            return true;
    }

    return false;
}

bool
TypedObject::obj_setGenericAttributes(JSContext *cx, HandleObject obj,
                                       HandleId id, unsigned *attrsp)
{
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
TypedObject::obj_deleteGeneric(JSContext *cx, HandleObject obj, HandleId id, bool *succeeded)
{
    if (IsOwnId(cx, obj, id))
        return ReportPropertyError(cx, JSMSG_CANT_DELETE, id);

    RootedObject proto(cx, obj->getProto());
    if (!proto) {
        *succeeded = false;
        return true;
    }

    return JSObject::deleteGeneric(cx, proto, id, succeeded);
}

bool
TypedObject::obj_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
                           MutableHandleValue statep, MutableHandleId idp)
{
    int32_t index;

    JS_ASSERT(obj->is<TypedObject>());
    Rooted<TypedObject *> typedObj(cx, &obj->as<TypedObject>());
    Rooted<TypeDescr *> descr(cx, &typedObj->typeDescr());
    switch (descr->kind()) {
      case type::Scalar:
      case type::Reference:
      case type::X4:
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

      case type::SizedArray:
      case type::UnsizedArray:
        switch (enum_op) {
          case JSENUMERATE_INIT_ALL:
          case JSENUMERATE_INIT:
            statep.setInt32(0);
            idp.set(INT_TO_JSID(typedObj->length()));
            break;

          case JSENUMERATE_NEXT:
            index = statep.toInt32();

            if (index < typedObj->length()) {
                idp.set(INT_TO_JSID(index));
                statep.setInt32(index + 1);
            } else {
                JS_ASSERT(index == typedObj->length());
                statep.setNull();
            }

            break;

          case JSENUMERATE_DESTROY:
            statep.setNull();
            break;
        }
        break;

      case type::Struct:
        switch (enum_op) {
          case JSENUMERATE_INIT_ALL:
          case JSENUMERATE_INIT:
            statep.setInt32(0);
            idp.set(INT_TO_JSID(descr->as<StructTypeDescr>().fieldCount()));
            break;

          case JSENUMERATE_NEXT:
            index = static_cast<uint32_t>(statep.toInt32());

            if ((size_t) index < descr->as<StructTypeDescr>().fieldCount()) {
                idp.set(AtomToId(&descr->as<StructTypeDescr>().fieldName(index)));
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
TypedObject::offsetOfOwnerSlot()
{
    return JSObject::getFixedSlotOffset(JS_BUFVIEW_SLOT_OWNER);
}

/* static */ size_t
TypedObject::offsetOfDataSlot()
{
#   ifdef DEBUG
    // Compute offset of private data based on TransparentTypedObject;
    // both OpaqueTypedObject and TransparentTypedObject have the same
    // number of slots, so no problem there.
    gc::AllocKind allocKind = gc::GetGCObjectKind(&TransparentTypedObject::class_);
    size_t nfixed = gc::GetGCKindSlots(allocKind);
    JS_ASSERT(JS_TYPEDOBJ_SLOT_DATA == nfixed - 1);
#   endif

    return JSObject::getPrivateDataOffset(JS_TYPEDOBJ_SLOT_DATA);
}

/* static */ size_t
TypedObject::offsetOfByteOffsetSlot()
{
    return JSObject::getFixedSlotOffset(JS_BUFVIEW_SLOT_BYTEOFFSET);
}

void
TypedObject::neuter(void *newData)
{
    setSlot(JS_BUFVIEW_SLOT_LENGTH, Int32Value(0));
    setSlot(JS_BUFVIEW_SLOT_BYTEOFFSET, Int32Value(0));
    setPrivate(newData);
}

/******************************************************************************
 * Typed Objects
 */

const Class TransparentTypedObject::class_ = {
    "TypedObject",
    Class::NON_NATIVE |
    JSCLASS_HAS_RESERVED_SLOTS(JS_TYPEDOBJ_SLOTS) |
    JSCLASS_HAS_PRIVATE |
    JSCLASS_IMPLEMENTS_BARRIERS,
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,        /* finalize    */
    nullptr,        /* call        */
    nullptr,        /* hasInstance */
    nullptr,        /* construct   */
    TypedObject::obj_trace,
    JS_NULL_CLASS_SPEC,
    JS_NULL_CLASS_EXT,
    {
        TypedObject::obj_lookupGeneric,
        TypedObject::obj_lookupProperty,
        TypedObject::obj_lookupElement,
        TypedObject::obj_defineGeneric,
        TypedObject::obj_defineProperty,
        TypedObject::obj_defineElement,
        TypedObject::obj_getGeneric,
        TypedObject::obj_getProperty,
        TypedObject::obj_getElement,
        TypedObject::obj_setGeneric,
        TypedObject::obj_setProperty,
        TypedObject::obj_setElement,
        TypedObject::obj_getGenericAttributes,
        TypedObject::obj_setGenericAttributes,
        TypedObject::obj_deleteGeneric,
        nullptr, nullptr, // watch/unwatch
        nullptr,   /* slice */
        TypedObject::obj_enumerate,
        nullptr, /* thisObject */
    }
};

static int32_t
LengthForType(TypeDescr &descr)
{
    switch (descr.kind()) {
      case type::Scalar:
      case type::Reference:
      case type::Struct:
      case type::X4:
        return 0;

      case type::SizedArray:
        return descr.as<SizedArrayTypeDescr>().length();

      case type::UnsizedArray:
        return 0;
    }

    MOZ_ASSUME_UNREACHABLE("Invalid kind");
}

static bool
CheckOffset(int32_t offset,
            int32_t size,
            int32_t alignment,
            int32_t bufferLength)
{
    JS_ASSERT(size >= 0);
    JS_ASSERT(alignment >= 0);

    // No negative offsets.
    if (offset < 0)
        return false;

    // Offset (plus size) must be fully contained within the buffer.
    if (offset > bufferLength)
        return false;
    if (offset + size < offset)
        return false;
    if (offset + size > bufferLength)
        return false;

    // Offset must be aligned.
    if ((offset % alignment) != 0)
        return false;

    return true;
}

/*static*/ bool
TypedObject::constructSized(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JS_ASSERT(args.callee().is<SizedTypeDescr>());
    Rooted<SizedTypeDescr*> callee(cx, &args.callee().as<SizedTypeDescr>());

    // Typed object constructors for sized types are overloaded in
    // three ways, in order of precedence:
    //
    //   new TypeObj()
    //   new TypeObj(buffer, [offset])
    //   new TypeObj(data)

    // Zero argument constructor:
    if (args.length() == 0) {
        int32_t length = LengthForType(*callee);
        Rooted<TypedObject*> obj(cx, createZeroed(cx, callee, length));
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    // Buffer constructor.
    if (args[0].isObject() && args[0].toObject().is<ArrayBufferObject>()) {
        Rooted<ArrayBufferObject*> buffer(cx);
        buffer = &args[0].toObject().as<ArrayBufferObject>();

        if (callee->opaque() || buffer->isNeutered()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                 nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
            return false;
        }

        int32_t offset;
        if (args.length() >= 2 && !args[1].isUndefined()) {
            if (!args[1].isInt32()) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                     nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
                return false;
            }

            offset = args[1].toInt32();
        } else {
            offset = 0;
        }

        if (args.length() >= 3 && !args[2].isUndefined()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                 nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
            return false;
        }

        if (!CheckOffset(offset, callee->size(), callee->alignment(),
                         buffer->byteLength()))
        {
            JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                 nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
            return false;
        }

        Rooted<TypedObject*> obj(cx);
        obj = TypedObject::createUnattached(cx, callee, LengthForType(*callee));
        if (!obj)
            return false;

        obj->attach(*buffer, offset);
        args.rval().setObject(*obj);
        return true;
    }

    // Data constructor.
    if (args[0].isObject()) {
        // Create the typed object.
        int32_t length = LengthForType(*callee);
        Rooted<TypedObject*> obj(cx, createZeroed(cx, callee, length));
        if (!obj)
            return false;

        // Initialize from `arg`.
        if (!ConvertAndCopyTo(cx, obj, args[0]))
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    // Something bogus.
    JS_ReportErrorNumber(cx, js_GetErrorMessage,
                         nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
    return false;
}

/*static*/ bool
TypedObject::constructUnsized(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JS_ASSERT(args.callee().is<TypeDescr>());
    Rooted<UnsizedArrayTypeDescr*> callee(cx);
    callee = &args.callee().as<UnsizedArrayTypeDescr>();

    // Typed object constructors for unsized arrays are overloaded in
    // four ways, in order of precedence:
    //
    //   new TypeObj(buffer, [offset, [length]]) // [1]
    //   new TypeObj(length)                     // [1]
    //   new TypeObj(data)
    //   new TypeObj()
    //
    // [1] Explicit lengths only available for unsized arrays.

    // Zero argument constructor:
    if (args.length() == 0) {
        Rooted<TypedObject*> obj(cx, createZeroed(cx, callee, 0));
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    // Length constructor.
    if (args[0].isInt32()) {
        int32_t length = args[0].toInt32();
        if (length < 0) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                 nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
            return false;
        }
        Rooted<TypedObject*> obj(cx, createZeroed(cx, callee, length));
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    // Buffer constructor.
    if (args[0].isObject() && args[0].toObject().is<ArrayBufferObject>()) {
        Rooted<ArrayBufferObject*> buffer(cx);
        buffer = &args[0].toObject().as<ArrayBufferObject>();

        if (callee->opaque()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                 nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
            return false;
        }

        int32_t offset;
        if (args.length() >= 2 && !args[1].isUndefined()) {
            if (!args[1].isInt32()) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                     nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
                return false;
            }

            offset = args[1].toInt32();
        } else {
            offset = 0;
        }

        if (!CheckOffset(offset, 0, callee->alignment(), buffer->byteLength())) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                 nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
            return false;
        }

        int32_t elemSize = callee->elementType().size();
        int32_t bytesRemaining = buffer->byteLength() - offset;
        int32_t maximumLength = bytesRemaining / elemSize;

        int32_t length;
        if (args.length() >= 3 && !args[2].isUndefined()) {
            if (!args[2].isInt32()) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                     nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
                return false;
            }
            length = args[2].toInt32();

            if (length < 0 || length > maximumLength) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                     nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
                return false;
            }
        } else {
            if ((bytesRemaining % elemSize) != 0) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                     nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
                return false;
            }

            length = maximumLength;
        }

        if (buffer->isNeutered()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                 nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
            return false;
        }

        Rooted<TypedObject*> obj(cx);
        obj = TypedObject::createUnattached(cx, callee, length);
        if (!obj)
            return false;

        obj->attach(args[0].toObject().as<ArrayBufferObject>(), offset);
    }

    // Data constructor for unsized values
    if (args[0].isObject()) {
        // Read length out of the object.
        RootedObject arg(cx, &args[0].toObject());
        RootedValue lengthVal(cx);
        if (!JSObject::getProperty(cx, arg, arg, cx->names().length, &lengthVal))
            return false;
        if (!lengthVal.isInt32()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                 nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
            return false;
        }
        int32_t length = lengthVal.toInt32();

        // Check that length * elementSize does not overflow.
        int32_t elementSize = callee->elementType().size();
        int32_t byteLength;
        if (!SafeMul(elementSize, length, &byteLength)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                 nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
            return false;
        }

        // Create the unsized array.
        Rooted<TypedObject*> obj(cx, createZeroed(cx, callee, length));
        if (!obj)
            return false;

        // Initialize from `arg`
        if (!ConvertAndCopyTo(cx, obj, args[0]))
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    // Something bogus.
    JS_ReportErrorNumber(cx, js_GetErrorMessage,
                         nullptr, JSMSG_TYPEDOBJECT_BAD_ARGS);
    return false;
}

/******************************************************************************
 * Handles
 */

const Class OpaqueTypedObject::class_ = {
    "Handle",
    Class::NON_NATIVE |
    JSCLASS_HAS_RESERVED_SLOTS(JS_TYPEDOBJ_SLOTS) |
    JSCLASS_HAS_PRIVATE |
    JSCLASS_IMPLEMENTS_BARRIERS,
    JS_PropertyStub,
    JS_DeletePropertyStub,
    JS_PropertyStub,
    JS_StrictPropertyStub,
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,        /* finalize    */
    nullptr,        /* call        */
    nullptr,        /* construct   */
    nullptr,        /* hasInstance */
    TypedObject::obj_trace,
    JS_NULL_CLASS_SPEC,
    JS_NULL_CLASS_EXT,
    {
        TypedObject::obj_lookupGeneric,
        TypedObject::obj_lookupProperty,
        TypedObject::obj_lookupElement,
        TypedObject::obj_defineGeneric,
        TypedObject::obj_defineProperty,
        TypedObject::obj_defineElement,
        TypedObject::obj_getGeneric,
        TypedObject::obj_getProperty,
        TypedObject::obj_getElement,
        TypedObject::obj_setGeneric,
        TypedObject::obj_setProperty,
        TypedObject::obj_setElement,
        TypedObject::obj_getGenericAttributes,
        TypedObject::obj_setGenericAttributes,
        TypedObject::obj_deleteGeneric,
        nullptr, nullptr, // watch/unwatch
        nullptr, // slice
        TypedObject::obj_enumerate,
        nullptr, /* thisObject */
    }
};

const JSFunctionSpec OpaqueTypedObject::handleStaticMethods[] = {
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
js::NewOpaqueTypedObject(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<SizedTypeDescr>());

    Rooted<SizedTypeDescr*> descr(cx, &args[0].toObject().as<SizedTypeDescr>());
    int32_t length = TypedObjLengthFromType(*descr);
    Rooted<TypedObject*> obj(cx);
    obj = TypedObject::createUnattachedWithClass(cx, &OpaqueTypedObject::class_, descr, length);
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

bool
js::NewDerivedTypedObject(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 3);
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<SizedTypeDescr>());
    JS_ASSERT(args[1].isObject() && args[1].toObject().is<TypedObject>());
    JS_ASSERT(args[2].isInt32());

    Rooted<SizedTypeDescr*> descr(cx, &args[0].toObject().as<SizedTypeDescr>());
    Rooted<TypedObject*> typedObj(cx, &args[1].toObject().as<TypedObject>());
    int32_t offset = args[2].toInt32();

    Rooted<TypedObject*> obj(cx);
    obj = TypedObject::createDerived(cx, descr, typedObj, offset);
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

bool
js::AttachTypedObject(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 3);
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedObject>());
    JS_ASSERT(args[1].isObject() && args[1].toObject().is<TypedObject>());
    JS_ASSERT(args[2].isInt32());

    TypedObject &handle = args[0].toObject().as<TypedObject>();
    TypedObject &target = args[1].toObject().as<TypedObject>();
    JS_ASSERT(handle.typedMem() == nullptr); // must not be attached already
    size_t offset = args[2].toInt32();
    handle.attach(target, offset);
    return true;
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::AttachTypedObjectJitInfo,
                                      AttachTypedObjectJitInfo,
                                      js::AttachTypedObject);

bool
js::SetTypedObjectOffset(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 2);
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedObject>());
    JS_ASSERT(args[1].isInt32());

    TypedObject &typedObj = args[0].toObject().as<TypedObject>();
    int32_t offset = args[1].toInt32();

    JS_ASSERT(!typedObj.owner().isNeutered());
    JS_ASSERT(typedObj.typedMem() != nullptr); // must be attached already

    typedObj.setPrivate(typedObj.owner().dataPointer() + offset);
    typedObj.setReservedSlot(JS_BUFVIEW_SLOT_BYTEOFFSET, Int32Value(offset));
    args.rval().setUndefined();
    return true;
}

bool
js::intrinsic_SetTypedObjectOffset(JSContext *cx, unsigned argc, Value *vp)
{
    // Do not use JSNativeThreadSafeWrapper<> so that ion can reference
    // this function more easily when inlining.
    return SetTypedObjectOffset(cx, argc, vp);
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::intrinsic_SetTypedObjectOffsetJitInfo,
                                      SetTypedObjectJitInfo,
                                      SetTypedObjectOffset);

bool
js::ObjectIsTypeDescr(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isObject());
    args.rval().setBoolean(args[0].toObject().is<TypeDescr>());
    return true;
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::ObjectIsTypeDescrJitInfo, ObjectIsTypeDescrJitInfo,
                                      js::ObjectIsTypeDescr);

bool
js::ObjectIsTypedObject(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isObject());
    args.rval().setBoolean(args[0].toObject().is<TypedObject>());
    return true;
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::ObjectIsTypedObjectJitInfo,
                                      ObjectIsTypedObjectJitInfo,
                                      js::ObjectIsTypedObject);

bool
js::ObjectIsOpaqueTypedObject(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isObject());
    args.rval().setBoolean(args[0].toObject().is<OpaqueTypedObject>());
    return true;
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::ObjectIsOpaqueTypedObjectJitInfo,
                                      ObjectIsOpaqueTypedObjectJitInfo,
                                      js::ObjectIsOpaqueTypedObject);

bool
js::ObjectIsTransparentTypedObject(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isObject());
    args.rval().setBoolean(args[0].toObject().is<TransparentTypedObject>());
    return true;
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::ObjectIsTransparentTypedObjectJitInfo,
                                      ObjectIsTransparentTypedObjectJitInfo,
                                      js::ObjectIsTransparentTypedObject);

bool
js::TypeDescrIsSimpleType(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isObject());
    JS_ASSERT(args[0].toObject().is<js::TypeDescr>());
    args.rval().setBoolean(args[0].toObject().is<js::SimpleTypeDescr>());
    return true;
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::TypeDescrIsSimpleTypeJitInfo,
                                      TypeDescrIsSimpleTypeJitInfo,
                                      js::TypeDescrIsSimpleType);

bool
js::TypeDescrIsArrayType(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isObject());
    JS_ASSERT(args[0].toObject().is<js::TypeDescr>());
    JSObject& obj = args[0].toObject();
    args.rval().setBoolean(obj.is<js::SizedArrayTypeDescr>() ||
                           obj.is<js::UnsizedArrayTypeDescr>());
    return true;
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::TypeDescrIsArrayTypeJitInfo,
                                      TypeDescrIsArrayTypeJitInfo,
                                      js::TypeDescrIsArrayType);

bool
js::TypeDescrIsSizedArrayType(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isObject());
    JS_ASSERT(args[0].toObject().is<js::TypeDescr>());
    args.rval().setBoolean(args[0].toObject().is<js::SizedArrayTypeDescr>());
    return true;
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::TypeDescrIsSizedArrayTypeJitInfo,
                                      TypeDescrIsSizedArrayTypeJitInfo,
                                      js::TypeDescrIsSizedArrayType);

bool
js::TypeDescrIsUnsizedArrayType(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isObject());
    JS_ASSERT(args[0].toObject().is<js::TypeDescr>());
    args.rval().setBoolean(args[0].toObject().is<js::UnsizedArrayTypeDescr>());
    return true;
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::TypeDescrIsUnsizedArrayTypeJitInfo,
                                      TypeDescrIsUnsizedArrayTypeJitInfo,
                                      js::TypeDescrIsUnsizedArrayType);

bool
js::TypedObjectIsAttached(ThreadSafeContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedObject>());
    TypedObject &typedObj = args[0].toObject().as<TypedObject>();
    args.rval().setBoolean(!typedObj.owner().isNeutered() && typedObj.typedMem() != nullptr);
    return true;
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::TypedObjectIsAttachedJitInfo,
                                      TypedObjectIsAttachedJitInfo,
                                      js::TypedObjectIsAttached);

bool
js::ClampToUint8(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 1);
    JS_ASSERT(args[0].isNumber());
    args.rval().setNumber(ClampDoubleToUint8(args[0].toNumber()));
    return true;
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::ClampToUint8JitInfo, ClampToUint8JitInfo,
                                      js::ClampToUint8);

bool
js::Memcpy(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 5);
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedObject>());
    JS_ASSERT(args[1].isInt32());
    JS_ASSERT(args[2].isObject() && args[2].toObject().is<TypedObject>());
    JS_ASSERT(args[3].isInt32());
    JS_ASSERT(args[4].isInt32());

    TypedObject &targetTypedObj = args[0].toObject().as<TypedObject>();
    int32_t targetOffset = args[1].toInt32();
    TypedObject &sourceTypedObj = args[2].toObject().as<TypedObject>();
    int32_t sourceOffset = args[3].toInt32();
    int32_t size = args[4].toInt32();

    JS_ASSERT(targetOffset >= 0);
    JS_ASSERT(sourceOffset >= 0);
    JS_ASSERT(size >= 0);
    JS_ASSERT(size + targetOffset <= targetTypedObj.size());
    JS_ASSERT(size + sourceOffset <= sourceTypedObj.size());

    uint8_t *target = targetTypedObj.typedMem(targetOffset);
    uint8_t *source = sourceTypedObj.typedMem(sourceOffset);
    memcpy(target, source, size);
    args.rval().setUndefined();
    return true;
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::MemcpyJitInfo, MemcpyJitInfo, js::Memcpy);

bool
js::GetTypedObjectModule(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<GlobalObject*> global(cx, cx->global());
    JS_ASSERT(global);
    args.rval().setObject(global->getTypedObjectModule());
    return true;
}

bool
js::GetFloat32x4TypeDescr(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<GlobalObject*> global(cx, cx->global());
    JS_ASSERT(global);
    args.rval().setObject(global->float32x4TypeDescr());
    return true;
}

bool
js::GetInt32x4TypeDescr(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<GlobalObject*> global(cx, cx->global());
    JS_ASSERT(global);
    args.rval().setObject(global->int32x4TypeDescr());
    return true;
}

#define JS_STORE_SCALAR_CLASS_IMPL(_constant, T, _name)                         \
bool                                                                            \
js::StoreScalar##T::Func(ThreadSafeContext *, unsigned argc, Value *vp)         \
{                                                                               \
    CallArgs args = CallArgsFromVp(argc, vp);                                   \
    JS_ASSERT(args.length() == 3);                                              \
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedObject>());      \
    JS_ASSERT(args[1].isInt32());                                               \
    JS_ASSERT(args[2].isNumber());                                              \
                                                                                \
    TypedObject &typedObj = args[0].toObject().as<TypedObject>();               \
    int32_t offset = args[1].toInt32();                                         \
                                                                                \
    /* Should be guaranteed by the typed objects API: */                        \
    JS_ASSERT(offset % MOZ_ALIGNOF(T) == 0);                                    \
                                                                                \
    T *target = reinterpret_cast<T*>(typedObj.typedMem(offset));                \
    double d = args[2].toNumber();                                              \
    *target = ConvertScalar<T>(d);                                              \
    args.rval().setUndefined();                                                 \
    return true;                                                                \
}                                                                               \
                                                                                \
JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::StoreScalar##T::JitInfo,              \
                                      StoreScalar##T,                           \
                                      js::StoreScalar##T::Func);

#define JS_STORE_REFERENCE_CLASS_IMPL(_constant, T, _name)                      \
bool                                                                            \
js::StoreReference##T::Func(ThreadSafeContext *, unsigned argc, Value *vp)      \
{                                                                               \
    CallArgs args = CallArgsFromVp(argc, vp);                                   \
    JS_ASSERT(args.length() == 3);                                              \
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedObject>());      \
    JS_ASSERT(args[1].isInt32());                                               \
                                                                                \
    TypedObject &typedObj = args[0].toObject().as<TypedObject>();               \
    int32_t offset = args[1].toInt32();                                         \
                                                                                \
    /* Should be guaranteed by the typed objects API: */                        \
    JS_ASSERT(offset % MOZ_ALIGNOF(T) == 0);                                    \
                                                                                \
    T *target = reinterpret_cast<T*>(typedObj.typedMem(offset));                \
    store(target, args[2]);                                                     \
    args.rval().setUndefined();                                                 \
    return true;                                                                \
}                                                                               \
                                                                                \
JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::StoreReference##T::JitInfo,           \
                                      StoreReference##T,                        \
                                      js::StoreReference##T::Func);

#define JS_LOAD_SCALAR_CLASS_IMPL(_constant, T, _name)                                  \
bool                                                                                    \
js::LoadScalar##T::Func(ThreadSafeContext *, unsigned argc, Value *vp)                  \
{                                                                                       \
    CallArgs args = CallArgsFromVp(argc, vp);                                           \
    JS_ASSERT(args.length() == 2);                                                      \
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedObject>());              \
    JS_ASSERT(args[1].isInt32());                                                       \
                                                                                        \
    TypedObject &typedObj = args[0].toObject().as<TypedObject>();                       \
    int32_t offset = args[1].toInt32();                                                 \
                                                                                        \
    /* Should be guaranteed by the typed objects API: */                                \
    JS_ASSERT(offset % MOZ_ALIGNOF(T) == 0);                                            \
                                                                                        \
    T *target = reinterpret_cast<T*>(typedObj.typedMem(offset));                        \
    args.rval().setNumber((double) *target);                                            \
    return true;                                                                        \
}                                                                                       \
                                                                                        \
JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::LoadScalar##T::JitInfo, LoadScalar##T,        \
                                      js::LoadScalar##T::Func);

#define JS_LOAD_REFERENCE_CLASS_IMPL(_constant, T, _name)                       \
bool                                                                            \
js::LoadReference##T::Func(ThreadSafeContext *, unsigned argc, Value *vp)       \
{                                                                               \
    CallArgs args = CallArgsFromVp(argc, vp);                                   \
    JS_ASSERT(args.length() == 2);                                              \
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedObject>());      \
    JS_ASSERT(args[1].isInt32());                                               \
                                                                                \
    TypedObject &typedObj = args[0].toObject().as<TypedObject>();               \
    int32_t offset = args[1].toInt32();                                         \
                                                                                \
    /* Should be guaranteed by the typed objects API: */                        \
    JS_ASSERT(offset % MOZ_ALIGNOF(T) == 0);                                    \
                                                                                \
    T *target = reinterpret_cast<T*>(typedObj.typedMem(offset));                \
    load(target, args.rval());                                                  \
    return true;                                                                \
}                                                                               \
                                                                                \
JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::LoadReference##T::JitInfo,            \
                                      LoadReference##T,                         \
                                      js::LoadReference##T::Func);

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

///////////////////////////////////////////////////////////////////////////
// Walking memory

template<typename V>
static void
visitReferences(SizedTypeDescr &descr,
                uint8_t *mem,
                V& visitor)
{
    if (descr.transparent())
        return;

    switch (descr.kind()) {
      case type::Scalar:
      case type::X4:
        return;

      case type::Reference:
        visitor.visitReference(descr.as<ReferenceTypeDescr>(), mem);
        return;

      case type::SizedArray:
      {
        SizedArrayTypeDescr &arrayDescr = descr.as<SizedArrayTypeDescr>();
        SizedTypeDescr &elementDescr = arrayDescr.elementType();
        for (int32_t i = 0; i < arrayDescr.length(); i++) {
            visitReferences(elementDescr, mem, visitor);
            mem += elementDescr.size();
        }
        return;
      }

      case type::UnsizedArray:
      {
        MOZ_ASSUME_UNREACHABLE("Only Sized Type representations");
      }

      case type::Struct:
      {
        StructTypeDescr &structDescr = descr.as<StructTypeDescr>();
        for (size_t i = 0; i < structDescr.fieldCount(); i++) {
            SizedTypeDescr &descr = structDescr.fieldDescr(i);
            size_t offset = structDescr.fieldOffset(i);
            visitReferences(descr, mem + offset, visitor);
        }
        return;
      }
    }

    MOZ_ASSUME_UNREACHABLE("Invalid type repr kind");
}

///////////////////////////////////////////////////////////////////////////
// Initializing instances

namespace js {
class MemoryInitVisitor {
    const JSRuntime *rt_;

  public:
    explicit MemoryInitVisitor(const JSRuntime *rt)
      : rt_(rt)
    {}

    void visitReference(ReferenceTypeDescr &descr, uint8_t *mem);
};
} // namespace js

void
js::MemoryInitVisitor::visitReference(ReferenceTypeDescr &descr, uint8_t *mem)
{
    switch (descr.type()) {
      case ReferenceTypeDescr::TYPE_ANY:
      {
        js::HeapValue *heapValue = reinterpret_cast<js::HeapValue *>(mem);
        heapValue->init(UndefinedValue());
        return;
      }

      case ReferenceTypeDescr::TYPE_OBJECT:
      {
        js::HeapPtrObject *objectPtr =
            reinterpret_cast<js::HeapPtrObject *>(mem);
        objectPtr->init(nullptr);
        return;
      }

      case ReferenceTypeDescr::TYPE_STRING:
      {
        js::HeapPtrString *stringPtr =
            reinterpret_cast<js::HeapPtrString *>(mem);
        stringPtr->init(rt_->emptyString);
        return;
      }
    }

    MOZ_ASSUME_UNREACHABLE("Invalid kind");
}

void
SizedTypeDescr::initInstances(const JSRuntime *rt, uint8_t *mem, size_t length)
{
    JS_ASSERT(length >= 1);

    MemoryInitVisitor visitor(rt);

    // Initialize the 0th instance
    memset(mem, 0, size());
    if (opaque())
        visitReferences(*this, mem, visitor);

    // Stamp out N copies of later instances
    uint8_t *target = mem;
    for (size_t i = 1; i < length; i++) {
        target += size();
        memcpy(target, mem, size());
    }
}

///////////////////////////////////////////////////////////////////////////
// Tracing instances

namespace js {
class MemoryTracingVisitor {
    JSTracer *trace_;

  public:

    explicit MemoryTracingVisitor(JSTracer *trace)
      : trace_(trace)
    {}

    void visitReference(ReferenceTypeDescr &descr, uint8_t *mem);
};
} // namespace js

void
js::MemoryTracingVisitor::visitReference(ReferenceTypeDescr &descr, uint8_t *mem)
{
    switch (descr.type()) {
      case ReferenceTypeDescr::TYPE_ANY:
      {
        js::HeapValue *heapValue = reinterpret_cast<js::HeapValue *>(mem);
        gc::MarkValue(trace_, heapValue, "reference-val");
        return;
      }

      case ReferenceTypeDescr::TYPE_OBJECT:
      {
        js::HeapPtrObject *objectPtr =
            reinterpret_cast<js::HeapPtrObject *>(mem);
        if (*objectPtr)
            gc::MarkObject(trace_, objectPtr, "reference-obj");
        return;
      }

      case ReferenceTypeDescr::TYPE_STRING:
      {
        js::HeapPtrString *stringPtr =
            reinterpret_cast<js::HeapPtrString *>(mem);
        if (*stringPtr)
            gc::MarkString(trace_, stringPtr, "reference-str");
        return;
      }
    }

    MOZ_ASSUME_UNREACHABLE("Invalid kind");
}

void
SizedTypeDescr::traceInstances(JSTracer *trace, uint8_t *mem, size_t length)
{
    MemoryTracingVisitor visitor(trace);

    for (size_t i = 0; i < length; i++) {
        visitReferences(*this, mem, visitor);
        mem += size();
    }
}

