/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * GC support for FinalizationRegistry and WeakRef objects.
 */

#include "gc/FinalizationObservers.h"

#include "mozilla/ScopeExit.h"

#include "builtin/FinalizationRegistryObject.h"
#include "builtin/WeakRefObject.h"
#include "gc/GCInternals.h"
#include "gc/GCRuntime.h"
#include "gc/Zone.h"
#include "vm/JSContext.h"

#include "gc/PrivateIterators-inl.h"
#include "gc/WeakMap-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::gc;

FinalizationObservers::FinalizationObservers(Zone* zone)
    : zone(zone),
      registries(zone),
      recordMap(zone),
      crossZoneWrappers(zone),
      weakRefMap(zone) {}

FinalizationObservers::~FinalizationObservers() {
  MOZ_ASSERT(registries.empty());
  MOZ_ASSERT(recordMap.empty());
  MOZ_ASSERT(crossZoneWrappers.empty());
}

bool GCRuntime::addFinalizationRegistry(
    JSContext* cx, Handle<FinalizationRegistryObject*> registry) {
  if (!cx->zone()->ensureFinalizationObservers() ||
      !cx->zone()->finalizationObservers()->addRegistry(registry)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

bool FinalizationObservers::addRegistry(
    Handle<FinalizationRegistryObject*> registry) {
  return registries.put(registry);
}

bool GCRuntime::registerWithFinalizationRegistry(JSContext* cx,
                                                 HandleObject target,
                                                 HandleObject record) {
  MOZ_ASSERT(!IsCrossCompartmentWrapper(target));
  MOZ_ASSERT(
      UncheckedUnwrapWithoutExpose(record)->is<FinalizationRecordObject>());
  MOZ_ASSERT(target->compartment() == record->compartment());

  Zone* zone = cx->zone();
  if (!zone->ensureFinalizationObservers() ||
      !zone->finalizationObservers()->addRecord(target, record)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

bool FinalizationObservers::addRecord(HandleObject target,
                                      HandleObject record) {
  // Add a record to the record map and clean up on failure.
  //
  // The following must be updated and kept in sync:
  //  - the zone's recordMap (to observe the target)
  //  - the registry's global objects's recordSet (to trace the record)
  //  - the count of cross zone records (to calculate sweep groups)

  MOZ_ASSERT(target->zone() == zone);

  FinalizationRecordObject* unwrappedRecord =
      &UncheckedUnwrapWithoutExpose(record)->as<FinalizationRecordObject>();

  Zone* registryZone = unwrappedRecord->zone();
  bool crossZone = registryZone != zone;
  if (crossZone && !addCrossZoneWrapper(record)) {
    return false;
  }
  auto wrapperGuard = mozilla::MakeScopeExit([&] {
    if (crossZone) {
      removeCrossZoneWrapper(record);
    }
  });

  GlobalObject* registryGlobal = &unwrappedRecord->global();
  auto* globalData = registryGlobal->getOrCreateFinalizationRegistryData();
  if (!globalData || !globalData->addRecord(unwrappedRecord)) {
    return false;
  }
  auto globalDataGuard = mozilla::MakeScopeExit(
      [&] { globalData->removeRecord(unwrappedRecord); });

  auto ptr = recordMap.lookupForAdd(target);
  if (!ptr && !recordMap.add(ptr, target, RecordVector(zone))) {
    return false;
  }

  if (!ptr->value().append(record)) {
    return false;
  }

  unwrappedRecord->setInRecordMap(true);

  globalDataGuard.release();
  wrapperGuard.release();
  return true;
}

bool FinalizationObservers::addCrossZoneWrapper(JSObject* wrapper) {
  MOZ_ASSERT(IsCrossCompartmentWrapper(wrapper));
  MOZ_ASSERT(UncheckedUnwrapWithoutExpose(wrapper)->zone() != zone);

  auto ptr = crossZoneWrappers.lookupForAdd(wrapper);
  MOZ_ASSERT(!ptr);
  return crossZoneWrappers.add(ptr, wrapper, UndefinedValue());
}

void FinalizationObservers::removeCrossZoneWrapper(JSObject* wrapper) {
  MOZ_ASSERT(IsCrossCompartmentWrapper(wrapper));
  MOZ_ASSERT(UncheckedUnwrapWithoutExpose(wrapper)->zone() != zone);

  auto ptr = crossZoneWrappers.lookupForAdd(wrapper);
  MOZ_ASSERT(ptr);
  crossZoneWrappers.remove(ptr);
}

static FinalizationRecordObject* UnwrapFinalizationRecord(JSObject* obj) {
  obj = UncheckedUnwrapWithoutExpose(obj);
  if (!obj->is<FinalizationRecordObject>()) {
    MOZ_ASSERT(JS_IsDeadWrapper(obj));
    // CCWs between the compartments have been nuked. The
    // FinalizationRegistry's callback doesn't run in this case.
    return nullptr;
  }
  return &obj->as<FinalizationRecordObject>();
}

void FinalizationObservers::clearRecords() {
#ifdef DEBUG
  // Check crossZoneWrappers was correct before clearing.
  for (RecordMap::Enum e(recordMap); !e.empty(); e.popFront()) {
    for (JSObject* object : e.front().value()) {
      FinalizationRecordObject* record = UnwrapFinalizationRecord(object);
      if (record && record->zone() != zone) {
        removeCrossZoneWrapper(object);
      }
    }
  }
  MOZ_ASSERT(crossZoneWrappers.empty());
#endif

  recordMap.clear();
  crossZoneWrappers.clear();
}

void GCRuntime::traceWeakFinalizationObserverEdges(JSTracer* trc, Zone* zone) {
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(trc->runtime()));
  FinalizationObservers* observers = zone->finalizationObservers();
  if (observers) {
    observers->traceWeakEdges(trc);
  }
}

void FinalizationObservers::traceRoots(JSTracer* trc) {
  // The crossZoneWrappers weak map is traced as a root; this does not keep any
  // of its entries alive by itself.
  crossZoneWrappers.trace(trc);
}

void FinalizationObservers::traceWeakEdges(JSTracer* trc) {
  // Sweep weak ref data.
  weakRefMap.traceWeak(trc);

  // Sweep finalization registry data and queue finalization records for cleanup
  // for any entries whose target is dying and remove them from the map.

  GCRuntime* gc = &trc->runtime()->gc;

  for (RegistrySet::Enum e(registries); !e.empty(); e.popFront()) {
    auto result = TraceWeakEdge(trc, &e.mutableFront(), "FinalizationRegistry");
    if (result.isDead()) {
      auto* registry =
          &result.initialTarget()->as<FinalizationRegistryObject>();
      registry->queue()->setHasRegistry(false);
      e.removeFront();
    } else {
      result.finalTarget()->as<FinalizationRegistryObject>().traceWeak(trc);
    }
  }

  for (RecordMap::Enum e(recordMap); !e.empty(); e.popFront()) {
    RecordVector& records = e.front().value();

    // Sweep finalization records, updating any pointers moved by the GC and
    // remove if necessary.
    records.mutableEraseIf([&](HeapPtrObject& heapPtr) {
      auto result = TraceWeakEdge(trc, &heapPtr, "FinalizationRecord");
      JSObject* obj =
          result.isLive() ? result.finalTarget() : result.initialTarget();
      FinalizationRecordObject* record = UnwrapFinalizationRecord(obj);

      bool shouldRemove = !result.isLive() || shouldRemoveRecord(record);
      if (shouldRemove && record && record->isInRecordMap()) {
        updateForRemovedRecord(obj, record);
      }

      return shouldRemove;
    });

#ifdef DEBUG
    for (JSObject* obj : records) {
      MOZ_ASSERT(UnwrapFinalizationRecord(obj)->isInRecordMap());
    }
#endif

    // Queue finalization records for targets that are dying.
    if (!TraceWeakEdge(trc, &e.front().mutableKey(),
                       "FinalizationRecord target")) {
      for (JSObject* obj : records) {
        FinalizationRecordObject* record = UnwrapFinalizationRecord(obj);
        FinalizationQueueObject* queue = record->queue();
        updateForRemovedRecord(obj, record);
        queue->queueRecordToBeCleanedUp(record);
        gc->queueFinalizationRegistryForCleanup(queue);
      }
      e.removeFront();
    }
  }
}

// static
bool FinalizationObservers::shouldRemoveRecord(
    FinalizationRecordObject* record) {
  // Records are removed from the target's vector for the following reasons:
  return !record ||                        // Nuked CCW to record.
         !record->isRegistered() ||        // Unregistered record.
         !record->queue()->hasRegistry();  // Dead finalization registry.
}

void FinalizationObservers::updateForRemovedRecord(
    JSObject* wrapper, FinalizationRecordObject* record) {
  // Remove other references to a record when it has been removed from the
  // zone's record map. See addRecord().
  MOZ_ASSERT(record->isInRecordMap());

  Zone* registryZone = record->zone();
  if (registryZone != zone) {
    removeCrossZoneWrapper(wrapper);
  }

  GlobalObject* registryGlobal = &record->global();
  auto* globalData = registryGlobal->maybeFinalizationRegistryData();
  globalData->removeRecord(record);

  // The removed record may be gray, and that's OK.
  AutoTouchingGrayThings atgt;

  record->setInRecordMap(false);
}

void GCRuntime::nukeFinalizationRecordWrapper(
    JSObject* wrapper, FinalizationRecordObject* record) {
  if (record->isInRecordMap()) {
    FinalizationRegistryObject::unregisterRecord(record);
    FinalizationObservers* observers = wrapper->zone()->finalizationObservers();
    observers->updateForRemovedRecord(wrapper, record);
  }
}

void GCRuntime::queueFinalizationRegistryForCleanup(
    FinalizationQueueObject* queue) {
  // Prod the embedding to call us back later to run the finalization callbacks,
  // if necessary.

  if (queue->isQueuedForCleanup()) {
    return;
  }

  // Derive the incumbent global by unwrapping the incumbent global object and
  // then getting its global.
  JSObject* object = UncheckedUnwrapWithoutExpose(queue->incumbentObject());
  MOZ_ASSERT(object);
  GlobalObject* incumbentGlobal = &object->nonCCWGlobal();

  callHostCleanupFinalizationRegistryCallback(queue->doCleanupFunction(),
                                              incumbentGlobal);

  // The queue object may be gray, and that's OK.
  AutoTouchingGrayThings atgt;

  queue->setQueuedForCleanup(true);
}

bool GCRuntime::registerWeakRef(HandleObject target, HandleObject weakRef) {
  MOZ_ASSERT(!IsCrossCompartmentWrapper(target));
  MOZ_ASSERT(UncheckedUnwrap(weakRef)->is<WeakRefObject>());
  MOZ_ASSERT(target->compartment() == weakRef->compartment());

  Zone* zone = target->zone();
  return zone->ensureFinalizationObservers() &&
         zone->finalizationObservers()->addWeakRefTarget(target, weakRef);
}

bool FinalizationObservers::addWeakRefTarget(HandleObject target,
                                             HandleObject weakRef) {
  auto ptr = weakRefMap.lookupForAdd(target);
  if (!ptr && !weakRefMap.add(ptr, target, WeakRefHeapPtrVector(zone))) {
    return false;
  }

  auto& refs = ptr->value();
  return refs.emplaceBack(weakRef);
}

void GCRuntime::nukeWeakRefWrapper(JSObject* wrapper, WeakRefObject* weakRef) {
  FinalizationObservers* observers = wrapper->zone()->finalizationObservers();
  if (!observers) {
    return;
  }

  if (observers->unregisterWeakRefWrapper(wrapper, weakRef)) {
    weakRef->clearTarget();
  }
}

bool FinalizationObservers::unregisterWeakRefWrapper(JSObject* wrapper,
                                                     WeakRefObject* weakRef) {
  JSObject* target = weakRef->target();
  MOZ_ASSERT(target);

  bool removed = false;
  WeakRefHeapPtrVector& weakRefs = weakRefMap.lookup(target)->value();
  weakRefs.eraseIf([wrapper, &removed](JSObject* obj) {
    bool remove = obj == wrapper;
    if (remove) {
      removed = true;
    }
    return remove;
  });

  return removed;
}

static WeakRefObject* UnwrapWeakRef(JSObject* obj) {
  MOZ_ASSERT(!JS_IsDeadWrapper(obj));
  obj = UncheckedUnwrapWithoutExpose(obj);
  return &obj->as<WeakRefObject>();
}

void WeakRefMap::traceWeak(JSTracer* trc) {
  for (Enum e(*this); !e.empty(); e.popFront()) {
    // If target is dying, clear the target field of all weakRefs, and remove
    // the entry from the map.
    auto result = TraceWeakEdge(trc, &e.front().mutableKey(), "WeakRef target");
    if (result.isDead()) {
      for (JSObject* obj : e.front().value()) {
        UnwrapWeakRef(obj)->clearTarget();
      }
      e.removeFront();
    } else {
      // Update the target field after compacting.
      e.front().value().traceWeak(trc, result.finalTarget());
    }
  }
}

// Like GCVector::sweep, but this method will also update the target in every
// weakRef in this GCVector.
void WeakRefHeapPtrVector::traceWeak(JSTracer* trc, JSObject* target) {
  mutableEraseIf([&](HeapPtrObject& obj) -> bool {
    auto result = TraceWeakEdge(trc, &obj, "WeakRef");
    if (result.isDead()) {
      UnwrapWeakRef(result.initialTarget())->clearTarget();
    } else {
      UnwrapWeakRef(result.finalTarget())->setTargetUnbarriered(target);
    }
    return result.isDead();
  });
}

FinalizationRegistryGlobalData::FinalizationRegistryGlobalData(Zone* zone)
    : recordSet(zone) {}

bool FinalizationRegistryGlobalData::addRecord(
    FinalizationRecordObject* record) {
  return recordSet.putNew(record);
}

void FinalizationRegistryGlobalData::removeRecord(
    FinalizationRecordObject* record) {
  MOZ_ASSERT(recordSet.has(record));
  recordSet.remove(record);
}

void FinalizationRegistryGlobalData::trace(JSTracer* trc) {
  recordSet.trace(trc);
}
