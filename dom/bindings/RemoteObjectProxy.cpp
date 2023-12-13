/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteObjectProxy.h"
#include "AccessCheck.h"
#include "jsfriendapi.h"
#include "js/Object.h"  // JS::GetClass
#include "xpcprivate.h"

namespace mozilla::dom {

bool RemoteObjectProxyBase::getOwnPropertyDescriptor(
    JSContext* aCx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
    JS::MutableHandle<Maybe<JS::PropertyDescriptor>> aDesc) const {
  bool ok = CrossOriginGetOwnPropertyHelper(aCx, aProxy, aId, aDesc);
  if (!ok || aDesc.isSome()) {
    return ok;
  }

  return CrossOriginPropertyFallback(aCx, aProxy, aId, aDesc);
}

bool RemoteObjectProxyBase::defineProperty(
    JSContext* aCx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
    JS::Handle<JS::PropertyDescriptor> aDesc,
    JS::ObjectOpResult& aResult) const {
  // https://html.spec.whatwg.org/multipage/browsers.html#windowproxy-defineownproperty
  // step 3 and
  // https://html.spec.whatwg.org/multipage/browsers.html#location-defineownproperty
  // step 2
  return ReportCrossOriginDenial(aCx, aId, "define"_ns);
}

bool RemoteObjectProxyBase::ownPropertyKeys(
    JSContext* aCx, JS::Handle<JSObject*> aProxy,
    JS::MutableHandleVector<jsid> aProps) const {
  // https://html.spec.whatwg.org/multipage/browsers.html#crossoriginownpropertykeys-(-o-)
  // step 2 and
  // https://html.spec.whatwg.org/multipage/browsers.html#crossoriginproperties-(-o-)
  JS::Rooted<JSObject*> holder(aCx);
  if (!EnsureHolder(aCx, aProxy, &holder) ||
      !js::GetPropertyKeys(aCx, holder,
                           JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS,
                           aProps)) {
    return false;
  }

  // https://html.spec.whatwg.org/multipage/browsers.html#crossoriginownpropertykeys-(-o-)
  // step 3 and 4
  return xpc::AppendCrossOriginWhitelistedPropNames(aCx, aProps);
}

bool RemoteObjectProxyBase::delete_(JSContext* aCx,
                                    JS::Handle<JSObject*> aProxy,
                                    JS::Handle<jsid> aId,
                                    JS::ObjectOpResult& aResult) const {
  // https://html.spec.whatwg.org/multipage/browsers.html#windowproxy-delete
  // step 3 and
  // https://html.spec.whatwg.org/multipage/browsers.html#location-delete step 2
  return ReportCrossOriginDenial(aCx, aId, "delete"_ns);
}

bool RemoteObjectProxyBase::getPrototypeIfOrdinary(
    JSContext* aCx, JS::Handle<JSObject*> aProxy, bool* aIsOrdinary,
    JS::MutableHandle<JSObject*> aProtop) const {
  // WindowProxy's and Location's [[GetPrototypeOf]] traps aren't the ordinary
  // definition:
  //
  //   https://html.spec.whatwg.org/multipage/browsers.html#windowproxy-getprototypeof
  //   https://html.spec.whatwg.org/multipage/browsers.html#location-getprototypeof
  //
  // We nonetheless can implement it with a static [[Prototype]], because the
  // [[GetPrototypeOf]] trap should always return null.
  *aIsOrdinary = true;
  aProtop.set(nullptr);
  return true;
}

bool RemoteObjectProxyBase::preventExtensions(
    JSContext* aCx, JS::Handle<JSObject*> aProxy,
    JS::ObjectOpResult& aResult) const {
  // https://html.spec.whatwg.org/multipage/browsers.html#windowproxy-preventextensions
  // and
  // https://html.spec.whatwg.org/multipage/browsers.html#location-preventextensions
  return aResult.failCantPreventExtensions();
}

bool RemoteObjectProxyBase::isExtensible(JSContext* aCx,
                                         JS::Handle<JSObject*> aProxy,
                                         bool* aExtensible) const {
  // https://html.spec.whatwg.org/multipage/browsers.html#windowproxy-isextensible
  // and
  // https://html.spec.whatwg.org/multipage/browsers.html#location-isextensible
  *aExtensible = true;
  return true;
}

bool RemoteObjectProxyBase::get(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                                JS::Handle<JS::Value> aReceiver,
                                JS::Handle<jsid> aId,
                                JS::MutableHandle<JS::Value> aVp) const {
  return CrossOriginGet(aCx, aProxy, aReceiver, aId, aVp);
}

bool RemoteObjectProxyBase::set(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                                JS::Handle<jsid> aId,
                                JS::Handle<JS::Value> aValue,
                                JS::Handle<JS::Value> aReceiver,
                                JS::ObjectOpResult& aResult) const {
  return CrossOriginSet(aCx, aProxy, aId, aValue, aReceiver, aResult);
}

bool RemoteObjectProxyBase::getOwnEnumerablePropertyKeys(
    JSContext* aCx, JS::Handle<JSObject*> aProxy,
    JS::MutableHandleVector<jsid> aProps) const {
  return true;
}

const char* RemoteObjectProxyBase::className(
    JSContext* aCx, JS::Handle<JSObject*> aProxy) const {
  MOZ_ASSERT(js::IsProxy(aProxy));

  return NamesOfInterfacesWithProtos(mPrototypeID);
}

void RemoteObjectProxyBase::GetOrCreateProxyObject(
    JSContext* aCx, void* aNative, const JSClass* aClasp,
    JS::Handle<JSObject*> aTransplantTo, JS::MutableHandle<JSObject*> aProxy,
    bool& aNewObjectCreated) const {
  xpc::CompartmentPrivate* priv =
      xpc::CompartmentPrivate::Get(JS::CurrentGlobalOrNull(aCx));
  xpc::CompartmentPrivate::RemoteProxyMap& map = priv->GetRemoteProxyMap();
  if (auto result = map.lookup(aNative)) {
    MOZ_RELEASE_ASSERT(!aTransplantTo,
                       "GOCPO failed by finding an existing value");

    aProxy.set(result->value());

    // During a transplant, we put an object that is temporarily not a
    // proxy object into the map. Make sure that we don't return one of
    // these objects in the middle of a transplant.
    MOZ_RELEASE_ASSERT(JS::GetClass(aProxy) == aClasp);

    return;
  }

  js::ProxyOptions options;
  options.setClass(aClasp);
  JS::Rooted<JS::Value> native(aCx, JS::PrivateValue(aNative));
  JS::Rooted<JSObject*> obj(
      aCx, js::NewProxyObject(aCx, this, native, nullptr, options));
  if (!obj) {
    MOZ_RELEASE_ASSERT(!aTransplantTo, "GOCPO failed at NewProxyObject");
    return;
  }

  bool success;
  if (!JS_SetImmutablePrototype(aCx, obj, &success)) {
    MOZ_RELEASE_ASSERT(!aTransplantTo,
                       "GOCPO failed at JS_SetImmutablePrototype");
    return;
  }
  MOZ_ASSERT(success);

  aNewObjectCreated = true;

  // If we're transplanting onto an object, we want to make sure that it does
  // not have the same class as aClasp to ensure that the release assert earlier
  // in this function will actually fire if we try to return a proxy object in
  // the middle of a transplant.
  MOZ_RELEASE_ASSERT(!aTransplantTo || (JS::GetClass(aTransplantTo) != aClasp),
                     "GOCPO failed by not changing the class");

  if (!map.put(aNative, aTransplantTo ? aTransplantTo : obj)) {
    MOZ_RELEASE_ASSERT(!aTransplantTo, "GOCPO failed at map.put");
    return;
  }

  aProxy.set(obj);
}

const char RemoteObjectProxyBase::sCrossOriginProxyFamily = 0;

}  // namespace mozilla::dom
