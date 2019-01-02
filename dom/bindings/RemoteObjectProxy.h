/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteObjectProxy_h
#define mozilla_dom_RemoteObjectProxy_h

#include "js/Proxy.h"
#include "mozilla/dom/PrototypeList.h"
#include "xpcpublic.h"

namespace mozilla {
namespace dom {

/**
 * Base class for RemoteObjectProxy. Implements the pieces of the handler that
 * don't depend on properties/methods of the specific WebIDL interface that this
 * proxy implements.
 */
class RemoteObjectProxyBase : public js::BaseProxyHandler {
 protected:
  explicit constexpr RemoteObjectProxyBase(prototypes::ID aPrototypeID)
      : BaseProxyHandler(&sCrossOriginProxyFamily, false),
        mPrototypeID(aPrototypeID) {}

 public:
  bool finalizeInBackground(const JS::Value& priv) const final { return false; }

  // Standard internal methods
  bool getOwnPropertyDescriptor(
      JSContext* aCx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
      JS::MutableHandle<JS::PropertyDescriptor> aDesc) const override;
  bool ownPropertyKeys(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                       JS::AutoIdVector& aProps) const override;
  bool defineProperty(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                      JS::Handle<jsid> aId,
                      JS::Handle<JS::PropertyDescriptor> aDesc,
                      JS::ObjectOpResult& result) const final;
  bool delete_(JSContext* aCx, JS::Handle<JSObject*> aProxy,
               JS::Handle<jsid> aId, JS::ObjectOpResult& aResult) const final;

  bool getPrototype(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                    JS::MutableHandle<JSObject*> aProtop) const final;
  bool setPrototype(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                    JS::Handle<JSObject*> aProto,
                    JS::ObjectOpResult& aResult) const final;
  bool getPrototypeIfOrdinary(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                              bool* aIsOrdinary,
                              JS::MutableHandle<JSObject*> aProtop) const final;

  bool preventExtensions(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                         JS::ObjectOpResult& aResult) const final;
  bool isExtensible(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                    bool* aExtensible) const final;

  bool get(JSContext* cx, JS::Handle<JSObject*> aProxy,
           JS::Handle<JS::Value> aReceiver, JS::Handle<jsid> aId,
           JS::MutableHandle<JS::Value> aVp) const final;
  bool set(JSContext* cx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
           JS::Handle<JS::Value> aValue, JS::Handle<JS::Value> aReceiver,
           JS::ObjectOpResult& aResult) const final;

  // SpiderMonkey extensions
  bool hasOwn(JSContext* aCx, JS::Handle<JSObject*> aProxy,
              JS::Handle<jsid> aId, bool* aBp) const override;
  bool getOwnEnumerablePropertyKeys(JSContext* aCx,
                                    JS::Handle<JSObject*> aProxy,
                                    JS::AutoIdVector& aProps) const override;

  bool isCallable(JSObject* aObj) const final { return false; }
  bool isConstructor(JSObject* aObj) const final { return false; }

  static void* GetNative(JSObject* aProxy) {
    return js::GetProxyPrivate(aProxy).toPrivate();
  }

  /**
   * Returns true if aProxy represents an object implementing the WebIDL
   * interface for aProtoID. aProxy should be a proxy object.
   */
  static inline bool IsRemoteObjectProxy(JSObject* aProxy,
                                         prototypes::ID aProtoID) {
    const js::BaseProxyHandler* handler = js::GetProxyHandler(aProxy);
    return handler->family() == &sCrossOriginProxyFamily &&
           static_cast<const RemoteObjectProxyBase*>(handler)->mPrototypeID ==
               aProtoID;
  }

 protected:
  bool getOwnPropertyDescriptorInternal(
      JSContext* aCx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
      JS::MutableHandle<JS::PropertyDescriptor> aDesc) const {
    JS::Rooted<JSObject*> holder(aCx);
    if (!EnsureHolder(aCx, aProxy, &holder) ||
        !JS_GetOwnPropertyDescriptorById(aCx, holder, aId, aDesc)) {
      return false;
    }

    if (aDesc.object()) {
      aDesc.object().set(aProxy);
    }

    return true;
  }

  JSObject* CreateProxyObject(JSContext* aCx, void* aNative,
                              const js::Class* aClasp) const;

  /**
   * Implements the tail of getOwnPropertyDescriptor, dealing in particular with
   * properties that are whitelisted by xpc::IsCrossOriginWhitelistedProp.
   */
  static bool getOwnPropertyDescriptorTail(
      JSContext* aCx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
      JS::MutableHandle<JS::PropertyDescriptor> aDesc);
  static bool ReportCrossOriginDenial(JSContext* aCx, JS::Handle<jsid> aId,
                                      const nsACString& aAccessType);

  /**
   * This gets a cached, or creates and caches, a holder object that contains
   * the WebIDL properties for this proxy.
   */
  bool EnsureHolder(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                    JS::MutableHandle<JSObject*> aHolder) const {
    // FIXME Need to have a holder per realm, should store a weakmap in the
    //       reserved slot.
    JS::Value v = js::GetProxyReservedSlot(aProxy, 0);
    if (v.isObject()) {
      aHolder.set(&v.toObject());
      return true;
    }

    aHolder.set(JS_NewObjectWithGivenProto(aCx, nullptr, nullptr));
    if (!aHolder || !DefinePropertiesAndFunctions(aCx, aHolder)) {
      return false;
    }

    js::SetProxyReservedSlot(aProxy, 0, JS::ObjectValue(*aHolder));
    return true;
  }

  virtual bool DefinePropertiesAndFunctions(
      JSContext* aCx, JS::Handle<JSObject*> aHolder) const = 0;

  const prototypes::ID mPrototypeID;

  static const char sCrossOriginProxyFamily;
};

/**
 * Proxy handler for proxy objects that represent an object implementing a
 * WebIDL interface that has cross-origin accessible properties/methods, and
 * which lives in a different process. The WebIDL code generator will create
 * arrays of cross-origin accessible properties/methods that can be used as
 * arguments to this template.
 *
 * The properties and methods can be cached on a holder JSObject, stored in a
 * reserved slot on the proxy object.
 *
 * The proxy objects that use a handler derived from this one are stored in a
 * hash map in the JS compartment's private (@see
 * xpc::CompartmentPrivate::GetRemoteProxyMap).
 */
template <class Native, JSPropertySpec* P, JSFunctionSpec* F>
class RemoteObjectProxy : public RemoteObjectProxyBase {
 public:
  JSObject* CreateProxyObject(JSContext* aCx, Native* aNative,
                              const js::Class* aClasp) const {
    return RemoteObjectProxyBase::CreateProxyObject(aCx, aNative, aClasp);
  }

 protected:
  using RemoteObjectProxyBase::RemoteObjectProxyBase;

 private:
  bool DefinePropertiesAndFunctions(
      JSContext* aCx, JS::Handle<JSObject*> aHolder) const final {
    return JS_DefineProperties(aCx, aHolder, P) &&
           JS_DefineFunctions(aCx, aHolder, F);
  }
};

/**
 * Returns true if aObj is a proxy object that represents an object implementing
 * the WebIDL interface for aProtoID.
 */
static inline bool IsRemoteObjectProxy(JSObject* aObj,
                                       prototypes::ID aProtoID) {
  if (!js::IsProxy(aObj)) {
    return false;
  }
  return RemoteObjectProxyBase::IsRemoteObjectProxy(aObj, aProtoID);
}

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_RemoteObjectProxy_h */
