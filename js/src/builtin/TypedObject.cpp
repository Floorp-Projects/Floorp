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
    JS_SELF_HOSTED_FN("objectType", "TypeOfTypedDatum", 1, 0),
    JS_SELF_HOSTED_FN("storage", "StorageOfTypedDatum", 1, 0),
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
                 HandleTypeDescr typeObj,
                 HandleTypedDatum datum,
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
    args[1].setObject(*datum);
    args[2].setInt32(offset);
    args[3].set(val);

    return Invoke(cx, args);
}

static bool
ConvertAndCopyTo(JSContext *cx, HandleTypedDatum datum, HandleValue val)
{
    Rooted<TypeDescr*> type(cx, &datum->typeDescr());
    return ConvertAndCopyTo(cx, type, datum, 0, val);
}

/*
 * Overwrites the contents of `datum` at offset `offset` with `val`
 * converted to the type `typeObj`
 */
static bool
Reify(JSContext *cx,
      HandleTypeDescr type,
      HandleTypedDatum datum,
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
    args[1].setObject(*datum);
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
static JSObject *
CreatePrototypeObjectForComplexTypeInstance(JSContext *cx,
                                            HandleObject ctorPrototype)
{
    RootedObject ctorPrototypePrototype(cx, GetPrototype(cx, ctorPrototype));
    if (!ctorPrototypePrototype)
        return nullptr;

    return NewObjectWithProto<JSObject>(cx, &*ctorPrototypePrototype, nullptr,
                                        TenuredObject);
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
    TypedDatum::constructUnsized,
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
    TypedDatum::constructSized,
    nullptr
};

const JSPropertySpec ArrayMetaTypeDescr::typeObjectProperties[] = {
    JS_PS_END
};

const JSFunctionSpec ArrayMetaTypeDescr::typeObjectMethods[] = {
    {"array", {nullptr, nullptr}, 1, 0, "ArrayShorthand"},
    JS_FN("dimension", UnsizedArrayTypeDescr::dimension, 1, 0),
    JS_SELF_HOSTED_FN("toSource", "DescrToSourceMethod", 0, 0),
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
js::InitializeCommonTypeDescriptorProperties(JSContext *cx,
                                             HandleTypeDescr obj,
                                             HandleObject typeReprOwnerObj)
{
    TypeRepresentation *typeRepr =
        TypeRepresentation::fromOwnerObject(*typeReprOwnerObj);

    obj->initReservedSlot(JS_DESCR_SLOT_ALIGNMENT,
                          Int32Value(typeRepr->alignment()));

    // Regardless of whether the data is transparent, we always
    // store the internal size/alignment slots.
    if (typeRepr->isSized()) {
        SizedTypeRepresentation *sizedTypeRepr = typeRepr->asSized();
        obj->initReservedSlot(JS_DESCR_SLOT_SIZE,
                              Int32Value(sizedTypeRepr->size()));
    }

    // If data is transparent, also store the public slots.
    if (typeRepr->transparent() && typeRepr->isSized()) {
        SizedTypeRepresentation *sizedTypeRepr = typeRepr->asSized();

        // byteLength
        RootedValue typeByteLength(cx, NumberValue(sizedTypeRepr->size()));
        if (!JSObject::defineProperty(cx, obj, cx->names().byteLength,
                                      typeByteLength,
                                      nullptr, nullptr,
                                      JSPROP_READONLY | JSPROP_PERMANENT))
        {
            return false;
        }

        // byteAlignment
        RootedValue typeByteAlignment(
            cx, NumberValue(sizedTypeRepr->alignment()));
        if (!JSObject::defineProperty(cx, obj, cx->names().byteAlignment,
                                      typeByteAlignment,
                                      nullptr, nullptr,
                                      JSPROP_READONLY | JSPROP_PERMANENT))
        {
            return false;
        }
    } else {
        // byteLength
        if (!JSObject::defineProperty(cx, obj, cx->names().byteLength,
                                      UndefinedHandleValue,
                                      nullptr, nullptr,
                                      JSPROP_READONLY | JSPROP_PERMANENT))
        {
            return false;
        }

        // byteAlignment
        if (!JSObject::defineProperty(cx, obj, cx->names().byteAlignment,
                                      UndefinedHandleValue,
                                      nullptr, nullptr,
                                      JSPROP_READONLY | JSPROP_PERMANENT))
        {
            return false;
        }
    }

    // variable -- true for unsized arrays
    RootedValue variable(cx, BooleanValue(!typeRepr->isSized()));
    if (!JSObject::defineProperty(cx, obj, cx->names().variable,
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
                           HandleObject arrayTypeReprObj,
                           HandleSizedTypeDescr elementType)
{
    JS_ASSERT(TypeRepresentation::isOwnerObject(*arrayTypeReprObj));

    Rooted<T*> obj(cx, NewObjectWithProto<T>(cx, arrayTypePrototype, nullptr,
                                             TenuredObject));
    if (!obj)
        return nullptr;
    obj->initReservedSlot(JS_DESCR_SLOT_TYPE_REPR,
                          ObjectValue(*arrayTypeReprObj));

    RootedValue elementTypeVal(cx, ObjectValue(*elementType));
    if (!JSObject::defineProperty(cx, obj, cx->names().elementType,
                                  elementTypeVal, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return nullptr;

    obj->initReservedSlot(JS_DESCR_SLOT_ARRAY_ELEM_TYPE, elementTypeVal);

    if (!InitializeCommonTypeDescriptorProperties(cx, obj, arrayTypeReprObj))
        return nullptr;

    RootedObject prototypeObj(cx);
    prototypeObj = CreatePrototypeObjectForComplexTypeInstance(cx, arrayTypePrototype);
    if (!prototypeObj)
        return nullptr;

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
    SizedTypeRepresentation *elementTypeRepr =
        elementType->typeRepresentation()->asSized();

    // construct the type repr
    RootedObject arrayTypeReprObj(
        cx, UnsizedArrayTypeRepresentation::Create(cx, elementTypeRepr));
    if (!arrayTypeReprObj)
        return false;

    // Extract ArrayType.prototype
    RootedObject arrayTypePrototype(cx, GetPrototype(cx, arrayTypeGlobal));
    if (!arrayTypePrototype)
        return nullptr;

    // Create the instance of ArrayType
    Rooted<UnsizedArrayTypeDescr *> obj(cx);
    obj = create<UnsizedArrayTypeDescr>(cx, arrayTypePrototype,
                                        arrayTypeReprObj, elementType);
    if (!obj)
        return false;

    // Add `length` property, which is undefined for an unsized array.
    if (!JSObject::defineProperty(cx, obj, cx->names().length,
                                  UndefinedHandleValue, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return nullptr;

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
    UnsizedArrayTypeRepresentation *unsizedTypeRepr =
        unsizedTypeDescr->typeRepresentation()->asUnsizedArray();
    int32_t length = args[0].toInt32();

    // Create sized type representation.
    RootedObject sizedTypeReprObj(cx);
    sizedTypeReprObj =
        SizedArrayTypeRepresentation::Create(cx, unsizedTypeRepr->element(),
                                             length);
    if (!sizedTypeReprObj)
        return false;

    // Create the sized type object.
    Rooted<SizedTypeDescr*> elementType(cx, &unsizedTypeDescr->elementType());
    Rooted<SizedArrayTypeDescr*> obj(cx);
    obj = ArrayMetaTypeDescr::create<SizedArrayTypeDescr>(
        cx, unsizedTypeDescr, sizedTypeReprObj, elementType);
    if (!obj)
        return false;

    obj->initReservedSlot(JS_DESCR_SLOT_SIZED_ARRAY_LENGTH, Int32Value(length));

    // Add `length` property.
    RootedValue lengthVal(cx, Int32Value(length));
    if (!JSObject::defineProperty(cx, obj, cx->names().length,
                                  lengthVal, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return nullptr;

    // Add `unsized` property, which is a link from the sized
    // array to the unsized array.
    RootedValue unsizedTypeDescrValue(cx, ObjectValue(*unsizedTypeDescr));
    if (!JSObject::defineProperty(cx, obj, cx->names().unsized,
                                  unsizedTypeDescrValue, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return nullptr;

    args.rval().setObject(*obj);
    return true;
}

/*********************************
 * StructType class
 */

const Class StructTypeDescr::class_ = {
    "StructType",
    JSCLASS_HAS_RESERVED_SLOTS(JS_DESCR_SLOTS) |
    JSCLASS_HAS_PRIVATE, // used to store FieldList
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
    TypedDatum::constructSized,
    nullptr  /* trace */
};

const JSPropertySpec StructMetaTypeDescr::typeObjectProperties[] = {
    JS_PS_END
};

const JSFunctionSpec StructMetaTypeDescr::typeObjectMethods[] = {
    {"array", {nullptr, nullptr}, 1, 0, "ArrayShorthand"},
    JS_SELF_HOSTED_FN("toSource", "DescrToSourceMethod", 0, 0),
    {"equivalent", {nullptr, nullptr}, 1, 0, "TypeDescrEquivalent"},
    JS_FS_END
};

const JSPropertySpec StructMetaTypeDescr::typedObjectProperties[] = {
    JS_PS_END
};

const JSFunctionSpec StructMetaTypeDescr::typedObjectMethods[] = {
    JS_FS_END
};

/*
 * NOTE: layout() does not check for duplicates in fields since the arguments
 * to StructMetaTypeDescr are currently passed as an object literal. Fix this if it
 * changes to taking an array of arrays.
 */
bool
StructMetaTypeDescr::layout(JSContext *cx,
                            HandleStructTypeDescr structType,
                            HandleObject fields)
{
    AutoIdVector ids(cx);
    if (!GetPropertyNames(cx, fields, JSITER_OWNONLY, &ids))
        return false;

    AutoPropertyNameVector fieldNames(cx);
    AutoValueVector fieldTypeObjs(cx);
    AutoObjectVector fieldTypeReprObjs(cx);

    RootedValue fieldTypeVal(cx);
    RootedId id(cx);
    for (unsigned int i = 0; i < ids.length(); i++) {
        id = ids[i];

        // Check that all the property names are non-numeric strings.
        uint32_t unused;
        if (!JSID_IS_ATOM(id) || JSID_TO_ATOM(id)->isIndex(&unused)) {
            RootedValue idValue(cx, IdToValue(id));
            ReportCannotConvertTo(cx, idValue, "StructType field name");
            return false;
        }

        if (!fieldNames.append(JSID_TO_ATOM(id)->asPropertyName())) {
            js_ReportOutOfMemory(cx);
            return false;
        }

        if (!JSObject::getGeneric(cx, fields, fields, id, &fieldTypeVal))
            return false;

        Rooted<SizedTypeDescr*> fieldType(cx);
        fieldType = ToObjectIf<SizedTypeDescr>(fieldTypeVal);
        if (!fieldType) {
            ReportCannotConvertTo(cx, fieldTypeVal, "StructType field specifier");
            return false;
        }

        if (!fieldTypeObjs.append(fieldTypeVal)) {
            js_ReportOutOfMemory(cx);
            return false;
        }

        if (!fieldTypeReprObjs.append(&fieldType->typeRepresentationOwnerObj())) {
            js_ReportOutOfMemory(cx);
            return false;
        }
    }

    // Construct the `TypeRepresentation*`.
    RootedObject typeReprObj(cx);
    typeReprObj = StructTypeRepresentation::Create(cx, fieldNames, fieldTypeReprObjs);
    if (!typeReprObj)
        return false;
    StructTypeRepresentation *typeRepr =
        TypeRepresentation::fromOwnerObject(*typeReprObj)->asStruct();
    structType->initReservedSlot(JS_DESCR_SLOT_TYPE_REPR,
                                 ObjectValue(*typeReprObj));

    // Construct for internal use an array with names of each field
    {
        AutoValueVector fieldNameValues(cx);
        for (unsigned int i = 0; i < ids.length(); i++) {
            RootedValue value(cx, IdToValue(ids[i]));
            if (!fieldNameValues.append(value))
                return false;
        }
        RootedObject fieldNamesVec(cx);
        fieldNamesVec = NewDenseCopiedArray(cx, fieldNameValues.length(),
                                            fieldNameValues.begin(), nullptr,
                                            TenuredObject);
        if (!fieldNamesVec)
            return false;
        structType->initReservedSlot(JS_DESCR_SLOT_STRUCT_FIELD_NAMES,
                                     ObjectValue(*fieldNamesVec));
    }

    // Construct for internal use an array with the type object for each field.
    {
        RootedObject fieldTypeVec(cx);
        fieldTypeVec = NewDenseCopiedArray(cx, fieldTypeObjs.length(),
                                           fieldTypeObjs.begin(), nullptr,
                                           TenuredObject);
        if (!fieldTypeVec)
            return false;
        structType->initReservedSlot(JS_DESCR_SLOT_STRUCT_FIELD_TYPES,
                                     ObjectValue(*fieldTypeVec));
    }

    // Construct for internal use an array with the offset for each field.
    {
        AutoValueVector fieldOffsets(cx);
        for (size_t i = 0; i < typeRepr->fieldCount(); i++) {
            const StructField &field = typeRepr->field(i);
            if (!fieldOffsets.append(Int32Value(field.offset)))
                return false;
        }
        RootedObject fieldOffsetsVec(cx);
        fieldOffsetsVec = NewDenseCopiedArray(cx, fieldOffsets.length(),
                                              fieldOffsets.begin(), nullptr,
                                              TenuredObject);
        if (!fieldOffsetsVec)
            return false;
        structType->initReservedSlot(JS_DESCR_SLOT_STRUCT_FIELD_OFFSETS,
                                     ObjectValue(*fieldOffsetsVec));
    }

    // Construct the fieldOffsets and fieldTypes objects:
    // fieldOffsets : { string: integer, ... }
    // fieldTypes : { string: Type, ... }
    RootedObject fieldOffsets(cx);
    fieldOffsets = NewObjectWithProto<JSObject>(cx, nullptr, nullptr, TenuredObject);
    RootedObject fieldTypes(cx);
    fieldTypes = NewObjectWithProto<JSObject>(cx, nullptr, nullptr, TenuredObject);
    for (size_t i = 0; i < typeRepr->fieldCount(); i++) {
        const StructField &field = typeRepr->field(i);
        RootedId fieldId(cx, NameToId(field.propertyName));

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

    if (!JSObject::freeze(cx, fieldOffsets))
        return false;

    if (!JSObject::freeze(cx, fieldTypes))
        return false;

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
StructMetaTypeDescr::create(JSContext *cx,
                            HandleObject metaTypeDescr,
                            HandleObject fields)
{
    RootedObject structTypePrototype(cx, GetPrototype(cx, metaTypeDescr));
    if (!structTypePrototype)
        return nullptr;

    Rooted<StructTypeDescr*> descr(cx);
    descr = NewObjectWithProto<StructTypeDescr>(cx, structTypePrototype, nullptr,
                                                TenuredObject);
    if (!descr)
        return nullptr;

    if (!StructMetaTypeDescr::layout(cx, descr, fields))
        return nullptr;

    RootedObject fieldsProto(cx);
    if (!JSObject::getProto(cx, fields, &fieldsProto))
        return nullptr;

    RootedObject typeReprObj(cx, &descr->typeRepresentationOwnerObj());
    if (!InitializeCommonTypeDescriptorProperties(cx, descr, typeReprObj))
        return nullptr;

    RootedObject prototypeObj(
        cx, CreatePrototypeObjectForComplexTypeInstance(cx, structTypePrototype));
    if (!prototypeObj)
        return nullptr;

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

bool
StructTypeDescr::fieldIndex(jsid id, size_t *out)
{
    JSObject &fieldNames =
        getReservedSlot(JS_DESCR_SLOT_STRUCT_FIELD_NAMES).toObject();
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

size_t
StructTypeDescr::fieldOffset(size_t index)
{
    JSObject &fieldOffsets =
        getReservedSlot(JS_DESCR_SLOT_STRUCT_FIELD_OFFSETS).toObject();
    JS_ASSERT(index < fieldOffsets.getDenseInitializedLength());
    return fieldOffsets.getDenseElement(index).toInt32();
}

SizedTypeDescr&
StructTypeDescr::fieldDescr(size_t index)
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
    RootedObject funcProto(cx, global->getOrCreateFunctionPrototype(cx));
    JS_ASSERT(funcProto);

    Rooted<T*> numFun(cx, NewObjectWithProto<T>(cx, funcProto, global,
                                                TenuredObject));
    if (!numFun)
        return false;

    RootedObject typeReprObj(cx, T::TypeRepr::Create(cx, type));
    if (!typeReprObj)
        return false;

    numFun->initReservedSlot(JS_DESCR_SLOT_TYPE_REPR,
                             ObjectValue(*typeReprObj));

    if (!InitializeCommonTypeDescriptorProperties(cx, numFun, typeReprObj))
        return false;

    numFun->initReservedSlot(JS_DESCR_SLOT_TYPE, Int32Value(type));

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
    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, T::construct, className, constructorLength);
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
        return nullptr;
    JS_FOR_EACH_SCALAR_TYPE_REPR(BINARYDATA_SCALAR_DEFINE)
#undef BINARYDATA_SCALAR_DEFINE

#define BINARYDATA_REFERENCE_DEFINE(constant_, type_, name_)                    \
    if (!DefineSimpleTypeDescr<ReferenceTypeDescr>(cx, global, module, constant_,   \
                                               cx->names().name_))              \
        return nullptr;
    JS_FOR_EACH_REFERENCE_TYPE_REPR(BINARYDATA_REFERENCE_DEFINE)
#undef BINARYDATA_REFERENCE_DEFINE

    // ArrayType.

    RootedObject arrayType(cx);
    arrayType = DefineMetaTypeDescr<ArrayMetaTypeDescr>(
        cx, global, module, TypedObjectModuleObject::ArrayTypePrototype);
    if (!arrayType)
        return nullptr;

    RootedValue arrayTypeValue(cx, ObjectValue(*arrayType));
    if (!JSObject::defineProperty(cx, module, cx->names().ArrayType,
                                  arrayTypeValue,
                                  nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return nullptr;

    // StructType.

    RootedObject structType(cx);
    structType = DefineMetaTypeDescr<StructMetaTypeDescr>(
        cx, global, module, TypedObjectModuleObject::StructTypePrototype);
    if (!structType)
        return nullptr;

    RootedValue structTypeValue(cx, ObjectValue(*structType));
    if (!JSObject::defineProperty(cx, module, cx->names().StructType,
                                  structTypeValue,
                                  nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
        return nullptr;

    // Everything is setup, install module on the global object:
    RootedValue moduleValue(cx, ObjectValue(*module));
    global->setConstructor(JSProto_TypedObject, moduleValue);
    if (!JSObject::defineProperty(cx, global, cx->names().TypedObject,
                                  moduleValue,
                                  nullptr, nullptr,
                                  0))
    {
        return nullptr;
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

/*
 * Each type repr has an associated TI type object (these will
 * eventually be used as the TI type objects for type objects, though
 * they are not now). To create these TI type objects, we need to know
 * the clasp/proto of the type object for a particular kind. This
 * accessor function returns those. In the case of the predefined type
 * objects, like scalars/references/x4s, this is invoked while
 * creating the typed object module, and thus does not require that
 * the typed object module is fully initialized. For array types and
 * struct types, it is invoked when the user creates an instance of
 * those types (and hence requires that the typed object module is
 * already initialized).
 */
/*static*/ bool
TypedObjectModuleObject::getSuitableClaspAndProto(JSContext *cx,
                                                  TypeDescr::Kind kind,
                                                  const Class **clasp,
                                                  MutableHandleObject proto)
{
    Rooted<GlobalObject *> global(cx, cx->global());
    JS_ASSERT(global);
    switch (kind) {
      case TypeDescr::Scalar:
        *clasp = &ScalarTypeDescr::class_;
        proto.set(global->getOrCreateFunctionPrototype(cx));
        break;

      case TypeDescr::Reference:
        *clasp = &ReferenceTypeDescr::class_;
        proto.set(global->getOrCreateFunctionPrototype(cx));
        break;

      case TypeDescr::X4:
        *clasp = &X4TypeDescr::class_;
        proto.set(global->getOrCreateFunctionPrototype(cx));
        break;

      case TypeDescr::Struct:
        *clasp = &StructTypeDescr::class_;
        proto.set(&global->getTypedObjectModule().getSlot(StructTypePrototype).toObject());
        break;

      case TypeDescr::SizedArray:
        *clasp = &SizedArrayTypeDescr::class_;
        proto.set(&global->getTypedObjectModule().getSlot(ArrayTypePrototype).toObject());
        break;

      case TypeDescr::UnsizedArray:
        *clasp = &UnsizedArrayTypeDescr::class_;
        proto.set(&global->getTypedObjectModule().getSlot(ArrayTypePrototype).toObject());
        break;
    }

    return !!proto;
}

/******************************************************************************
 * Typed datums
 *
 * Datums represent either typed objects or handles. See comment in
 * TypedObject.h.
 */

/*static*/ TypedDatum *
TypedDatum::createUnattached(JSContext *cx,
                             HandleTypeDescr descr,
                             int32_t length)
{
    if (descr->opaque())
        return createUnattachedWithClass(cx, &OpaqueTypedObject::class_, descr, length);
    else
        return createUnattachedWithClass(cx, &TransparentTypedObject::class_, descr, length);
}


/*static*/ TypedDatum *
TypedDatum::createUnattachedWithClass(JSContext *cx,
                                      const Class *clasp,
                                      HandleTypeDescr type,
                                      int32_t length)
{
    JS_ASSERT(clasp == &TransparentTypedObject::class_ ||
              clasp == &OpaqueTypedObject::class_);
    JS_ASSERT(JSCLASS_RESERVED_SLOTS(clasp) == JS_DATUM_SLOTS);
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

    obj->setPrivate(nullptr);
    obj->initReservedSlot(JS_DATUM_SLOT_BYTEOFFSET, Int32Value(0));
    obj->initReservedSlot(JS_DATUM_SLOT_BYTELENGTH, Int32Value(0));
    obj->initReservedSlot(JS_DATUM_SLOT_OWNER, NullValue());
    obj->initReservedSlot(JS_DATUM_SLOT_NEXT_VIEW, PrivateValue(nullptr));
    obj->initReservedSlot(JS_DATUM_SLOT_NEXT_BUFFER, PrivateValue(UNSET_BUFFER_LINK));
    obj->initReservedSlot(JS_DATUM_SLOT_LENGTH, Int32Value(length));
    obj->initReservedSlot(JS_DATUM_SLOT_TYPE_DESCR, ObjectValue(*type));

    // Tag the type object for this instance with the type
    // representation, if that has not been done already.
    if (cx->typeInferenceEnabled() && !type->is<SimpleTypeDescr>()) {
        // FIXME Bug 929651           ^~~~~~~~~~~~~~~~~~~~~~~~~~~
        RootedTypeObject typeObj(cx, obj->getType(cx));
        if (typeObj) {
            if (!typeObj->addTypedObjectAddendum(cx, type))
                return nullptr;
        }
    }

    return static_cast<TypedDatum*>(&*obj);
}

void
TypedDatum::attach(ArrayBufferObject &buffer, int32_t offset)
{
    JS_ASSERT(offset >= 0);
    JS_ASSERT(offset + size() <= buffer.byteLength());

    buffer.addView(this);
    setPrivate(buffer.dataPointer() + offset);
    setReservedSlot(JS_DATUM_SLOT_BYTEOFFSET, Int32Value(offset));
    setReservedSlot(JS_DATUM_SLOT_OWNER, ObjectValue(buffer));
}

void
TypedDatum::attach(TypedDatum &datum, int32_t offset)
{
    attach(datum.owner(), datum.offset() + offset);
}

// Returns a suitable JS_DATUM_SLOT_LENGTH value for an instance of
// the type `type`. `type` must not be an unsized array.
static int32_t
DatumLengthFromType(TypeDescr &descr)
{
    TypeRepresentation *typeRepr = descr.typeRepresentation();
    switch (typeRepr->kind()) {
      case TypeDescr::Scalar:
      case TypeDescr::Reference:
      case TypeDescr::Struct:
      case TypeDescr::X4:
        return 0;

      case TypeDescr::SizedArray:
        return typeRepr->asSizedArray()->length();

      case TypeDescr::UnsizedArray:
        MOZ_ASSUME_UNREACHABLE("DatumLengthFromType() invoked on unsized type");
    }
    MOZ_ASSUME_UNREACHABLE("Invalid kind");
}

/*static*/ TypedDatum *
TypedDatum::createDerived(JSContext *cx, HandleSizedTypeDescr type,
                          HandleTypedDatum datum, size_t offset)
{
    JS_ASSERT(offset <= datum->size());
    JS_ASSERT(offset + type->size() <= datum->size());

    int32_t length = DatumLengthFromType(*type);

    const js::Class *clasp = datum->getClass();
    Rooted<TypedDatum*> obj(cx);
    obj = createUnattachedWithClass(cx, clasp, type, length);
    if (!obj)
        return nullptr;

    obj->attach(*datum, offset);
    return obj;
}

/*static*/ TypedDatum *
TypedDatum::createZeroed(JSContext *cx,
                         HandleTypeDescr descr,
                         int32_t length)
{
    // Create unattached wrapper object.
    Rooted<TypedDatum*> obj(cx, createUnattached(cx, descr, length));
    if (!obj)
        return nullptr;

    // Allocate and initialize the memory for this instance.
    // Also initialize the JS_DATUM_SLOT_LENGTH slot.
    TypeRepresentation *typeRepr = descr->typeRepresentation();
    switch (descr->kind()) {
      case TypeDescr::Scalar:
      case TypeDescr::Reference:
      case TypeDescr::Struct:
      case TypeDescr::X4:
      case TypeDescr::SizedArray:
      {
        size_t totalSize = descr->as<SizedTypeDescr>().size();
        Rooted<ArrayBufferObject*> buffer(cx);
        buffer = ArrayBufferObject::create(cx, totalSize, false);
        if (!buffer)
            return nullptr;
        typeRepr->asSized()->initInstance(cx->runtime(), buffer->dataPointer(), 1);
        obj->attach(*buffer, 0);
        return obj;
      }

      case TypeDescr::UnsizedArray:
      {
        SizedTypeRepresentation *elementTypeRepr =
            typeRepr->asUnsizedArray()->element();

        int32_t totalSize;
        if (!SafeMul(elementTypeRepr->size(), length, &totalSize)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                 JSMSG_TYPEDOBJECT_TOO_BIG);
            return nullptr;
        }

        Rooted<ArrayBufferObject*> buffer(cx);
        buffer = ArrayBufferObject::create(cx, totalSize, false);
        if (!buffer)
            return nullptr;

        if (length)
            elementTypeRepr->initInstance(cx->runtime(), buffer->dataPointer(), length);
        obj->attach(*buffer, 0);
        return obj;
      }
    }

    MOZ_ASSUME_UNREACHABLE("Bad TypeRepresentation Kind");
}

static bool
ReportDatumTypeError(JSContext *cx,
                     const unsigned errorNumber,
                     HandleTypedDatum obj)
{
    // Serialize type string using self-hosted function DescrToSource
    RootedFunction func(
        cx, SelfHostedFunction(cx, cx->names().DescrToSource));
    if (!func)
        return false;
    InvokeArgs args(cx);
    if (!args.init(1))
        return false;
    args.setCallee(ObjectValue(*func));
    args[0].setObject(obj->typeDescr());
    if (!Invoke(cx, args))
        return false;

    RootedString result(cx, args.rval().toString());
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
    gc::MarkSlot(trace, &object->getReservedSlotRef(JS_DATUM_SLOT_TYPE_DESCR),
                 "TypedObjectTypeDescr");

    ArrayBufferViewObject::trace(trace, object);

    JS_ASSERT(object->is<TypedDatum>());
    TypedDatum &datum = object->as<TypedDatum>();
    TypeRepresentation *repr = datum.typeRepresentation();
    if (repr->opaque()) {
        uint8_t *mem = datum.typedMem();
        if (!mem)
            return; // unattached handle or partially constructed

        switch (repr->kind()) {
          case TypeDescr::Scalar:
          case TypeDescr::Reference:
          case TypeDescr::Struct:
          case TypeDescr::SizedArray:
          case TypeDescr::X4:
            repr->asSized()->traceInstance(trace, mem, 1);
            break;

          case TypeDescr::UnsizedArray:
            repr->asUnsizedArray()->element()->traceInstance(trace, mem, datum.length());
            break;
        }
    }
}

bool
TypedDatum::obj_lookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                              MutableHandleObject objp, MutableHandleShape propp)
{
    JS_ASSERT(obj->is<TypedDatum>());

    Rooted<TypeDescr*> typeDescr(cx, &obj->as<TypedDatum>().typeDescr());
    TypeRepresentation *typeRepr = typeDescr->typeRepresentation();

    switch (typeRepr->kind()) {
      case TypeDescr::Scalar:
      case TypeDescr::Reference:
      case TypeDescr::X4:
        break;

      case TypeDescr::SizedArray:
      case TypeDescr::UnsizedArray:
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

      case TypeDescr::Struct:
      {
        StructTypeRepresentation *structTypeRepr = typeRepr->asStruct();
        const StructField *field = structTypeRepr->fieldNamed(id);
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
    JS_ASSERT(obj->is<TypedDatum>());
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

bool
TypedDatum::obj_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver,
                           HandleId id, MutableHandleValue vp)
{
    JS_ASSERT(obj->is<TypedDatum>());
    Rooted<TypedDatum *> datum(cx, &obj->as<TypedDatum>());

    // Dispatch elements to obj_getElement:
    uint32_t index;
    if (js_IdIsIndex(id, &index))
        return obj_getElement(cx, obj, receiver, index, vp);

    // Handle everything else here:

    TypeRepresentation *typeRepr = datum->typeRepresentation();
    switch (typeRepr->kind()) {
      case TypeDescr::Scalar:
      case TypeDescr::Reference:
        break;

      case TypeDescr::X4:
        break;

      case TypeDescr::SizedArray:
      case TypeDescr::UnsizedArray:
        if (JSID_IS_ATOM(id, cx->names().length)) {
            if (!datum->typedMem()) { // unattached
                JS_ReportErrorNumber(
                    cx, js_GetErrorMessage,
                    nullptr, JSMSG_TYPEDOBJECT_HANDLE_UNATTACHED);
                return false;
            }

            vp.setInt32(datum->length());
            return true;
        }
        break;

      case TypeDescr::Struct: {
        Rooted<StructTypeDescr*> descr(cx, &datum->typeDescr().as<StructTypeDescr>());

        size_t fieldIndex;
        if (!descr->fieldIndex(id, &fieldIndex))
            break;

        size_t offset = descr->fieldOffset(fieldIndex);
        Rooted<SizedTypeDescr*> fieldType(cx, &descr->fieldDescr(fieldIndex));
        return Reify(cx, fieldType, datum, offset, vp);
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
    JS_ASSERT(obj->is<TypedDatum>());
    Rooted<TypedDatum *> datum(cx, &obj->as<TypedDatum>());
    Rooted<TypeDescr *> descr(cx, &datum->typeDescr());

    switch (descr->kind()) {
      case TypeDescr::Scalar:
      case TypeDescr::Reference:
      case TypeDescr::X4:
      case TypeDescr::Struct:
        break;

      case TypeDescr::SizedArray:
        return obj_getArrayElement<SizedArrayTypeDescr>(cx, datum, descr,
                                                        index, vp);

      case TypeDescr::UnsizedArray:
        return obj_getArrayElement<UnsizedArrayTypeDescr>(cx, datum, descr,
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
TypedDatum::obj_getArrayElement(JSContext *cx,
                                Handle<TypedDatum*> datum,
                                Handle<TypeDescr*> typeDescr,
                                uint32_t index,
                                MutableHandleValue vp)
{
    JS_ASSERT(typeDescr->is<T>());

    if (index >= datum->length()) {
        vp.setUndefined();
        return true;
    }

    Rooted<SizedTypeDescr*> elementType(cx, &typeDescr->as<T>().elementType());
    size_t offset = elementType->size() * index;
    return Reify(cx, elementType, datum, offset, vp);
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
    JS_ASSERT(obj->is<TypedDatum>());
    Rooted<TypedDatum *> datum(cx, &obj->as<TypedDatum>());

    uint32_t index;
    if (js_IdIsIndex(id, &index))
        return obj_setElement(cx, obj, index, vp, strict);

    TypeRepresentation *typeRepr = datum->typeRepresentation();
    switch (typeRepr->kind()) {
      case ScalarTypeDescr::Scalar:
      case TypeDescr::Reference:
        break;

      case ScalarTypeDescr::X4:
        break;

      case ScalarTypeDescr::SizedArray:
      case ScalarTypeDescr::UnsizedArray:
        if (JSID_IS_ATOM(id, cx->names().length)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage,
                                 nullptr, JSMSG_CANT_REDEFINE_ARRAY_LENGTH);
            return false;
        }
        break;

      case ScalarTypeDescr::Struct: {
        Rooted<StructTypeDescr*> descr(cx, &datum->typeDescr().as<StructTypeDescr>());

        size_t fieldIndex;
        if (!descr->fieldIndex(id, &fieldIndex))
            break;

        size_t offset = descr->fieldOffset(fieldIndex);
        Rooted<SizedTypeDescr*> fieldType(cx, &descr->fieldDescr(fieldIndex));
        return ConvertAndCopyTo(cx, fieldType, datum, offset, vp);
      }
    }

    return ReportDatumTypeError(cx, JSMSG_OBJECT_NOT_EXTENSIBLE, datum);
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
    JS_ASSERT(obj->is<TypedDatum>());
    Rooted<TypedDatum *> datum(cx, &obj->as<TypedDatum>());
    Rooted<TypeDescr *> descr(cx, &datum->typeDescr());

    switch (descr->kind()) {
      case TypeDescr::Scalar:
      case TypeDescr::Reference:
      case TypeDescr::X4:
      case TypeDescr::Struct:
        break;

      case TypeDescr::SizedArray:
        return obj_setArrayElement<SizedArrayTypeDescr>(cx, datum, descr, index, vp);

      case TypeDescr::UnsizedArray:
        return obj_setArrayElement<UnsizedArrayTypeDescr>(cx, datum, descr, index, vp);
    }

    return ReportDatumTypeError(cx, JSMSG_OBJECT_NOT_EXTENSIBLE, datum);
}

template<class T>
/*static*/ bool
TypedDatum::obj_setArrayElement(JSContext *cx,
                                Handle<TypedDatum*> datum,
                                Handle<TypeDescr*> descr,
                                uint32_t index,
                                MutableHandleValue vp)
{
    JS_ASSERT(descr->is<T>());

    if (index >= datum->length()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage,
                             nullptr, JSMSG_TYPEDOBJECT_BINARYARRAY_BAD_INDEX);
        return false;
    }

    Rooted<SizedTypeDescr*> elementType(cx);
    elementType = &descr->as<T>().elementType();
    size_t offset = elementType->size() * index;
    return ConvertAndCopyTo(cx, elementType, datum, offset, vp);
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
    Rooted<TypedDatum *> datum(cx, &obj->as<TypedDatum>());
    TypeRepresentation *typeRepr = datum->typeRepresentation();

    switch (typeRepr->kind()) {
      case TypeDescr::Scalar:
      case TypeDescr::Reference:
        break;

      case TypeDescr::X4:
        break;

      case TypeDescr::SizedArray:
      case TypeDescr::UnsizedArray:
        if (js_IdIsIndex(id, &index)) {
            *attrsp = JSPROP_ENUMERATE | JSPROP_PERMANENT;
            return true;
        }
        if (JSID_IS_ATOM(id, cx->names().length)) {
            *attrsp = JSPROP_READONLY | JSPROP_PERMANENT;
            return true;
        }
        break;

      case TypeDescr::Struct:
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
    Rooted<TypedDatum *> datum(cx, &obj->as<TypedDatum>());
    TypeRepresentation *typeRepr = datum->typeRepresentation();

    switch (typeRepr->kind()) {
      case TypeDescr::Scalar:
      case TypeDescr::Reference:
      case TypeDescr::X4:
        return false;

      case TypeDescr::SizedArray:
      case TypeDescr::UnsizedArray:
        return js_IdIsIndex(id, &index) || JSID_IS_ATOM(id, cx->names().length);

      case TypeDescr::Struct:
        return typeRepr->asStruct()->fieldNamed(id) != nullptr;
    }

    return false;
}

bool
TypedDatum::obj_setGenericAttributes(JSContext *cx, HandleObject obj,
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

    JS_ASSERT(obj->is<TypedDatum>());
    Rooted<TypedDatum *> datum(cx, &obj->as<TypedDatum>());
    TypeRepresentation *typeRepr = datum->typeRepresentation();

    switch (typeRepr->kind()) {
      case TypeDescr::Scalar:
      case TypeDescr::Reference:
      case TypeDescr::X4:
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

      case TypeDescr::SizedArray:
      case TypeDescr::UnsizedArray:
        switch (enum_op) {
          case JSENUMERATE_INIT_ALL:
          case JSENUMERATE_INIT:
            statep.setInt32(0);
            idp.set(INT_TO_JSID(datum->length()));
            break;

          case JSENUMERATE_NEXT:
            index = static_cast<int32_t>(statep.toInt32());

            if (index < datum->length()) {
                idp.set(INT_TO_JSID(index));
                statep.setInt32(index + 1);
            } else {
                JS_ASSERT(index == datum->length());
                statep.setNull();
            }

            break;

          case JSENUMERATE_DESTROY:
            statep.setNull();
            break;
        }
        break;

      case TypeDescr::Struct:
        switch (enum_op) {
          case JSENUMERATE_INIT_ALL:
          case JSENUMERATE_INIT:
            statep.setInt32(0);
            idp.set(INT_TO_JSID(typeRepr->asStruct()->fieldCount()));
            break;

          case JSENUMERATE_NEXT:
            index = static_cast<uint32_t>(statep.toInt32());

            if (index < typeRepr->asStruct()->fieldCount()) {
                idp.set(NameToId(typeRepr->asStruct()->field(index).propertyName));
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
    // the offset of 7 is based on the alloc kind
    return JSObject::getPrivateDataOffset(JS_DATUM_SLOT_DATA);
}

void
TypedDatum::neuter(JSContext *cx)
{
    setSlot(JS_DATUM_SLOT_LENGTH, Int32Value(0));
    setSlot(JS_DATUM_SLOT_BYTELENGTH, Int32Value(0));
    setSlot(JS_DATUM_SLOT_BYTEOFFSET, Int32Value(0));
    setPrivate(nullptr);
}

/******************************************************************************
 * Typed Objects
 */

const Class TransparentTypedObject::class_ = {
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
    nullptr,        /* finalize    */
    nullptr,        /* call        */
    nullptr,        /* construct   */
    nullptr,        /* hasInstance */
    TypedDatum::obj_trace,
    JS_NULL_CLASS_SPEC,
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
        nullptr,   /* slice */
        TypedDatum::obj_enumerate,
        nullptr, /* thisObject */
    }
};

static int32_t
LengthForType(TypeDescr &descr)
{
    switch (descr.kind()) {
      case TypeDescr::Scalar:
      case TypeDescr::Reference:
      case TypeDescr::Struct:
      case TypeDescr::X4:
        return 0;

      case TypeDescr::SizedArray:
        return descr.as<SizedArrayTypeDescr>().length();

      case TypeDescr::UnsizedArray:
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
TypedDatum::constructSized(JSContext *cx, unsigned int argc, Value *vp)
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
        Rooted<TypedDatum*> obj(cx, createZeroed(cx, callee, length));
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    // Buffer constructor.
    if (args[0].isObject() && args[0].toObject().is<ArrayBufferObject>()) {
        Rooted<ArrayBufferObject*> buffer(cx);
        buffer = &args[0].toObject().as<ArrayBufferObject>();

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

        Rooted<TypedDatum*> obj(cx);
        obj = TypedDatum::createUnattached(cx, callee, LengthForType(*callee));
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
        Rooted<TypedDatum*> obj(cx, createZeroed(cx, callee, length));
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
TypedDatum::constructUnsized(JSContext *cx, unsigned int argc, Value *vp)
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
        Rooted<TypedDatum*> obj(cx, createZeroed(cx, callee, 0));
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    // Length constructor.
    if (args[0].isInt32()) {
        int32_t length = args[0].toInt32();
        Rooted<TypedDatum*> obj(cx, createZeroed(cx, callee, length));
        if (!obj)
            return false;
        args.rval().setObject(*obj);
        return true;
    }

    // Buffer constructor.
    if (args[0].isObject() && args[0].toObject().is<ArrayBufferObject>()) {
        Rooted<ArrayBufferObject*> buffer(cx);
        buffer = &args[0].toObject().as<ArrayBufferObject>();

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

        Rooted<TypedDatum*> obj(cx);
        obj = TypedDatum::createUnattached(cx, callee, length);
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
        Rooted<TypedDatum*> obj(cx, createZeroed(cx, callee, length));
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
    nullptr,        /* finalize    */
    nullptr,        /* call        */
    nullptr,        /* construct   */
    nullptr,        /* hasInstance */
    TypedDatum::obj_trace,
    JS_NULL_CLASS_SPEC,
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
        nullptr, // slice
        TypedDatum::obj_enumerate,
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
    int32_t length = DatumLengthFromType(*descr);
    Rooted<TypedDatum*> obj(cx);
    obj = TypedDatum::createUnattachedWithClass(cx, &OpaqueTypedObject::class_, descr, length);
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

bool
js::NewDerivedTypedDatum(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 3);
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<SizedTypeDescr>());
    JS_ASSERT(args[1].isObject() && args[1].toObject().is<TypedDatum>());
    JS_ASSERT(args[2].isInt32());

    Rooted<SizedTypeDescr*> descr(cx, &args[0].toObject().as<SizedTypeDescr>());
    Rooted<TypedDatum*> datum(cx, &args[1].toObject().as<TypedDatum>());
    int32_t offset = args[2].toInt32();

    Rooted<TypedDatum*> obj(cx);
    obj = TypedDatum::createDerived(cx, descr, datum, offset);
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

bool
js::AttachDatum(ThreadSafeContext *, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args.length() == 3);
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedDatum>());
    JS_ASSERT(args[1].isObject() && args[1].toObject().is<TypedDatum>());
    JS_ASSERT(args[2].isInt32());

    TypedDatum &handle = args[0].toObject().as<TypedDatum>();
    TypedDatum &target = args[1].toObject().as<TypedDatum>();
    JS_ASSERT(handle.typedMem() == nullptr); // must not be attached already
    size_t offset = args[2].toInt32();
    handle.attach(target, offset);
    return true;
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::AttachDatumJitInfo, AttachDatumJitInfo,
                                      js::AttachDatum);

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
js::DatumIsAttached(ThreadSafeContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedDatum>());
    TypedDatum &datum = args[0].toObject().as<TypedDatum>();
    args.rval().setBoolean(datum.typedMem() != nullptr);
    return true;
}

JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::DatumIsAttachedJitInfo,
                                      DatumIsAttachedJitInfo,
                                      js::DatumIsAttached);

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
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedDatum>());
    JS_ASSERT(args[1].isInt32());
    JS_ASSERT(args[2].isObject() && args[2].toObject().is<TypedDatum>());
    JS_ASSERT(args[3].isInt32());
    JS_ASSERT(args[4].isInt32());

    TypedDatum &targetDatum = args[0].toObject().as<TypedDatum>();
    int32_t targetOffset = args[1].toInt32();
    TypedDatum &sourceDatum = args[2].toObject().as<TypedDatum>();
    int32_t sourceOffset = args[3].toInt32();
    int32_t size = args[4].toInt32();

    JS_ASSERT(targetOffset >= 0);
    JS_ASSERT(sourceOffset >= 0);
    JS_ASSERT(size >= 0);
    JS_ASSERT((size_t) (size + targetOffset) <= targetDatum.size());
    JS_ASSERT((size_t) (size + sourceOffset) <= sourceDatum.size());

    uint8_t *target = targetDatum.typedMem(targetOffset);
    uint8_t *source = sourceDatum.typedMem(sourceOffset);
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
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedDatum>());       \
    JS_ASSERT(args[1].isInt32());                                               \
    JS_ASSERT(args[2].isNumber());                                              \
                                                                                \
    TypedDatum &datum = args[0].toObject().as<TypedDatum>();                    \
    int32_t offset = args[1].toInt32();                                         \
                                                                                \
    /* Should be guaranteed by the typed objects API: */                        \
    JS_ASSERT(offset % MOZ_ALIGNOF(T) == 0);                                    \
                                                                                \
    T *target = reinterpret_cast<T*>(datum.typedMem(offset));                   \
    double d = args[2].toNumber();                                              \
    *target = ConvertScalar<T>(d);                                              \
    args.rval().setUndefined();                                                 \
    return true;                                                                \
}                                                                               \
                                                                                \
JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::StoreScalar##T::JitInfo, StoreScalar##T, \
                                       js::StoreScalar##T::Func);

#define JS_STORE_REFERENCE_CLASS_IMPL(_constant, T, _name)                      \
bool                                                                            \
js::StoreReference##T::Func(ThreadSafeContext *, unsigned argc, Value *vp)      \
{                                                                               \
    CallArgs args = CallArgsFromVp(argc, vp);                                   \
    JS_ASSERT(args.length() == 3);                                              \
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedDatum>());       \
    JS_ASSERT(args[1].isInt32());                                               \
                                                                                \
    TypedDatum &datum = args[0].toObject().as<TypedDatum>();                    \
    int32_t offset = args[1].toInt32();                                         \
                                                                                \
    /* Should be guaranteed by the typed objects API: */                        \
    JS_ASSERT(offset % MOZ_ALIGNOF(T) == 0);                                    \
                                                                                \
    T *target = reinterpret_cast<T*>(datum.typedMem(offset));                   \
    store(target, args[2]);                                                     \
    args.rval().setUndefined();                                                 \
    return true;                                                                \
}                                                                               \
                                                                                \
 JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::StoreReference##T::JitInfo, StoreReference##T, \
                                       js::StoreReference##T::Func);

#define JS_LOAD_SCALAR_CLASS_IMPL(_constant, T, _name)                          \
bool                                                                            \
js::LoadScalar##T::Func(ThreadSafeContext *, unsigned argc, Value *vp)          \
{                                                                               \
    CallArgs args = CallArgsFromVp(argc, vp);                                   \
    JS_ASSERT(args.length() == 2);                                              \
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedDatum>());       \
    JS_ASSERT(args[1].isInt32());                                               \
                                                                                \
    TypedDatum &datum = args[0].toObject().as<TypedDatum>();                    \
    int32_t offset = args[1].toInt32();                                         \
                                                                                \
    /* Should be guaranteed by the typed objects API: */                        \
    JS_ASSERT(offset % MOZ_ALIGNOF(T) == 0);                                    \
                                                                                \
    T *target = reinterpret_cast<T*>(datum.typedMem(offset));                   \
    args.rval().setNumber((double) *target);                                    \
    return true;                                                                \
}                                                                               \
                                                                                \
JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::LoadScalar##T::JitInfo, LoadScalar##T, \
                                      js::LoadScalar##T::Func);

#define JS_LOAD_REFERENCE_CLASS_IMPL(_constant, T, _name)                       \
bool                                                                            \
js::LoadReference##T::Func(ThreadSafeContext *, unsigned argc, Value *vp)       \
{                                                                               \
    CallArgs args = CallArgsFromVp(argc, vp);                                   \
    JS_ASSERT(args.length() == 2);                                              \
    JS_ASSERT(args[0].isObject() && args[0].toObject().is<TypedDatum>());       \
    JS_ASSERT(args[1].isInt32());                                               \
                                                                                \
    TypedDatum &datum = args[0].toObject().as<TypedDatum>();                    \
    int32_t offset = args[1].toInt32();                                         \
                                                                                \
    /* Should be guaranteed by the typed objects API: */                        \
    JS_ASSERT(offset % MOZ_ALIGNOF(T) == 0);                                    \
                                                                                \
    T *target = reinterpret_cast<T*>(datum.typedMem(offset));                   \
    load(target, args.rval());                                                  \
    return true;                                                                \
}                                                                               \
                                                                                \
JS_JITINFO_NATIVE_PARALLEL_THREADSAFE(js::LoadReference##T::JitInfo, LoadReference##T, \
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
