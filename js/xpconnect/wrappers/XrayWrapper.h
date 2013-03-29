/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/GuardObjects.h"

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
holder_get(JSContext *cx, JSHandleObject holder, JSHandleId id, JSMutableHandleValue vp);
JSBool
holder_set(JSContext *cx, JSHandleObject holder, JSHandleId id, JSBool strict, JSMutableHandleValue vp);

namespace XrayUtils {

extern JSClass HolderClass;

bool CloneExpandoChain(JSContext *cx, JSObject *src, JSObject *dst);

bool
IsTransparent(JSContext *cx, JSObject *wrapper, jsid id);

JSObject *
GetNativePropertiesObject(JSContext *cx, JSObject *wrapper);

bool
IsXrayResolving(JSContext *cx, JSObject *wrapper, jsid id);

}

class XrayTraits;
class XPCWrappedNativeXrayTraits;
class ProxyXrayTraits;
class DOMXrayTraits;


enum XrayType {
    XrayForDOMObject,
    XrayForWrappedNative,
    NotXray
};

XrayType GetXrayType(JSObject *obj);
XrayTraits* GetXrayTraits(JSObject *obj);

// NB: Base *must* derive from JSProxyHandler
template <typename Base, typename Traits = XPCWrappedNativeXrayTraits >
class XrayWrapper : public Base {
  public:
    XrayWrapper(unsigned flags);
    virtual ~XrayWrapper();

    /* Fundamental proxy traps. */
    virtual bool isExtensible(JSObject *wrapper) MOZ_OVERRIDE;
    virtual bool preventExtensions(JSContext *cx, JS::Handle<JSObject*> wrapper) MOZ_OVERRIDE;
    virtual bool getPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                                       js::PropertyDescriptor *desc, unsigned flags);
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                                          js::PropertyDescriptor *desc,
                                          unsigned flags);
    virtual bool defineProperty(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                                js::PropertyDescriptor *desc);
    virtual bool getOwnPropertyNames(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                     js::AutoIdVector &props);
    virtual bool delete_(JSContext *cx, JS::Handle<JSObject*> wrapper,
                         JS::Handle<jsid> id, bool *bp);
    virtual bool enumerate(JSContext *cx, JS::Handle<JSObject*> wrapper, js::AutoIdVector &props);

    /* Derived proxy traps. */
    virtual bool get(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<JSObject*> receiver,
                     JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp);
    virtual bool set(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<JSObject*> receiver,
                     JS::Handle<jsid> id, bool strict, JS::MutableHandle<JS::Value> vp);
    virtual bool has(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                     bool *bp);
    virtual bool hasOwn(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                        bool *bp);
    virtual bool keys(JSContext *cx, JS::Handle<JSObject*> wrapper,
                      js::AutoIdVector &props);
    virtual bool iterate(JSContext *cx, JS::Handle<JSObject*> wrapper, unsigned flags,
                         JS::MutableHandle<JS::Value> vp);

    virtual bool call(JSContext *cx, JS::Handle<JSObject*> wrapper, unsigned argc,
                      js::Value *vp);
    virtual bool construct(JSContext *cx, JS::Handle<JSObject*> wrapper,
                           unsigned argc, js::Value *argv,
                           JS::MutableHandle<JS::Value> rval);

    static XrayWrapper singleton;

  private:
    bool enumerate(JSContext *cx, JS::Handle<JSObject*> wrapper, unsigned flags,
                   JS::AutoIdVector &props);
};

#define PermissiveXrayXPCWN xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::XPCWrappedNativeXrayTraits>
#define SecurityXrayXPCWN xpc::XrayWrapper<js::CrossCompartmentSecurityWrapper, xpc::XPCWrappedNativeXrayTraits>
#define PermissiveXrayDOM xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::DOMXrayTraits>
#define SecurityXrayDOM xpc::XrayWrapper<js::CrossCompartmentSecurityWrapper, xpc::DOMXrayTraits>
#define SCSecurityXrayXPCWN xpc::XrayWrapper<js::SameCompartmentSecurityWrapper, xpc::XPCWrappedNativeXrayTraits>
#define SCPermissiveXrayXPCWN xpc::XrayWrapper<js::Wrapper, xpc::XPCWrappedNativeXrayTraits>
#define SCPermissiveXrayDOM xpc::XrayWrapper<js::Wrapper, xpc::DOMXrayTraits>

class SandboxProxyHandler : public js::Wrapper {
public:
    SandboxProxyHandler() : js::Wrapper(0)
    {
    }

    virtual bool getPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> proxy,
                                       JS::Handle<jsid> id, js::PropertyDescriptor *desc,
                                       unsigned flags) MOZ_OVERRIDE;
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> proxy,
                                          JS::Handle<jsid> id, js::PropertyDescriptor *desc,
                                          unsigned flags) MOZ_OVERRIDE;

    // We just forward the derived traps to the BaseProxyHandler versions which
    // implement them in terms of the fundamental traps.
    virtual bool has(JSContext *cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                     bool *bp) MOZ_OVERRIDE;
    virtual bool hasOwn(JSContext *cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                        bool *bp) MOZ_OVERRIDE;
    virtual bool get(JSContext *cx, JS::Handle<JSObject*> proxy, JS::Handle<JSObject*> receiver,
                     JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp) MOZ_OVERRIDE;
    virtual bool set(JSContext *cx, JS::Handle<JSObject*> proxy, JS::Handle<JSObject*> receiver,
                     JS::Handle<jsid> id, bool strict, JS::MutableHandle<JS::Value> vp) MOZ_OVERRIDE;
    virtual bool keys(JSContext *cx, JS::Handle<JSObject*> proxy,
                      JS::AutoIdVector &props) MOZ_OVERRIDE;
    virtual bool iterate(JSContext *cx, JS::Handle<JSObject*> proxy, unsigned flags,
                         JS::MutableHandle<JS::Value> vp) MOZ_OVERRIDE;
};

extern SandboxProxyHandler sandboxProxyHandler;

// A proxy handler that lets us wrap callables and invoke them with
// the correct this object, while forwarding all other operations down
// to them directly.
class SandboxCallableProxyHandler : public js::Wrapper {
public:
    SandboxCallableProxyHandler() : js::Wrapper(0)
    {
    }

    virtual bool call(JSContext *cx, JS::Handle<JSObject*> proxy, unsigned argc,
                      JS::Value *vp);
};

extern SandboxCallableProxyHandler sandboxCallableProxyHandler;

class AutoSetWrapperNotShadowing;
class XPCWrappedNativeXrayTraits;

class ResolvingId {
public:
    ResolvingId(JSObject *wrapper, jsid id);
    ~ResolvingId();

    bool isXrayShadowing(jsid id);
    bool isResolving(jsid id);
    static ResolvingId* getResolvingId(JSObject *holder);
    static JSObject* getHolderObject(JSObject *wrapper);
    static ResolvingId *getResolvingIdFromWrapper(JSObject *wrapper);

private:
    friend class AutoSetWrapperNotShadowing;
    friend class XPCWrappedNativeXrayTraits;

    jsid mId;
    JSObject *mHolder;
    ResolvingId *mPrev;
    bool mXrayShadowing;
};

}

