/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Finalization registry GC implementation.
 */

#include "gc/FinalizationRegistry.h"

#include "mozilla/ScopeExit.h"

#include "builtin/FinalizationRegistryObject.h"
#include "gc/GCRuntime.h"
#include "gc/Zone.h"
#include "vm/JSContext.h"

#include "gc/PrivateIterators-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::gc;

FinalizationRegistryZone::FinalizationRegistryZone(Zone* zone)
    : zone(zone), registries(zone), recordMap(zone), crossZoneCount(zone) {}

FinalizationRegistryZone::~FinalizationRegistryZone() {
  MOZ_ASSERT(registries.empty());
  MOZ_ASSERT(recordMap.empty());
  MOZ_ASSERT(crossZoneCount.empty());
}

bool GCRuntime::addFinalizationRegistry(
    JSContext* cx, Handle<FinalizationRegistryObject*> registry) {
  if (!cx->zone()->ensureFinalizationRegistryZone() ||
      !cx->zone()->finalizationRegistryZone()->addRegistry(registry)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

bool FinalizationRegistryZone::addRegistry(
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
  if (!zone->ensureFinalizationRegistryZone() ||
      !zone->finalizationRegistryZone()->addRecord(target, record)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

bool FinalizationRegistryZone::addRecord(HandleObject target,
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
  if (crossZone && !incCrossZoneCount(registryZone)) {
    return false;
  }
  auto countGuard = mozilla::MakeScopeExit([&] {
    if (crossZone) {
      decCrossZoneCount(registryZone);
    }
  });

  GlobalObject* registryGlobal = &unwrappedRecord->global();
  auto* globalData = registryGlobal->getOrCreateFinalizationRegistryData();
  if (!globalData || !globalData->addRecord(record)) {
    return false;
  }
  auto globalDataGuard =
      mozilla::MakeScopeExit([&] { globalData->removeRecord(record); });

  auto ptr = recordMap.lookupForAdd(target);
  if (!ptr && !recordMap.add(ptr, target, RecordVector(zone))) {
    return false;
  }

  if (!ptr->value().append(record)) {
    return false;
  }

  unwrappedRecord->setInRecordMap(true);

  globalDataGuard.release();
  countGuard.release();
  return true;
}

bool FinalizationRegistryZone::incCrossZoneCount(Zone* otherZone) {
  MOZ_ASSERT(otherZone != zone);

  auto ptr = crossZoneCount.lookupForAdd(otherZone);
  if (!ptr && !crossZoneCount.add(ptr, otherZone, 0)) {
    return false;
  }

  ptr->value()++;
  return true;
}

void FinalizationRegistryZone::decCrossZoneCount(Zone* otherZone) {
  MOZ_ASSERT(otherZone != zone);

  auto ptr = crossZoneCount.lookup(otherZone);
  MOZ_ASSERT(ptr);
  MOZ_ASSERT(ptr->value() != 0);
  ptr->value()--;

  if (ptr->value() == 0) {
    crossZoneCount.remove(ptr);
  }
}

void FinalizationRegistryZone::clearRecords() {
  recordMap.clear();
  crossZoneCount.clear();
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

void GCRuntime::traceWeakFinalizationRegistryEdges(JSTracer* trc, Zone* zone) {
  FinalizationRegistryZone* frzone = zone->finalizationRegistryZone();
  if (frzone) {
    frzone->traceWeakEdges(trc);
  }
}

void FinalizationRegistryZone::traceWeakEdges(JSTracer* trc) {
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
bool FinalizationRegistryZone::shouldRemoveRecord(
    FinalizationRecordObject* record) {
  // Records are removed from the target's vector for the following reasons:
  return !record ||                        // Nuked CCW to record.
         !record->isRegistered() ||        // Unregistered record.
         !record->queue()->hasRegistry();  // Dead finalization registry.
}

void FinalizationRegistryZone::updateForRemovedRecord(
    JSObject* wrapper, FinalizationRecordObject* record) {
  // Remove other references to a record when it has been removed from the
  // zone's record map. See addRecord().
  MOZ_ASSERT(record->isInRecordMap());

  Zone* registryZone = record->zone();
  if (registryZone != zone) {
    decCrossZoneCount(registryZone);
  }

  GlobalObject* registryGlobal = &record->global();
  auto* globalData = registryGlobal->maybeFinalizationRegistryData();
  globalData->removeRecord(wrapper);

  // The removed record may be gray, and that's OK.
  AutoTouchingGrayThings atgt;

  record->setInRecordMap(false);
}

void GCRuntime::nukeFinalizationRecordWrapper(
    JSObject* wrapper, FinalizationRecordObject* record) {
  if (record->isInRecordMap()) {
    FinalizationRegistryObject::unregisterRecord(record);
    wrapper->zone()->finalizationRegistryZone()->updateForRemovedRecord(wrapper,
                                                                        record);
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

bool FinalizationRegistryZone::findSweepGroupEdges() {
  // Ensure finalization registries are swept in the same sweep group as their
  // targets.

  for (ZoneCountMap::Enum e(crossZoneCount); !e.empty(); e.popFront()) {
    Zone* registryZone = e.front().key();
    if (!zone->addSweepGroupEdgeTo(registryZone) ||
        !registryZone->addSweepGroupEdgeTo(zone)) {
      return false;
    }
  }

  return true;
}

FinalizationRegistryGlobalData::FinalizationRegistryGlobalData(Zone* zone)
    : recordSet(zone) {}

bool FinalizationRegistryGlobalData::addRecord(JSObject* record) {
  return recordSet.putNew(record);
}

void FinalizationRegistryGlobalData::removeRecord(JSObject* record) {
  MOZ_ASSERT(recordSet.has(record));
  recordSet.remove(record);
}

void FinalizationRegistryGlobalData::trace(JSTracer* trc,
                                           GlobalObject* global) {
  for (RecordSet::Enum e(recordSet); !e.empty(); e.popFront()) {
    HeapPtrObject& obj = e.mutableFront();
    TraceCrossCompartmentEdge(trc, global, &obj,
                              "FinalizationRegistryGlobalData::recordSet");
#ifdef DEBUG
    if (trc->kind() != JS::TracerKind::Moving) {
      FinalizationRecordObject* record = UnwrapFinalizationRecord(obj);
      MOZ_ASSERT(record);
      MOZ_ASSERT(record->isInRecordMap());
    }
#endif
  }
}
