/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Finalization registry GC implementation.
 */

#include "builtin/FinalizationRegistryObject.h"
#include "gc/GCRuntime.h"
#include "gc/Zone.h"
#include "vm/JSContext.h"

#include "gc/PrivateIterators-inl.h"

using namespace js;
using namespace js::gc;

bool GCRuntime::addFinalizationRegistry(JSContext* cx,
                                        FinalizationRegistryObject* registry) {
  if (!cx->zone()->finalizationRegistries().put(registry)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

bool GCRuntime::registerWithFinalizationRegistry(JSContext* cx,
                                                 HandleObject target,
                                                 HandleObject record) {
  MOZ_ASSERT(!IsCrossCompartmentWrapper(target));
  MOZ_ASSERT(
      UncheckedUnwrapWithoutExpose(record)->is<FinalizationRecordObject>());
  MOZ_ASSERT(target->compartment() == record->compartment());

  auto& map = target->zone()->finalizationRecordMap();
  auto ptr = map.lookupForAdd(target);
  if (!ptr) {
    if (!map.add(ptr, target, FinalizationRecordVector(target->zone()))) {
      ReportOutOfMemory(cx);
      return false;
    }
  }
  if (!ptr->value().append(record)) {
    ReportOutOfMemory(cx);
    return false;
  }
  return true;
}

void GCRuntime::markFinalizationRegistryRoots(JSTracer* trc) {
  // The held values for all finalization records store in zone maps are marked
  // as roots. Finalization records store a finalization registry as a weak
  // pointer in a private value, which does not get marked.
  for (GCZonesIter zone(this); !zone.done(); zone.next()) {
    Zone::FinalizationRecordMap& map = zone->finalizationRecordMap();
    for (Zone::FinalizationRecordMap::Enum e(map); !e.empty(); e.popFront()) {
      e.front().value().trace(trc);
    }
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

void GCRuntime::sweepFinalizationRegistries(Zone* zone) {
  // Sweep finalization registry data and queue finalization records for cleanup
  // for any entries whose target is dying and remove them from the map.

  Zone::FinalizationRegistrySet& set = zone->finalizationRegistries();
  set.sweep();
  for (auto r = set.all(); !r.empty(); r.popFront()) {
    r.front()->as<FinalizationRegistryObject>().sweep();
  }

  Zone::FinalizationRecordMap& map = zone->finalizationRecordMap();
  for (Zone::FinalizationRecordMap::Enum e(map); !e.empty(); e.popFront()) {
    FinalizationRecordVector& records = e.front().value();

    // Update any pointers moved by the GC.
    records.sweep();

    // Sweep finalization records and remove records for:
    records.eraseIf([](JSObject* obj) {
      FinalizationRecordObject* record = UnwrapFinalizationRecord(obj);
      return !record ||              // Nuked CCW to record.
             !record->isActive() ||  // Unregistered record or dead finalization
                                     // registry in previous sweep group.
             !record->sweep();       // Dead finalization registry in this sweep
                                     // group.
    });

    // Queue finalization records for targets that are dying.
    if (IsAboutToBeFinalized(&e.front().mutableKey())) {
      for (JSObject* obj : records) {
        FinalizationRecordObject* record = UnwrapFinalizationRecord(obj);
        FinalizationRegistryObject* registry = record->registryDuringGC(this);
        registry->queueRecordToBeCleanedUp(record);
        queueFinalizationRegistryForCleanup(registry);
      }
      e.removeFront();
    }
  }
}

void GCRuntime::queueFinalizationRegistryForCleanup(
    FinalizationRegistryObject* registry) {
  // Prod the embedding to call us back later to run the finalization callbacks.
  if (!registry->isQueuedForCleanup()) {
    callHostCleanupFinalizationRegistryCallback(registry);
    registry->setQueuedForCleanup(true);
  }
}

bool GCRuntime::cleanupQueuedFinalizationRegistry(
    JSContext* cx, HandleFinalizationRegistryObject registry) {
  registry->setQueuedForCleanup(false);
  bool ok = FinalizationRegistryObject::cleanupQueuedRecords(cx, registry);
  return ok;
}
