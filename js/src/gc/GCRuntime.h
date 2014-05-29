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

namespace js {

struct ScriptAndCounts
{
    /* This structure is stored and marked from the JSRuntime. */
    JSScript *script;
    ScriptCounts scriptCounts;

    PCCounts &getPCCounts(jsbytecode *pc) const {
        return scriptCounts.pcCountsVector[script->pcToOffset(pc)];
    }

    jit::IonScriptCounts *getIonCounts() const {
        return scriptCounts.ionCounts;
    }
};

typedef Vector<ScriptAndCounts, 0, SystemAllocPolicy> ScriptAndCountsVector;

namespace gc {

typedef Vector<JS::Zone *, 4, SystemAllocPolicy> ZoneVector;

class MarkingValidator;
class AutoPrepareForTracing;

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
    bool init(uint32_t maxbytes);
    void finish();

    void setGCZeal(uint8_t zeal, uint32_t frequency);
    template <typename T> bool addRoot(T *rp, const char *name, JSGCRootType rootType);
    void removeRoot(void *rp);
    void setMarkStackLimit(size_t limit);

    bool isHeapBusy() { return heapState != js::Idle; }
    bool isHeapMajorCollecting() { return heapState == js::MajorCollecting; }
    bool isHeapMinorCollecting() { return heapState == js::MinorCollecting; }
    bool isHeapCollecting() { return isHeapMajorCollecting() || isHeapMinorCollecting(); }

    bool triggerGC(JS::gcreason::Reason reason);
    bool triggerZoneGC(Zone *zone, JS::gcreason::Reason reason);
    void maybeGC(Zone *zone);
    void minorGC(JS::gcreason::Reason reason);
    void minorGC(JSContext *cx, JS::gcreason::Reason reason);
    void gcIfNeeded(JSContext *cx);
    void collect(bool incremental, int64_t budget, JSGCInvocationKind gckind,
                 JS::gcreason::Reason reason);
    void gcSlice(JSGCInvocationKind gckind, JS::gcreason::Reason reason, int64_t millis);
    void runDebugGC();

    void markRuntime(JSTracer *trc, bool useSavedRoots = false);

#ifdef JS_GC_ZEAL
    void verifyPreBarriers();
    void verifyPostBarriers();
    void maybeVerifyPreBarriers(bool always);
    void maybeVerifyPostBarriers(bool always);
#endif

  public:
    // Internal public interface
    void recordNativeStackTop();
#ifdef JS_THREADSAFE
    void notifyRequestEnd() { conservativeGC.updateForRequestEnd(); }
#endif
    bool isBackgroundSweeping() { return helperState.isBackgroundSweeping(); }
    void waitBackgroundSweepEnd() { helperState.waitBackgroundSweepEnd(); }
    void waitBackgroundSweepOrAllocEnd() { helperState.waitBackgroundSweepOrAllocEnd(); }
    void startBackgroundShrink() { helperState.startBackgroundShrink(); }
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
#endif

    void setAlwaysPreserveCode() { alwaysPreserveCode = true; }

    bool isGenerationalGCEnabled() { return generationalDisabled == 0; }
    void disableGenerationalGC();
    void enableGenerationalGC();

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

    inline bool wantBackgroundAllocation() const;

    bool initGCZeal();
    void requestInterrupt(JS::gcreason::Reason reason);
    bool gcCycle(bool incremental, int64_t budget, JSGCInvocationKind gckind,
                 JS::gcreason::Reason reason);
    void budgetIncrementalGC(int64_t *budget);
    void resetIncrementalGC(const char *reason);
    void incrementalCollectSlice(int64_t budget, JS::gcreason::Reason reason,
                                 JSGCInvocationKind gckind);
    void pushZealSelectedObjects();
    bool beginMarkPhase();
    bool shouldPreserveJITCode(JSCompartment *comp, int64_t currentTime);
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

    void computeNonIncrementalMarkingForValidation();
    void validateIncrementalMarking();
    void finishMarkingValidation();

    void markConservativeStackRoots(JSTracer *trc, bool useSavedRoots);

#ifdef DEBUG
    void checkForCompartmentMismatches();
    void markAllWeakReferences(gcstats::Phase phase);
    void markAllGrayReferences();
#endif

  public:  // Internal state, public for now
    JSRuntime             *rt;

    /* Embedders can use this zone however they wish. */
    JS::Zone              *systemZone;

    /* List of compartments and zones (protected by the GC lock). */
    js::gc::ZoneVector    zones;

    js::gc::SystemPageAllocator pageAllocator;

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
    js::GCMarker          marker;
    void                  *verifyPreData;
    void                  *verifyPostData;
    bool                  chunkAllocationSinceLastGC;
    int64_t               nextFullGCTime;
    int64_t               lastGCTime;
    int64_t               jitReleaseTime;

    JSGCMode              mode;

    size_t                allocationThreshold;
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

    /* During shutdown, the GC needs to clean up every possible object. */
    bool                  shouldCleanUpEverything;

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

    js::gcstats::Statistics stats;

    /* Incremented on every GC slice. */
    uint64_t              number;

    /* The   number at the time of the most recent GC's first slice. */
    uint64_t              startNumber;

    /* Whether the currently running GC can finish in multiple slices. */
    bool                  isIncremental;

    /* Whether all compartments are being collected in first GC slice. */
    bool                  isFull;

    /* The reason that an interrupt-triggered GC should be called. */
    JS::gcreason::Reason  triggerReason;

    /*
     * If this is true, all marked objects must belong to a compartment being
     * GCed. This is used to look for compartment bugs.
     */
    bool                  strictCompartmentChecking;

#ifdef DEBUG
    /*
     * If this is 0, all cross-compartment proxies must be registered in the
     * wrapper map. This checking must be disabled temporarily while creating
     * new wrappers. When non-zero, this records the recursion depth of wrapper
     * creation.
     */
    uintptr_t             disableStrictProxyCheckingCount;
#else
    uintptr_t             unused1;
#endif

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

#ifdef DEBUG
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
    bool                  incrementalEnabled;

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

    bool                  poke;

    volatile js::HeapState heapState;

#ifdef JSGC_GENERATIONAL
    js::Nursery           nursery;
    js::gc::StoreBuffer   storeBuffer;
#endif

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

    JSGCCallback          gcCallback;
    void                  *gcCallbackData;

    JS::GCSliceCallback   sliceCallback;
    CallbackVector<JSFinalizeCallback> finalizeCallbacks;

    /*
     * Malloc counter to measure memory pressure for GC scheduling. It runs
     * from   maxMallocBytes down to zero.
     */
    mozilla::Atomic<ptrdiff_t, mozilla::ReleaseAcquire>   mallocBytes;

    /*
     * Whether a GC has been triggered as a result of   mallocBytes falling
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

    /*
     * The GC can only safely decommit memory when the page size of the
     * running process matches the compiled arena size.
     */
    size_t                systemPageSize;

    /* The OS allocation granularity may not match the page size. */
    size_t                systemAllocGranularity;

    /* Strong references on scripts held for PCCount profiling API. */
    js::ScriptAndCountsVector *scriptAndCountsVector;

  private:
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

    //friend class js::gc::Chunk; // todo: remove
    friend class js::GCHelperState;
    friend class js::gc::MarkingValidator;
};

} /* namespace gc */
} /* namespace js */

#endif
