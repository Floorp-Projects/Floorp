/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MaybeCrossOriginObject.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMJSProxyHandler.h"
#include "mozilla/dom/RemoteObjectProxy.h"
#include "js/friend/WindowProxy.h"  // js::IsWindowProxy
#include "js/Object.h"              // JS::GetClass
#include "js/Proxy.h"
#include "js/RootingAPI.h"
#include "js/Wrapper.h"
#include "jsfriendapi.h"
#include "AccessCheck.h"
#include "nsContentUtils.h"

#ifdef DEBUG
static bool IsLocation(JSObject* obj) {
  return strcmp(JS::GetClass(obj)->name, "Location") == 0;
}
#endif  // DEBUG

namespace mozilla::dom {

/* static */
bool MaybeCrossOriginObjectMixins::IsPlatformObjectSameOrigin(JSContext* cx,
                                                              JSObject* obj) {
  MOZ_ASSERT(!js::IsCrossCompartmentWrapper(obj));
  // WindowProxy and Window must always be same-Realm, so we can do
  // our IsPlatformObjectSameOrigin check against either one.  But verify that
  // in case we have a WindowProxy the right things happen.
  MOZ_ASSERT(js::GetNonCCWObjectRealm(obj) ==
                 // "true" for second arg means to unwrap WindowProxy to
                 // get at the Window.
                 js::GetNonCCWObjectRealm(js::UncheckedUnwrap(obj, true)),
             "WindowProxy not same-Realm as Window?");

  BasePrincipal* subjectPrincipal =
      BasePrincipal::Cast(nsContentUtils::SubjectPrincipal(cx));
  BasePrincipal* objectPrincipal =
      BasePrincipal::Cast(nsContentUtils::ObjectPrincipal(obj));

  // The spec effectively has an EqualsConsideringDomain check here,
  // because the spec has no concept of asymmetric security
  // relationships.  But we shouldn't ever end up here in the
  // asymmetric case anyway: That case should end up with Xrays, which
  // don't call into this code.
  //
  // Let's assert that EqualsConsideringDomain and
  // SubsumesConsideringDomain give the same results and use
  // EqualsConsideringDomain for the check we actually do, since it's
  // stricter and more closely matches the spec.
  //
  // That said, if the (not very well named)
  // OriginAttributes::IsRestrictOpenerAccessForFPI() method returns
  // false, we want to use FastSubsumesConsideringDomainIgnoringFPD
  // instead of FastEqualsConsideringDomain, because in that case we
  // still want to treat things which are in different first-party
  // contexts as same-origin.
  MOZ_ASSERT(
      subjectPrincipal->FastEqualsConsideringDomain(objectPrincipal) ==
          subjectPrincipal->FastSubsumesConsideringDomain(objectPrincipal),
      "Why are we in an asymmetric case here?");
  if (OriginAttributes::IsRestrictOpenerAccessForFPI()) {
    return subjectPrincipal->FastEqualsConsideringDomain(objectPrincipal);
  }

  return subjectPrincipal->FastSubsumesConsideringDomainIgnoringFPD(
             objectPrincipal) &&
         objectPrincipal->FastSubsumesConsideringDomainIgnoringFPD(
             subjectPrincipal);
}

bool MaybeCrossOriginObjectMixins::CrossOriginGetOwnPropertyHelper(
    JSContext* cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
    JS::MutableHandle<Maybe<JS::PropertyDescriptor>> desc) const {
  MOZ_ASSERT(!IsPlatformObjectSameOrigin(cx, obj) || IsRemoteObjectProxy(obj),
             "Why did we get called?");
  // First check for an IDL-defined cross-origin property with the given name.
  // This corresponds to
  // https://html.spec.whatwg.org/multipage/browsers.html#crossorigingetownpropertyhelper-(-o,-p-)
  // step 2.
  JS::Rooted<JSObject*> holder(cx);
  if (!EnsureHolder(cx, obj, &holder)) {
    return false;
  }

  JS::Rooted<JS::PropertyDescriptor> holderDesc(cx);
  if (!JS_GetOwnPropertyDescriptorById(cx, holder, id, &holderDesc)) {
    return false;
  }

  if (holderDesc.object()) {
    holderDesc.object().set(obj);
    desc.set(Some(holderDesc.get()));
  } else {
    desc.reset();
  }

  return true;
}

/* static */
bool MaybeCrossOriginObjectMixins::CrossOriginPropertyFallback(
    JSContext* cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
    JS::MutableHandle<Maybe<JS::PropertyDescriptor>> desc) {
  MOZ_ASSERT(desc.isNothing(), "Why are we being called?");

  // Step 1.
  if (xpc::IsCrossOriginWhitelistedProp(cx, id)) {
    // Spec says to return PropertyDescriptor {
    //   [[Value]]: undefined, [[Writable]]: false, [[Enumerable]]: false,
    //   [[Configurable]]: true
    // }.
    JS::Rooted<JS::PropertyDescriptor> descUndef(cx);
    descUndef.setDataDescriptor(JS::UndefinedHandleValue, JSPROP_READONLY);
    descUndef.object().set(obj);
    desc.set(Some(descUndef.get()));
    return true;
  }

  // Step 2.
  return ReportCrossOriginDenial(cx, id, "access"_ns);
}

/* static */
bool MaybeCrossOriginObjectMixins::CrossOriginGet(
    JSContext* cx, JS::Handle<JSObject*> obj, JS::Handle<JS::Value> receiver,
    JS::Handle<jsid> id, JS::MutableHandle<JS::Value> vp) {
  // This is fairly similar to BaseProxyHandler::get, but there are some
  // differences.  Most importantly, we want to throw if we have a descriptor
  // with no getter, while BaseProxyHandler::get returns undefined.  The other
  // big difference is that we don't have to worry about prototypes (ours is
  // always null).

  // We want to invoke [[GetOwnProperty]] on "obj", but _without_ entering its
  // compartment, because for the proxies we have here [[GetOwnProperty]] will
  // do security checks based on the current Realm.  Unfortunately,
  // JS_GetPropertyDescriptorById asserts that compartments match.  Luckily, we
  // know that "obj" is a proxy here, so we can directly call its
  // getOwnPropertyDescriptor() hook.
  //
  // It looks like Proxy::getOwnPropertyDescriptor is not public, so just grab
  // the handler and call its getOwnPropertyDescriptor hook directly.
  MOZ_ASSERT(js::IsProxy(obj), "How did we get a bogus object here?");
  MOZ_ASSERT(
      js::IsWindowProxy(obj) || IsLocation(obj) || IsRemoteObjectProxy(obj),
      "Unexpected proxy");
  MOZ_ASSERT(!IsPlatformObjectSameOrigin(cx, obj) || IsRemoteObjectProxy(obj),
             "Why did we get called?");
  js::AssertSameCompartment(cx, receiver);

  // Step 1.
  JS::Rooted<Maybe<JS::PropertyDescriptor>> desc(cx);
  if (!js::GetProxyHandler(obj)->getOwnPropertyDescriptor(cx, obj, id, &desc)) {
    return false;
  }

  // Step 2.
  MOZ_ASSERT(desc.isSome(),
             "Callees should throw in all cases when they are not finding a "
             "property decriptor");
  desc->assertComplete();

  // Step 3.
  if (desc->isDataDescriptor()) {
    vp.set(desc->value());
    return true;
  }

  // Step 4.
  MOZ_ASSERT(desc->isAccessorDescriptor());

  // Step 5.
  JS::Rooted<JSObject*> getter(cx);
  if (!desc->hasGetterObject() || !(getter = desc->getterObject())) {
    // Step 6.
    return ReportCrossOriginDenial(cx, id, "get"_ns);
  }

  // Step 7.
  return JS::Call(cx, receiver, getter, JS::HandleValueArray::empty(), vp);
}

/* static */
bool MaybeCrossOriginObjectMixins::CrossOriginSet(
    JSContext* cx, JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
    JS::Handle<JS::Value> v, JS::Handle<JS::Value> receiver,
    JS::ObjectOpResult& result) {
  // We want to invoke [[GetOwnProperty]] on "obj", but _without_ entering its
  // compartment, because for the proxies we have here [[GetOwnProperty]] will
  // do security checks based on the current Realm.  Unfortunately,
  // JS_GetPropertyDescriptorById asserts that compartments match.  Luckily, we
  // know that "obj" is a proxy here, so we can directly call its
  // getOwnPropertyDescriptor() hook.
  //
  // It looks like Proxy::getOwnPropertyDescriptor is not public, so just grab
  // the handler and call its getOwnPropertyDescriptor hook directly.
  MOZ_ASSERT(js::IsProxy(obj), "How did we get a bogus object here?");
  MOZ_ASSERT(
      js::IsWindowProxy(obj) || IsLocation(obj) || IsRemoteObjectProxy(obj),
      "Unexpected proxy");
  MOZ_ASSERT(!IsPlatformObjectSameOrigin(cx, obj) || IsRemoteObjectProxy(obj),
             "Why did we get called?");
  js::AssertSameCompartment(cx, receiver);
  js::AssertSameCompartment(cx, v);

  // Step 1.
  JS::Rooted<Maybe<JS::PropertyDescriptor>> desc(cx);
  if (!js::GetProxyHandler(obj)->getOwnPropertyDescriptor(cx, obj, id, &desc)) {
    return false;
  }

  // Step 2.
  MOZ_ASSERT(desc.isSome(),
             "Callees should throw in all cases when they are not finding a "
             "property decriptor");
  desc->assertComplete();

  // Step 3.
  JS::Rooted<JSObject*> setter(cx);
  if (desc->hasSetterObject() && (setter = desc->setterObject())) {
    JS::Rooted<JS::Value> ignored(cx);
    // Step 3.1.
    if (!JS::Call(cx, receiver, setter, JS::HandleValueArray(v), &ignored)) {
      return false;
    }

    // Step 3.2.
    return result.succeed();
  }

  // Step 4.
  return ReportCrossOriginDenial(cx, id, "set"_ns);
}

/* static */
bool MaybeCrossOriginObjectMixins::EnsureHolder(
    JSContext* cx, JS::Handle<JSObject*> obj, size_t slot,
    const CrossOriginProperties& properties,
    JS::MutableHandle<JSObject*> holder) {
  MOZ_ASSERT(!IsPlatformObjectSameOrigin(cx, obj) || IsRemoteObjectProxy(obj),
             "Why are we calling this at all in same-origin cases?");
  // We store the holders in a weakmap stored in obj's slot.  Our object is
  // always a proxy, so we can just go ahead and use GetProxyReservedSlot here.
  JS::Rooted<JS::Value> weakMapVal(cx, js::GetProxyReservedSlot(obj, slot));
  if (weakMapVal.isUndefined()) {
    // Enter the Realm of "obj" when we allocate the WeakMap, since we are going
    // to store it in a slot on "obj" and in general we may not be
    // same-compartment with "obj" here.
    JSAutoRealm ar(cx, obj);
    JSObject* newMap = JS::NewWeakMapObject(cx);
    if (!newMap) {
      return false;
    }
    weakMapVal.setObject(*newMap);
    js::SetProxyReservedSlot(obj, slot, weakMapVal);
  }
  MOZ_ASSERT(weakMapVal.isObject(),
             "How did a non-object else end up in this slot?");

  JS::Rooted<JSObject*> map(cx, &weakMapVal.toObject());
  MOZ_ASSERT(JS::IsWeakMapObject(map),
             "How did something else end up in this slot?");

  // We need to be in "map"'s compartment to work with it.  Per spec, the key
  // for this map is supposed to be the pair (current settings, relevant
  // settings).  The current settings corresponds to the current Realm of cx.
  // The relevant settings corresponds to the Realm of "obj", but since all of
  // our objects are per-Realm singletons, we are basically using "obj" itself
  // as part of the key.
  //
  // To represent the current settings, we use a dedicated key object of the
  // current-Realm.
  //
  // We can't use the current global, because we can't get a useful
  // cross-compartment wrapper for it; such wrappers would always go
  // through a WindowProxy and would not be guarantee to keep pointing to a
  // single Realm when unwrapped.  We want to grab this key before we start
  // changing Realms.
  //
  // Also we can't use arbitrary object (e.g.: Object.prototype), because at
  // this point those compartments are not same-origin, and don't have access to
  // each other, and the object retrieved here will be wrapped by a security
  // wrapper below, and the wrapper will be stored into the cache
  // (see Compartment::wrap).  Those compartments can get access later by
  // modifying `document.domain`, and wrapping objects after that point
  // shouldn't result in a security wrapper.  Wrap operation looks up the
  // existing wrapper in the cache, that contains the security wrapper created
  // here.  We should use unique/private object here, so that this doesn't
  // affect later wrap operation.
  JS::Rooted<JSObject*> key(cx, JS::GetRealmKeyObject(cx));
  if (!key) {
    return false;
  }

  JS::Rooted<JS::Value> holderVal(cx);
  {  // Scope for working with the map
    JSAutoRealm ar(cx, map);
    if (!MaybeWrapObject(cx, &key)) {
      return false;
    }

    if (!JS::GetWeakMapEntry(cx, map, key, &holderVal)) {
      return false;
    }
  }

  if (holderVal.isObject()) {
    // We want to do an unchecked unwrap, because the holder (and the current
    // caller) may actually be more privileged than our map.
    holder.set(js::UncheckedUnwrap(&holderVal.toObject()));

    // holder might be a dead object proxy if things got nuked.
    if (!JS_IsDeadWrapper(holder)) {
      MOZ_ASSERT(js::GetContextRealm(cx) == js::GetNonCCWObjectRealm(holder),
                 "How did we end up with a key/value mismatch?");
      return true;
    }
  }

  // We didn't find a usable holder.  Go ahead and allocate one.  At this point
  // we have two options: we could allocate the holder in the current Realm and
  // store a cross-compartment wrapper for it in the map as needed, or we could
  // allocate the holder in the Realm of the map and have it hold
  // cross-compartment references to all the methods it holds, since those
  // methods need to be in our current Realm.  It seems better to allocate the
  // holder in our current Realm.
  bool isChrome = xpc::AccessCheck::isChrome(js::GetContextRealm(cx));
  holder.set(JS_NewObjectWithGivenProto(cx, nullptr, nullptr));
  if (!holder || !JS_DefineProperties(cx, holder, properties.mAttributes) ||
      !JS_DefineFunctions(cx, holder, properties.mMethods) ||
      (isChrome && properties.mChromeOnlyAttributes &&
       !JS_DefineProperties(cx, holder, properties.mChromeOnlyAttributes)) ||
      (isChrome && properties.mChromeOnlyMethods &&
       !JS_DefineFunctions(cx, holder, properties.mChromeOnlyMethods))) {
    return false;
  }

  holderVal.setObject(*holder);
  {  // Scope for working with the map
    JSAutoRealm ar(cx, map);

    // Key is already in the right Realm, but we need to wrap the value.
    if (!MaybeWrapValue(cx, &holderVal)) {
      return false;
    }

    if (!JS::SetWeakMapEntry(cx, map, key, holderVal)) {
      return false;
    }
  }

  return true;
}

/* static */
bool MaybeCrossOriginObjectMixins::ReportCrossOriginDenial(
    JSContext* aCx, JS::Handle<jsid> aId, const nsACString& aAccessType) {
  xpc::AccessCheck::reportCrossOriginDenial(aCx, aId, aAccessType);
  return false;
}

template <typename Base>
bool MaybeCrossOriginObject<Base>::getPrototype(
    JSContext* cx, JS::Handle<JSObject*> proxy,
    JS::MutableHandle<JSObject*> protop) const {
  if (!IsPlatformObjectSameOrigin(cx, proxy)) {
    protop.set(nullptr);
    return true;
  }

  {  // Scope for JSAutoRealm
    JSAutoRealm ar(cx, proxy);
    protop.set(getSameOriginPrototype(cx));
    if (!protop) {
      return false;
    }
  }

  return MaybeWrapObject(cx, protop);
}

template <typename Base>
bool MaybeCrossOriginObject<Base>::setPrototype(
    JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<JSObject*> proto,
    JS::ObjectOpResult& result) const {
  // Inlined version of
  // https://tc39.github.io/ecma262/#sec-set-immutable-prototype
  js::AssertSameCompartment(cx, proto);

  // We have to be careful how we get the prototype.  In particular, we do _NOT_
  // want to enter the Realm of "proxy" to do that, in case we're not
  // same-origin with it here.
  JS::Rooted<JSObject*> wrappedProxy(cx, proxy);
  if (!MaybeWrapObject(cx, &wrappedProxy)) {
    return false;
  }

  JS::Rooted<JSObject*> currentProto(cx);
  if (!js::GetObjectProto(cx, wrappedProxy, &currentProto)) {
    return false;
  }

  if (currentProto != proto) {
    return result.failCantSetProto();
  }

  return result.succeed();
}

template <typename Base>
bool MaybeCrossOriginObject<Base>::getPrototypeIfOrdinary(
    JSContext* cx, JS::Handle<JSObject*> proxy, bool* isOrdinary,
    JS::MutableHandle<JSObject*> protop) const {
  // We have a custom [[GetPrototypeOf]]
  *isOrdinary = false;
  return true;
}

template <typename Base>
bool MaybeCrossOriginObject<Base>::setImmutablePrototype(
    JSContext* cx, JS::Handle<JSObject*> proxy, bool* succeeded) const {
  // We just want to disallow this.
  *succeeded = false;
  return true;
}

template <typename Base>
bool MaybeCrossOriginObject<Base>::isExtensible(JSContext* cx,
                                                JS::Handle<JSObject*> proxy,
                                                bool* extensible) const {
  // We never allow [[PreventExtensions]] to succeed.
  *extensible = true;
  return true;
}

template <typename Base>
bool MaybeCrossOriginObject<Base>::preventExtensions(
    JSContext* cx, JS::Handle<JSObject*> proxy,
    JS::ObjectOpResult& result) const {
  return result.failCantPreventExtensions();
}

template <typename Base>
bool MaybeCrossOriginObject<Base>::defineProperty(
    JSContext* cx, JS::Handle<JSObject*> proxy, JS::Handle<jsid> id,
    JS::Handle<JS::PropertyDescriptor> desc, JS::ObjectOpResult& result) const {
  if (!IsPlatformObjectSameOrigin(cx, proxy)) {
    return ReportCrossOriginDenial(cx, id, "define"_ns);
  }

  // Enter the Realm of proxy and do the remaining work in there.
  JSAutoRealm ar(cx, proxy);
  JS::Rooted<JS::PropertyDescriptor> descCopy(cx, desc);
  if (!JS_WrapPropertyDescriptor(cx, &descCopy)) {
    return false;
  }

  JS_MarkCrossZoneId(cx, id);

  return definePropertySameOrigin(cx, proxy, id, descCopy, result);
}

template <typename Base>
bool MaybeCrossOriginObject<Base>::enumerate(
    JSContext* cx, JS::Handle<JSObject*> proxy,
    JS::MutableHandleVector<jsid> props) const {
  // Just get the property keys from ourselves, in whatever Realm we happen to
  // be in. It's important to not enter the Realm of "proxy" here, because that
  // would affect the list of keys we claim to have. We wrap the proxy in the
  // current compartment just to be safe; it doesn't affect behavior as far as
  // CrossOriginObjectWrapper and MaybeCrossOriginObject are concerned.
  JS::Rooted<JSObject*> self(cx, proxy);
  if (!MaybeWrapObject(cx, &self)) {
    return false;
  }

  return js::GetPropertyKeys(cx, self, 0, props);
}

template <typename Base>
bool MaybeCrossOriginObject<Base>::hasInstance(JSContext* cx,
                                               JS::Handle<JSObject*> proxy,
                                               JS::MutableHandle<JS::Value> v,
                                               bool* bp) const {
  if (!IsPlatformObjectSameOrigin(cx, proxy)) {
    // In the cross-origin case we never have @@hasInstance, and we're never
    // callable, so just go ahead and report an error.  If we enter the realm of
    // "proxy" to do that, our caller won't be able to do anything with the
    // exception, so instead let's wrap "proxy" into our realm.  We definitely
    // do NOT want to call JS::InstanceofOperator here after entering "proxy's"
    // realm, because that would do the wrong thing with @@hasInstance on the
    // object by seeing any such definitions when we should not.
    JS::Rooted<JS::Value> val(cx, JS::ObjectValue(*proxy));
    if (!MaybeWrapValue(cx, &val)) {
      return false;
    }
    return js::ReportIsNotFunction(cx, val);
  }

  // We need to wrap `proxy` into our compartment or enter proxy's realm
  // and wrap `v` into proxy's compartment because at this point `v` and `proxy`
  // might no longer be same-compartment. One solution is to enter the realm of
  // `proxy` and look up @@hasInstance there. However, that will lead to
  // incorrect error reporting because the mechanism for reporting the "not a
  // function" exception only works correctly if we are in the realm of the
  // script that encountered the instanceof expression. Thus, we don't want to
  // switch realms and will wrap `proxy` into our current compartment and lookup
  // @@hasInstance. Note that accesses to get @@hasInstance on `proxy` after it
  // is wrapped in the `cx` compartment will still work because `cx` and `proxy`
  // are same-origin.
  JS::Rooted<JSObject*> proxyWrap(cx, proxy);
  if (!MaybeWrapObject(cx, &proxyWrap)) {
    return false;
  }
  // We are not calling BaseProxyHandler::hasInstance here because it expects
  // `proxy` to be passed as the object. However, `proxy`, as a
  // MaybeCrossOriginObject, may not be in current cx->realm() and we may now
  // have a cross-compartment wrapper for `proxy`.
  return JS::InstanceofOperator(cx, proxyWrap, v, bp);
}

// Force instantiations of the out-of-line template methods we need.
template class MaybeCrossOriginObject<js::Wrapper>;
template class MaybeCrossOriginObject<DOMProxyHandler>;

}  // namespace mozilla::dom
