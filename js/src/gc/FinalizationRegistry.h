/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_FinalizationRegistry_h
#define gc_FinalizationRegistry_h

#include "gc/Barrier.h"
#include "gc/WeakMap.h"
#include "gc/ZoneAllocator.h"
#include "js/GCHashTable.h"
#include "js/GCVector.h"

namespace js {

class FinalizationRegistryObject;
class FinalizationRecordObject;

namespace gc {

// Per-zone data structures to support FinalizationRegistry.
class FinalizationRegistryZone {
  Zone* const zone;

  // The set of all finalization registries in the associated zone. These are
  // direct pointers and are not wrapped.
  using RegistrySet = GCHashSet<HeapPtrObject, MovableCellHasher<HeapPtrObject>,
                                ZoneAllocPolicy>;
  RegistrySet registries;

  // A vector of FinalizationRecord objects, or CCWs to them.
  using RecordVector = GCVector<HeapPtrObject, 1, ZoneAllocPolicy>;

  // A map from finalization registry targets to a vector of finalization
  // records representing registries that the target is registered with and
  // their associated held values. The records may be in other zones and are
  // wrapped appropriately.
  using RecordMap =
      GCHashMap<HeapPtrObject, RecordVector, MovableCellHasher<HeapPtrObject>,
                ZoneAllocPolicy>;
  RecordMap recordMap;

  // A weak map used as a set of cross-zone record wrappers. The weak map
  // marking rules keep the wrappers alive while the record is alive and ensure
  // that they are both swept in the same sweep group.
  using WrapperWeakSet = ObjectValueWeakMap;
  WrapperWeakSet crossZoneWrappers;

 public:
  explicit FinalizationRegistryZone(Zone* zone);
  ~FinalizationRegistryZone();

  bool addRegistry(Handle<FinalizationRegistryObject*> registry);
  bool addRecord(HandleObject target, HandleObject record);

  void clearRecords();

  void traceRoots(JSTracer* trc);
  void traceWeakEdges(JSTracer* trc);

  void updateForRemovedRecord(JSObject* wrapper,
                              FinalizationRecordObject* record);

 private:
  bool addCrossZoneWrapper(JSObject* wrapper);
  void removeCrossZoneWrapper(JSObject* wrapper);

  static bool shouldRemoveRecord(FinalizationRecordObject* record);
};

// Per-global data structures to support FinalizationRegistry.
class FinalizationRegistryGlobalData {
  // Set of finalization records for finalization registries in this
  // realm. These are traced as part of the realm's global.
  using RecordSet = GCHashSet<HeapPtrObject, MovableCellHasher<HeapPtrObject>,
                              ZoneAllocPolicy>;
  RecordSet recordSet;

 public:
  explicit FinalizationRegistryGlobalData(Zone* zone);

  bool addRecord(FinalizationRecordObject* record);
  void removeRecord(FinalizationRecordObject* record);

  void trace(JSTracer* trc);
};

}  // namespace gc
}  // namespace js

#endif  // gc_FinalizationRegistry_h
