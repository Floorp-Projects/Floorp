/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ProxyObject.h"

#include "gc/Allocator.h"
#include "gc/GCTrace.h"
#include "proxy/DeadObjectProxy.h"
#include "vm/Realm.h"

#include "gc/ObjectKind-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;

static gc::AllocKind GetProxyGCObjectKind(const JSClass* clasp,
                                          const BaseProxyHandler* handler,
                                          const Value& priv) {
  MOZ_ASSERT(clasp->isProxy());

  uint32_t nreserved = JSCLASS_RESERVED_SLOTS(clasp);

  // For now assert each Proxy Class has at least 1 reserved slot. This is
  // not a hard requirement, but helps catch Classes that need an explicit
  // JSCLASS_HAS_RESERVED_SLOTS since bug 1360523.
  MOZ_ASSERT(nreserved > 0);

  MOZ_ASSERT(
      js::detail::ProxyValueArray::sizeOf(nreserved) % sizeof(Value) == 0,
      "ProxyValueArray must be a multiple of Value");

  uint32_t nslots =
      js::detail::ProxyValueArray::sizeOf(nreserved) / sizeof(Value);
  MOZ_ASSERT(nslots <= NativeObject::MAX_FIXED_SLOTS);

  gc::AllocKind kind = gc::GetGCObjectKind(nslots);
  if (handler->finalizeInBackground(priv)) {
    kind = ForegroundToBackgroundAllocKind(kind);
  }

  return kind;
}

void ProxyObject::init(const BaseProxyHandler* handler, HandleValue priv,
                       JSContext* cx) {
  setInlineValueArray();

  detail::ProxyValueArray* values = detail::GetProxyDataLayout(this)->values();
  values->init(numReservedSlots());

  data.handler = handler;

  if (IsCrossCompartmentWrapper(this)) {
    MOZ_ASSERT(cx->global() == &cx->compartment()->globalForNewCCW());
    setCrossCompartmentPrivate(priv);
  } else {
    setSameCompartmentPrivate(priv);
  }
}

/* static */
ProxyObject* ProxyObject::New(JSContext* cx, const BaseProxyHandler* handler,
                              HandleValue priv, TaggedProto proto_,
                              const JSClass* clasp) {
  Rooted<TaggedProto> proto(cx, proto_);

  MOZ_ASSERT(clasp->isProxy());
  MOZ_ASSERT(isValidProxyClass(clasp));
  MOZ_ASSERT(clasp->shouldDelayMetadataBuilder());
  MOZ_ASSERT_IF(proto.isObject(),
                cx->compartment() == proto.toObject()->compartment());
  MOZ_ASSERT(clasp->hasFinalize());

#ifdef DEBUG
  if (priv.isGCThing()) {
    JS::AssertCellIsNotGray(priv.toGCThing());
  }
#endif

  /*
   * Eagerly mark properties unknown for proxies, so we don't try to track
   * their properties and so that we don't need to walk the compartment if
   * their prototype changes later.  But don't do this for DOM proxies,
   * because we want to be able to keep track of them in typesets in useful
   * ways.
   */
  if (proto.isObject() && !clasp->isDOMClass()) {
    ObjectGroupRealm& realm = ObjectGroupRealm::getForNewObject(cx);
    RootedObject protoObj(cx, proto.toObject());
    if (!JSObject::setNewGroupUnknown(cx, realm, clasp, protoObj)) {
      return nullptr;
    }
  }

  gc::AllocKind allocKind = GetProxyGCObjectKind(clasp, handler, priv);

  Realm* realm = cx->realm();

  AutoSetNewObjectMetadata metadata(cx);
  // Try to look up the group and shape in the NewProxyCache.
  RootedObjectGroup group(cx);
  RootedShape shape(cx);
  if (!realm->newProxyCache.lookup(clasp, proto, group.address(),
                                   shape.address())) {
    group = ObjectGroup::defaultNewGroup(cx, clasp, proto, nullptr);
    if (!group) {
      return nullptr;
    }

    shape = EmptyShape::getInitialShape(cx, clasp, proto, /* nfixed = */ 0);
    if (!shape) {
      return nullptr;
    }

    realm->newProxyCache.add(group, shape);
  }

  MOZ_ASSERT(group->realm() == realm);
  MOZ_ASSERT(shape->zone() == cx->zone());
  MOZ_ASSERT(!IsAboutToBeFinalizedUnbarriered(group.address()));
  MOZ_ASSERT(!IsAboutToBeFinalizedUnbarriered(shape.address()));

  // Ensure that the wrapper has the same lifetime assumptions as the
  // wrappee. Prefer to allocate in the nursery, when possible.
  bool privIsTenured = priv.isGCThing() && priv.toGCThing()->isTenured();
  gc::InitialHeap heap = privIsTenured || !handler->canNurseryAllocate()
                             ? gc::TenuredHeap
                             : gc::DefaultHeap;

  debugCheckNewObject(group, shape, allocKind, heap);

  JSObject* obj =
      AllocateObject(cx, allocKind, /* nDynamicSlots = */ 0, heap, clasp);
  if (!obj) {
    return nullptr;
  }

  ProxyObject* proxy = static_cast<ProxyObject*>(obj);
  proxy->initGroup(group);
  proxy->initShape(shape);

  MOZ_ASSERT(clasp->shouldDelayMetadataBuilder());
  realm->setObjectPendingMetadata(cx, proxy);

  js::gc::gcTracer.traceCreateObject(proxy);

  proxy->init(handler, priv, cx);

  // Don't track types of properties of non-DOM and non-singleton proxies.
  if (!clasp->isDOMClass()) {
    MarkObjectGroupUnknownProperties(cx, proxy->group());
  }

  return proxy;
}

/* static */
ProxyObject* ProxyObject::NewSingleton(JSContext* cx,
                                       const BaseProxyHandler* handler,
                                       HandleValue priv, TaggedProto proto_,
                                       const JSClass* clasp) {
  Rooted<TaggedProto> proto(cx, proto_);

  MOZ_ASSERT(clasp->isProxy());
  MOZ_ASSERT(isValidProxyClass(clasp));
  MOZ_ASSERT(clasp->shouldDelayMetadataBuilder());
  MOZ_ASSERT_IF(proto.isObject(),
                cx->compartment() == proto.toObject()->compartment());
  MOZ_ASSERT(clasp->hasFinalize());

#ifdef DEBUG
  if (priv.isGCThing()) {
    JS::AssertCellIsNotGray(priv.toGCThing());
  }
#endif

  gc::AllocKind allocKind = GetProxyGCObjectKind(clasp, handler, priv);

  AutoSetNewObjectMetadata metadata(cx);
  Rooted<ProxyObject*> proxy(cx);
  {
    Realm* realm = cx->realm();

    // We're creating a singleton, so go straight to getting a singleton group,
    // from the singleton group cache (or creating it freshly if needed).
    RootedObjectGroup group(cx, ObjectGroup::lazySingletonGroup(
                                    cx, ObjectGroupRealm::getForNewObject(cx),
                                    realm, clasp, proto));
    if (!group) {
      return nullptr;
    }

    MOZ_ASSERT(group->realm() == realm);
    MOZ_ASSERT(group->singleton());
    MOZ_ASSERT(!IsAboutToBeFinalizedUnbarriered(group.address()));

    // Also retrieve an empty shape.  Unlike for non-singleton proxies, this
    // shape lookup is not cached in |realm->newProxyCache|.  We could cache it
    // there, but distinguishing group/shape for singleton and non-singleton
    // proxies would increase contention on the cache (and might end up evicting
    // non-singleton cases where performance really matters).  Assume that
    // singleton proxies are rare, and don't bother caching their shapes/groups.
    RootedShape shape(
        cx, EmptyShape::getInitialShape(cx, clasp, proto, /* nfixed = */ 0));
    if (!shape) {
      return nullptr;
    }

    MOZ_ASSERT(shape->zone() == cx->zone());
    MOZ_ASSERT(!IsAboutToBeFinalizedUnbarriered(shape.address()));

    gc::InitialHeap heap = gc::TenuredHeap;
    debugCheckNewObject(group, shape, allocKind, heap);

    JSObject* obj =
        AllocateObject(cx, allocKind, /* nDynamicSlots = */ 0, heap, clasp);
    if (!obj) {
      return nullptr;
    }

    proxy = static_cast<ProxyObject*>(obj);
    proxy->initGroup(group);
    proxy->initShape(shape);

    MOZ_ASSERT(clasp->shouldDelayMetadataBuilder());
    realm->setObjectPendingMetadata(cx, proxy);

    js::gc::gcTracer.traceCreateObject(proxy);
  }

  proxy->init(handler, priv, cx);

  MOZ_ASSERT(proxy->isSingleton());
  return proxy;
}

gc::AllocKind ProxyObject::allocKindForTenure() const {
  MOZ_ASSERT(usingInlineValueArray());
  Value priv = private_();
  return GetProxyGCObjectKind(getClass(), data.handler, priv);
}

void ProxyObject::setCrossCompartmentPrivate(const Value& priv) {
  setPrivate(priv);
}

void ProxyObject::setSameCompartmentPrivate(const Value& priv) {
  MOZ_ASSERT(IsObjectValueInCompartment(priv, compartment()));
  setPrivate(priv);
}

inline void ProxyObject::setPrivate(const Value& priv) {
  MOZ_ASSERT_IF(IsMarkedBlack(this) && priv.isGCThing(),
                !JS::GCThingIsMarkedGray(JS::GCCellPtr(priv)));
  *slotOfPrivate() = priv;
}

void ProxyObject::nuke() {
  // Clear the target reference and replaced it with a value that encodes
  // various information about the original target.
  setSameCompartmentPrivate(DeadProxyTargetValue(this));

  // Update the handler to make this a DeadObjectProxy.
  setHandler(&DeadObjectProxy::singleton);

  // The proxy's reserved slots are not cleared and will continue to be
  // traced. This avoids the possibility of triggering write barriers while
  // nuking proxies in dead compartments which could otherwise cause those
  // compartments to be kept alive. Note that these are slots cannot hold
  // cross compartment pointers, so this cannot cause the target compartment
  // to leak.
}

JS_FRIEND_API void js::detail::SetValueInProxy(Value* slot,
                                               const Value& value) {
  // Slots in proxies are not GCPtrValues, so do a cast whenever assigning
  // values to them which might trigger a barrier.
  *reinterpret_cast<GCPtrValue*>(slot) = value;
}
