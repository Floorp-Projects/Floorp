/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsproxy.h"
#include "jswrapper.h" // UncheckedUnwrap

#include "vm/ProxyObject.h"

#include "jsobjinlines.h"

using namespace js;

bool
DirectProxyHandler::getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                          MutableHandle<PropertyDescriptor> desc) const
{
    assertEnteredPolicy(cx, proxy, id, GET | SET | GET_PROPERTY_DESCRIPTOR);
    MOZ_ASSERT(!hasPrototype()); // Should never be called if there's a prototype.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JS_GetPropertyDescriptorById(cx, target, id, desc);
}

bool
DirectProxyHandler::getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                             MutableHandle<PropertyDescriptor> desc) const
{
    assertEnteredPolicy(cx, proxy, id, GET | SET | GET_PROPERTY_DESCRIPTOR);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return js::GetOwnPropertyDescriptor(cx, target, id, desc);
}

bool
DirectProxyHandler::defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                                   MutableHandle<PropertyDescriptor> desc) const
{
    assertEnteredPolicy(cx, proxy, id, SET);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    RootedValue v(cx, desc.value());
    return CheckDefineProperty(cx, target, id, v, desc.attributes(), desc.getter(), desc.setter()) &&
           JS_DefinePropertyById(cx, target, id, v, desc.attributes(), desc.getter(), desc.setter());
}

bool
DirectProxyHandler::ownPropertyKeys(JSContext *cx, HandleObject proxy,
                                    AutoIdVector &props) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, ENUMERATE);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetPropertyKeys(cx, target, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, &props);
}

bool
DirectProxyHandler::delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const
{
    assertEnteredPolicy(cx, proxy, id, SET);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JSObject::deleteGeneric(cx, target, id, bp);
}

bool
DirectProxyHandler::enumerate(JSContext *cx, HandleObject proxy,
                              AutoIdVector &props) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, ENUMERATE);
    MOZ_ASSERT(!hasPrototype()); // Should never be called if there's a prototype.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetPropertyKeys(cx, target, 0, &props);
}

bool
DirectProxyHandler::call(JSContext *cx, HandleObject proxy, const CallArgs &args) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, CALL);
    RootedValue target(cx, proxy->as<ProxyObject>().private_());
    return Invoke(cx, args.thisv(), target, args.length(), args.array(), args.rval());
}

bool
DirectProxyHandler::construct(JSContext *cx, HandleObject proxy, const CallArgs &args) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, CALL);
    RootedValue target(cx, proxy->as<ProxyObject>().private_());
    return InvokeConstructor(cx, target, args.length(), args.array(), args.rval());
}

bool
DirectProxyHandler::nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl,
                               CallArgs args) const
{
    args.setThis(ObjectValue(*args.thisv().toObject().as<ProxyObject>().target()));
    if (!test(args.thisv())) {
        ReportIncompatible(cx, args);
        return false;
    }

    return CallNativeImpl(cx, impl, args);
}

bool
DirectProxyHandler::hasInstance(JSContext *cx, HandleObject proxy, MutableHandleValue v,
                                bool *bp) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, GET);
    bool b;
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    if (!HasInstance(cx, target, v, &b))
        return false;
    *bp = !!b;
    return true;
}

bool
DirectProxyHandler::getPrototypeOf(JSContext *cx, HandleObject proxy, MutableHandleObject protop) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JSObject::getProto(cx, target, protop);
}

bool
DirectProxyHandler::setPrototypeOf(JSContext *cx, HandleObject proxy, HandleObject proto, bool *bp) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JSObject::setProto(cx, target, proto, bp);
}

bool
DirectProxyHandler::objectClassIs(HandleObject proxy, ESClassValue classValue,
                                  JSContext *cx) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return ObjectClassIs(target, classValue, cx);
}

const char *
DirectProxyHandler::className(JSContext *cx, HandleObject proxy) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, GET);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JSObject::className(cx, target);
}

JSString *
DirectProxyHandler::fun_toString(JSContext *cx, HandleObject proxy,
                                 unsigned indent) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, GET);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return fun_toStringHelper(cx, target, indent);
}

bool
DirectProxyHandler::regexp_toShared(JSContext *cx, HandleObject proxy,
                                    RegExpGuard *g) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return RegExpToShared(cx, target, g);
}

bool
DirectProxyHandler::boxedValue_unbox(JSContext *cx, HandleObject proxy, MutableHandleValue vp) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return Unbox(cx, target, vp);
}

JSObject *
DirectProxyHandler::weakmapKeyDelegate(JSObject *proxy) const
{
    return UncheckedUnwrap(proxy);
}

bool
DirectProxyHandler::has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const
{
    assertEnteredPolicy(cx, proxy, id, GET);
    MOZ_ASSERT(!hasPrototype()); // Should never be called if there's a prototype.
    bool found;
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    if (!JS_HasPropertyById(cx, target, id, &found))
        return false;
    *bp = !!found;
    return true;
}

bool
DirectProxyHandler::hasOwn(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const
{
    // Note: Proxy::set needs to invoke hasOwn to determine where the setter
    // lives, so we allow SET operations to invoke us.
    assertEnteredPolicy(cx, proxy, id, GET | SET);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    Rooted<PropertyDescriptor> desc(cx);
    if (!JS_GetPropertyDescriptorById(cx, target, id, &desc))
        return false;
    *bp = (desc.object() == target);
    return true;
}

bool
DirectProxyHandler::get(JSContext *cx, HandleObject proxy, HandleObject receiver,
                        HandleId id, MutableHandleValue vp) const
{
    assertEnteredPolicy(cx, proxy, id, GET);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JSObject::getGeneric(cx, target, receiver, id, vp);
}

bool
DirectProxyHandler::set(JSContext *cx, HandleObject proxy, HandleObject receiver,
                        HandleId id, bool strict, MutableHandleValue vp) const
{
    assertEnteredPolicy(cx, proxy, id, SET);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JSObject::setGeneric(cx, target, receiver, id, vp, strict);
}

bool
DirectProxyHandler::getOwnEnumerablePropertyKeys(JSContext *cx, HandleObject proxy,
                                                 AutoIdVector &props) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, ENUMERATE);
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetPropertyKeys(cx, target, JSITER_OWNONLY, &props);
}

bool
DirectProxyHandler::iterate(JSContext *cx, HandleObject proxy, unsigned flags,
                            MutableHandleValue vp) const
{
    assertEnteredPolicy(cx, proxy, JSID_VOID, ENUMERATE);
    MOZ_ASSERT(!hasPrototype()); // Should never be called if there's a prototype.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return GetIterator(cx, target, flags, vp);
}

bool
DirectProxyHandler::isExtensible(JSContext *cx, HandleObject proxy, bool *extensible) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JSObject::isExtensible(cx, target, extensible);
}

bool
DirectProxyHandler::preventExtensions(JSContext *cx, HandleObject proxy) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    return JSObject::preventExtensions(cx, target);
}

bool
DirectProxyHandler::isCallable(JSObject *obj) const
{
    JSObject * target = obj->as<ProxyObject>().target();
    return target->isCallable();
}
