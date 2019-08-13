/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_GCRuntime_h
#define gc_GCRuntime_h

#include "mozilla/Atomics.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"

#include "gc/ArenaList.h"
#include "gc/AtomMarking.h"
#include "gc/GCMarker.h"
#include "gc/GCParallelTask.h"
#include "gc/Nursery.h"
#include "gc/Scheduling.h"
#include "gc/Statistics.h"
#include "gc/StoreBuffer.h"
#include "js/GCAnnotations.h"
#include "js/UniquePtr.h"
#include "vm/AtomsTable.h"

namespace js {

class AutoAccessAtomsZone;
class AutoLockGC;
class AutoLockGCBgAlloc;
class AutoLockHelperThreadState;
class VerifyPreTracer;
class ZoneAllocator;

namespace gc {

using BlackGrayEdgeVector = Vector<TenuredCell*, 0, SystemAllocPolicy>;
using ZoneVector = Vector<JS::Zone*, 4, SystemAllocPolicy>;

class AutoCallGCCallbacks;
class AutoGCSession;
class AutoRunParallelTask;
class AutoTraceSession;
class MarkingValidator;
struct MovingTracer;
enum class ShouldCheckThresholds;
class SweepGroupsIter;
class WeakCacheSweepIterator;

enum IncrementalProgress { NotFinished = 0, Finished };

// Interface to a sweep action.
struct SweepAction {
  // The arguments passed to each action.
  struct Args {
    GCRuntime* gc;
    JSFreeOp* fop;
    SliceBudget& budget;
  };

  virtual ~SweepAction() {}
  virtual IncrementalProgress run(Args& state) = 0;
  virtual void assertFinished() const = 0;
  virtual bool shouldSkip() { return false; }
};

class ChunkPool {
  Chunk* head_;
  size_t count_;

 public:
  ChunkPool() : head_(nullptr), count_(0) {}
  ~ChunkPool() {
    // TODO: We should be able to assert that the chunk pool is empty but
    // this causes XPCShell test failures on Windows 2012. See bug 1379232.
  }

  bool empty() const { return !head_; }
  size_t count() const { return count_; }

  Chunk* head() {
    MOZ_ASSERT(head_);
    return head_;
  }
  Chunk* pop();
  void push(Chunk* chunk);
  Chunk* remove(Chunk* chunk);

  void sort();

 private:
  Chunk* mergeSort(Chunk* list, size_t count);
  bool isSorted() const;

#ifdef DEBUG
 public:
  bool contains(Chunk* chunk) const;
  bool verify() const;
#endif

 public:
  // Pool mutation does not invalidate an Iter unless the mutation
  // is of the Chunk currently being visited by the Iter.
  class Iter {
   public:
    explicit Iter(ChunkPool& pool) : current_(pool.head_) {}
    bool done() const { return !current_; }
    void next();
    Chunk* get() const { return current_; }
    operator Chunk*() const { return get(); }
    Chunk* operator->() const { return get(); }

   private:
    Chunk* current_;
  };
};

class BackgroundSweepTask : public GCParallelTaskHelper<BackgroundSweepTask> {
 public:
  explicit BackgroundSweepTask(JSRuntime* rt) : GCParallelTaskHelper(rt) {}
  void run();
};

class BackgroundFreeTask : public GCParallelTaskHelper<BackgroundFreeTask> {
 public:
  explicit BackgroundFreeTask(JSRuntime* rt) : GCParallelTaskHelper(rt) {}
  void run();
};

// Performs extra allocation off thread so that when memory is required on the
// main thread it will already be available and waiting.
class BackgroundAllocTask : public GCParallelTaskHelper<BackgroundAllocTask> {
  // Guarded by the GC lock.
  GCLockData<ChunkPool&> chunkPool_;

  const bool enabled_;

 public:
  BackgroundAllocTask(JSRuntime* rt, ChunkPool& pool);
  bool enabled() const { return enabled_; }

  void run();
};

// Search the provided Chunks for free arenas and decommit them.
class BackgroundDecommitTask
    : public GCParallelTaskHelper<BackgroundDecommitTask> {
 public:
  using ChunkVector = mozilla::Vector<Chunk*>;

  explicit BackgroundDecommitTask(JSRuntime* rt) : GCParallelTaskHelper(rt) {}
  void setChunksToScan(ChunkVector& chunks);

  void run();

 private:
  MainThreadOrGCTaskData<ChunkVector> toDecommit;
};

template <typename F>
struct Callback {
  MainThreadOrGCTaskData<F> op;
  MainThreadOrGCTaskData<void*> data;

  Callback() : op(nullptr), data(nullptr) {}
  Callback(F op, void* data) : op(op), data(data) {}
};

template <typename F>
using CallbackVector =
    MainThreadData<Vector<Callback<F>, 4, SystemAllocPolicy>>;

template <typename T, typename Iter0, typename Iter1>
class ChainedIter {
  Iter0 iter0_;
  Iter1 iter1_;

 public:
  ChainedIter(const Iter0& iter0, const Iter1& iter1)
      : iter0_(iter0), iter1_(iter1) {}

  bool done() const { return iter0_.done() && iter1_.done(); }
  void next() {
    MOZ_ASSERT(!done());
    if (!iter0_.done()) {
      iter0_.next();
    } else {
      MOZ_ASSERT(!iter1_.done());
      iter1_.next();
    }
  }
  T get() const {
    MOZ_ASSERT(!done());
    if (!iter0_.done()) {
      return iter0_.get();
    }
    MOZ_ASSERT(!iter1_.done());
    return iter1_.get();
  }

  operator T() const { return get(); }
  T operator->() const { return get(); }
};

typedef HashMap<Value*, const char*, DefaultHasher<Value*>, SystemAllocPolicy>
    RootedValueMap;

using AllocKinds = mozilla::EnumSet<AllocKind, uint32_t>;

// A singly linked list of zones.
class ZoneList {
  static Zone* const End;

  Zone* head;
  Zone* tail;

 public:
  ZoneList();
  ~ZoneList();

  bool isEmpty() const;
  Zone* front() const;

  void append(Zone* zone);
  void transferFrom(ZoneList& other);
  Zone* removeFront();
  void clear();

 private:
  explicit ZoneList(Zone* singleZone);
  void check() const;

  ZoneList(const ZoneList& other) = delete;
  ZoneList& operator=(const ZoneList& other) = delete;
};

class GCRuntime {
  friend GCMarker::MarkQueueProgress GCMarker::processMarkQueue();

 public:
  explicit GCRuntime(JSRuntime* rt);
  MOZ_MUST_USE bool init(uint32_t maxbytes, uint32_t maxNurseryBytes);
  void finishRoots();
  void finish();

  inline bool hasZealMode(ZealMode mode);
  inline void clearZealMode(ZealMode mode);
  inline bool upcomingZealousGC();
  inline bool needZealousGC();
  inline bool hasIncrementalTwoSliceZealMode();

  MOZ_MUST_USE bool addRoot(Value* vp, const char* name);
  void removeRoot(Value* vp);
  void setMarkStackLimit(size_t limit, AutoLockGC& lock);

  MOZ_MUST_USE bool setParameter(JSGCParamKey key, uint32_t value,
                                 AutoLockGC& lock);
  void resetParameter(JSGCParamKey key, AutoLockGC& lock);
  uint32_t getParameter(JSGCParamKey key, const AutoLockGC& lock);

  MOZ_MUST_USE bool triggerGC(JS::GCReason reason);
  // Check whether to trigger a zone GC after allocating GC cells. During an
  // incremental GC, optionally count |nbytes| towards the threshold for
  // performing the next slice.
  void maybeAllocTriggerZoneGC(Zone* zone, size_t nbytes = 0);
  // Check whether to trigger a zone GC after malloc memory.
  void maybeMallocTriggerZoneGC(Zone* zone);
  bool maybeMallocTriggerZoneGC(Zone* zone, const HeapSize& heap,
                                const ZoneThreshold& threshold,
                                JS::GCReason reason);
  // The return value indicates if we were able to do the GC.
  bool triggerZoneGC(Zone* zone, JS::GCReason reason, size_t usedBytes,
                     size_t thresholdBytes);
  void maybeGC();
  bool checkEagerAllocTrigger(const HeapSize& size,
                              const ZoneThreshold& threshold);
  // The return value indicates whether a major GC was performed.
  bool gcIfRequested();
  void gc(JSGCInvocationKind gckind, JS::GCReason reason);
  void startGC(JSGCInvocationKind gckind, JS::GCReason reason,
               int64_t millis = 0);
  void gcSlice(JS::GCReason reason, int64_t millis = 0);
  void finishGC(JS::GCReason reason);
  void abortGC();
  void startDebugGC(JSGCInvocationKind gckind, SliceBudget& budget);
  void debugGCSlice(SliceBudget& budget);

  void triggerFullGCForAtoms(JSContext* cx);

  void runDebugGC();
  void notifyRootsRemoved();

  enum TraceOrMarkRuntime { TraceRuntime, MarkRuntime };
  void traceRuntime(JSTracer* trc, AutoTraceSession& session);
  void traceRuntimeForMinorGC(JSTracer* trc, AutoGCSession& session);

  void purgeRuntimeForMinorGC();

  void shrinkBuffers();
  void onOutOfMallocMemory();
  void onOutOfMallocMemory(const AutoLockGC& lock);

#ifdef JS_GC_ZEAL
  const uint32_t* addressOfZealModeBits() { return &zealModeBits.refNoCheck(); }
  void getZealBits(uint32_t* zealBits, uint32_t* frequency,
                   uint32_t* nextScheduled);
  void setZeal(uint8_t zeal, uint32_t frequency);
  void unsetZeal(uint8_t zeal);
  bool parseAndSetZeal(const char* str);
  void setNextScheduled(uint32_t count);
  void verifyPreBarriers();
  void maybeVerifyPreBarriers(bool always);
  bool selectForMarking(JSObject* object);
  void clearSelectedForMarking();
  void setDeterministic(bool enable);
#endif

  uint64_t nextCellUniqueId() {
    MOZ_ASSERT(nextCellUniqueId_ > 0);
    uint64_t uid = ++nextCellUniqueId_;
    return uid;
  }

  void setLowMemoryState(bool newState) { lowMemoryState = newState; }
  bool systemHasLowMemory() const { return lowMemoryState; }

#ifdef DEBUG
  bool shutdownCollectedEverything() const { return arenasEmptyAtShutdown; }
#endif

 public:
  // Internal public interface
  State state() const { return incrementalState; }
  bool isHeapCompacting() const { return state() == State::Compact; }
  bool isForegroundSweeping() const { return state() == State::Sweep; }
  bool isBackgroundSweeping() const { return sweepTask.isRunning(); }
  void waitBackgroundSweepEnd();
  void waitBackgroundAllocEnd() { allocTask.cancelAndWait(); }
  void waitBackgroundFreeEnd();

  void lockGC() { lock.lock(); }

  void unlockGC() { lock.unlock(); }

#ifdef DEBUG
  bool currentThreadHasLockedGC() const { return lock.ownedByCurrentThread(); }
#endif  // DEBUG

  void setAlwaysPreserveCode() { alwaysPreserveCode = true; }

  bool isIncrementalGCAllowed() const { return incrementalAllowed; }
  void disallowIncrementalGC() { incrementalAllowed = false; }

  bool isIncrementalGCEnabled() const {
    return (mode == JSGC_MODE_INCREMENTAL ||
            mode == JSGC_MODE_ZONE_INCREMENTAL) &&
           incrementalAllowed;
  }
  bool isIncrementalGCInProgress() const {
    return state() != State::NotActive && !isVerifyPreBarriersEnabled();
  }
  bool hasForegroundWork() const;

  bool isCompactingGCEnabled() const;

  bool isShrinkingGC() const { return invocationKind == GC_SHRINK; }

  bool initSweepActions();

  void setGrayRootsTracer(JSTraceDataOp traceOp, void* data);
  MOZ_MUST_USE bool addBlackRootsTracer(JSTraceDataOp traceOp, void* data);
  void removeBlackRootsTracer(JSTraceDataOp traceOp, void* data);
  void clearBlackAndGrayRootTracers();

  void updateMemoryCountersOnGCStart();

  void setGCCallback(JSGCCallback callback, void* data);
  void callGCCallback(JSGCStatus status) const;
  void setObjectsTenuredCallback(JSObjectsTenuredCallback callback, void* data);
  void callObjectsTenuredCallback();
  MOZ_MUST_USE bool addFinalizeCallback(JSFinalizeCallback callback,
                                        void* data);
  void removeFinalizeCallback(JSFinalizeCallback func);
  MOZ_MUST_USE bool addWeakPointerZonesCallback(
      JSWeakPointerZonesCallback callback, void* data);
  void removeWeakPointerZonesCallback(JSWeakPointerZonesCallback callback);
  MOZ_MUST_USE bool addWeakPointerCompartmentCallback(
      JSWeakPointerCompartmentCallback callback, void* data);
  void removeWeakPointerCompartmentCallback(
      JSWeakPointerCompartmentCallback callback);
  JS::GCSliceCallback setSliceCallback(JS::GCSliceCallback callback);
  JS::GCNurseryCollectionCallback setNurseryCollectionCallback(
      JS::GCNurseryCollectionCallback callback);
  JS::DoCycleCollectionCallback setDoCycleCollectionCallback(
      JS::DoCycleCollectionCallback callback);

  void setFullCompartmentChecks(bool enable);

  JS::Zone* getCurrentSweepGroup() { return currentSweepGroup; }
  unsigned getCurrentSweepGroupIndex() {
    return state() == State::Sweep ? sweepGroupIndex : 0;
  }

  uint64_t gcNumber() const { return number; }

  uint64_t minorGCCount() const { return minorGCNumber; }
  void incMinorGcNumber() {
    ++minorGCNumber;
    ++number;
  }

  uint64_t majorGCCount() const { return majorGCNumber; }
  void incMajorGcNumber() { ++majorGCNumber; }

  int64_t defaultSliceBudgetMS() const { return defaultTimeBudgetMS_; }

  bool isIncrementalGc() const { return isIncremental; }
  bool isFullGc() const { return isFull; }
  bool isCompactingGc() const { return isCompacting; }

  bool areGrayBitsValid() const { return grayBitsValid; }
  void setGrayBitsInvalid() { grayBitsValid = false; }

  mozilla::TimeStamp lastGCTime() const { return lastGCTime_; }

  bool majorGCRequested() const {
    return majorGCTriggerReason != JS::GCReason::NO_REASON;
  }

  bool fullGCForAtomsRequested() const { return fullGCForAtomsRequested_; }

  double computeHeapGrowthFactor(size_t lastBytes);
  size_t computeTriggerBytes(double growthFactor, size_t lastBytes);

  JSGCMode gcMode() const { return mode; }
  void setGCMode(JSGCMode m) {
    mode = m;
    marker.setGCMode(mode);
  }

  inline void updateOnFreeArenaAlloc(const ChunkInfo& info);
  inline void updateOnArenaFree();

  ChunkPool& fullChunks(const AutoLockGC& lock) { return fullChunks_.ref(); }
  ChunkPool& availableChunks(const AutoLockGC& lock) {
    return availableChunks_.ref();
  }
  ChunkPool& emptyChunks(const AutoLockGC& lock) { return emptyChunks_.ref(); }
  const ChunkPool& fullChunks(const AutoLockGC& lock) const {
    return fullChunks_.ref();
  }
  const ChunkPool& availableChunks(const AutoLockGC& lock) const {
    return availableChunks_.ref();
  }
  const ChunkPool& emptyChunks(const AutoLockGC& lock) const {
    return emptyChunks_.ref();
  }
  typedef ChainedIter<Chunk*, ChunkPool::Iter, ChunkPool::Iter>
      NonEmptyChunksIter;
  NonEmptyChunksIter allNonEmptyChunks(const AutoLockGC& lock) {
    return NonEmptyChunksIter(ChunkPool::Iter(availableChunks(lock)),
                              ChunkPool::Iter(fullChunks(lock)));
  }

  Chunk* getOrAllocChunk(AutoLockGCBgAlloc& lock);
  void recycleChunk(Chunk* chunk, const AutoLockGC& lock);

#ifdef JS_GC_ZEAL
  void startVerifyPreBarriers();
  void endVerifyPreBarriers();
  void finishVerifier();
  bool isVerifyPreBarriersEnabled() const { return verifyPreData; }
  bool shouldYieldForZeal(ZealMode mode);
#else
  bool isVerifyPreBarriersEnabled() const { return false; }
#endif

  // Queue memory memory to be freed on a background thread if possible.
  void queueUnusedLifoBlocksForFree(LifoAlloc* lifo);
  void queueAllLifoBlocksForFree(LifoAlloc* lifo);
  void queueAllLifoBlocksForFreeAfterMinorGC(LifoAlloc* lifo);
  void queueBuffersForFreeAfterMinorGC(Nursery::BufferSet& buffers);

  // Public here for ReleaseArenaLists and FinalizeTypedArenas.
  void releaseArena(Arena* arena, const AutoLockGC& lock);

  void releaseHeldRelocatedArenas();
  void releaseHeldRelocatedArenasWithoutUnlocking(const AutoLockGC& lock);

  // Allocator
  template <AllowGC allowGC>
  MOZ_MUST_USE bool checkAllocatorState(JSContext* cx, AllocKind kind);
  template <AllowGC allowGC>
  JSObject* tryNewNurseryObject(JSContext* cx, size_t thingSize,
                                size_t nDynamicSlots, const Class* clasp);
  template <AllowGC allowGC>
  static JSObject* tryNewTenuredObject(JSContext* cx, AllocKind kind,
                                       size_t thingSize, size_t nDynamicSlots);
  template <typename T, AllowGC allowGC>
  static T* tryNewTenuredThing(JSContext* cx, AllocKind kind, size_t thingSize);
  template <AllowGC allowGC>
  JSString* tryNewNurseryString(JSContext* cx, size_t thingSize,
                                AllocKind kind);
  static TenuredCell* refillFreeListInGC(Zone* zone, AllocKind thingKind);

  void setParallelAtomsAllocEnabled(bool enabled);

  void bufferGrayRoots();

  /*
   * Concurrent sweep infrastructure.
   */
  void startTask(GCParallelTask& task, gcstats::PhaseKind phase,
                 AutoLockHelperThreadState& locked);
  void joinTask(GCParallelTask& task, gcstats::PhaseKind phase,
                AutoLockHelperThreadState& locked);

  void mergeRealms(JS::Realm* source, JS::Realm* target);

 private:
  enum IncrementalResult { ResetIncremental = 0, Ok };

  // Delete an empty zone after its contents have been merged.
  void deleteEmptyZone(Zone* zone);

  // For ArenaLists::allocateFromArena()
  friend class ArenaLists;
  Chunk* pickChunk(AutoLockGCBgAlloc& lock);
  Arena* allocateArena(Chunk* chunk, Zone* zone, AllocKind kind,
                       ShouldCheckThresholds checkThresholds,
                       const AutoLockGC& lock);

  // Allocator internals
  MOZ_MUST_USE bool gcIfNeededAtAllocation(JSContext* cx);
  template <typename T>
  static void checkIncrementalZoneState(JSContext* cx, T* t);
  static TenuredCell* refillFreeListFromAnyThread(JSContext* cx,
                                                  AllocKind thingKind);
  static TenuredCell* refillFreeListFromMainThread(JSContext* cx,
                                                   AllocKind thingKind);
  static TenuredCell* refillFreeListFromHelperThread(JSContext* cx,
                                                     AllocKind thingKind);
  void attemptLastDitchGC(JSContext* cx);

  /*
   * Return the list of chunks that can be released outside the GC lock.
   * Must be called either during the GC or with the GC lock taken.
   */
  friend class BackgroundDecommitTask;
  ChunkPool expireEmptyChunkPool(const AutoLockGC& lock);
  void freeEmptyChunks(const AutoLockGC& lock);
  void prepareToFreeChunk(ChunkInfo& info);

  friend class BackgroundAllocTask;
  bool wantBackgroundAllocation(const AutoLockGC& lock) const;
  bool startBackgroundAllocTaskIfIdle();

  void requestMajorGC(JS::GCReason reason);
  SliceBudget defaultBudget(JS::GCReason reason, int64_t millis);
  IncrementalResult budgetIncrementalGC(bool nonincrementalByAPI,
                                        JS::GCReason reason,
                                        SliceBudget& budget);
  IncrementalResult resetIncrementalGC(AbortReason reason);

  // Assert if the system state is such that we should never
  // receive a request to do GC work.
  void checkCanCallAPI();

  // Check if the system state is such that GC has been supressed
  // or otherwise delayed.
  MOZ_MUST_USE bool checkIfGCAllowedInCurrentState(JS::GCReason reason);

  gcstats::ZoneGCStats scanZonesBeforeGC();

  using MaybeInvocationKind = mozilla::Maybe<JSGCInvocationKind>;

  void collect(bool nonincrementalByAPI, SliceBudget budget,
               const MaybeInvocationKind& gckind,
               JS::GCReason reason) JS_HAZ_GC_CALL;

  /*
   * Run one GC "cycle" (either a slice of incremental GC or an entire
   * non-incremental GC).
   *
   * Returns:
   *  * ResetIncremental if we "reset" an existing incremental GC, which would
   *    force us to run another cycle or
   *  * Ok otherwise.
   */
  MOZ_MUST_USE IncrementalResult gcCycle(bool nonincrementalByAPI,
                                         SliceBudget budget,
                                         const MaybeInvocationKind& gckind,
                                         JS::GCReason reason);
  bool shouldRepeatForDeadZone(JS::GCReason reason);
  void incrementalSlice(SliceBudget& budget, const MaybeInvocationKind& gckind,
                        JS::GCReason reason, AutoGCSession& session);
  MOZ_MUST_USE bool shouldCollectNurseryForSlice(bool nonincrementalByAPI,
                                                 SliceBudget& budget);

  friend class AutoCallGCCallbacks;
  void maybeCallGCCallback(JSGCStatus status);

  void pushZealSelectedObjects();
  void purgeRuntime();
  MOZ_MUST_USE bool beginMarkPhase(JS::GCReason reason, AutoGCSession& session);
  bool prepareZonesForCollection(JS::GCReason reason, bool* isFullOut);
  bool shouldPreserveJITCode(JS::Realm* realm,
                             const mozilla::TimeStamp& currentTime,
                             JS::GCReason reason, bool canAllocateMoreCode);
  void startBackgroundFreeAfterMinorGC();
  void traceRuntimeForMajorGC(JSTracer* trc, AutoGCSession& session);
  void traceRuntimeAtoms(JSTracer* trc, const AutoAccessAtomsZone& atomsAccess);
  void traceKeptAtoms(JSTracer* trc);
  void traceRuntimeCommon(JSTracer* trc, TraceOrMarkRuntime traceOrMark);
  void traceEmbeddingBlackRoots(JSTracer* trc);
  void traceEmbeddingGrayRoots(JSTracer* trc);
  void checkNoRuntimeRoots(AutoGCSession& session);
  void maybeDoCycleCollection();
  void markCompartments();
  IncrementalProgress markUntilBudgetExhausted(SliceBudget& sliceBudget,
                                               gcstats::PhaseKind phase);
  void drainMarkStack();
  template <class ZoneIterT>
  void markWeakReferences(gcstats::PhaseKind phase);
  void markWeakReferencesInCurrentGroup(gcstats::PhaseKind phase);
  template <class ZoneIterT>
  void markGrayRoots(gcstats::PhaseKind phase);
  void markBufferedGrayRoots(JS::Zone* zone);
  void markAllWeakReferences(gcstats::PhaseKind phase);
  void markAllGrayReferences(gcstats::PhaseKind phase);

  void beginSweepPhase(JS::GCReason reason, AutoGCSession& session);
  void groupZonesForSweeping(JS::GCReason reason);
  MOZ_MUST_USE bool findSweepGroupEdges();
  void getNextSweepGroup();
  IncrementalProgress markGrayReferencesInCurrentGroup(JSFreeOp* fop,
                                                       SliceBudget& budget);
  IncrementalProgress endMarkingSweepGroup(JSFreeOp* fop, SliceBudget& budget);
  void markIncomingCrossCompartmentPointers(MarkColor color);
  IncrementalProgress beginSweepingSweepGroup(JSFreeOp* fop,
                                              SliceBudget& budget);
  void sweepDebuggerOnMainThread(JSFreeOp* fop);
  void sweepJitDataOnMainThread(JSFreeOp* fop);
  IncrementalProgress endSweepingSweepGroup(JSFreeOp* fop, SliceBudget& budget);
  IncrementalProgress performSweepActions(SliceBudget& sliceBudget);
  IncrementalProgress sweepTypeInformation(JSFreeOp* fop, SliceBudget& budget);
  IncrementalProgress releaseSweptEmptyArenas(JSFreeOp* fop,
                                              SliceBudget& budget);
  void startSweepingAtomsTable();
  IncrementalProgress sweepAtomsTable(JSFreeOp* fop, SliceBudget& budget);
  IncrementalProgress sweepWeakCaches(JSFreeOp* fop, SliceBudget& budget);
  IncrementalProgress finalizeAllocKind(JSFreeOp* fop, SliceBudget& budget);
  IncrementalProgress sweepShapeTree(JSFreeOp* fop, SliceBudget& budget);
  void endSweepPhase(bool lastGC);
  bool allCCVisibleZonesWereCollected() const;
  void sweepZones(JSFreeOp* fop, bool destroyingRuntime);
  void decommitFreeArenasWithoutUnlocking(const AutoLockGC& lock);
  void startDecommit();
  void queueZonesAndStartBackgroundSweep(ZoneList& zones);
  void sweepFromBackgroundThread(AutoLockHelperThreadState& lock);
  void startBackgroundFree();
  void freeFromBackgroundThread(AutoLockHelperThreadState& lock);
  void sweepBackgroundThings(ZoneList& zones, LifoAlloc& freeBlocks);
  void assertBackgroundSweepingFinished();
  bool shouldCompact();
  void beginCompactPhase();
  IncrementalProgress compactPhase(JS::GCReason reason,
                                   SliceBudget& sliceBudget,
                                   AutoGCSession& session);
  void endCompactPhase();
  void sweepTypesAfterCompacting(Zone* zone);
  void sweepZoneAfterCompacting(Zone* zone);
  MOZ_MUST_USE bool relocateArenas(Zone* zone, JS::GCReason reason,
                                   Arena*& relocatedListOut,
                                   SliceBudget& sliceBudget);
  void updateTypeDescrObjects(MovingTracer* trc, Zone* zone);
  void updateCellPointers(Zone* zone, AllocKinds kinds, size_t bgTaskCount);
  void updateAllCellPointers(MovingTracer* trc, Zone* zone);
  void updateZonePointersToRelocatedCells(Zone* zone);
  void updateRuntimePointersToRelocatedCells(AutoGCSession& session);
  void protectAndHoldArenas(Arena* arenaList);
  void unprotectHeldRelocatedArenas();
  void clearRelocatedArenas(Arena* arenaList, JS::GCReason reason);
  void clearRelocatedArenasWithoutUnlocking(Arena* arenaList,
                                            JS::GCReason reason,
                                            const AutoLockGC& lock);
  void releaseRelocatedArenas(Arena* arenaList);
  void releaseRelocatedArenasWithoutUnlocking(Arena* arenaList,
                                              const AutoLockGC& lock);
  void finishCollection();

  void computeNonIncrementalMarkingForValidation(AutoGCSession& session);
  void validateIncrementalMarking();
  void finishMarkingValidation();

#ifdef DEBUG
  void checkForCompartmentMismatches();
#endif

  void callFinalizeCallbacks(JSFreeOp* fop, JSFinalizeStatus status) const;
  void callWeakPointerZonesCallbacks() const;
  void callWeakPointerCompartmentCallbacks(JS::Compartment* comp) const;
  void callDoCycleCollectionCallback(JSContext* cx);

 public:
  JSRuntime* const rt;

  /* Embedders can use this zone and group however they wish. */
  UnprotectedData<JS::Zone*> systemZone;

  // All zones in the runtime, except the atoms zone.
 private:
  MainThreadOrGCTaskData<ZoneVector> zones_;

 public:
  ZoneVector& zones() { return zones_.ref(); }

  // The unique atoms zone.
  WriteOnceData<Zone*> atomsZone;

 private:
  UnprotectedData<gcstats::Statistics> stats_;

 public:
  gcstats::Statistics& stats() { return stats_.ref(); }

  GCMarker marker;

  Vector<JS::GCCellPtr, 0, SystemAllocPolicy> unmarkGrayStack;

  /* Track heap size for this runtime. */
  HeapSize heapSize;

  /* GC scheduling state and parameters. */
  GCSchedulingTunables tunables;
  GCSchedulingState schedulingState;

  // State used for managing atom mark bitmaps in each zone.
  AtomMarkingRuntime atomMarking;

 private:
  // When chunks are empty, they reside in the emptyChunks pool and are
  // re-used as needed or eventually expired if not re-used. The emptyChunks
  // pool gets refilled from the background allocation task heuristically so
  // that empty chunks should always be available for immediate allocation
  // without syscalls.
  GCLockData<ChunkPool> emptyChunks_;

  // Chunks which have had some, but not all, of their arenas allocated live
  // in the available chunk lists. When all available arenas in a chunk have
  // been allocated, the chunk is removed from the available list and moved
  // to the fullChunks pool. During a GC, if all arenas are free, the chunk
  // is moved back to the emptyChunks pool and scheduled for eventual
  // release.
  GCLockData<ChunkPool> availableChunks_;

  // When all arenas in a chunk are used, it is moved to the fullChunks pool
  // so as to reduce the cost of operations on the available lists.
  GCLockData<ChunkPool> fullChunks_;

  MainThreadData<RootedValueMap> rootsHash;

  // An incrementing id used to assign unique ids to cells that require one.
  mozilla::Atomic<uint64_t, mozilla::ReleaseAcquire,
                  mozilla::recordreplay::Behavior::DontPreserve>
      nextCellUniqueId_;

  /*
   * Number of the committed arenas in all GC chunks including empty chunks.
   */
  mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire,
                  mozilla::recordreplay::Behavior::DontPreserve>
      numArenasFreeCommitted;
  MainThreadData<VerifyPreTracer*> verifyPreData;

 private:
  UnprotectedData<bool> chunkAllocationSinceLastGC;
  MainThreadData<mozilla::TimeStamp> lastGCTime_;

  /*
   * JSGC_MODE
   * prefs: javascript.options.mem.gc_per_zone and
   *   javascript.options.mem.gc_incremental.
   */
  MainThreadData<JSGCMode> mode;

  mozilla::Atomic<size_t, mozilla::ReleaseAcquire,
                  mozilla::recordreplay::Behavior::DontPreserve>
      numActiveZoneIters;

  /* During shutdown, the GC needs to clean up every possible object. */
  MainThreadData<bool> cleanUpEverything;

  // Gray marking must be done after all black marking is complete. However,
  // we do not have write barriers on XPConnect roots. Therefore, XPConnect
  // roots must be accumulated in the first slice of incremental GC. We
  // accumulate these roots in each zone's gcGrayRoots vector and then mark
  // them later, after black marking is complete for each compartment. This
  // accumulation can fail, but in that case we switch to non-incremental GC.
  enum class GrayBufferState { Unused, Okay, Failed };
  MainThreadOrGCTaskData<GrayBufferState> grayBufferState;
  bool hasValidGrayRootsBuffer() const {
    return grayBufferState == GrayBufferState::Okay;
  }

  // Clear each zone's gray buffers, but do not change the current state.
  void resetBufferedGrayRoots() const;

  // Reset the gray buffering state to Unused.
  void clearBufferedGrayRoots() {
    grayBufferState = GrayBufferState::Unused;
    resetBufferedGrayRoots();
  }

  /*
   * The gray bits can become invalid if UnmarkGray overflows the stack. A
   * full GC will reset this bit, since it fills in all the gray bits.
   */
  UnprotectedData<bool> grayBitsValid;

  mozilla::Atomic<JS::GCReason, mozilla::Relaxed,
                  mozilla::recordreplay::Behavior::DontPreserve>
      majorGCTriggerReason;

 private:
  /* Perform full GC if rt->keepAtoms() becomes false. */
  MainThreadData<bool> fullGCForAtomsRequested_;

  /* Incremented at the start of every minor GC. */
  MainThreadData<uint64_t> minorGCNumber;

  /* Incremented at the start of every major GC. */
  MainThreadData<uint64_t> majorGCNumber;

  /* Incremented on every GC slice or minor collection. */
  MainThreadData<uint64_t> number;

  /* Whether the currently running GC can finish in multiple slices. */
  MainThreadData<bool> isIncremental;

  /* Whether all zones are being collected in first GC slice. */
  MainThreadData<bool> isFull;

  /* Whether the heap will be compacted at the end of GC. */
  MainThreadData<bool> isCompacting;

  /* The invocation kind of the current GC, taken from the first slice. */
  MainThreadOrGCTaskData<JSGCInvocationKind> invocationKind;

  /* The initial GC reason, taken from the first slice. */
  MainThreadData<JS::GCReason> initialReason;

  /*
   * The current incremental GC phase. This is also used internally in
   * non-incremental GC.
   */
  MainThreadOrGCTaskData<State> incrementalState;

  /* The incremental state at the start of this slice. */
  MainThreadData<State> initialState;

#ifdef JS_GC_ZEAL
  /* Whether to pay attention the zeal settings in this incremental slice. */
  MainThreadData<bool> useZeal;
#endif

  /* Indicates that the last incremental slice exhausted the mark stack. */
  MainThreadData<bool> lastMarkSlice;

  // Whether it's currently safe to yield to the mutator in an incremental GC.
  MainThreadData<bool> safeToYield;

  /* Whether any sweeping will take place in the separate GC helper thread. */
  MainThreadData<bool> sweepOnBackgroundThread;

  /* Singly linked list of zones to be swept in the background. */
  HelperThreadLockData<ZoneList> backgroundSweepZones;

  /*
   * Free LIFO blocks are transferred to these allocators before being freed on
   * a background thread.
   */
  HelperThreadLockData<LifoAlloc> lifoBlocksToFree;
  MainThreadData<LifoAlloc> lifoBlocksToFreeAfterMinorGC;
  HelperThreadLockData<Nursery::BufferSet> buffersToFreeAfterMinorGC;

  /* Index of current sweep group (for stats). */
  MainThreadData<unsigned> sweepGroupIndex;

  /*
   * Incremental sweep state.
   */

  MainThreadData<JS::Zone*> sweepGroups;
  MainThreadOrGCTaskData<JS::Zone*> currentSweepGroup;
  MainThreadData<UniquePtr<SweepAction>> sweepActions;
  MainThreadOrGCTaskData<JS::Zone*> sweepZone;
  MainThreadOrGCTaskData<AllocKind> sweepAllocKind;
  MainThreadData<mozilla::Maybe<AtomsTable::SweepIterator>> maybeAtomsToSweep;
  MainThreadOrGCTaskData<JS::detail::WeakCacheBase*> sweepCache;
  MainThreadData<bool> hasMarkedGrayRoots;
  MainThreadData<bool> abortSweepAfterCurrentGroup;

#ifdef DEBUG
  // During gray marking, delay AssertCellIsNotGray checks by
  // recording the cell pointers here and checking after marking has
  // finished.
  MainThreadData<Vector<const Cell*, 0, SystemAllocPolicy>>
      cellsToAssertNotGray;
  friend void js::gc::detail::AssertCellIsNotGray(const Cell*);
#endif

  friend class SweepGroupsIter;
  friend class WeakCacheSweepIterator;

  /*
   * Incremental compacting state.
   */
  MainThreadData<bool> startedCompacting;
  MainThreadData<ZoneList> zonesToMaybeCompact;
  MainThreadData<Arena*> relocatedArenasToRelease;

#ifdef JS_GC_ZEAL
  MainThreadData<MarkingValidator*> markingValidator;
#endif

  /*
   * Default budget for incremental GC slice. See js/SliceBudget.h.
   *
   * JSGC_SLICE_TIME_BUDGET_MS
   * pref: javascript.options.mem.gc_incremental_slice_ms,
   */
  MainThreadData<int64_t> defaultTimeBudgetMS_;

  /*
   * We disable incremental GC if we encounter a Class with a trace hook
   * that does not implement write barriers.
   */
  MainThreadData<bool> incrementalAllowed;

  /*
   * Whether compacting GC can is enabled globally.
   *
   * JSGC_COMPACTING_ENABLED
   * pref: javascript.options.mem.gc_compacting
   */
  MainThreadData<bool> compactingEnabled;

  MainThreadData<bool> rootsRemoved;

  /*
   * These options control the zealousness of the GC. At every allocation,
   * nextScheduled is decremented. When it reaches zero we do a full GC.
   *
   * At this point, if zeal_ is one of the types that trigger periodic
   * collection, then nextScheduled is reset to the value of zealFrequency.
   * Otherwise, no additional GCs take place.
   *
   * You can control these values in several ways:
   *   - Set the JS_GC_ZEAL environment variable
   *   - Call gczeal() or schedulegc() from inside shell-executed JS code
   *     (see the help for details)
   *
   * If gcZeal_ == 1 then we perform GCs in select places (during MaybeGC and
   * whenever we are notified that GC roots have been removed). This option is
   * mainly useful to embedders.
   *
   * We use zeal_ == 4 to enable write barrier verification. See the comment
   * in gc/Verifier.cpp for more information about this.
   *
   * zeal_ values from 8 to 10 periodically run different types of
   * incremental GC.
   *
   * zeal_ value 14 performs periodic shrinking collections.
   */
#ifdef JS_GC_ZEAL
  static_assert(size_t(ZealMode::Count) <= 32,
                "Too many zeal modes to store in a uint32_t");
  MainThreadData<uint32_t> zealModeBits;
  MainThreadData<int> zealFrequency;
  MainThreadData<int> nextScheduled;
  MainThreadData<bool> deterministicOnly;
  MainThreadData<int> incrementalLimit;

  MainThreadData<Vector<JSObject*, 0, SystemAllocPolicy>> selectedForMarking;
#endif

  MainThreadData<bool> fullCompartmentChecks;

  MainThreadData<uint32_t> gcCallbackDepth;

  Callback<JSGCCallback> gcCallback;
  Callback<JS::DoCycleCollectionCallback> gcDoCycleCollectionCallback;
  Callback<JSObjectsTenuredCallback> tenuredCallback;
  CallbackVector<JSFinalizeCallback> finalizeCallbacks;
  CallbackVector<JSWeakPointerZonesCallback> updateWeakPointerZonesCallbacks;
  CallbackVector<JSWeakPointerCompartmentCallback>
      updateWeakPointerCompartmentCallbacks;

  /*
   * The trace operations to trace embedding-specific GC roots. One is for
   * tracing through black roots and the other is for tracing through gray
   * roots. The black/gray distinction is only relevant to the cycle
   * collector.
   */
  CallbackVector<JSTraceDataOp> blackRootTracers;
  Callback<JSTraceDataOp> grayRootTracer;

  /* Always preserve JIT code during GCs, for testing. */
  MainThreadData<bool> alwaysPreserveCode;

  MainThreadData<bool> lowMemoryState;

#ifdef DEBUG
  MainThreadData<bool> arenasEmptyAtShutdown;
#endif

  /* Synchronize GC heap access among GC helper threads and the main thread. */
  friend class js::AutoLockGC;
  friend class js::AutoLockGCBgAlloc;
  js::Mutex lock;

  friend class BackgroundSweepTask;
  friend class BackgroundFreeTask;

  BackgroundAllocTask allocTask;
  BackgroundSweepTask sweepTask;
  BackgroundFreeTask freeTask;
  BackgroundDecommitTask decommitTask;

  /*
   * During incremental sweeping, this field temporarily holds the arenas of
   * the current AllocKind being swept in order of increasing free space.
   */
  MainThreadData<SortedArenaList> incrementalSweepList;

 private:
  MainThreadData<Nursery> nursery_;
  MainThreadData<gc::StoreBuffer> storeBuffer_;

 public:
  Nursery& nursery() { return nursery_.ref(); }
  gc::StoreBuffer& storeBuffer() { return storeBuffer_.ref(); }

  void* addressOfNurseryPosition() {
    return nursery_.refNoCheck().addressOfPosition();
  }
  const void* addressOfNurseryCurrentEnd() {
    return nursery_.refNoCheck().addressOfCurrentEnd();
  }
  const void* addressOfStringNurseryCurrentEnd() {
    return nursery_.refNoCheck().addressOfCurrentStringEnd();
  }
  uint32_t* addressOfNurseryAllocCount() {
    return stats().addressOfAllocsSinceMinorGCNursery();
  }

  void minorGC(JS::GCReason reason,
               gcstats::PhaseKind phase = gcstats::PhaseKind::MINOR_GC)
      JS_HAZ_GC_CALL;
  void evictNursery(JS::GCReason reason = JS::GCReason::EVICT_NURSERY) {
    minorGC(reason, gcstats::PhaseKind::EVICT_NURSERY);
  }

  mozilla::TimeStamp lastLastDitchTime;

  friend class MarkingValidator;
  friend class AutoEnterIteration;
};

/* Prevent compartments and zones from being collected during iteration. */
class MOZ_RAII AutoEnterIteration {
  GCRuntime* gc;

 public:
  explicit AutoEnterIteration(GCRuntime* gc_) : gc(gc_) {
    ++gc->numActiveZoneIters;
  }

  ~AutoEnterIteration() {
    MOZ_ASSERT(gc->numActiveZoneIters);
    --gc->numActiveZoneIters;
  }
};

#ifdef JS_GC_ZEAL

inline bool GCRuntime::hasZealMode(ZealMode mode) {
  static_assert(size_t(ZealMode::Limit) < sizeof(zealModeBits) * 8,
                "Zeal modes must fit in zealModeBits");
  return zealModeBits & (1 << uint32_t(mode));
}

inline void GCRuntime::clearZealMode(ZealMode mode) {
  zealModeBits &= ~(1 << uint32_t(mode));
  MOZ_ASSERT(!hasZealMode(mode));
}

inline bool GCRuntime::upcomingZealousGC() { return nextScheduled == 1; }

inline bool GCRuntime::needZealousGC() {
  if (nextScheduled > 0 && --nextScheduled == 0) {
    if (hasZealMode(ZealMode::Alloc) || hasZealMode(ZealMode::GenerationalGC) ||
        hasZealMode(ZealMode::IncrementalMultipleSlices) ||
        hasZealMode(ZealMode::Compact) || hasIncrementalTwoSliceZealMode()) {
      nextScheduled = zealFrequency;
    }
    return true;
  }
  return false;
}

inline bool GCRuntime::hasIncrementalTwoSliceZealMode() {
  return hasZealMode(ZealMode::YieldBeforeMarking) ||
         hasZealMode(ZealMode::YieldBeforeSweeping) ||
         hasZealMode(ZealMode::YieldBeforeSweepingAtoms) ||
         hasZealMode(ZealMode::YieldBeforeSweepingCaches) ||
         hasZealMode(ZealMode::YieldBeforeSweepingTypes) ||
         hasZealMode(ZealMode::YieldBeforeSweepingObjects) ||
         hasZealMode(ZealMode::YieldBeforeSweepingNonObjects) ||
         hasZealMode(ZealMode::YieldBeforeSweepingShapeTrees) ||
         hasZealMode(ZealMode::YieldWhileGrayMarking);
}

#else
inline bool GCRuntime::hasZealMode(ZealMode mode) { return false; }
inline void GCRuntime::clearZealMode(ZealMode mode) {}
inline bool GCRuntime::upcomingZealousGC() { return false; }
inline bool GCRuntime::needZealousGC() { return false; }
inline bool GCRuntime::hasIncrementalTwoSliceZealMode() { return false; }
#endif

bool IsCurrentlyAnimating(const mozilla::TimeStamp& lastAnimationTime,
                          const mozilla::TimeStamp& currentTime);

} /* namespace gc */
} /* namespace js */

#endif
