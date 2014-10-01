/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMJSProxyHandler_h
#define mozilla_dom_DOMJSProxyHandler_h

#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"

#include "jsapi.h"
#include "jsproxy.h"
#include "nsString.h"

#define DOM_PROXY_OBJECT_SLOT js::PROXY_PRIVATE_SLOT

namespace mozilla {
namespace dom {

enum {
  /**
   * DOM proxies have an extra slot for the expando object at index
   * JSPROXYSLOT_EXPANDO.
   *
   * The expando object is a plain JSObject whose properties correspond to
   * "expandos" (custom properties set by the script author).
   *
   * The exact value stored in the JSPROXYSLOT_EXPANDO slot depends on whether
   * the interface is annotated with the [OverrideBuiltins] extended attribute.
   *
   * If it is, the proxy is initialized with a PrivateValue, which contains a
   * pointer to a js::ExpandoAndGeneration object; this contains a pointer to
   * the actual expando object as well as the "generation" of the object.
   *
   * If it is not, the proxy is initialized with an UndefinedValue. In
   * EnsureExpandoObject, it is set to an ObjectValue that points to the
   * expando object directly. (It is set back to an UndefinedValue only when
   * the object is about to die.)
   */
  JSPROXYSLOT_EXPANDO = 0
};

template<typename T> struct Prefable;

class BaseDOMProxyHandler : public js::BaseProxyHandler
{
public:
  explicit MOZ_CONSTEXPR BaseDOMProxyHandler(const void* aProxyFamily, bool aHasPrototype = false)
    : js::BaseProxyHandler(aProxyFamily, aHasPrototype)
  {}

  // Implementations of traps that can be implemented in terms of
  // fundamental traps.
  bool enumerate(JSContext* cx, JS::Handle<JSObject*> proxy,
                 JS::AutoIdVector& props) const MOZ_OVERRIDE;
  bool getPropertyDescriptor(JSContext* cx, JS::Handle<JSObject*> proxy,
                             JS::Handle<jsid> id,
                             JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;
  bool getOwnPropertyDescriptor(JSContext* cx, JS::Handle<JSObject*> proxy,
                                JS::Handle<jsid> id,
                                JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE;

  bool watch(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
             JS::Handle<JSObject*> callable) const MOZ_OVERRIDE;
  bool unwatch(JSContext* cx, JS::Handle<JSObject*> proxy,
               JS::Handle<jsid> id) const MOZ_OVERRIDE;
  virtual bool getOwnPropertyNames(JSContext* cx, JS::Handle<JSObject*> proxy,
                                   JS::AutoIdVector &props) const MOZ_OVERRIDE;
  // We override keys() and implement it directly instead of using the
  // default implementation, which would getOwnPropertyNames and then
  // filter out the non-enumerable ones.  This avoids doing
  // unnecessary work during enumeration.
  virtual bool keys(JSContext* cx, JS::Handle<JSObject*> proxy,
                    JS::AutoIdVector &props) const MOZ_OVERRIDE;

protected:
  // Hook for subclasses to implement shared getOwnPropertyNames()/keys()
  // functionality.  The "flags" argument is either JSITER_OWNONLY (for keys())
  // or JSITER_OWNONLY | JSITER_HIDDEN (for getOwnPropertyNames()).
  virtual bool ownPropNames(JSContext* cx, JS::Handle<JSObject*> proxy,
                            unsigned flags,
                            JS::AutoIdVector& props) const = 0;

  // Hook for subclasses to allow set() to ignore named props while other things
  // that look at property descriptors see them.  This is intentionally not
  // named getOwnPropertyDescriptor to avoid subclasses that override it hiding
  // our public getOwnPropertyDescriptor.
  virtual bool getOwnPropDescriptor(JSContext* cx,
                                    JS::Handle<JSObject*> proxy,
                                    JS::Handle<jsid> id,
                                    bool ignoreNamedProps,
                                    JS::MutableHandle<JSPropertyDescriptor> desc) const = 0;
};

class DOMProxyHandler : public BaseDOMProxyHandler
{
public:
  MOZ_CONSTEXPR DOMProxyHandler()
    : BaseDOMProxyHandler(&family)
  {}

  bool preventExtensions(JSContext *cx, JS::Handle<JSObject*> proxy) const MOZ_OVERRIDE;
  bool defineProperty(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                      JS::MutableHandle<JSPropertyDescriptor> desc) const MOZ_OVERRIDE
  {
    bool unused;
    return defineProperty(cx, proxy, id, desc, &unused);
  }
  virtual bool defineProperty(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                              JS::MutableHandle<JSPropertyDescriptor> desc, bool* defined)
                              const;
  bool set(JSContext *cx, JS::Handle<JSObject*> proxy, JS::Handle<JSObject*> receiver,
           JS::Handle<jsid> id, bool strict, JS::MutableHandle<JS::Value> vp)
           const MOZ_OVERRIDE;
  bool delete_(JSContext* cx, JS::Handle<JSObject*> proxy,
               JS::Handle<jsid> id, bool* bp) const MOZ_OVERRIDE;
  bool has(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
           bool* bp) const MOZ_OVERRIDE;
  bool isExtensible(JSContext *cx, JS::Handle<JSObject*> proxy, bool *extensible)
                    const MOZ_OVERRIDE;

  /*
   * If assigning to proxy[id] hits a named setter with OverrideBuiltins or
   * an indexed setter, call it and set *done to true on success. Otherwise, set
   * *done to false.
   */
  virtual bool setCustom(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                         JS::MutableHandle<JS::Value> vp, bool *done) const;

  static JSObject* GetExpandoObject(JSObject* obj);

  /* GetAndClearExpandoObject does not DROP or clear the preserving wrapper flag. */
  static JSObject* GetAndClearExpandoObject(JSObject* obj);
  static JSObject* EnsureExpandoObject(JSContext* cx,
                                       JS::Handle<JSObject*> obj);

  static const char family;
};

inline bool IsDOMProxy(JSObject *obj)
{
    const js::Class* clasp = js::GetObjectClass(obj);
    return clasp->isProxy() &&
           js::GetProxyHandler(obj)->family() == &DOMProxyHandler::family;
}

inline const DOMProxyHandler*
GetDOMProxyHandler(JSObject* obj)
{
  MOZ_ASSERT(IsDOMProxy(obj));
  return static_cast<const DOMProxyHandler*>(js::GetProxyHandler(obj));
}

extern jsid s_length_id;

int32_t IdToInt32(JSContext* cx, JS::Handle<jsid> id);

// XXXbz this should really return uint32_t, with the maximum value
// meaning "not an index"...
inline int32_t
GetArrayIndexFromId(JSContext* cx, JS::Handle<jsid> id)
{
  if (MOZ_LIKELY(JSID_IS_INT(id))) {
    return JSID_TO_INT(id);
  }
  if (MOZ_LIKELY(id == s_length_id)) {
    return -1;
  }
  if (MOZ_LIKELY(JSID_IS_ATOM(id))) {
    JSAtom* atom = JSID_TO_ATOM(id);
    char16_t s;
    {
      JS::AutoCheckCannotGC nogc;
      if (js::AtomHasLatin1Chars(atom)) {
        s = *js::GetLatin1AtomChars(nogc, atom);
      } else {
        s = *js::GetTwoByteAtomChars(nogc, atom);
      }
    }
    if (MOZ_LIKELY((unsigned)s >= 'a' && (unsigned)s <= 'z'))
      return -1;

    uint32_t i;
    JSLinearString* str = js::AtomToLinearString(JSID_TO_ATOM(id));
    return js::StringIsArrayIndex(str, &i) ? i : -1;
  }
  return IdToInt32(cx, id);
}

inline bool
IsArrayIndex(int32_t index)
{
  return index >= 0;
}

inline void
FillPropertyDescriptor(JS::MutableHandle<JSPropertyDescriptor> desc,
                       JSObject* obj, bool readonly, bool enumerable = true)
{
  desc.object().set(obj);
  desc.setAttributes((readonly ? JSPROP_READONLY : 0) |
                     (enumerable ? JSPROP_ENUMERATE : 0));
  desc.setGetter(nullptr);
  desc.setSetter(nullptr);
}

inline void
FillPropertyDescriptor(JS::MutableHandle<JSPropertyDescriptor> desc,
                       JSObject* obj, JS::Value v,
                       bool readonly, bool enumerable = true)
{
  desc.value().set(v);
  FillPropertyDescriptor(desc, obj, readonly, enumerable);
}

inline void
FillPropertyDescriptor(JS::MutableHandle<JSPropertyDescriptor> desc,
                       JSObject* obj, unsigned attributes, JS::Value v)
{
  desc.object().set(obj);
  desc.value().set(v);
  desc.setAttributes(attributes);
  desc.setGetter(nullptr);
  desc.setSetter(nullptr);
}

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_DOMProxyHandler_h */
