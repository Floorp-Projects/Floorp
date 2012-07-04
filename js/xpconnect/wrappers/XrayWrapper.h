/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jswrapper.h"

// Xray wrappers re-resolve the original native properties on the native
// object and always directly access to those properties.
// Because they work so differently from the rest of the wrapper hierarchy,
// we pull them out of the Wrapper inheritance hierarchy and create a
// little world around them.

class XPCWrappedNative;

namespace xpc {

JSBool
holder_get(JSContext *cx, JSHandleObject holder, JSHandleId id, jsval *vp);
JSBool
holder_set(JSContext *cx, JSHandleObject holder, JSHandleId id, JSBool strict, jsval *vp);

namespace XrayUtils {

extern JSClass HolderClass;

bool CloneExpandoChain(JSContext *cx, JSObject *src, JSObject *dst);

JSObject *createHolder(JSContext *cx, JSObject *wrappedNative, JSObject *parent);

bool
IsTransparent(JSContext *cx, JSObject *wrapper);

JSObject *
GetNativePropertiesObject(JSContext *cx, JSObject *wrapper);

}

class XPCWrappedNativeXrayTraits;
class ProxyXrayTraits;
class DOMXrayTraits;

// NB: Base *must* derive from JSProxyHandler
template <typename Base, typename Traits = XPCWrappedNativeXrayTraits >
class XrayWrapper : public Base {
  public:
    XrayWrapper(unsigned flags);
    virtual ~XrayWrapper();

    /* Fundamental proxy traps. */
    virtual bool getPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                       bool set, js::PropertyDescriptor *desc);
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JSObject *wrapper, jsid id,
                                          bool set, js::PropertyDescriptor *desc);
    virtual bool defineProperty(JSContext *cx, JSObject *wrapper, jsid id,
                                js::PropertyDescriptor *desc);
    virtual bool getOwnPropertyNames(JSContext *cx, JSObject *wrapper,
                                     js::AutoIdVector &props);
    virtual bool delete_(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual bool enumerate(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props);

    /* Derived proxy traps. */
    virtual bool get(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id,
                     js::Value *vp);
    virtual bool set(JSContext *cx, JSObject *wrapper, JSObject *receiver, jsid id,
                     bool strict, js::Value *vp);
    virtual bool has(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual bool hasOwn(JSContext *cx, JSObject *wrapper, jsid id, bool *bp);
    virtual bool keys(JSContext *cx, JSObject *wrapper, js::AutoIdVector &props);
    virtual bool iterate(JSContext *cx, JSObject *wrapper, unsigned flags, js::Value *vp);

    virtual bool call(JSContext *cx, JSObject *wrapper, unsigned argc, js::Value *vp);
    virtual bool construct(JSContext *cx, JSObject *wrapper,
                           unsigned argc, js::Value *argv, js::Value *rval);

    static XrayWrapper singleton;

  private:
    bool enumerate(JSContext *cx, JSObject *wrapper, unsigned flags,
                   JS::AutoIdVector &props);
};

typedef XrayWrapper<js::CrossCompartmentWrapper, ProxyXrayTraits > XrayProxy;
typedef XrayWrapper<js::CrossCompartmentWrapper, DOMXrayTraits > XrayDOM;

class SandboxProxyHandler : public js::IndirectWrapper {
public:
    SandboxProxyHandler() : js::IndirectWrapper(0)
    {
    }

    virtual bool getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id,
                                       bool set, js::PropertyDescriptor *desc);
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy,
                                          jsid id, bool set,
                                          js::PropertyDescriptor *desc);
};

extern SandboxProxyHandler sandboxProxyHandler;
}
