/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsproxy.h"

#include "vm/ProxyObject.h"

#include "jscntxtinlines.h"

using namespace js;

bool
BaseProxyHandler::enter(JSContext *cx, HandleObject wrapper, HandleId id, Action act,
                        bool *bp) const
{
    *bp = true;
    return true;
}

bool
BaseProxyHandler::has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const
{
    assertEnteredPolicy(cx, proxy, id, GET);
    Rooted<PropertyDescriptor> desc(cx);
    if (!getPropertyDescriptor(cx, proxy, id, &desc))
        return false;
    *bp = !!desc.object();
    return true;
}

bool
BaseProxyHandler::hasOwn(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const
{
    // Note: Proxy::set needs to invoke hasOwn to determine where the setter
    // lives, so we allow SET operations to invoke us.
    assertEnteredPolicy(cx, proxy, id, GET | SET);
    Rooted<PropertyDescriptor> desc(cx);
    if (!getOwnPropertyDescriptor(cx, proxy, id, &desc))
        return false;
    *bp = !!desc.object();
    return true;
}

bool
BaseProxyHandler::get(JSContext *cx, HandleObject proxy, HandleObject receiver,
                      HandleId id, MutableHandleValue vp) const
{
    assertEnteredPolicy(cx, proxy, id, GET);

    Rooted<PropertyDescriptor> desc(cx);
    if (!getPropertyDescriptor(cx, proxy, id, &desc))
        return false;
    if (!desc.object()) {
        vp.setUndefined();
        return true;
    }
    if (!desc.getter() ||
        (!desc.hasGetterObject() && desc.getter() == JS_PropertyStub))
    {
        vp.set(desc.value());
        return true;
    }
    if (desc.hasGetterObject())
        return InvokeGetterOrSetter(cx, receiver, ObjectValue(*desc.getterObject()),
                                    0, nullptr, vp);
    if (!desc.isShared())
        vp.set(desc.value());
    else
        vp.setUndefined();

    return CallJSPropertyOp(cx, desc.getter(), receiver, id, vp);
}

bool
BaseProxyHandler::set(JSContext *cx, HandleObject proxy, HandleObject receiver,
                      HandleId id, bool strict, MutableHandleValue vp) const
{
    assertEnteredPolicy(cx, proxy, id, SET);

    // Find an own or inherited property. The code here is strange for maximum
    // backward compatibility with earlier code written before ES6 and before
    // SetPropertyIgnoringNamedGetter.
    Rooted<PropertyDescriptor> desc(cx);
    if (!getOwnPropertyDescriptor(cx, proxy, id, &desc))
        return false;
    bool descIsOwn = desc.object() != nullptr;
    if (!descIsOwn) {
        if (!getPropertyDescriptor(cx, proxy, id, &desc))
            return false;
    }

    return SetPropertyIgnoringNamedGetter(cx, this, proxy, receiver, id, &desc, descIsOwn, strict,
                                          vp);
}

bool
js::SetPropertyIgnoringNamedGetter(JSContext *cx, const BaseProxyHandler *handler,
                                   HandleObject proxy, HandleObject receiver,
                                   HandleId id, MutableHandle<PropertyDescriptor> desc,
                                   bool descIsOwn, bool strict, MutableHandleValue vp)
{
    /* The control-flow here differs from ::get() because of the fall-through case below. */
    if (descIsOwn) {
        MOZ_ASSERT(desc.object());

        // Check for read-only properties.
        if (desc.isReadonly())
            return strict ? Throw(cx, id, JSMSG_READ_ONLY) : true;
        if (!desc.setter()) {
            // Be wary of the odd explicit undefined setter case possible through
            // Object.defineProperty.
            if (!desc.hasSetterObject())
                desc.setSetter(JS_StrictPropertyStub);
        } else if (desc.hasSetterObject() || desc.setter() != JS_StrictPropertyStub) {
            if (!CallSetter(cx, receiver, id, desc.setter(), desc.attributes(), strict, vp))
                return false;
            if (!proxy->is<ProxyObject>() || proxy->as<ProxyObject>().handler() != handler)
                return true;
            if (desc.isShared())
                return true;
        }
        if (!desc.getter()) {
            // Same as above for the null setter case.
            if (!desc.hasGetterObject())
                desc.setGetter(JS_PropertyStub);
        }
        desc.value().set(vp.get());
        return handler->defineProperty(cx, receiver, id, desc);
    }
    if (desc.object()) {
        // Check for read-only properties.
        if (desc.isReadonly())
            return strict ? Throw(cx, id, JSMSG_CANT_REDEFINE_PROP) : true;
        if (!desc.setter()) {
            // Be wary of the odd explicit undefined setter case possible through
            // Object.defineProperty.
            if (!desc.hasSetterObject())
                desc.setSetter(JS_StrictPropertyStub);
        } else if (desc.hasSetterObject() || desc.setter() != JS_StrictPropertyStub) {
            if (!CallSetter(cx, receiver, id, desc.setter(), desc.attributes(), strict, vp))
                return false;
            if (!proxy->is<ProxyObject>() || proxy->as<ProxyObject>().handler() != handler)
                return true;
            if (desc.isShared())
                return true;
        }
        if (!desc.getter()) {
            // Same as above for the null setter case.
            if (!desc.hasGetterObject())
                desc.setGetter(JS_PropertyStub);
        }
        desc.value().set(vp.get());
        return handler->defineProperty(cx, receiver, id, desc);
    }

    desc.object().set(receiver);
    desc.value().set(vp.get());
    desc.setAttributes(JSPROP_ENUMERATE);
    desc.setGetter(nullptr);
    desc.setSetter(nullptr); // Pick up the class getter/setter.
    return handler->defineProperty(cx, receiver, id, desc);
}

bool
BaseProxyHandler::getOwnEnumerablePropertyKeys(JSContext *cx, HandleObject proxy,
                                               AutoIdVector &props) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, ENUMERATE);
    MOZ_ASSERT(props.length() == 0);

    if (!ownPropertyKeys(cx, proxy, props))
        return false;

    /* Select only the enumerable properties through in-place iteration. */
    RootedId id(cx);
    size_t i = 0;
    for (size_t j = 0, len = props.length(); j < len; j++) {
        MOZ_ASSERT(i <= j);
        id = props[j];
        if (JSID_IS_SYMBOL(id))
            continue;

        AutoWaivePolicy policy(cx, proxy, id, BaseProxyHandler::GET);
        Rooted<PropertyDescriptor> desc(cx);
        if (!getOwnPropertyDescriptor(cx, proxy, id, &desc))
            return false;
        if (desc.object() && desc.isEnumerable())
            props[i++].set(id);
    }

    MOZ_ASSERT(i <= props.length());
    props.resize(i);

    return true;
}

bool
BaseProxyHandler::iterate(JSContext *cx, HandleObject proxy, unsigned flags,
                          MutableHandleValue vp) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, ENUMERATE);

    AutoIdVector props(cx);
    if ((flags & JSITER_OWNONLY)
        ? !getOwnEnumerablePropertyKeys(cx, proxy, props)
        : !enumerate(cx, proxy, props)) {
        return false;
    }

    return EnumeratedIdVectorToIterator(cx, proxy, flags, props, vp);
}

bool
BaseProxyHandler::call(JSContext *cx, HandleObject proxy, const CallArgs &args) const
{
    MOZ_CRASH("callable proxies should implement call trap");
}

bool
BaseProxyHandler::construct(JSContext *cx, HandleObject proxy, const CallArgs &args) const
{
    MOZ_CRASH("callable proxies should implement construct trap");
}

const char *
BaseProxyHandler::className(JSContext *cx, HandleObject proxy) const
{
    return proxy->isCallable() ? "Function" : "Object";
}

JSString *
BaseProxyHandler::fun_toString(JSContext *cx, HandleObject proxy, unsigned indent) const
{
    if (proxy->isCallable())
        return JS_NewStringCopyZ(cx, "function () {\n    [native code]\n}");
    RootedValue v(cx, ObjectValue(*proxy));
    ReportIsNotFunction(cx, v);
    return nullptr;
}

bool
BaseProxyHandler::regexp_toShared(JSContext *cx, HandleObject proxy,
                                  RegExpGuard *g) const
{
    MOZ_CRASH("This should have been a wrapped regexp");
}

bool
BaseProxyHandler::boxedValue_unbox(JSContext *cx, HandleObject proxy, MutableHandleValue vp) const
{
    vp.setUndefined();
    return true;
}

bool
BaseProxyHandler::defaultValue(JSContext *cx, HandleObject proxy, JSType hint,
                               MutableHandleValue vp) const
{
    return DefaultValue(cx, proxy, hint, vp);
}

bool
BaseProxyHandler::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl,
                             CallArgs args) const
{
    ReportIncompatible(cx, args);
    return false;
}

bool
BaseProxyHandler::hasInstance(JSContext *cx, HandleObject proxy, MutableHandleValue v,
                              bool *bp) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, GET);
    RootedValue val(cx, ObjectValue(*proxy.get()));
    js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS,
                        JSDVG_SEARCH_STACK, val, js::NullPtr());
    return false;
}

bool
BaseProxyHandler::objectClassIs(HandleObject proxy, ESClassValue classValue, JSContext *cx) const
{
    return false;
}

void
BaseProxyHandler::finalize(JSFreeOp *fop, JSObject *proxy) const
{
}

void
BaseProxyHandler::objectMoved(JSObject *proxy, const JSObject *old) const
{
}

JSObject *
BaseProxyHandler::weakmapKeyDelegate(JSObject *proxy) const
{
    return nullptr;
}

bool
BaseProxyHandler::getPrototypeOf(JSContext *cx, HandleObject proxy, MutableHandleObject protop) const
{
    MOZ_CRASH("Must override getPrototypeOf with lazy prototype.");
}

bool
BaseProxyHandler::setPrototypeOf(JSContext *cx, HandleObject, HandleObject, bool *) const
{
    // Disallow sets of protos on proxies with lazy protos, but no hook.
    // This keeps us away from the footgun of having the first proto set opt
    // you out of having dynamic protos altogether.
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_SETPROTOTYPEOF_FAIL,
                         "incompatible Proxy");
    return false;
}

bool
BaseProxyHandler::watch(JSContext *cx, HandleObject proxy, HandleId id, HandleObject callable) const
{
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_WATCH,
                         proxy->getClass()->name);
    return false;
}

bool
BaseProxyHandler::unwatch(JSContext *cx, HandleObject proxy, HandleId id) const
{
    return true;
}

bool
BaseProxyHandler::slice(JSContext *cx, HandleObject proxy, uint32_t begin, uint32_t end,
                        HandleObject result) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, GET);

    return js::SliceSlowly(cx, proxy, proxy, begin, end, result);
}

bool
BaseProxyHandler::isCallable(JSObject *obj) const
{
    return false;
}

bool
BaseProxyHandler::isConstructor(JSObject *obj) const
{
    return false;
}
