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
  JS::Value v = js::GetProxyPrivate(proxy);
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
                               DOMProxyShadows);
  }
};

SetDOMProxyInformation gSetDOMProxyInformation;

// static
void
DOMProxyHandler::ClearExternalRefsForWrapperRelease(JSObject* obj)
{
  MOZ_ASSERT(IsDOMProxy(obj), "expected a DOM proxy object");
  JS::Value v = js::GetProxyPrivate(obj);
  if (v.isUndefined()) {
    // No expando.
    return;
  }

  // See EnsureExpandoObject for the work we're trying to undo here.

  if (v.isObject()) {
    // Drop us from the DOM expando hashtable.  Don't worry about clearing our
    // slot reference to the expando; we're about to die anyway.
    xpc::ObjectScope(obj)->RemoveDOMExpandoObject(obj);
    return;
  }

  // Prevent having a dangling pointer to our expando from the
  // ExpandoAndGeneration.
  js::ExpandoAndGeneration* expandoAndGeneration =
    static_cast<js::ExpandoAndGeneration*>(v.toPrivate());
  expandoAndGeneration->expando = UndefinedValue();
}

// static
JSObject*
DOMProxyHandler::GetAndClearExpandoObject(JSObject* obj)
{
  MOZ_ASSERT(IsDOMProxy(obj), "expected a DOM proxy object");
  JS::Value v = js::GetProxyPrivate(obj);
  if (v.isUndefined()) {
    return nullptr;
  }

  if (v.isObject()) {
    js::SetProxyPrivate(obj, UndefinedValue());
    xpc::ObjectScope(obj)->RemoveDOMExpandoObject(obj);
  } else {
    js::ExpandoAndGeneration* expandoAndGeneration =
      static_cast<js::ExpandoAndGeneration*>(v.toPrivate());
    v = expandoAndGeneration->expando;
    if (v.isUndefined()) {
      return nullptr;
    }
    // We have to expose v to active JS here.  The reason for that is that we
    // might be in the middle of a GC right now.  If our proxy hasn't been
    // traced yet, when it _does_ get traced it won't trace the expando, since
    // we're breaking that link.  But the Rooted we're presumably being placed
    // into is also not going to trace us, because Rooted marking is done at
    // the very beginning of the GC.  In that situation, we need to manually
    // mark the expando as live here.  JS::ExposeValueToActiveJS will do just
    // that for us.
    //
    // We don't need to do this in the non-expandoAndGeneration case, because
    // in that case our value is stored in a slot and slots will already mark
    // the old thing live when the value in the slot changes.
    JS::ExposeValueToActiveJS(v);
    expandoAndGeneration->expando = UndefinedValue();
  }


  return &v.toObject();
}

// static
JSObject*
DOMProxyHandler::EnsureExpandoObject(JSContext* cx, JS::Handle<JSObject*> obj)
{
  NS_ASSERTION(IsDOMProxy(obj), "expected a DOM proxy object");
  JS::Value v = js::GetProxyPrivate(obj);
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
  js::SetProxyPrivate(obj, ObjectValue(*expando));

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
  JS::Value v = js::GetProxyPrivate(obj);
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
  JS::Value v = js::GetProxyPrivate(proxy);
  MOZ_ASSERT(!v.isObject(), "Should not have expando object directly!");

  // The proxy's private slot is set when we allocate the proxy,
  // so it cannot be |undefined|.
  MOZ_ASSERT(!v.isUndefined());

  js::ExpandoAndGeneration* expandoAndGeneration =
    static_cast<js::ExpandoAndGeneration*>(v.toPrivate());
  JS::TraceEdge(trc, &expandoAndGeneration->expando,
                "Shadowing DOM proxy expando");
}

} // namespace dom
} // namespace mozilla
