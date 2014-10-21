/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XrayWrapper_h
#define XrayWrapper_h

#include "mozilla/Attributes.h"

#include "WrapperFactory.h"

#include "jswrapper.h"

// Xray wrappers re-resolve the original native properties on the native
// object and always directly access to those properties.
// Because they work so differently from the rest of the wrapper hierarchy,
// we pull them out of the Wrapper inheritance hierarchy and create a
// little world around them.

class nsIPrincipal;
class XPCWrappedNative;

namespace xpc {

namespace XrayUtils {

bool IsXPCWNHolderClass(const JSClass *clasp);

bool CloneExpandoChain(JSContext *cx, JSObject *src, JSObject *dst);

bool
IsTransparent(JSContext *cx, JS::HandleObject wrapper, JS::HandleId id);

JSObject *
GetNativePropertiesObject(JSContext *cx, JSObject *wrapper);

bool
HasNativeProperty(JSContext *cx, JS::HandleObject wrapper, JS::HandleId id,
                  bool *hasProp);
}

enum XrayType {
    XrayForDOMObject,
    XrayForWrappedNative,
    XrayForJSObject,
    XrayForOpaqueObject,
    NotXray
};

class XrayTraits
{
public:
    XrayTraits() {}

    static JSObject* getTargetObject(JSObject *wrapper) {
        return js::UncheckedUnwrap(wrapper, /* stopAtOuter = */ false);
    }

    virtual bool resolveNativeProperty(JSContext *cx, JS::HandleObject wrapper,
                                       JS::HandleObject holder, JS::HandleId id,
                                       JS::MutableHandle<JSPropertyDescriptor> desc) = 0;
    // NB: resolveOwnProperty may decide whether or not to cache what it finds
    // on the holder. If the result is not cached, the lookup will happen afresh
    // for each access, which is the right thing for things like dynamic NodeList
    // properties.
    virtual bool resolveOwnProperty(JSContext *cx, const js::Wrapper &jsWrapper,
                                    JS::HandleObject wrapper, JS::HandleObject holder,
                                    JS::HandleId id, JS::MutableHandle<JSPropertyDescriptor> desc);

    bool delete_(JSContext *cx, JS::HandleObject wrapper, JS::HandleId id, bool *bp) {
        *bp = true;
        return true;
    }

    static const char *className(JSContext *cx, JS::HandleObject wrapper, const js::Wrapper& baseInstance) {
        return baseInstance.className(cx, wrapper);
    }

    virtual void preserveWrapper(JSObject *target) = 0;

    bool getExpandoObject(JSContext *cx, JS::HandleObject target,
                          JS::HandleObject consumer, JS::MutableHandleObject expandObject);
    JSObject* ensureExpandoObject(JSContext *cx, JS::HandleObject wrapper,
                                  JS::HandleObject target);

    JSObject* getHolder(JSObject *wrapper);
    JSObject* ensureHolder(JSContext *cx, JS::HandleObject wrapper);
    virtual JSObject* createHolder(JSContext *cx, JSObject *wrapper) = 0;

    JSObject* getExpandoChain(JS::HandleObject obj);
    bool setExpandoChain(JSContext *cx, JS::HandleObject obj, JS::HandleObject chain);
    bool cloneExpandoChain(JSContext *cx, JS::HandleObject dst, JS::HandleObject src);

private:
    bool expandoObjectMatchesConsumer(JSContext *cx, JS::HandleObject expandoObject,
                                      nsIPrincipal *consumerOrigin,
                                      JS::HandleObject exclusiveGlobal);
    bool getExpandoObjectInternal(JSContext *cx, JS::HandleObject target,
                                  nsIPrincipal *origin, JSObject *exclusiveGlobal,
                                  JS::MutableHandleObject expandoObject);
    JSObject* attachExpandoObject(JSContext *cx, JS::HandleObject target,
                                  nsIPrincipal *origin,
                                  JS::HandleObject exclusiveGlobal);

    XrayTraits(XrayTraits &) MOZ_DELETE;
    const XrayTraits& operator=(XrayTraits &) MOZ_DELETE;
};

class XPCWrappedNativeXrayTraits : public XrayTraits
{
public:
    enum {
        HasPrototype = 0
    };

    static const XrayType Type = XrayForWrappedNative;

    virtual bool resolveNativeProperty(JSContext *cx, JS::HandleObject wrapper,
                                       JS::HandleObject holder, JS::HandleId id,
                                       JS::MutableHandle<JSPropertyDescriptor> desc) MOZ_OVERRIDE;
    virtual bool resolveOwnProperty(JSContext *cx, const js::Wrapper &jsWrapper, JS::HandleObject wrapper,
                                    JS::HandleObject holder, JS::HandleId id,
                                    JS::MutableHandle<JSPropertyDescriptor> desc) MOZ_OVERRIDE;
    bool defineProperty(JSContext *cx, JS::HandleObject wrapper, JS::HandleId id,
                        JS::MutableHandle<JSPropertyDescriptor> desc,
                        JS::Handle<JSPropertyDescriptor> existingDesc, bool *defined);
    virtual bool enumerateNames(JSContext *cx, JS::HandleObject wrapper, unsigned flags,
                                JS::AutoIdVector &props);
    static bool call(JSContext *cx, JS::HandleObject wrapper,
                     const JS::CallArgs &args, const js::Wrapper& baseInstance);
    static bool construct(JSContext *cx, JS::HandleObject wrapper,
                          const JS::CallArgs &args, const js::Wrapper& baseInstance);

    static bool resolveDOMCollectionProperty(JSContext *cx, JS::HandleObject wrapper,
                                             JS::HandleObject holder, JS::HandleId id,
                                             JS::MutableHandle<JSPropertyDescriptor> desc);

    static XPCWrappedNative* getWN(JSObject *wrapper);

    virtual void preserveWrapper(JSObject *target) MOZ_OVERRIDE;

    virtual JSObject* createHolder(JSContext *cx, JSObject *wrapper) MOZ_OVERRIDE;

    static const JSClass HolderClass;
    static XPCWrappedNativeXrayTraits singleton;
};

class DOMXrayTraits : public XrayTraits
{
public:
    enum {
        HasPrototype = 1
    };

    static const XrayType Type = XrayForDOMObject;

    virtual bool resolveNativeProperty(JSContext *cx, JS::HandleObject wrapper,
                                       JS::HandleObject holder, JS::HandleId id,
                                       JS::MutableHandle<JSPropertyDescriptor> desc) MOZ_OVERRIDE
    {
        // Xrays for DOM binding objects have a prototype chain that consists of
        // Xrays for the prototypes of the DOM binding object (ignoring changes
        // in the prototype chain made by script, plugins or XBL). All properties for
        // these Xrays are really own properties, either of the instance object or
        // of the prototypes.
        // FIXME https://bugzilla.mozilla.org/show_bug.cgi?id=1072482
        //       This should really be:
        // MOZ_CRASH("resolveNativeProperty hook should never be called with HasPrototype = 1");
        //       but we can't do that yet because XrayUtils::HasNativeProperty calls this.
        return true;
    }
    virtual bool resolveOwnProperty(JSContext *cx, const js::Wrapper &jsWrapper, JS::HandleObject wrapper,
                                    JS::HandleObject holder, JS::HandleId id,
                                    JS::MutableHandle<JSPropertyDescriptor> desc) MOZ_OVERRIDE;
    bool defineProperty(JSContext *cx, JS::HandleObject wrapper, JS::HandleId id,
                        JS::MutableHandle<JSPropertyDescriptor> desc,
                        JS::Handle<JSPropertyDescriptor> existingDesc, bool *defined);
    virtual bool enumerateNames(JSContext *cx, JS::HandleObject wrapper, unsigned flags,
                                JS::AutoIdVector &props);
    static bool call(JSContext *cx, JS::HandleObject wrapper,
                     const JS::CallArgs &args, const js::Wrapper& baseInstance);
    static bool construct(JSContext *cx, JS::HandleObject wrapper,
                          const JS::CallArgs &args, const js::Wrapper& baseInstance);

    static bool getPrototypeOf(JSContext *cx, JS::HandleObject wrapper,
                               JS::HandleObject target,
                               JS::MutableHandleObject protop);

    virtual void preserveWrapper(JSObject *target) MOZ_OVERRIDE;

    virtual JSObject* createHolder(JSContext *cx, JSObject *wrapper) MOZ_OVERRIDE;

    static DOMXrayTraits singleton;
};

class JSXrayTraits : public XrayTraits
{
public:
    enum {
        HasPrototype = 1
    };
    static const XrayType Type = XrayForJSObject;

    virtual bool resolveNativeProperty(JSContext *cx, JS::HandleObject wrapper,
                                       JS::HandleObject holder, JS::HandleId id,
                                       JS::MutableHandle<JSPropertyDescriptor> desc) MOZ_OVERRIDE
    {
        MOZ_CRASH("resolveNativeProperty hook should never be called with HasPrototype = 1");
    }

    virtual bool resolveOwnProperty(JSContext *cx, const js::Wrapper &jsWrapper, JS::HandleObject wrapper,
                                    JS::HandleObject holder, JS::HandleId id,
                                    JS::MutableHandle<JSPropertyDescriptor> desc) MOZ_OVERRIDE;

    bool delete_(JSContext *cx, JS::HandleObject wrapper, JS::HandleId id, bool *bp);

    bool defineProperty(JSContext *cx, JS::HandleObject wrapper, JS::HandleId id,
                        JS::MutableHandle<JSPropertyDescriptor> desc,
                        JS::Handle<JSPropertyDescriptor> existingDesc, bool *defined);

    virtual bool enumerateNames(JSContext *cx, JS::HandleObject wrapper, unsigned flags,
                                JS::AutoIdVector &props);

    static bool call(JSContext *cx, JS::HandleObject wrapper,
                     const JS::CallArgs &args, const js::Wrapper& baseInstance)
    {
        JSXrayTraits &self = JSXrayTraits::singleton;
        JS::RootedObject holder(cx, self.ensureHolder(cx, wrapper));
        if (self.getProtoKey(holder) == JSProto_Function)
            return baseInstance.call(cx, wrapper, args);

        JS::RootedValue v(cx, JS::ObjectValue(*wrapper));
        js_ReportIsNotFunction(cx, v);
        return false;
    }

    static bool construct(JSContext *cx, JS::HandleObject wrapper,
                          const JS::CallArgs &args, const js::Wrapper& baseInstance)
    {
        JSXrayTraits &self = JSXrayTraits::singleton;
        JS::RootedObject holder(cx, self.ensureHolder(cx, wrapper));
        if (self.getProtoKey(holder) == JSProto_Function)
            return baseInstance.construct(cx, wrapper, args);

        JS::RootedValue v(cx, JS::ObjectValue(*wrapper));
        js_ReportIsNotFunction(cx, v);
        return false;
    }

    bool getPrototypeOf(JSContext *cx, JS::HandleObject wrapper,
                        JS::HandleObject target,
                        JS::MutableHandleObject protop)
    {
        JS::RootedObject holder(cx, ensureHolder(cx, wrapper));
        JSProtoKey key = getProtoKey(holder);
        if (isPrototype(holder)) {
            JSProtoKey parentKey = js::ParentKeyForStandardClass(key);
            if (parentKey == JSProto_Null) {
                protop.set(nullptr);
                return true;
            }
            key = parentKey;
        }

        {
            JSAutoCompartment ac(cx, target);
            if (!JS_GetClassPrototype(cx, key, protop))
                return false;
        }
        return JS_WrapObject(cx, protop);
    }

    virtual void preserveWrapper(JSObject *target) MOZ_OVERRIDE {
        // In the case of pure JS objects, there is no underlying object, and
        // the target is the canonical representation of state. If it gets
        // collected, then expandos and such should be collected too. So there's
        // nothing to do here.
    }

    enum {
        SLOT_PROTOKEY = 0,
        SLOT_ISPROTOTYPE,
        SLOT_CONSTRUCTOR_FOR,
        SLOT_COUNT
    };
    virtual JSObject* createHolder(JSContext *cx, JSObject *wrapper) MOZ_OVERRIDE;

    static JSProtoKey getProtoKey(JSObject *holder) {
        int32_t key = js::GetReservedSlot(holder, SLOT_PROTOKEY).toInt32();
        return static_cast<JSProtoKey>(key);
    }

    static bool isPrototype(JSObject *holder) {
        return js::GetReservedSlot(holder, SLOT_ISPROTOTYPE).toBoolean();
    }

    static JSProtoKey constructorFor(JSObject *holder) {
        int32_t key = js::GetReservedSlot(holder, SLOT_CONSTRUCTOR_FOR).toInt32();
        return static_cast<JSProtoKey>(key);
    }

    static bool getOwnPropertyFromTargetIfSafe(JSContext *cx,
                                               JS::HandleObject target,
                                               JS::HandleObject wrapper,
                                               JS::HandleId id,
                                               JS::MutableHandle<JSPropertyDescriptor> desc);

    static const JSClass HolderClass;
    static JSXrayTraits singleton;
};

// These traits are used when the target is not Xrayable and we therefore want
// to make it opaque modulo the usual Xray machinery (like expandos and
// .wrappedJSObject).
class OpaqueXrayTraits : public XrayTraits
{
public:
    enum {
        HasPrototype = 1
    };
    static const XrayType Type = XrayForOpaqueObject;

    virtual bool resolveNativeProperty(JSContext *cx, JS::HandleObject wrapper,
                                       JS::HandleObject holder, JS::HandleId id,
                                       JS::MutableHandle<JSPropertyDescriptor> desc) MOZ_OVERRIDE
    {
        MOZ_CRASH("resolveNativeProperty hook should never be called with HasPrototype = 1");
    }

    virtual bool resolveOwnProperty(JSContext *cx, const js::Wrapper &jsWrapper, JS::HandleObject wrapper,
                                    JS::HandleObject holder, JS::HandleId id,
                                    JS::MutableHandle<JSPropertyDescriptor> desc) MOZ_OVERRIDE;

    bool defineProperty(JSContext *cx, JS::HandleObject wrapper, JS::HandleId id,
                        JS::MutableHandle<JSPropertyDescriptor> desc,
                        JS::Handle<JSPropertyDescriptor> existingDesc, bool *defined)
    {
        *defined = false;
        return true;
    }

    virtual bool enumerateNames(JSContext *cx, JS::HandleObject wrapper, unsigned flags,
                                JS::AutoIdVector &props)
    {
        return true;
    }

    static bool call(JSContext *cx, JS::HandleObject wrapper,
                     const JS::CallArgs &args, const js::Wrapper& baseInstance)
    {
        JS::RootedValue v(cx, JS::ObjectValue(*wrapper));
        js_ReportIsNotFunction(cx, v);
        return false;
    }

    static bool construct(JSContext *cx, JS::HandleObject wrapper,
                          const JS::CallArgs &args, const js::Wrapper& baseInstance)
    {
        JS::RootedValue v(cx, JS::ObjectValue(*wrapper));
        js_ReportIsNotFunction(cx, v);
        return false;
    }

    bool getPrototypeOf(JSContext *cx, JS::HandleObject wrapper,
                        JS::HandleObject target,
                        JS::MutableHandleObject protop)
    {
        // Opaque wrappers just get targetGlobal.Object.prototype as their
        // prototype. This is preferable to using a null prototype because it
        // lets things like |toString| and |__proto__| work.
        {
            JSAutoCompartment ac(cx, target);
            if (!JS_GetClassPrototype(cx, JSProto_Object, protop))
                return false;
        }
        return JS_WrapObject(cx, protop);
    }

    static const char *className(JSContext *cx, JS::HandleObject wrapper, const js::Wrapper& baseInstance) {
        return "Opaque";
    }

    virtual void preserveWrapper(JSObject *target) MOZ_OVERRIDE { }

    virtual JSObject* createHolder(JSContext *cx, JSObject *wrapper) MOZ_OVERRIDE
    {
        JS::RootedObject global(cx, JS_GetGlobalForObject(cx, wrapper));
        return JS_NewObjectWithGivenProto(cx, nullptr, JS::NullPtr(), global);
    }

    static OpaqueXrayTraits singleton;
};

XrayType GetXrayType(JSObject *obj);
XrayTraits* GetXrayTraits(JSObject *obj);

// NB: Base *must* derive from JSProxyHandler
template <typename Base, typename Traits = XPCWrappedNativeXrayTraits >
class XrayWrapper : public Base {
  public:
    MOZ_CONSTEXPR explicit XrayWrapper(unsigned flags)
      : Base(flags | WrapperFactory::IS_XRAY_WRAPPER_FLAG, Traits::HasPrototype)
    { };

    /* Standard internal methods. */
    virtual bool getOwnPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                                          JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool defineProperty(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                                JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool ownPropertyKeys(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                 JS::AutoIdVector &props) const MOZ_OVERRIDE;
    virtual bool delete_(JSContext *cx, JS::Handle<JSObject*> wrapper,
                         JS::Handle<jsid> id, bool *bp) const MOZ_OVERRIDE;
    virtual bool enumerate(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::AutoIdVector &props) const MOZ_OVERRIDE;
    virtual bool isExtensible(JSContext *cx, JS::Handle<JSObject*> wrapper, bool *extensible) const MOZ_OVERRIDE;
    virtual bool preventExtensions(JSContext *cx, JS::Handle<JSObject*> wrapper) const MOZ_OVERRIDE;
    virtual bool getPrototypeOf(JSContext *cx, JS::HandleObject wrapper,
                                JS::MutableHandleObject protop) const MOZ_OVERRIDE;
    virtual bool setPrototypeOf(JSContext *cx, JS::HandleObject wrapper,
                                JS::HandleObject proto, bool *bp) const MOZ_OVERRIDE;
    virtual bool has(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                     bool *bp) const MOZ_OVERRIDE;
    virtual bool get(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<JSObject*> receiver,
                     JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp) const MOZ_OVERRIDE;
    virtual bool set(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<JSObject*> receiver,
                     JS::Handle<jsid> id, bool strict, JS::MutableHandle<JS::Value> vp) const MOZ_OVERRIDE;
    virtual bool call(JSContext *cx, JS::Handle<JSObject*> wrapper,
                      const JS::CallArgs &args) const MOZ_OVERRIDE;
    virtual bool construct(JSContext *cx, JS::Handle<JSObject*> wrapper,
                           const JS::CallArgs &args) const MOZ_OVERRIDE;

    /* SpiderMonkey extensions. */
    virtual bool getPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                                       JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool hasOwn(JSContext *cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                        bool *bp) const MOZ_OVERRIDE;
    virtual bool getOwnEnumerablePropertyKeys(JSContext *cx, JS::Handle<JSObject*> wrapper,
                                              JS::AutoIdVector &props) const MOZ_OVERRIDE;
    virtual bool iterate(JSContext *cx, JS::Handle<JSObject*> wrapper, unsigned flags,
                         JS::MutableHandle<JS::Value> vp) const MOZ_OVERRIDE;

    virtual const char *className(JSContext *cx, JS::HandleObject proxy) const MOZ_OVERRIDE;
    virtual bool defaultValue(JSContext *cx, JS::HandleObject wrapper,
                              JSType hint, JS::MutableHandleValue vp)
                              const MOZ_OVERRIDE;

    static const XrayWrapper singleton;

  private:
    template <bool HasPrototype>
    typename mozilla::EnableIf<HasPrototype, bool>::Type
        getPrototypeOfHelper(JSContext *cx, JS::HandleObject wrapper,
                             JS::HandleObject target, JS::MutableHandleObject protop) const
    {
        return Traits::singleton.getPrototypeOf(cx, wrapper, target, protop);
    }
    template <bool HasPrototype>
    typename mozilla::EnableIf<!HasPrototype, bool>::Type
        getPrototypeOfHelper(JSContext *cx, JS::HandleObject wrapper,
                             JS::HandleObject target, JS::MutableHandleObject protop) const
    {
        return Base::getPrototypeOf(cx, wrapper, protop);
    }
    bool getPrototypeOfHelper(JSContext *cx, JS::HandleObject wrapper,
                              JS::HandleObject target, JS::MutableHandleObject protop) const
    {
        return getPrototypeOfHelper<Traits::HasPrototype>(cx, wrapper, target,
                                                          protop);
    }

  protected:
    bool enumerate(JSContext *cx, JS::Handle<JSObject*> wrapper, unsigned flags,
                   JS::AutoIdVector &props) const;
};

#define PermissiveXrayXPCWN xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::XPCWrappedNativeXrayTraits>
#define SecurityXrayXPCWN xpc::XrayWrapper<js::CrossCompartmentSecurityWrapper, xpc::XPCWrappedNativeXrayTraits>
#define PermissiveXrayDOM xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::DOMXrayTraits>
#define SecurityXrayDOM xpc::XrayWrapper<js::CrossCompartmentSecurityWrapper, xpc::DOMXrayTraits>
#define PermissiveXrayJS xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::JSXrayTraits>
#define PermissiveXrayOpaque xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::OpaqueXrayTraits>
#define SCSecurityXrayXPCWN xpc::XrayWrapper<js::SameCompartmentSecurityWrapper, xpc::XPCWrappedNativeXrayTraits>

class SandboxProxyHandler : public js::Wrapper {
public:
    MOZ_CONSTEXPR SandboxProxyHandler() : js::Wrapper(0)
    {
    }

    virtual bool getOwnPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> proxy,
                                          JS::Handle<jsid> id,
                                          JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;

    // We just forward the high-level methods to the BaseProxyHandler versions
    // which implement them in terms of lower-level methods.
    virtual bool has(JSContext *cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                     bool *bp) const MOZ_OVERRIDE;
    virtual bool get(JSContext *cx, JS::Handle<JSObject*> proxy, JS::Handle<JSObject*> receiver,
                     JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp) const MOZ_OVERRIDE;
    virtual bool set(JSContext *cx, JS::Handle<JSObject*> proxy, JS::Handle<JSObject*> receiver,
                     JS::Handle<jsid> id, bool strict, JS::MutableHandle<JS::Value> vp) const MOZ_OVERRIDE;

    virtual bool getPropertyDescriptor(JSContext *cx, JS::Handle<JSObject*> proxy,
                                       JS::Handle<jsid> id,
                                       JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
    virtual bool hasOwn(JSContext *cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                        bool *bp) const MOZ_OVERRIDE;
    virtual bool getOwnEnumerablePropertyKeys(JSContext *cx, JS::Handle<JSObject*> proxy,
                                              JS::AutoIdVector &props) const MOZ_OVERRIDE;
    virtual bool iterate(JSContext *cx, JS::Handle<JSObject*> proxy, unsigned flags,
                         JS::MutableHandle<JS::Value> vp) const MOZ_OVERRIDE;
};

extern const SandboxProxyHandler sandboxProxyHandler;

// A proxy handler that lets us wrap callables and invoke them with
// the correct this object, while forwarding all other operations down
// to them directly.
class SandboxCallableProxyHandler : public js::Wrapper {
public:
    MOZ_CONSTEXPR SandboxCallableProxyHandler() : js::Wrapper(0)
    {
    }

    virtual bool call(JSContext *cx, JS::Handle<JSObject*> proxy,
                      const JS::CallArgs &args) const MOZ_OVERRIDE;
};

extern const SandboxCallableProxyHandler sandboxCallableProxyHandler;

class AutoSetWrapperNotShadowing;

}

#endif
