/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef proxy_ScriptedIndirectProxyHandler_h
#define proxy_ScriptedIndirectProxyHandler_h

#include "jsproxy.h"

namespace js {

/* Derived class for all scripted indirect proxy handlers. */
class ScriptedIndirectProxyHandler : public BaseProxyHandler
{
  public:
    MOZ_CONSTEXPR ScriptedIndirectProxyHandler()
      : BaseProxyHandler(&family)
    { }

    /* ES5 Harmony fundamental proxy traps. */
    virtual bool preventExtensions(JSContext *cx, HandleObject proxy) const MOZ_OVERRIDE;
    virtual bool getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                       MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                          MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                                MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool ownPropertyKeys(JSContext *cx, HandleObject proxy,
                                 AutoIdVector &props) const MOZ_OVERRIDE;
    virtual bool delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const MOZ_OVERRIDE;
    virtual bool enumerate(JSContext *cx, HandleObject proxy, AutoIdVector &props) const MOZ_OVERRIDE;

    /* ES5 Harmony derived proxy traps. */
    virtual bool has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const MOZ_OVERRIDE;
    virtual bool hasOwn(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const MOZ_OVERRIDE;
    virtual bool get(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id,
                     MutableHandleValue vp) const MOZ_OVERRIDE;
    virtual bool set(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id,
                     bool strict, MutableHandleValue vp) const MOZ_OVERRIDE;
    virtual bool keys(JSContext *cx, HandleObject proxy, AutoIdVector &props) const MOZ_OVERRIDE;
    virtual bool iterate(JSContext *cx, HandleObject proxy, unsigned flags,
                         MutableHandleValue vp) const MOZ_OVERRIDE;

    /* Spidermonkey extensions. */
    virtual bool isExtensible(JSContext *cx, HandleObject proxy, bool *extensible) const MOZ_OVERRIDE;
    virtual bool nativeCall(JSContext *cx, IsAcceptableThis test, NativeImpl impl,
                            CallArgs args) const MOZ_OVERRIDE;
    virtual JSString *fun_toString(JSContext *cx, HandleObject proxy, unsigned indent) const MOZ_OVERRIDE;
    virtual bool isScripted() const MOZ_OVERRIDE { return true; }

    static const char family;
    static const ScriptedIndirectProxyHandler singleton;
};

/* Derived class to handle Proxy.createFunction() */
class CallableScriptedIndirectProxyHandler : public ScriptedIndirectProxyHandler
{
  public:
    CallableScriptedIndirectProxyHandler() : ScriptedIndirectProxyHandler() { }
    virtual bool call(JSContext *cx, HandleObject proxy, const CallArgs &args) const MOZ_OVERRIDE;
    virtual bool construct(JSContext *cx, HandleObject proxy, const CallArgs &args) const MOZ_OVERRIDE;

    virtual bool isCallable(JSObject *obj) const MOZ_OVERRIDE {
        return true;
    }
    virtual bool isConstructor(JSObject *obj) const MOZ_OVERRIDE {
        return true;
    }

    static const CallableScriptedIndirectProxyHandler singleton;
};

bool
proxy_create(JSContext *cx, unsigned argc, Value *vp);

bool
proxy_createFunction(JSContext *cx, unsigned argc, Value *vp);

} /* namespace js */

#endif /* proxy_ScriptedIndirectProxyHandler_h */
