/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=99 ft=cpp: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "mozilla/dom/DOMJSProxyHandler.h"
#include "xpcpublic.h"
#include "xpcprivate.h"
#include "XPCQuickStubs.h"
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
  return InternJSString(cx, s_length_id, "length");
}


const char HandlerFamily = 0;

js::DOMProxyShadowsResult
DOMProxyShadows(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id)
{
  JS::Value v = js::GetProxyExtra(proxy, JSPROXYSLOT_EXPANDO);
  if (v.isObject()) {
    bool hasOwn;
    if (!JS_AlreadyHasOwnPropertyById(cx, &v.toObject(), id, &hasOwn))
      return js::ShadowCheckFailed;

    return hasOwn ? js::Shadows : js::DoesntShadow;
  }

  if (v.isUndefined()) {
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
    js::SetDOMProxyInformation((const void*) &HandlerFamily,
                               js::PROXY_EXTRA_SLOT + JSPROXYSLOT_EXPANDO, DOMProxyShadows);
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
    XPCWrappedNativeScope* scope = xpc::MaybeGetObjectScope(obj);
    if (scope) {
      scope->RemoveDOMExpandoObject(obj);
    }
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
    JS_NewObjectWithGivenProto(cx, nullptr, nullptr, js::GetObjectParent(obj)));
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

  XPCWrappedNativeScope* scope = xpc::GetObjectScope(obj);
  if (!scope->RegisterDOMExpandoObject(obj)) {
    return nullptr;
  }

  cache->SetPreservingWrapper(true);
  js::SetProxyExtra(obj, JSPROXYSLOT_EXPANDO, ObjectValue(*expando));

  return expando;
}

bool
DOMProxyHandler::isExtensible(JSContext *cx, JS::Handle<JSObject*> proxy, bool *extensible)
{
  // always extensible per WebIDL
  *extensible = true;
  return true;
}

bool
DOMProxyHandler::preventExtensions(JSContext *cx, JS::Handle<JSObject*> proxy)
{
  // Throw a TypeError, per WebIDL.
  JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                       JSMSG_CANT_CHANGE_EXTENSIBILITY);
  return false;
}

bool
BaseDOMProxyHandler::getPropertyDescriptor(JSContext* cx,
                                           JS::Handle<JSObject*> proxy,
                                           JS::Handle<jsid> id,
                                           MutableHandle<JSPropertyDescriptor> desc,
                                           unsigned flags)
{
  if (!getOwnPropertyDescriptor(cx, proxy, id, desc, flags)) {
    return false;
  }
  if (desc.object()) {
    return true;
  }

  JS::Rooted<JSObject*> proto(cx);
  if (!js::GetObjectProto(cx, proxy, &proto)) {
    return false;
  }
  if (!proto) {
    desc.object().set(nullptr);
    return true;
  }

  return JS_GetPropertyDescriptorById(cx, proto, id, 0, desc);
}

bool
DOMProxyHandler::defineProperty(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                                MutableHandle<JSPropertyDescriptor> desc, bool* defined)
{
  if (desc.hasGetterObject() && desc.setter() == JS_StrictPropertyStub) {
    return JS_ReportErrorFlagsAndNumber(cx,
                                        JSREPORT_WARNING | JSREPORT_STRICT |
                                        JSREPORT_STRICT_MODE_ERROR,
                                        js_GetErrorMessage, nullptr,
                                        JSMSG_GETTER_ONLY);
  }

  if (xpc::WrapperFactory::IsXrayWrapper(proxy)) {
    return true;
  }

  JSObject* expando = EnsureExpandoObject(cx, proxy);
  if (!expando) {
    return false;
  }

  bool dummy;
  return js_DefineOwnProperty(cx, expando, id, desc, &dummy);
}

bool
DOMProxyHandler::delete_(JSContext* cx, JS::Handle<JSObject*> proxy,
                         JS::Handle<jsid> id, bool* bp)
{
  JS::Rooted<JSObject*> expando(cx);
  if (!xpc::WrapperFactory::IsXrayWrapper(proxy) && (expando = GetExpandoObject(proxy))) {
    return JS_DeletePropertyById2(cx, expando, id, bp);
  }

  *bp = true;
  return true;
}

bool
BaseDOMProxyHandler::enumerate(JSContext* cx, JS::Handle<JSObject*> proxy,
                               AutoIdVector& props)
{
  JS::Rooted<JSObject*> proto(cx);
  if (!JS_GetPrototype(cx, proxy, &proto))  {
    return false;
  }
  return getOwnPropertyNames(cx, proxy, props) &&
         (!proto || js::GetPropertyNames(cx, proto, 0, &props));
}

bool
BaseDOMProxyHandler::watch(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                           JS::Handle<JSObject*> callable)
{
  return js::WatchGuts(cx, proxy, id, callable);
}

bool
BaseDOMProxyHandler::unwatch(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id)
{
  return js::UnwatchGuts(cx, proxy, id);
}

bool
DOMProxyHandler::has(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id, bool* bp)
{
  if (!hasOwn(cx, proxy, id, bp)) {
    return false;
  }

  if (*bp) {
    // We have the property ourselves; no need to worry about our prototype
    // chain.
    return true;
  }

  // OK, now we have to look at the proto
  JS::Rooted<JSObject*> proto(cx);
  if (!js::GetObjectProto(cx, proxy, &proto)) {
    return false;
  }
  if (!proto) {
    return true;
  }
  bool protoHasProp;
  bool ok = JS_HasPropertyById(cx, proto, id, &protoHasProp);
  if (ok) {
    *bp = protoHasProp;
  }
  return ok;
}

int32_t
IdToInt32(JSContext* cx, JS::Handle<jsid> id)
{
  JS::Rooted<JS::Value> idval(cx);
  double array_index;
  int32_t i;
  if (!::JS_IdToValue(cx, id, idval.address()) ||
      !JS::ToNumber(cx, idval, &array_index) ||
      !::JS_DoubleIsInt32(array_index, &i)) {
    return -1;
  }

  return i;
}

} // namespace dom
} // namespace mozilla
