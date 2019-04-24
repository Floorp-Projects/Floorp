/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Compartment-inl.h"

#include "mozilla/MemoryReporting.h"

#include <stddef.h>

#include "jsfriendapi.h"

#include "gc/Policy.h"
#include "gc/PublicIterators.h"
#include "js/Date.h"
#include "js/Proxy.h"
#include "js/RootingAPI.h"
#include "js/StableStringChars.h"
#include "js/Wrapper.h"
#include "proxy/DeadObjectProxy.h"
#include "vm/Debugger.h"
#include "vm/Iteration.h"
#include "vm/JSContext.h"
#include "vm/WrapperObject.h"

#include "gc/GC-inl.h"
#include "gc/Marking-inl.h"
#include "vm/JSAtom-inl.h"
#include "vm/JSFunction-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;

using JS::AutoStableStringChars;

Compartment::Compartment(Zone* zone, bool invisibleToDebugger)
    : zone_(zone),
      runtime_(zone->runtimeFromAnyThread()),
      invisibleToDebugger_(invisibleToDebugger),
      crossCompartmentWrappers(0) {}

#ifdef JSGC_HASH_TABLE_CHECKS

void Compartment::checkWrapperMapAfterMovingGC() {
  /*
   * Assert that the postbarriers have worked and that nothing is left in
   * wrapperMap that points into the nursery, and that the hash table entries
   * are discoverable.
   */
  for (WrapperMap::Enum e(crossCompartmentWrappers); !e.empty(); e.popFront()) {
    auto checkGCThing = [](auto tp) { CheckGCThingAfterMovingGC(*tp); };
    e.front().mutableKey().applyToWrapped(checkGCThing);
    e.front().mutableKey().applyToDebugger(checkGCThing);

    WrapperMap::Ptr ptr = crossCompartmentWrappers.lookup(e.front().key());
    MOZ_RELEASE_ASSERT(ptr.found() && &*ptr == &e.front());
  }
}

#endif  // JSGC_HASH_TABLE_CHECKS

bool Compartment::putWrapper(JSContext* cx, const CrossCompartmentKey& wrapped,
                             const js::Value& wrapper) {
  MOZ_ASSERT(wrapped.is<JSString*>() == wrapper.isString());
  MOZ_ASSERT_IF(!wrapped.is<JSString*>(), wrapper.isObject());

  if (!crossCompartmentWrappers.put(wrapped, wrapper)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

static JSString* CopyStringPure(JSContext* cx, JSString* str) {
  /*
   * Directly allocate the copy in the destination compartment, rather than
   * first flattening it (and possibly allocating in source compartment),
   * because we don't know whether the flattening will pay off later.
   */

  size_t len = str->length();
  JSString* copy;
  if (str->isLinear()) {
    /* Only use AutoStableStringChars if the NoGC allocation fails. */
    if (str->hasLatin1Chars()) {
      JS::AutoCheckCannotGC nogc;
      copy = NewStringCopyN<NoGC>(cx, str->asLinear().latin1Chars(nogc), len);
    } else {
      JS::AutoCheckCannotGC nogc;
      copy = NewStringCopyNDontDeflate<NoGC>(
          cx, str->asLinear().twoByteChars(nogc), len);
    }
    if (copy) {
      return copy;
    }

    AutoStableStringChars chars(cx);
    if (!chars.init(cx, str)) {
      return nullptr;
    }

    return chars.isLatin1() ? NewStringCopyN<CanGC>(
                                  cx, chars.latin1Range().begin().get(), len)
                            : NewStringCopyNDontDeflate<CanGC>(
                                  cx, chars.twoByteRange().begin().get(), len);
  }

  if (str->hasLatin1Chars()) {
    UniquePtr<Latin1Char[], JS::FreePolicy> copiedChars =
        str->asRope().copyLatin1CharsZ(cx, js::StringBufferArena);
    if (!copiedChars) {
      return nullptr;
    }

    return NewString<CanGC>(cx, std::move(copiedChars), len);
  }

  UniqueTwoByteChars copiedChars =
      str->asRope().copyTwoByteCharsZ(cx, js::StringBufferArena);
  if (!copiedChars) {
    return nullptr;
  }

  return NewStringDontDeflate<CanGC>(cx, std::move(copiedChars), len);
}

bool Compartment::wrap(JSContext* cx, MutableHandleString strp) {
  MOZ_ASSERT(cx->compartment() == this);

  /* If the string is already in this compartment, we are done. */
  JSString* str = strp;
  if (str->zoneFromAnyThread() == zone()) {
    return true;
  }

  /*
   * If the string is an atom, we don't have to copy, but we do need to mark
   * the atom as being in use by the new zone.
   */
  if (str->isAtom()) {
    cx->markAtom(&str->asAtom());
    return true;
  }

  /* Check the cache. */
  RootedValue key(cx, StringValue(str));
  if (WrapperMap::Ptr p =
          crossCompartmentWrappers.lookup(CrossCompartmentKey(key))) {
    strp.set(p->value().get().toString());
    return true;
  }

  /* No dice. Make a copy, and cache it. */
  JSString* copy = CopyStringPure(cx, str);
  if (!copy) {
    return false;
  }
  if (!putWrapper(cx, CrossCompartmentKey(key), StringValue(copy))) {
    return false;
  }

  strp.set(copy);
  return true;
}

bool Compartment::wrap(JSContext* cx, MutableHandleBigInt bi) {
  MOZ_ASSERT(cx->compartment() == this);

  if (bi->zone() == cx->zone()) {
    return true;
  }

  BigInt* copy = BigInt::copy(cx, bi);
  if (!copy) {
    return false;
  }
  bi.set(copy);
  return true;
}

bool Compartment::getNonWrapperObjectForCurrentCompartment(
    JSContext* cx, MutableHandleObject obj) {
  // Ensure that we have entered a realm.
  MOZ_ASSERT(cx->global());

  // If we have a cross-compartment wrapper, make sure that the cx isn't
  // associated with the self-hosting zone. We don't want to create
  // wrappers for objects in other runtimes, which may be the case for the
  // self-hosting zone.
  MOZ_ASSERT(!cx->runtime()->isSelfHostingZone(cx->zone()));
  MOZ_ASSERT(!cx->runtime()->isSelfHostingZone(obj->zone()));

  // The object is already in the right compartment. Normally same-
  // compartment returns the object itself, however, windows are always
  // wrapped by a proxy, so we have to check for that case here manually.
  if (obj->compartment() == this) {
    obj.set(ToWindowProxyIfWindow(obj));
    return true;
  }

  // Note that if the object is same-compartment, but has been wrapped into a
  // different compartment, we need to unwrap it and return the bare same-
  // compartment object. Note again that windows are always wrapped by a
  // WindowProxy even when same-compartment so take care not to strip this
  // particular wrapper.
  RootedObject objectPassedToWrap(cx, obj);
  obj.set(UncheckedUnwrap(obj, /* stopAtWindowProxy = */ true));
  if (obj->compartment() == this) {
    MOZ_ASSERT(!IsWindow(obj));
    return true;
  }

  // Disallow creating new wrappers if we nuked the object's realm or the
  // current compartment.
  if (!AllowNewWrapper(this, obj)) {
    obj.set(NewDeadProxyObject(cx, IsCallableFlag(obj->isCallable()),
                               IsConstructorFlag(obj->isConstructor())));
    return !!obj;
  }

  // Use the WindowProxy instead of the Window here, so that we don't have to
  // deal with this in the rest of the wrapping code.
  if (IsWindow(obj)) {
    obj.set(ToWindowProxyIfWindow(obj));

    // ToWindowProxyIfWindow can return a CCW if |obj| was a navigated-away-from
    // Window. Strip any CCWs.
    obj.set(UncheckedUnwrap(obj));

    if (JS_IsDeadWrapper(obj)) {
      obj.set(NewDeadProxyObject(cx, obj));
      return !!obj;
    }

    MOZ_ASSERT(IsWindowProxy(obj));

    // We crossed a compartment boundary there, so may now have a gray object.
    // This function is not allowed to return gray objects, so don't do that.
    ExposeObjectToActiveJS(obj);
  }

  // If the object is a dead wrapper, return a new dead wrapper rather than
  // trying to wrap it for a different compartment.
  if (JS_IsDeadWrapper(obj)) {
    obj.set(NewDeadProxyObject(cx, obj));
    return !!obj;
  }

  // Invoke the prewrap callback. The prewrap callback is responsible for
  // doing similar reification as above, but can account for any additional
  // embedder requirements.
  //
  // We're a bit worried about infinite recursion here, so we do a check -
  // see bug 809295.
  auto preWrap = cx->runtime()->wrapObjectCallbacks->preWrap;
  if (!CheckSystemRecursionLimit(cx)) {
    return false;
  }
  if (preWrap) {
    preWrap(cx, cx->global(), obj, objectPassedToWrap, obj);
    if (!obj) {
      return false;
    }
  }
  MOZ_ASSERT(!IsWindow(obj));

  return true;
}

bool Compartment::getOrCreateWrapper(JSContext* cx, HandleObject existing,
                                     MutableHandleObject obj) {
  // If we already have a wrapper for this value, use it.
  RootedValue key(cx, ObjectValue(*obj));
  if (WrapperMap::Ptr p =
          crossCompartmentWrappers.lookup(CrossCompartmentKey(key))) {
    obj.set(&p->value().get().toObject());
    MOZ_ASSERT(obj->is<CrossCompartmentWrapperObject>());
    return true;
  }

  // Ensure that the wrappee is exposed in case we are creating a new wrapper
  // for a gray object.
  ExposeObjectToActiveJS(obj);

  // Create a new wrapper for the object.
  auto wrap = cx->runtime()->wrapObjectCallbacks->wrap;
  RootedObject wrapper(cx, wrap(cx, existing, obj));
  if (!wrapper) {
    return false;
  }

  // We maintain the invariant that the key in the cross-compartment wrapper
  // map is always directly wrapped by the value.
  MOZ_ASSERT(Wrapper::wrappedObject(wrapper) == &key.get().toObject());

  if (!putWrapper(cx, CrossCompartmentKey(key), ObjectValue(*wrapper))) {
    // Enforce the invariant that all cross-compartment wrapper object are
    // in the map by nuking the wrapper if we couldn't add it.
    // Unfortunately it's possible for the wrapper to still be marked if we
    // took this path, for example if the object metadata callback stashes a
    // reference to it.
    if (wrapper->is<CrossCompartmentWrapperObject>()) {
      NukeCrossCompartmentWrapper(cx, wrapper);
    }
    return false;
  }

  obj.set(wrapper);
  return true;
}

bool Compartment::wrap(JSContext* cx, MutableHandleObject obj) {
  MOZ_ASSERT(cx->compartment() == this);

  if (!obj) {
    return true;
  }

  AutoDisableProxyCheck adpc;

  // Anything we're wrapping has already escaped into script, so must have
  // been unmarked-gray at some point in the past.
  JS::AssertObjectIsNotGray(obj);

  // The passed object may already be wrapped, or may fit a number of special
  // cases that we need to check for and manually correct.
  if (!getNonWrapperObjectForCurrentCompartment(cx, obj)) {
    return false;
  }

  // If the reification above did not result in a same-compartment object,
  // get or create a new wrapper object in this compartment for it.
  if (obj->compartment() != this) {
    if (!getOrCreateWrapper(cx, nullptr, obj)) {
      return false;
    }
  }

  // Ensure that the wrapper is also exposed.
  ExposeObjectToActiveJS(obj);
  return true;
}

bool Compartment::rewrap(JSContext* cx, MutableHandleObject obj,
                         HandleObject existingArg) {
  MOZ_ASSERT(cx->compartment() == this);
  MOZ_ASSERT(obj);
  MOZ_ASSERT(existingArg);
  MOZ_ASSERT(existingArg->compartment() == cx->compartment());
  MOZ_ASSERT(IsDeadProxyObject(existingArg));

  AutoDisableProxyCheck adpc;

  // It may not be possible to re-use existing; if so, clear it so that we
  // are forced to create a new wrapper. Note that this cannot call out to
  // |wrap| because of the different gray unmarking semantics.
  RootedObject existing(cx, existingArg);
  if (existing->hasStaticPrototype() ||
      // Note: Class asserted above, so all that's left to check is callability
      existing->isCallable() || obj->isCallable()) {
    existing.set(nullptr);
  }

  // The passed object may already be wrapped, or may fit a number of special
  // cases that we need to check for and manually correct.
  if (!getNonWrapperObjectForCurrentCompartment(cx, obj)) {
    return false;
  }

  // If the reification above resulted in a same-compartment object, we do
  // not need to create or return an existing wrapper.
  if (obj->compartment() == this) {
    return true;
  }

  return getOrCreateWrapper(cx, existing, obj);
}

bool Compartment::wrap(JSContext* cx,
                       MutableHandle<JS::PropertyDescriptor> desc) {
  if (!wrap(cx, desc.object())) {
    return false;
  }

  if (desc.hasGetterObject()) {
    if (!wrap(cx, desc.getterObject())) {
      return false;
    }
  }
  if (desc.hasSetterObject()) {
    if (!wrap(cx, desc.setterObject())) {
      return false;
    }
  }

  return wrap(cx, desc.value());
}

bool Compartment::wrap(JSContext* cx, MutableHandle<GCVector<Value>> vec) {
  for (size_t i = 0; i < vec.length(); ++i) {
    if (!wrap(cx, vec[i])) {
      return false;
    }
  }
  return true;
}

void Compartment::traceOutgoingCrossCompartmentWrappers(JSTracer* trc) {
  MOZ_ASSERT(JS::RuntimeHeapIsMajorCollecting());
  MOZ_ASSERT(!zone()->isCollectingFromAnyThread() ||
             trc->runtime()->gc.isHeapCompacting());

  for (NonStringWrapperEnum e(this); !e.empty(); e.popFront()) {
    if (e.front().key().is<JSObject*>()) {
      Value v = e.front().value().unbarrieredGet();
      ProxyObject* wrapper = &v.toObject().as<ProxyObject>();

      /*
       * We have a cross-compartment wrapper. Its private pointer may
       * point into the compartment being collected, so we should mark it.
       */
      ProxyObject::traceEdgeToTarget(trc, wrapper);
    }
  }
}

/* static */
void Compartment::traceIncomingCrossCompartmentEdgesForZoneGC(JSTracer* trc) {
  gcstats::AutoPhase ap(trc->runtime()->gc.stats(),
                        gcstats::PhaseKind::MARK_CCWS);
  MOZ_ASSERT(JS::RuntimeHeapIsMajorCollecting());
  for (CompartmentsIter c(trc->runtime()); !c.done(); c.next()) {
    if (!c->zone()->isCollecting()) {
      c->traceOutgoingCrossCompartmentWrappers(trc);
    }
  }
  Debugger::traceIncomingCrossCompartmentEdges(trc);
}

void Compartment::sweepAfterMinorGC(JSTracer* trc) {
  crossCompartmentWrappers.sweepAfterMinorGC(trc);

  for (RealmsInCompartmentIter r(this); !r.done(); r.next()) {
    r->sweepAfterMinorGC();
  }
}

/*
 * Remove dead wrappers from the table. We must sweep all compartments, since
 * string entries in the crossCompartmentWrappers table are not marked during
 * markCrossCompartmentWrappers.
 */
void Compartment::sweepCrossCompartmentWrappers() {
  crossCompartmentWrappers.sweep();
}

void CrossCompartmentKey::trace(JSTracer* trc) {
  applyToWrapped(
      [trc](auto tp) { TraceRoot(trc, tp, "CrossCompartmentKey::wrapped"); });
  applyToDebugger(
      [trc](auto tp) { TraceRoot(trc, tp, "CrossCompartmentKey::debugger"); });
}

bool CrossCompartmentKey::needsSweep() {
  auto needsSweep = [](auto tp) { return IsAboutToBeFinalizedUnbarriered(tp); };
  return applyToWrapped(needsSweep) || applyToDebugger(needsSweep);
}

/* static */
void Compartment::fixupCrossCompartmentWrappersAfterMovingGC(JSTracer* trc) {
  MOZ_ASSERT(trc->runtime()->gc.isHeapCompacting());

  for (CompartmentsIter comp(trc->runtime()); !comp.done(); comp.next()) {
    // Sweep the wrapper map to update keys (wrapped values) in other
    // compartments that may have been moved.
    comp->sweepCrossCompartmentWrappers();
    // Trace the wrappers in the map to update their cross-compartment edges
    // to wrapped values in other compartments that may have been moved.
    comp->traceOutgoingCrossCompartmentWrappers(trc);
  }
}

void Compartment::fixupAfterMovingGC() {
  MOZ_ASSERT(zone()->isGCCompacting());

  for (RealmsInCompartmentIter r(this); !r.done(); r.next()) {
    r->fixupAfterMovingGC();
  }

  // Sweep the wrapper map to update values (wrapper objects) in this
  // compartment that may have been moved.
  sweepCrossCompartmentWrappers();
}

void Compartment::addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                         size_t* compartmentObjects,
                                         size_t* crossCompartmentWrappersTables,
                                         size_t* compartmentsPrivateData) {
  *compartmentObjects += mallocSizeOf(this);
  *crossCompartmentWrappersTables +=
      crossCompartmentWrappers.sizeOfExcludingThis(mallocSizeOf);

  if (auto callback = runtime_->sizeOfIncludingThisCompartmentCallback) {
    *compartmentsPrivateData += callback(mallocSizeOf, this);
  }
}

GlobalObject& Compartment::firstGlobal() const {
  for (Realm* realm : realms_) {
    if (!realm->hasLiveGlobal()) {
      continue;
    }
    GlobalObject* global = realm->maybeGlobal();
    ExposeObjectToActiveJS(global);
    return *global;
  }
  MOZ_CRASH("If all our globals are dead, why is someone expecting a global?");
}

JS_FRIEND_API JSObject* js::GetFirstGlobalInCompartment(JS::Compartment* comp) {
  return &comp->firstGlobal();
}

JS_FRIEND_API bool js::CompartmentHasLiveGlobal(JS::Compartment* comp) {
  MOZ_ASSERT(comp);
  for (Realm* r : comp->realms()) {
    if (r->hasLiveGlobal()) {
      return true;
    }
  }
  return false;
}
