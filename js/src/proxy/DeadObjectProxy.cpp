/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "proxy/DeadObjectProxy.h"

#include "jsapi.h"
#include "jsfun.h" // XXXefaust Bug 1064662

#include "vm/ProxyObject.h"

using namespace js;
using namespace js::gc;

static void
ReportDead(JSContext *cx)
{
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_DEAD_OBJECT);
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
bool
DeadObjectProxy<CC, BF>::getOwnPropertyDescriptor(JSContext* cx, HandleObject wrapper, HandleId id,
                                                  MutableHandle<PropertyDescriptor> desc) const
{
    ReportDead(cx);
    return false;
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
bool
DeadObjectProxy<CC, BF>::defineProperty(JSContext* cx, HandleObject wrapper, HandleId id,
                                        Handle<PropertyDescriptor> desc,
                                        ObjectOpResult& result) const
{
    ReportDead(cx);
    return false;
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
bool
DeadObjectProxy<CC, BF>::ownPropertyKeys(JSContext* cx, HandleObject wrapper,
                                         AutoIdVector& props) const
{
    ReportDead(cx);
    return false;
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
bool
DeadObjectProxy<CC, BF>::delete_(JSContext* cx, HandleObject wrapper, HandleId id,
                                 ObjectOpResult& result) const
{
    ReportDead(cx);
    return false;
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
bool
DeadObjectProxy<CC, BF>::getPrototype(JSContext* cx, HandleObject proxy,
                                      MutableHandleObject protop) const
{
    protop.set(nullptr);
    return true;
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
bool
DeadObjectProxy<CC, BF>::getPrototypeIfOrdinary(JSContext* cx, HandleObject proxy, bool* isOrdinary,
                                                MutableHandleObject protop) const
{
    *isOrdinary = false;
    return true;
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
bool
DeadObjectProxy<CC, BF>::preventExtensions(JSContext* cx, HandleObject proxy,
                                           ObjectOpResult& result) const
{
    ReportDead(cx);
    return false;
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
bool
DeadObjectProxy<CC, BF>::isExtensible(JSContext* cx, HandleObject proxy, bool* extensible) const
{
    // This is kind of meaningless, but dead-object semantics aside,
    // [[Extensible]] always being true is consistent with other proxy types.
    *extensible = true;
    return true;
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
bool
DeadObjectProxy<CC, BF>::call(JSContext* cx, HandleObject wrapper, const CallArgs& args) const
{
    ReportDead(cx);
    return false;
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
bool
DeadObjectProxy<CC, BF>::construct(JSContext* cx, HandleObject wrapper, const CallArgs& args) const
{
    ReportDead(cx);
    return false;
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
bool
DeadObjectProxy<CC, BF>::nativeCall(JSContext* cx, IsAcceptableThis test, NativeImpl impl,
                                    const CallArgs& args) const
{
    ReportDead(cx);
    return false;
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
bool
DeadObjectProxy<CC, BF>::hasInstance(JSContext* cx, HandleObject proxy, MutableHandleValue v,
                                     bool* bp) const
{
    ReportDead(cx);
    return false;
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
bool
DeadObjectProxy<CC, BF>::getBuiltinClass(JSContext* cx, HandleObject proxy, ESClass* cls) const
{
    ReportDead(cx);
    return false;
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
bool
DeadObjectProxy<CC, BF>::isArray(JSContext* cx, HandleObject obj, JS::IsArrayAnswer* answer) const
{
    ReportDead(cx);
    return false;
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
const char*
DeadObjectProxy<CC, BF>::className(JSContext* cx, HandleObject wrapper) const
{
    return "DeadObject";
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
JSString*
DeadObjectProxy<CC, BF>::fun_toString(JSContext* cx, HandleObject proxy, bool isToSource) const
{
    ReportDead(cx);
    return nullptr;
}

template <DeadProxyIsCallableIsConstructorOption CC,
          DeadProxyBackgroundFinalized BF>
RegExpShared*
DeadObjectProxy<CC, BF>::regexp_toShared(JSContext* cx, HandleObject proxy) const
{
    ReportDead(cx);
    return nullptr;
}

template <>
const char DeadObjectProxy<DeadProxyNotCallableNotConstructor,
                           DeadProxyBackgroundFinalized::Yes>::family = 0;
template <>
const char DeadObjectProxy<DeadProxyNotCallableNotConstructor,
                           DeadProxyBackgroundFinalized::No>::family = 0;
template <>
const char DeadObjectProxy<DeadProxyNotCallableIsConstructor,
                           DeadProxyBackgroundFinalized::Yes>::family = 0;
template <>
const char DeadObjectProxy<DeadProxyNotCallableIsConstructor,
                           DeadProxyBackgroundFinalized::No>::family = 0;
template <>
const char DeadObjectProxy<DeadProxyIsCallableNotConstructor,
                           DeadProxyBackgroundFinalized::Yes>::family = 0;
template <>
const char DeadObjectProxy<DeadProxyIsCallableNotConstructor,
                           DeadProxyBackgroundFinalized::No>::family = 0;
template <>
const char DeadObjectProxy<DeadProxyIsCallableIsConstructor,
                           DeadProxyBackgroundFinalized::Yes>::family = 0;
template <>
const char DeadObjectProxy<DeadProxyIsCallableIsConstructor,
                           DeadProxyBackgroundFinalized::No>::family = 0;

bool
js::IsDeadProxyObject(JSObject* obj)
{
    return
        IsDerivedProxyObject(obj,
          DeadObjectProxy<DeadProxyNotCallableNotConstructor,
                          DeadProxyBackgroundFinalized::Yes>::singleton()) ||
        IsDerivedProxyObject(obj,
          DeadObjectProxy<DeadProxyNotCallableNotConstructor,
                          DeadProxyBackgroundFinalized::No>::singleton()) ||
        IsDerivedProxyObject(obj,
          DeadObjectProxy<DeadProxyIsCallableIsConstructor,
                          DeadProxyBackgroundFinalized::Yes>::singleton()) ||
        IsDerivedProxyObject(obj,
          DeadObjectProxy<DeadProxyIsCallableIsConstructor,
                          DeadProxyBackgroundFinalized::No>::singleton()) ||
        IsDerivedProxyObject(obj,
          DeadObjectProxy<DeadProxyIsCallableNotConstructor,
                          DeadProxyBackgroundFinalized::Yes>::singleton()) ||
        IsDerivedProxyObject(obj,
          DeadObjectProxy<DeadProxyIsCallableNotConstructor,
                          DeadProxyBackgroundFinalized::No>::singleton()) ||
        IsDerivedProxyObject(obj,
          DeadObjectProxy<DeadProxyNotCallableIsConstructor,
                          DeadProxyBackgroundFinalized::Yes>::singleton()) ||
        IsDerivedProxyObject(obj,
          DeadObjectProxy<DeadProxyNotCallableIsConstructor,
                          DeadProxyBackgroundFinalized::No>::singleton());
}


const BaseProxyHandler*
js::SelectDeadProxyHandler(ProxyObject* obj)
{
    // When nuking scripted proxies, isCallable and isConstructor values for
    // the proxy needs to be preserved.  So does background-finalization status.
    uint32_t callable = obj->handler()->isCallable(obj);
    uint32_t constructor = obj->handler()->isConstructor(obj);
    bool finalizeInBackground = obj->handler()->finalizeInBackground(obj->private_());

    if (callable) {
        if (constructor) {
            if (finalizeInBackground) {
                return DeadObjectProxy<DeadProxyIsCallableIsConstructor,
                                       DeadProxyBackgroundFinalized::Yes>::singleton();
            } else {
                return DeadObjectProxy<DeadProxyIsCallableIsConstructor,
                                       DeadProxyBackgroundFinalized::No>::singleton();
            }
        }

        if (finalizeInBackground) {
            return DeadObjectProxy<DeadProxyIsCallableNotConstructor,
                                   DeadProxyBackgroundFinalized::Yes>::singleton();
        }

        return DeadObjectProxy<DeadProxyIsCallableNotConstructor,
                               DeadProxyBackgroundFinalized::No>::singleton();
    }

    if (constructor) {
        if (finalizeInBackground) {
            return DeadObjectProxy<DeadProxyNotCallableIsConstructor,
                                   DeadProxyBackgroundFinalized::Yes>::singleton();
        }

        return DeadObjectProxy<DeadProxyNotCallableIsConstructor,
                               DeadProxyBackgroundFinalized::No>::singleton();
    }

    if (finalizeInBackground) {
        return DeadObjectProxy<DeadProxyNotCallableNotConstructor,
                               DeadProxyBackgroundFinalized::Yes>::singleton();
    }

    return DeadObjectProxy<DeadProxyNotCallableNotConstructor,
                           DeadProxyBackgroundFinalized::No>::singleton();
}

JSObject*
js::NewDeadProxyObject(JSContext* cx, JSObject* origObj)
{
    MOZ_ASSERT_IF(origObj, origObj->is<ProxyObject>());

    const BaseProxyHandler* handler;
    if (origObj && origObj->is<ProxyObject>())
        handler = SelectDeadProxyHandler(&origObj->as<ProxyObject>());
    else
        handler = DeadObjectProxy<DeadProxyNotCallableNotConstructor,
                                  DeadProxyBackgroundFinalized::Yes>::singleton();

    return NewProxyObject(cx, handler, NullHandleValue, nullptr, ProxyOptions());
}
