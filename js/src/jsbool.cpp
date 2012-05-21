/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS boolean implementation.
 */

#include "mozilla/FloatingPoint.h"

#include "jstypes.h"
#include "jsutil.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsinfer.h"
#include "jsversion.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsstr.h"

#include "vm/GlobalObject.h"
#include "vm/StringBuffer.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "vm/BooleanObject-inl.h"
#include "vm/MethodGuard-inl.h"

using namespace js;
using namespace js::types;

Class js::BooleanClass = {
    "Boolean",
    JSCLASS_HAS_RESERVED_SLOTS(1) | JSCLASS_HAS_CACHED_PROTO(JSProto_Boolean),    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

#if JS_HAS_TOSOURCE
static JSBool
bool_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    bool b, ok;
    if (!BoxedPrimitiveMethodGuard(cx, args, bool_toSource, &b, &ok))
        return ok;

    StringBuffer sb(cx);
    if (!sb.append("(new Boolean(") || !BooleanToStringBuffer(cx, b, sb) || !sb.append("))"))
        return false;

    JSString *str = sb.finishString();
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}
#endif

static JSBool
bool_toString(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    bool b, ok;
    if (!BoxedPrimitiveMethodGuard<bool>(cx, args, bool_toString, &b, &ok))
        return ok;

    args.rval().setString(cx->runtime->atomState.booleanAtoms[b ? 1 : 0]);
    return true;
}

static JSBool
bool_valueOf(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    bool b, ok;
    if (!BoxedPrimitiveMethodGuard(cx, args, bool_valueOf, &b, &ok))
        return ok;

    args.rval().setBoolean(b);
    return true;
}

static JSFunctionSpec boolean_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,  bool_toSource,  0, 0),
#endif
    JS_FN(js_toString_str,  bool_toString,  0, 0),
    JS_FN(js_valueOf_str,   bool_valueOf,   0, 0),
    JS_FS_END
};

static JSBool
Boolean(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    bool b = args.length() != 0 ? js_ValueToBoolean(args[0]) : false;

    if (IsConstructing(vp)) {
        JSObject *obj = BooleanObject::create(cx, b);
        if (!obj)
            return false;
        args.rval().setObject(*obj);
    } else {
        args.rval().setBoolean(b);
    }
    return true;
}

JSObject *
js_InitBooleanClass(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isNative());

    RootedVar<GlobalObject*> global(cx, &obj->asGlobal());

    RootedVarObject booleanProto (cx, global->createBlankPrototype(cx, &BooleanClass));
    if (!booleanProto)
        return NULL;
    booleanProto->setFixedSlot(BooleanObject::PRIMITIVE_VALUE_SLOT, BooleanValue(false));

    RootedVarFunction ctor(cx);
    ctor = global->createConstructor(cx, Boolean, CLASS_NAME(cx, Boolean), 1);
    if (!ctor)
        return NULL;

    if (!LinkConstructorAndPrototype(cx, ctor, booleanProto))
        return NULL;

    if (!DefinePropertiesAndBrand(cx, booleanProto, NULL, boolean_methods))
        return NULL;

    if (!DefineConstructorAndPrototype(cx, global, JSProto_Boolean, ctor, booleanProto))
        return NULL;

    return booleanProto;
}

JSString *
js_BooleanToString(JSContext *cx, JSBool b)
{
    return cx->runtime->atomState.booleanAtoms[b ? 1 : 0];
}

namespace js {

bool
BooleanGetPrimitiveValueSlow(JSContext *cx, JSObject &obj, Value *vp)
{
    JS_ASSERT(ObjectClassIs(obj, ESClass_Boolean, cx));
    JS_ASSERT(obj.isProxy());

    /*
     * To respect the proxy abstraction, we can't simply unwrap and call
     * getPrimitiveThis on the wrapped object. All we know is that obj says
     * its [[Class]] is "Boolean". Boolean.prototype.valueOf is specified to
     * return the [[PrimitiveValue]] internal property, so call that instead.
     */
    InvokeArgsGuard ag;
    if (!cx->stack.pushInvokeArgs(cx, 0, &ag))
        return false;
    ag.calleev().setUndefined();
    ag.thisv().setObject(obj);
    if (!GetProxyHandler(&obj)->nativeCall(cx, &obj, &BooleanClass, bool_valueOf, ag))
        return false;
    *vp = ag.rval();
    return true;
}

}  /* namespace js */

JSBool
js_ValueToBoolean(const Value &v)
{
    if (v.isInt32())
        return v.toInt32() != 0;
    if (v.isString())
        return v.toString()->length() != 0;
    if (v.isObject())
        return JS_TRUE;
    if (v.isNullOrUndefined())
        return JS_FALSE;
    if (v.isDouble()) {
        double d;

        d = v.toDouble();
        return !MOZ_DOUBLE_IS_NaN(d) && d != 0;
    }
    JS_ASSERT(v.isBoolean());
    return v.toBoolean();
}
