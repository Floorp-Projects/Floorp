/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_GCAPI_h
#define js_GCAPI_h

#include "mozilla/NullPtr.h"

#include "js/HeapAPI.h"
#include "js/RootingAPI.h"
#include "js/Value.h"

namespace JS {

#define GCREASONS(D)                            \
    /* Reasons internal to the JS engine */     \
    D(API)                                      \
    D(MAYBEGC)                                  \
    D(DESTROY_RUNTIME)                          \
    D(DESTROY_CONTEXT)                          \
    D(LAST_DITCH)                               \
    D(TOO_MUCH_MALLOC)                          \
    D(ALLOC_TRIGGER)                            \
    D(DEBUG_GC)                                 \
    D(TRANSPLANT)                               \
    D(RESET)                                    \
    D(OUT_OF_NURSERY)                           \
    D(EVICT_NURSERY)                            \
    D(FULL_STORE_BUFFER)                        \
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
    D(RESERVED11)                               \
    D(RESERVED12)                               \
    D(RESERVED13)                               \
    D(RESERVED14)                               \
    D(RESERVED15)                               \
    D(RESERVED16)                               \
    D(RESERVED17)                               \
    D(RESERVED18)                               \
    D(RESERVED19)                               \
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
    D(FINISH_LARGE_EVALUTE)

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
     * there are 26. But the cost of extra buckets seems to be low while the
     * cost of switching histograms is high.
     */
    NUM_TELEMETRY_REASONS = 100
};

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

/*
 * Schedule the given zone to be collected as part of the next GC.
 */
extern JS_FRIEND_API(void)
PrepareZoneForGC(Zone *zone);

/*
 * Schedule all zones to be collected in the next GC.
 */
extern JS_FRIEND_API(void)
PrepareForFullGC(JSRuntime *rt);

/*
 * When performing an incremental GC, the zones that were selected for the
 * previous incremental slice must be selected in subsequent slices as well.
 * This function selects those slices automatically.
 */
extern JS_FRIEND_API(void)
PrepareForIncrementalGC(JSRuntime *rt);

/*
 * Returns true if any zone in the system has been scheduled for GC with one of
 * the functions above or by the JS engine.
 */
extern JS_FRIEND_API(bool)
IsGCScheduled(JSRuntime *rt);

/*
 * Undoes the effect of the Prepare methods above. The given zone will not be
 * collected in the next GC.
 */
extern JS_FRIEND_API(void)
SkipZoneForGC(Zone *zone);

/*
 * Non-Incremental GC:
 *
 * The following functions perform a non-incremental GC.
 */

/*
 * Performs a non-incremental collection of all selected zones. Some objects
 * that are unreachable from the program may still be alive afterwards because
 * of internal references.
 */
extern JS_FRIEND_API(void)
GCForReason(JSRuntime *rt, gcreason::Reason reason);

/*
 * Perform a non-incremental collection after clearing caches and other
 * temporary references to objects. This will remove all unreferenced objects
 * in the system.
 */
extern JS_FRIEND_API(void)
ShrinkingGC(JSRuntime *rt, gcreason::Reason reason);

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
 *  - All native objects that have their own trace hook must indicate that they
 *    implement read and write barriers with the JSCLASS_IMPLEMENTS_BARRIERS
 *    flag.
 *
 * Note: Even if incremental GC is enabled and working correctly,
 *       non-incremental collections can still happen when low on memory.
 */

/*
 * Begin an incremental collection and perform one slice worth of work or
 * perform a slice of an ongoing incremental collection. When this function
 * returns, the collection is not complete. This function must be called
 * repeatedly until !IsIncrementalGCInProgress(rt).
 *
 * Note: SpiderMonkey's GC is not realtime. Slices in practice may be longer or
 *       shorter than the requested interval.
 */
extern JS_FRIEND_API(void)
IncrementalGC(JSRuntime *rt, gcreason::Reason reason, int64_t millis = 0);

/*
 * If IsIncrementalGCInProgress(rt), this call finishes the ongoing collection
 * by performing an arbitrarily long slice. If !IsIncrementalGCInProgress(rt),
 * this is equivalent to GCForReason. When this function returns,
 * IsIncrementalGCInProgress(rt) will always be false.
 */
extern JS_FRIEND_API(void)
FinishIncrementalGC(JSRuntime *rt, gcreason::Reason reason);

enum GCProgress {
    /*
     * During non-incremental GC, the GC is bracketed by JSGC_CYCLE_BEGIN/END
     * callbacks. During an incremental GC, the sequence of callbacks is as
     * follows:
     *   JSGC_CYCLE_BEGIN, JSGC_SLICE_END  (first slice)
     *   JSGC_SLICE_BEGIN, JSGC_SLICE_END  (second slice)
     *   ...
     *   JSGC_SLICE_BEGIN, JSGC_CYCLE_END  (last slice)
     */

    GC_CYCLE_BEGIN,
    GC_SLICE_BEGIN,
    GC_SLICE_END,
    GC_CYCLE_END
};

struct JS_FRIEND_API(GCDescription) {
    bool isCompartment_;

    GCDescription(bool isCompartment)
      : isCompartment_(isCompartment) {}

    jschar *formatMessage(JSRuntime *rt) const;
    jschar *formatJSON(JSRuntime *rt, uint64_t timestamp) const;
};

typedef void
(* GCSliceCallback)(JSRuntime *rt, GCProgress progress, const GCDescription &desc);

/*
 * The GC slice callback is called at the beginning and end of each slice. This
 * callback may be used for GC notifications as well as to perform additional
 * marking.
 */
extern JS_FRIEND_API(GCSliceCallback)
SetGCSliceCallback(JSRuntime *rt, GCSliceCallback callback);

/*
 * Incremental GC defaults to enabled, but may be disabled for testing or in
 * embeddings that have not yet implemented barriers on their native classes.
 * There is not currently a way to re-enable incremental GC once it has been
 * disabled on the runtime.
 */
extern JS_FRIEND_API(void)
DisableIncrementalGC(JSRuntime *rt);

/*
 * Returns true if incremental GC is enabled. Simply having incremental GC
 * enabled is not sufficient to ensure incremental collections are happening.
 * See the comment "Incremental GC" above for reasons why incremental GC may be
 * suppressed. Inspection of the "nonincremental reason" field of the
 * GCDescription returned by GCSliceCallback may help narrow down the cause if
 * collections are not happening incrementally when expected.
 */
extern JS_FRIEND_API(bool)
IsIncrementalGCEnabled(JSRuntime *rt);

/*
 * Returns true while an incremental GC is ongoing, both when actively
 * collecting and between slices.
 */
JS_FRIEND_API(bool)
IsIncrementalGCInProgress(JSRuntime *rt);

/*
 * Returns true when writes to GC things must call an incremental (pre) barrier.
 * This is generally only true when running mutator code in-between GC slices.
 * At other times, the barrier may be elided for performance.
 */
extern JS_FRIEND_API(bool)
IsIncrementalBarrierNeeded(JSRuntime *rt);

extern JS_FRIEND_API(bool)
IsIncrementalBarrierNeeded(JSContext *cx);

/*
 * Notify the GC that a reference to a GC thing is about to be overwritten.
 * These methods must be called if IsIncrementalBarrierNeeded.
 */
extern JS_FRIEND_API(void)
IncrementalReferenceBarrier(void *ptr, JSGCTraceKind kind);

extern JS_FRIEND_API(void)
IncrementalValueBarrier(const Value &v);

extern JS_FRIEND_API(void)
IncrementalObjectBarrier(JSObject *obj);

/*
 * Returns true if the most recent GC ran incrementally.
 */
extern JS_FRIEND_API(bool)
WasIncrementalGC(JSRuntime *rt);

/*
 * Generational GC:
 *
 * Note: Generational GC is not yet enabled by default. The following class
 *       is non-functional unless SpiderMonkey was configured with
 *       --enable-gcgenerational.
 */

/* Ensure that generational GC is disabled within some scope. */
class JS_FRIEND_API(AutoDisableGenerationalGC)
{
    JSRuntime *runtime;
#if defined(JSGC_GENERATIONAL) && defined(JS_GC_ZEAL)
    bool restartVerifier;
#endif

  public:
    AutoDisableGenerationalGC(JSRuntime *rt);
    ~AutoDisableGenerationalGC();
};

/*
 * Returns true if generational allocation and collection is currently enabled
 * on the given runtime.
 */
extern JS_FRIEND_API(bool)
IsGenerationalGCEnabled(JSRuntime *rt);

/*
 * Returns the GC's "number". This does not correspond directly to the number
 * of GCs that have been run, but is guaranteed to be monotonically increasing
 * with GC activity.
 */
extern JS_FRIEND_API(size_t)
GetGCNumber();

/*
 * The GC does not immediately return the unused memory freed by a collection
 * back to the system incase it is needed soon afterwards. This call forces the
 * GC to return this memory immediately.
 */
extern JS_FRIEND_API(void)
ShrinkGCBuffers(JSRuntime *rt);

/*
 * Assert if any GC occured while this guard object was live. This is most
 * useful to help the exact rooting hazard analysis in complex regions, since
 * it cannot understand dataflow.
 *
 * Note: GC behavior is unpredictable even when deterministice and is generally
 *       non-deterministic in practice. The fact that this guard has not
 *       asserted is not a guarantee that a GC cannot happen in the guarded
 *       region. As a rule, anyone performing a GC unsafe action should
 *       understand the GC properties of all code in that region and ensure
 *       that the hazard analysis is correct for that code, rather than relying
 *       on this class.
 */
class JS_PUBLIC_API(AutoAssertNoGC)
{
#ifdef JS_DEBUG
    JSRuntime *runtime;
    size_t gcNumber;

  public:
    AutoAssertNoGC();
    AutoAssertNoGC(JSRuntime *rt);
    ~AutoAssertNoGC();
#else
  public:
    /* Prevent unreferenced local warnings in opt builds. */
    AutoAssertNoGC() {}
    AutoAssertNoGC(JSRuntime *) {}
#endif
};

class JS_PUBLIC_API(ObjectPtr)
{
    Heap<JSObject *> value;

  public:
    ObjectPtr() : value(nullptr) {}

    ObjectPtr(JSObject *obj) : value(obj) {}

    /* Always call finalize before the destructor. */
    ~ObjectPtr() { MOZ_ASSERT(!value); }

    void finalize(JSRuntime *rt) {
        if (IsIncrementalBarrierNeeded(rt))
            IncrementalObjectBarrier(value);
        value = nullptr;
    }

    void init(JSObject *obj) { value = obj; }

    JSObject *get() const { return value; }

    void writeBarrierPre(JSRuntime *rt) {
        IncrementalObjectBarrier(value);
    }

    bool isAboutToBeFinalized();

    ObjectPtr &operator=(JSObject *obj) {
        IncrementalObjectBarrier(value);
        value = obj;
        return *this;
    }

    void trace(JSTracer *trc, const char *name);

    JSObject &operator*() const { return *value; }
    JSObject *operator->() const { return value; }
    operator JSObject *() const { return value; }
};

/*
 * Unsets the gray bit for anything reachable from |thing|. |kind| should not be
 * JSTRACE_SHAPE. |thing| should be non-null.
 */
extern JS_FRIEND_API(bool)
UnmarkGrayGCThingRecursively(void *thing, JSGCTraceKind kind);

/*
 * This should be called when an object that is marked gray is exposed to the JS
 * engine (by handing it to running JS code or writing it into live JS
 * data). During incremental GC, since the gray bits haven't been computed yet,
 * we conservatively mark the object black.
 */
static MOZ_ALWAYS_INLINE void
ExposeGCThingToActiveJS(void *thing, JSGCTraceKind kind)
{
    MOZ_ASSERT(kind != JSTRACE_SHAPE);

    shadow::Runtime *rt = js::gc::GetGCThingRuntime(thing);
#ifdef JSGC_GENERATIONAL
    /*
     * GC things residing in the nursery cannot be gray: they have no mark bits.
     * All live objects in the nursery are moved to tenured at the beginning of
     * each GC slice, so the gray marker never sees nursery things.
     */
    if (js::gc::IsInsideNursery(rt, thing))
        return;
#endif
    if (IsIncrementalBarrierNeededOnGCThing(rt, thing, kind))
        IncrementalReferenceBarrier(thing, kind);
    else if (GCThingIsMarkedGray(thing))
        UnmarkGrayGCThingRecursively(thing, kind);
}

static MOZ_ALWAYS_INLINE void
ExposeValueToActiveJS(const Value &v)
{
    if (v.isMarkable())
        ExposeGCThingToActiveJS(v.toGCThing(), v.gcKind());
}

static MOZ_ALWAYS_INLINE void
ExposeObjectToActiveJS(JSObject *obj)
{
    ExposeGCThingToActiveJS(obj, JSTRACE_OBJECT);
}

/*
 * If a GC is currently marking, mark the object black.
 */
static MOZ_ALWAYS_INLINE void
MarkGCThingAsLive(JSRuntime *rt_, void *thing, JSGCTraceKind kind)
{
    shadow::Runtime *rt = shadow::Runtime::asShadowRuntime(rt_);
#ifdef JSGC_GENERATIONAL
    /*
     * Any object in the nursery will not be freed during any GC running at that time.
     */
    if (js::gc::IsInsideNursery(rt, thing))
        return;
#endif
    if (IsIncrementalBarrierNeededOnGCThing(rt, thing, kind))
        IncrementalReferenceBarrier(thing, kind);
}

static MOZ_ALWAYS_INLINE void
MarkStringAsLive(Zone *zone, JSString *string)
{
    JSRuntime *rt = JS::shadow::Zone::asShadowZone(zone)->runtimeFromMainThread();
    MarkGCThingAsLive(rt, string, JSTRACE_STRING);
}

/*
 * Internal to Firefox.
 *
 * Note: this is not related to the PokeGC in nsJSEnvironment.
 */
extern JS_FRIEND_API(void)
PokeGC(JSRuntime *rt);

/*
 * Internal to Firefox.
 */
extern JS_FRIEND_API(void)
NotifyDidPaint(JSRuntime *rt);

} /* namespace JS */

#endif /* js_GCAPI_h */
