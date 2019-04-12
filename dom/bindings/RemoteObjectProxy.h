/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteObjectProxy_h
#define mozilla_dom_RemoteObjectProxy_h

#include "js/Proxy.h"
#include "mozilla/dom/MaybeCrossOriginObject.h"
#include "mozilla/dom/PrototypeList.h"
#include "xpcpublic.h"

namespace mozilla {
namespace dom {

/**
 * Base class for RemoteObjectProxy. Implements the pieces of the handler that
 * don't depend on properties/methods of the specific WebIDL interface that this
 * proxy implements.
 */
class RemoteObjectProxyBase : public js::BaseProxyHandler,
                              public MaybeCrossOriginObjectMixins {
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
                       JS::MutableHandleVector<jsid> aProps) const override;
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
  bool getOwnEnumerablePropertyKeys(
      JSContext* aCx, JS::Handle<JSObject*> aProxy,
      JS::MutableHandleVector<jsid> aProps) const override;
  const char* className(JSContext* aCx,
                        JS::Handle<JSObject*> aProxy) const final;

  bool isCallable(JSObject* aObj) const final { return false; }
  bool isConstructor(JSObject* aObj) const final { return false; }

  virtual void NoteChildren(JSObject* aProxy,
                            nsCycleCollectionTraversalCallback& aCb) const = 0;

  static void* GetNative(JSObject* aProxy) {
    return js::GetProxyPrivate(aProxy).toPrivate();
  }

  /**
   * Returns true if aProxy is a cross-process proxy that represents
   * an object implementing the WebIDL interface for aProtoID. aProxy
   * should be a proxy object.
   */
  static inline bool IsRemoteObjectProxy(JSObject* aProxy,
                                         prototypes::ID aProtoID) {
    const js::BaseProxyHandler* handler = js::GetProxyHandler(aProxy);
    return handler->family() == &sCrossOriginProxyFamily &&
           static_cast<const RemoteObjectProxyBase*>(handler)->mPrototypeID ==
               aProtoID;
  }

  /**
   * Returns true if aProxy is a cross-process proxy, no matter which
   * interface it represents.  aProxy should be a proxy object.
   */
  static inline bool IsRemoteObjectProxy(JSObject* aProxy) {
    const js::BaseProxyHandler* handler = js::GetProxyHandler(aProxy);
    return handler->family() == &sCrossOriginProxyFamily;
  }

 protected:
  /**
   * Gets an existing cached proxy object, or creates a new one and caches it.
   * aProxy will be null on failure. aNewObjectCreated is set to true if a new
   * object was created, callers probably need to addref the native in that
   * case. aNewObjectCreated can be true even if aProxy is null, if something
   * failed after creating the object.
   */
  void GetOrCreateProxyObject(JSContext* aCx, void* aNative,
                              const js::Class* aClasp,
                              JS::MutableHandle<JSObject*> aProxy,
                              bool& aNewObjectCreated) const;

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
  void finalize(JSFreeOp* aFop, JSObject* aProxy) const final {
    auto native = static_cast<Native*>(GetNative(aProxy));
    RefPtr<Native> self(dont_AddRef(native));
  }

  void GetProxyObject(JSContext* aCx, Native* aNative,
                      JS::MutableHandle<JSObject*> aProxy) const {
    bool objectCreated = false;
    GetOrCreateProxyObject(aCx, aNative, &sClass, aProxy, objectCreated);
    if (objectCreated) {
      NS_ADDREF(aNative);
    }
  }

 protected:
  using RemoteObjectProxyBase::RemoteObjectProxyBase;

 private:
  bool EnsureHolder(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                    JS::MutableHandle<JSObject*> aHolder) const final {
    return MaybeCrossOriginObjectMixins::EnsureHolder(
        aCx, aProxy, /* slot = */ 0, P, F, aHolder);
  }

  static const js::Class sClass;
};

/**
 * Returns true if aObj is a cross-process proxy object that
 * represents an object implementing the WebIDL interface for
 * aProtoID.
 */
static inline bool IsRemoteObjectProxy(JSObject* aObj,
                                       prototypes::ID aProtoID) {
  if (!js::IsProxy(aObj)) {
    return false;
  }
  return RemoteObjectProxyBase::IsRemoteObjectProxy(aObj, aProtoID);
}

/**
 * Returns true if aObj is a cross-process proxy object, no matter
 * which WebIDL interface it corresponds to.
 */
static inline bool IsRemoteObjectProxy(JSObject* aObj) {
  if (!js::IsProxy(aObj)) {
    return false;
  }
  return RemoteObjectProxyBase::IsRemoteObjectProxy(aObj);
}

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_RemoteObjectProxy_h */
