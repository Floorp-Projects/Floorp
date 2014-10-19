/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef proxy_ScriptedDirectProxyHandler_h
#define proxy_ScriptedDirectProxyHandler_h

#include "jsproxy.h"

namespace js {

/* Derived class for all scripted direct proxy handlers. */
class ScriptedDirectProxyHandler : public DirectProxyHandler {
  public:
    MOZ_CONSTEXPR ScriptedDirectProxyHandler()
      : DirectProxyHandler(&family)
    { }

    /* Standard internal methods. */
    virtual bool getOwnPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                          MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool defineProperty(JSContext *cx, HandleObject proxy, HandleId id,
                                MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool ownPropertyKeys(JSContext *cx, HandleObject proxy,
                                 AutoIdVector &props) const MOZ_OVERRIDE;
    virtual bool delete_(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const MOZ_OVERRIDE;
    virtual bool enumerate(JSContext *cx, HandleObject proxy, AutoIdVector &props) const MOZ_OVERRIDE;
    virtual bool isExtensible(JSContext *cx, HandleObject proxy, bool *extensible) const MOZ_OVERRIDE;
    virtual bool preventExtensions(JSContext *cx, HandleObject proxy, bool *succeeded) const MOZ_OVERRIDE;

    /* These two are standard internal methods but aren't implemented to spec yet. */
    virtual bool getPrototypeOf(JSContext *cx, HandleObject proxy,
                                MutableHandleObject protop) const MOZ_OVERRIDE;
    virtual bool setPrototypeOf(JSContext *cx, HandleObject proxy, HandleObject proto,
                                bool *bp) const MOZ_OVERRIDE;
    /* Non-standard, but needed to handle revoked proxies. */
    virtual bool setImmutablePrototype(JSContext *cx, HandleObject proxy,
                                       bool *succeeded) const MOZ_OVERRIDE;

    virtual bool has(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const MOZ_OVERRIDE;
    virtual bool get(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id,
                     MutableHandleValue vp) const MOZ_OVERRIDE;
    virtual bool set(JSContext *cx, HandleObject proxy, HandleObject receiver, HandleId id,
                     bool strict, MutableHandleValue vp) const MOZ_OVERRIDE;
    virtual bool call(JSContext *cx, HandleObject proxy, const CallArgs &args) const MOZ_OVERRIDE;
    virtual bool construct(JSContext *cx, HandleObject proxy, const CallArgs &args) const MOZ_OVERRIDE;

    /* SpiderMonkey extensions. */
    virtual bool getPropertyDescriptor(JSContext *cx, HandleObject proxy, HandleId id,
                                       MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool hasOwn(JSContext *cx, HandleObject proxy, HandleId id, bool *bp) const MOZ_OVERRIDE {
        return BaseProxyHandler::hasOwn(cx, proxy, id, bp);
    }


    // Kick getOwnEnumerablePropertyKeys out to ownPropertyKeys and then
    // filter. [[GetOwnProperty]] could potentially change the enumerability of
    // the target's properties.
    virtual bool getOwnEnumerablePropertyKeys(JSContext *cx, HandleObject proxy,
                                              AutoIdVector &props) const MOZ_OVERRIDE {
        return BaseProxyHandler::getOwnEnumerablePropertyKeys(cx, proxy, props);
    }
    virtual bool iterate(JSContext *cx, HandleObject proxy, unsigned flags,
                         MutableHandleValue vp) const MOZ_OVERRIDE;

    virtual bool isCallable(JSObject *obj) const MOZ_OVERRIDE;
    virtual bool isConstructor(JSObject *obj) const MOZ_OVERRIDE {
        // For now we maintain the broken behavior that a scripted proxy is constructable if it's
        // callable. See bug 929467.
        return isCallable(obj);
    }
    virtual bool isScripted() const MOZ_OVERRIDE { return true; }

    static const char family;
    static const ScriptedDirectProxyHandler singleton;

    // The "proxy extra" slot index in which the handler is stored. Revocable proxies need to set
    // this at revocation time.
    static const int HANDLER_EXTRA = 0;
    static const int IS_CALLABLE_EXTRA = 1;
    // The "function extended" slot index in which the revocation object is stored. Per spec, this
    // is to be cleared during the first revocation.
    static const int REVOKE_SLOT = 0;
};

bool
proxy(JSContext *cx, unsigned argc, jsval *vp);

bool
proxy_revocable(JSContext *cx, unsigned argc, jsval *vp);

} /* namespace js */

#endif /* proxy_ScriptedDirectProxyHandler_h */
