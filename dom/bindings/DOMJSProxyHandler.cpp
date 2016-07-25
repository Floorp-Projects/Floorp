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
#include "nsDOMClassInfo.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/dom/BindingUtils.h"

#include "jsapi.h"

using namespace JS;

namespace mozilla {
namespace dom {

jsid s_length_id = JSID_VOID;

bool
DefineStaticJSVals(JSContext* cx)
{
  return AtomizeAndPinJSString(cx, s_length_id, "length");
}

const char DOMProxyHandler::family = 0;

js::DOMProxyShadowsResult
DOMProxyShadows(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id)
{
  JS::Rooted<JSObject*> expando(cx, DOMProxyHandler::GetExpandoObject(proxy));
  JS::Value v = js::GetProxyExtra(proxy, JSPROXYSLOT_EXPANDO);
  bool isOverrideBuiltins = !v.isObject() && !v.isUndefined();
  if (expando) {
    bool hasOwn;
    if (!JS_AlreadyHasOwnPropertyById(cx, expando, id, &hasOwn))
      return js::ShadowCheckFailed;

    if (hasOwn) {
      return isOverrideBuiltins ?
        js::ShadowsViaIndirectExpando : js::ShadowsViaDirectExpando;
    }
  }

  if (!isOverrideBuiltins) {
    // Our expando, if any, didn't shadow, so we're not shadowing at all.
    return js::DoesntShadow;
  }

  bool hasOwn;
  if (!GetProxyHandler(proxy)->hasOwn(cx, proxy, id, &hasOwn))
    return js::ShadowCheckFailed;

  return hasOwn ? js::Shadows : js::DoesntShadowUnique;
}

// Store the information for the specialized ICs.
struct SetDOMProxyInformation
{
  SetDOMProxyInformation() {
    js::SetDOMProxyInformation((const void*) &DOMProxyHandler::family,
                               JSPROXYSLOT_EXPANDO, DOMProxyShadows);
  }
};

SetDOMProxyInformation gSetDOMProxyInformation;

// static
JSObject*
DOMProxyHandler::GetAndClearExpandoObject(JSObject* obj)
{
  MOZ_ASSERT(IsDOMProxy(obj), "expected a DOM proxy object");
  JS::Value v = js::GetProxyExtra(obj, JSPROXYSLOT_EXPANDO);
  if (v.isUndefined()) {
    return nullptr;
  }

  if (v.isObject()) {
    js::SetProxyExtra(obj, JSPROXYSLOT_EXPANDO, UndefinedValue());
    xpc::ObjectScope(obj)->RemoveDOMExpandoObject(obj);
  } else {
    js::ExpandoAndGeneration* expandoAndGeneration =
      static_cast<js::ExpandoAndGeneration*>(v.toPrivate());
    v = expandoAndGeneration->expando;
    if (v.isUndefined()) {
      return nullptr;
    }
    expandoAndGeneration->expando = UndefinedValue();
  }


  return &v.toObject();
}

// static
JSObject*
DOMProxyHandler::EnsureExpandoObject(JSContext* cx, JS::Handle<JSObject*> obj)
{
  NS_ASSERTION(IsDOMProxy(obj), "expected a DOM proxy object");
  JS::Value v = js::GetProxyExtra(obj, JSPROXYSLOT_EXPANDO);
  if (v.isObject()) {
    return &v.toObject();
  }

  js::ExpandoAndGeneration* expandoAndGeneration;
  if (!v.isUndefined()) {
    expandoAndGeneration = static_cast<js::ExpandoAndGeneration*>(v.toPrivate());
    if (expandoAndGeneration->expando.isObject()) {
      return &expandoAndGeneration->expando.toObject();
    }
  } else {
    expandoAndGeneration = nullptr;
  }

  JS::Rooted<JSObject*> expando(cx,
    JS_NewObjectWithGivenProto(cx, nullptr, nullptr));
  if (!expando) {
    return nullptr;
  }

  nsISupports* native = UnwrapDOMObject<nsISupports>(obj);
  nsWrapperCache* cache;
  CallQueryInterface(native, &cache);
  if (expandoAndGeneration) {
    cache->PreserveWrapper(native);
    expandoAndGeneration->expando.setObject(*expando);

    return expando;
  }

  if (!xpc::ObjectScope(obj)->RegisterDOMExpandoObject(obj)) {
    return nullptr;
  }

  cache->SetPreservingWrapper(true);
  js::SetProxyExtra(obj, JSPROXYSLOT_EXPANDO, ObjectValue(*expando));

  return expando;
}

bool
DOMProxyHandler::preventExtensions(JSContext* cx, JS::Handle<JSObject*> proxy,
                                   JS::ObjectOpResult& result) const
{
  // always extensible per WebIDL
  return result.failCantPreventExtensions();
}

bool
DOMProxyHandler::isExtensible(JSContext *cx, JS::Handle<JSObject*> proxy, bool *extensible) const
{
  *extensible = true;
  return true;
}

bool
BaseDOMProxyHandler::getOwnPropertyDescriptor(JSContext* cx,
                                              JS::Handle<JSObject*> proxy,
                                              JS::Handle<jsid> id,
                                              MutableHandle<PropertyDescriptor> desc) const
{
  return getOwnPropDescriptor(cx, proxy, id, /* ignoreNamedProps = */ false,
                              desc);
}

bool
DOMProxyHandler::defineProperty(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                                Handle<PropertyDescriptor> desc,
                                JS::ObjectOpResult &result, bool *defined) const
{
  if (desc.hasGetterObject() && desc.setter() == JS_StrictPropertyStub) {
    return result.failGetterOnly();
  }

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
  *defined = true;
  return true;
}

bool
DOMProxyHandler::set(JSContext *cx, Handle<JSObject*> proxy, Handle<jsid> id,
                     Handle<JS::Value> v, Handle<JS::Value> receiver,
                     ObjectOpResult &result) const
{
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
  JS::Rooted<PropertyDescriptor> ownDesc(cx);
  if (!getOwnPropDescriptor(cx, proxy, id, /* ignoreNamedProps = */ true,
                            &ownDesc)) {
    return false;
  }
  return js::SetPropertyIgnoringNamedGetter(cx, proxy, id, v, receiver, ownDesc, result);
}

bool
DOMProxyHandler::delete_(JSContext* cx, JS::Handle<JSObject*> proxy,
                         JS::Handle<jsid> id, JS::ObjectOpResult &result) const
{
  JS::Rooted<JSObject*> expando(cx);
  if (!xpc::WrapperFactory::IsXrayWrapper(proxy) && (expando = GetExpandoObject(proxy))) {
    return JS_DeletePropertyById(cx, expando, id, result);
  }

  return result.succeed();
}

bool
BaseDOMProxyHandler::watch(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                           JS::Handle<JSObject*> callable) const
{
  return js::WatchGuts(cx, proxy, id, callable);
}

bool
BaseDOMProxyHandler::unwatch(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id) const
{
  return js::UnwatchGuts(cx, proxy, id);
}

bool
BaseDOMProxyHandler::ownPropertyKeys(JSContext* cx,
                                     JS::Handle<JSObject*> proxy,
                                     JS::AutoIdVector& props) const
{
  return ownPropNames(cx, proxy, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, props);
}

bool
BaseDOMProxyHandler::getPrototypeIfOrdinary(JSContext* cx, JS::Handle<JSObject*> proxy,
                                            bool* isOrdinary,
                                            JS::MutableHandle<JSObject*> proto) const
{
  *isOrdinary = true;
  proto.set(GetStaticPrototype(proxy));
  return true;
}

bool
BaseDOMProxyHandler::getOwnEnumerablePropertyKeys(JSContext* cx,
                                                  JS::Handle<JSObject*> proxy,
                                                  JS::AutoIdVector& props) const
{
  return ownPropNames(cx, proxy, JSITER_OWNONLY, props);
}

bool
DOMProxyHandler::setCustom(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                           JS::Handle<JS::Value> v, bool *done) const
{
  *done = false;
  return true;
}

//static
JSObject *
DOMProxyHandler::GetExpandoObject(JSObject *obj)
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

void
ShadowingDOMProxyHandler::trace(JSTracer* trc, JSObject* proxy) const
{
  DOMProxyHandler::trace(trc, proxy);

  MOZ_ASSERT(IsDOMProxy(proxy), "expected a DOM proxy object");
  JS::Value v = js::GetProxyExtra(proxy, JSPROXYSLOT_EXPANDO);
  MOZ_ASSERT(!v.isObject(), "Should not have expando object directly!");

  if (v.isUndefined()) {
    // This can happen if we GC while creating our object, before we get a
    // chance to set up its JSPROXYSLOT_EXPANDO slot.
    return;
  }

  js::ExpandoAndGeneration* expandoAndGeneration =
    static_cast<js::ExpandoAndGeneration*>(v.toPrivate());
  JS::TraceEdge(trc, &expandoAndGeneration->expando,
                "Shadowing DOM proxy expando");
}

} // namespace dom
} // namespace mozilla
