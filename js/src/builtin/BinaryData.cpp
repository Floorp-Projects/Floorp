/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/BinaryData.h"

#include "mozilla/FloatingPoint.h"

#include "jscompartment.h"
#include "jsobj.h"

#include "jsatominlines.h"
#include "jsobjinlines.h"

#include "vm/GlobalObject.h"
#include "vm/TypedArrayObject.h"

using namespace js;

JSBool TypeThrowError(JSContext *cx, unsigned argc, Value *vp)
{
    return ReportIsNotFunction(cx, *vp);
}

JSBool DataThrowError(JSContext *cx, unsigned argc, Value *vp)
{
    return ReportIsNotFunction(cx, *vp);
}

static void
ReportTypeError(JSContext *cx, Value fromValue, const char *toType)
{
    char *valueStr = JS_EncodeString(cx, JS_ValueToString(cx, fromValue));
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_CONVERT_TO,
                         valueStr, toType);
    JS_free(cx, (void *) valueStr);
}

static void
ReportTypeError(JSContext *cx, Value fromValue, JSString *toType)
{
    const char *fnName = JS_EncodeString(cx, toType);
    ReportTypeError(cx, fromValue, fnName);
    JS_free(cx, (void *) fnName);
}

static inline bool
IsNumericType(JSObject *type)
{
    return type && &NumericTypeClasses[NUMERICTYPE_UINT8] <= type->getClass() &&
                   type->getClass() <= &NumericTypeClasses[NUMERICTYPE_FLOAT64];
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
    JS_ASSERT(0);
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
JSBool
NumericType<T>::call(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() < 1) {
        char *fnName = JS_EncodeString(cx, args.callee().as<JSFunction>().atom());
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                             fnName, "0", "s");
        JS_free(cx, (void *) fnName);
        return false;
    }

    RootedValue arg(cx, args[0]);
    T answer;
    if (!convert(cx, arg, &answer))
        return false; // convert() raises TypeError.

    // TODO Once reify is implemented (in a later patch) this will be replaced
    // by a call to reify.
    args.rval().set(NumberValue(answer));
    return true;
}

template<unsigned int N>
JSBool
NumericTypeToString(JSContext *cx, unsigned int argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(NUMERICTYPE_UINT8 <= N && N <= NUMERICTYPE_FLOAT64);
    JSString *s = JS_NewStringCopyZ(cx, NumericTypeClasses[N].name);
    args.rval().set(StringValue(s));
    return true;
}

JSBool
createArrayType(JSContext *cx, unsigned argc, Value *vp)
{
    return false;
}

JSBool
createStructType(JSContext *cx, unsigned argc, Value *vp)
{
    return false;
}

JSBool
DataInstanceUpdate(JSContext *cx, unsigned argc, Value *vp)
{
    return false;
}

JSBool
ArrayTypeObject::repeat(JSContext *cx, unsigned int argc, Value *vp)
{
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

static JSObject *
SetupComplexHeirarchy(JSContext *cx, Handle<GlobalObject *> global,
                      HandleObject complexObject)
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

    RootedObject DataProto(cx, DataProtoVal.toObjectOrNull());
    if (!DataProto)
        return NULL;

    // Set complexObject.prototype.__proto__ = Data
    RootedObject prototypeObj(cx, JS_NewObject(cx, NULL, NULL, global));
    if (!LinkConstructorAndPrototype(cx, complexObject, prototypeObj))
        return NULL;

    if (!JS_SetPrototype(cx, prototypeObj, DataObject))
        return NULL;

    // Set complexObject.prototype.prototype.__proto__ = Data.prototype
    RootedObject prototypePrototypeObj(cx, JS_NewObject(cx, NULL, NULL,
                                       global));

    if (!LinkConstructorAndPrototype(cx, prototypeObj,
                                     prototypePrototypeObj))
        return NULL;

    if (!JS_SetPrototype(cx, prototypePrototypeObj, DataProto))
        return NULL;

    return complexObject;
}

static JSObject *
InitComplexClasses(JSContext *cx, Handle<GlobalObject *> global)
{
    RootedFunction ArrayTypeFun(cx,
            JS_DefineFunction(cx, global, "ArrayType", createArrayType, 1, 0));

    if (!ArrayTypeFun)
        return NULL;

    if (!SetupComplexHeirarchy(cx, global, ArrayTypeFun))
        return NULL;

    // ArrayType.prototype.repeat
    RootedValue ArrayTypePrototypeVal(cx);
    if (!JSObject::getProperty(cx, ArrayTypeFun, ArrayTypeFun,
                               cx->names().classPrototype, &ArrayTypePrototypeVal))
        return NULL;

    if (!JS_DefineFunction(cx, ArrayTypePrototypeVal.toObjectOrNull(), "repeat",
                           ArrayTypeObject::repeat, 1, 0))
        return NULL;

    RootedFunction StructTypeFun(cx,
        JS_DefineFunction(cx, global, "StructType", createStructType, 1, 0));

    if (!StructTypeFun)
        return NULL;

    if (!SetupComplexHeirarchy(cx, global, StructTypeFun))
        return NULL;

    return global;
}

JSObject *
js_InitBinaryDataClasses(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->is<GlobalObject>());
    Rooted<GlobalObject *> global(cx, &obj->as<GlobalObject>());

    JSObject *funProto = JS_GetFunctionPrototype(cx, global);
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

    if (!InitComplexClasses(cx, global))
        return NULL;
    return global;
}
