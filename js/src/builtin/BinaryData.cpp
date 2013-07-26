/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/BinaryData.h"

#include "jscompartment.h"
#include "jsobj.h"

#include "jsatominlines.h"
#include "jsobjinlines.h"

#include "vm/GlobalObject.h"

using namespace js;

JSBool TypeThrowError(JSContext *cx, unsigned argc, Value *vp)
{
    return ReportIsNotFunction(cx, *vp);
}

JSBool DataThrowError(JSContext *cx, unsigned argc, Value *vp)
{
    return ReportIsNotFunction(cx, *vp);
}

// FIXME will actually require knowing function name
JSBool createNumericBlock(JSContext *cx, unsigned argc, jsval *vp)
{
    return false;
}

JSBool createArrayType(JSContext *cx, unsigned argc, jsval *vp)
{
    return false;
}

JSBool createStructType(JSContext *cx, unsigned argc, jsval *vp)
{
    return false;
}

JSBool DataInstanceUpdate(JSContext *cx, unsigned argc, jsval *vp)
{
    return false;
}

JSBool
ArrayTypeObject::repeat(JSContext *cx, unsigned int argc, jsval *vp)
{
    return false;
}

bool
GlobalObject::initDataObject(JSContext *cx, Handle<GlobalObject *> global)
{
    RootedObject DataProto(cx);
    DataProto = NewObjectWithGivenProto(cx, &DataClass, global->getOrCreateObjectPrototype(cx), global, SingletonObject);
    if (!DataProto)
        return false;

    RootedAtom DataName(cx, ClassName(JSProto_Data, cx));
    RootedFunction DataCtor(cx, global->createConstructor(cx, DataThrowError, DataName, 1, JSFunction::ExtendedFinalizeKind));
    if (!DataCtor)
        return false;

    if (!JS_DefineFunction(cx, DataProto, "update", DataInstanceUpdate, 1, 0))
        return false;

    if (!LinkConstructorAndPrototype(cx, DataCtor, DataProto))
        return false;

    if (!DefineConstructorAndPrototype(cx, global, JSProto_Data, DataCtor, DataProto))
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
    RootedFunction TypeCtor(cx, global->createConstructor(cx, TypeThrowError, TypeName, 1, JSFunction::ExtendedFinalizeKind));
    if (!TypeCtor)
        return false;

    if (!LinkConstructorAndPrototype(cx, TypeCtor, TypeProto))
        return false;

    if (!DefineConstructorAndPrototype(cx, global, JSProto_Type, TypeCtor, TypeProto))
        return false;

    global->setReservedSlot(JSProto_Type, ObjectValue(*TypeCtor));
    return true;
}

static JSObject *
SetupComplexHeirarchy(JSContext *cx, Handle<GlobalObject *> global, HandleObject complexObject)
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
    if (!JSObject::getProperty(cx, DataObject, DataObject, cx->names().classPrototype, &DataProtoVal))
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
    // TODO does this have to actually be a Class so we can set accessor properties etc?
    RootedObject prototypePrototypeObj(cx, JS_NewObject(cx, NULL, NULL, global));
    if (!LinkConstructorAndPrototype(cx, prototypeObj, prototypePrototypeObj))
        return NULL;

    if (!JS_SetPrototype(cx, prototypePrototypeObj, DataProto))
        return NULL;

    return complexObject;
}

static JSObject *
InitComplexClasses(JSContext *cx, Handle<GlobalObject *> global)
{
    // TODO FIXME use DefineConstructorAndPrototype and other
    // utilities
    RootedFunction ArrayTypeFun(cx, JS_DefineFunction(cx, global, "ArrayType", createArrayType, 1, 0));
    if (!ArrayTypeFun)
        return NULL;

    if (!SetupComplexHeirarchy(cx, global, ArrayTypeFun))
        return NULL;

    // ArrayType.prototype.repeat
    RootedValue ArrayTypePrototypeVal(cx);
    if (!JSObject::getProperty(cx, ArrayTypeFun, ArrayTypeFun, cx->names().classPrototype, &ArrayTypePrototypeVal))
        return NULL;

    if (!JS_DefineFunction(cx, ArrayTypePrototypeVal.toObjectOrNull(), "repeat", ArrayTypeObject::repeat, 1, 0))
        return NULL;

    RootedFunction StructTypeFun(cx, JS_DefineFunction(cx, global, "StructType", createStructType, 1, 0));
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

typedef float_t float32_t;
typedef double_t float64_t;
#define BINARYDATA_NUMERIC_DEFINE(type_)\
    do {\
        JSFunction *numFun = JS_DefineFunction(cx, obj, #type_, createNumericBlock, 1, 0);\
        if (!numFun)\
            return NULL;\
\
        if (!JS_DefineProperty(cx, numFun, "bytes", INT_TO_JSVAL(sizeof(type_##_t)), JS_PropertyStub, JS_StrictPropertyStub, 0))\
        return NULL;\
    } while(0);
    BINARYDATA_FOR_EACH_NUMERIC_TYPES(BINARYDATA_NUMERIC_DEFINE)
#undef BINARYDATA_NUMERIC_DEFINE

    if (!InitComplexClasses(cx, global))
        return NULL;
    return global;
}
