/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMJSProxyHandler_h
#define mozilla_dom_DOMJSProxyHandler_h

#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"

#include "jsapi.h"
#include "js/Object.h"  // JS::GetClass
#include "js/Proxy.h"

namespace mozilla::dom {

/**
 * DOM proxies store the expando object in the private slot.
 *
 * The expando object is a plain JSObject whose properties correspond to
 * "expandos" (custom properties set by the script author).
 *
 * The exact value stored in the proxy's private slot depends on whether the
 * interface is annotated with the [OverrideBuiltins] extended attribute.
 *
 * If it is, the proxy is initialized with a PrivateValue, which contains a
 * pointer to a JS::ExpandoAndGeneration object; this contains a pointer to
 * the actual expando object as well as the "generation" of the object.  The
 * proxy handler will trace the expando object stored in the
 * JS::ExpandoAndGeneration while the proxy itself is alive.
 *
 * If it is not, the proxy is initialized with an UndefinedValue. In
 * EnsureExpandoObject, it is set to an ObjectValue that points to the
 * expando object directly. (It is set back to an UndefinedValue only when
 * the object is about to die.)
 */

class BaseDOMProxyHandler : public js::BaseProxyHandler {
 public:
  explicit constexpr BaseDOMProxyHandler(const void* aProxyFamily,
                                         bool aHasPrototype = false)
      : js::BaseProxyHandler(aProxyFamily, aHasPrototype) {}

  // Implementations of methods that can be implemented in terms of
  // other lower-level methods.
  bool getOwnPropertyDescriptor(
      JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
      JS::MutableHandle<Maybe<JS::PropertyDescriptor>> desc) const override;
  virtual bool ownPropertyKeys(
      JSContext* cx, JS::Handle<JSObject*> proxy,
      JS::MutableHandleVector<jsid> props) const override;

  virtual bool getPrototypeIfOrdinary(
      JSContext* cx, JS::Handle<JSObject*> proxy, bool* isOrdinary,
      JS::MutableHandle<JSObject*> proto) const override;

  // We override getOwnEnumerablePropertyKeys() and implement it directly
  // instead of using the default implementation, which would call
  // ownPropertyKeys and then filter out the non-enumerable ones. This avoids
  // unnecessary work during enumeration.
  virtual bool getOwnEnumerablePropertyKeys(
      JSContext* cx, JS::Handle<JSObject*> proxy,
      JS::MutableHandleVector<jsid> props) const override;

 protected:
  // Hook for subclasses to implement shared ownPropertyKeys()/keys()
  // functionality.  The "flags" argument is either JSITER_OWNONLY (for keys())
  // or JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS (for
  // ownPropertyKeys()).
  virtual bool ownPropNames(JSContext* cx, JS::Handle<JSObject*> proxy,
                            unsigned flags,
                            JS::MutableHandleVector<jsid> props) const = 0;

  // Hook for subclasses to allow set() to ignore named props while other things
  // that look at property descriptors see them.  This is intentionally not
  // named getOwnPropertyDescriptor to avoid subclasses that override it hiding
  // our public getOwnPropertyDescriptor.
  virtual bool getOwnPropDescriptor(
      JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
      bool ignoreNamedProps,
      JS::MutableHandle<Maybe<JS::PropertyDescriptor>> desc) const = 0;
};

class DOMProxyHandler : public BaseDOMProxyHandler {
 public:
  constexpr DOMProxyHandler() : BaseDOMProxyHandler(&family) {}

  bool defineProperty(JSContext* cx, JS::Handle<JSObject*> proxy,
                      JS::Handle<jsid> id,
                      JS::Handle<JS::PropertyDescriptor> desc,
                      JS::ObjectOpResult& result) const override {
    bool unused;
    return defineProperty(cx, proxy, id, desc, result, &unused);
  }
  virtual bool defineProperty(JSContext* cx, JS::Handle<JSObject*> proxy,
                              JS::Handle<jsid> id,
                              JS::Handle<JS::PropertyDescriptor> desc,
                              JS::ObjectOpResult& result, bool* done) const;
  bool delete_(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
               JS::ObjectOpResult& result) const override;
  bool preventExtensions(JSContext* cx, JS::Handle<JSObject*> proxy,
                         JS::ObjectOpResult& result) const override;
  bool isExtensible(JSContext* cx, JS::Handle<JSObject*> proxy,
                    bool* extensible) const override;
  bool set(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
           JS::Handle<JS::Value> v, JS::Handle<JS::Value> receiver,
           JS::ObjectOpResult& result) const override;

  /*
   * If assigning to proxy[id] hits a named setter with OverrideBuiltins or
   * an indexed setter, call it and set *done to true on success. Otherwise, set
   * *done to false.
   */
  virtual bool setCustom(JSContext* cx, JS::Handle<JSObject*> proxy,
                         JS::Handle<jsid> id, JS::Handle<JS::Value> v,
                         bool* done) const;

  /*
   * Get the expando object for the given DOM proxy.
   */
  static JSObject* GetExpandoObject(JSObject* obj);

  /*
   * Clear the expando object for the given DOM proxy and return it.  This
   * function will ensure that the returned object is exposed to active JS if
   * the given object is exposed.
   *
   * GetAndClearExpandoObject does not DROP or clear the preserving wrapper
   * flag.
   */
  static JSObject* GetAndClearExpandoObject(JSObject* obj);

  /*
   * Ensure that the given proxy (obj) has an expando object, and return it.
   * Returns null on failure.
   */
  static JSObject* EnsureExpandoObject(JSContext* cx,
                                       JS::Handle<JSObject*> obj);

  static const char family;
};

// Class used by shadowing handlers (the ones that have [OverrideBuiltins].
// This handles tracing the expando in JS::ExpandoAndGeneration.
class ShadowingDOMProxyHandler : public DOMProxyHandler {
  virtual void trace(JSTracer* trc, JSObject* proxy) const override;
};

inline bool IsDOMProxy(JSObject* obj) {
  return js::IsProxy(obj) &&
         js::GetProxyHandler(obj)->family() == &DOMProxyHandler::family;
}

inline const DOMProxyHandler* GetDOMProxyHandler(JSObject* obj) {
  MOZ_ASSERT(IsDOMProxy(obj));
  return static_cast<const DOMProxyHandler*>(js::GetProxyHandler(obj));
}

}  // namespace mozilla::dom

#endif /* mozilla_dom_DOMProxyHandler_h */
