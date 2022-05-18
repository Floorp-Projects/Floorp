/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ObservableArrayProxyHandler_h
#define mozilla_dom_ObservableArrayProxyHandler_h

#include "js/TypeDecls.h"
#include "js/Wrapper.h"

namespace mozilla::dom {

/**
 * Proxy handler for observable array exotic object.
 *
 * The indexed properties are stored in the backing list object in reserved slot
 * of the proxy object with special treatment intact.
 *
 * The additional properties are stored in the proxy target object.
 */

class ObservableArrayProxyHandler : public js::ForwardingProxyHandler {
 public:
  explicit constexpr ObservableArrayProxyHandler()
      : js::ForwardingProxyHandler(&family) {}

  // Implementations of methods that can be implemented in terms of
  // other lower-level methods.
  bool defineProperty(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                      JS::Handle<JS::PropertyKey> aId,
                      JS::Handle<JS::PropertyDescriptor> aDesc,
                      JS::ObjectOpResult& aResult) const override;

  bool delete_(JSContext* aCx, JS::Handle<JSObject*> aProxy,
               JS::Handle<JS::PropertyKey> aId,
               JS::ObjectOpResult& aResult) const override;

  bool get(JSContext* aCx, JS::Handle<JSObject*> aProxy,
           JS::Handle<JS::Value> aReceiver, JS::Handle<JS::PropertyKey> aId,
           JS::MutableHandle<JS::Value> aVp) const override;

  bool getOwnPropertyDescriptor(
      JSContext* aCx, JS::Handle<JSObject*> aProxy,
      JS::Handle<JS::PropertyKey> aId,
      JS::MutableHandle<Maybe<JS::PropertyDescriptor>> aDesc) const override;

  bool has(JSContext* aCx, JS::Handle<JSObject*> aProxy,
           JS::Handle<JS::PropertyKey> aId, bool* aBp) const override;

  bool ownPropertyKeys(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                       JS::MutableHandleVector<jsid> aProps) const override;

  bool preventExtensions(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                         JS::ObjectOpResult& aResult) const override;

  bool set(JSContext* aCx, JS::Handle<JSObject*> aProxy,
           JS::Handle<JS::PropertyKey> aId, JS::Handle<JS::Value> aV,
           JS::Handle<JS::Value> aReceiver,
           JS::ObjectOpResult& aResult) const override;

  bool SetLength(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                 uint32_t aLength) const;

  static const char family;

 protected:
  bool GetBackingListObject(
      JSContext* aCx, JS::Handle<JSObject*> aProxy,
      JS::MutableHandle<JSObject*> aBackingListObject) const;

  bool GetBackingListLength(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                            uint32_t* aLength) const;

  bool SetLength(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                 JS::Handle<JSObject*> aBackingList, uint32_t aLength,
                 JS::ObjectOpResult& aResult) const;

  bool SetLength(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                 JS::Handle<JSObject*> aBackingList,
                 JS::Handle<JS::Value> aValue,
                 JS::ObjectOpResult& aResult) const;

  // Hook for subclasses to invoke the setting the indexed value steps which
  // would invoke DeleteAlgorithm/SetAlgorithm defined and implemented per
  // interface. Returns false and throw exception on failure.
  virtual bool SetIndexedValue(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                               JS::Handle<JSObject*> aBackingList,
                               uint32_t aIndex, JS::Handle<JS::Value> aValue,
                               JS::ObjectOpResult& aResult) const = 0;

  // Hook for subclasses to invoke the DeleteAlgorithm defined and implemented
  // per interface. Returns false and throw exception on failure.
  virtual bool OnDeleteItem(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                            JS::Handle<JS::Value> aValue,
                            uint32_t aIndex) const = 0;
};

inline bool IsObservableArrayProxy(JSObject* obj) {
  return js::IsProxy(obj) && js::GetProxyHandler(obj)->family() ==
                                 &ObservableArrayProxyHandler::family;
}

inline const ObservableArrayProxyHandler* GetObservableArrayProxyHandler(
    JSObject* obj) {
  MOZ_ASSERT(IsObservableArrayProxy(obj));
  return static_cast<const ObservableArrayProxyHandler*>(
      js::GetProxyHandler(obj));
}

}  // namespace mozilla::dom

#endif /* mozilla_dom_ObservableArrayProxyHandler_h */
