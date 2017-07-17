/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_GCAPI_h
#define js_GCAPI_h

#include "mozilla/TimeStamp.h"
#include "mozilla/Vector.h"

#include "js/GCAnnotations.h"
#include "js/HeapAPI.h"
#include "js/UniquePtr.h"
#include "js/Utility.h"

namespace js {
namespace gc {
class GCRuntime;
} // namespace gc
namespace gcstats {
struct Statistics;
} // namespace gcstats
} // namespace js

typedef enum JSGCMode {
    /** Perform only global GCs. */
    JSGC_MODE_GLOBAL = 0,

    /** Perform per-zone GCs until too much garbage has accumulated. */
    JSGC_MODE_ZONE = 1,

    /**
     * Collect in short time slices rather than all at once. Implies
     * JSGC_MODE_ZONE.
     */
    JSGC_MODE_INCREMENTAL = 2
} JSGCMode;

/**
 * Kinds of js_GC invocation.
 */
typedef enum JSGCInvocationKind {
    /* Normal invocation. */
    GC_NORMAL = 0,

    /* Minimize GC triggers and release empty GC chunks right away. */
    GC_SHRINK = 1
} JSGCInvocationKind;

namespace JS {

#define GCREASONS(D)                            \
    /* Reasons internal to the JS engine */     \
    D(API)                                      \
    D(EAGER_ALLOC_TRIGGER)                      \
    D(DESTROY_RUNTIME)                          \
    D(ROOTS_REMOVED)                            \
    D(LAST_DITCH)                               \
    D(TOO_MUCH_MALLOC)                          \
    D(ALLOC_TRIGGER)                            \
    D(DEBUG_GC)                                 \
    D(COMPARTMENT_REVIVED)                      \
    D(RESET)                                    \
    D(OUT_OF_NURSERY)                           \
    D(EVICT_NURSERY)                            \
    D(UNUSED0)                                  \
    D(SHARED_MEMORY_LIMIT)                      \
    D(UNUSED1)                                  \
    D(INCREMENTAL_TOO_SLOW)                     \
    D(ABORT_GC)                                 \
    D(FULL_WHOLE_CELL_BUFFER)                   \
    D(FULL_GENERIC_BUFFER)                      \
    D(FULL_VALUE_BUFFER)                        \
    D(FULL_CELL_PTR_BUFFER)                     \
    D(FULL_SLOT_BUFFER)                         \
                                                \
    /* These are reserved for future use. */    \
    D(RESERVED0)                                \
    D(RESERVED1)                                \
    D(RESERVED2)                                \
    D(RESERVED3)                                \
    D(RESERVED4)                                \
    D(RESERVED5)                                \
    D(RESERVED6)                                \
    D(RESERVED7)                                \
    D(RESERVED8)                                \
    D(RESERVED9)                                \
    D(RESERVED10)                               \
                                                \
    /* Reasons from Firefox */                  \
    D(DOM_WINDOW_UTILS)                         \
    D(COMPONENT_UTILS)                          \
    D(MEM_PRESSURE)                             \
    D(CC_WAITING)                               \
    D(CC_FORCED)                                \
    D(LOAD_END)                                 \
    D(POST_COMPARTMENT)                         \
    D(PAGE_HIDE)                                \
    D(NSJSCONTEXT_DESTROY)                      \
    D(SET_NEW_DOCUMENT)                         \
    D(SET_DOC_SHELL)                            \
    D(DOM_UTILS)                                \
    D(DOM_IPC)                                  \
    D(DOM_WORKER)                               \
    D(INTER_SLICE_GC)                           \
    D(REFRESH_FRAME)                            \
    D(FULL_GC_TIMER)                            \
    D(SHUTDOWN_CC)                              \
    D(UNUSED2)                                  \
    D(USER_INACTIVE)                            \
    D(XPCONNECT_SHUTDOWN)

namespace gcreason {

/* GCReasons will end up looking like JSGC_MAYBEGC */
enum Reason {
#define MAKE_REASON(name) name,
    GCREASONS(MAKE_REASON)
#undef MAKE_REASON
    NO_REASON,
    NUM_REASONS,

    /*
     * For telemetry, we want to keep a fixed max bucket size over time so we
     * don't have to switch histograms. 100 is conservative; as of this writing
     * there are 52. But the cost of extra buckets seems to be low while the
     * cost of switching histograms is high.
     */
    NUM_TELEMETRY_REASONS = 100
};

/**
 * Get a statically allocated C string explaining the given GC reason.
 */
extern JS_PUBLIC_API(const char*)
ExplainReason(JS::gcreason::Reason reason);

} /* namespace gcreason */

/*
 * Zone GC:
 *
 * SpiderMonkey's GC is capable of performing a collection on an arbitrary
 * subset of the zones in the system. This allows an embedding to minimize
 * collection time by only collecting zones that have run code recently,
 * ignoring the parts of the heap that are unlikely to have changed.
 *
 * When triggering a GC using one of the functions below, it is first necessary
 * to select the zones to be collected. To do this, you can call
 * PrepareZoneForGC on each zone, or you can call PrepareForFullGC to select
 * all zones. Failing to select any zone is an error.
 */

/**
 * Schedule the given zone to be collected as part of the next GC.
 */
extern JS_PUBLIC_API(void)
PrepareZoneForGC(Zone* zone);

/**
 * Schedule all zones to be collected in the next GC.
 */
extern JS_PUBLIC_API(void)
PrepareForFullGC(JSContext* cx);

/**
 * When performing an incremental GC, the zones that were selected for the
 * previous incremental slice must be selected in subsequent slices as well.
 * This function selects those slices automatically.
 */
extern JS_PUBLIC_API(void)
PrepareForIncrementalGC(JSContext* cx);

/**
 * Returns true if any zone in the system has been scheduled for GC with one of
 * the functions above or by the JS engine.
 */
extern JS_PUBLIC_API(bool)
IsGCScheduled(JSContext* cx);

/**
 * Undoes the effect of the Prepare methods above. The given zone will not be
 * collected in the next GC.
 */
extern JS_PUBLIC_API(void)
SkipZoneForGC(Zone* zone);

/*
 * Non-Incremental GC:
 *
 * The following functions perform a non-incremental GC.
 */

/**
 * Performs a non-incremental collection of all selected zones.
 *
 * If the gckind argument is GC_NORMAL, then some objects that are unreachable
 * from the program may still be alive afterwards because of internal
 * references; if GC_SHRINK is passed then caches and other temporary references
 * to objects will be cleared and all unreferenced objects will be removed from
 * the system.
 */
extern JS_PUBLIC_API(void)
GCForReason(JSContext* cx, JSGCInvocationKind gckind, gcreason::Reason reason);

/*
 * Incremental GC:
 *
 * Incremental GC divides the full mark-and-sweep collection into multiple
 * slices, allowing client JavaScript code to run between each slice. This
 * allows interactive apps to avoid long collection pauses. Incremental GC does
 * not make collection take less time, it merely spreads that time out so that
 * the pauses are less noticable.
 *
 * For a collection to be carried out incrementally the following conditions
 * must be met:
 *  - The collection must be run by calling JS::IncrementalGC() rather than
 *    JS_GC().
 *  - The GC mode must have been set to JSGC_MODE_INCREMENTAL with
 *    JS_SetGCParameter().
 *
 * Note: Even if incremental GC is enabled and working correctly,
 *       non-incremental collections can still happen when low on memory.
 */

/**
 * Begin an incremental collection and perform one slice worth of work. When
 * this function returns, the collection may not be complete.
 * IncrementalGCSlice() must be called repeatedly until
 * !IsIncrementalGCInProgress(cx).
 *
 * Note: SpiderMonkey's GC is not realtime. Slices in practice may be longer or
 *       shorter than the requested interval.
 */
extern JS_PUBLIC_API(void)
StartIncrementalGC(JSContext* cx, JSGCInvocationKind gckind, gcreason::Reason reason,
                   int64_t millis = 0);

/**
 * Perform a slice of an ongoing incremental collection. When this function
 * returns, the collection may not be complete. It must be called repeatedly
 * until !IsIncrementalGCInProgress(cx).
 *
 * Note: SpiderMonkey's GC is not realtime. Slices in practice may be longer or
 *       shorter than the requested interval.
 */
extern JS_PUBLIC_API(void)
IncrementalGCSlice(JSContext* cx, gcreason::Reason reason, int64_t millis = 0);

/**
 * If IsIncrementalGCInProgress(cx), this call finishes the ongoing collection
 * by performing an arbitrarily long slice. If !IsIncrementalGCInProgress(cx),
 * this is equivalent to GCForReason. When this function returns,
 * IsIncrementalGCInProgress(cx) will always be false.
 */
extern JS_PUBLIC_API(void)
FinishIncrementalGC(JSContext* cx, gcreason::Reason reason);

/**
 * If IsIncrementalGCInProgress(cx), this call aborts the ongoing collection and
 * performs whatever work needs to be done to return the collector to its idle
 * state. This may take an arbitrarily long time. When this function returns,
 * IsIncrementalGCInProgress(cx) will always be false.
 */
extern JS_PUBLIC_API(void)
AbortIncrementalGC(JSContext* cx);

namespace dbg {

// The `JS::dbg::GarbageCollectionEvent` class is essentially a view of the
// `js::gcstats::Statistics` data without the uber implementation-specific bits.
// It should generally be palatable for web developers.
class GarbageCollectionEvent
{
    // The major GC number of the GC cycle this data pertains to.
    uint64_t majorGCNumber_;

    // Reference to a non-owned, statically allocated C string. This is a very
    // short reason explaining why a GC was triggered.
    const char* reason;

    // Reference to a nullable, non-owned, statically allocated C string. If the
    // collection was forced to be non-incremental, this is a short reason of
    // why the GC could not perform an incremental collection.
    const char* nonincrementalReason;

    // Represents a single slice of a possibly multi-slice incremental garbage
    // collection.
    struct Collection {
        mozilla::TimeStamp startTimestamp;
        mozilla::TimeStamp endTimestamp;
    };

    // The set of garbage collection slices that made up this GC cycle.
    mozilla::Vector<Collection> collections;

    GarbageCollectionEvent(const GarbageCollectionEvent& rhs) = delete;
    GarbageCollectionEvent& operator=(const GarbageCollectionEvent& rhs) = delete;

  public:
    explicit GarbageCollectionEvent(uint64_t majorGCNum)
        : majorGCNumber_(majorGCNum)
        , reason(nullptr)
        , nonincrementalReason(nullptr)
        , collections()
    { }

    using Ptr = js::UniquePtr<GarbageCollectionEvent>;
    static Ptr Create(JSRuntime* rt, ::js::gcstats::Statistics& stats, uint64_t majorGCNumber);

    JSObject* toJSObject(JSContext* cx) const;

    uint64_t majorGCNumber() const { return majorGCNumber_; }
};

} // namespace dbg

enum GCProgress {
    /*
     * During GC, the GC is bracketed by GC_CYCLE_BEGIN/END callbacks. Each
     * slice between those (whether an incremental or the sole non-incremental
     * slice) is bracketed by GC_SLICE_BEGIN/GC_SLICE_END.
     */

    GC_CYCLE_BEGIN,
    GC_SLICE_BEGIN,
    GC_SLICE_END,
    GC_CYCLE_END
};

struct JS_PUBLIC_API(GCDescription) {
    bool isZone_;
    bool isComplete_;
    JSGCInvocationKind invocationKind_;
    gcreason::Reason reason_;

    GCDescription(bool isZone, bool isComplete, JSGCInvocationKind kind, gcreason::Reason reason)
      : isZone_(isZone), isComplete_(isComplete), invocationKind_(kind), reason_(reason) {}

    char16_t* formatSliceMessage(JSContext* cx) const;
    char16_t* formatSummaryMessage(JSContext* cx) const;
    char16_t* formatJSON(JSContext* cx, uint64_t timestamp) const;

    mozilla::TimeStamp startTime(JSContext* cx) const;
    mozilla::TimeStamp endTime(JSContext* cx) const;
    mozilla::TimeStamp lastSliceStart(JSContext* cx) const;
    mozilla::TimeStamp lastSliceEnd(JSContext* cx) const;

    JS::UniqueChars sliceToJSON(JSContext* cx) const;
    JS::UniqueChars summaryToJSON(JSContext* cx) const;

    JS::dbg::GarbageCollectionEvent::Ptr toGCEvent(JSContext* cx) const;
};

extern JS_PUBLIC_API(UniqueChars)
MinorGcToJSON(JSContext* cx);

typedef void
(* GCSliceCallback)(JSContext* cx, GCProgress progress, const GCDescription& desc);

/**
 * The GC slice callback is called at the beginning and end of each slice. This
 * callback may be used for GC notifications as well as to perform additional
 * marking.
 */
extern JS_PUBLIC_API(GCSliceCallback)
SetGCSliceCallback(JSContext* cx, GCSliceCallback callback);

/**
 * Describes the progress of an observed nursery collection.
 */
enum class GCNurseryProgress {
    /**
     * The nursery collection is starting.
     */
    GC_NURSERY_COLLECTION_START,
    /**
     * The nursery collection is ending.
     */
    GC_NURSERY_COLLECTION_END
};

/**
 * A nursery collection callback receives the progress of the nursery collection
 * and the reason for the collection.
 */
using GCNurseryCollectionCallback = void(*)(JSContext* cx, GCNurseryProgress progress,
                                            gcreason::Reason reason);

/**
 * Set the nursery collection callback for the given runtime. When set, it will
 * be called at the start and end of every nursery collection.
 */
extern JS_PUBLIC_API(GCNurseryCollectionCallback)
SetGCNurseryCollectionCallback(JSContext* cx, GCNurseryCollectionCallback callback);

typedef void
(* DoCycleCollectionCallback)(JSContext* cx);

/**
 * The purge gray callback is called after any COMPARTMENT_REVIVED GC in which
 * the majority of compartments have been marked gray.
 */
extern JS_PUBLIC_API(DoCycleCollectionCallback)
SetDoCycleCollectionCallback(JSContext* cx, DoCycleCollectionCallback callback);

/**
 * Incremental GC defaults to enabled, but may be disabled for testing or in
 * embeddings that have not yet implemented barriers on their native classes.
 * There is not currently a way to re-enable incremental GC once it has been
 * disabled on the runtime.
 */
extern JS_PUBLIC_API(void)
DisableIncrementalGC(JSContext* cx);

/**
 * Returns true if incremental GC is enabled. Simply having incremental GC
 * enabled is not sufficient to ensure incremental collections are happening.
 * See the comment "Incremental GC" above for reasons why incremental GC may be
 * suppressed. Inspection of the "nonincremental reason" field of the
 * GCDescription returned by GCSliceCallback may help narrow down the cause if
 * collections are not happening incrementally when expected.
 */
extern JS_PUBLIC_API(bool)
IsIncrementalGCEnabled(JSContext* cx);

/**
 * Returns true while an incremental GC is ongoing, both when actively
 * collecting and between slices.
 */
extern JS_PUBLIC_API(bool)
IsIncrementalGCInProgress(JSContext* cx);

/**
 * Returns true while an incremental GC is ongoing, both when actively
 * collecting and between slices.
 */
extern JS_PUBLIC_API(bool)
IsIncrementalGCInProgress(JSRuntime* rt);

/*
 * Returns true when writes to GC thing pointers (and reads from weak pointers)
 * must call an incremental barrier. This is generally only true when running
 * mutator code in-between GC slices. At other times, the barrier may be elided
 * for performance.
 */
extern JS_PUBLIC_API(bool)
IsIncrementalBarrierNeeded(JSContext* cx);

/*
 * Notify the GC that a reference to a JSObject is about to be overwritten.
 * This method must be called if IsIncrementalBarrierNeeded.
 */
extern JS_PUBLIC_API(void)
IncrementalPreWriteBarrier(JSObject* obj);

/*
 * Notify the GC that a weak reference to a GC thing has been read.
 * This method must be called if IsIncrementalBarrierNeeded.
 */
extern JS_PUBLIC_API(void)
IncrementalReadBarrier(GCCellPtr thing);

/**
 * Returns true if the most recent GC ran incrementally.
 */
extern JS_PUBLIC_API(bool)
WasIncrementalGC(JSRuntime* rt);

/*
 * Generational GC:
 *
 * Note: Generational GC is not yet enabled by default. The following class
 *       is non-functional unless SpiderMonkey was configured with
 *       --enable-gcgenerational.
 */

/** Ensure that generational GC is disabled within some scope. */
class JS_PUBLIC_API(AutoDisableGenerationalGC)
{
    JSContext* cx;

  public:
    explicit AutoDisableGenerationalGC(JSContext* cx);
    ~AutoDisableGenerationalGC();
};

/**
 * Returns true if generational allocation and collection is currently enabled
 * on the given runtime.
 */
extern JS_PUBLIC_API(bool)
IsGenerationalGCEnabled(JSRuntime* rt);

/**
 * Returns the GC's "number". This does not correspond directly to the number
 * of GCs that have been run, but is guaranteed to be monotonically increasing
 * with GC activity.
 */
extern JS_PUBLIC_API(size_t)
GetGCNumber();

/**
 * Pass a subclass of this "abstract" class to callees to require that they
 * never GC. Subclasses can use assertions or the hazard analysis to ensure no
 * GC happens.
 */
class JS_PUBLIC_API(AutoRequireNoGC)
{
  protected:
    AutoRequireNoGC() {}
    ~AutoRequireNoGC() {}
};

/**
 * Diagnostic assert (see MOZ_DIAGNOSTIC_ASSERT) that GC cannot occur while this
 * class is live. This class does not disable the static rooting hazard
 * analysis.
 *
 * This works by entering a GC unsafe region, which is checked on allocation and
 * on GC.
 */
class JS_PUBLIC_API(AutoAssertNoGC) : public AutoRequireNoGC
{
    JSContext* cx_;

  public:
    explicit AutoAssertNoGC(JSContext* cx = nullptr);
    ~AutoAssertNoGC();
};

/**
 * Assert if an allocation of a GC thing occurs while this class is live. This
 * class does not disable the static rooting hazard analysis.
 */
class JS_PUBLIC_API(AutoAssertNoAlloc)
{
#ifdef JS_DEBUG
    js::gc::GCRuntime* gc;

  public:
    AutoAssertNoAlloc() : gc(nullptr) {}
    explicit AutoAssertNoAlloc(JSContext* cx);
    void disallowAlloc(JSRuntime* rt);
    ~AutoAssertNoAlloc();
#else
  public:
    AutoAssertNoAlloc() {}
    explicit AutoAssertNoAlloc(JSContext* cx) {}
    void disallowAlloc(JSRuntime* rt) {}
#endif
};

/**
 * Disable the static rooting hazard analysis in the live region and assert if
 * any allocation that could potentially trigger a GC occurs while this guard
 * object is live. This is most useful to help the exact rooting hazard analysis
 * in complex regions, since it cannot understand dataflow.
 *
 * Note: GC behavior is unpredictable even when deterministic and is generally
 *       non-deterministic in practice. The fact that this guard has not
 *       asserted is not a guarantee that a GC cannot happen in the guarded
 *       region. As a rule, anyone performing a GC unsafe action should
 *       understand the GC properties of all code in that region and ensure
 *       that the hazard analysis is correct for that code, rather than relying
 *       on this class.
 */
class JS_PUBLIC_API(AutoSuppressGCAnalysis) : public AutoAssertNoAlloc
{
  public:
    AutoSuppressGCAnalysis() : AutoAssertNoAlloc() {}
    explicit AutoSuppressGCAnalysis(JSContext* cx) : AutoAssertNoAlloc(cx) {}
} JS_HAZ_GC_SUPPRESSED;

/**
 * Assert that code is only ever called from a GC callback, disable the static
 * rooting hazard analysis and assert if any allocation that could potentially
 * trigger a GC occurs while this guard object is live.
 *
 * This is useful to make the static analysis ignore code that runs in GC
 * callbacks.
 */
class JS_PUBLIC_API(AutoAssertGCCallback) : public AutoSuppressGCAnalysis
{
  public:
#ifdef DEBUG
    AutoAssertGCCallback();
#else
    AutoAssertGCCallback() {}
#endif
};

/**
 * Place AutoCheckCannotGC in scopes that you believe can never GC. These
 * annotations will be verified both dynamically via AutoAssertNoGC, and
 * statically with the rooting hazard analysis (implemented by making the
 * analysis consider AutoCheckCannotGC to be a GC pointer, and therefore
 * complain if it is live across a GC call.) It is useful when dealing with
 * internal pointers to GC things where the GC thing itself may not be present
 * for the static analysis: e.g. acquiring inline chars from a JSString* on the
 * heap.
 *
 * We only do the assertion checking in DEBUG builds.
 */
#ifdef DEBUG
class JS_PUBLIC_API(AutoCheckCannotGC) : public AutoAssertNoGC
{
  public:
    explicit AutoCheckCannotGC(JSContext* cx = nullptr) : AutoAssertNoGC(cx) {}
} JS_HAZ_GC_INVALIDATED;
#else
class JS_PUBLIC_API(AutoCheckCannotGC) : public AutoRequireNoGC
{
  public:
    explicit AutoCheckCannotGC(JSContext* cx = nullptr) {}
} JS_HAZ_GC_INVALIDATED;
#endif

/**
 * Unsets the gray bit for anything reachable from |thing|. |kind| should not be
 * JS::TraceKind::Shape. |thing| should be non-null. The return value indicates
 * if anything was unmarked.
 */
extern JS_FRIEND_API(bool)
UnmarkGrayGCThingRecursively(GCCellPtr thing);

} /* namespace JS */

namespace js {
namespace gc {

static MOZ_ALWAYS_INLINE void
ExposeGCThingToActiveJS(JS::GCCellPtr thing)
{
    // GC things residing in the nursery cannot be gray: they have no mark bits.
    // All live objects in the nursery are moved to tenured at the beginning of
    // each GC slice, so the gray marker never sees nursery things.
    if (IsInsideNursery(thing.asCell()))
        return;

    // There's nothing to do for permanent GC things that might be owned by
    // another runtime.
    if (thing.mayBeOwnedByOtherRuntime())
        return;

    if (IsIncrementalBarrierNeededOnTenuredGCThing(thing))
        JS::IncrementalReadBarrier(thing);
    else if (js::gc::detail::TenuredCellIsMarkedGray(thing.asCell()))
        JS::UnmarkGrayGCThingRecursively(thing);

    MOZ_ASSERT(!js::gc::detail::TenuredCellIsMarkedGray(thing.asCell()));
}

template <typename T>
extern JS_PUBLIC_API(bool)
EdgeNeedsSweepUnbarrieredSlow(T* thingp);

static MOZ_ALWAYS_INLINE bool
EdgeNeedsSweepUnbarriered(JSObject** objp)
{
    // This function does not handle updating nursery pointers. Raw JSObject
    // pointers should be updated separately or replaced with
    // JS::Heap<JSObject*> which handles this automatically.
    MOZ_ASSERT(!JS::CurrentThreadIsHeapMinorCollecting());
    if (IsInsideNursery(reinterpret_cast<Cell*>(*objp)))
        return false;

    auto zone = JS::shadow::Zone::asShadowZone(detail::GetGCThingZone(uintptr_t(*objp)));
    if (!zone->isGCSweepingOrCompacting())
        return false;

    return EdgeNeedsSweepUnbarrieredSlow(objp);
}

} /* namespace gc */
} /* namespace js */

namespace JS {

/*
 * This should be called when an object that is marked gray is exposed to the JS
 * engine (by handing it to running JS code or writing it into live JS
 * data). During incremental GC, since the gray bits haven't been computed yet,
 * we conservatively mark the object black.
 */
static MOZ_ALWAYS_INLINE void
ExposeObjectToActiveJS(JSObject* obj)
{
    MOZ_ASSERT(obj);
    MOZ_ASSERT(!js::gc::EdgeNeedsSweepUnbarrieredSlow(&obj));
    js::gc::ExposeGCThingToActiveJS(GCCellPtr(obj));
}

static MOZ_ALWAYS_INLINE void
ExposeScriptToActiveJS(JSScript* script)
{
    MOZ_ASSERT(!js::gc::EdgeNeedsSweepUnbarrieredSlow(&script));
    js::gc::ExposeGCThingToActiveJS(GCCellPtr(script));
}

/*
 * Internal to Firefox.
 */
extern JS_FRIEND_API(void)
NotifyGCRootsRemoved(JSContext* cx);

/*
 * Internal to Firefox.
 */
extern JS_FRIEND_API(void)
NotifyDidPaint(JSContext* cx);

} /* namespace JS */

#endif /* js_GCAPI_h */
