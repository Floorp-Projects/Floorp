/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * The Intl module specified by standard ECMA-402,
 * ECMAScript Internationalization API Specification.
 */

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsinterp.h"
#include "jsobj.h"

#include "builtin/Intl.h"
#include "vm/GlobalObject.h"
#include "vm/Stack.h"

#include "jsobjinlines.h"

using namespace js;

/******************** Common to Intl constructors ********************/

static bool
IntlInitialize(JSContext *cx, HandleObject obj, Handle<PropertyName*> initializer,
               HandleValue locales, HandleValue options)
{
    RootedValue initializerValue(cx);
    if (!cx->global()->getIntrinsicValue(cx, initializer, &initializerValue))
        return false;
    JS_ASSERT(initializerValue.isObject());
    JS_ASSERT(initializerValue.toObject().isFunction());

    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, 3, &args))
        return false;

    args.setCallee(initializerValue);
    args.setThis(NullValue());
    args[0] = ObjectValue(*obj);
    args[1] = locales;
    args[2] = options;

    return Invoke(cx, args);
}

/******************** Collator ********************/

static Class CollatorClass = {
    js_Object_str,
    0,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

#if JS_HAS_TOSOURCE
static JSBool
collator_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    vp->setString(cx->names().Collator);
    return true;
}
#endif

static JSFunctionSpec collator_static_methods[] = {
    {"supportedLocalesOf", JSOP_NULLWRAPPER, 1, JSFunction::INTERPRETED, "Intl_Collator_supportedLocalesOf"},
    JS_FS_END
};

static JSFunctionSpec collator_methods[] = {
    {"resolvedOptions", JSOP_NULLWRAPPER, 0, JSFunction::INTERPRETED, "Intl_Collator_resolvedOptions"},
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, collator_toSource, 0, 0),
#endif
    JS_FS_END
};

/**
 * Collator constructor.
 * Spec: ECMAScript Internationalization API Specification, 10.1
 */
static JSBool
Collator(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject obj(cx);

    bool construct = IsConstructing(args);
    if (!construct) {
        // 10.1.2.1 step 3
        JSObject *intl = cx->global()->getOrCreateIntlObject(cx);
        if (!intl)
            return false;
        RootedValue self(cx, args.thisv());
        if (!self.isUndefined() && (!self.isObject() || self.toObject() != *intl)) {
            // 10.1.2.1 step 4
            obj = ToObject(cx, self);
            if (!obj)
                return false;
            // 10.1.2.1 step 5
            if (!obj->isExtensible())
                return Throw(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE);
        } else {
            // 10.1.2.1 step 3.a
            construct = true;
        }
    }
    if (construct) {
        // 10.1.3.1 paragraph 2
        RootedObject proto(cx, cx->global()->getOrCreateCollatorPrototype(cx));
        if (!proto)
            return false;
        obj = NewObjectWithGivenProto(cx, &CollatorClass, proto, cx->global());
        if (!obj)
            return false;
    }

    // 10.1.2.1 steps 1 and 2; 10.1.3.1 steps 1 and 2
    RootedValue locales(cx, args.length() > 0 ? args[0] : UndefinedValue());
    RootedValue options(cx, args.length() > 1 ? args[1] : UndefinedValue());
    // 10.1.2.1 step 6; 10.1.3.1 step 3
    if (!IntlInitialize(cx, obj, cx->names().InitializeCollator, locales, options))
        return false;

    // 10.1.2.1 steps 3.a and 7
    args.rval().setObject(*obj);
    return true;
}

static JSObject *
InitCollatorClass(JSContext *cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx, global->createConstructor(cx, &Collator, cx->names().Collator, 0));
    if (!ctor)
        return NULL;

    RootedObject proto(cx, global->asGlobal().getOrCreateCollatorPrototype(cx));
    if (!proto)
        return NULL;
    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return NULL;

    // 10.2.2
    if (!JS_DefineFunctions(cx, ctor, collator_static_methods))
        return NULL;

    // 10.3.2 and 10.3.3
    if (!JS_DefineFunctions(cx, proto, collator_methods))
        return NULL;

    /*
     * Install the getter for Collator.prototype.compare, which returns a bound
     * comparison function for the specified Collator object (suitable for
     * passing to methods like Array.prototype.sort).
     */
    RootedValue getter(cx);
    if (!cx->global()->getIntrinsicValue(cx, cx->names().CollatorCompareGet, &getter))
        return NULL;
    RootedValue undefinedValue(cx, UndefinedValue());
    if (!JSObject::defineProperty(cx, proto, cx->names().compare, undefinedValue,
                                  JS_DATA_TO_FUNC_PTR(JSPropertyOp, &getter.toObject()),
                                  NULL, JSPROP_GETTER)) {
        return NULL;
    }

    // 10.2.1 and 10.3
    RootedValue locales(cx, UndefinedValue());
    RootedValue options(cx, UndefinedValue());
    if (!IntlInitialize(cx, proto, cx->names().InitializeCollator, locales, options))
        return NULL;

    // 8.1
    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!JSObject::defineProperty(cx, Intl, cx->names().Collator, ctorValue,
                                  JS_PropertyStub, JS_StrictPropertyStub, 0)) {
        return NULL;
    }

    return ctor;
}

bool
GlobalObject::initCollatorProto(JSContext *cx, Handle<GlobalObject*> global)
{
    RootedObject proto(cx, global->createBlankPrototype(cx, &CollatorClass));
    if (!proto)
        return false;
    global->setReservedSlot(COLLATOR_PROTO, ObjectValue(*proto));
    return true;
}


/******************** NumberFormat ********************/

static Class NumberFormatClass = {
    js_Object_str,
    0,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

#if JS_HAS_TOSOURCE
static JSBool
numberFormat_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    vp->setString(cx->names().NumberFormat);
    return true;
}
#endif

static JSFunctionSpec numberFormat_static_methods[] = {
    {"supportedLocalesOf", JSOP_NULLWRAPPER, 1, JSFunction::INTERPRETED, "Intl_NumberFormat_supportedLocalesOf"},
    JS_FS_END
};

static JSFunctionSpec numberFormat_methods[] = {
    {"resolvedOptions", JSOP_NULLWRAPPER, 0, JSFunction::INTERPRETED, "Intl_NumberFormat_resolvedOptions"},
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, numberFormat_toSource, 0, 0),
#endif
    JS_FS_END
};

/**
 * NumberFormat constructor.
 * Spec: ECMAScript Internationalization API Specification, 11.1
 */
static JSBool
NumberFormat(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject obj(cx);

    bool construct = IsConstructing(args);
    if (!construct) {
        // 11.1.2.1 step 3
        JSObject *intl = cx->global()->getOrCreateIntlObject(cx);
        if (!intl)
            return false;
        RootedValue self(cx, args.thisv());
        if (!self.isUndefined() && (!self.isObject() || self.toObject() != *intl)) {
            // 11.1.2.1 step 4
            obj = ToObject(cx, self);
            if (!obj)
                return false;
            // 11.1.2.1 step 5
            if (!obj->isExtensible())
                return Throw(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE);
        } else {
            // 11.1.2.1 step 3.a
            construct = true;
        }
    }
    if (construct) {
        // 11.1.3.1 paragraph 2
        RootedObject proto(cx, cx->global()->getOrCreateNumberFormatPrototype(cx));
        if (!proto)
            return false;
        obj = NewObjectWithGivenProto(cx, &NumberFormatClass, proto, cx->global());
        if (!obj)
            return false;
    }

    // 11.1.2.1 steps 1 and 2; 11.1.3.1 steps 1 and 2
    RootedValue locales(cx, args.length() > 0 ? args[0] : UndefinedValue());
    RootedValue options(cx, args.length() > 1 ? args[1] : UndefinedValue());
    // 11.1.2.1 step 6; 11.1.3.1 step 3
    if (!IntlInitialize(cx, obj, cx->names().InitializeNumberFormat, locales, options))
        return false;

    // 11.1.2.1 steps 3.a and 7
    args.rval().setObject(*obj);
    return true;
}

static JSObject *
InitNumberFormatClass(JSContext *cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx, global->createConstructor(cx, &NumberFormat, cx->names().NumberFormat, 0));
    if (!ctor)
        return NULL;

    RootedObject proto(cx, global->asGlobal().getOrCreateNumberFormatPrototype(cx));
    if (!proto)
        return NULL;
    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return NULL;

    // 11.2.2
    if (!JS_DefineFunctions(cx, ctor, numberFormat_static_methods))
        return NULL;

    // 11.3.2 and 11.3.3
    if (!JS_DefineFunctions(cx, proto, numberFormat_methods))
        return NULL;

    /*
     * Install the getter for NumberFormat.prototype.format, which returns a
     * bound formatting function for the specified NumberFormat object (suitable
     * for passing to methods like Array.prototype.map).
     */
    RootedValue getter(cx);
    if (!cx->global()->getIntrinsicValue(cx, cx->names().NumberFormatFormatGet, &getter))
        return NULL;
    RootedValue undefinedValue(cx, UndefinedValue());
    if (!JSObject::defineProperty(cx, proto, cx->names().format, undefinedValue,
                                  JS_DATA_TO_FUNC_PTR(JSPropertyOp, &getter.toObject()),
                                  NULL, JSPROP_GETTER)) {
        return NULL;
    }

    // 11.2.1 and 11.3
    RootedValue locales(cx, UndefinedValue());
    RootedValue options(cx, UndefinedValue());
    if (!IntlInitialize(cx, proto, cx->names().InitializeNumberFormat, locales, options))
        return NULL;

    // 8.1
    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!JSObject::defineProperty(cx, Intl, cx->names().NumberFormat, ctorValue,
                                  JS_PropertyStub, JS_StrictPropertyStub, 0)) {
        return NULL;
    }

    return ctor;
}

bool
GlobalObject::initNumberFormatProto(JSContext *cx, Handle<GlobalObject*> global)
{
    RootedObject proto(cx, global->createBlankPrototype(cx, &NumberFormatClass));
    if (!proto)
        return false;
    global->setReservedSlot(NUMBER_FORMAT_PROTO, ObjectValue(*proto));
    return true;
}


/******************** DateTimeFormat ********************/

static Class DateTimeFormatClass = {
    js_Object_str,
    0,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

#if JS_HAS_TOSOURCE
static JSBool
dateTimeFormat_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    vp->setString(cx->names().DateTimeFormat);
    return true;
}
#endif

static JSFunctionSpec dateTimeFormat_static_methods[] = {
    {"supportedLocalesOf", JSOP_NULLWRAPPER, 1, JSFunction::INTERPRETED, "Intl_DateTimeFormat_supportedLocalesOf"},
    JS_FS_END
};

static JSFunctionSpec dateTimeFormat_methods[] = {
    {"resolvedOptions", JSOP_NULLWRAPPER, 0, JSFunction::INTERPRETED, "Intl_DateTimeFormat_resolvedOptions"},
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, dateTimeFormat_toSource, 0, 0),
#endif
    JS_FS_END
};

/**
 * DateTimeFormat constructor.
 * Spec: ECMAScript Internationalization API Specification, 12.1
 */
static JSBool
DateTimeFormat(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject obj(cx);

    bool construct = IsConstructing(args);
    if (!construct) {
        // 12.1.2.1 step 3
        JSObject *intl = cx->global()->getOrCreateIntlObject(cx);
        if (!intl)
            return false;
        RootedValue self(cx, args.thisv());
        if (!self.isUndefined() && (!self.isObject() || self.toObject() != *intl)) {
            // 12.1.2.1 step 4
            obj = ToObject(cx, self);
            if (!obj)
                return false;
            // 12.1.2.1 step 5
            if (!obj->isExtensible())
                return Throw(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE);
        } else {
            // 12.1.2.1 step 3.a
            construct = true;
        }
    }
    if (construct) {
        // 12.1.3.1 paragraph 2
        RootedObject proto(cx, cx->global()->getOrCreateDateTimeFormatPrototype(cx));
        if (!proto)
            return false;
        obj = NewObjectWithGivenProto(cx, &DateTimeFormatClass, proto, cx->global());
        if (!obj)
            return false;
    }

    // 12.1.2.1 steps 1 and 2; 12.1.3.1 steps 1 and 2
    RootedValue locales(cx, args.length() > 0 ? args[0] : UndefinedValue());
    RootedValue options(cx, args.length() > 1 ? args[1] : UndefinedValue());
    // 12.1.2.1 step 6; 12.1.3.1 step 3
    if (!IntlInitialize(cx, obj, cx->names().InitializeDateTimeFormat, locales, options))
        return false;

    // 12.1.2.1 steps 3.a and 7
    args.rval().setObject(*obj);
    return true;
}

static JSObject *
InitDateTimeFormatClass(JSContext *cx, HandleObject Intl, Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx, global->createConstructor(cx, &DateTimeFormat, cx->names().DateTimeFormat, 0));
    if (!ctor)
        return NULL;

    RootedObject proto(cx, global->asGlobal().getOrCreateDateTimeFormatPrototype(cx));
    if (!proto)
        return NULL;
    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return NULL;

    // 12.2.2
    if (!JS_DefineFunctions(cx, ctor, dateTimeFormat_static_methods))
        return NULL;

    // 12.3.2 and 12.3.3
    if (!JS_DefineFunctions(cx, proto, dateTimeFormat_methods))
        return NULL;

    /*
     * Install the getter for DateTimeFormat.prototype.format, which returns a
     * bound formatting function for the specified DateTimeFormat object
     * (suitable for passing to methods like Array.prototype.map).
     */
    RootedValue getter(cx);
    if (!cx->global()->getIntrinsicValue(cx, cx->names().DateTimeFormatFormatGet, &getter))
        return NULL;
    RootedValue undefinedValue(cx, UndefinedValue());
    if (!JSObject::defineProperty(cx, proto, cx->names().format, undefinedValue,
                                  JS_DATA_TO_FUNC_PTR(JSPropertyOp, &getter.toObject()),
                                  NULL, JSPROP_GETTER)) {
        return NULL;
    }

    // 12.2.1 and 12.3
    RootedValue locales(cx, UndefinedValue());
    RootedValue options(cx, UndefinedValue());
    if (!IntlInitialize(cx, proto, cx->names().InitializeDateTimeFormat, locales, options))
        return NULL;

    // 8.1
    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!JSObject::defineProperty(cx, Intl, cx->names().DateTimeFormat, ctorValue,
                                  JS_PropertyStub, JS_StrictPropertyStub, 0)) {
        return NULL;
    }

    return ctor;
}

bool
GlobalObject::initDateTimeFormatProto(JSContext *cx, Handle<GlobalObject*> global)
{
    RootedObject proto(cx, global->createBlankPrototype(cx, &DateTimeFormatClass));
    if (!proto)
        return false;
    global->setReservedSlot(DATE_TIME_FORMAT_PROTO, ObjectValue(*proto));
    return true;
}


/******************** Intl ********************/

Class js::IntlClass = {
    js_Object_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_Intl),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

#if JS_HAS_TOSOURCE
static JSBool
intl_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    vp->setString(cx->names().Intl);
    return true;
}
#endif

static JSFunctionSpec intl_static_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,  intl_toSource,        0, 0),
#endif
    JS_FS_END
};

/**
 * Initializes the Intl Object and its standard built-in properties.
 * Spec: ECMAScript Internationalization API Specification, 8.0, 8.1
 */
JSObject *
js_InitIntlClass(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->isGlobal());
    Rooted<GlobalObject*> global(cx, &obj->asGlobal());

    // The constructors above need to be able to determine whether they've been
    // called with this being "the standard built-in Intl object". The global
    // object reserves slots to track standard built-in objects, but doesn't
    // normally keep references to non-constructors. This makes sure there is one.
    RootedObject Intl(cx, global->getOrCreateIntlObject(cx));
    if (!Intl)
        return NULL;

    RootedValue IntlValue(cx, ObjectValue(*Intl));
    if (!JSObject::defineProperty(cx, global, cx->names().Intl, IntlValue,
                                  JS_PropertyStub, JS_StrictPropertyStub, 0)) {
        return NULL;
    }

    if (!JS_DefineFunctions(cx, Intl, intl_static_methods))
        return NULL;

    // Skip initialization of the Intl constructors during initialization of the
    // self-hosting global as we may get here before self-hosted code is compiled,
    // and no core code refers to the Intl classes.
    if (!cx->runtime->isSelfHostingGlobal(cx->global())) {
        if (!InitCollatorClass(cx, Intl, global))
            return NULL;
        if (!InitNumberFormatClass(cx, Intl, global))
            return NULL;
        if (!InitDateTimeFormatClass(cx, Intl, global))
            return NULL;
    }

    MarkStandardClassInitializedNoProto(global, &IntlClass);

    return Intl;
}

bool
GlobalObject::initIntlObject(JSContext *cx, Handle<GlobalObject*> global)
{
    RootedObject Intl(cx);
    Intl = NewObjectWithGivenProto(cx, &IntlClass, global->getOrCreateObjectPrototype(cx),
                                   global, SingletonObject);
    if (!Intl)
        return false;

    global->setReservedSlot(JSProto_Intl, ObjectValue(*Intl));
    return true;
}
