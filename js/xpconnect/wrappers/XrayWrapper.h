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
#include "js/Proxy.h"

// Slot where Xray functions for Web IDL methods store a pointer to
// the Xray wrapper they're associated with.
#define XRAY_DOM_FUNCTION_PARENT_WRAPPER_SLOT 0
// Slot where in debug builds Xray functions for Web IDL methods store
// a pointer to their themselves, just so we can assert that they're the
// sort of functions we expect.
#define XRAY_DOM_FUNCTION_NATIVE_SLOT_FOR_SELF 1

// Xray wrappers re-resolve the original native properties on the native
// object and always directly access to those properties.
// Because they work so differently from the rest of the wrapper hierarchy,
// we pull them out of the Wrapper inheritance hierarchy and create a
// little world around them.

class nsIPrincipal;
class XPCWrappedNative;

namespace xpc {

namespace XrayUtils {

bool IsXPCWNHolderClass(const JSClass* clasp);

bool CloneExpandoChain(JSContext* cx, JSObject* src, JSObject* dst);

bool
IsTransparent(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id);

JSObject*
GetNativePropertiesObject(JSContext* cx, JSObject* wrapper);

bool
HasNativeProperty(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
                  bool* hasProp);
} // namespace XrayUtils

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
    constexpr XrayTraits() {}

    static JSObject* getTargetObject(JSObject* wrapper) {
        return js::UncheckedUnwrap(wrapper, /* stopAtWindowProxy = */ false);
    }

    virtual bool resolveNativeProperty(JSContext* cx, JS::HandleObject wrapper,
                                       JS::HandleObject holder, JS::HandleId id,
                                       JS::MutableHandle<JS::PropertyDescriptor> desc) = 0;
    // NB: resolveOwnProperty may decide whether or not to cache what it finds
    // on the holder. If the result is not cached, the lookup will happen afresh
    // for each access, which is the right thing for things like dynamic NodeList
    // properties.
    virtual bool resolveOwnProperty(JSContext* cx, const js::Wrapper& jsWrapper,
                                    JS::HandleObject wrapper, JS::HandleObject holder,
                                    JS::HandleId id, JS::MutableHandle<JS::PropertyDescriptor> desc);

    bool delete_(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
                 JS::ObjectOpResult& result) {
        return result.succeed();
    }

    static const char* className(JSContext* cx, JS::HandleObject wrapper, const js::Wrapper& baseInstance) {
        return baseInstance.className(cx, wrapper);
    }

    virtual void preserveWrapper(JSObject* target) = 0;

    bool getExpandoObject(JSContext* cx, JS::HandleObject target,
                          JS::HandleObject consumer, JS::MutableHandleObject expandObject);
    JSObject* ensureExpandoObject(JSContext* cx, JS::HandleObject wrapper,
                                  JS::HandleObject target);

    JSObject* getHolder(JSObject* wrapper);
    JSObject* ensureHolder(JSContext* cx, JS::HandleObject wrapper);
    virtual JSObject* createHolder(JSContext* cx, JSObject* wrapper) = 0;

    JSObject* getExpandoChain(JS::HandleObject obj);
    bool setExpandoChain(JSContext* cx, JS::HandleObject obj, JS::HandleObject chain);
    bool cloneExpandoChain(JSContext* cx, JS::HandleObject dst, JS::HandleObject src);

private:
    bool expandoObjectMatchesConsumer(JSContext* cx, JS::HandleObject expandoObject,
                                      nsIPrincipal* consumerOrigin,
                                      JS::HandleObject exclusiveGlobal);
    bool getExpandoObjectInternal(JSContext* cx, JS::HandleObject target,
                                  nsIPrincipal* origin, JSObject* exclusiveGlobal,
                                  JS::MutableHandleObject expandoObject);
    JSObject* attachExpandoObject(JSContext* cx, JS::HandleObject target,
                                  nsIPrincipal* origin,
                                  JS::HandleObject exclusiveGlobal);

    XrayTraits(XrayTraits&) = delete;
    const XrayTraits& operator=(XrayTraits&) = delete;
};

class XPCWrappedNativeXrayTraits : public XrayTraits
{
public:
    enum {
        HasPrototype = 0
    };

    static const XrayType Type = XrayForWrappedNative;

    virtual bool resolveNativeProperty(JSContext* cx, JS::HandleObject wrapper,
                                       JS::HandleObject holder, JS::HandleId id,
                                       JS::MutableHandle<JS::PropertyDescriptor> desc) override;
    virtual bool resolveOwnProperty(JSContext* cx, const js::Wrapper& jsWrapper, JS::HandleObject wrapper,
                                    JS::HandleObject holder, JS::HandleId id,
                                    JS::MutableHandle<JS::PropertyDescriptor> desc) override;
    bool defineProperty(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
                        JS::Handle<JS::PropertyDescriptor> desc,
                        JS::Handle<JS::PropertyDescriptor> existingDesc,
                        JS::ObjectOpResult& result, bool* defined)
    {
        *defined = false;
        return true;
    }
    virtual bool enumerateNames(JSContext* cx, JS::HandleObject wrapper, unsigned flags,
                                JS::AutoIdVector& props);
    static bool call(JSContext* cx, JS::HandleObject wrapper,
                     const JS::CallArgs& args, const js::Wrapper& baseInstance);
    static bool construct(JSContext* cx, JS::HandleObject wrapper,
                          const JS::CallArgs& args, const js::Wrapper& baseInstance);

    static XPCWrappedNative* getWN(JSObject* wrapper);

    virtual void preserveWrapper(JSObject* target) override;

    virtual JSObject* createHolder(JSContext* cx, JSObject* wrapper) override;

    static const JSClass HolderClass;
    static XPCWrappedNativeXrayTraits singleton;
};

class DOMXrayTraits : public XrayTraits
{
public:
    constexpr DOMXrayTraits() = default;

    enum {
        HasPrototype = 1
    };

    static const XrayType Type = XrayForDOMObject;

    virtual bool resolveNativeProperty(JSContext* cx, JS::HandleObject wrapper,
                                       JS::HandleObject holder, JS::HandleId id,
                                       JS::MutableHandle<JS::PropertyDescriptor> desc) override
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
    virtual bool resolveOwnProperty(JSContext* cx, const js::Wrapper& jsWrapper, JS::HandleObject wrapper,
                                    JS::HandleObject holder, JS::HandleId id,
                                    JS::MutableHandle<JS::PropertyDescriptor> desc) override;
    bool defineProperty(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
                        JS::Handle<JS::PropertyDescriptor> desc,
                        JS::Handle<JS::PropertyDescriptor> existingDesc,
                        JS::ObjectOpResult& result, bool* defined);
    virtual bool enumerateNames(JSContext* cx, JS::HandleObject wrapper, unsigned flags,
                                JS::AutoIdVector& props);
    static bool call(JSContext* cx, JS::HandleObject wrapper,
                     const JS::CallArgs& args, const js::Wrapper& baseInstance);
    static bool construct(JSContext* cx, JS::HandleObject wrapper,
                          const JS::CallArgs& args, const js::Wrapper& baseInstance);

    static bool getPrototype(JSContext* cx, JS::HandleObject wrapper,
                             JS::HandleObject target,
                             JS::MutableHandleObject protop);

    virtual void preserveWrapper(JSObject* target) override;

    virtual JSObject* createHolder(JSContext* cx, JSObject* wrapper) override;

    static DOMXrayTraits singleton;
};

class JSXrayTraits : public XrayTraits
{
public:
    enum {
        HasPrototype = 1
    };
    static const XrayType Type = XrayForJSObject;

    virtual bool resolveNativeProperty(JSContext* cx, JS::HandleObject wrapper,
                                       JS::HandleObject holder, JS::HandleId id,
                                       JS::MutableHandle<JS::PropertyDescriptor> desc) override
    {
        MOZ_CRASH("resolveNativeProperty hook should never be called with HasPrototype = 1");
    }

    virtual bool resolveOwnProperty(JSContext* cx, const js::Wrapper& jsWrapper, JS::HandleObject wrapper,
                                    JS::HandleObject holder, JS::HandleId id,
                                    JS::MutableHandle<JS::PropertyDescriptor> desc) override;

    bool delete_(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id, JS::ObjectOpResult& result);

    bool defineProperty(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
                        JS::Handle<JS::PropertyDescriptor> desc,
                        JS::Handle<JS::PropertyDescriptor> existingDesc,
                        JS::ObjectOpResult& result, bool* defined);

    virtual bool enumerateNames(JSContext* cx, JS::HandleObject wrapper, unsigned flags,
                                JS::AutoIdVector& props);

    static bool call(JSContext* cx, JS::HandleObject wrapper,
                     const JS::CallArgs& args, const js::Wrapper& baseInstance)
    {
        JSXrayTraits& self = JSXrayTraits::singleton;
        JS::RootedObject holder(cx, self.ensureHolder(cx, wrapper));
        if (self.getProtoKey(holder) == JSProto_Function)
            return baseInstance.call(cx, wrapper, args);

        JS::RootedValue v(cx, JS::ObjectValue(*wrapper));
        js::ReportIsNotFunction(cx, v);
        return false;
    }

    static bool construct(JSContext* cx, JS::HandleObject wrapper,
                          const JS::CallArgs& args, const js::Wrapper& baseInstance);

    bool getPrototype(JSContext* cx, JS::HandleObject wrapper,
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

    virtual void preserveWrapper(JSObject* target) override {
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
    virtual JSObject* createHolder(JSContext* cx, JSObject* wrapper) override;

    static JSProtoKey getProtoKey(JSObject* holder) {
        int32_t key = js::GetReservedSlot(holder, SLOT_PROTOKEY).toInt32();
        return static_cast<JSProtoKey>(key);
    }

    static bool isPrototype(JSObject* holder) {
        return js::GetReservedSlot(holder, SLOT_ISPROTOTYPE).toBoolean();
    }

    static JSProtoKey constructorFor(JSObject* holder) {
        int32_t key = js::GetReservedSlot(holder, SLOT_CONSTRUCTOR_FOR).toInt32();
        return static_cast<JSProtoKey>(key);
    }

    // Operates in the wrapper compartment.
    static bool getOwnPropertyFromWrapperIfSafe(JSContext* cx,
                                                JS::HandleObject wrapper,
                                                JS::HandleId id,
                                                JS::MutableHandle<JS::PropertyDescriptor> desc);

    // Like the above, but operates in the target compartment.
    static bool getOwnPropertyFromTargetIfSafe(JSContext* cx,
                                               JS::HandleObject target,
                                               JS::HandleObject wrapper,
                                               JS::HandleId id,
                                               JS::MutableHandle<JS::PropertyDescriptor> desc);

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

    virtual bool resolveNativeProperty(JSContext* cx, JS::HandleObject wrapper,
                                       JS::HandleObject holder, JS::HandleId id,
                                       JS::MutableHandle<JS::PropertyDescriptor> desc) override
    {
        MOZ_CRASH("resolveNativeProperty hook should never be called with HasPrototype = 1");
    }

    virtual bool resolveOwnProperty(JSContext* cx, const js::Wrapper& jsWrapper, JS::HandleObject wrapper,
                                    JS::HandleObject holder, JS::HandleId id,
                                    JS::MutableHandle<JS::PropertyDescriptor> desc) override;

    bool defineProperty(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
                        JS::Handle<JS::PropertyDescriptor> desc,
                        JS::Handle<JS::PropertyDescriptor> existingDesc,
                        JS::ObjectOpResult& result, bool* defined)
    {
        *defined = false;
        return true;
    }

    virtual bool enumerateNames(JSContext* cx, JS::HandleObject wrapper, unsigned flags,
                                JS::AutoIdVector& props)
    {
        return true;
    }

    static bool call(JSContext* cx, JS::HandleObject wrapper,
                     const JS::CallArgs& args, const js::Wrapper& baseInstance)
    {
        JS::RootedValue v(cx, JS::ObjectValue(*wrapper));
        js::ReportIsNotFunction(cx, v);
        return false;
    }

    static bool construct(JSContext* cx, JS::HandleObject wrapper,
                          const JS::CallArgs& args, const js::Wrapper& baseInstance)
    {
        JS::RootedValue v(cx, JS::ObjectValue(*wrapper));
        js::ReportIsNotFunction(cx, v);
        return false;
    }

    bool getPrototype(JSContext* cx, JS::HandleObject wrapper,
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

    static const char* className(JSContext* cx, JS::HandleObject wrapper, const js::Wrapper& baseInstance) {
        return "Opaque";
    }

    virtual void preserveWrapper(JSObject* target) override { }

    virtual JSObject* createHolder(JSContext* cx, JSObject* wrapper) override
    {
        return JS_NewObjectWithGivenProto(cx, nullptr, nullptr);
    }

    static OpaqueXrayTraits singleton;
};

XrayType GetXrayType(JSObject* obj);
XrayTraits* GetXrayTraits(JSObject* obj);

// NB: Base *must* derive from JSProxyHandler
template <typename Base, typename Traits = XPCWrappedNativeXrayTraits >
class XrayWrapper : public Base {
  public:
    constexpr explicit XrayWrapper(unsigned flags)
      : Base(flags | WrapperFactory::IS_XRAY_WRAPPER_FLAG, Traits::HasPrototype)
    { };

    /* Standard internal methods. */
    virtual bool getOwnPropertyDescriptor(JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                                          JS::MutableHandle<JS::PropertyDescriptor> desc) const override;
    virtual bool defineProperty(JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                                JS::Handle<JS::PropertyDescriptor> desc,
                                JS::ObjectOpResult& result) const override;
    virtual bool ownPropertyKeys(JSContext* cx, JS::Handle<JSObject*> wrapper,
                                 JS::AutoIdVector& props) const override;
    virtual bool delete_(JSContext* cx, JS::Handle<JSObject*> wrapper,
                         JS::Handle<jsid> id, JS::ObjectOpResult& result) const override;
    virtual bool enumerate(JSContext* cx, JS::Handle<JSObject*> wrapper,
                           JS::MutableHandle<JSObject*> objp) const override;
    virtual bool getPrototype(JSContext* cx, JS::HandleObject wrapper,
                              JS::MutableHandleObject protop) const override;
    virtual bool setPrototype(JSContext* cx, JS::HandleObject wrapper,
                              JS::HandleObject proto, JS::ObjectOpResult& result) const override;
    virtual bool getPrototypeIfOrdinary(JSContext* cx, JS::HandleObject wrapper, bool* isOrdinary,
                                        JS::MutableHandleObject protop) const override;
    virtual bool setImmutablePrototype(JSContext* cx, JS::HandleObject wrapper,
                                       bool* succeeded) const override;
    virtual bool preventExtensions(JSContext* cx, JS::Handle<JSObject*> wrapper,
                                   JS::ObjectOpResult& result) const override;
    virtual bool isExtensible(JSContext* cx, JS::Handle<JSObject*> wrapper, bool* extensible) const override;
    virtual bool has(JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                     bool* bp) const override;
    virtual bool get(JSContext* cx, JS::Handle<JSObject*> wrapper, JS::HandleValue receiver,
                     JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp) const override;
    virtual bool set(JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                     JS::Handle<JS::Value> v, JS::Handle<JS::Value> receiver,
                     JS::ObjectOpResult& result) const override;
    virtual bool call(JSContext* cx, JS::Handle<JSObject*> wrapper,
                      const JS::CallArgs& args) const override;
    virtual bool construct(JSContext* cx, JS::Handle<JSObject*> wrapper,
                           const JS::CallArgs& args) const override;

    /* SpiderMonkey extensions. */
    virtual bool getPropertyDescriptor(JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                                       JS::MutableHandle<JS::PropertyDescriptor> desc) const override;
    virtual bool hasOwn(JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
                        bool* bp) const override;
    virtual bool getOwnEnumerablePropertyKeys(JSContext* cx, JS::Handle<JSObject*> wrapper,
                                              JS::AutoIdVector& props) const override;

    virtual const char* className(JSContext* cx, JS::HandleObject proxy) const override;

    static const XrayWrapper singleton;

  private:
    template <bool HasPrototype>
    typename mozilla::EnableIf<HasPrototype, bool>::Type
        getPrototypeHelper(JSContext* cx, JS::HandleObject wrapper,
                           JS::HandleObject target, JS::MutableHandleObject protop) const
    {
        return Traits::singleton.getPrototype(cx, wrapper, target, protop);
    }
    template <bool HasPrototype>
    typename mozilla::EnableIf<!HasPrototype, bool>::Type
        getPrototypeHelper(JSContext* cx, JS::HandleObject wrapper,
                           JS::HandleObject target, JS::MutableHandleObject protop) const
    {
        return Base::getPrototype(cx, wrapper, protop);
    }
    bool getPrototypeHelper(JSContext* cx, JS::HandleObject wrapper,
                            JS::HandleObject target, JS::MutableHandleObject protop) const
    {
        return getPrototypeHelper<Traits::HasPrototype>(cx, wrapper, target,
                                                        protop);
    }

  protected:
    bool getPropertyKeys(JSContext* cx, JS::Handle<JSObject*> wrapper, unsigned flags,
                         JS::AutoIdVector& props) const;
};

#define PermissiveXrayXPCWN xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::XPCWrappedNativeXrayTraits>
#define SecurityXrayXPCWN xpc::XrayWrapper<js::CrossCompartmentSecurityWrapper, xpc::XPCWrappedNativeXrayTraits>
#define PermissiveXrayDOM xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::DOMXrayTraits>
#define SecurityXrayDOM xpc::XrayWrapper<js::CrossCompartmentSecurityWrapper, xpc::DOMXrayTraits>
#define PermissiveXrayJS xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::JSXrayTraits>
#define PermissiveXrayOpaque xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::OpaqueXrayTraits>
#define SCSecurityXrayXPCWN xpc::XrayWrapper<js::SameCompartmentSecurityWrapper, xpc::XPCWrappedNativeXrayTraits>

extern template class PermissiveXrayXPCWN;
extern template class SecurityXrayXPCWN;
extern template class PermissiveXrayDOM;
extern template class SecurityXrayDOM;
extern template class PermissiveXrayJS;
extern template class PermissiveXrayOpaque;
extern template class PermissiveXrayXPCWN;

class SandboxProxyHandler : public js::Wrapper {
public:
    constexpr SandboxProxyHandler() : js::Wrapper(0)
    {
    }

    virtual bool getOwnPropertyDescriptor(JSContext* cx, JS::Handle<JSObject*> proxy,
                                          JS::Handle<jsid> id,
                                          JS::MutableHandle<JS::PropertyDescriptor> desc) const override;

    // We just forward the high-level methods to the BaseProxyHandler versions
    // which implement them in terms of lower-level methods.
    virtual bool has(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                     bool* bp) const override;
    virtual bool get(JSContext* cx, JS::Handle<JSObject*> proxy, JS::HandleValue receiver,
                     JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp) const override;
    virtual bool set(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                     JS::Handle<JS::Value> v, JS::Handle<JS::Value> receiver,
                     JS::ObjectOpResult& result) const override;

    virtual bool getPropertyDescriptor(JSContext* cx, JS::Handle<JSObject*> proxy,
                                       JS::Handle<jsid> id,
                                       JS::MutableHandle<JS::PropertyDescriptor> desc) const override;
    virtual bool hasOwn(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                        bool* bp) const override;
    virtual bool getOwnEnumerablePropertyKeys(JSContext* cx, JS::Handle<JSObject*> proxy,
                                              JS::AutoIdVector& props) const override;
    virtual bool enumerate(JSContext* cx, JS::Handle<JSObject*> proxy,
                           JS::MutableHandle<JSObject*> objp) const override;
};

extern const SandboxProxyHandler sandboxProxyHandler;

// A proxy handler that lets us wrap callables and invoke them with
// the correct this object, while forwarding all other operations down
// to them directly.
class SandboxCallableProxyHandler : public js::Wrapper {
public:
    constexpr SandboxCallableProxyHandler() : js::Wrapper(0)
    {
    }

    virtual bool call(JSContext* cx, JS::Handle<JSObject*> proxy,
                      const JS::CallArgs& args) const override;

    static const size_t SandboxProxySlot = 0;

    static inline JSObject* getSandboxProxy(JS::Handle<JSObject*> proxy)
    {
        return &js::GetProxyExtra(proxy, SandboxProxySlot).toObject();
    }
};

extern const SandboxCallableProxyHandler sandboxCallableProxyHandler;

class AutoSetWrapperNotShadowing;

} // namespace xpc

#endif
