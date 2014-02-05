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

class DOMClass;

enum {
  JSPROXYSLOT_EXPANDO = 0,
  JSPROXYSLOT_XRAY_EXPANDO
};

template<typename T> struct Prefable;

// This variable exists solely to provide a unique address for use as an identifier.
extern const char HandlerFamily;
inline const void* ProxyFamily() { return &HandlerFamily; }

inline bool IsDOMProxy(JSObject *obj)
{
    const js::Class* clasp = js::GetObjectClass(obj);
    return clasp->isProxy() &&
           js::GetProxyHandler(obj)->family() == ProxyFamily();
}

class BaseDOMProxyHandler : public js::BaseProxyHandler
{
public:
  BaseDOMProxyHandler(const void* aProxyFamily)
    : js::BaseProxyHandler(aProxyFamily)
  {}

  // Implementations of traps that can be implemented in terms of
  // fundamental traps.
  bool enumerate(JSContext* cx, JS::Handle<JSObject*> proxy,
                 JS::AutoIdVector& props) MOZ_OVERRIDE;
  bool getPropertyDescriptor(JSContext* cx, JS::Handle<JSObject*> proxy,
                             JS::Handle<jsid> id,
                             JS::MutableHandle<JSPropertyDescriptor> desc,
                             unsigned flags) MOZ_OVERRIDE;

  bool watch(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
             JS::Handle<JSObject*> callable) MOZ_OVERRIDE;
  bool unwatch(JSContext* cx, JS::Handle<JSObject*> proxy,
               JS::Handle<jsid> id) MOZ_OVERRIDE;
};

class DOMProxyHandler : public BaseDOMProxyHandler
{
public:
  DOMProxyHandler()
    : BaseDOMProxyHandler(ProxyFamily())
  {
  }

  bool preventExtensions(JSContext *cx, JS::Handle<JSObject*> proxy) MOZ_OVERRIDE;
  bool defineProperty(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                      JS::MutableHandle<JSPropertyDescriptor> desc) MOZ_OVERRIDE
  {
    bool unused;
    return defineProperty(cx, proxy, id, desc, &unused);
  }
  virtual bool defineProperty(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                              JS::MutableHandle<JSPropertyDescriptor> desc, bool* defined);
  bool delete_(JSContext* cx, JS::Handle<JSObject*> proxy,
               JS::Handle<jsid> id, bool* bp) MOZ_OVERRIDE;
  bool has(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id, bool* bp) MOZ_OVERRIDE;
  bool isExtensible(JSContext *cx, JS::Handle<JSObject*> proxy, bool *extensible) MOZ_OVERRIDE;

  static JSObject* GetExpandoObject(JSObject* obj)
  {
    MOZ_ASSERT(IsDOMProxy(obj), "expected a DOM proxy object");
    JS::Value v = js::GetProxyExtra(obj, JSPROXYSLOT_EXPANDO);
    if (v.isObject()) {
      return &v.toObject();
    }

    if (v.isUndefined()) {
      return nullptr;
    }

    js::ExpandoAndGeneration* expandoAndGeneration =
      static_cast<js::ExpandoAndGeneration*>(v.toPrivate());
    v = expandoAndGeneration->expando;
    return v.isUndefined() ? nullptr : &v.toObject();
  }
  /* GetAndClearExpandoObject does not DROP or clear the preserving wrapper flag. */
  static JSObject* GetAndClearExpandoObject(JSObject* obj);
  static JSObject* EnsureExpandoObject(JSContext* cx,
                                       JS::Handle<JSObject*> obj);
};

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
    jschar s = *js::GetAtomChars(atom);
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
FillPropertyDescriptor(JS::MutableHandle<JSPropertyDescriptor> desc, JSObject* obj, bool readonly)
{
  desc.object().set(obj);
  desc.setAttributes((readonly ? JSPROP_READONLY : 0) | JSPROP_ENUMERATE);
  desc.setGetter(nullptr);
  desc.setSetter(nullptr);
  desc.setShortId(0);
}

inline void
FillPropertyDescriptor(JS::MutableHandle<JSPropertyDescriptor> desc, JSObject* obj, JS::Value v,
                       bool readonly)
{
  desc.value().set(v);
  FillPropertyDescriptor(desc, obj, readonly);
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
  desc.setShortId(0);
}

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_DOMProxyHandler_h */
