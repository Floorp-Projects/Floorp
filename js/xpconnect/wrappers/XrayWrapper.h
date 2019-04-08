/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XrayWrapper_h
#define XrayWrapper_h

#include "mozilla/Attributes.h"

#include "WrapperFactory.h"

#include "jsapi.h"
#include "js/Proxy.h"
#include "js/Wrapper.h"

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

namespace xpc {

namespace XrayUtils {

bool IsTransparent(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id);

bool HasNativeProperty(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
                       bool* hasProp);
}  // namespace XrayUtils

enum XrayType {
  XrayForDOMObject,
  XrayForJSObject,
  XrayForOpaqueObject,
  NotXray
};

class XrayTraits {
 public:
  constexpr XrayTraits() {}

  static JSObject* getTargetObject(JSObject* wrapper) {
    JSObject* target =
        js::UncheckedUnwrap(wrapper, /* stopAtWindowProxy = */ false);
    if (target) {
      JS::ExposeObjectToActiveJS(target);
    }
    return target;
  }

  // NB: resolveOwnProperty may decide whether or not to cache what it finds
  // on the holder. If the result is not cached, the lookup will happen afresh
  // for each access, which is the right thing for things like dynamic NodeList
  // properties.
  virtual bool resolveOwnProperty(
      JSContext* cx, JS::HandleObject wrapper, JS::HandleObject target,
      JS::HandleObject holder, JS::HandleId id,
      JS::MutableHandle<JS::PropertyDescriptor> desc);

  bool delete_(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
               JS::ObjectOpResult& result) {
    return result.succeed();
  }

  static bool getBuiltinClass(JSContext* cx, JS::HandleObject wrapper,
                              const js::Wrapper& baseInstance,
                              js::ESClass* cls) {
    return baseInstance.getBuiltinClass(cx, wrapper, cls);
  }

  static const char* className(JSContext* cx, JS::HandleObject wrapper,
                               const js::Wrapper& baseInstance) {
    return baseInstance.className(cx, wrapper);
  }

  virtual void preserveWrapper(JSObject* target) = 0;

  bool getExpandoObject(JSContext* cx, JS::HandleObject target,
                        JS::HandleObject consumer,
                        JS::MutableHandleObject expandObject);
  JSObject* ensureExpandoObject(JSContext* cx, JS::HandleObject wrapper,
                                JS::HandleObject target);

  // Slots for holder objects.
  enum {
    HOLDER_SLOT_CACHED_PROTO = 0,
    HOLDER_SLOT_EXPANDO = 1,
    HOLDER_SHARED_SLOT_COUNT
  };

  static JSObject* getHolder(JSObject* wrapper);
  JSObject* ensureHolder(JSContext* cx, JS::HandleObject wrapper);
  virtual JSObject* createHolder(JSContext* cx, JSObject* wrapper) = 0;

  JSObject* getExpandoChain(JS::HandleObject obj);
  JSObject* detachExpandoChain(JS::HandleObject obj);
  bool setExpandoChain(JSContext* cx, JS::HandleObject obj,
                       JS::HandleObject chain);
  bool cloneExpandoChain(JSContext* cx, JS::HandleObject dst,
                         JS::HandleObject srcChain);

 protected:
  static const JSClass HolderClass;

  // Get the JSClass we should use for our expando object.
  virtual const JSClass* getExpandoClass(JSContext* cx,
                                         JS::HandleObject target) const;

 private:
  bool expandoObjectMatchesConsumer(JSContext* cx,
                                    JS::HandleObject expandoObject,
                                    nsIPrincipal* consumerOrigin);

  // |expandoChain| is the expando chain in the wrapped object's compartment.
  // |exclusiveWrapper| is any xray that has exclusive use of the expando.
  // |cx| may be in any compartment.
  bool getExpandoObjectInternal(JSContext* cx, JSObject* expandoChain,
                                JS::HandleObject exclusiveWrapper,
                                nsIPrincipal* origin,
                                JS::MutableHandleObject expandoObject);

  // |cx| is in the target's compartment, and |exclusiveWrapper| is any xray
  // that has exclusive use of the expando. |exclusiveWrapperGlobal| is the
  // caller's global and must be same-compartment with |exclusiveWrapper|.
  JSObject* attachExpandoObject(JSContext* cx, JS::HandleObject target,
                                JS::HandleObject exclusiveWrapper,
                                JS::HandleObject exclusiveWrapperGlobal,
                                nsIPrincipal* origin);

  XrayTraits(XrayTraits&) = delete;
  const XrayTraits& operator=(XrayTraits&) = delete;
};

class DOMXrayTraits : public XrayTraits {
 public:
  constexpr DOMXrayTraits() = default;

  static const XrayType Type = XrayForDOMObject;

  virtual bool resolveOwnProperty(
      JSContext* cx, JS::HandleObject wrapper, JS::HandleObject target,
      JS::HandleObject holder, JS::HandleId id,
      JS::MutableHandle<JS::PropertyDescriptor> desc) override;

  bool delete_(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
               JS::ObjectOpResult& result);

  bool defineProperty(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
                      JS::Handle<JS::PropertyDescriptor> desc,
                      JS::Handle<JS::PropertyDescriptor> existingDesc,
                      JS::ObjectOpResult& result, bool* defined);
  virtual bool enumerateNames(JSContext* cx, JS::HandleObject wrapper,
                              unsigned flags, JS::MutableHandleIdVector props);
  static bool call(JSContext* cx, JS::HandleObject wrapper,
                   const JS::CallArgs& args, const js::Wrapper& baseInstance);
  static bool construct(JSContext* cx, JS::HandleObject wrapper,
                        const JS::CallArgs& args,
                        const js::Wrapper& baseInstance);

  static bool getPrototype(JSContext* cx, JS::HandleObject wrapper,
                           JS::HandleObject target,
                           JS::MutableHandleObject protop);

  virtual void preserveWrapper(JSObject* target) override;

  virtual JSObject* createHolder(JSContext* cx, JSObject* wrapper) override;

  static DOMXrayTraits singleton;

 protected:
  virtual const JSClass* getExpandoClass(
      JSContext* cx, JS::HandleObject target) const override;
};

class JSXrayTraits : public XrayTraits {
 public:
  static const XrayType Type = XrayForJSObject;

  virtual bool resolveOwnProperty(
      JSContext* cx, JS::HandleObject wrapper, JS::HandleObject target,
      JS::HandleObject holder, JS::HandleId id,
      JS::MutableHandle<JS::PropertyDescriptor> desc) override;

  bool delete_(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
               JS::ObjectOpResult& result);

  bool defineProperty(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
                      JS::Handle<JS::PropertyDescriptor> desc,
                      JS::Handle<JS::PropertyDescriptor> existingDesc,
                      JS::ObjectOpResult& result, bool* defined);

  virtual bool enumerateNames(JSContext* cx, JS::HandleObject wrapper,
                              unsigned flags, JS::MutableHandleIdVector props);

  static bool call(JSContext* cx, JS::HandleObject wrapper,
                   const JS::CallArgs& args, const js::Wrapper& baseInstance) {
    JSXrayTraits& self = JSXrayTraits::singleton;
    JS::RootedObject holder(cx, self.ensureHolder(cx, wrapper));
    if (xpc::JSXrayTraits::getProtoKey(holder) == JSProto_Function) {
      return baseInstance.call(cx, wrapper, args);
    }

    JS::RootedValue v(cx, JS::ObjectValue(*wrapper));
    js::ReportIsNotFunction(cx, v);
    return false;
  }

  static bool construct(JSContext* cx, JS::HandleObject wrapper,
                        const JS::CallArgs& args,
                        const js::Wrapper& baseInstance);

  bool getPrototype(JSContext* cx, JS::HandleObject wrapper,
                    JS::HandleObject target, JS::MutableHandleObject protop) {
    JS::RootedObject holder(cx, ensureHolder(cx, wrapper));
    JSProtoKey key = getProtoKey(holder);
    if (isPrototype(holder)) {
      JSProtoKey protoKey = js::InheritanceProtoKeyForStandardClass(key);
      if (protoKey == JSProto_Null) {
        protop.set(nullptr);
        return true;
      }
      key = protoKey;
    }

    {
      JSAutoRealm ar(cx, target);
      if (!JS_GetClassPrototype(cx, key, protop)) {
        return false;
      }
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
    SLOT_PROTOKEY = HOLDER_SHARED_SLOT_COUNT,
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
  static bool getOwnPropertyFromWrapperIfSafe(
      JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
      JS::MutableHandle<JS::PropertyDescriptor> desc);

  // Like the above, but operates in the target compartment. wrapperGlobal is
  // the caller's global (must be in the wrapper compartment).
  static bool getOwnPropertyFromTargetIfSafe(
      JSContext* cx, JS::HandleObject target, JS::HandleObject wrapper,
      JS::HandleObject wrapperGlobal, JS::HandleId id,
      JS::MutableHandle<JS::PropertyDescriptor> desc);

  static const JSClass HolderClass;
  static JSXrayTraits singleton;
};

// These traits are used when the target is not Xrayable and we therefore want
// to make it opaque modulo the usual Xray machinery (like expandos and
// .wrappedJSObject).
class OpaqueXrayTraits : public XrayTraits {
 public:
  static const XrayType Type = XrayForOpaqueObject;

  virtual bool resolveOwnProperty(
      JSContext* cx, JS::HandleObject wrapper, JS::HandleObject target,
      JS::HandleObject holder, JS::HandleId id,
      JS::MutableHandle<JS::PropertyDescriptor> desc) override;

  bool defineProperty(JSContext* cx, JS::HandleObject wrapper, JS::HandleId id,
                      JS::Handle<JS::PropertyDescriptor> desc,
                      JS::Handle<JS::PropertyDescriptor> existingDesc,
                      JS::ObjectOpResult& result, bool* defined) {
    *defined = false;
    return true;
  }

  virtual bool enumerateNames(JSContext* cx, JS::HandleObject wrapper,
                              unsigned flags, JS::MutableHandleIdVector props) {
    return true;
  }

  static bool call(JSContext* cx, JS::HandleObject wrapper,
                   const JS::CallArgs& args, const js::Wrapper& baseInstance) {
    JS::RootedValue v(cx, JS::ObjectValue(*wrapper));
    js::ReportIsNotFunction(cx, v);
    return false;
  }

  static bool construct(JSContext* cx, JS::HandleObject wrapper,
                        const JS::CallArgs& args,
                        const js::Wrapper& baseInstance) {
    JS::RootedValue v(cx, JS::ObjectValue(*wrapper));
    js::ReportIsNotFunction(cx, v);
    return false;
  }

  bool getPrototype(JSContext* cx, JS::HandleObject wrapper,
                    JS::HandleObject target, JS::MutableHandleObject protop) {
    // Opaque wrappers just get targetGlobal.Object.prototype as their
    // prototype. This is preferable to using a null prototype because it
    // lets things like |toString| and |__proto__| work.
    {
      JSAutoRealm ar(cx, target);
      if (!JS_GetClassPrototype(cx, JSProto_Object, protop)) {
        return false;
      }
    }
    return JS_WrapObject(cx, protop);
  }

  static bool getBuiltinClass(JSContext* cx, JS::HandleObject wrapper,
                              const js::Wrapper& baseInstance,
                              js::ESClass* cls) {
    *cls = js::ESClass::Other;
    return true;
  }

  static const char* className(JSContext* cx, JS::HandleObject wrapper,
                               const js::Wrapper& baseInstance) {
    return "Opaque";
  }

  virtual void preserveWrapper(JSObject* target) override {}

  virtual JSObject* createHolder(JSContext* cx, JSObject* wrapper) override {
    return JS_NewObjectWithGivenProto(cx, &HolderClass, nullptr);
  }

  static OpaqueXrayTraits singleton;
};

XrayType GetXrayType(JSObject* obj);
XrayTraits* GetXrayTraits(JSObject* obj);

template <typename Base, typename Traits>
class XrayWrapper : public Base {
  static_assert(mozilla::IsBaseOf<js::BaseProxyHandler, Base>::value,
                "Base *must* derive from js::BaseProxyHandler");

 public:
  constexpr explicit XrayWrapper(unsigned flags)
      : Base(flags | WrapperFactory::IS_XRAY_WRAPPER_FLAG,
             /* aHasPrototype = */ true){};

  /* Standard internal methods. */
  virtual bool getOwnPropertyDescriptor(
      JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<jsid> id,
      JS::MutableHandle<JS::PropertyDescriptor> desc) const override;
  virtual bool defineProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                              JS::Handle<jsid> id,
                              JS::Handle<JS::PropertyDescriptor> desc,
                              JS::ObjectOpResult& result) const override;
  virtual bool ownPropertyKeys(JSContext* cx, JS::Handle<JSObject*> wrapper,
                               JS::MutableHandleIdVector props) const override;
  virtual bool delete_(JSContext* cx, JS::Handle<JSObject*> wrapper,
                       JS::Handle<jsid> id,
                       JS::ObjectOpResult& result) const override;
  virtual bool enumerate(JSContext* cx, JS::Handle<JSObject*> wrapper,
                         JS::MutableHandleIdVector props) const override;
  virtual bool getPrototype(JSContext* cx, JS::HandleObject wrapper,
                            JS::MutableHandleObject protop) const override;
  virtual bool setPrototype(JSContext* cx, JS::HandleObject wrapper,
                            JS::HandleObject proto,
                            JS::ObjectOpResult& result) const override;
  virtual bool getPrototypeIfOrdinary(
      JSContext* cx, JS::HandleObject wrapper, bool* isOrdinary,
      JS::MutableHandleObject protop) const override;
  virtual bool setImmutablePrototype(JSContext* cx, JS::HandleObject wrapper,
                                     bool* succeeded) const override;
  virtual bool preventExtensions(JSContext* cx, JS::Handle<JSObject*> wrapper,
                                 JS::ObjectOpResult& result) const override;
  virtual bool isExtensible(JSContext* cx, JS::Handle<JSObject*> wrapper,
                            bool* extensible) const override;
  virtual bool has(JSContext* cx, JS::Handle<JSObject*> wrapper,
                   JS::Handle<jsid> id, bool* bp) const override;
  virtual bool get(JSContext* cx, JS::Handle<JSObject*> wrapper,
                   JS::HandleValue receiver, JS::Handle<jsid> id,
                   JS::MutableHandle<JS::Value> vp) const override;
  virtual bool set(JSContext* cx, JS::Handle<JSObject*> wrapper,
                   JS::Handle<jsid> id, JS::Handle<JS::Value> v,
                   JS::Handle<JS::Value> receiver,
                   JS::ObjectOpResult& result) const override;
  virtual bool call(JSContext* cx, JS::Handle<JSObject*> wrapper,
                    const JS::CallArgs& args) const override;
  virtual bool construct(JSContext* cx, JS::Handle<JSObject*> wrapper,
                         const JS::CallArgs& args) const override;

  /* SpiderMonkey extensions. */
  virtual bool hasOwn(JSContext* cx, JS::Handle<JSObject*> wrapper,
                      JS::Handle<jsid> id, bool* bp) const override;
  virtual bool getOwnEnumerablePropertyKeys(
      JSContext* cx, JS::Handle<JSObject*> wrapper,
      JS::MutableHandleIdVector props) const override;

  virtual bool getBuiltinClass(JSContext* cx, JS::HandleObject wapper,
                               js::ESClass* cls) const override;
  virtual bool hasInstance(JSContext* cx, JS::HandleObject wrapper,
                           JS::MutableHandleValue v, bool* bp) const override;
  virtual const char* className(JSContext* cx,
                                JS::HandleObject proxy) const override;

  static const XrayWrapper singleton;

 protected:
  bool getPropertyKeys(JSContext* cx, JS::Handle<JSObject*> wrapper,
                       unsigned flags, JS::MutableHandleIdVector props) const;
};

#define PermissiveXrayDOM \
  xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::DOMXrayTraits>
#define PermissiveXrayJS \
  xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::JSXrayTraits>
#define PermissiveXrayOpaque \
  xpc::XrayWrapper<js::CrossCompartmentWrapper, xpc::OpaqueXrayTraits>

extern template class PermissiveXrayDOM;
extern template class PermissiveXrayJS;
extern template class PermissiveXrayOpaque;

/*
 * Slots for Xray expando objects.  See comments in XrayWrapper.cpp for details
 * of how these get used; we mostly want the value of JSSLOT_EXPANDO_COUNT here.
 */
enum ExpandoSlots {
  JSSLOT_EXPANDO_NEXT = 0,
  JSSLOT_EXPANDO_ORIGIN,
  JSSLOT_EXPANDO_EXCLUSIVE_WRAPPER_HOLDER,
  JSSLOT_EXPANDO_PROTOTYPE,
  JSSLOT_EXPANDO_COUNT
};

extern const JSClassOps XrayExpandoObjectClassOps;

/*
 * Clear the given slot on all Xray expandos for the given object.
 *
 * No-op when called on non-main threads (where Xrays don't exist).
 */
void ClearXrayExpandoSlots(JSObject* target, size_t slotIndex);

/*
 * Ensure the given wrapper has an expando object and return it.  This can
 * return null on failure.  Will only be called when "wrapper" is an Xray for a
 * DOM object.
 */
JSObject* EnsureXrayExpandoObject(JSContext* cx, JS::HandleObject wrapper);

// Information about xrays for use by the JITs.
extern js::XrayJitInfo gXrayJitInfo;

}  // namespace xpc

#endif
