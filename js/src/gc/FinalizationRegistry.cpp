/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Finalization registry GC implementation.
 */

#include "gc/FinalizationRegistry.h"

#include "builtin/FinalizationRegistryObject.h"
#include "gc/GCRuntime.h"
#include "gc/Zone.h"
#include "vm/JSContext.h"

#include "gc/PrivateIterators-inl.h"
#include "vm/JSObject-inl.h"

using namespace js;
using namespace js::gc;

FinalizationRegistryZone::FinalizationRegistryZone(Zone* zone)
    : zone(zone), registries(zone), recordMap(zone) {}

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
  MOZ_ASSERT(target->zone() == zone);

  auto ptr = recordMap.lookupForAdd(target);
  if (!ptr && !recordMap.add(ptr, target, RecordVector(zone))) {
    return false;
  }
  return ptr->value().append(record);
}

void FinalizationRegistryZone::clearRecords() { recordMap.clear(); }

void GCRuntime::markFinalizationRegistryRoots(JSTracer* trc) {
  for (GCZonesIter zone(this); !zone.done(); zone.next()) {
    FinalizationRegistryZone* frzone = zone->finalizationRegistryZone();
    if (frzone) {
      frzone->markRoots(trc);
    }
  }
}

void FinalizationRegistryZone::markRoots(JSTracer* trc) {
  // All finalization records stored in the zone maps are marked as roots.
  // Records can be removed from these maps during sweeping in which case they
  // die in the next collection.
  for (RecordMap::Enum e(recordMap); !e.empty(); e.popFront()) {
    e.front().value().trace(trc);
  }
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

    // Update any pointers moved by the GC.
    records.traceWeak(trc);

    // Sweep finalization records and remove records for:
    records.eraseIf([](JSObject* obj) {
      FinalizationRecordObject* record = UnwrapFinalizationRecord(obj);
      return !record ||                        // Nuked CCW to record.
             !record->isActive() ||            // Unregistered record.
             !record->queue()->hasRegistry();  // Dead finalization registry.
    });

    // Queue finalization records for targets that are dying.
    if (!TraceWeakEdge(trc, &e.front().mutableKey(),
                       "FinalizationRecord target")) {
      for (JSObject* obj : records) {
        FinalizationRecordObject* record = UnwrapFinalizationRecord(obj);
        FinalizationQueueObject* queue = record->queue();
        queue->queueRecordToBeCleanedUp(record);
        gc->queueFinalizationRegistryForCleanup(queue);
      }
      e.removeFront();
    }
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

  queue->setQueuedForCleanup(true);
}
