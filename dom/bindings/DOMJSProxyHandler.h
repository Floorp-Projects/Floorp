/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMJSProxyHandler_h
#define mozilla_dom_DOMJSProxyHandler_h

#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"

#include "jsapi.h"
#include "js/Proxy.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

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
 * pointer to a js::ExpandoAndGeneration object; this contains a pointer to
 * the actual expando object as well as the "generation" of the object.  The
 * proxy handler will trace the expando object stored in the
 * js::ExpandoAndGeneration while the proxy itself is alive.
 *
 * If it is not, the proxy is initialized with an UndefinedValue. In
 * EnsureExpandoObject, it is set to an ObjectValue that points to the
 * expando object directly. (It is set back to an UndefinedValue only when
 * the object is about to die.)
 */

template <typename T>
struct Prefable;

class BaseDOMProxyHandler : public js::BaseProxyHandler {
 public:
  explicit constexpr BaseDOMProxyHandler(const void* aProxyFamily,
                                         bool aHasPrototype = false)
      : js::BaseProxyHandler(aProxyFamily, aHasPrototype) {}

  // Implementations of methods that can be implemented in terms of
  // other lower-level methods.
  bool getOwnPropertyDescriptor(
      JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
      JS::MutableHandle<JS::PropertyDescriptor> desc) const override;
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
      JS::MutableHandle<JS::PropertyDescriptor> desc) const = 0;
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
                              JS::ObjectOpResult& result, bool* defined) const;
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
// This handles tracing the expando in ExpandoAndGeneration.
class ShadowingDOMProxyHandler : public DOMProxyHandler {
  virtual void trace(JSTracer* trc, JSObject* proxy) const override;
};

inline bool IsDOMProxy(JSObject* obj) {
  const js::Class* clasp = js::GetObjectClass(obj);
  return clasp->isProxy() &&
         js::GetProxyHandler(obj)->family() == &DOMProxyHandler::family;
}

inline const DOMProxyHandler* GetDOMProxyHandler(JSObject* obj) {
  MOZ_ASSERT(IsDOMProxy(obj));
  return static_cast<const DOMProxyHandler*>(js::GetProxyHandler(obj));
}

extern jsid s_length_id;

// A return value of UINT32_MAX indicates "not an array index".  Note, in
// particular, that UINT32_MAX itself is not a valid array index in general.
inline uint32_t GetArrayIndexFromId(JS::Handle<jsid> id) {
  // Much like js::IdIsIndex, except with a fast path for "length" and another
  // fast path for starting with a lowercase ascii char.  Is that second one
  // really needed?  I guess it is because StringIsArrayIndex is out of line...
  // as of now, use id.get() instead of id otherwise operands mismatch error
  // occurs.
  if (MOZ_LIKELY(JSID_IS_INT(id))) {
    return JSID_TO_INT(id);
  }
  if (MOZ_LIKELY(id.get() == s_length_id)) {
    return UINT32_MAX;
  }
  if (MOZ_UNLIKELY(!JSID_IS_ATOM(id))) {
    return UINT32_MAX;
  }

  JSLinearString* str = js::AtomToLinearString(JSID_TO_ATOM(id));
  char16_t s;
  {
    JS::AutoCheckCannotGC nogc;
    if (js::LinearStringHasLatin1Chars(str)) {
      s = *js::GetLatin1LinearStringChars(nogc, str);
    } else {
      s = *js::GetTwoByteLinearStringChars(nogc, str);
    }
  }
  if (MOZ_LIKELY((unsigned)s >= 'a' && (unsigned)s <= 'z')) return UINT32_MAX;

  uint32_t i;
  return js::StringIsArrayIndex(str, &i) ? i : UINT32_MAX;
}

inline bool IsArrayIndex(uint32_t index) { return index < UINT32_MAX; }

inline void FillPropertyDescriptor(
    JS::MutableHandle<JS::PropertyDescriptor> desc, JSObject* obj,
    bool readonly, bool enumerable = true) {
  desc.object().set(obj);
  desc.setAttributes((readonly ? JSPROP_READONLY : 0) |
                     (enumerable ? JSPROP_ENUMERATE : 0));
  desc.setGetter(nullptr);
  desc.setSetter(nullptr);
}

inline void FillPropertyDescriptor(
    JS::MutableHandle<JS::PropertyDescriptor> desc, JSObject* obj,
    const JS::Value& v, bool readonly, bool enumerable = true) {
  desc.value().set(v);
  FillPropertyDescriptor(desc, obj, readonly, enumerable);
}

inline void FillPropertyDescriptor(
    JS::MutableHandle<JS::PropertyDescriptor> desc, JSObject* obj,
    unsigned attributes, const JS::Value& v) {
  desc.object().set(obj);
  desc.value().set(v);
  desc.setAttributes(attributes);
  desc.setGetter(nullptr);
  desc.setSetter(nullptr);
}

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_DOMProxyHandler_h */
