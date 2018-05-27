/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_GCRuntime_h
#define gc_GCRuntime_h

#include "mozilla/Atomics.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Maybe.h"

#include "gc/ArenaList.h"
#include "gc/AtomMarking.h"
#include "gc/GCHelperState.h"
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

class AutoLockGC;
class AutoLockGCBgAlloc;
class AutoLockHelperThreadState;
class VerifyPreTracer;

namespace gc {

using BlackGrayEdgeVector = Vector<TenuredCell*, 0, SystemAllocPolicy>;
using ZoneVector = Vector<JS::Zone*, 4, SystemAllocPolicy>;

class AutoCallGCCallbacks;
class AutoRunParallelTask;
class AutoTraceSession;
class MarkingValidator;
struct MovingTracer;
enum class ShouldCheckThresholds;
class SweepGroupsIter;
class WeakCacheSweepIterator;

enum IncrementalProgress
{
    NotFinished = 0,
    Finished
};

// Interface to a sweep action.
//
// Note that we don't need perfect forwarding for args here because the
// types are not deduced but come ultimately from the type of a function pointer
// passed to SweepFunc.
template <typename... Args>
struct SweepAction
{
    virtual ~SweepAction() {}
    virtual IncrementalProgress run(Args... args) = 0;
    virtual void assertFinished() const = 0;
};

class ChunkPool
{
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

    Chunk* head() { MOZ_ASSERT(head_); return head_; }
    Chunk* pop();
    void push(Chunk* chunk);
    Chunk* remove(Chunk* chunk);

#ifdef DEBUG
    bool contains(Chunk* chunk) const;
    bool verify() const;
#endif

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

// Performs extra allocation off thread so that when memory is required on the
// main thread it will already be available and waiting.
class BackgroundAllocTask : public GCParallelTaskHelper<BackgroundAllocTask>
{
    // Guarded by the GC lock.
    GCLockData<ChunkPool&> chunkPool_;

    const bool enabled_;

  public:
    BackgroundAllocTask(JSRuntime* rt, ChunkPool& pool);
    bool enabled() const { return enabled_; }

    void run();
};

// Search the provided Chunks for free arenas and decommit them.
class BackgroundDecommitTask : public GCParallelTaskHelper<BackgroundDecommitTask>
{
  public:
    using ChunkVector = mozilla::Vector<Chunk*>;

    explicit BackgroundDecommitTask(JSRuntime *rt) : GCParallelTaskHelper(rt) {}
    void setChunksToScan(ChunkVector &chunks);

    void run();

  private:
    MainThreadOrGCTaskData<ChunkVector> toDecommit;
};

template<typename F>
struct Callback {
    MainThreadOrGCTaskData<F> op;
    MainThreadOrGCTaskData<void*> data;

    Callback()
      : op(nullptr), data(nullptr)
    {}
    Callback(F op, void* data)
      : op(op), data(data)
    {}
};

template<typename F>
using CallbackVector = MainThreadData<Vector<Callback<F>, 4, SystemAllocPolicy>>;

template <typename T, typename Iter0, typename Iter1>
class ChainedIter
{
    Iter0 iter0_;
    Iter1 iter1_;

  public:
    ChainedIter(const Iter0& iter0, const Iter1& iter1)
      : iter0_(iter0), iter1_(iter1)
    {}

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
        if (!iter0_.done())
            return iter0_.get();
        MOZ_ASSERT(!iter1_.done());
        return iter1_.get();
    }

    operator T() const { return get(); }
    T operator->() const { return get(); }
};

typedef HashMap<Value*, const char*, DefaultHasher<Value*>, SystemAllocPolicy> RootedValueMap;

using AllocKinds = mozilla::EnumSet<AllocKind>;

// A singly linked list of zones.
class ZoneList
{
    static Zone * const End;

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

class GCRuntime
{
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

    MOZ_MUST_USE bool setParameter(JSGCParamKey key, uint32_t value, AutoLockGC& lock);
    void resetParameter(JSGCParamKey key, AutoLockGC& lock);
    uint32_t getParameter(JSGCParamKey key, const AutoLockGC& lock);

    MOZ_MUST_USE bool triggerGC(JS::gcreason::Reason reason);
    void maybeAllocTriggerZoneGC(Zone* zone, const AutoLockGC& lock);
    // The return value indicates if we were able to do the GC.
    bool triggerZoneGC(Zone* zone, JS::gcreason::Reason reason,
                       size_t usedBytes, size_t thresholdBytes);
    void maybeGC(Zone* zone);
    // The return value indicates whether a major GC was performed.
    bool gcIfRequested();
    void gc(JSGCInvocationKind gckind, JS::gcreason::Reason reason);
    void startGC(JSGCInvocationKind gckind, JS::gcreason::Reason reason, int64_t millis = 0);
    void gcSlice(JS::gcreason::Reason reason, int64_t millis = 0);
    void finishGC(JS::gcreason::Reason reason);
    void abortGC();
    void startDebugGC(JSGCInvocationKind gckind, SliceBudget& budget);
    void debugGCSlice(SliceBudget& budget);

    void triggerFullGCForAtoms(JSContext* cx);

    void runDebugGC();
    void notifyRootsRemoved();

    enum TraceOrMarkRuntime {
        TraceRuntime,
        MarkRuntime
    };
    void traceRuntime(JSTracer* trc, AutoTraceSession& session);
    void traceRuntimeForMinorGC(JSTracer* trc, AutoTraceSession& session);

    void purgeRuntimeForMinorGC();

    void shrinkBuffers();
    void onOutOfMallocMemory();
    void onOutOfMallocMemory(const AutoLockGC& lock);

#ifdef JS_GC_ZEAL
    const void* addressOfZealModeBits() { return &zealModeBits; }
    void getZealBits(uint32_t* zealBits, uint32_t* frequency, uint32_t* nextScheduled);
    void setZeal(uint8_t zeal, uint32_t frequency);
    bool parseAndSetZeal(const char* str);
    void setNextScheduled(uint32_t count);
    void verifyPreBarriers();
    void maybeVerifyPreBarriers(bool always);
    bool selectForMarking(JSObject* object);
    void clearSelectedForMarking();
    void setDeterministic(bool enable);
#endif

#ifdef ENABLE_WASM_GC
    // If we run with wasm-gc enabled and there's wasm frames on the stack,
    // then GCs are suppressed and many APIs should not be available.
    // TODO (bug 1456824) This is temporary and should be removed once proper
    // GC support is implemented.
    static bool temporaryAbortIfWasmGc(JSContext* cx);
#else
    static bool temporaryAbortIfWasmGc(JSContext* cx) { return false; }
#endif

    uint64_t nextCellUniqueId() {
        MOZ_ASSERT(nextCellUniqueId_ > 0);
        uint64_t uid = ++nextCellUniqueId_;
        return uid;
    }

#ifdef DEBUG
    bool shutdownCollectedEverything() const {
        return arenasEmptyAtShutdown;
    }
#endif

  public:
    // Internal public interface
    State state() const { return incrementalState; }
    bool isHeapCompacting() const { return state() == State::Compact; }
    bool isForegroundSweeping() const { return state() == State::Sweep; }
    bool isBackgroundSweeping() { return helperState.isBackgroundSweeping(); }
    void waitBackgroundSweepEnd() { helperState.waitBackgroundSweepEnd(); }
    void waitBackgroundSweepOrAllocEnd() {
        helperState.waitBackgroundSweepEnd();
        allocTask.cancelAndWait();
    }

#ifdef DEBUG
    bool onBackgroundThread() { return helperState.onBackgroundThread(); }
#endif // DEBUG

    void lockGC() {
        lock.lock();
    }

    void unlockGC() {
        lock.unlock();
    }

#ifdef DEBUG
    bool currentThreadHasLockedGC() const {
        return lock.ownedByCurrentThread();
    }
#endif // DEBUG

    void setAlwaysPreserveCode() { alwaysPreserveCode = true; }

    bool isIncrementalGCAllowed() const { return incrementalAllowed; }
    void disallowIncrementalGC() { incrementalAllowed = false; }

    bool isIncrementalGCEnabled() const { return mode == JSGC_MODE_INCREMENTAL && incrementalAllowed; }
    bool isIncrementalGCInProgress() const { return state() != State::NotActive; }

    bool isCompactingGCEnabled() const;

    bool isShrinkingGC() const { return invocationKind == GC_SHRINK; }

    bool initSweepActions();

    void setGrayRootsTracer(JSTraceDataOp traceOp, void* data);
    MOZ_MUST_USE bool addBlackRootsTracer(JSTraceDataOp traceOp, void* data);
    void removeBlackRootsTracer(JSTraceDataOp traceOp, void* data);

    int32_t getMallocBytes() const { return mallocCounter.bytes(); }
    size_t maxMallocBytesAllocated() const { return mallocCounter.maxBytes(); }
    void setMaxMallocBytes(size_t value, const AutoLockGC& lock);

    bool updateMallocCounter(size_t nbytes) {
        mallocCounter.update(nbytes);
        TriggerKind trigger = mallocCounter.shouldTriggerGC(tunables);
        if (MOZ_LIKELY(trigger == NoTrigger) || trigger <= mallocCounter.triggered())
            return false;

        if (!triggerGC(JS::gcreason::TOO_MUCH_MALLOC))
            return false;

        // Even though this method may be called off the main thread it is safe
        // to access mallocCounter here since triggerGC() will return false in
        // that case.
        stats().recordTrigger(mallocCounter.bytes(), mallocCounter.maxBytes());

        mallocCounter.recordTrigger(trigger);
        return true;
    }

    void updateMallocCountersOnGCStart();

    void setGCCallback(JSGCCallback callback, void* data);
    void callGCCallback(JSGCStatus status) const;
    void setObjectsTenuredCallback(JSObjectsTenuredCallback callback,
                                   void* data);
    void callObjectsTenuredCallback();
    MOZ_MUST_USE bool addFinalizeCallback(JSFinalizeCallback callback, void* data);
    void removeFinalizeCallback(JSFinalizeCallback func);
    MOZ_MUST_USE bool addWeakPointerZonesCallback(JSWeakPointerZonesCallback callback,
                                                      void* data);
    void removeWeakPointerZonesCallback(JSWeakPointerZonesCallback callback);
    MOZ_MUST_USE bool addWeakPointerCompartmentCallback(JSWeakPointerCompartmentCallback callback,
                                                        void* data);
    void removeWeakPointerCompartmentCallback(JSWeakPointerCompartmentCallback callback);
    JS::GCSliceCallback setSliceCallback(JS::GCSliceCallback callback);
    JS::GCNurseryCollectionCallback setNurseryCollectionCallback(
        JS::GCNurseryCollectionCallback callback);
    JS::DoCycleCollectionCallback setDoCycleCollectionCallback(JS::DoCycleCollectionCallback callback);
    void callDoCycleCollectionCallback(JSContext* cx);

    void setFullCompartmentChecks(bool enable);

    JS::Zone* getCurrentSweepGroup() { return currentSweepGroup; }

    uint64_t gcNumber() const { return number; }

    uint64_t minorGCCount() const { return minorGCNumber; }
    void incMinorGcNumber() { ++minorGCNumber; ++number; }

    uint64_t majorGCCount() const { return majorGCNumber; }
    void incMajorGcNumber() { ++majorGCNumber; ++number; }

    int64_t defaultSliceBudget() const { return defaultTimeBudget_; }

    bool isIncrementalGc() const { return isIncremental; }
    bool isFullGc() const { return isFull; }
    bool isCompactingGc() const { return isCompacting; }

    bool areGrayBitsValid() const { return grayBitsValid; }
    void setGrayBitsInvalid() { grayBitsValid = false; }

    bool majorGCRequested() const { return majorGCTriggerReason != JS::gcreason::NO_REASON; }

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
    ChunkPool& availableChunks(const AutoLockGC& lock) { return availableChunks_.ref(); }
    ChunkPool& emptyChunks(const AutoLockGC& lock) { return emptyChunks_.ref(); }
    const ChunkPool& fullChunks(const AutoLockGC& lock) const { return fullChunks_.ref(); }
    const ChunkPool& availableChunks(const AutoLockGC& lock) const { return availableChunks_.ref(); }
    const ChunkPool& emptyChunks(const AutoLockGC& lock) const { return emptyChunks_.ref(); }
    typedef ChainedIter<Chunk*, ChunkPool::Iter, ChunkPool::Iter> NonEmptyChunksIter;
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
    bool isVerifyPreBarriersEnabled() const { return !!verifyPreData; }
    bool shouldYieldForZeal(ZealMode mode);
#else
    bool isVerifyPreBarriersEnabled() const { return false; }
#endif

    // Free certain LifoAlloc blocks when it is safe to do so.
    void freeUnusedLifoBlocksAfterSweeping(LifoAlloc* lifo);
    void freeAllLifoBlocksAfterSweeping(LifoAlloc* lifo);

    // Public here for ReleaseArenaLists and FinalizeTypedArenas.
    void releaseArena(Arena* arena, const AutoLockGC& lock);

    void releaseHeldRelocatedArenas();
    void releaseHeldRelocatedArenasWithoutUnlocking(const AutoLockGC& lock);

    // Allocator
    template <AllowGC allowGC>
    MOZ_MUST_USE bool checkAllocatorState(JSContext* cx, AllocKind kind);
    template <AllowGC allowGC>
    JSObject* tryNewNurseryObject(JSContext* cx, size_t thingSize, size_t nDynamicSlots,
                                  const Class* clasp);
    template <AllowGC allowGC>
    static JSObject* tryNewTenuredObject(JSContext* cx, AllocKind kind, size_t thingSize,
                                         size_t nDynamicSlots);
    template <typename T, AllowGC allowGC>
    static T* tryNewTenuredThing(JSContext* cx, AllocKind kind, size_t thingSize);
    template <AllowGC allowGC>
    JSString* tryNewNurseryString(JSContext* cx, size_t thingSize, AllocKind kind);
    static TenuredCell* refillFreeListInGC(Zone* zone, AllocKind thingKind);

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
    enum IncrementalResult
    {
        Reset = 0,
        Ok
    };

    // Delete an empty zone after its contents have been merged.
    void deleteEmptyZone(Zone* zone);

    // For ArenaLists::allocateFromArena()
    friend class ArenaLists;
    Chunk* pickChunk(AutoLockGCBgAlloc& lock);
    Arena* allocateArena(Chunk* chunk, Zone* zone, AllocKind kind,
                         ShouldCheckThresholds checkThresholds, const AutoLockGC& lock);


    void arenaAllocatedDuringGC(JS::Zone* zone, Arena* arena);

    // Allocator internals
    MOZ_MUST_USE bool gcIfNeededAtAllocation(JSContext* cx);
    template <typename T>
    static void checkIncrementalZoneState(JSContext* cx, T* t);
    static TenuredCell* refillFreeListFromAnyThread(JSContext* cx, AllocKind thingKind);
    static TenuredCell* refillFreeListFromMainThread(JSContext* cx, AllocKind thingKind);
    static TenuredCell* refillFreeListFromHelperThread(JSContext* cx, AllocKind thingKind);

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
    void startBackgroundAllocTaskIfIdle();

    void requestMajorGC(JS::gcreason::Reason reason);
    SliceBudget defaultBudget(JS::gcreason::Reason reason, int64_t millis);
    IncrementalResult budgetIncrementalGC(bool nonincrementalByAPI, JS::gcreason::Reason reason,
                                          SliceBudget& budget, AutoTraceSession& session);
    IncrementalResult resetIncrementalGC(AbortReason reason, AutoTraceSession& session);

    // Assert if the system state is such that we should never
    // receive a request to do GC work.
    void checkCanCallAPI();

    // Check if the system state is such that GC has been supressed
    // or otherwise delayed.
    MOZ_MUST_USE bool checkIfGCAllowedInCurrentState(JS::gcreason::Reason reason);

    gcstats::ZoneGCStats scanZonesBeforeGC();
    void collect(bool nonincrementalByAPI, SliceBudget budget, JS::gcreason::Reason reason) JS_HAZ_GC_CALL;
    MOZ_MUST_USE IncrementalResult gcCycle(bool nonincrementalByAPI, SliceBudget& budget,
                                           JS::gcreason::Reason reason);
    bool shouldRepeatForDeadZone(JS::gcreason::Reason reason);
    void incrementalCollectSlice(SliceBudget& budget, JS::gcreason::Reason reason,
                                 AutoTraceSession& session);

    friend class AutoCallGCCallbacks;
    void maybeCallGCCallback(JSGCStatus status);

    void changeToNonIncrementalGC();
    void pushZealSelectedObjects();
    void purgeRuntime();
    MOZ_MUST_USE bool beginMarkPhase(JS::gcreason::Reason reason, AutoTraceSession& session);
    bool prepareZonesForCollection(JS::gcreason::Reason reason, bool* isFullOut,
                                   AutoLockForExclusiveAccess& lock);
    bool shouldPreserveJITCode(JS::Realm* realm, int64_t currentTime,
                               JS::gcreason::Reason reason, bool canAllocateMoreCode);
    void traceRuntimeForMajorGC(JSTracer* trc, AutoTraceSession& session);
    void traceRuntimeAtoms(JSTracer* trc, AutoLockForExclusiveAccess& lock);
    void traceRuntimeCommon(JSTracer* trc, TraceOrMarkRuntime traceOrMark,
                            AutoTraceSession& session);
    void maybeDoCycleCollection();
    void markCompartments();
    IncrementalProgress drainMarkStack(SliceBudget& sliceBudget, gcstats::PhaseKind phase);
    template <class CompartmentIterT> void markWeakReferences(gcstats::PhaseKind phase);
    void markWeakReferencesInCurrentGroup(gcstats::PhaseKind phase);
    template <class ZoneIterT, class CompartmentIterT> void markGrayReferences(gcstats::PhaseKind phase);
    void markBufferedGrayRoots(JS::Zone* zone);
    void markGrayReferencesInCurrentGroup(gcstats::PhaseKind phase);
    void markAllWeakReferences(gcstats::PhaseKind phase);
    void markAllGrayReferences(gcstats::PhaseKind phase);

    void beginSweepPhase(JS::gcreason::Reason reason, AutoTraceSession& session);
    void groupZonesForSweeping(JS::gcreason::Reason reason);
    MOZ_MUST_USE bool findInterZoneEdges();
    void getNextSweepGroup();
    IncrementalProgress endMarkingSweepGroup(FreeOp* fop, SliceBudget& budget);
    IncrementalProgress beginSweepingSweepGroup(FreeOp* fop, SliceBudget& budget);
#ifdef JS_GC_ZEAL
    IncrementalProgress maybeYieldForSweepingZeal(FreeOp* fop, SliceBudget& budget);
#endif
    bool shouldReleaseObservedTypes();
    void sweepDebuggerOnMainThread(FreeOp* fop);
    void sweepJitDataOnMainThread(FreeOp* fop);
    IncrementalProgress endSweepingSweepGroup(FreeOp* fop, SliceBudget& budget);
    IncrementalProgress performSweepActions(SliceBudget& sliceBudget);
    IncrementalProgress sweepTypeInformation(FreeOp* fop, SliceBudget& budget, Zone* zone);
    IncrementalProgress releaseSweptEmptyArenas(FreeOp* fop, SliceBudget& budget, Zone* zone);
    void startSweepingAtomsTable();
    IncrementalProgress sweepAtomsTable(FreeOp* fop, SliceBudget& budget);
    IncrementalProgress sweepWeakCaches(FreeOp* fop, SliceBudget& budget);
    IncrementalProgress finalizeAllocKind(FreeOp* fop, SliceBudget& budget, Zone* zone,
                                          AllocKind kind);
    IncrementalProgress sweepShapeTree(FreeOp* fop, SliceBudget& budget, Zone* zone);
    void endSweepPhase(bool lastGC);
    bool allCCVisibleZonesWereCollected() const;
    void sweepZones(FreeOp* fop, bool destroyingRuntime);
    void decommitAllWithoutUnlocking(const AutoLockGC& lock);
    void startDecommit();
    void queueZonesForBackgroundSweep(ZoneList& zones);
    void sweepBackgroundThings(ZoneList& zones, LifoAlloc& freeBlocks);
    void assertBackgroundSweepingFinished();
    bool shouldCompact();
    void beginCompactPhase();
    IncrementalProgress compactPhase(JS::gcreason::Reason reason, SliceBudget& sliceBudget,
                                     AutoTraceSession& session);
    void endCompactPhase();
    void sweepTypesAfterCompacting(Zone* zone);
    void sweepZoneAfterCompacting(Zone* zone);
    MOZ_MUST_USE bool relocateArenas(Zone* zone, JS::gcreason::Reason reason,
                                     Arena*& relocatedListOut, SliceBudget& sliceBudget);
    void updateTypeDescrObjects(MovingTracer* trc, Zone* zone);
    void updateCellPointers(Zone* zone, AllocKinds kinds, size_t bgTaskCount);
    void updateAllCellPointers(MovingTracer* trc, Zone* zone);
    void updateZonePointersToRelocatedCells(Zone* zone);
    void updateRuntimePointersToRelocatedCells(AutoTraceSession& session);
    void protectAndHoldArenas(Arena* arenaList);
    void unprotectHeldRelocatedArenas();
    void releaseRelocatedArenas(Arena* arenaList);
    void releaseRelocatedArenasWithoutUnlocking(Arena* arenaList, const AutoLockGC& lock);
    void finishCollection();

    void computeNonIncrementalMarkingForValidation(AutoTraceSession& session);
    void validateIncrementalMarking();
    void finishMarkingValidation();

#ifdef DEBUG
    void checkForCompartmentMismatches();
#endif

    void callFinalizeCallbacks(FreeOp* fop, JSFinalizeStatus status) const;
    void callWeakPointerZonesCallbacks() const;
    void callWeakPointerCompartmentCallbacks(JSCompartment* comp) const;

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

    /* Track heap usage for this runtime. */
    HeapUsage usage;

    /* GC scheduling state and parameters. */
    GCSchedulingTunables tunables;
    GCSchedulingState schedulingState;

    // State used for managing atom mark bitmaps in each zone. Protected by the
    // exclusive access lock.
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
    mozilla::Atomic<uint64_t, mozilla::ReleaseAcquire> nextCellUniqueId_;

    /*
     * Number of the committed arenas in all GC chunks including empty chunks.
     */
    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> numArenasFreeCommitted;
    MainThreadData<VerifyPreTracer*> verifyPreData;

  private:
    UnprotectedData<bool> chunkAllocationSinceLastGC;
    MainThreadData<int64_t> lastGCTime;

    /*
     * JSGC_MODE
     * prefs: javascript.options.mem.gc_per_zone and
     *   javascript.options.mem.gc_incremental.
     */
    MainThreadData<JSGCMode> mode;

    mozilla::Atomic<size_t, mozilla::ReleaseAcquire> numActiveZoneIters;

    /* During shutdown, the GC needs to clean up every possible object. */
    MainThreadData<bool> cleanUpEverything;

    // Gray marking must be done after all black marking is complete. However,
    // we do not have write barriers on XPConnect roots. Therefore, XPConnect
    // roots must be accumulated in the first slice of incremental GC. We
    // accumulate these roots in each zone's gcGrayRoots vector and then mark
    // them later, after black marking is complete for each compartment. This
    // accumulation can fail, but in that case we switch to non-incremental GC.
    enum class GrayBufferState {
        Unused,
        Okay,
        Failed
    };
    MainThreadOrGCTaskData<GrayBufferState> grayBufferState;
    bool hasValidGrayRootsBuffer() const { return grayBufferState == GrayBufferState::Okay; }

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

    mozilla::Atomic<JS::gcreason::Reason, mozilla::Relaxed> majorGCTriggerReason;

  private:
    /* Perform full GC if rt->keepAtoms() becomes false. */
    MainThreadData<bool> fullGCForAtomsRequested_;

    /* Incremented at the start of every minor GC. */
    MainThreadData<uint64_t> minorGCNumber;

    /* Incremented at the start of every major GC. */
    MainThreadData<uint64_t> majorGCNumber;

    /* The major GC number at which to release observed type information. */
    MainThreadData<uint64_t> jitReleaseNumber;

    /* Incremented on every GC slice. */
    MainThreadData<uint64_t> number;

    /* Whether the currently running GC can finish in multiple slices. */
    MainThreadData<bool> isIncremental;

    /* Whether all zones are being collected in first GC slice. */
    MainThreadData<bool> isFull;

    /* Whether the heap will be compacted at the end of GC. */
    MainThreadData<bool> isCompacting;

    /* The invocation kind of the current GC, taken from the first slice. */
    MainThreadData<JSGCInvocationKind> invocationKind;

    /* The initial GC reason, taken from the first slice. */
    MainThreadData<JS::gcreason::Reason> initialReason;

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

    /* Whether it's currently safe to yield to the mutator in an incremental GC. */
    MainThreadData<bool> safeToYield;

    /* Whether any sweeping will take place in the separate GC helper thread. */
    MainThreadData<bool> sweepOnBackgroundThread;

    /* Whether observed type information is being released in the current GC. */
    MainThreadData<bool> releaseObservedTypes;

    /* Singly linked list of zones to be swept in the background. */
    MainThreadOrGCTaskData<ZoneList> backgroundSweepZones;

    /*
     * Free LIFO blocks are transferred to this allocator before being freed on
     * the background GC thread after sweeping.
     */
    MainThreadOrGCTaskData<LifoAlloc> blocksToFreeAfterSweeping;

  private:
    /* Index of current sweep group (for stats). */
    MainThreadData<unsigned> sweepGroupIndex;

    /*
     * Incremental sweep state.
     */

    MainThreadData<JS::Zone*> sweepGroups;
    MainThreadOrGCTaskData<JS::Zone*> currentSweepGroup;
    MainThreadData<UniquePtr<SweepAction<GCRuntime*, FreeOp*, SliceBudget&>>> sweepActions;
    MainThreadOrGCTaskData<JS::Zone*> sweepZone;
    MainThreadData<mozilla::Maybe<AtomSet::Enum>> maybeAtomsToSweep;
    MainThreadOrGCTaskData<JS::detail::WeakCacheBase*> sweepCache;
    MainThreadData<bool> abortSweepAfterCurrentGroup;

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
     * JSGC_SLICE_TIME_BUDGET
     * pref: javascript.options.mem.gc_incremental_slice_ms,
     */
    MainThreadData<int64_t> defaultTimeBudget_;

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
    CallbackVector<JSWeakPointerCompartmentCallback> updateWeakPointerCompartmentCallbacks;

    MemoryCounter mallocCounter;

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

#ifdef DEBUG
    MainThreadData<bool> arenasEmptyAtShutdown;
#endif

    /* Synchronize GC heap access among GC helper threads and the main thread. */
    friend class js::AutoLockGC;
    friend class js::AutoLockGCBgAlloc;
    js::Mutex lock;

    BackgroundAllocTask allocTask;
    BackgroundDecommitTask decommitTask;

    js::GCHelperState helperState;

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

    // Free LIFO blocks are transferred to this allocator before being freed
    // after minor GC.
    MainThreadData<LifoAlloc> blocksToFreeAfterMinorGC;

    const void* addressOfNurseryPosition() {
        return nursery_.refNoCheck().addressOfPosition();
    }
    const void* addressOfNurseryCurrentEnd() {
        return nursery_.refNoCheck().addressOfCurrentEnd();
    }
    const void* addressOfStringNurseryCurrentEnd() {
        return nursery_.refNoCheck().addressOfCurrentStringEnd();
    }

    void minorGC(JS::gcreason::Reason reason,
                 gcstats::PhaseKind phase = gcstats::PhaseKind::MINOR_GC) JS_HAZ_GC_CALL;
    void evictNursery(JS::gcreason::Reason reason = JS::gcreason::EVICT_NURSERY) {
        minorGC(reason, gcstats::PhaseKind::EVICT_NURSERY);
    }
    void freeAllLifoBlocksAfterMinorGC(LifoAlloc* lifo);

    friend class js::GCHelperState;
    friend class MarkingValidator;
    friend class AutoTraceSession;
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

inline bool
GCRuntime::hasZealMode(ZealMode mode)
{
    static_assert(size_t(ZealMode::Limit) < sizeof(zealModeBits) * 8,
                  "Zeal modes must fit in zealModeBits");
    return zealModeBits & (1 << uint32_t(mode));
}

inline void
GCRuntime::clearZealMode(ZealMode mode)
{
    zealModeBits &= ~(1 << uint32_t(mode));
    MOZ_ASSERT(!hasZealMode(mode));
}

inline bool
GCRuntime::upcomingZealousGC() {
    return nextScheduled == 1;
}

inline bool
GCRuntime::needZealousGC() {
    if (nextScheduled > 0 && --nextScheduled == 0) {
        if (hasZealMode(ZealMode::Alloc) ||
            hasZealMode(ZealMode::GenerationalGC) ||
            hasZealMode(ZealMode::IncrementalMultipleSlices) ||
            hasZealMode(ZealMode::Compact) ||
            hasIncrementalTwoSliceZealMode())
        {
            nextScheduled = zealFrequency;
        }
        return true;
    }
    return false;
}

inline bool
GCRuntime::hasIncrementalTwoSliceZealMode() {
    return hasZealMode(ZealMode::YieldBeforeMarking) ||
           hasZealMode(ZealMode::YieldBeforeSweeping) ||
           hasZealMode(ZealMode::YieldBeforeSweepingAtoms) ||
           hasZealMode(ZealMode::YieldBeforeSweepingCaches) ||
           hasZealMode(ZealMode::YieldBeforeSweepingTypes) ||
           hasZealMode(ZealMode::YieldBeforeSweepingObjects) ||
           hasZealMode(ZealMode::YieldBeforeSweepingNonObjects) ||
           hasZealMode(ZealMode::YieldBeforeSweepingShapeTrees);
}

#else
inline bool GCRuntime::hasZealMode(ZealMode mode) { return false; }
inline void GCRuntime::clearZealMode(ZealMode mode) { }
inline bool GCRuntime::upcomingZealousGC() { return false; }
inline bool GCRuntime::needZealousGC() { return false; }
inline bool GCRuntime::hasIncrementalTwoSliceZealMode() { return false; }
#endif

} /* namespace gc */
} /* namespace js */

#endif
