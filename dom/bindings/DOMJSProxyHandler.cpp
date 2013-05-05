/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=99 ft=cpp: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "DOMJSProxyHandler.h"
#include "xpcpublic.h"
#include "xpcprivate.h"
#include "XPCQuickStubs.h"
#include "XPCWrapper.h"
#include "WrapperFactory.h"
#include "nsDOMClassInfo.h"
#include "nsGlobalWindow.h"
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
  JSAutoRequest ar(cx);

  return InternJSString(cx, s_length_id, "length");
}


int HandlerFamily;

js::ListBaseShadowsResult
DOMListShadows(JSContext* cx, JSHandleObject proxy, JSHandleId id)
{
  JS::Value v = js::GetProxyExtra(proxy, JSPROXYSLOT_EXPANDO);
  if (v.isObject()) {
    JSBool hasOwn;
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
struct SetListBaseInformation
{
  SetListBaseInformation() {
    js::SetListBaseInformation((void*) &HandlerFamily,
                               js::JSSLOT_PROXY_EXTRA + JSPROXYSLOT_EXPANDO, DOMListShadows);
  }
};

SetListBaseInformation gSetListBaseInformation;

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
  } else {
    js::ExpandoAndGeneration* expandoAndGeneration =
      static_cast<js::ExpandoAndGeneration*>(v.toPrivate());
    v = expandoAndGeneration->expando;
    if (v.isUndefined()) {
      return nullptr;
    }
    expandoAndGeneration->expando = UndefinedValue();
  }

  xpc::GetObjectScope(obj)->RemoveDOMExpandoObject(obj);

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

  JSObject* expando = JS_NewObjectWithGivenProto(cx, nullptr, nullptr,
                                                 js::GetObjectParent(obj));
  if (!expando) {
    return nullptr;
  }

  XPCWrappedNativeScope* scope = xpc::GetObjectScope(obj);
  if (!scope->RegisterDOMExpandoObject(obj)) {
    return nullptr;
  }

  nsWrapperCache* cache;
  CallQueryInterface(UnwrapDOMObject<nsISupports>(obj), &cache);
  cache->SetPreservingWrapper(true);

  if (expandoAndGeneration) {
    expandoAndGeneration->expando.setObject(*expando);
  } else {
    js::SetProxyExtra(obj, JSPROXYSLOT_EXPANDO, ObjectValue(*expando));
  }

  return expando;
}

bool
DOMProxyHandler::isExtensible(JSObject *proxy)
{
  return true; // always extensible per WebIDL
}

bool
DOMProxyHandler::preventExtensions(JSContext *cx, JS::Handle<JSObject*> proxy)
{
  // Throw a TypeError, per WebIDL.
  JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_CHANGE_EXTENSIBILITY);
  return false;
}

bool
DOMProxyHandler::getPropertyDescriptor(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                                       JSPropertyDescriptor* desc, unsigned flags)
{
  if (!getOwnPropertyDescriptor(cx, proxy, id, desc, flags)) {
    return false;
  }
  if (desc->obj) {
    return true;
  }

  JS::Rooted<JSObject*> proto(cx);
  if (!js::GetObjectProto(cx, proxy, &proto)) {
    return false;
  }
  if (!proto) {
    desc->obj = NULL;
    return true;
  }

  return JS_GetPropertyDescriptorById(cx, proto, id, 0, desc);
}

bool
DOMProxyHandler::defineProperty(JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
                                JSPropertyDescriptor* desc)
{
  if ((desc->attrs & JSPROP_GETTER) && desc->setter == JS_StrictPropertyStub) {
    return JS_ReportErrorFlagsAndNumber(cx,
                                        JSREPORT_WARNING | JSREPORT_STRICT |
                                        JSREPORT_STRICT_MODE_ERROR,
                                        js_GetErrorMessage, NULL,
                                        JSMSG_GETTER_ONLY);
  }

  if (xpc::WrapperFactory::IsXrayWrapper(proxy)) {
    return true;
  }

  JSObject* expando = EnsureExpandoObject(cx, proxy);
  if (!expando) {
    return false;
  }

  JSBool dummy;
  return js_DefineOwnProperty(cx, expando, id, *desc, &dummy);
}

bool
DOMProxyHandler::delete_(JSContext* cx, JS::Handle<JSObject*> proxy,
                         JS::Handle<jsid> id, bool* bp)
{
  JSBool b = true;

  JS::Rooted<JSObject*> expando(cx);
  if (!xpc::WrapperFactory::IsXrayWrapper(proxy) && (expando = GetExpandoObject(proxy))) {
    JS::Rooted<Value> v(cx);
    if (!JS_DeletePropertyById2(cx, expando, id, v.address()) ||
        !JS_ValueToBoolean(cx, v, &b)) {
      return false;
    }
  }

  *bp = !!b;
  return true;
}

bool
DOMProxyHandler::enumerate(JSContext* cx, JS::Handle<JSObject*> proxy, AutoIdVector& props)
{
  JS::Rooted<JSObject*> proto(cx);
  if (!JS_GetPrototype(cx, proxy, proto.address())) {
    return false;
  }
  return getOwnPropertyNames(cx, proxy, props) &&
         (!proto || js::GetPropertyNames(cx, proto, 0, &props));
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
  JSBool protoHasProp;
  bool ok = JS_HasPropertyById(cx, proto, id, &protoHasProp);
  if (ok) {
    *bp = protoHasProp;
  }
  return ok;
}

bool
DOMProxyHandler::AppendNamedPropertyIds(JSContext* cx,
                                        JS::Handle<JSObject*> proxy,
                                        nsTArray<nsString>& names,
                                        bool shadowPrototypeProperties,
                                        JS::AutoIdVector& props)
{
  for (uint32_t i = 0; i < names.Length(); ++i) {
    JS::Rooted<JS::Value> v(cx);
    if (!xpc::NonVoidStringToJsval(cx, names[i], v.address())) {
      return false;
    }

    JS::Rooted<jsid> id(cx);
    if (!JS_ValueToId(cx, v, id.address())) {
      return false;
    }

    if (shadowPrototypeProperties ||
        !HasPropertyOnPrototype(cx, proxy, this, id)) {
      if (!props.append(id)) {
        return false;
      }
    }
  }

  return true;
}

int32_t
IdToInt32(JSContext* cx, JS::Handle<jsid> id)
{
  JSAutoRequest ar(cx);

  JS::Value idval;
  double array_index;
  int32_t i;
  if (!::JS_IdToValue(cx, id, &idval) ||
      !::JS_ValueToNumber(cx, idval, &array_index) ||
      !::JS_DoubleIsInt32(array_index, &i)) {
    return -1;
  }

  return i;
}

} // namespace dom
} // namespace mozilla
