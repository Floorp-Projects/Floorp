/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Zone_h
#define gc_Zone_h

#include "mozilla/Atomics.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/SegmentedVector.h"

#include "ds/Bitmap.h"
#include "gc/ArenaList.h"
#include "gc/Barrier.h"
#include "gc/FindSCCs.h"
#include "gc/GCMarker.h"
#include "gc/NurseryAwareHashMap.h"
#include "gc/ZoneAllocator.h"
#include "js/GCHashTable.h"
#include "vm/AtomsTable.h"
#include "vm/JSScript.h"
#include "vm/TypeInference.h"

namespace js {

class RegExpZone;
class WeakRefObject;

namespace jit {
class JitZone;
}  // namespace jit

namespace gc {

class ZoneList;

using ZoneComponentFinder = ComponentFinder<JS::Zone>;

struct UniqueIdGCPolicy {
  static bool needsSweep(Cell** cell, uint64_t* value);
};

// Maps a Cell* to a unique, 64bit id.
using UniqueIdMap = GCHashMap<Cell*, uint64_t, PointerHasher<Cell*>,
                              SystemAllocPolicy, UniqueIdGCPolicy>;

extern uint64_t NextCellUniqueId(JSRuntime* rt);

template <typename T>
class ZoneAllCellIter;

template <typename T>
class ZoneCellIter;

// A vector of FinalizationRecord objects, or CCWs to them.
using FinalizationRecordVector = GCVector<HeapPtrObject, 1, ZoneAllocPolicy>;

}  // namespace gc

using StringWrapperMap =
    NurseryAwareHashMap<JSString*, JSString*, DefaultHasher<JSString*>,
                        ZoneAllocPolicy>;

class MOZ_NON_TEMPORARY_CLASS ExternalStringCache {
  static const size_t NumEntries = 4;
  mozilla::Array<JSString*, NumEntries> entries_;

  ExternalStringCache(const ExternalStringCache&) = delete;
  void operator=(const ExternalStringCache&) = delete;

 public:
  ExternalStringCache() { purge(); }
  void purge() { mozilla::PodArrayZero(entries_); }

  MOZ_ALWAYS_INLINE JSString* lookup(const char16_t* chars, size_t len) const;
  MOZ_ALWAYS_INLINE void put(JSString* s);
};

class MOZ_NON_TEMPORARY_CLASS FunctionToStringCache {
  struct Entry {
    BaseScript* script;
    JSString* string;

    void set(BaseScript* scriptArg, JSString* stringArg) {
      script = scriptArg;
      string = stringArg;
    }
  };
  static const size_t NumEntries = 2;
  mozilla::Array<Entry, NumEntries> entries_;

  FunctionToStringCache(const FunctionToStringCache&) = delete;
  void operator=(const FunctionToStringCache&) = delete;

 public:
  FunctionToStringCache() { purge(); }
  void purge() { mozilla::PodArrayZero(entries_); }

  MOZ_ALWAYS_INLINE JSString* lookup(BaseScript* script) const;
  MOZ_ALWAYS_INLINE void put(BaseScript* script, JSString* string);
};

// WeakRefHeapPtrVector is a GCVector of WeakRefObjects.
class WeakRefHeapPtrVector
    : public GCVector<js::HeapPtrObject, 1, js::ZoneAllocPolicy> {
 public:
  using GCVector::GCVector;

  // call in compacting, to update the target in each WeakRefObject.
  void sweep(js::HeapPtrObject& target);
};

// WeakRefMap is a per-zone GCHashMap, which maps from the target of the JS
// WeakRef to the list of JS WeakRefs.
class WeakRefMap
    : public GCHashMap<HeapPtrObject, WeakRefHeapPtrVector,
                       MovableCellHasher<HeapPtrObject>, ZoneAllocPolicy> {
 public:
  using GCHashMap::GCHashMap;
  using Base = GCHashMap<HeapPtrObject, WeakRefHeapPtrVector,
                         MovableCellHasher<HeapPtrObject>, ZoneAllocPolicy>;
  void sweep();
};

}  // namespace js

namespace JS {

// [SMDOC] GC Zones
//
// A zone is a collection of compartments. Every compartment belongs to exactly
// one zone. In Firefox, there is roughly one zone per tab along with a system
// zone for everything else. Zones mainly serve as boundaries for garbage
// collection. Unlike compartments, they have no special security properties.
//
// Every GC thing belongs to exactly one zone. GC things from the same zone but
// different compartments can share an arena (4k page). GC things from different
// zones cannot be stored in the same arena. The garbage collector is capable of
// collecting one zone at a time; it cannot collect at the granularity of
// compartments.
//
// GC things are tied to zones and compartments as follows:
//
// - JSObjects belong to a compartment and cannot be shared between
//   compartments. If an object needs to point to a JSObject in a different
//   compartment, regardless of zone, it must go through a cross-compartment
//   wrapper. Each compartment keeps track of its outgoing wrappers in a table.
//   JSObjects find their compartment via their ObjectGroup.
//
// - JSStrings do not belong to any particular compartment, but they do belong
//   to a zone. Thus, two different compartments in the same zone can point to a
//   JSString. When a string needs to be wrapped, we copy it if it's in a
//   different zone and do nothing if it's in the same zone. Thus, transferring
//   strings within a zone is very efficient.
//
// - Shapes and base shapes belong to a zone and are shared between compartments
//   in that zone where possible. Accessor shapes store getter and setter
//   JSObjects which belong to a single compartment, so these shapes and all
//   their descendants can't be shared with other compartments.
//
// - Scripts are also compartment-local and cannot be shared. A script points to
//   its compartment.
//
// - ObjectGroup and JitCode objects belong to a compartment and cannot be
//   shared. There is no mechanism to obtain the compartment from a JitCode
//   object.
//
// A zone remains alive as long as any GC things in the zone are alive. A
// compartment remains alive as long as any JSObjects, scripts, shapes, or base
// shapes within it are alive.
//
// We always guarantee that a zone has at least one live compartment by refusing
// to delete the last compartment in a live zone.
class Zone : public js::ZoneAllocator, public js::gc::GraphNodeBase<JS::Zone> {
 private:
  js::WriteOnceData<bool> isAtomsZone_;
  js::WriteOnceData<bool> isSelfHostingZone_;
  js::WriteOnceData<bool> isSystemZone_;

  enum class HelperThreadUse : uint32_t { None, Pending, Active };
  mozilla::Atomic<HelperThreadUse, mozilla::SequentiallyConsistent>
      helperThreadUse_;

  // The helper thread context with exclusive access to this zone, if
  // usedByHelperThread(), or nullptr when on the main thread.
  js::UnprotectedData<JSContext*> helperThreadOwnerContext_;

 public:
  js::gc::ArenaLists arenas;

  js::TypeZone types;

  // Per-zone data for use by an embedder.
  js::ZoneData<void*> data;

  js::ZoneData<uint32_t> tenuredStrings;
  js::ZoneData<uint32_t> tenuredBigInts;

  js::ZoneData<bool> allocNurseryStrings;
  js::ZoneData<bool> allocNurseryBigInts;

  // When true, skip calling the metadata callback. We use this:
  // - to avoid invoking the callback recursively;
  // - to avoid observing lazy prototype setup (which confuses callbacks that
  //   want to use the types being set up!);
  // - to avoid attaching allocation stacks to allocation stack nodes, which
  //   is silly
  // And so on.
  js::ZoneData<bool> suppressAllocationMetadataBuilder;

  // Script side-tables. These used to be held by Realm, but are now placed
  // here in order to allow JSScript to access them during finalize (see bug
  // 1568245; this change in 1575350). The tables are initialized lazily by
  // JSScript.
  js::UniquePtr<js::ScriptCountsMap> scriptCountsMap;
  js::UniquePtr<js::ScriptLCovMap> scriptLCovMap;
  js::UniquePtr<js::DebugScriptMap> debugScriptMap;
#ifdef MOZ_VTUNE
  js::UniquePtr<js::ScriptVTuneIdMap> scriptVTuneIdMap;
#endif

#ifdef DEBUG
  js::MainThreadData<unsigned> gcSweepGroupIndex;
#endif

 private:
  // Side map for storing unique ids for cells, independent of address.
  js::ZoneOrGCTaskData<js::gc::UniqueIdMap> uniqueIds_;

  // Number of allocations since the most recent minor GC for this thread.
  mozilla::Atomic<uint32_t, mozilla::Relaxed> tenuredAllocsSinceMinorGC_;

  // Live weakmaps in this zone.
  js::ZoneOrGCTaskData<mozilla::LinkedList<js::WeakMapBase>> gcWeakMapList_;

  // The set of compartments in this zone.
  using CompartmentVector =
      js::Vector<JS::Compartment*, 1, js::SystemAllocPolicy>;
  js::MainThreadOrGCTaskData<CompartmentVector> compartments_;

  // All cross-zone string wrappers in the zone.
  js::MainThreadOrGCTaskData<js::StringWrapperMap> crossZoneStringWrappers_;

  // This zone's gray roots.
  using GrayRootVector =
      mozilla::SegmentedVector<js::gc::Cell*, 1024 * sizeof(js::gc::Cell*),
                               js::SystemAllocPolicy>;
  js::ZoneOrGCTaskData<GrayRootVector> gcGrayRoots_;

  // List of non-ephemeron weak containers to sweep during
  // beginSweepingSweepGroup.
  js::ZoneOrGCTaskData<mozilla::LinkedList<detail::WeakCacheBase>> weakCaches_;

  // Mapping from not yet marked keys to a vector of all values that the key
  // maps to in any live weak map. Separate tables for nursery and tenured
  // keys.
  js::ZoneOrGCTaskData<js::gc::WeakKeyTable> gcWeakKeys_;
  js::ZoneOrGCTaskData<js::gc::WeakKeyTable> gcNurseryWeakKeys_;

  // Keep track of all TypeDescr and related objects in this compartment.
  // This is used by the GC to trace them all first when compacting, since the
  // TypedObject trace hook may access these objects.
  //
  // There are no barriers here - the set contains only tenured objects so no
  // post-barrier is required, and these are weak references so no pre-barrier
  // is required.
  using TypeDescrObjectSet =
      js::GCHashSet<JSObject*, js::MovableCellHasher<JSObject*>,
                    js::SystemAllocPolicy>;

  js::ZoneData<JS::WeakCache<TypeDescrObjectSet>> typeDescrObjects_;

  js::MainThreadData<js::UniquePtr<js::RegExpZone>> regExps_;

  // Bitmap of atoms marked by this zone.
  js::ZoneOrGCTaskData<js::SparseBitmap> markedAtoms_;

  // Set of atoms recently used by this Zone. Purged on GC unless
  // keepAtomsCount is non-zero.
  js::ZoneOrGCTaskData<js::AtomSet> atomCache_;

  // Cache storing allocated external strings. Purged on GC.
  js::ZoneOrGCTaskData<js::ExternalStringCache> externalStringCache_;

  // Cache for Function.prototype.toString. Purged on GC.
  js::ZoneOrGCTaskData<js::FunctionToStringCache> functionToStringCache_;

  // Count of AutoKeepAtoms instances for this zone. When any instances exist,
  // atoms in the runtime will be marked from this zone's atom mark bitmap,
  // rather than when traced in the normal way. Threads parsing off the main
  // thread do not increment this value, but the presence of any such threads
  // also inhibits collection of atoms. We don't scan the stacks of exclusive
  // threads, so we need to avoid collecting their objects in another way. The
  // only GC thing pointers they have are to their exclusive compartment
  // (which is not collected) or to the atoms compartment. Therefore, we avoid
  // collecting the atoms zone when exclusive threads are running.
  js::ZoneOrGCTaskData<unsigned> keepAtomsCount;

  // Whether purging atoms was deferred due to keepAtoms being set. If this
  // happen then the cache will be purged when keepAtoms drops to zero.
  js::ZoneOrGCTaskData<bool> purgeAtomsDeferred;

  // Shared Shape property tree.
  js::ZoneData<js::PropertyTree> propertyTree_;

  // Set of all unowned base shapes in the Zone.
  js::ZoneData<js::BaseShapeSet> baseShapes_;

  // Set of initial shapes in the Zone. For certain prototypes -- namely,
  // those of various builtin classes -- there are two entries: one for a
  // lookup via TaggedProto, and one for a lookup via JSProtoKey. See
  // InitialShapeProto.
  js::ZoneData<js::InitialShapeSet> initialShapes_;

  // List of shapes that may contain nursery pointers.
  using NurseryShapeVector =
      js::Vector<js::AccessorShape*, 0, js::SystemAllocPolicy>;
  js::ZoneData<NurseryShapeVector> nurseryShapes_;

  // The set of all finalization registries in this zone.
  using FinalizationRegistrySet =
      GCHashSet<js::HeapPtrObject, js::MovableCellHasher<js::HeapPtrObject>,
                js::ZoneAllocPolicy>;
  js::ZoneOrGCTaskData<FinalizationRegistrySet> finalizationRegistries_;

  // A map from finalization registry targets to a list of finalization records
  // representing registries that the target is registered with and their
  // associated held values.
  using FinalizationRecordMap =
      GCHashMap<js::HeapPtrObject, js::gc::FinalizationRecordVector,
                js::MovableCellHasher<js::HeapPtrObject>, js::ZoneAllocPolicy>;
  js::ZoneOrGCTaskData<FinalizationRecordMap> finalizationRecordMap_;

  js::ZoneOrGCTaskData<js::jit::JitZone*> jitZone_;

  js::MainThreadData<bool> gcScheduled_;
  js::MainThreadData<bool> gcScheduledSaved_;
  js::MainThreadData<bool> gcPreserveCode_;
  js::ZoneData<bool> keepShapeCaches_;
  js::MainThreadData<bool> wasCollected_;

  // Allow zones to be linked into a list
  js::MainThreadOrGCTaskData<Zone*> listNext_;
  static Zone* const NotOnList;
  friend class js::gc::ZoneList;

  js::ZoneOrGCTaskData<js::WeakRefMap> weakRefMap_;

  using KeptAliveSet =
      JS::GCHashSet<js::HeapPtrObject, js::MovableCellHasher<js::HeapPtrObject>,
                    js::ZoneAllocPolicy>;
  friend class js::WeakRefObject;
  js::ZoneOrGCTaskData<KeptAliveSet> keptObjects;

 public:
  static JS::Zone* from(ZoneAllocator* zoneAlloc) {
    return static_cast<Zone*>(zoneAlloc);
  }

  explicit Zone(JSRuntime* rt);
  ~Zone();

  MOZ_MUST_USE bool init();

  void destroy(JSFreeOp* fop);

  bool ownedByCurrentHelperThread();
  void setHelperThreadOwnerContext(JSContext* cx);

  // Whether this zone was created for use by a helper thread.
  bool createdForHelperThread() const {
    return helperThreadUse_ != HelperThreadUse::None;
  }
  // Whether this zone is currently in use by a helper thread.
  bool usedByHelperThread() {
    MOZ_ASSERT_IF(isAtomsZone(), helperThreadUse_ == HelperThreadUse::None);
    return helperThreadUse_ == HelperThreadUse::Active;
  }
  void setCreatedForHelperThread() {
    MOZ_ASSERT(helperThreadUse_ == HelperThreadUse::None);
    helperThreadUse_ = HelperThreadUse::Pending;
  }
  void setUsedByHelperThread() {
    MOZ_ASSERT(helperThreadUse_ == HelperThreadUse::Pending);
    helperThreadUse_ = HelperThreadUse::Active;
  }
  void clearUsedByHelperThread() {
    MOZ_ASSERT(helperThreadUse_ != HelperThreadUse::None);
    helperThreadUse_ = HelperThreadUse::None;
  }

  MOZ_MUST_USE bool findSweepGroupEdges(Zone* atomsZone);

  enum ShouldDiscardBaselineCode : bool {
    KeepBaselineCode = false,
    DiscardBaselineCode
  };

  enum ShouldDiscardJitScripts : bool {
    KeepJitScripts = false,
    DiscardJitScripts
  };

  void discardJitCode(
      JSFreeOp* fop,
      ShouldDiscardBaselineCode discardBaselineCode = DiscardBaselineCode,
      ShouldDiscardJitScripts discardJitScripts = KeepJitScripts);

  void addSizeOfIncludingThis(
      mozilla::MallocSizeOf mallocSizeOf, JS::CodeSizes* code, size_t* typePool,
      size_t* regexpZone, size_t* jitZone, size_t* baselineStubsOptimized,
      size_t* uniqueIdMap, size_t* shapeCaches, size_t* atomsMarkBitmaps,
      size_t* compartmentObjects, size_t* crossCompartmentWrappersTables,
      size_t* compartmentsPrivateData, size_t* scriptCountsMapArg);

  // Iterate over all cells in the zone. See the definition of ZoneCellIter
  // in gc/GC-inl.h for the possible arguments and documentation.
  template <typename T, typename... Args>
  js::gc::ZoneCellIter<T> cellIter(Args&&... args) {
    return js::gc::ZoneCellIter<T>(const_cast<Zone*>(this),
                                   std::forward<Args>(args)...);
  }

  // As above, but can return about-to-be-finalised things.
  template <typename T, typename... Args>
  js::gc::ZoneAllCellIter<T> cellIterUnsafe(Args&&... args) {
    return js::gc::ZoneAllCellIter<T>(const_cast<Zone*>(this),
                                      std::forward<Args>(args)...);
  }

  void beginSweepTypes();

  bool hasMarkedRealms();

  void scheduleGC() {
    MOZ_ASSERT(!RuntimeHeapIsBusy());
    gcScheduled_ = true;
  }
  void unscheduleGC() { gcScheduled_ = false; }
  bool isGCScheduled() { return gcScheduled_; }

  void setPreservingCode(bool preserving) { gcPreserveCode_ = preserving; }
  bool isPreservingCode() const { return gcPreserveCode_; }

  // Whether this zone can currently be collected. This doesn't take account
  // of AutoKeepAtoms for the atoms zone.
  bool canCollect();

  void changeGCState(GCState prev, GCState next) {
    MOZ_ASSERT(RuntimeHeapIsBusy());
    MOZ_ASSERT(gcState() == prev);
    MOZ_ASSERT(canCollect());
    gcState_ = next;
  }

  bool isCollecting() const {
    MOZ_ASSERT(js::CurrentThreadCanAccessRuntime(runtimeFromMainThread()));
    return isCollectingFromAnyThread();
  }

  bool isCollectingFromAnyThread() const {
    if (RuntimeHeapIsCollecting()) {
      return gcState_ != NoGC;
    } else {
      return needsIncrementalBarrier();
    }
  }

  bool shouldMarkInZone() const {
    return needsIncrementalBarrier() || isGCMarking();
  }

  // Was this zone collected in the last GC.
  bool wasCollected() const { return wasCollected_; }
  void setWasCollected(bool v) { wasCollected_ = v; }

  // Get a number that is incremented whenever this zone is collected, and
  // possibly at other times too.
  uint64_t gcNumber();

  void setNeedsIncrementalBarrier(bool needs);
  const uint32_t* addressOfNeedsIncrementalBarrier() const {
    return &needsIncrementalBarrier_;
  }

  static constexpr size_t offsetOfNeedsIncrementalBarrier() {
    return offsetof(Zone, needsIncrementalBarrier_);
  }

  js::jit::JitZone* getJitZone(JSContext* cx) {
    return jitZone_ ? jitZone_ : createJitZone(cx);
  }
  js::jit::JitZone* jitZone() { return jitZone_; }

  bool isAtomsZone() const { return isAtomsZone_; }
  bool isSelfHostingZone() const { return isSelfHostingZone_; }
  bool isSystemZone() const { return isSystemZone_; }

  void setIsAtomsZone();
  void setIsSelfHostingZone();
  void setIsSystemZone();

  void prepareForCompacting();

  void sweepAfterMinorGC(JSTracer* trc);
  void sweepUniqueIds();
  void sweepWeakMaps();
  void sweepCompartments(JSFreeOp* fop, bool keepAtleastOne, bool lastGC);

  js::gc::UniqueIdMap& uniqueIds() { return uniqueIds_.ref(); }

  void notifyObservingDebuggers();

  void clearTables();

  void addTenuredAllocsSinceMinorGC(uint32_t allocs) {
    tenuredAllocsSinceMinorGC_ += allocs;
  }

  uint32_t getAndResetTenuredAllocsSinceMinorGC() {
    return tenuredAllocsSinceMinorGC_.exchange(0);
  }

  mozilla::LinkedList<js::WeakMapBase>& gcWeakMapList() {
    return gcWeakMapList_.ref();
  }

  CompartmentVector& compartments() { return compartments_.ref(); }

  js::StringWrapperMap& crossZoneStringWrappers() {
    return crossZoneStringWrappers_.ref();
  }
  const js::StringWrapperMap& crossZoneStringWrappers() const {
    return crossZoneStringWrappers_.ref();
  }

  void dropStringWrappersOnGC();

  void sweepAllCrossCompartmentWrappers();
  static void fixupAllCrossCompartmentWrappersAfterMovingGC(JSTracer* trc);

  GrayRootVector& gcGrayRoots() { return gcGrayRoots_.ref(); }

  mozilla::LinkedList<detail::WeakCacheBase>& weakCaches() {
    return weakCaches_.ref();
  }
  void registerWeakCache(detail::WeakCacheBase* cachep) {
    weakCaches().insertBack(cachep);
  }

  void delegatePreWriteBarrier(JSObject* obj, JSObject* delegate) {
    if (needsIncrementalBarrier()) {
      delegatePreWriteBarrierInternal(obj, delegate);
    }
  }

  void delegatePreWriteBarrierInternal(JSObject* obj, JSObject* delegate);
  js::gc::WeakKeyTable& gcWeakKeys() { return gcWeakKeys_.ref(); }
  js::gc::WeakKeyTable& gcNurseryWeakKeys() { return gcNurseryWeakKeys_.ref(); }

  js::gc::WeakKeyTable& gcWeakKeys(const js::gc::Cell* cell) {
    return cell->isTenured() ? gcWeakKeys() : gcNurseryWeakKeys();
  }

  // Perform all pending weakmap entry marking for this zone after
  // transitioning to weak marking mode.
  js::gc::IncrementalProgress enterWeakMarkingMode(js::GCMarker* marker,
                                                   js::SliceBudget& budget);
  void checkWeakMarkingMode();

  // A set of edges from this zone to other zones used during GC to calculate
  // sweep groups.
  NodeSet& gcSweepGroupEdges() {
    return gcGraphEdges;  // Defined in GraphNodeBase base class.
  }
  bool hasSweepGroupEdgeTo(Zone* otherZone) const {
    return gcGraphEdges.has(otherZone);
  }
  MOZ_MUST_USE bool addSweepGroupEdgeTo(Zone* otherZone) {
    MOZ_ASSERT(otherZone->isGCMarking());
    return gcSweepGroupEdges().put(otherZone);
  }
  void clearSweepGroupEdges() { gcSweepGroupEdges().clear(); }

  js::RegExpZone& regExps() { return *regExps_.ref(); }

  JS::WeakCache<TypeDescrObjectSet>& typeDescrObjects() {
    return typeDescrObjects_.ref();
  }

  bool addTypeDescrObject(JSContext* cx, HandleObject obj);

  void keepAtoms() { keepAtomsCount++; }
  void releaseAtoms();
  bool hasKeptAtoms() const { return keepAtomsCount; }

  js::SparseBitmap& markedAtoms() { return markedAtoms_.ref(); }

  js::AtomSet& atomCache() { return atomCache_.ref(); }

  void traceAtomCache(JSTracer* trc);
  void purgeAtomCacheOrDefer();
  void purgeAtomCache();

  js::ExternalStringCache& externalStringCache() {
    return externalStringCache_.ref();
  };

  js::FunctionToStringCache& functionToStringCache() {
    return functionToStringCache_.ref();
  }

  js::PropertyTree& propertyTree() { return propertyTree_.ref(); }

  js::BaseShapeSet& baseShapes() { return baseShapes_.ref(); }

  js::InitialShapeSet& initialShapes() { return initialShapes_.ref(); }

  NurseryShapeVector& nurseryShapes() { return nurseryShapes_.ref(); }

  void fixupInitialShapeTable();
  void fixupAfterMovingGC();
  void fixupScriptMapsAfterMovingGC(JSTracer* trc);

  static js::HashNumber UniqueIdToHash(uint64_t uid);

  // Creates a HashNumber based on getUniqueId. Returns false on OOM.
  MOZ_MUST_USE bool getHashCode(js::gc::Cell* cell, js::HashNumber* hashp);

  // Gets an existing UID in |uidp| if one exists.
  MOZ_MUST_USE bool maybeGetUniqueId(js::gc::Cell* cell, uint64_t* uidp);

  // Puts an existing UID in |uidp|, or creates a new UID for this Cell and
  // puts that into |uidp|. Returns false on OOM.
  MOZ_MUST_USE bool getOrCreateUniqueId(js::gc::Cell* cell, uint64_t* uidp);

  js::HashNumber getHashCodeInfallible(js::gc::Cell* cell);
  uint64_t getUniqueIdInfallible(js::gc::Cell* cell);

  // Return true if this cell has a UID associated with it.
  MOZ_MUST_USE bool hasUniqueId(js::gc::Cell* cell);

  // Transfer an id from another cell. This must only be called on behalf of a
  // moving GC. This method is infallible.
  void transferUniqueId(js::gc::Cell* tgt, js::gc::Cell* src);

  // Remove any unique id associated with this Cell.
  void removeUniqueId(js::gc::Cell* cell);

  // When finished parsing off-thread, transfer any UIDs we created in the
  // off-thread zone into the target zone.
  void adoptUniqueIds(JS::Zone* source);

  bool keepShapeCaches() const { return keepShapeCaches_; }
  void setKeepShapeCaches(bool b) { keepShapeCaches_ = b; }

  // Delete an empty compartment after its contents have been merged.
  void deleteEmptyCompartment(JS::Compartment* comp);

  void clearRootsForShutdownGC();
  void finishRoots();

  void traceScriptTableRoots(JSTracer* trc);

  void clearScriptCounts(Realm* realm);
  void clearScriptLCov(Realm* realm);

  // Add the target of JS WeakRef to a kept-alive set maintained by GC.
  // See: https://tc39.es/proposal-weakrefs/#sec-keepduringjob
  bool keepDuringJob(HandleObject target);

  void traceKeptObjects(JSTracer* trc);

  // Clear the kept-alive set.
  // See: https://tc39.es/proposal-weakrefs/#sec-clear-kept-objects
  void clearKeptObjects();

#ifdef JSGC_HASH_TABLE_CHECKS
  void checkAllCrossCompartmentWrappersAfterMovingGC();
  void checkStringWrappersAfterMovingGC();

  void checkInitialShapesTableAfterMovingGC();
  void checkBaseShapeTableAfterMovingGC();

  // Assert that the UniqueId table has been redirected successfully.
  void checkUniqueIdTableAfterMovingGC();

  void checkScriptMapsAfterMovingGC();
#endif

#ifdef DEBUG
  // For testing purposes, return the index of the sweep group which this zone
  // was swept in in the last GC.
  unsigned lastSweepGroupIndex() { return gcSweepGroupIndex; }
#endif

 private:
  js::jit::JitZone* createJitZone(JSContext* cx);

  bool isQueuedForBackgroundSweep() { return isOnList(); }

  void sweepWeakKeysAfterMinorGC();

  FinalizationRegistrySet& finalizationRegistries() {
    return finalizationRegistries_.ref();
  }

  FinalizationRecordMap& finalizationRecordMap() {
    return finalizationRecordMap_.ref();
  }

  bool isOnList() const;
  Zone* nextZone() const;

  js::WeakRefMap& weakRefMap() { return weakRefMap_.ref(); }

  friend bool js::CurrentThreadCanAccessZone(Zone* zone);
  friend class js::gc::GCRuntime;
};

}  // namespace JS

namespace js {
namespace gc {
const char* StateName(JS::Zone::GCState state);
}  // namespace gc
}  // namespace js

#endif  // gc_Zone_h
