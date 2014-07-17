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

class MarkingValidator;
struct AutoPrepareForTracing;
class AutoTraceSession;

class ChunkPool
{
    Chunk   *emptyChunkListHead;
    size_t  emptyCount;

  public:
    ChunkPool()
      : emptyChunkListHead(nullptr),
        emptyCount(0)
    {}

    size_t getEmptyCount() const {
        return emptyCount;
    }

    /* Must be called with the GC lock taken. */
    inline Chunk *get(JSRuntime *rt);

    /* Must be called either during the GC or with the GC lock taken. */
    inline void put(Chunk *chunk);

    class Enum {
      public:
        Enum(ChunkPool &pool) : pool(pool), chunkp(&pool.emptyChunkListHead) {}
        bool empty() { return !*chunkp; }
        Chunk *front();
        inline void popFront();
        inline void removeAndPopFront();
      private:
        ChunkPool &pool;
        Chunk **chunkp;
    };
};

struct ConservativeGCData
{
    /*
     * The GC scans conservatively between ThreadData::nativeStackBase and
     * nativeStackTop unless the latter is nullptr.
     */
    uintptr_t           *nativeStackTop;

    union {
        jmp_buf         jmpbuf;
        uintptr_t       words[JS_HOWMANY(sizeof(jmp_buf), sizeof(uintptr_t))];
    } registerSnapshot;

    ConservativeGCData() {
        mozilla::PodZero(this);
    }

    ~ConservativeGCData() {
#ifdef JS_THREADSAFE
        /*
         * The conservative GC scanner should be disabled when the thread leaves
         * the last request.
         */
        JS_ASSERT(!hasStackToScan());
#endif
    }

    MOZ_NEVER_INLINE void recordStackTop();

#ifdef JS_THREADSAFE
    void updateForRequestEnd() {
        nativeStackTop = nullptr;
    }
#endif

    bool hasStackToScan() const {
        return !!nativeStackTop;
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

class GCRuntime
{
  public:
    explicit GCRuntime(JSRuntime *rt);
    bool init(uint32_t maxbytes, uint32_t maxNurseryBytes);
    void finish();

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
    void maybeGC(Zone *zone);
    void minorGC(JS::gcreason::Reason reason);
    void minorGC(JSContext *cx, JS::gcreason::Reason reason);
    void gcIfNeeded(JSContext *cx);
    void collect(bool incremental, int64_t budget, JSGCInvocationKind gckind,
                 JS::gcreason::Reason reason);
    void gcSlice(JSGCInvocationKind gckind, JS::gcreason::Reason reason, int64_t millis = 0);
    void runDebugGC();
    inline void poke();

    void markRuntime(JSTracer *trc, bool useSavedRoots = false);

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

    size_t bytesAllocated() { return bytes; }
    size_t maxBytesAllocated() { return maxBytes; }
    size_t maxMallocBytesAllocated() { return maxBytes; }

  public:
    // Internal public interface
    js::gc::State state() { return incrementalState; }
    void recordNativeStackTop();
#ifdef JS_THREADSAFE
    void notifyRequestEnd() { conservativeGC.updateForRequestEnd(); }
#endif
    bool isBackgroundSweeping() { return helperState.isBackgroundSweeping(); }
    void waitBackgroundSweepEnd() { helperState.waitBackgroundSweepEnd(); }
    void waitBackgroundSweepOrAllocEnd() { helperState.waitBackgroundSweepOrAllocEnd(); }
    void startBackgroundAllocationIfIdle() { helperState.startBackgroundAllocationIfIdle(); }
    void freeLater(void *p) { helperState.freeLater(p); }

#ifdef DEBUG

    bool onBackgroundThread() { return helperState.onBackgroundThread(); }

    bool currentThreadOwnsGCLock() {
#ifdef JS_THREADSAFE
        return lockOwner == PR_GetCurrentThread();
#else
        return true;
#endif
    }

#endif // DEBUG

#ifdef JS_THREADSAFE
    void assertCanLock() {
        JS_ASSERT(!currentThreadOwnsGCLock());
    }
#endif

    void lockGC() {
#ifdef JS_THREADSAFE
        PR_Lock(lock);
        JS_ASSERT(!lockOwner);
#ifdef DEBUG
        lockOwner = PR_GetCurrentThread();
#endif
#endif
    }

    void unlockGC() {
#ifdef JS_THREADSAFE
        JS_ASSERT(lockOwner == PR_GetCurrentThread());
        lockOwner = nullptr;
        PR_Unlock(lock);
#endif
    }

#ifdef DEBUG
    bool isAllocAllowed() { return noGCOrAllocationCheck == 0; }
    void disallowAlloc() { ++noGCOrAllocationCheck; }
    void allowAlloc() {
        JS_ASSERT(!isAllocAllowed());
        --noGCOrAllocationCheck;
    }

    bool isInsideUnsafeRegion() { return inUnsafeRegion != 0; }
    void enterUnsafeRegion() { ++inUnsafeRegion; }
    void leaveUnsafeRegion() {
        JS_ASSERT(inUnsafeRegion > 0);
        --inUnsafeRegion;
    }

    bool isStrictProxyCheckingEnabled() { return disableStrictProxyCheckingCount == 0; }
    void disableStrictProxyChecking() { ++disableStrictProxyCheckingCount; }
    void enableStrictProxyChecking() {
        JS_ASSERT(disableStrictProxyCheckingCount > 0);
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
    JS::GCSliceCallback setSliceCallback(JS::GCSliceCallback callback);

    void setValidate(bool enable);
    void setFullCompartmentChecks(bool enable);

    bool isManipulatingDeadZones() { return manipulatingDeadZones; }
    void setManipulatingDeadZones(bool value) { manipulatingDeadZones = value; }
    unsigned objectsMarkedInDeadZonesCount() { return objectsMarkedInDeadZones; }
    void incObjectsMarkedInDeadZone() {
        JS_ASSERT(manipulatingDeadZones);
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
    size_t computeTriggerBytes(double growthFactor, size_t lastBytes, JSGCInvocationKind gckind);
    size_t allocationThreshold() { return allocThreshold; }

    JSGCMode gcMode() const { return mode; }
    void setGCMode(JSGCMode m) {
        mode = m;
        marker.setGCMode(mode);
    }

    inline void updateOnFreeArenaAlloc(const ChunkInfo &info);
    inline void updateOnArenaFree(const ChunkInfo &info);
    inline void updateBytesAllocated(ptrdiff_t size);

    GCChunkSet::Range allChunks() { return chunkSet.all(); }
    inline Chunk **getAvailableChunkList(Zone *zone);
    void moveChunkToFreePool(Chunk *chunk);
    bool hasChunk(Chunk *chunk) { return chunkSet.has(chunk); }

#ifdef JS_GC_ZEAL
    void startVerifyPreBarriers();
    bool endVerifyPreBarriers();
    void startVerifyPostBarriers();
    bool endVerifyPostBarriers();
    void finishVerifier();
#endif

  private:
    // For ArenaLists::allocateFromArenaInline()
    friend class ArenaLists;
    Chunk *pickChunk(Zone *zone, AutoMaybeStartBackgroundAllocation &maybeStartBackgroundAllocation);
    inline void arenaAllocatedDuringGC(JS::Zone *zone, ArenaHeader *arena);

    /*
     * Return the list of chunks that can be released outside the GC lock.
     * Must be called either during the GC or with the GC lock taken.
     */
    Chunk *expireChunkPool(bool shrinkBuffers, bool releaseAll);
    void expireAndFreeChunkPool(bool releaseAll);
    void freeChunkList(Chunk *chunkListHead);
    void prepareToFreeChunk(ChunkInfo &info);
    void releaseChunk(Chunk *chunk);

    inline bool wantBackgroundAllocation() const;

    bool initZeal();
    void requestInterrupt(JS::gcreason::Reason reason);
    bool gcCycle(bool incremental, int64_t budget, JSGCInvocationKind gckind,
                 JS::gcreason::Reason reason);
    gcstats::ZoneGCStats scanZonesBeforeGC();
    void budgetIncrementalGC(int64_t *budget);
    void resetIncrementalGC(const char *reason);
    void incrementalCollectSlice(int64_t budget, JS::gcreason::Reason reason,
                                 JSGCInvocationKind gckind);
    void pushZealSelectedObjects();
    bool beginMarkPhase(JS::gcreason::Reason reason);
    bool shouldPreserveJITCode(JSCompartment *comp, int64_t currentTime,
                               JS::gcreason::Reason reason);
    void bufferGrayRoots();
    bool drainMarkStack(SliceBudget &sliceBudget, gcstats::Phase phase);
    template <class CompartmentIterT> void markWeakReferences(gcstats::Phase phase);
    void markWeakReferencesInCurrentGroup(gcstats::Phase phase);
    template <class ZoneIterT, class CompartmentIterT> void markGrayReferences();
    void markGrayReferencesInCurrentGroup();
    void beginSweepPhase(bool lastGC);
    void findZoneGroups();
    bool findZoneEdgesForWeakMaps();
    void getNextZoneGroup();
    void endMarkingZoneGroup();
    void beginSweepingZoneGroup();
    bool releaseObservedTypes();
    void endSweepingZoneGroup();
    bool sweepPhase(SliceBudget &sliceBudget);
    void endSweepPhase(JSGCInvocationKind gckind, bool lastGC);
    void sweepZones(FreeOp *fop, bool lastGC);
    void decommitArenasFromAvailableList(Chunk **availableListHeadp);
    void decommitArenas();
    void expireChunksAndArenas(bool shouldShrink);
    void sweepBackgroundThings(bool onBackgroundThread);
    void assertBackgroundSweepingFinished();

    void computeNonIncrementalMarkingForValidation();
    void validateIncrementalMarking();
    void finishMarkingValidation();

    void markConservativeStackRoots(JSTracer *trc, bool useSavedRoots);

#ifdef DEBUG
    void checkForCompartmentMismatches();
    void markAllWeakReferences(gcstats::Phase phase);
    void markAllGrayReferences();
#endif

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

  private:
    /*
     * Set of all GC chunks with at least one allocated thing. The
     * conservative GC uses it to quickly check if a possible GC thing points
     * into an allocated chunk.
     */
    js::GCChunkSet        chunkSet;

    /*
     * Doubly-linked lists of chunks from user and system compartments. The GC
     * allocates its arenas from the corresponding list and when all arenas
     * in the list head are taken, then the chunk is removed from the list.
     * During the GC when all arenas in a chunk become free, that chunk is
     * removed from the list and scheduled for release.
     */
    js::gc::Chunk         *systemAvailableChunkListHead;
    js::gc::Chunk         *userAvailableChunkListHead;
    js::gc::ChunkPool     chunkPool;

    js::RootedValueMap    rootsHash;

    /* This is updated by both the main and GC helper threads. */
    mozilla::Atomic<size_t, mozilla::ReleaseAcquire>   bytes;

    size_t                maxBytes;
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
    int64_t               jitReleaseTime;

    JSGCMode              mode;

    size_t                allocThreshold;
    bool                  highFrequencyGC;
    uint64_t              highFrequencyTimeThreshold;
    uint64_t              highFrequencyLowLimitBytes;
    uint64_t              highFrequencyHighLimitBytes;
    double                highFrequencyHeapGrowthMax;
    double                highFrequencyHeapGrowthMin;
    double                lowFrequencyHeapGrowth;
    bool                  dynamicHeapGrowth;
    bool                  dynamicMarkSlice;
    uint64_t              decommitThreshold;
    unsigned              minEmptyChunkCount;
    unsigned              maxEmptyChunkCount;

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

    /* Incremented on every GC slice. */
    uint64_t              number;

    /* The number at the time of the most recent GC's first slice. */
    uint64_t              startNumber;

    /* Whether the currently running GC can finish in multiple slices. */
    bool                  isIncremental;

    /* Whether all compartments are being collected in first GC slice. */
    bool                  isFull;

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
    int                   finalizePhase;
    JS::Zone              *sweepZone;
    int                   sweepKindIndex;
    bool                  abortSweepAfterCurrentGroup;

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
     * are   nextScheduled and gcDebugCompartmentGC. At every allocation,
     *   nextScheduled is decremented. When it reaches zero, we do either a
     * full or a compartmental GC, based on   debugCompartmentGC.
     *
     * At this point, if   zeal_ is one of the types that trigger periodic
     * collection, then   nextScheduled is reset to the value of
     *   zealFrequency. Otherwise, no additional GCs take place.
     *
     * You can control these values in several ways:
     *   - Pass the -Z flag to the shell (see the usage info for details)
     *   - Call   zeal() or schedulegc() from inside shell-executed JS code
     *     (see the help for details)
     *
     * If gzZeal_ == 1 then we perform GCs in select places (during MaybeGC and
     * whenever a GC poke happens). This option is mainly useful to embedders.
     *
     * We use   zeal_ == 4 to enable write barrier verification. See the comment
     * in jsgc.cpp for more information about this.
     *
     *   zeal_ values from 8 to 10 periodically run different types of
     * incremental GC.
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

    ConservativeGCData conservativeGC;

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
             zealMode <= ZealIncrementalMultipleSlices))
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
