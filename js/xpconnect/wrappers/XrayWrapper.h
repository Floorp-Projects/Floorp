/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XrayWrapper_h
#define XrayWrapper_h

#include "mozilla/Attributes.h"

#include "jswrapper.h"

// Xray wrappers re-resolve the original native properties on the native
// object and always directly access to those properties.
// Because they work so differently from the rest of the wrapper hierarchy,
// we pull them out of the Wrapper inheritance hierarchy and create a
// little world around them.

namespace xpc {

bool
holder_get(JSContext *cx, JS::HandleObject holder, JS::HandleId id, JS::MutableHandleValue vp);
bool
holder_set(JSContext *cx, JS::HandleObject holder, JS::HandleId id, bool strict,
           JS::MutableHandleValue vp);

namespace XrayUtils {

bool IsXPCWNHolderClass(const JSClass *clasp);

bool CloneExpandoChain(JSContext *cx, JSObject *src, JSObject *dst);

bool
IsTransparent(JSContext *cx, JS::HandleObject wrapper, JS::HandleId id);

JSObject *
GetNativePropertiesObject(JSContext *cx, JSObject *wrapper);

bool
IsXrayResolving(JSContext *cx, JS::HandleObject wrapper, JS::HandleId id);

bool
HasNativeProperty(JSContext *cx, JS::HandleObject wrapper, JS::HandleId id,
                  bool *hasProp);
}

class XrayTraits;
class XPCWrappedNativeXrayTraits;
class DOMXrayTraits;
class JSXrayTraits;


enum XrayType {
    XrayForDOMObject,
    XrayForWrappedNative,
    XrayForJSObject,
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
    virtual bool isExtensible(JSContext *cx, JS::Handle<JSObject*> wrapper, bool *extensible) MOZ_OVERRIDE;
    virtual bool preventExtensions(JSContext *cx, JS::Handle<JSObject*> wrapper) MOZ_OVERRIDE;
    virtual bool getPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                                       JS::MutableHandle<JSPropertyDescriptor> desc);
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                                          JS::MutableHandle<JSPropertyDescriptor> desc);
    virtual bool defineProperty(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                                JS::MutableHandle<JSPropertyDescriptor> desc);
    virtual bool getOwnPropertyNames(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                     JS::AutoIdVector &props);
    virtual bool delete_(JSContext *cx, JS::Handle<JSObject*> wrapper,
                         JS::Handle<jsid> id, bool *bp);
    virtual bool enumerate(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::AutoIdVector &props);

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
                      JS::AutoIdVector &props);
    virtual bool iterate(JSContext *cx, JS::Handle<JSObject*> wrapper, unsigned flags,
                         JS::MutableHandle<JS::Value> vp);

    virtual bool call(JSContext *cx, JS::Handle<JSObject*> wrapper,
                      const JS::CallArgs &args) MOZ_OVERRIDE;
    virtual bool construct(JSContext *cx, JS::Handle<JSObject*> wrapper,
                           const JS::CallArgs &args) MOZ_OVERRIDE;

    virtual bool defaultValue(JSContext *cx, JS::HandleObject wrapper,
                              JSType hint, JS::MutableHandleValue vp)
                              MOZ_OVERRIDE;

    virtual bool getPrototypeOf(JSContext *cx, JS::HandleObject wrapper,
                                JS::MutableHandleObject protop) MOZ_OVERRIDE;
    virtual bool setPrototypeOf(JSContext *cx, JS::HandleObject wrapper,
                                JS::HandleObject proto, bool *bp) MOZ_OVERRIDE;

    static XrayWrapper singleton;

  private:
    template <bool HasPrototype>
    typename mozilla::EnableIf<HasPrototype, bool>::Type
        getPrototypeOfHelper(JSContext *cx, JS::HandleObject wrapper,
                             JS::HandleObject target, JS::MutableHandleObject protop)
    {
        return Traits::singleton.getPrototypeOf(cx, wrapper, target, protop);
    }
    template <bool HasPrototype>
    typename mozilla::EnableIf<!HasPrototype, bool>::Type
        getPrototypeOfHelper(JSContext *cx, JS::HandleObject wrapper,
                             JS::HandleObject target, JS::MutableHandleObject protop)
    {
        return Base::getPrototypeOf(cx, wrapper, protop);
    }
    bool getPrototypeOfHelper(JSContext *cx, JS::HandleObject wrapper,
                              JS::HandleObject target, JS::MutableHandleObject protop)
    {
        return getPrototypeOfHelper<Traits::HasPrototype>(cx, wrapper, target,
                                                          protop);
    }

    bool enumerate(JSContext *cx, JS::Handle<JSObject*> wrapper, unsigned flags,
                   JS::AutoIdVector &props);
};

#define PermissiveXrayXPCWN xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::XPCWrappedNativeXrayTraits>
#define SecurityXrayXPCWN xpc::XrayWrapper<js::CrossCompartmentSecurityWrapper, xpc::XPCWrappedNativeXrayTraits>
#define PermissiveXrayDOM xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::DOMXrayTraits>
#define SecurityXrayDOM xpc::XrayWrapper<js::CrossCompartmentSecurityWrapper, xpc::DOMXrayTraits>
#define PermissiveXrayJS xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::JSXrayTraits>
#define SCSecurityXrayXPCWN xpc::XrayWrapper<js::SameCompartmentSecurityWrapper, xpc::XPCWrappedNativeXrayTraits>

class SandboxProxyHandler : public js::Wrapper {
public:
    SandboxProxyHandler() : js::Wrapper(0)
    {
    }

    virtual bool getPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> proxy,
                                       JS::Handle<jsid> id,
                                       JS::MutableHandle<JSPropertyDescriptor> desc) MOZ_OVERRIDE;
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> proxy,
                                          JS::Handle<jsid> id,
                                          JS::MutableHandle<JSPropertyDescriptor> desc) MOZ_OVERRIDE;

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

    virtual bool call(JSContext *cx, JS::Handle<JSObject*> proxy,
                      const JS::CallArgs &args) MOZ_OVERRIDE;
};

extern SandboxCallableProxyHandler sandboxCallableProxyHandler;

class AutoSetWrapperNotShadowing;
class XPCWrappedNativeXrayTraits;

class MOZ_STACK_CLASS ResolvingId {
public:
    ResolvingId(JSContext *cx, JS::HandleObject wrapper, JS::HandleId id);
    ~ResolvingId();

    bool isXrayShadowing(jsid id);
    bool isResolving(jsid id);
    static ResolvingId* getResolvingId(JSObject *holder);
    static JSObject* getHolderObject(JSObject *wrapper);
    static ResolvingId *getResolvingIdFromWrapper(JSObject *wrapper);

private:
    friend class AutoSetWrapperNotShadowing;
    friend class XPCWrappedNativeXrayTraits;

    JS::HandleId mId;
    JS::RootedObject mHolder;
    ResolvingId *mPrev;
    bool mXrayShadowing;
};

}

#endif
