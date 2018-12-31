/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteObjectProxy.h"
#include "AccessCheck.h"
#include "jsfriendapi.h"

namespace mozilla {
namespace dom {

// Give RemoteObjectProxy 2 reserved slots, like the other wrappers, so
// JSObject::swap can swap it with CrossCompartmentWrappers without requiring
// malloc.
const js::Class RemoteObjectProxyClass =
    PROXY_CLASS_DEF("Proxy", JSCLASS_HAS_RESERVED_SLOTS(2));

bool RemoteObjectProxyBase::getOwnPropertyDescriptor(
    JSContext* aCx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
    JS::MutableHandle<JS::PropertyDescriptor> aDesc) const {
  bool ok = getOwnPropertyDescriptorInternal(aCx, aProxy, aId, aDesc);
  if (!ok || aDesc.object()) {
    return ok;
  }

  return getOwnPropertyDescriptorTail(aCx, aProxy, aId, aDesc);
}

bool RemoteObjectProxyBase::defineProperty(
    JSContext* aCx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
    JS::Handle<JS::PropertyDescriptor> aDesc,
    JS::ObjectOpResult& aResult) const {
  // https://html.spec.whatwg.org/multipage/browsers.html#windowproxy-defineownproperty
  // step 3 and
  // https://html.spec.whatwg.org/multipage/browsers.html#location-defineownproperty
  // step 2
  return ReportCrossOriginDenial(aCx, aId, NS_LITERAL_CSTRING("define"));
}

bool RemoteObjectProxyBase::ownPropertyKeys(JSContext* aCx,
                                            JS::Handle<JSObject*> aProxy,
                                            JS::AutoIdVector& aProps) const {
  // https://html.spec.whatwg.org/multipage/browsers.html#crossoriginownpropertykeys-(-o-)
  // step 2 and
  // https://html.spec.whatwg.org/multipage/browsers.html#crossoriginproperties-(-o-)
  JS::Rooted<JSObject*> holder(aCx);
  if (!EnsureHolder(aCx, aProxy, &holder) ||
      !js::GetPropertyKeys(aCx, holder,
                           JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS,
                           &aProps)) {
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
  return ReportCrossOriginDenial(aCx, aId, NS_LITERAL_CSTRING("delete"));
}

bool RemoteObjectProxyBase::getPrototype(
    JSContext* aCx, JS::Handle<JSObject*> aProxy,
    JS::MutableHandle<JSObject*> aProtop) const {
  // https://html.spec.whatwg.org/multipage/browsers.html#windowproxy-getprototypeof
  // step 3 and
  // https://html.spec.whatwg.org/multipage/browsers.html#location-getprototypeof
  // step 2
  aProtop.set(nullptr);
  return true;
}

bool RemoteObjectProxyBase::setPrototype(JSContext* aCx,
                                         JS::Handle<JSObject*> aProxy,
                                         JS::Handle<JSObject*> aProto,
                                         JS::ObjectOpResult& aResult) const {
  // https://html.spec.whatwg.org/multipage/browsers.html#windowproxy-setprototypeof
  // and
  // https://html.spec.whatwg.org/multipage/browsers.html#location-setprototypeof
  // say to call SetImmutablePrototype, which does nothing and just returns
  // whether the passed-in value equals the current prototype. Our current
  // prototype is always null, so this just comes down to returning whether null
  // was passed in.
  //
  // In terms of ObjectOpResult that means calling one of the fail*() things on
  // it if non-null was passed, and it's got one that does just what we want.
  if (!aProto) {
    return aResult.succeed();
  }
  return aResult.failCantSetProto();
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
  Rooted<PropertyDescriptor> desc(aCx);
  if (!getOwnPropertyDescriptor(aCx, aProxy, aId, &desc)) {
    return false;
  }

  MOZ_ASSERT(desc.object());

  if (desc.isDataDescriptor()) {
    aVp.set(desc.value());
    return true;
  }

  if (!desc.hasGetterObject()) {
    return ReportCrossOriginDenial(aCx, aId, NS_LITERAL_CSTRING("get"));
  }

  JS::Rooted<JSObject*> getter(aCx, desc.getterObject());
  return JS::Call(aCx, aReceiver, getter, JS::HandleValueArray::empty(), aVp);
}

bool RemoteObjectProxyBase::set(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                                JS::Handle<jsid> aId,
                                JS::Handle<JS::Value> aValue,
                                JS::Handle<JS::Value> aReceiver,
                                JS::ObjectOpResult& aResult) const {
  Rooted<PropertyDescriptor> desc(aCx);
  if (!getOwnPropertyDescriptor(aCx, aProxy, aId, &desc)) {
    return false;
  }

  MOZ_ASSERT(desc.object());

  if (!desc.hasSetterObject()) {
    return ReportCrossOriginDenial(aCx, aId, NS_LITERAL_CSTRING("set"));
  }

  JS::Rooted<JSObject*> setter(aCx, desc.setterObject());
  JS::Rooted<JS::Value> rv(aCx);
  return JS::Call(aCx, aReceiver, setter, JS::HandleValueArray(aValue), &rv) &&
         aResult.succeed();
}

bool RemoteObjectProxyBase::hasOwn(JSContext* aCx, JS::Handle<JSObject*> aProxy,
                                   JS::Handle<jsid> aId, bool* aBp) const {
  JS::Rooted<JSObject*> holder(aCx);
  if (!EnsureHolder(aCx, aProxy, &holder) ||
      !JS_AlreadyHasOwnPropertyById(aCx, holder, aId, aBp)) {
    return false;
  }

  if (!*aBp) {
    *aBp = xpc::IsCrossOriginWhitelistedProp(aCx, aId);
  }

  return true;
}

bool RemoteObjectProxyBase::getOwnEnumerablePropertyKeys(
    JSContext* aCx, JS::Handle<JSObject*> aProxy,
    JS::AutoIdVector& aProps) const {
  return true;
}

JSObject* RemoteObjectProxyBase::CreateProxyObject(
    JSContext* aCx, void* aNative, const js::Class* aClasp) const {
  js::ProxyOptions options;
  options.setClass(aClasp);
  JS::Rooted<JS::Value> native(aCx, JS::PrivateValue(aNative));
  return js::NewProxyObject(aCx, this, native, nullptr, options);
}

/* static */
bool RemoteObjectProxyBase::getOwnPropertyDescriptorTail(
    JSContext* aCx, JS::Handle<JSObject*> aProxy, JS::Handle<jsid> aId,
    JS::MutableHandle<JS::PropertyDescriptor> aDesc) {
  if (xpc::IsCrossOriginWhitelistedProp(aCx, aId)) {
    // https://html.spec.whatwg.org/multipage/browsers.html#crossorigingetownpropertyhelper-(-o,-p-)
    // step 3 says to return PropertyDescriptor {
    //   [[Value]]: undefined, [[Writable]]: false, [[Enumerable]]: false,
    //   [[Configurable]]: true
    // }.
    //
    aDesc.setDataDescriptor(JS::UndefinedHandleValue, JSPROP_READONLY);
    aDesc.object().set(aProxy);
    return true;
  }

  return ReportCrossOriginDenial(aCx, aId, NS_LITERAL_CSTRING("access"));
}

/* static */
bool RemoteObjectProxyBase::ReportCrossOriginDenial(
    JSContext* aCx, JS::Handle<jsid> aId, const nsACString& aAccessType) {
  xpc::AccessCheck::reportCrossOriginDenial(aCx, aId, aAccessType);
  return false;
}

const char RemoteObjectProxyBase::sCrossOriginProxyFamily = 0;

}  // namespace dom
}  // namespace mozilla
