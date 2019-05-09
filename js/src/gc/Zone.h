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

#include "gc/FindSCCs.h"
#include "js/GCHashTable.h"
#include "vm/MallocProvider.h"
#include "vm/Runtime.h"
#include "vm/TypeInference.h"

namespace js {

class Debugger;
class RegExpZone;

namespace jit {
class JitZone;
}  // namespace jit

namespace gc {

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

}  // namespace gc

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
    JSScript* script;
    JSString* string;

    void set(JSScript* scriptArg, JSString* stringArg) {
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

  MOZ_ALWAYS_INLINE JSString* lookup(JSScript* script) const;
  MOZ_ALWAYS_INLINE void put(JSScript* script, JSString* string);
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
class Zone : public JS::shadow::Zone,
             public js::gc::GraphNodeBase<JS::Zone>,
             public js::MallocProvider<JS::Zone> {
 public:
  explicit Zone(JSRuntime* rt);
  ~Zone();
  MOZ_MUST_USE bool init(bool isSystem);
  void destroy(js::FreeOp* fop);

 private:
  enum class HelperThreadUse : uint32_t { None, Pending, Active };
  mozilla::Atomic<HelperThreadUse, mozilla::SequentiallyConsistent,
                  mozilla::recordreplay::Behavior::DontPreserve>
      helperThreadUse_;

  // The helper thread context with exclusive access to this zone, if
  // usedByHelperThread(), or nullptr when on the main thread.
  js::UnprotectedData<JSContext*> helperThreadOwnerContext_;

 public:
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

  enum ShouldReleaseTypes : bool { KeepTypes = false, ReleaseTypes };

  void discardJitCode(
      js::FreeOp* fop,
      ShouldDiscardBaselineCode discardBaselineCode = DiscardBaselineCode,
      ShouldReleaseTypes releaseTypes = KeepTypes);

  void addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                              size_t* typePool, size_t* regexpZone,
                              size_t* jitZone, size_t* baselineStubsOptimized,
                              size_t* cachedCFG, size_t* uniqueIdMap,
                              size_t* shapeCaches, size_t* atomsMarkBitmaps,
                              size_t* compartmentObjects,
                              size_t* crossCompartmentWrappersTables,
                              size_t* compartmentsPrivateData);

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

  MOZ_MUST_USE void* onOutOfMemory(js::AllocFunction allocFunc,
                                   arena_id_t arena, size_t nbytes,
                                   void* reallocPtr = nullptr);
  void reportAllocationOverflow();

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
    MOZ_ASSERT_IF(next != NoGC, canCollect());
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

  bool isAtomsZone() const { return runtimeFromAnyThread()->isAtomsZone(this); }
  bool isSelfHostingZone() const {
    return runtimeFromAnyThread()->isSelfHostingZone(this);
  }

  void prepareForCompacting();

#ifdef DEBUG
  // If this returns true, all object tracing must be done with a GC marking
  // tracer.
  bool requireGCTracer() const;

  // For testing purposes, return the index of the sweep group which this zone
  // was swept in in the last GC.
  unsigned lastSweepGroupIndex() { return gcLastSweepGroupIndex; }
#endif

  void sweepBreakpoints(js::FreeOp* fop);
  void sweepUniqueIds();
  void sweepWeakMaps();
  void sweepCompartments(js::FreeOp* fop, bool keepAtleastOne, bool lastGC);

  using DebuggerVector = js::Vector<js::Debugger*, 0, js::SystemAllocPolicy>;

 private:
  js::ZoneData<DebuggerVector*> debuggers;

  js::jit::JitZone* createJitZone(JSContext* cx);

  bool isQueuedForBackgroundSweep() { return isOnList(); }

  // Side map for storing a unique ids for cells, independent of address.
  js::ZoneOrGCTaskData<js::gc::UniqueIdMap> uniqueIds_;

  js::gc::UniqueIdMap& uniqueIds() { return uniqueIds_.ref(); }

 public:
  bool hasDebuggers() const { return debuggers && debuggers->length(); }
  DebuggerVector* getDebuggers() const { return debuggers; }
  DebuggerVector* getOrCreateDebuggers(JSContext* cx);

  void notifyObservingDebuggers();

  void clearTables();

  /*
   * When true, skip calling the metadata callback. We use this:
   * - to avoid invoking the callback recursively;
   * - to avoid observing lazy prototype setup (which confuses callbacks that
   *   want to use the types being set up!);
   * - to avoid attaching allocation stacks to allocation stack nodes, which
   *   is silly
   * And so on.
   */
  js::ZoneData<bool> suppressAllocationMetadataBuilder;

  js::gc::ArenaLists arenas;

 private:
  // Number of allocations since the most recent minor GC for this thread.
  mozilla::Atomic<uint32_t, mozilla::Relaxed,
                  mozilla::recordreplay::Behavior::DontPreserve>
      tenuredAllocsSinceMinorGC_;

 public:
  void addTenuredAllocsSinceMinorGC(uint32_t allocs) {
    tenuredAllocsSinceMinorGC_ += allocs;
  }

  uint32_t getAndResetTenuredAllocsSinceMinorGC() {
    return tenuredAllocsSinceMinorGC_.exchange(0);
  }

  js::TypeZone types;

 private:
  /* Live weakmaps in this zone. */
  js::ZoneOrGCTaskData<mozilla::LinkedList<js::WeakMapBase>> gcWeakMapList_;

 public:
  mozilla::LinkedList<js::WeakMapBase>& gcWeakMapList() {
    return gcWeakMapList_.ref();
  }

  typedef js::Vector<JS::Compartment*, 1, js::SystemAllocPolicy>
      CompartmentVector;

 private:
  // The set of compartments in this zone.
  js::MainThreadOrGCTaskData<CompartmentVector> compartments_;

 public:
  CompartmentVector& compartments() { return compartments_.ref(); }

  // This zone's gray roots.
  using GrayRootVector =
      mozilla::SegmentedVector<js::gc::Cell*, 1024 * sizeof(js::gc::Cell*),
                               js::SystemAllocPolicy>;

 private:
  js::ZoneOrGCTaskData<GrayRootVector> gcGrayRoots_;

 public:
  GrayRootVector& gcGrayRoots() { return gcGrayRoots_.ref(); }

  // This zone's weak edges found via graph traversal during marking,
  // preserved for re-scanning during sweeping.
  using WeakEdges = js::Vector<js::gc::TenuredCell**, 0, js::SystemAllocPolicy>;

 private:
  js::ZoneOrGCTaskData<WeakEdges> gcWeakRefs_;

 public:
  WeakEdges& gcWeakRefs() { return gcWeakRefs_.ref(); }

 private:
  // List of non-ephemeron weak containers to sweep during
  // beginSweepingSweepGroup.
  js::ZoneOrGCTaskData<mozilla::LinkedList<detail::WeakCacheBase>> weakCaches_;

 public:
  mozilla::LinkedList<detail::WeakCacheBase>& weakCaches() {
    return weakCaches_.ref();
  }
  void registerWeakCache(detail::WeakCacheBase* cachep) {
    weakCaches().insertBack(cachep);
  }

 private:
  /*
   * Mapping from not yet marked keys to a vector of all values that the key
   * maps to in any live weak map.
   */
  js::ZoneOrGCTaskData<js::gc::WeakKeyTable> gcWeakKeys_;

 public:
  js::gc::WeakKeyTable& gcWeakKeys() { return gcWeakKeys_.ref(); }

  // A set of edges from this zone to other zones used during GC to calculate
  // sweep groups.
  NodeSet& gcSweepGroupEdges() {
    return gcGraphEdges;  // Defined in GraphNodeBase base class.
  }
  MOZ_MUST_USE bool addSweepGroupEdgeTo(Zone* otherZone) {
    MOZ_ASSERT(otherZone->isGCMarking());
    return gcSweepGroupEdges().put(otherZone);
  }
  void clearSweepGroupEdges() { gcSweepGroupEdges().clear(); }

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

 private:
  js::ZoneData<JS::WeakCache<TypeDescrObjectSet>> typeDescrObjects_;

  // Malloc counter to measure memory pressure for GC scheduling. This counter
  // is used for allocations where the size of the allocation is not known on
  // free. Currently this is used for all internal malloc allocations.
  js::gc::MemoryCounter gcMallocCounter;

  // Malloc counter used for allocations where size information is
  // available. Used for some internal and all tracked external allocations.
  js::gc::MemoryTracker gcMallocSize;

  // Counter of JIT code executable memory for GC scheduling. Also imprecise,
  // since wasm can generate code that outlives a zone.
  js::gc::MemoryCounter jitCodeCounter;

  void updateMemoryCounter(js::gc::MemoryCounter& counter, size_t nbytes) {
    JSRuntime* rt = runtimeFromAnyThread();

    counter.update(nbytes);
    auto trigger = counter.shouldTriggerGC(rt->gc.tunables);
    if (MOZ_LIKELY(trigger == js::gc::NoTrigger) ||
        trigger <= counter.triggered()) {
      return;
    }

    maybeTriggerGCForTooMuchMalloc(counter, trigger);
  }

  void maybeTriggerGCForTooMuchMalloc(js::gc::MemoryCounter& counter,
                                      js::gc::TriggerKind trigger);

  js::MainThreadData<js::UniquePtr<js::RegExpZone>> regExps_;

 public:
  js::RegExpZone& regExps() { return *regExps_.ref(); }

  JS::WeakCache<TypeDescrObjectSet>& typeDescrObjects() {
    return typeDescrObjects_.ref();
  }

  bool addTypeDescrObject(JSContext* cx, HandleObject obj);

  void setGCMaxMallocBytes(size_t value, const js::AutoLockGC& lock) {
    gcMallocCounter.setMax(value, lock);
  }
  void updateMallocCounter(size_t nbytes) {
    updateMemoryCounter(gcMallocCounter, nbytes);
  }
  void adoptMallocBytes(Zone* other) {
    gcMallocCounter.adopt(other->gcMallocCounter);
    gcMallocSize.adopt(other->gcMallocSize);
  }
  size_t GCMaxMallocBytes() const { return gcMallocCounter.maxBytes(); }
  size_t GCMallocBytes() const { return gcMallocCounter.bytes(); }

  void updateJitCodeMallocBytes(size_t nbytes) {
    updateMemoryCounter(jitCodeCounter, nbytes);
  }

  void updateAllGCMallocCountersOnGCStart();
  void updateAllGCMallocCountersOnGCEnd(const js::AutoLockGC& lock);
  js::gc::TriggerKind shouldTriggerGCForTooMuchMalloc();

  // Memory accounting APIs for memory owned by GC cells.
  void addCellMemory(js::gc::Cell* cell, size_t nbytes, js::MemoryUse use) {
    gcMallocSize.addMemory(cell, nbytes, use);
  }
  void removeCellMemory(js::gc::Cell* cell, size_t nbytes, js::MemoryUse use) {
    gcMallocSize.removeMemory(cell, nbytes, use);
  }

  size_t totalBytes() const {
    return zoneSize.gcBytes() + gcMallocSize.bytes();
  }

  void keepAtoms() { keepAtomsCount++; }
  void releaseAtoms();
  bool hasKeptAtoms() const { return keepAtomsCount; }

 private:
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

 public:
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

  // Track heap size under this Zone.
  js::gc::HeapSize zoneSize;

  // Thresholds used to trigger GC.
  js::gc::ZoneHeapThreshold threshold;

  // Amount of data to allocate before triggering a new incremental slice for
  // the current GC.
  js::UnprotectedData<size_t> gcDelayBytes;

  js::ZoneData<uint32_t> tenuredStrings;
  js::ZoneData<bool> allocNurseryStrings;

 private:
  // Shared Shape property tree.
  js::ZoneData<js::PropertyTree> propertyTree_;

 public:
  js::PropertyTree& propertyTree() { return propertyTree_.ref(); }

 private:
  // Set of all unowned base shapes in the Zone.
  js::ZoneData<js::BaseShapeSet> baseShapes_;

 public:
  js::BaseShapeSet& baseShapes() { return baseShapes_.ref(); }

 private:
  // Set of initial shapes in the Zone. For certain prototypes -- namely,
  // those of various builtin classes -- there are two entries: one for a
  // lookup via TaggedProto, and one for a lookup via JSProtoKey. See
  // InitialShapeProto.
  js::ZoneData<js::InitialShapeSet> initialShapes_;

 public:
  js::InitialShapeSet& initialShapes() { return initialShapes_.ref(); }

 private:
  // List of shapes that may contain nursery pointers.
  using NurseryShapeVector =
      js::Vector<js::AccessorShape*, 0, js::SystemAllocPolicy>;
  js::ZoneData<NurseryShapeVector> nurseryShapes_;

 public:
  NurseryShapeVector& nurseryShapes() { return nurseryShapes_.ref(); }

#ifdef JSGC_HASH_TABLE_CHECKS
  void checkInitialShapesTableAfterMovingGC();
  void checkBaseShapeTableAfterMovingGC();
#endif
  void fixupInitialShapeTable();
  void fixupAfterMovingGC();

  // Per-zone data for use by an embedder.
  js::ZoneData<void*> data;

  js::ZoneData<bool> isSystem;

#ifdef DEBUG
  js::MainThreadData<unsigned> gcLastSweepGroupIndex;
#endif

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

#ifdef JSGC_HASH_TABLE_CHECKS
  // Assert that the UniqueId table has been redirected successfully.
  void checkUniqueIdTableAfterMovingGC();
#endif

  bool keepShapeCaches() const { return keepShapeCaches_; }
  void setKeepShapeCaches(bool b) { keepShapeCaches_ = b; }

  // Delete an empty compartment after its contents have been merged.
  void deleteEmptyCompartment(JS::Compartment* comp);

  // Non-zero if the storage underlying any typed object in this zone might
  // be detached. This is stored in Zone because IC stubs bake in a pointer
  // to this field and Baseline IC code is shared across realms within a
  // Zone. Furthermore, it's not entirely clear if this flag is ever set to
  // a non-zero value since bug 1458011.
  uint32_t detachedTypedObjects = 0;

 private:
  js::ZoneOrGCTaskData<js::jit::JitZone*> jitZone_;

  js::MainThreadData<bool> gcScheduled_;
  js::MainThreadData<bool> gcScheduledSaved_;
  js::MainThreadData<bool> gcPreserveCode_;
  js::ZoneData<bool> keepShapeCaches_;

  // Allow zones to be linked into a list
  friend class js::gc::ZoneList;
  static Zone* const NotOnList;
  js::MainThreadOrGCTaskData<Zone*> listNext_;
  bool isOnList() const;
  Zone* nextZone() const;

  friend bool js::CurrentThreadCanAccessZone(Zone* zone);
  friend class js::gc::GCRuntime;
};

}  // namespace JS

namespace js {

/*
 * Allocation policy that uses Zone::pod_malloc and friends, so that memory
 * pressure is accounted for on the zone. This is suitable for memory associated
 * with GC things allocated in the zone.
 *
 * Since it doesn't hold a JSContext (those may not live long enough), it can't
 * report out-of-memory conditions itself; the caller must check for OOM and
 * take the appropriate action.
 *
 * FIXME bug 647103 - replace these *AllocPolicy names.
 */
class ZoneAllocPolicy {
  JS::Zone* const zone;

 public:
  MOZ_IMPLICIT ZoneAllocPolicy(JS::Zone* z) : zone(z) {}

  template <typename T>
  T* maybe_pod_malloc(size_t numElems) {
    return zone->maybe_pod_malloc<T>(numElems);
  }
  template <typename T>
  T* maybe_pod_calloc(size_t numElems) {
    return zone->maybe_pod_calloc<T>(numElems);
  }
  template <typename T>
  T* maybe_pod_realloc(T* p, size_t oldSize, size_t newSize) {
    return zone->maybe_pod_realloc<T>(p, oldSize, newSize);
  }
  template <typename T>
  T* pod_malloc(size_t numElems) {
    return zone->pod_malloc<T>(numElems);
  }
  template <typename T>
  T* pod_calloc(size_t numElems) {
    return zone->pod_calloc<T>(numElems);
  }
  template <typename T>
  T* pod_realloc(T* p, size_t oldSize, size_t newSize) {
    return zone->pod_realloc<T>(p, oldSize, newSize);
  }

  template <typename T>
  void free_(T* p, size_t numElems = 0) {
    js_free(p);
  }
  void reportAllocOverflow() const {}

  MOZ_MUST_USE bool checkSimulatedOOM() const {
    return !js::oom::ShouldFailWithOOM();
  }
};

// Convenience functions for memory accounting on the zone.

// Associate malloc memory with a GC thing. This call must be matched by a
// following call to RemoveCellMemory with the same size and use. The total
// amount of malloc memory associated with a zone is used to trigger GC.
inline void AddCellMemory(gc::TenuredCell* cell, size_t nbytes,
                          MemoryUse use) {
  if (nbytes) {
    cell->zone()->addCellMemory(cell, nbytes, use);
  }
}
inline void AddCellMemory(gc::Cell* cell, size_t nbytes, MemoryUse use) {
  if (cell->isTenured()) {
    AddCellMemory(&cell->asTenured(), nbytes, use);
  }
}

// Remove association between malloc memory and a GC thing. This call must
// follow a call to AddCellMemory with the same size and use.
inline void RemoveCellMemory(gc::TenuredCell* cell, size_t nbytes,
                             MemoryUse use) {
  if (nbytes) {
    cell->zoneFromAnyThread()->removeCellMemory(cell, nbytes, use);
  }
}
inline void RemoveCellMemory(gc::Cell* cell, size_t nbytes, MemoryUse use) {
  if (cell->isTenured()) {
    RemoveCellMemory(&cell->asTenured(), nbytes, use);
  }
}

}  // namespace js

#endif  // gc_Zone_h
