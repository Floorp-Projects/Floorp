/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_FinalizationRegistry_h
#define gc_FinalizationRegistry_h

#include "gc/Barrier.h"
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

  // A map containing the number of targets in this zone whose finalization
  // registry is in a different zone.
  using ZoneCountMap =
      HashMap<Zone*, size_t, DefaultHasher<Zone*>, ZoneAllocPolicy>;
  ZoneCountMap crossZoneCount;

 public:
  explicit FinalizationRegistryZone(Zone* zone);
  ~FinalizationRegistryZone();

  bool addRegistry(Handle<FinalizationRegistryObject*> registry);
  bool addRecord(HandleObject target, HandleObject record);

  void clearRecords();

  void traceWeakEdges(JSTracer* trc);
  bool findSweepGroupEdges();

  void updateForRemovedRecord(JSObject* wrapper,
                              FinalizationRecordObject* record);

 private:
  bool incCrossZoneCount(Zone* otherZone);
  void decCrossZoneCount(Zone* otherZone);

  static bool shouldRemoveRecord(FinalizationRecordObject* record);
};

// Per-global data structures to support FinalizationRegistry.
class FinalizationRegistryGlobalData {
  // Set of possibly cross-realm finalization record wrappers for finalization
  // registries in this realm. These are traced as part of the realm's global.
  using RecordSet = GCHashSet<HeapPtrObject, MovableCellHasher<HeapPtrObject>,
                              ZoneAllocPolicy>;
  RecordSet recordSet;

 public:
  explicit FinalizationRegistryGlobalData(Zone* zone);

  bool addRecord(JSObject* record);
  void removeRecord(JSObject* record);

  void trace(JSTracer* trc, GlobalObject* global);
};

}  // namespace gc
}  // namespace js

#endif  // gc_FinalizationRegistry_h
