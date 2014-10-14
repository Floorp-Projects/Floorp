/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_GCRuntime_h
#define gc_GCRuntime_h

#include "jsgc.h"

#include "gc/Heap.h"
#ifdef JSGC_GENERATIONAL
# include "gc/Nursery.h"
#endif
#include "gc/Statistics.h"
#ifdef JSGC_GENERATIONAL
# include "gc/StoreBuffer.h"
#endif
#include "gc/Tracer.h"

/* Perform validation of incremental marking in debug builds but not on B2G. */
#if defined(DEBUG) && !defined(MOZ_B2G)
#define JS_GC_MARKING_VALIDATION
#endif

namespace js {

namespace gc {

typedef Vector<JS::Zone *, 4, SystemAllocPolicy> ZoneVector;

struct FinalizePhase;
class MarkingValidator;
struct AutoPrepareForTracing;
class AutoTraceSession;

/*
 * Encapsulates all of the GC tunables. These are effectively constant and
 * should only be modified by setParameter.
 */
class GCSchedulingTunables
{
    /*
     * Soft limit on the number of bytes we are allowed to allocate in the GC
     * heap. Attempts to allocate gcthings over this limit will return null and
     * subsequently invoke the standard OOM machinery, independent of available
     * physical memory.
     */
    size_t gcMaxBytes_;

    /*
     * The base value used to compute zone->trigger.gcBytes(). When
     * usage.gcBytes() surpasses threshold.gcBytes() for a zone, the zone may
     * be scheduled for a GC, depending on the exact circumstances.
     */
    size_t gcZoneAllocThresholdBase_;

    /* Fraction of threshold.gcBytes() which triggers an incremental GC. */
    double zoneAllocThresholdFactor_;

    /*
     * Number of bytes to allocate between incremental slices in GCs triggered
     * by the zone allocation threshold.
     */
    size_t zoneAllocDelayBytes_;

    /*
     * Totally disables |highFrequencyGC|, the HeapGrowthFactor, and other
     * tunables that make GC non-deterministic.
     */
    bool dynamicHeapGrowthEnabled_;

    /*
     * We enter high-frequency mode if we GC a twice within this many
     * microseconds. This value is stored directly in microseconds.
     */
    uint64_t highFrequencyThresholdUsec_;

    /*
     * When in the |highFrequencyGC| mode, these parameterize the per-zone
     * "HeapGrowthFactor" computation.
     */
    uint64_t highFrequencyLowLimitBytes_;
    uint64_t highFrequencyHighLimitBytes_;
    double highFrequencyHeapGrowthMax_;
    double highFrequencyHeapGrowthMin_;

    /*
     * When not in |highFrequencyGC| mode, this is the global (stored per-zone)
     * "HeapGrowthFactor".
     */
    double lowFrequencyHeapGrowth_;

    /*
     * Doubles the length of IGC slices when in the |highFrequencyGC| mode.
     */
    bool dynamicMarkSliceEnabled_;

    /*
     * Controls the number of empty chunks reserved for future allocation.
     */
    unsigned minEmptyChunkCount_;
    unsigned maxEmptyChunkCount_;

  public:
    GCSchedulingTunables()
      : gcMaxBytes_(0),
        gcZoneAllocThresholdBase_(30 * 1024 * 1024),
        zoneAllocThresholdFactor_(0.9),
        zoneAllocDelayBytes_(1024 * 1024),
        dynamicHeapGrowthEnabled_(false),
        highFrequencyThresholdUsec_(1000 * 1000),
        highFrequencyLowLimitBytes_(100 * 1024 * 1024),
        highFrequencyHighLimitBytes_(500 * 1024 * 1024),
        highFrequencyHeapGrowthMax_(3.0),
        highFrequencyHeapGrowthMin_(1.5),
        lowFrequencyHeapGrowth_(1.5),
        dynamicMarkSliceEnabled_(false),
        minEmptyChunkCount_(1),
        maxEmptyChunkCount_(30)
    {}

    size_t gcMaxBytes() const { return gcMaxBytes_; }
    size_t gcZoneAllocThresholdBase() const { return gcZoneAllocThresholdBase_; }
    double zoneAllocThresholdFactor() const { return zoneAllocThresholdFactor_; }
    size_t zoneAllocDelayBytes() const { return zoneAllocDelayBytes_; }
    bool isDynamicHeapGrowthEnabled() const { return dynamicHeapGrowthEnabled_; }
    uint64_t highFrequencyThresholdUsec() const { return highFrequencyThresholdUsec_; }
    uint64_t highFrequencyLowLimitBytes() const { return highFrequencyLowLimitBytes_; }
    uint64_t highFrequencyHighLimitBytes() const { return highFrequencyHighLimitBytes_; }
    double highFrequencyHeapGrowthMax() const { return highFrequencyHeapGrowthMax_; }
    double highFrequencyHeapGrowthMin() const { return highFrequencyHeapGrowthMin_; }
    double lowFrequencyHeapGrowth() const { return lowFrequencyHeapGrowth_; }
    bool isDynamicMarkSliceEnabled() const { return dynamicMarkSliceEnabled_; }
    unsigned minEmptyChunkCount() const { return minEmptyChunkCount_; }
    unsigned maxEmptyChunkCount() const { return maxEmptyChunkCount_; }

    void setParameter(JSGCParamKey key, uint32_t value);
};

/*
 * Internal values that effect GC scheduling that are not directly exposed
 * in the GC API.
 */
class GCSchedulingState
{
    /*
     * Influences how we schedule and run GC's in several subtle ways. The most
     * important factor is in how it controls the "HeapGrowthFactor". The
     * growth factor is a measure of how large (as a percentage of the last GC)
     * the heap is allowed to grow before we try to schedule another GC.
     */
    bool inHighFrequencyGCMode_;

  public:
    GCSchedulingState()
      : inHighFrequencyGCMode_(false)
    {}

    bool inHighFrequencyGCMode() const { return inHighFrequencyGCMode_; }

    void updateHighFrequencyMode(uint64_t lastGCTime, uint64_t currentTime,
                                 const GCSchedulingTunables &tunables) {
        inHighFrequencyGCMode_ =
            tunables.isDynamicHeapGrowthEnabled() && lastGCTime &&
            lastGCTime + tunables.highFrequencyThresholdUsec() > currentTime;
    }
};

template<typename F>
struct Callback {
    F op;
    void *data;

    Callback()
      : op(nullptr), data(nullptr)
    {}
    Callback(F op, void *data)
      : op(op), data(data)
    {}
};

template<typename F>
class CallbackVector : public Vector<Callback<F>, 4, SystemAllocPolicy> {};

template <typename T, typename Iter0, typename Iter1>
class ChainedIter
{
    Iter0 iter0_;
    Iter1 iter1_;

  public:
    ChainedIter(const Iter0 &iter0, const Iter1 &iter1)
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

class GCRuntime
{
  public:
    explicit GCRuntime(JSRuntime *rt);
    ~GCRuntime();
    bool init(uint32_t maxbytes, uint32_t maxNurseryBytes);

    inline int zeal();
    inline bool upcomingZealousGC();
    inline bool needZealousGC();

    template <typename T> bool addRoot(T *rp, const char *name, JSGCRootType rootType);
    void removeRoot(void *rp);
    void setMarkStackLimit(size_t limit);

    void setParameter(JSGCParamKey key, uint32_t value);
    uint32_t getParameter(JSGCParamKey key);

    bool isHeapBusy() { return heapState != js::Idle; }
    bool isHeapMajorCollecting() { return heapState == js::MajorCollecting; }
    bool isHeapMinorCollecting() { return heapState == js::MinorCollecting; }
    bool isHeapCollecting() { return isHeapMajorCollecting() || isHeapMinorCollecting(); }
#ifdef JSGC_COMPACTING
    bool isHeapCompacting() { return isHeapMajorCollecting() && state() == COMPACT; }
#else
    bool isHeapCompacting() { return false; }
#endif

    // Performance note: if isFJMinorCollecting turns out to be slow because
    // reading the counter is slow then we may be able to augment the counter
    // with a volatile flag that is set iff the counter is greater than
    // zero. (It will require some care to make sure the two variables stay in
    // sync.)
    bool isFJMinorCollecting() { return fjCollectionCounter > 0; }
    void incFJMinorCollecting() { fjCollectionCounter++; }
    void decFJMinorCollecting() { fjCollectionCounter--; }

    bool triggerGC(JS::gcreason::Reason reason);
    bool triggerZoneGC(Zone *zone, JS::gcreason::Reason reason);
    bool maybeGC(Zone *zone);
    void maybePeriodicFullGC();
    void minorGC(JS::gcreason::Reason reason);
    void minorGC(JSContext *cx, JS::gcreason::Reason reason);
    void evictNursery(JS::gcreason::Reason reason = JS::gcreason::EVICT_NURSERY) { minorGC(reason); }
    void gcIfNeeded(JSContext *cx);
    void gc(JSGCInvocationKind gckind, JS::gcreason::Reason reason);
    void gcSlice(JSGCInvocationKind gckind, JS::gcreason::Reason reason, int64_t millis = 0);
    void gcFinalSlice(JSGCInvocationKind gckind, JS::gcreason::Reason reason);
    void gcDebugSlice(bool limit, int64_t objCount);

    void runDebugGC();
    inline void poke();

    enum TraceOrMarkRuntime {
        TraceRuntime,
        MarkRuntime
    };
    enum TraceRootsOrUsedSaved {
        TraceRoots,
        UseSavedRoots
    };
    void markRuntime(JSTracer *trc,
                     TraceOrMarkRuntime traceOrMark = TraceRuntime,
                     TraceRootsOrUsedSaved rootsSource = TraceRoots);

    void notifyDidPaint();
    void shrinkBuffers();

#ifdef JS_GC_ZEAL
    const void *addressOfZealMode() { return &zealMode; }
    void setZeal(uint8_t zeal, uint32_t frequency);
    void setNextScheduled(uint32_t count);
    void verifyPreBarriers();
    void verifyPostBarriers();
    void maybeVerifyPreBarriers(bool always);
    void maybeVerifyPostBarriers(bool always);
    bool selectForMarking(JSObject *object);
    void clearSelectedForMarking();
    void setDeterministic(bool enable);
#endif

    size_t maxMallocBytesAllocated() { return maxMallocBytes; }

  public:
    // Internal public interface
    js::gc::State state() { return incrementalState; }
    bool isBackgroundSweeping() { return helperState.isBackgroundSweeping(); }
    void waitBackgroundSweepEnd() { helperState.waitBackgroundSweepEnd(); }
    void waitBackgroundSweepOrAllocEnd() { helperState.waitBackgroundSweepOrAllocEnd(); }
    void startBackgroundAllocationIfIdle() { helperState.startBackgroundAllocationIfIdle(); }

#ifdef DEBUG

    bool onBackgroundThread() { return helperState.onBackgroundThread(); }

    bool currentThreadOwnsGCLock() {
        return lockOwner == PR_GetCurrentThread();
    }

#endif // DEBUG

    void assertCanLock() {
        MOZ_ASSERT(!currentThreadOwnsGCLock());
    }

    void lockGC() {
        PR_Lock(lock);
        MOZ_ASSERT(!lockOwner);
#ifdef DEBUG
        lockOwner = PR_GetCurrentThread();
#endif
    }

    void unlockGC() {
        MOZ_ASSERT(lockOwner == PR_GetCurrentThread());
        lockOwner = nullptr;
        PR_Unlock(lock);
    }

#ifdef DEBUG
    bool isAllocAllowed() { return noGCOrAllocationCheck == 0; }
    void disallowAlloc() { ++noGCOrAllocationCheck; }
    void allowAlloc() {
        MOZ_ASSERT(!isAllocAllowed());
        --noGCOrAllocationCheck;
    }

    bool isInsideUnsafeRegion() { return inUnsafeRegion != 0; }
    void enterUnsafeRegion() { ++inUnsafeRegion; }
    void leaveUnsafeRegion() {
        MOZ_ASSERT(inUnsafeRegion > 0);
        --inUnsafeRegion;
    }

    bool isStrictProxyCheckingEnabled() { return disableStrictProxyCheckingCount == 0; }
    void disableStrictProxyChecking() { ++disableStrictProxyCheckingCount; }
    void enableStrictProxyChecking() {
        MOZ_ASSERT(disableStrictProxyCheckingCount > 0);
        --disableStrictProxyCheckingCount;
    }
#endif

    void setAlwaysPreserveCode() { alwaysPreserveCode = true; }

    bool isIncrementalGCAllowed() { return incrementalAllowed; }
    void disallowIncrementalGC() { incrementalAllowed = false; }

    bool isIncrementalGCEnabled() { return mode == JSGC_MODE_INCREMENTAL && incrementalAllowed; }
    bool isIncrementalGCInProgress() { return state() != gc::NO_INCREMENTAL && !verifyPreData; }

    bool isGenerationalGCEnabled() { return generationalDisabled == 0; }
    void disableGenerationalGC();
    void enableGenerationalGC();

#ifdef JSGC_COMPACTING
    void disableCompactingGC();
    void enableCompactingGC();
#endif

    void setGrayRootsTracer(JSTraceDataOp traceOp, void *data);
    bool addBlackRootsTracer(JSTraceDataOp traceOp, void *data);
    void removeBlackRootsTracer(JSTraceDataOp traceOp, void *data);

    void setMaxMallocBytes(size_t value);
    void resetMallocBytes();
    bool isTooMuchMalloc() const { return mallocBytes <= 0; }
    void updateMallocCounter(JS::Zone *zone, size_t nbytes);
    void onTooMuchMalloc();

    void setGCCallback(JSGCCallback callback, void *data);
    bool addFinalizeCallback(JSFinalizeCallback callback, void *data);
    void removeFinalizeCallback(JSFinalizeCallback func);
    bool addWeakPointerCallback(JSWeakPointerCallback callback, void *data);
    void removeWeakPointerCallback(JSWeakPointerCallback func);
    JS::GCSliceCallback setSliceCallback(JS::GCSliceCallback callback);

    void setValidate(bool enable);
    void setFullCompartmentChecks(bool enable);

    bool isManipulatingDeadZones() { return manipulatingDeadZones; }
    void setManipulatingDeadZones(bool value) { manipulatingDeadZones = value; }
    unsigned objectsMarkedInDeadZonesCount() { return objectsMarkedInDeadZones; }
    void incObjectsMarkedInDeadZone() {
        MOZ_ASSERT(manipulatingDeadZones);
        ++objectsMarkedInDeadZones;
    }

    JS::Zone *getCurrentZoneGroup() { return currentZoneGroup; }
    void setFoundBlackGrayEdges() { foundBlackGrayEdges = true; }

    uint64_t gcNumber() { return number; }
    void incGcNumber() { ++number; }

    bool isIncrementalGc() { return isIncremental; }
    bool isFullGc() { return isFull; }

    bool shouldCleanUpEverything() { return cleanUpEverything; }

    bool areGrayBitsValid() { return grayBitsValid; }
    void setGrayBitsInvalid() { grayBitsValid = false; }

    bool isGcNeeded() { return isNeeded; }

    double computeHeapGrowthFactor(size_t lastBytes);
    size_t computeTriggerBytes(double growthFactor, size_t lastBytes);

    JSGCMode gcMode() const { return mode; }
    void setGCMode(JSGCMode m) {
        mode = m;
        marker.setGCMode(mode);
    }

    inline void updateOnFreeArenaAlloc(const ChunkInfo &info);
    inline void updateOnArenaFree(const ChunkInfo &info);

    ChunkPool &emptyChunks() { return emptyChunks_; }
    ChunkPool &availableChunks() { return availableChunks_; }
    ChunkPool &fullChunks() { return fullChunks_; }
    typedef ChainedIter<Chunk *, ChunkPool::Iter, ChunkPool::Iter> NonEmptyChunksIter;
    NonEmptyChunksIter allNonEmptyChunks() {
        return NonEmptyChunksIter(ChunkPool::Iter(availableChunks_), ChunkPool::Iter(fullChunks_));
    }

#ifdef JS_GC_ZEAL
    void startVerifyPreBarriers();
    bool endVerifyPreBarriers();
    void startVerifyPostBarriers();
    bool endVerifyPostBarriers();
    void finishVerifier();
    bool isVerifyPreBarriersEnabled() const { return !!verifyPreData; }
#else
    bool isVerifyPreBarriersEnabled() const { return false; }
#endif

    template <AllowGC allowGC>
    static void *refillFreeListFromAnyThread(ThreadSafeContext *cx, AllocKind thingKind);
    static void *refillFreeListInGC(Zone *zone, AllocKind thingKind);

  private:
    // For ArenaLists::allocateFromArena()
    friend class ArenaLists;
    Chunk *pickChunk(AutoMaybeStartBackgroundAllocation &maybeStartBGAlloc);
    inline void arenaAllocatedDuringGC(JS::Zone *zone, ArenaHeader *arena);

    template <AllowGC allowGC>
    static void *refillFreeListFromMainThread(JSContext *cx, AllocKind thingKind);
    static void *refillFreeListOffMainThread(ExclusiveContext *cx, AllocKind thingKind);
    static void *refillFreeListPJS(ForkJoinContext *cx, AllocKind thingKind);

    // Return the list of chunks that can be released outside the GC lock.
    ChunkPool expireEmptyChunks(bool shrinkBuffers, const AutoLockGC &lock);
    void freeChunks(ChunkPool pool);
    void prepareToFreeChunk(ChunkInfo &info);

    inline bool wantBackgroundAllocation() const;

    bool initZeal();
    void requestInterrupt(JS::gcreason::Reason reason);
    void collect(bool incremental, int64_t budget, JSGCInvocationKind gckind,
                 JS::gcreason::Reason reason);
    bool gcCycle(bool incremental, int64_t budget, JSGCInvocationKind gckind,
                 JS::gcreason::Reason reason);
    gcstats::ZoneGCStats scanZonesBeforeGC();
    void budgetIncrementalGC(int64_t *budget);
    void resetIncrementalGC(const char *reason);
    void incrementalCollectSlice(int64_t budget, JS::gcreason::Reason reason);
    void pushZealSelectedObjects();
    bool beginMarkPhase(JS::gcreason::Reason reason);
    bool shouldPreserveJITCode(JSCompartment *comp, int64_t currentTime,
                               JS::gcreason::Reason reason);
    void bufferGrayRoots();
    bool drainMarkStack(SliceBudget &sliceBudget, gcstats::Phase phase);
    template <class CompartmentIterT> void markWeakReferences(gcstats::Phase phase);
    void markWeakReferencesInCurrentGroup(gcstats::Phase phase);
    template <class ZoneIterT, class CompartmentIterT> void markGrayReferences(gcstats::Phase phase);
    void markGrayReferencesInCurrentGroup(gcstats::Phase phase);
    void markAllWeakReferences(gcstats::Phase phase);
    void markAllGrayReferences(gcstats::Phase phase);

    void beginSweepPhase(bool lastGC);
    void findZoneGroups();
    bool findZoneEdgesForWeakMaps();
    void getNextZoneGroup();
    void endMarkingZoneGroup();
    void beginSweepingZoneGroup();
    bool shouldReleaseObservedTypes();
    void endSweepingZoneGroup();
    bool sweepPhase(SliceBudget &sliceBudget);
    void endSweepPhase(bool lastGC);
    void sweepZones(FreeOp *fop, bool lastGC);
    void decommitArenas(const AutoLockGC &lock);
    void expireChunksAndArenas(bool shouldShrink, const AutoLockGC &lock);
    void sweepBackgroundThings();
    void assertBackgroundSweepingFinished();
    bool shouldCompact();
#ifdef JSGC_COMPACTING
    void sweepZoneAfterCompacting(Zone *zone);
    void compactPhase();
    ArenaHeader *relocateArenas();
    void updatePointersToRelocatedCells();
    void releaseRelocatedArenas(ArenaHeader *relocatedList);
#endif
    void finishCollection();

    void computeNonIncrementalMarkingForValidation();
    void validateIncrementalMarking();
    void finishMarkingValidation();

    void markConservativeStackRoots(JSTracer *trc, bool useSavedRoots);

#ifdef DEBUG
    void checkForCompartmentMismatches();
#endif

    void callFinalizeCallbacks(FreeOp *fop, JSFinalizeStatus status) const;
    void callWeakPointerCallbacks() const;

  public:
    JSRuntime             *rt;

    /* Embedders can use this zone however they wish. */
    JS::Zone              *systemZone;

    /* List of compartments and zones (protected by the GC lock). */
    js::gc::ZoneVector    zones;

#ifdef JSGC_GENERATIONAL
    js::Nursery           nursery;
    js::gc::StoreBuffer   storeBuffer;
#endif

    js::gcstats::Statistics stats;

    js::GCMarker          marker;

    /* Track heap usage for this runtime. */
    HeapUsage usage;

    /* GC scheduling state and parameters. */
    GCSchedulingTunables  tunables;
    GCSchedulingState     schedulingState;

  private:
    // Chunks may be empty, available (partially allocated), or fully utilized.

    // When empty, chunks reside in the emptyChunks pool and are re-used as
    // needed or eventually expired if not re-used. The emptyChunks pool gets
    // refilled from the background allocation task heuristically so that empty
    // chunks should always available for immediate allocation without syscalls.
    ChunkPool             emptyChunks_;

    // Chunks which have had some, but not all, of their arenas allocated live
    // in the available chunk lists. When all available arenas in a chunk have
    // been allocated, the chunk is removed from the available list and moved
    // to the fullChunks pool. During a GC, if all arenas are free, the chunk
    // is moved back to the emptyChunks pool and scheduled for eventual
    // release.
    ChunkPool             availableChunks_;

    // When all arenas in a chunk are used, it is moved to the fullChunks pool
    // so as to reduce the cost of operations on the available lists.
    ChunkPool             fullChunks_;

    js::RootedValueMap    rootsHash;

    size_t                maxMallocBytes;

    /*
     * Number of the committed arenas in all GC chunks including empty chunks.
     */
    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire>   numArenasFreeCommitted;
    void                  *verifyPreData;
    void                  *verifyPostData;
    bool                  chunkAllocationSinceLastGC;
    int64_t               nextFullGCTime;
    int64_t               lastGCTime;

    JSGCMode              mode;

    uint64_t              decommitThreshold;

    /* During shutdown, the GC needs to clean up every possible object. */
    bool                  cleanUpEverything;

    /*
     * The gray bits can become invalid if UnmarkGray overflows the stack. A
     * full GC will reset this bit, since it fills in all the gray bits.
     */
    bool                  grayBitsValid;

    /*
     * These flags must be kept separate so that a thread requesting a
     * compartment GC doesn't cancel another thread's concurrent request for a
     * full GC.
     */
    volatile uintptr_t    isNeeded;

    /* Incremented at the start of every major GC. */
    uint64_t              majorGCNumber;

    /* The major GC number at which to release observed type information. */
    uint64_t              jitReleaseNumber;

    /* Incremented on every GC slice. */
    uint64_t              number;

    /* The number at the time of the most recent GC's first slice. */
    uint64_t              startNumber;

    /* Whether the currently running GC can finish in multiple slices. */
    bool                  isIncremental;

    /* Whether all compartments are being collected in first GC slice. */
    bool                  isFull;

    /* The invocation kind of the current GC, taken from the first slice. */
    JSGCInvocationKind    invocationKind;

    /* The reason that an interrupt-triggered GC should be called. */
    JS::gcreason::Reason  triggerReason;

    /*
     * If this is 0, all cross-compartment proxies must be registered in the
     * wrapper map. This checking must be disabled temporarily while creating
     * new wrappers. When non-zero, this records the recursion depth of wrapper
     * creation.
     */
    mozilla::DebugOnly<uintptr_t>  disableStrictProxyCheckingCount;

    /*
     * The current incremental GC phase. This is also used internally in
     * non-incremental GC.
     */
    js::gc::State         incrementalState;

    /* Indicates that the last incremental slice exhausted the mark stack. */
    bool                  lastMarkSlice;

    /* Whether any sweeping will take place in the separate GC helper thread. */
    bool                  sweepOnBackgroundThread;

    /* Whether observed type information is being released in the current GC. */
    bool                  releaseObservedTypes;

    /* Whether any black->gray edges were found during marking. */
    bool                  foundBlackGrayEdges;

    /* List head of zones to be swept in the background. */
    JS::Zone              *sweepingZones;

    /* Index of current zone group (for stats). */
    unsigned              zoneGroupIndex;

    /*
     * Incremental sweep state.
     */
    JS::Zone              *zoneGroups;
    JS::Zone              *currentZoneGroup;
    bool                  sweepingTypes;
    unsigned              finalizePhase;
    JS::Zone              *sweepZone;
    unsigned              sweepKindIndex;
    bool                  abortSweepAfterCurrentGroup;

    /*
     * Concurrent sweep infrastructure.
     */
    void startTask(GCParallelTask &task, gcstats::Phase phase);
    void joinTask(GCParallelTask &task, gcstats::Phase phase);

    /*
     * List head of arenas allocated during the sweep phase.
     */
    js::gc::ArenaHeader   *arenasAllocatedDuringSweep;

#ifdef JS_GC_MARKING_VALIDATION
    js::gc::MarkingValidator *markingValidator;
#endif

    /*
     * Indicates that a GC slice has taken place in the middle of an animation
     * frame, rather than at the beginning. In this case, the next slice will be
     * delayed so that we don't get back-to-back slices.
     */
    volatile uintptr_t    interFrameGC;

    /* Default budget for incremental GC slice. See SliceBudget in jsgc.h. */
    int64_t               sliceBudget;

    /*
     * We disable incremental GC if we encounter a js::Class with a trace hook
     * that does not implement write barriers.
     */
    bool                  incrementalAllowed;

    /*
     * GGC can be enabled from the command line while testing.
     */
    unsigned              generationalDisabled;

#ifdef JSGC_COMPACTING
    /*
     * Some code cannot tolerate compacting GC so it can be disabled with this
     * counter.  This can happen from code executing in a ThreadSafeContext so
     * we make it atomic.
     */
    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> compactingDisabled;
#endif

    /*
     * This is true if we are in the middle of a brain transplant (e.g.,
     * JS_TransplantObject) or some other operation that can manipulate
     * dead zones.
     */
    bool                  manipulatingDeadZones;

    /*
     * This field is incremented each time we mark an object inside a
     * zone with no incoming cross-compartment pointers. Typically if
     * this happens it signals that an incremental GC is marking too much
     * stuff. At various times we check this counter and, if it has changed, we
     * run an immediate, non-incremental GC to clean up the dead
     * zones. This should happen very rarely.
     */
    unsigned              objectsMarkedInDeadZones;

    bool                  poked;

    volatile js::HeapState heapState;

    /*
     * ForkJoin workers enter and leave GC independently; this counter
     * tracks the number that are currently in GC.
     *
     * Technically this should be #ifdef JSGC_FJGENERATIONAL but that
     * affects the observed size of JSRuntime in problematic ways, see
     * note in vm/ThreadPool.h.
     */
    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> fjCollectionCounter;

    /*
     * These options control the zealousness of the GC. The fundamental values
     * are nextScheduled and gcDebugCompartmentGC. At every allocation,
     * nextScheduled is decremented. When it reaches zero, we do either a full
     * or a compartmental GC, based on debugCompartmentGC.
     *
     * At this point, if zeal_ is one of the types that trigger periodic
     * collection, then nextScheduled is reset to the value of zealFrequency.
     * Otherwise, no additional GCs take place.
     *
     * You can control these values in several ways:
     *   - Set the JS_GC_ZEAL environment variable
     *   - Call zeal() or schedulegc() from inside shell-executed JS code
     *     (see the help for details)
     *
     * If gzZeal_ == 1 then we perform GCs in select places (during MaybeGC and
     * whenever a GC poke happens). This option is mainly useful to embedders.
     *
     * We use zeal_ == 4 to enable write barrier verification. See the comment
     * in jsgc.cpp for more information about this.
     *
     * zeal_ values from 8 to 10 periodically run different types of
     * incremental GC.
     *
     * zeal_ value 14 performs periodic shrinking collections.
     */
#ifdef JS_GC_ZEAL
    int                   zealMode;
    int                   zealFrequency;
    int                   nextScheduled;
    bool                  deterministicOnly;
    int                   incrementalLimit;

    js::Vector<JSObject *, 0, js::SystemAllocPolicy>   selectedForMarking;
#endif

    bool                  validate;
    bool                  fullCompartmentChecks;

    Callback<JSGCCallback>  gcCallback;
    CallbackVector<JSFinalizeCallback> finalizeCallbacks;
    CallbackVector<JSWeakPointerCallback> updateWeakPointerCallbacks;

    /*
     * Malloc counter to measure memory pressure for GC scheduling. It runs
     * from   maxMallocBytes down to zero.
     */
    mozilla::Atomic<ptrdiff_t, mozilla::ReleaseAcquire>   mallocBytes;

    /*
     * Whether a GC has been triggered as a result of mallocBytes falling
     * below zero.
     */
    mozilla::Atomic<bool, mozilla::ReleaseAcquire>   mallocGCTriggered;

    /*
     * The trace operations to trace embedding-specific GC roots. One is for
     * tracing through black roots and the other is for tracing through gray
     * roots. The black/gray distinction is only relevant to the cycle
     * collector.
     */
    CallbackVector<JSTraceDataOp> blackRootTracers;
    Callback<JSTraceDataOp> grayRootTracer;

#ifdef DEBUG
    /*
     * Some regions of code are hard for the static rooting hazard analysis to
     * understand. In those cases, we trade the static analysis for a dynamic
     * analysis. When this is non-zero, we should assert if we trigger, or
     * might trigger, a GC.
     */
    int inUnsafeRegion;
#endif

    /* Always preserve JIT code during GCs, for testing. */
    bool                  alwaysPreserveCode;

#ifdef DEBUG
    size_t                noGCOrAllocationCheck;
#endif

    /* Synchronize GC heap access between main thread and GCHelperState. */
    PRLock                *lock;
    mozilla::DebugOnly<PRThread *>   lockOwner;

    GCHelperState helperState;

    /*
     * During incremental sweeping, this field temporarily holds the arenas of
     * the current AllocKind being swept in order of increasing free space.
     */
    SortedArenaList incrementalSweepList;

    friend class js::GCHelperState;
    friend class js::gc::MarkingValidator;
    friend class js::gc::AutoTraceSession;
};

#ifdef JS_GC_ZEAL
inline int
GCRuntime::zeal() {
    return zealMode;
}

inline bool
GCRuntime::upcomingZealousGC() {
    return nextScheduled == 1;
}

inline bool
GCRuntime::needZealousGC() {
    if (nextScheduled > 0 && --nextScheduled == 0) {
        if (zealMode == ZealAllocValue ||
            zealMode == ZealGenerationalGCValue ||
            (zealMode >= ZealIncrementalRootsThenFinish &&
             zealMode <= ZealIncrementalMultipleSlices) ||
            zealMode == ZealCompactValue)
        {
            nextScheduled = zealFrequency;
        }
        return true;
    }
    return false;
}
#else
inline int GCRuntime::zeal() { return 0; }
inline bool GCRuntime::upcomingZealousGC() { return false; }
inline bool GCRuntime::needZealousGC() { return false; }
#endif

} /* namespace gc */
} /* namespace js */

#endif
