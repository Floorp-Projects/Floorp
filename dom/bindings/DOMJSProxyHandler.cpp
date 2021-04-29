/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DOMJSProxyHandler.h"
#include "xpcpublic.h"
#include "xpcprivate.h"
#include "XPCWrapper.h"
#include "WrapperFactory.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/BindingUtils.h"

#include "jsapi.h"
#include "js/friend/DOMProxy.h"  // JS::DOMProxyShadowsResult, JS::ExpandoAndGeneration, JS::SetDOMProxyInformation
#include "js/Object.h"           // JS::GetCompartment

using namespace JS;

namespace mozilla::dom {

jsid s_length_id = JSID_VOID;

bool DefineStaticJSVals(JSContext* cx) {
  return AtomizeAndPinJSString(cx, s_length_id, "length");
}

const char DOMProxyHandler::family = 0;

JS::DOMProxyShadowsResult DOMProxyShadows(JSContext* cx,
                                          JS::Handle<JSObject*> proxy,
                                          JS::Handle<jsid> id) {
  using DOMProxyShadowsResult = JS::DOMProxyShadowsResult;

  JS::Rooted<JSObject*> expando(cx, DOMProxyHandler::GetExpandoObject(proxy));
  JS::Value v = js::GetProxyPrivate(proxy);
  bool isOverrideBuiltins = !v.isObject() && !v.isUndefined();
  if (expando) {
    bool hasOwn;
    if (!JS_AlreadyHasOwnPropertyById(cx, expando, id, &hasOwn))
      return DOMProxyShadowsResult::ShadowCheckFailed;

    if (hasOwn) {
      return isOverrideBuiltins
                 ? DOMProxyShadowsResult::ShadowsViaIndirectExpando
                 : DOMProxyShadowsResult::ShadowsViaDirectExpando;
    }
  }

  if (!isOverrideBuiltins) {
    // Our expando, if any, didn't shadow, so we're not shadowing at all.
    return DOMProxyShadowsResult::DoesntShadow;
  }

  bool hasOwn;
  if (!GetProxyHandler(proxy)->hasOwn(cx, proxy, id, &hasOwn))
    return DOMProxyShadowsResult::ShadowCheckFailed;

  return hasOwn ? DOMProxyShadowsResult::Shadows
                : DOMProxyShadowsResult::DoesntShadowUnique;
}

// Store the information for the specialized ICs.
struct SetDOMProxyInformation {
  SetDOMProxyInformation() {
    JS::SetDOMProxyInformation((const void*)&DOMProxyHandler::family,
                               DOMProxyShadows,
                               &RemoteObjectProxyBase::sCrossOriginProxyFamily);
  }
};

SetDOMProxyInformation gSetDOMProxyInformation;

static inline void CheckExpandoObject(JSObject* proxy,
                                      const JS::Value& expando) {
#ifdef DEBUG
  JSObject* obj = &expando.toObject();
  MOZ_ASSERT(!js::gc::EdgeNeedsSweepUnbarriered(&obj));
  MOZ_ASSERT(JS::GetCompartment(proxy) == JS::GetCompartment(obj));

  // When we create an expando object in EnsureExpandoObject below, we preserve
  // the wrapper. The wrapper is released when the object is unlinked, but we
  // should never call these functions after that point.
  nsISupports* native = UnwrapDOMObject<nsISupports>(proxy);
  nsWrapperCache* cache;
  // QueryInterface to nsWrapperCache will not GC.
  JS::AutoSuppressGCAnalysis suppress;
  CallQueryInterface(native, &cache);
  MOZ_ASSERT(cache->PreservingWrapper());
#endif
}

static inline void CheckExpandoAndGeneration(
    JSObject* proxy, JS::ExpandoAndGeneration* expandoAndGeneration) {
#ifdef DEBUG
  JS::Value value = expandoAndGeneration->expando;
  if (!value.isUndefined()) CheckExpandoObject(proxy, value);
#endif
}

static inline void CheckDOMProxy(JSObject* proxy) {
#ifdef DEBUG
  MOZ_ASSERT(IsDOMProxy(proxy), "expected a DOM proxy object");
  MOZ_ASSERT(!js::gc::EdgeNeedsSweepUnbarriered(&proxy));
  nsISupports* native = UnwrapDOMObject<nsISupports>(proxy);
  nsWrapperCache* cache;
  // QI to nsWrapperCache cannot GC for very non-obvious reasons; see
  // https://searchfox.org/mozilla-central/rev/55da592d85c2baf8d8818010c41d9738c97013d2/js/xpconnect/src/XPCWrappedJSClass.cpp#521,545-548
  JS::AutoSuppressGCAnalysis nogc;
  CallQueryInterface(native, &cache);
  MOZ_ASSERT(cache->GetWrapperPreserveColor() == proxy);
#endif
}

// static
JSObject* DOMProxyHandler::GetAndClearExpandoObject(JSObject* obj) {
  CheckDOMProxy(obj);

  JS::Value v = js::GetProxyPrivate(obj);
  if (v.isUndefined()) {
    return nullptr;
  }

  if (v.isObject()) {
    js::SetProxyPrivate(obj, UndefinedValue());
  } else {
    auto* expandoAndGeneration =
        static_cast<JS::ExpandoAndGeneration*>(v.toPrivate());
    v = expandoAndGeneration->expando;
    if (v.isUndefined()) {
      return nullptr;
    }
    expandoAndGeneration->expando = UndefinedValue();
  }

  CheckExpandoObject(obj, v);

  return &v.toObject();
}

// static
JSObject* DOMProxyHandler::EnsureExpandoObject(JSContext* cx,
                                               JS::Handle<JSObject*> obj) {
  CheckDOMProxy(obj);

  JS::Value v = js::GetProxyPrivate(obj);
  if (v.isObject()) {
    CheckExpandoObject(obj, v);
    return &v.toObject();
  }

  JS::ExpandoAndGeneration* expandoAndGeneration = nullptr;
  if (!v.isUndefined()) {
    expandoAndGeneration =
        static_cast<JS::ExpandoAndGeneration*>(v.toPrivate());
    CheckExpandoAndGeneration(obj, expandoAndGeneration);
    if (expandoAndGeneration->expando.isObject()) {
      return &expandoAndGeneration->expando.toObject();
    }
  }

  JS::Rooted<JSObject*> expando(
      cx, JS_NewObjectWithGivenProto(cx, nullptr, nullptr));
  if (!expando) {
    return nullptr;
  }

  nsISupports* native = UnwrapDOMObject<nsISupports>(obj);
  nsWrapperCache* cache;
  CallQueryInterface(native, &cache);
  cache->PreserveWrapper(native);

  if (expandoAndGeneration) {
    expandoAndGeneration->expando.setObject(*expando);
    return expando;
  }

  js::SetProxyPrivate(obj, ObjectValue(*expando));

  return expando;
}

bool DOMProxyHandler::preventExtensions(JSContext* cx,
                                        JS::Handle<JSObject*> proxy,
                                        JS::ObjectOpResult& result) const {
  // always extensible per WebIDL
  return result.failCantPreventExtensions();
}

bool DOMProxyHandler::isExtensible(JSContext* cx, JS::Handle<JSObject*> proxy,
                                   bool* extensible) const {
  *extensible = true;
  return true;
}

bool BaseDOMProxyHandler::getOwnPropertyDescriptor(
    JSContext* cx, Handle<JSObject*> proxy, Handle<jsid> id,
    MutableHandle<Maybe<PropertyDescriptor>> desc) const {
  Rooted<PropertyDescriptor> ownDesc(cx);
  if (!getOwnPropDescriptor(cx, proxy, id, /* ignoreNamedProps = */ false,
                            &ownDesc)) {
    return false;
  }

  if (ownDesc.object()) {
    desc.set(Some(ownDesc.get()));
  } else {
    desc.reset();
  }
  return true;
}

bool DOMProxyHandler::defineProperty(JSContext* cx, JS::Handle<JSObject*> proxy,
                                     JS::Handle<jsid> id,
                                     Handle<PropertyDescriptor> desc,
                                     JS::ObjectOpResult& result,
                                     bool* done) const {
  if (xpc::WrapperFactory::IsXrayWrapper(proxy)) {
    return result.succeed();
  }

  JS::Rooted<JSObject*> expando(cx, EnsureExpandoObject(cx, proxy));
  if (!expando) {
    return false;
  }

  if (!JS_DefinePropertyById(cx, expando, id, desc, result)) {
    return false;
  }
  *done = true;
  return true;
}

bool DOMProxyHandler::set(JSContext* cx, Handle<JSObject*> proxy,
                          Handle<jsid> id, Handle<JS::Value> v,
                          Handle<JS::Value> receiver,
                          ObjectOpResult& result) const {
  MOZ_ASSERT(!xpc::WrapperFactory::IsXrayWrapper(proxy),
             "Should not have a XrayWrapper here");
  bool done;
  if (!setCustom(cx, proxy, id, v, &done)) {
    return false;
  }
  if (done) {
    return result.succeed();
  }

  // Make sure to ignore our named properties when checking for own
  // property descriptors for a set.
  Rooted<PropertyDescriptor> ownDesc_(cx);
  if (!getOwnPropDescriptor(cx, proxy, id, /* ignoreNamedProps = */ true,
                            &ownDesc_)) {
    return false;
  }

  Rooted<Maybe<PropertyDescriptor>> ownDesc(cx);
  if (ownDesc_.object()) {
    ownDesc.set(mozilla::Some(ownDesc_.get()));
  } else {
    ownDesc.reset();
  }

  return js::SetPropertyIgnoringNamedGetter(cx, proxy, id, v, receiver, ownDesc,
                                            result);
}

bool DOMProxyHandler::delete_(JSContext* cx, JS::Handle<JSObject*> proxy,
                              JS::Handle<jsid> id,
                              JS::ObjectOpResult& result) const {
  JS::Rooted<JSObject*> expando(cx);
  if (!xpc::WrapperFactory::IsXrayWrapper(proxy) &&
      (expando = GetExpandoObject(proxy))) {
    return JS_DeletePropertyById(cx, expando, id, result);
  }

  return result.succeed();
}

bool BaseDOMProxyHandler::ownPropertyKeys(
    JSContext* cx, JS::Handle<JSObject*> proxy,
    JS::MutableHandleVector<jsid> props) const {
  return ownPropNames(cx, proxy,
                      JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, props);
}

bool BaseDOMProxyHandler::getPrototypeIfOrdinary(
    JSContext* cx, JS::Handle<JSObject*> proxy, bool* isOrdinary,
    JS::MutableHandle<JSObject*> proto) const {
  *isOrdinary = true;
  proto.set(GetStaticPrototype(proxy));
  return true;
}

bool BaseDOMProxyHandler::getOwnEnumerablePropertyKeys(
    JSContext* cx, JS::Handle<JSObject*> proxy,
    JS::MutableHandleVector<jsid> props) const {
  return ownPropNames(cx, proxy, JSITER_OWNONLY, props);
}

bool DOMProxyHandler::setCustom(JSContext* cx, JS::Handle<JSObject*> proxy,
                                JS::Handle<jsid> id, JS::Handle<JS::Value> v,
                                bool* done) const {
  *done = false;
  return true;
}

// static
JSObject* DOMProxyHandler::GetExpandoObject(JSObject* obj) {
  CheckDOMProxy(obj);

  JS::Value v = js::GetProxyPrivate(obj);
  if (v.isObject()) {
    CheckExpandoObject(obj, v);
    return &v.toObject();
  }

  if (v.isUndefined()) {
    return nullptr;
  }

  auto* expandoAndGeneration =
      static_cast<JS::ExpandoAndGeneration*>(v.toPrivate());
  CheckExpandoAndGeneration(obj, expandoAndGeneration);

  v = expandoAndGeneration->expando;
  return v.isUndefined() ? nullptr : &v.toObject();
}

void ShadowingDOMProxyHandler::trace(JSTracer* trc, JSObject* proxy) const {
  DOMProxyHandler::trace(trc, proxy);

  MOZ_ASSERT(IsDOMProxy(proxy), "expected a DOM proxy object");
  JS::Value v = js::GetProxyPrivate(proxy);
  MOZ_ASSERT(!v.isObject(), "Should not have expando object directly!");

  // The proxy's private slot is set when we allocate the proxy,
  // so it cannot be |undefined|.
  MOZ_ASSERT(!v.isUndefined());

  auto* expandoAndGeneration =
      static_cast<JS::ExpandoAndGeneration*>(v.toPrivate());
  JS::TraceEdge(trc, &expandoAndGeneration->expando,
                "Shadowing DOM proxy expando");
}

}  // namespace mozilla::dom
