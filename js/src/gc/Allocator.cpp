/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Allocator.h"

#include "jscntxt.h"

#include "gc/GCTrace.h"
#include "gc/Nursery.h"
#include "jit/JitCompartment.h"
#include "vm/Runtime.h"
#include "vm/String.h"

#include "jsobjinlines.h"

using namespace js;
using namespace gc;

static inline bool
ShouldNurseryAllocateObject(const Nursery &nursery, InitialHeap heap)
{
    return nursery.isEnabled() && heap != TenuredHeap;
}

/*
 * Attempt to allocate a new GC thing out of the nursery. If there is not enough
 * room in the nursery or there is an OOM, this method will return nullptr.
 */
template <AllowGC allowGC>
inline JSObject *
TryNewNurseryObject(JSContext *cx, size_t thingSize, size_t nDynamicSlots, const Class *clasp)
{
    MOZ_ASSERT(!IsAtomsCompartment(cx->compartment()));
    JSRuntime *rt = cx->runtime();
    Nursery &nursery = rt->gc.nursery;
    JSObject *obj = nursery.allocateObject(cx, thingSize, nDynamicSlots, clasp);
    if (obj)
        return obj;
    if (allowGC && !rt->mainThread.suppressGC) {
        cx->minorGC(JS::gcreason::OUT_OF_NURSERY);

        /* Exceeding gcMaxBytes while tenuring can disable the Nursery. */
        if (nursery.isEnabled()) {
            JSObject *obj = nursery.allocateObject(cx, thingSize, nDynamicSlots, clasp);
            MOZ_ASSERT(obj);
            return obj;
        }
    }
    return nullptr;
}

static inline bool
PossiblyFail()
{
    JS_OOM_POSSIBLY_FAIL_BOOL();
    return true;
}

static inline bool
GCIfNeeded(ExclusiveContext *cx)
{
    if (cx->isJSContext()) {
        JSContext *ncx = cx->asJSContext();
        JSRuntime *rt = ncx->runtime();

#ifdef JS_GC_ZEAL
        if (rt->gc.needZealousGC())
            rt->gc.runDebugGC();
#endif

        // Invoking the interrupt callback can fail and we can't usefully
        // handle that here. Just check in case we need to collect instead.
        if (rt->hasPendingInterrupt())
            rt->gc.gcIfRequested(ncx);

        // If we have grown past our GC heap threshold while in the middle of
        // an incremental GC, we're growing faster than we're GCing, so stop
        // the world and do a full, non-incremental GC right now, if possible.
        if (rt->gc.isIncrementalGCInProgress() &&
            cx->zone()->usage.gcBytes() > cx->zone()->threshold.gcTriggerBytes())
        {
            PrepareZoneForGC(cx->zone());
            AutoKeepAtoms keepAtoms(cx->perThreadData);
            rt->gc.gc(GC_NORMAL, JS::gcreason::INCREMENTAL_TOO_SLOW);
        }
    }

    return true;
}

template <AllowGC allowGC>
static inline bool
CheckAllocatorState(ExclusiveContext *cx, AllocKind kind)
{
    if (allowGC) {
        if (!GCIfNeeded(cx))
            return false;
    }

    if (!cx->isJSContext())
        return true;

    JSContext *ncx = cx->asJSContext();
    JSRuntime *rt = ncx->runtime();
#if defined(JS_GC_ZEAL) || defined(DEBUG)
    MOZ_ASSERT_IF(rt->isAtomsCompartment(ncx->compartment()),
                  kind == FINALIZE_STRING ||
                  kind == FINALIZE_FAT_INLINE_STRING ||
                  kind == FINALIZE_SYMBOL ||
                  kind == FINALIZE_JITCODE);
    MOZ_ASSERT(!rt->isHeapBusy());
    MOZ_ASSERT(rt->gc.isAllocAllowed());
#endif

    // Crash if we perform a GC action when it is not safe.
    if (allowGC && !rt->mainThread.suppressGC)
        JS::AutoAssertOnGC::VerifyIsSafeToGC(rt);

    // For testing out of memory conditions
    if (!PossiblyFail()) {
        ReportOutOfMemory(ncx);
        return false;
    }

    return true;
}

template <typename T>
static inline void
CheckIncrementalZoneState(ExclusiveContext *cx, T *t)
{
#ifdef DEBUG
    if (!cx->isJSContext())
        return;

    Zone *zone = cx->asJSContext()->zone();
    MOZ_ASSERT_IF(t && zone->wasGCStarted() && (zone->isGCMarking() || zone->isGCSweeping()),
                  t->asTenured().arenaHeader()->allocatedDuringIncremental);
#endif
}

/*
 * Allocate a new GC thing. After a successful allocation the caller must
 * fully initialize the thing before calling any function that can potentially
 * trigger GC. This will ensure that GC tracing never sees junk values stored
 * in the partially initialized thing.
 */

template <AllowGC allowGC>
inline JSObject *
AllocateObject(ExclusiveContext *cx, AllocKind kind, size_t nDynamicSlots, InitialHeap heap,
               const Class *clasp)
{
    size_t thingSize = Arena::thingSize(kind);

    MOZ_ASSERT(thingSize == Arena::thingSize(kind));
    MOZ_ASSERT(thingSize >= sizeof(JSObject_Slots0));
    static_assert(sizeof(JSObject_Slots0) >= CellSize,
                  "All allocations must be at least the allocator-imposed minimum size.");

    if (!CheckAllocatorState<allowGC>(cx, kind))
        return nullptr;

    if (cx->isJSContext() &&
        ShouldNurseryAllocateObject(cx->asJSContext()->nursery(), heap))
    {
        JSObject *obj = TryNewNurseryObject<allowGC>(cx->asJSContext(), thingSize, nDynamicSlots,
                                                     clasp);
        if (obj)
            return obj;
    }

    HeapSlot *slots = nullptr;
    if (nDynamicSlots) {
        slots = cx->zone()->pod_malloc<HeapSlot>(nDynamicSlots);
        if (MOZ_UNLIKELY(!slots))
            return nullptr;
        Debug_SetSlotRangeToCrashOnTouch(slots, nDynamicSlots);
    }

    JSObject *obj = reinterpret_cast<JSObject *>(cx->arenas()->allocateFromFreeList(kind, thingSize));
    if (!obj)
        obj = reinterpret_cast<JSObject *>(GCRuntime::refillFreeListFromAnyThread<allowGC>(cx, kind));

    if (obj)
        obj->setInitialSlotsMaybeNonNative(slots);
    else
        js_free(slots);

    CheckIncrementalZoneState(cx, obj);
    TraceTenuredAlloc(obj, kind);
    return obj;
}

template <typename T, AllowGC allowGC>
inline T *
AllocateNonObject(ExclusiveContext *cx)
{
    static_assert(sizeof(T) >= CellSize,
                  "All allocations must be at least the allocator-imposed minimum size.");

    AllocKind kind = MapTypeToFinalizeKind<T>::kind;
    size_t thingSize = sizeof(T);

    MOZ_ASSERT(thingSize == Arena::thingSize(kind));
    if (!CheckAllocatorState<allowGC>(cx, kind))
        return nullptr;

    T *t = static_cast<T *>(cx->arenas()->allocateFromFreeList(kind, thingSize));
    if (!t)
        t = static_cast<T *>(GCRuntime::refillFreeListFromAnyThread<allowGC>(cx, kind));

    CheckIncrementalZoneState(cx, t);
    TraceTenuredAlloc(t, kind);
    return t;
}

/*
 * When allocating for initialization from a cached object copy, we will
 * potentially destroy the cache entry we want to copy if we allow GC. On the
 * other hand, since these allocations are extremely common, we don't want to
 * delay GC from these allocation sites. Instead we allow the GC, but still
 * fail the allocation, forcing the non-cached path.
 */
template <AllowGC allowGC>
NativeObject *
js::gc::AllocateObjectForCacheHit(JSContext *cx, AllocKind kind, InitialHeap heap,
                                  const js::Class *clasp)
{
    MOZ_ASSERT(clasp->isNative());

    if (ShouldNurseryAllocateObject(cx->nursery(), heap)) {
        size_t thingSize = Arena::thingSize(kind);

        MOZ_ASSERT(thingSize == Arena::thingSize(kind));
        if (!CheckAllocatorState<NoGC>(cx, kind))
            return nullptr;

        JSObject *obj = TryNewNurseryObject<NoGC>(cx, thingSize, 0, clasp);
        if (!obj && allowGC) {
            cx->minorGC(JS::gcreason::OUT_OF_NURSERY);
            return nullptr;
        }
        return reinterpret_cast<NativeObject *>(obj);
    }

    JSObject *obj = AllocateObject<NoGC>(cx, kind, 0, heap, clasp);
    if (!obj && allowGC) {
        cx->runtime()->gc.maybeGC(cx->zone());
        return nullptr;
    }

    return reinterpret_cast<NativeObject *>(obj);
}
template NativeObject *js::gc::AllocateObjectForCacheHit<CanGC>(JSContext *, AllocKind, InitialHeap,
                                                                const Class *);
template NativeObject *js::gc::AllocateObjectForCacheHit<NoGC>(JSContext *, AllocKind, InitialHeap,
                                                               const Class *);

template <AllowGC allowGC>
JSObject *
js::NewGCObject(ExclusiveContext *cx, AllocKind kind, size_t nDynamicSlots,
            InitialHeap heap, const Class *clasp)
{
    MOZ_ASSERT(kind >= FINALIZE_OBJECT0 && kind <= FINALIZE_OBJECT_LAST);
    return AllocateObject<allowGC>(cx, kind, nDynamicSlots, heap, clasp);
}
template JSObject *js::NewGCObject<CanGC>(ExclusiveContext *, AllocKind, size_t, InitialHeap,
                                          const Class *);
template JSObject *js::NewGCObject<NoGC>(ExclusiveContext *, AllocKind, size_t, InitialHeap,
                                         const Class *);

template <AllowGC allowGC>
jit::JitCode *
js::NewJitCode(ExclusiveContext *cx)
{
    return AllocateNonObject<jit::JitCode, allowGC>(cx);
}
template jit::JitCode *js::NewJitCode<CanGC>(ExclusiveContext *cx);
template jit::JitCode *js::NewJitCode<NoGC>(ExclusiveContext *cx);

ObjectGroup *
js::NewObjectGroup(ExclusiveContext *cx)
{
    return AllocateNonObject<ObjectGroup, CanGC>(cx);
}

template <AllowGC allowGC>
JSString *
js::NewGCString(ExclusiveContext *cx)
{
    return AllocateNonObject<JSString, allowGC>(cx);
}
template JSString *js::NewGCString<CanGC>(ExclusiveContext *cx);
template JSString *js::NewGCString<NoGC>(ExclusiveContext *cx);

template <AllowGC allowGC>
JSFatInlineString *
js::NewGCFatInlineString(ExclusiveContext *cx)
{
    return AllocateNonObject<JSFatInlineString, allowGC>(cx);
}
template JSFatInlineString *js::NewGCFatInlineString<CanGC>(ExclusiveContext *cx);
template JSFatInlineString *js::NewGCFatInlineString<NoGC>(ExclusiveContext *cx);

JSExternalString *
js::NewGCExternalString(ExclusiveContext *cx)
{
    return AllocateNonObject<JSExternalString, CanGC>(cx);
}

Shape *
js::NewGCShape(ExclusiveContext *cx)
{
    return AllocateNonObject<Shape, CanGC>(cx);
}

Shape *
js::NewGCAccessorShape(ExclusiveContext *cx)
{
    return AllocateNonObject<AccessorShape, CanGC>(cx);
}

JSScript *
js::NewGCScript(ExclusiveContext *cx)
{
    return AllocateNonObject<JSScript, CanGC>(cx);
}

LazyScript *
js::NewGCLazyScript(ExclusiveContext *cx)
{
    return AllocateNonObject<LazyScript, CanGC>(cx);
}

template <AllowGC allowGC>
BaseShape *
js::NewGCBaseShape(ExclusiveContext *cx)
{
    return AllocateNonObject<BaseShape, allowGC>(cx);
}
template BaseShape *js::NewGCBaseShape<CanGC>(ExclusiveContext *cx);
template BaseShape *js::NewGCBaseShape<NoGC>(ExclusiveContext *cx);

template <AllowGC allowGC>
JS::Symbol *
js::NewGCSymbol(ExclusiveContext *cx)
{
    return AllocateNonObject<JS::Symbol, allowGC>(cx);
}
template JS::Symbol *js::NewGCSymbol<CanGC>(ExclusiveContext *cx);
template JS::Symbol *js::NewGCSymbol<NoGC>(ExclusiveContext *cx);
