/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Marking.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/TypeTraits.h"

#include "jsgc.h"
#include "jsprf.h"

#include "gc/GCInternals.h"
#include "jit/IonCode.h"
#include "js/SliceBudget.h"
#include "vm/ArgumentsObject.h"
#include "vm/ArrayObject.h"
#include "vm/ScopeObject.h"
#include "vm/Shape.h"
#include "vm/Symbol.h"
#include "vm/TypedArrayObject.h"
#include "vm/UnboxedObject.h"

#include "jscompartmentinlines.h"
#include "jsobjinlines.h"

#include "gc/Nursery-inl.h"
#include "vm/String-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::DebugOnly;
using mozilla::IsBaseOf;
using mozilla::IsSame;
using mozilla::MakeRange;

void * const js::NullPtr::constNullValue = nullptr;

JS_PUBLIC_DATA(void * const) JS::NullPtr::constNullValue = nullptr;

#if defined(DEBUG)
template<typename T>
static inline bool
IsThingPoisoned(T* thing)
{
    const uint8_t poisonBytes[] = {
        JS_FRESH_NURSERY_PATTERN,
        JS_SWEPT_NURSERY_PATTERN,
        JS_ALLOCATED_NURSERY_PATTERN,
        JS_FRESH_TENURED_PATTERN,
        JS_SWEPT_TENURED_PATTERN,
        JS_ALLOCATED_TENURED_PATTERN,
        JS_SWEPT_CODE_PATTERN,
        JS_SWEPT_FRAME_PATTERN
    };
    const int numPoisonBytes = sizeof(poisonBytes) / sizeof(poisonBytes[0]);
    uint32_t* p = reinterpret_cast<uint32_t*>(reinterpret_cast<FreeSpan*>(thing) + 1);
    // Note: all free patterns are odd to make the common, not-poisoned case a single test.
    if ((*p & 1) == 0)
        return false;
    for (int i = 0; i < numPoisonBytes; ++i) {
        const uint8_t pb = poisonBytes[i];
        const uint32_t pw = pb | (pb << 8) | (pb << 16) | (pb << 24);
        if (*p == pw)
            return true;
    }
    return false;
}
#endif

template <typename T> bool ThingIsPermanentAtomOrWellKnownSymbol(T* thing) { return false; }
template <> bool ThingIsPermanentAtomOrWellKnownSymbol<JSString>(JSString* str) {
    return str->isPermanentAtom();
}
template <> bool ThingIsPermanentAtomOrWellKnownSymbol<JSFlatString>(JSFlatString* str) {
    return str->isPermanentAtom();
}
template <> bool ThingIsPermanentAtomOrWellKnownSymbol<JSLinearString>(JSLinearString* str) {
    return str->isPermanentAtom();
}
template <> bool ThingIsPermanentAtomOrWellKnownSymbol<JSAtom>(JSAtom* atom) {
    return atom->isPermanent();
}
template <> bool ThingIsPermanentAtomOrWellKnownSymbol<PropertyName>(PropertyName* name) {
    return name->isPermanent();
}
template <> bool ThingIsPermanentAtomOrWellKnownSymbol<JS::Symbol>(JS::Symbol* sym) {
    return sym->isWellKnownSymbol();
}

template<typename T>
void
js::CheckTracedThing(JSTracer* trc, T thing)
{
#ifdef DEBUG
    MOZ_ASSERT(trc);
    MOZ_ASSERT(thing);

    thing = MaybeForwarded(thing);

    /* This function uses data that's not available in the nursery. */
    if (IsInsideNursery(thing))
        return;

    MOZ_ASSERT_IF(!MovingTracer::IsMovingTracer(trc) && !Nursery::IsMinorCollectionTracer(trc),
                  !IsForwarded(thing));

    /*
     * Permanent atoms are not associated with this runtime, but will be
     * ignored during marking.
     */
    if (ThingIsPermanentAtomOrWellKnownSymbol(thing))
        return;

    Zone* zone = thing->zoneFromAnyThread();
    JSRuntime* rt = trc->runtime();

    MOZ_ASSERT_IF(!MovingTracer::IsMovingTracer(trc), CurrentThreadCanAccessZone(zone));
    MOZ_ASSERT_IF(!MovingTracer::IsMovingTracer(trc), CurrentThreadCanAccessRuntime(rt));

    MOZ_ASSERT(zone->runtimeFromAnyThread() == trc->runtime());

    MOZ_ASSERT(thing->isAligned());
    MOZ_ASSERT(MapTypeToTraceKind<typename mozilla::RemovePointer<T>::Type>::kind ==
               GetGCThingTraceKind(thing));

    /*
     * Do not check IsMarkingTracer directly -- it should only be used in paths
     * where we cannot be the gray buffering tracer.
     */
    bool isGcMarkingTracer = trc->isMarkingTracer();

    MOZ_ASSERT_IF(zone->requireGCTracer(), isGcMarkingTracer || IsBufferingGrayRoots(trc));

    if (isGcMarkingTracer) {
        GCMarker* gcMarker = static_cast<GCMarker*>(trc);
        MOZ_ASSERT_IF(gcMarker->shouldCheckCompartments(),
                      zone->isCollecting() || rt->isAtomsZone(zone));

        MOZ_ASSERT_IF(gcMarker->markColor() == GRAY,
                      !zone->isGCMarkingBlack() || rt->isAtomsZone(zone));

        MOZ_ASSERT(!(zone->isGCSweeping() || zone->isGCFinished() || zone->isGCCompacting()));
    }

    /*
     * Try to assert that the thing is allocated.  This is complicated by the
     * fact that allocated things may still contain the poison pattern if that
     * part has not been overwritten, and that the free span list head in the
     * ArenaHeader may not be synced with the real one in ArenaLists.  Also,
     * background sweeping may be running and concurrently modifiying the free
     * list.
     */
    MOZ_ASSERT_IF(IsThingPoisoned(thing) && rt->isHeapBusy() && !rt->gc.isBackgroundSweeping(),
                  !InFreeList(thing->asTenured().arenaHeader(), thing));
#endif
}

template <typename S>
struct CheckTracedFunctor : public VoidDefaultAdaptor<S> {
    template <typename T> void operator()(T* t, JSTracer* trc) { CheckTracedThing(trc, t); }
};

namespace js {
template<>
void
CheckTracedThing<Value>(JSTracer* trc, Value val)
{
    DispatchValueTyped(CheckTracedFunctor<Value>(), val, trc);
}

template <>
void
CheckTracedThing<jsid>(JSTracer* trc, jsid id)
{
    DispatchIdTyped(CheckTracedFunctor<jsid>(), id, trc);
}

#define IMPL_CHECK_TRACED_THING(_, type, __) \
    template void CheckTracedThing<type*>(JSTracer*, type*);
FOR_EACH_GC_LAYOUT(IMPL_CHECK_TRACED_THING);
#undef IMPL_CHECK_TRACED_THING
} // namespace js

static bool
ShouldMarkCrossCompartment(JSTracer* trc, JSObject* src, Cell* cell)
{
    if (!trc->isMarkingTracer())
        return true;

    uint32_t color = static_cast<GCMarker*>(trc)->markColor();
    MOZ_ASSERT(color == BLACK || color == GRAY);

    if (!cell->isTenured()) {
        MOZ_ASSERT(color == BLACK);
        return false;
    }
    TenuredCell& tenured = cell->asTenured();

    JS::Zone* zone = tenured.zone();
    if (color == BLACK) {
        /*
         * Having black->gray edges violates our promise to the cycle
         * collector. This can happen if we're collecting a compartment and it
         * has an edge to an uncollected compartment: it's possible that the
         * source and destination of the cross-compartment edge should be gray,
         * but the source was marked black by the conservative scanner.
         */
        if (tenured.isMarked(GRAY)) {
            MOZ_ASSERT(!zone->isCollecting());
            trc->runtime()->gc.setFoundBlackGrayEdges();
        }
        return zone->isGCMarking();
    } else {
        if (zone->isGCMarkingBlack()) {
            /*
             * The destination compartment is being not being marked gray now,
             * but it will be later, so record the cell so it can be marked gray
             * at the appropriate time.
             */
            if (!tenured.isMarked())
                DelayCrossCompartmentGrayMarking(src);
            return false;
        }
        return zone->isGCMarkingGray();
    }
}

static bool
ShouldMarkCrossCompartment(JSTracer* trc, JSObject* src, Value val)
{
    return val.isMarkable() && ShouldMarkCrossCompartment(trc, src, (Cell*)val.toGCThing());
}

#define JS_ROOT_MARKING_ASSERT(trc) \
    MOZ_ASSERT_IF(trc->isMarkingTracer(), \
                  trc->runtime()->gc.state() == NO_INCREMENTAL || \
                  trc->runtime()->gc.state() == MARK_ROOTS);

// A C++ version of JSGCTraceKind
enum class TraceKind {
#define NAMES(name, _, __) name,
FOR_EACH_GC_LAYOUT(NAMES)
#undef NAMES
};

#define FOR_EACH_GC_POINTER_TYPE(D) \
    D(BaseShape*) \
    D(UnownedBaseShape*) \
    D(jit::JitCode*) \
    D(NativeObject*) \
    D(ArrayObject*) \
    D(ArgumentsObject*) \
    D(ArrayBufferObject*) \
    D(ArrayBufferObjectMaybeShared*) \
    D(ArrayBufferViewObject*) \
    D(DebugScopeObject*) \
    D(GlobalObject*) \
    D(JSObject*) \
    D(JSFunction*) \
    D(NestedScopeObject*) \
    D(PlainObject*) \
    D(SavedFrame*) \
    D(ScopeObject*) \
    D(ScriptSourceObject*) \
    D(SharedArrayBufferObject*) \
    D(SharedTypedArrayObject*) \
    D(JSScript*) \
    D(LazyScript*) \
    D(Shape*) \
    D(JSAtom*) \
    D(JSString*) \
    D(JSFlatString*) \
    D(JSLinearString*) \
    D(PropertyName*) \
    D(JS::Symbol*) \
    D(js::ObjectGroup*) \
    D(Value) \
    D(jsid)

// The second parameter to BaseGCType is derived automatically based on T. The
// relation here is that for any T, the TraceKind will automatically,
// statically select the correct Cell layout for marking. Below, we instantiate
// each override with a declaration of the most derived layout type.
//
// Usage:
//   BaseGCType<T>::type
//
// Examples:
//   BaseGCType<JSFunction>::type => JSObject
//   BaseGCType<UnownedBaseShape>::type => BaseShape
//   etc.
template <typename T,
          TraceKind = IsBaseOf<JSObject, T>::value     ? TraceKind::Object
                    : IsBaseOf<JSString, T>::value     ? TraceKind::String
                    : IsBaseOf<JS::Symbol, T>::value   ? TraceKind::Symbol
                    : IsBaseOf<JSScript, T>::value     ? TraceKind::Script
                    : IsBaseOf<Shape, T>::value        ? TraceKind::Shape
                    : IsBaseOf<BaseShape, T>::value    ? TraceKind::BaseShape
                    : IsBaseOf<jit::JitCode, T>::value ? TraceKind::JitCode
                    : IsBaseOf<LazyScript, T>::value   ? TraceKind::LazyScript
                    :                                    TraceKind::ObjectGroup>
struct BaseGCType;
#define IMPL_BASE_GC_TYPE(name, type_, _) \
    template <typename T> struct BaseGCType<T, TraceKind:: name> { typedef type_ type; };
FOR_EACH_GC_LAYOUT(IMPL_BASE_GC_TYPE);
#undef IMPL_BASE_GC_TYPE

// Our barrier templates are parameterized on the pointer types so that we can
// share the definitions with Value and jsid. Thus, we need to strip the
// pointer before sending the type to BaseGCType and re-add it on the other
// side. As such:
template <typename T> struct PtrBaseGCType {};
template <> struct PtrBaseGCType<Value> { typedef Value type; };
template <> struct PtrBaseGCType<jsid> { typedef jsid type; };
template <typename T> struct PtrBaseGCType<T*> { typedef typename BaseGCType<T>::type* type; };

template <typename T>
typename PtrBaseGCType<T>::type*
ConvertToBase(T* thingp)
{
    return reinterpret_cast<typename PtrBaseGCType<T>::type*>(thingp);
}

template <typename T> void DispatchToTracer(JSTracer* trc, T* thingp, const char* name);
template <typename T> T DoCallback(JS::CallbackTracer* trc, T* thingp, const char* name);
template <typename T> void DoMarking(GCMarker* gcmarker, T thing);
static bool ShouldMarkCrossCompartment(JSTracer* trc, JSObject* src, Cell* cell);
static bool ShouldMarkCrossCompartment(JSTracer* trc, JSObject* src, Value val);

template <typename T>
void
js::TraceEdge(JSTracer* trc, BarrieredBase<T>* thingp, const char* name)
{
    DispatchToTracer(trc, ConvertToBase(thingp->unsafeGet()), name);
}

template <typename T>
void
js::TraceManuallyBarrieredEdge(JSTracer* trc, T* thingp, const char* name)
{
    DispatchToTracer(trc, ConvertToBase(thingp), name);
}

template <typename T>
void
js::TraceRoot(JSTracer* trc, T* thingp, const char* name)
{
    JS_ROOT_MARKING_ASSERT(trc);
    DispatchToTracer(trc, ConvertToBase(thingp), name);
}

template <typename T>
void
js::TraceRange(JSTracer* trc, size_t len, BarrieredBase<T>* vec, const char* name)
{
    JS::AutoTracingIndex index(trc);
    for (auto i : MakeRange(len)) {
        if (InternalGCMethods<T>::isMarkable(vec[i].get()))
            DispatchToTracer(trc, ConvertToBase(vec[i].unsafeGet()), name);
        ++index;
    }
}

template <typename T>
void
js::TraceRootRange(JSTracer* trc, size_t len, T* vec, const char* name)
{
    JS_ROOT_MARKING_ASSERT(trc);
    JS::AutoTracingIndex index(trc);
    for (auto i : MakeRange(len)) {
        if (InternalGCMethods<T>::isMarkable(vec[i]))
            DispatchToTracer(trc, ConvertToBase(&vec[i]), name);
        ++index;
    }
}

// Instantiate a copy of the Tracing templates for each derived type.
#define INSTANTIATE_ALL_VALID_TRACE_FUNCTIONS(type) \
    template void js::TraceEdge<type>(JSTracer*, BarrieredBase<type>*, const char*); \
    template void js::TraceManuallyBarrieredEdge<type>(JSTracer*, type*, const char*); \
    template void js::TraceRoot<type>(JSTracer*, type*, const char*); \
    template void js::TraceRange<type>(JSTracer*, size_t, BarrieredBase<type>*, const char*); \
    template void js::TraceRootRange<type>(JSTracer*, size_t, type*, const char*);
FOR_EACH_GC_POINTER_TYPE(INSTANTIATE_ALL_VALID_TRACE_FUNCTIONS)
#undef INSTANTIATE_ALL_VALID_TRACE_FUNCTIONS

template <typename T>
void
js::TraceManuallyBarrieredCrossCompartmentEdge(JSTracer* trc, JSObject* src, T* dst,
                                               const char* name)
{
    if (ShouldMarkCrossCompartment(trc, src, *dst))
        DispatchToTracer(trc, dst, name);
}
template void js::TraceManuallyBarrieredCrossCompartmentEdge<JSObject*>(JSTracer*, JSObject*,
                                                                        JSObject**, const char*);
template void js::TraceManuallyBarrieredCrossCompartmentEdge<JSScript*>(JSTracer*, JSObject*,
                                                                        JSScript**, const char*);

template <typename T>
void
js::TraceCrossCompartmentEdge(JSTracer* trc, JSObject* src, BarrieredBase<T>* dst, const char* name)
{
    if (ShouldMarkCrossCompartment(trc, src, dst->get()))
        DispatchToTracer(trc, dst->unsafeGet(), name);
}
template void js::TraceCrossCompartmentEdge<Value>(JSTracer*, JSObject*, BarrieredBase<Value>*,
                                                   const char*);

template <typename T>
void
js::TraceProcessGlobalRoot(JSTracer* trc, T* thing, const char* name)
{
    JS_ROOT_MARKING_ASSERT(trc);
    MOZ_ASSERT(ThingIsPermanentAtomOrWellKnownSymbol(thing));

    // We have to mark permanent atoms and well-known symbols through a special
    // method because the default DoMarking implementation automatically skips
    // them. Fortunately, atoms (permanent and non) cannot refer to other GC
    // things so they do not need to go through the mark stack and may simply
    // be marked directly.  Moreover, well-known symbols can refer only to
    // permanent atoms, so likewise require no subsquent marking.
    CheckTracedThing(trc, thing);
    if (trc->isMarkingTracer())
        thing->markIfUnmarked(gc::BLACK);
    else
        DoCallback(trc->asCallbackTracer(), ConvertToBase(&thing), name);
}
template void js::TraceProcessGlobalRoot<JSAtom>(JSTracer*, JSAtom*, const char*);
template void js::TraceProcessGlobalRoot<JS::Symbol>(JSTracer*, JS::Symbol*, const char*);

// This method is responsible for dynamic dispatch to the real tracer
// implementation. Consider replacing this choke point with virtual dispatch:
// a sufficiently smart C++ compiler may be able to devirtualize some paths.
template <typename T>
void
DispatchToTracer(JSTracer* trc, T* thingp, const char* name)
{
#define IS_SAME_TYPE_OR(name, type, _) mozilla::IsSame<type*, T>::value ||
    static_assert(
            FOR_EACH_GC_LAYOUT(IS_SAME_TYPE_OR)
            mozilla::IsSame<T, JS::Value>::value ||
            mozilla::IsSame<T, jsid>::value,
            "Only the base cell layout types are allowed into marking/tracing internals");
#undef IS_SAME_TYPE_OR
    if (trc->isMarkingTracer())
        return DoMarking(static_cast<GCMarker*>(trc), *thingp);
    DoCallback(trc->asCallbackTracer(), thingp, name);
}

template <typename T>
static inline bool
MustSkipMarking(T thing)
{
    // Don't mark things outside a zone if we are in a per-zone GC.
    return !thing->zone()->isGCMarking();
}

template <>
bool
MustSkipMarking<JSObject*>(JSObject* obj)
{
    // We may mark a Nursery thing outside the context of the
    // MinorCollectionTracer because of a pre-barrier. The pre-barrier is not
    // needed in this case because we perform a minor collection before each
    // incremental slice.
    if (IsInsideNursery(obj))
        return true;

    // Don't mark things outside a zone if we are in a per-zone GC. It is
    // faster to check our own arena header, which we can do since we know that
    // the object is tenured.
    return !TenuredCell::fromPointer(obj)->zone()->isGCMarking();
}

template <>
bool
MustSkipMarking<JSString*>(JSString* str)
{
    // Don't mark permanent atoms, as they may be associated with another
    // runtime. Note that traverse() also checks this, but we need to not
    // run the isGCMarking test from off-main-thread, so have to check it here
    // too.
    return str->isPermanentAtom() ||
           !str->zone()->isGCMarking();
}

template <>
bool
MustSkipMarking<JS::Symbol*>(JS::Symbol* sym)
{
    // As for JSString, don't touch a globally owned well-known symbol from
    // off-main-thread.
    return sym->isWellKnownSymbol() ||
           !sym->zone()->isGCMarking();
}

template <typename T>
void
DoMarking(GCMarker* gcmarker, T thing)
{
    // Do per-type marking precondition checks.
    if (MustSkipMarking(thing))
        return;

    gcmarker->traverse(thing);

    // Mark the compartment as live.
    SetMaybeAliveFlag(thing);
}

template <typename S>
struct DoMarkingFunctor : public VoidDefaultAdaptor<S> {
    template <typename T> void operator()(T* t, GCMarker* gcmarker) { DoMarking(gcmarker, t); }
};

template <>
void
DoMarking<Value>(GCMarker* gcmarker, Value val)
{
    DispatchValueTyped(DoMarkingFunctor<Value>(), val, gcmarker);
}

template <>
void
DoMarking<jsid>(GCMarker* gcmarker, jsid id)
{
    DispatchIdTyped(DoMarkingFunctor<jsid>(), id, gcmarker);
}

// The default traversal calls out to the fully generic traceChildren function
// to visit the child edges. In the absense of other traversal mechanisms, this
// function will rapidly grow the stack past its bounds and crash the process.
// Thus, this generic tracing should only be used in cases where subsequent
// tracing will not recurse.
template <typename T>
void
js::GCMarker::traverse(T* thing)
{
    auto asBase = static_cast<typename BaseGCType<T>::type*>(thing);
    MOZ_ASSERT(!ThingIsPermanentAtomOrWellKnownSymbol(asBase));
    if (mark(asBase))
        thing->traceChildren(this);
}

// Shape, BaseShape, String, and Symbol are extremely common, but have simple
// patterns of recursion. We traverse trees of these edges immediately, with
// aggressive, manual inlining, implemented by eagerlyTraceChildren.
template <typename T>
void
js::GCMarker::markAndScan(T* thing)
{
    if (ThingIsPermanentAtomOrWellKnownSymbol(thing))
        return;
    if (mark(thing))
        eagerlyMarkChildren(thing);
}
namespace js {
template <> void GCMarker::traverse(Shape* thing) { markAndScan(thing); }
template <> void GCMarker::traverse(BaseShape* thing) { markAndScan(thing); }
template <> void GCMarker::traverse(JSString* thing) { markAndScan(thing); }
template <> void GCMarker::traverse(JS::Symbol* thing) { markAndScan(thing); }
} // namespace js

// Object and ObjectGroup are extremely common and can contain arbitrarily
// nested graphs, so are not trivially inlined. In this case we use a mark
// stack to control recursion. JitCode shares none of these properties, but is
// included for historical reasons.
template <typename T>
void
js::GCMarker::markAndPush(StackTag tag, T* thing)
{
    if (mark(thing))
        pushTaggedPtr(tag, thing);
}
namespace js {
template <> void GCMarker::traverse(JSObject* thing) { markAndPush(ObjectTag, thing); }
template <> void GCMarker::traverse(ObjectGroup* thing) { markAndPush(GroupTag, thing); }
template <> void GCMarker::traverse(jit::JitCode* thing) { markAndPush(JitCodeTag, thing); }
} // namespace js

template <typename T>
bool
js::GCMarker::mark(T* thing)
{
    CheckTracedThing(this, thing);
    JS_COMPARTMENT_ASSERT(runtime(), thing);
    MOZ_ASSERT(!IsInsideNursery(gc::TenuredCell::fromPointer(thing)));
    return gc::ParticipatesInCC<T>::value
           ? gc::TenuredCell::fromPointer(thing)->markIfUnmarked(markColor())
           : gc::TenuredCell::fromPointer(thing)->markIfUnmarked(gc::BLACK);
}

template <typename T>
static inline void
CheckIsMarkedThing(T* thingp)
{
#define IS_SAME_TYPE_OR(name, type, _) mozilla::IsSame<type*, T>::value ||
    static_assert(
            FOR_EACH_GC_LAYOUT(IS_SAME_TYPE_OR)
            false, "Only the base cell layout types are allowed into marking/tracing internals");
#undef IS_SAME_TYPE_OR

#ifdef DEBUG
    MOZ_ASSERT(thingp);
    MOZ_ASSERT(*thingp);
    JSRuntime* rt = (*thingp)->runtimeFromAnyThread();
    MOZ_ASSERT_IF(!ThingIsPermanentAtomOrWellKnownSymbol(*thingp),
                  CurrentThreadCanAccessRuntime(rt) ||
                  (rt->isHeapCollecting() && rt->gc.state() == SWEEP));
#endif
}

template <typename T>
static bool
IsMarkedInternal(T* thingp)
{
    CheckIsMarkedThing(thingp);
    JSRuntime* rt = (*thingp)->runtimeFromAnyThread();

    if (IsInsideNursery(*thingp)) {
        MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
        return rt->gc.nursery.getForwardedPointer(thingp);
    }

    Zone* zone = (*thingp)->asTenured().zoneFromAnyThread();
    if (!zone->isCollectingFromAnyThread() || zone->isGCFinished())
        return true;
    if (zone->isGCCompacting() && IsForwarded(*thingp))
        *thingp = Forwarded(*thingp);
    return (*thingp)->asTenured().isMarked();
}

template <typename S>
struct IsMarkedFunctor : public IdentityDefaultAdaptor<S> {
    template <typename T> S operator()(T* t, bool* rv) {
        *rv = IsMarkedInternal(&t);
        return js::gc::RewrapValueOrId<S, T*>::wrap(t);
    }
};

template <>
bool
IsMarkedInternal<Value>(Value* valuep)
{
    bool rv = true;
    *valuep = DispatchValueTyped(IsMarkedFunctor<Value>(), *valuep, &rv);
    return rv;
}

template <>
bool
IsMarkedInternal<jsid>(jsid* idp)
{
    bool rv = true;
    *idp = DispatchIdTyped(IsMarkedFunctor<jsid>(), *idp, &rv);
    return rv;
}

template <typename T>
static bool
IsAboutToBeFinalizedInternal(T* thingp)
{
    CheckIsMarkedThing(thingp);
    T thing = *thingp;
    JSRuntime* rt = thing->runtimeFromAnyThread();

    /* Permanent atoms are never finalized by non-owning runtimes. */
    if (ThingIsPermanentAtomOrWellKnownSymbol(thing) && !TlsPerThreadData.get()->associatedWith(rt))
        return false;

    Nursery& nursery = rt->gc.nursery;
    MOZ_ASSERT_IF(!rt->isHeapMinorCollecting(), !IsInsideNursery(thing));
    if (rt->isHeapMinorCollecting()) {
        if (IsInsideNursery(thing))
            return !nursery.getForwardedPointer(thingp);
        return false;
    }

    Zone* zone = thing->asTenured().zoneFromAnyThread();
    if (zone->isGCSweeping()) {
        if (thing->asTenured().arenaHeader()->allocatedDuringIncremental)
            return false;
        return !thing->asTenured().isMarked();
    }
    else if (zone->isGCCompacting() && IsForwarded(thing)) {
        *thingp = Forwarded(thing);
        return false;
    }

    return false;
}

template <typename S>
struct IsAboutToBeFinalizedFunctor : public IdentityDefaultAdaptor<S> {
    template <typename T> S operator()(T* t, bool* rv) {
        *rv = IsAboutToBeFinalizedInternal(&t);
        return js::gc::RewrapValueOrId<S, T*>::wrap(t);
    }
};

template <>
bool
IsAboutToBeFinalizedInternal<Value>(Value* valuep)
{
    bool rv = false;
    *valuep = DispatchValueTyped(IsAboutToBeFinalizedFunctor<Value>(), *valuep, &rv);
    return rv;
}

template <>
bool
IsAboutToBeFinalizedInternal<jsid>(jsid* idp)
{
    bool rv = false;
    *idp = DispatchIdTyped(IsAboutToBeFinalizedFunctor<jsid>(), *idp, &rv);
    return rv;
}

namespace js {
namespace gc {

template <typename T>
bool
IsMarkedUnbarriered(T* thingp)
{
    return IsMarkedInternal(ConvertToBase(thingp));
}

template <typename T>
bool
IsMarked(BarrieredBase<T>* thingp)
{
    return IsMarkedInternal(ConvertToBase(thingp->unsafeGet()));
}

template <typename T>
bool
IsMarked(ReadBarriered<T>* thingp)
{
    return IsMarkedInternal(ConvertToBase(thingp->unsafeGet()));
}

template <typename T>
bool
IsAboutToBeFinalizedUnbarriered(T* thingp)
{
    return IsAboutToBeFinalizedInternal(ConvertToBase(thingp));
}

template <typename T>
bool
IsAboutToBeFinalized(BarrieredBase<T>* thingp)
{
    return IsAboutToBeFinalizedInternal(ConvertToBase(thingp->unsafeGet()));
}

template <typename T>
bool
IsAboutToBeFinalized(ReadBarriered<T>* thingp)
{
    return IsAboutToBeFinalizedInternal(ConvertToBase(thingp->unsafeGet()));
}

// Instantiate a copy of the Tracing templates for each derived type.
#define INSTANTIATE_ALL_VALID_TRACE_FUNCTIONS(type) \
    template bool IsMarkedUnbarriered<type>(type*); \
    template bool IsMarked<type>(BarrieredBase<type>*); \
    template bool IsMarked<type>(ReadBarriered<type>*); \
    template bool IsAboutToBeFinalizedUnbarriered<type>(type*); \
    template bool IsAboutToBeFinalized<type>(BarrieredBase<type>*); \
    template bool IsAboutToBeFinalized<type>(ReadBarriered<type>*);
FOR_EACH_GC_POINTER_TYPE(INSTANTIATE_ALL_VALID_TRACE_FUNCTIONS)
#undef INSTANTIATE_ALL_VALID_TRACE_FUNCTIONS

template <typename T>
T*
UpdateIfRelocated(JSRuntime* rt, T** thingp)
{
    MOZ_ASSERT(thingp);
    if (!*thingp)
        return nullptr;

    if (rt->isHeapMinorCollecting() && IsInsideNursery(*thingp)) {
        rt->gc.nursery.getForwardedPointer(thingp);
        return *thingp;
    }

    Zone* zone = (*thingp)->zone();
    if (zone->isGCCompacting() && IsForwarded(*thingp))
        *thingp = Forwarded(*thingp);

    return *thingp;
}

} /* namespace gc */
} /* namespace js */

/*** Externally Typed Marking ***/

// A typed functor adaptor for TraceRoot.
struct TraceRootFunctor {
    template <typename T>
    void operator()(JSTracer* trc, Cell** thingp, const char* name) {
        TraceRoot(trc, reinterpret_cast<T**>(thingp), name);
    }
};

// A typed functor adaptor for TraceManuallyBarrieredEdge.
struct TraceManuallyBarrieredEdgeFunctor {
    template <typename T>
    void operator()(JSTracer* trc, Cell** thingp, const char* name) {
        TraceManuallyBarrieredEdge(trc, reinterpret_cast<T**>(thingp), name);
    }
};

void
js::gc::TraceGenericPointerRoot(JSTracer* trc, Cell** thingp, const char* name)
{
    MOZ_ASSERT(thingp);
    if (!*thingp)
        return;
    TraceRootFunctor f;
    CallTyped(f, (*thingp)->getTraceKind(), trc, thingp, name);
}

void
js::gc::TraceManuallyBarrieredGenericPointerEdge(JSTracer* trc, Cell** thingp, const char* name)
{
    MOZ_ASSERT(thingp);
    if (!*thingp)
        return;
    TraceManuallyBarrieredEdgeFunctor f;
    CallTyped(f, (*thingp)->getTraceKind(), trc, thingp, name);
}

/*** Type Marking ***/

void
TypeSet::MarkTypeRoot(JSTracer* trc, TypeSet::Type* v, const char* name)
{
    JS_ROOT_MARKING_ASSERT(trc);
    MarkTypeUnbarriered(trc, v, name);
}

void
TypeSet::MarkTypeUnbarriered(JSTracer* trc, TypeSet::Type* v, const char* name)
{
    if (v->isSingletonUnchecked()) {
        JSObject* obj = v->singleton();
        DispatchToTracer(trc, &obj, name);
        *v = TypeSet::ObjectType(obj);
    } else if (v->isGroupUnchecked()) {
        ObjectGroup* group = v->group();
        DispatchToTracer(trc, &group, name);
        *v = TypeSet::ObjectType(group);
    }
}

/*** Slot Marking ***/

void
gc::MarkObjectSlots(JSTracer* trc, NativeObject* obj, uint32_t start, uint32_t nslots)
{
    JS::AutoTracingIndex index(trc, start);
    for (uint32_t i = start; i < (start + nslots); ++i) {
        HeapSlot& slot = obj->getSlotRef(i);
        if (InternalGCMethods<Value>::isMarkable(slot))
            DispatchToTracer(trc, slot.unsafeGet(), "object slot");
        ++index;
    }
}

/*** Special Marking ***/

void
gc::MarkValueForBarrier(JSTracer* trc, Value* valuep, const char* name)
{
    MOZ_ASSERT(!trc->runtime()->isHeapCollecting());
    TraceManuallyBarrieredEdge(trc, valuep, name);
}

void
gc::MarkIdForBarrier(JSTracer* trc, jsid* idp, const char* name)
{
    TraceManuallyBarrieredEdge(trc, idp, name);
}

/*** Push Mark Stack ***/

/*
 * GCMarker::traverse for BaseShape unpacks its children directly onto the mark
 * stack. For a pre-barrier between incremental slices, this may result in
 * objects in the nursery getting pushed onto the mark stack. It is safe to
 * ignore these objects because they will be marked by the matching
 * post-barrier during the minor GC at the start of each incremental slice.
 */
static void
MaybePushMarkStackBetweenSlices(GCMarker* gcmarker, JSObject* thing)
{
    MOZ_ASSERT_IF(gcmarker->runtime()->isHeapBusy(), !IsInsideNursery(thing));

    if (!IsInsideNursery(thing))
        gcmarker->traverse(thing);
}

void
BaseShape::traceChildren(JSTracer* trc)
{
    if (isOwned())
        TraceEdge(trc, &unowned_, "base");

    JSObject* global = compartment()->unsafeUnbarrieredMaybeGlobal();
    if (global)
        TraceManuallyBarrieredEdge(trc, &global, "global");
}

inline void
GCMarker::eagerlyMarkChildren(Shape* shape)
{
  restart:
    traverse(shape->base());

    const BarrieredBase<jsid>& id = shape->propidRef();
    if (JSID_IS_STRING(id))
        traverse(JSID_TO_STRING(id));
    else if (JSID_IS_SYMBOL(id))
        traverse(JSID_TO_SYMBOL(id));

    if (shape->hasGetterObject())
        MaybePushMarkStackBetweenSlices(this, shape->getterObject());

    if (shape->hasSetterObject())
        MaybePushMarkStackBetweenSlices(this, shape->setterObject());

    shape = shape->previous();
    if (shape && shape->markIfUnmarked(this->markColor()))
        goto restart;
}

inline void
GCMarker::eagerlyMarkChildren(BaseShape* base)
{
    base->assertConsistency();

    base->compartment()->mark();

    if (GlobalObject* global = base->compartment()->unsafeUnbarrieredMaybeGlobal())
        traverse(global);

    /*
     * All children of the owned base shape are consistent with its
     * unowned one, thus we do not need to trace through children of the
     * unowned base shape.
     */
    if (base->isOwned()) {
        UnownedBaseShape* unowned = base->baseUnowned();
        MOZ_ASSERT(base->compartment() == unowned->compartment());
        unowned->markIfUnmarked(markColor());
    }
}

static inline void
ScanLinearString(GCMarker* gcmarker, JSLinearString* str)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime(), str);
    MOZ_ASSERT(str->isMarked());

    /*
     * Add extra asserts to confirm the static type to detect incorrect string
     * mutations.
     */
    MOZ_ASSERT(str->JSString::isLinear());
    while (str->hasBase()) {
        str = str->base();
        MOZ_ASSERT(str->JSString::isLinear());
        if (str->isPermanentAtom())
            break;
        JS_COMPARTMENT_ASSERT(gcmarker->runtime(), str);
        if (!str->markIfUnmarked())
            break;
    }
}

/*
 * The function tries to scan the whole rope tree using the marking stack as
 * temporary storage. If that becomes full, the unscanned ropes are added to
 * the delayed marking list. When the function returns, the marking stack is
 * at the same depth as it was on entry. This way we avoid using tags when
 * pushing ropes to the stack as ropes never leaks to other users of the
 * stack. This also assumes that a rope can only point to other ropes or
 * linear strings, it cannot refer to GC things of other types.
 */
static void
ScanRope(GCMarker* gcmarker, JSRope* rope)
{
    ptrdiff_t savedPos = gcmarker->stack.position();
    JS_DIAGNOSTICS_ASSERT(GetGCThingTraceKind(rope) == JSTRACE_STRING);
    for (;;) {
        JS_DIAGNOSTICS_ASSERT(GetGCThingTraceKind(rope) == JSTRACE_STRING);
        JS_DIAGNOSTICS_ASSERT(rope->JSString::isRope());
        JS_COMPARTMENT_ASSERT(gcmarker->runtime(), rope);
        MOZ_ASSERT(rope->isMarked());
        JSRope* next = nullptr;

        JSString* right = rope->rightChild();
        if (!right->isPermanentAtom() && right->markIfUnmarked()) {
            if (right->isLinear())
                ScanLinearString(gcmarker, &right->asLinear());
            else
                next = &right->asRope();
        }

        JSString* left = rope->leftChild();
        if (!left->isPermanentAtom() && left->markIfUnmarked()) {
            if (left->isLinear()) {
                ScanLinearString(gcmarker, &left->asLinear());
            } else {
                /*
                 * When both children are ropes, set aside the right one to
                 * scan it later.
                 */
                if (next && !gcmarker->stack.push(reinterpret_cast<uintptr_t>(next)))
                    gcmarker->delayMarkingChildren(next);
                next = &left->asRope();
            }
        }
        if (next) {
            rope = next;
        } else if (savedPos != gcmarker->stack.position()) {
            MOZ_ASSERT(savedPos < gcmarker->stack.position());
            rope = reinterpret_cast<JSRope*>(gcmarker->stack.pop());
        } else {
            break;
        }
    }
    MOZ_ASSERT(savedPos == gcmarker->stack.position());
 }

inline void
GCMarker::eagerlyMarkChildren(JSString* str)
{
    if (str->isLinear())
        ScanLinearString(this, &str->asLinear());
    else
        ScanRope(this, &str->asRope());
}

inline void
GCMarker::eagerlyMarkChildren(JS::Symbol* sym)
{
    if (JSString* desc = sym->description())
        traverse(desc);
}

/*
 * This function is used by the cycle collector to trace through a
 * shape. The cycle collector does not care about shapes or base
 * shapes, so those are not marked. Instead, any shapes or base shapes
 * that are encountered have their children marked. Stack space is
 * bounded.
 */
void
gc::MarkCycleCollectorChildren(JSTracer* trc, Shape* shape)
{
    /*
     * We need to mark the global, but it's OK to only do this once instead of
     * doing it for every Shape in our lineage, since it's always the same
     * global.
     */
    JSObject* global = shape->compartment()->unsafeUnbarrieredMaybeGlobal();
    MOZ_ASSERT(global);
    TraceManuallyBarrieredEdge(trc, &global, "global");

    do {
        MOZ_ASSERT(global == shape->compartment()->unsafeUnbarrieredMaybeGlobal());

        MOZ_ASSERT(shape->base());
        shape->base()->assertConsistency();

        TraceEdge(trc, &shape->propidRef(), "propid");

        if (shape->hasGetterObject()) {
            JSObject* tmp = shape->getterObject();
            TraceManuallyBarrieredEdge(trc, &tmp, "getter");
            MOZ_ASSERT(tmp == shape->getterObject());
        }

        if (shape->hasSetterObject()) {
            JSObject* tmp = shape->setterObject();
            TraceManuallyBarrieredEdge(trc, &tmp, "setter");
            MOZ_ASSERT(tmp == shape->setterObject());
        }

        shape = shape->previous();
    } while (shape);
}

void
TraceObjectGroupCycleCollectorChildrenCallback(JS::CallbackTracer* trc,
                                               void** thingp, JSGCTraceKind kind);

// Object groups can point to other object groups via an UnboxedLayout or the
// the original unboxed group link. There can potentially be deep or cyclic
// chains of such groups to trace through without going through a thing that
// participates in cycle collection. These need to be handled iteratively to
// avoid blowing the stack when running the cycle collector's callback tracer.
struct ObjectGroupCycleCollectorTracer : public JS::CallbackTracer
{
    explicit ObjectGroupCycleCollectorTracer(JS::CallbackTracer* innerTracer)
        : JS::CallbackTracer(innerTracer->runtime(),
                             TraceObjectGroupCycleCollectorChildrenCallback,
                             DoNotTraceWeakMaps),
          innerTracer(innerTracer)
    {}

    JS::CallbackTracer* innerTracer;
    Vector<ObjectGroup*, 4, SystemAllocPolicy> seen, worklist;
};

void
TraceObjectGroupCycleCollectorChildrenCallback(JS::CallbackTracer* trcArg,
                                               void** thingp, JSGCTraceKind kind)
{
    ObjectGroupCycleCollectorTracer* trc = static_cast<ObjectGroupCycleCollectorTracer*>(trcArg);
    JS::GCCellPtr thing(*thingp, kind);

    if (thing.isObject() || thing.isScript()) {
        // Invoke the inner cycle collector callback on this child. It will not
        // recurse back into TraceChildren.
        trc->innerTracer->invoke(thingp, kind);
        return;
    }

    if (thing.isObjectGroup()) {
        // If this group is required to be in an ObjectGroup chain, trace it
        // via the provided worklist rather than continuing to recurse.
        ObjectGroup* group = static_cast<ObjectGroup*>(thing.asCell());
        if (group->maybeUnboxedLayout()) {
            for (size_t i = 0; i < trc->seen.length(); i++) {
                if (trc->seen[i] == group)
                    return;
            }
            if (trc->seen.append(group) && trc->worklist.append(group)) {
                return;
            } else {
                // If append fails, keep tracing normally. The worst that will
                // happen is we end up overrecursing.
            }
        }
    }

    TraceChildren(trc, thing.asCell(), thing.kind());
}

void
gc::MarkCycleCollectorChildren(JSTracer* trc, ObjectGroup* group)
{
    MOZ_ASSERT(trc->isCallbackTracer());

    // Early return if this group is not required to be in an ObjectGroup chain.
    if (!group->maybeUnboxedLayout()) {
        TraceChildren(trc, group, JSTRACE_OBJECT_GROUP);
        return;
    }

    ObjectGroupCycleCollectorTracer groupTracer(trc->asCallbackTracer());
    TraceChildren(&groupTracer, group, JSTRACE_OBJECT_GROUP);

    while (!groupTracer.worklist.empty()) {
        ObjectGroup* innerGroup = groupTracer.worklist.popCopy();
        TraceChildren(&groupTracer, innerGroup, JSTRACE_OBJECT_GROUP);
    }
}

static void
ScanObjectGroup(GCMarker* gcmarker, ObjectGroup* group)
{
    unsigned count = group->getPropertyCount();
    for (unsigned i = 0; i < count; i++) {
        if (ObjectGroup::Property* prop = group->getProperty(i))
            DoMarking(gcmarker, prop->id.get());
    }

    if (group->proto().isObject())
        gcmarker->traverse(group->proto().toObject());

    group->compartment()->mark();

    if (GlobalObject* global = group->compartment()->unsafeUnbarrieredMaybeGlobal())
        gcmarker->traverse(global);

    if (group->newScript())
        group->newScript()->trace(gcmarker);

    if (group->maybePreliminaryObjects())
        group->maybePreliminaryObjects()->trace(gcmarker);

    if (group->maybeUnboxedLayout())
        group->unboxedLayout().trace(gcmarker);

    if (ObjectGroup* unboxedGroup = group->maybeOriginalUnboxedGroup())
        gcmarker->traverse(unboxedGroup);

    if (TypeDescr* descr = group->maybeTypeDescr())
        gcmarker->traverse(descr);

    if (JSFunction* fun = group->maybeInterpretedFunction())
        gcmarker->traverse(fun);
}

void
js::ObjectGroup::traceChildren(JSTracer* trc)
{
    unsigned count = getPropertyCount();
    for (unsigned i = 0; i < count; i++) {
        if (ObjectGroup::Property* prop = getProperty(i))
            TraceEdge(trc, &prop->id, "group_property");
    }

    if (proto().isObject())
        TraceEdge(trc, &protoRaw(), "group_proto");

    if (newScript())
        newScript()->trace(trc);

    if (maybePreliminaryObjects())
        maybePreliminaryObjects()->trace(trc);

    if (maybeUnboxedLayout())
        unboxedLayout().trace(trc);

    if (ObjectGroup* unboxedGroup = maybeOriginalUnboxedGroup()) {
        TraceManuallyBarrieredEdge(trc, &unboxedGroup, "group_original_unboxed_group");
        setOriginalUnboxedGroup(unboxedGroup);
    }

    if (JSObject* descr = maybeTypeDescr()) {
        TraceManuallyBarrieredEdge(trc, &descr, "group_type_descr");
        setTypeDescr(&descr->as<TypeDescr>());
    }

    if (JSObject* fun = maybeInterpretedFunction()) {
        TraceManuallyBarrieredEdge(trc, &fun, "group_function");
        setInterpretedFunction(&fun->as<JSFunction>());
    }
}

template<typename T>
static void
PushArenaTyped(GCMarker* gcmarker, ArenaHeader* aheader)
{
    for (ArenaCellIterUnderGC i(aheader); !i.done(); i.next())
        gcmarker->traverse(i.get<T>());
}

void
gc::PushArena(GCMarker* gcmarker, ArenaHeader* aheader)
{
    switch (MapAllocToTraceKind(aheader->getAllocKind())) {
      case JSTRACE_OBJECT:
        PushArenaTyped<JSObject>(gcmarker, aheader);
        break;

      case JSTRACE_SCRIPT:
        PushArenaTyped<JSScript>(gcmarker, aheader);
        break;

      case JSTRACE_STRING:
        PushArenaTyped<JSString>(gcmarker, aheader);
        break;

      case JSTRACE_SYMBOL:
        PushArenaTyped<JS::Symbol>(gcmarker, aheader);
        break;

      case JSTRACE_BASE_SHAPE:
        PushArenaTyped<js::BaseShape>(gcmarker, aheader);
        break;

      case JSTRACE_JITCODE:
        PushArenaTyped<js::jit::JitCode>(gcmarker, aheader);
        break;

      case JSTRACE_LAZY_SCRIPT:
        PushArenaTyped<LazyScript>(gcmarker, aheader);
        break;

      case JSTRACE_SHAPE:
        PushArenaTyped<js::Shape>(gcmarker, aheader);
        break;

      case JSTRACE_OBJECT_GROUP:
        PushArenaTyped<js::ObjectGroup>(gcmarker, aheader);
        break;

      default:
        MOZ_CRASH("Invalid trace kind in PushArena.");
    }
}

struct SlotArrayLayout
{
    union {
        HeapSlot* end;
        uintptr_t kind;
    };
    union {
        HeapSlot* start;
        uintptr_t index;
    };
    NativeObject* obj;

    static void staticAsserts() {
        /* This should have the same layout as three mark stack items. */
        JS_STATIC_ASSERT(sizeof(SlotArrayLayout) == 3 * sizeof(uintptr_t));
    }
};

/*
 * During incremental GC, we return from drainMarkStack without having processed
 * the entire stack. At that point, JS code can run and reallocate slot arrays
 * that are stored on the stack. To prevent this from happening, we replace all
 * ValueArrayTag stack items with SavedValueArrayTag. In the latter, slots
 * pointers are replaced with slot indexes, and slot array end pointers are
 * replaced with the kind of index (properties vs. elements).
 */
void
GCMarker::saveValueRanges()
{
    for (uintptr_t* p = stack.tos_; p > stack.stack_; ) {
        uintptr_t tag = *--p & StackTagMask;
        if (tag == ValueArrayTag) {
            *p &= ~StackTagMask;
            p -= 2;
            SlotArrayLayout* arr = reinterpret_cast<SlotArrayLayout*>(p);
            NativeObject* obj = arr->obj;
            MOZ_ASSERT(obj->isNative());

            HeapSlot* vp = obj->getDenseElementsAllowCopyOnWrite();
            if (arr->end == vp + obj->getDenseInitializedLength()) {
                MOZ_ASSERT(arr->start >= vp);
                arr->index = arr->start - vp;
                arr->kind = HeapSlot::Element;
            } else {
                HeapSlot* vp = obj->fixedSlots();
                unsigned nfixed = obj->numFixedSlots();
                if (arr->start == arr->end) {
                    arr->index = obj->slotSpan();
                } else if (arr->start >= vp && arr->start < vp + nfixed) {
                    MOZ_ASSERT(arr->end == vp + Min(nfixed, obj->slotSpan()));
                    arr->index = arr->start - vp;
                } else {
                    MOZ_ASSERT(arr->start >= obj->slots_ &&
                               arr->end == obj->slots_ + obj->slotSpan() - nfixed);
                    arr->index = (arr->start - obj->slots_) + nfixed;
                }
                arr->kind = HeapSlot::Slot;
            }
            p[2] |= SavedValueArrayTag;
        } else if (tag == SavedValueArrayTag) {
            p -= 2;
        }
    }
}

bool
GCMarker::restoreValueArray(NativeObject* obj, void** vpp, void** endp)
{
    uintptr_t start = stack.pop();
    HeapSlot::Kind kind = (HeapSlot::Kind) stack.pop();

    if (kind == HeapSlot::Element) {
        if (!obj->is<ArrayObject>())
            return false;

        uint32_t initlen = obj->getDenseInitializedLength();
        HeapSlot* vp = obj->getDenseElementsAllowCopyOnWrite();
        if (start < initlen) {
            *vpp = vp + start;
            *endp = vp + initlen;
        } else {
            /* The object shrunk, in which case no scanning is needed. */
            *vpp = *endp = vp;
        }
    } else {
        MOZ_ASSERT(kind == HeapSlot::Slot);
        HeapSlot* vp = obj->fixedSlots();
        unsigned nfixed = obj->numFixedSlots();
        unsigned nslots = obj->slotSpan();
        if (start < nslots) {
            if (start < nfixed) {
                *vpp = vp + start;
                *endp = vp + Min(nfixed, nslots);
            } else {
                *vpp = obj->slots_ + start - nfixed;
                *endp = obj->slots_ + nslots - nfixed;
            }
        } else {
            /* The object shrunk, in which case no scanning is needed. */
            *vpp = *endp = vp;
        }
    }

    MOZ_ASSERT(*vpp <= *endp);
    return true;
}

void
GCMarker::processMarkStackOther(uintptr_t tag, uintptr_t addr)
{
    if (tag == GroupTag) {
        ScanObjectGroup(this, reinterpret_cast<ObjectGroup*>(addr));
    } else if (tag == SavedValueArrayTag) {
        MOZ_ASSERT(!(addr & CellMask));
        NativeObject* obj = reinterpret_cast<NativeObject*>(addr);
        HeapValue* vp;
        HeapValue* end;
        if (restoreValueArray(obj, (void**)&vp, (void**)&end))
            pushValueArray(obj, vp, end);
        else
            repush(obj);
    } else if (tag == JitCodeTag) {
        reinterpret_cast<jit::JitCode*>(addr)->traceChildren(this);
    }
}

MOZ_ALWAYS_INLINE void
GCMarker::markAndScanString(JSObject* source, JSString* str)
{
    if (!str->isPermanentAtom()) {
        JS_COMPARTMENT_ASSERT(runtime(), str);
        MOZ_ASSERT(runtime()->isAtomsZone(str->zone()) || str->zone() == source->zone());
        if (str->markIfUnmarked())
            eagerlyMarkChildren(str);
    }
}

MOZ_ALWAYS_INLINE void
GCMarker::markAndScanSymbol(JSObject* source, JS::Symbol* sym)
{
    if (!sym->isWellKnownSymbol()) {
        JS_COMPARTMENT_ASSERT(runtime(), sym);
        MOZ_ASSERT(runtime()->isAtomsZone(sym->zone()) || sym->zone() == source->zone());
        if (sym->markIfUnmarked())
            eagerlyMarkChildren(sym);
    }
}

inline void
GCMarker::processMarkStackTop(SliceBudget& budget)
{
    /*
     * The function uses explicit goto and implements the scanning of the
     * object directly. It allows to eliminate the tail recursion and
     * significantly improve the marking performance, see bug 641025.
     */
    HeapSlot* vp;
    HeapSlot* end;
    JSObject* obj;

    const int32_t* unboxedTraceList;
    uint8_t* unboxedMemory;

    uintptr_t addr = stack.pop();
    uintptr_t tag = addr & StackTagMask;
    addr &= ~StackTagMask;

    if (tag == ValueArrayTag) {
        JS_STATIC_ASSERT(ValueArrayTag == 0);
        MOZ_ASSERT(!(addr & CellMask));
        obj = reinterpret_cast<JSObject*>(addr);
        uintptr_t addr2 = stack.pop();
        uintptr_t addr3 = stack.pop();
        MOZ_ASSERT(addr2 <= addr3);
        MOZ_ASSERT((addr3 - addr2) % sizeof(Value) == 0);
        vp = reinterpret_cast<HeapSlot*>(addr2);
        end = reinterpret_cast<HeapSlot*>(addr3);
        goto scan_value_array;
    }

    if (tag == ObjectTag) {
        obj = reinterpret_cast<JSObject*>(addr);
        JS_COMPARTMENT_ASSERT(runtime(), obj);
        goto scan_obj;
    }

    processMarkStackOther(tag, addr);
    return;

  scan_value_array:
    MOZ_ASSERT(vp <= end);
    while (vp != end) {
        budget.step();
        if (budget.isOverBudget()) {
            pushValueArray(obj, vp, end);
            return;
        }

        const Value& v = *vp++;
        if (v.isString()) {
            markAndScanString(obj, v.toString());
        } else if (v.isObject()) {
            JSObject* obj2 = &v.toObject();
            MOZ_ASSERT(obj->compartment() == obj2->compartment());
            if (mark(obj2)) {
                // Save the rest of this value array for later and start scanning obj2's children.N
                pushValueArray(obj, vp, end);
                obj = obj2;
                goto scan_obj;
            }
        } else if (v.isSymbol()) {
            markAndScanSymbol(obj, v.toSymbol());
        }
    }
    return;

  scan_unboxed:
    {
        while (*unboxedTraceList != -1) {
            JSString* str = *reinterpret_cast<JSString**>(unboxedMemory + *unboxedTraceList);
            markAndScanString(obj, str);
            unboxedTraceList++;
        }
        unboxedTraceList++;
        while (*unboxedTraceList != -1) {
            JSObject* obj2 = *reinterpret_cast<JSObject**>(unboxedMemory + *unboxedTraceList);
            MOZ_ASSERT_IF(obj2, obj->compartment() == obj2->compartment());
            if (obj2 && mark(obj2))
                repush(obj2);
            unboxedTraceList++;
        }
        unboxedTraceList++;
        while (*unboxedTraceList != -1) {
            const Value& v = *reinterpret_cast<Value*>(unboxedMemory + *unboxedTraceList);
            if (v.isString()) {
                markAndScanString(obj, v.toString());
            } else if (v.isObject()) {
                JSObject* obj2 = &v.toObject();
                MOZ_ASSERT(obj->compartment() == obj2->compartment());
                if (mark(obj2))
                    repush(obj2);
            } else if (v.isSymbol()) {
                markAndScanSymbol(obj, v.toSymbol());
            }
            unboxedTraceList++;
        }
        return;
    }

  scan_obj:
    {
        JS_COMPARTMENT_ASSERT(runtime(), obj);

        budget.step();
        if (budget.isOverBudget()) {
            repush(obj);
            return;
        }

        ObjectGroup* group = obj->groupFromGC();
        traverse(group);

        /* Call the trace hook if necessary. */
        const Class* clasp = group->clasp();
        if (clasp->trace) {
            // Global objects all have the same trace hook. That hook is safe without barriers
            // if the global has no custom trace hook of its own, or has been moved to a different
            // compartment, and so can't have one.
            MOZ_ASSERT_IF(!(clasp->trace == JS_GlobalObjectTraceHook &&
                            (!obj->compartment()->options().getTrace() || !obj->isOwnGlobal())),
                          clasp->flags & JSCLASS_IMPLEMENTS_BARRIERS);
            if (clasp->trace == InlineTypedObject::obj_trace) {
                Shape* shape = obj->as<InlineTypedObject>().shapeFromGC();
                traverse(shape);
                TypeDescr* descr = &obj->as<InlineTypedObject>().typeDescr();
                if (!descr->hasTraceList())
                    return;
                unboxedTraceList = descr->traceList();
                unboxedMemory = obj->as<InlineTypedObject>().inlineTypedMem();
                goto scan_unboxed;
            }
            if (clasp == &UnboxedPlainObject::class_) {
                JSObject* expando = obj->as<UnboxedPlainObject>().maybeExpando();
                if (expando && mark(expando))
                    repush(expando);
                const UnboxedLayout& layout = obj->as<UnboxedPlainObject>().layout();
                unboxedTraceList = layout.traceList();
                if (!unboxedTraceList)
                    return;
                unboxedMemory = obj->as<UnboxedPlainObject>().data();
                goto scan_unboxed;
            }
            clasp->trace(this, obj);
        }

        if (!clasp->isNative())
            return;

        NativeObject* nobj = &obj->as<NativeObject>();

        Shape* shape = nobj->lastProperty();
        traverse(shape);

        unsigned nslots = nobj->slotSpan();

        do {
            if (nobj->hasEmptyElements())
                break;

            if (nobj->denseElementsAreCopyOnWrite()) {
                JSObject* owner = nobj->getElementsHeader()->ownerObject();
                if (owner != nobj) {
                    traverse(owner);
                    break;
                }
            }

            vp = nobj->getDenseElementsAllowCopyOnWrite();
            end = vp + nobj->getDenseInitializedLength();
            if (!nslots)
                goto scan_value_array;
            pushValueArray(nobj, vp, end);
        } while (false);

        vp = nobj->fixedSlots();
        if (nobj->slots_) {
            unsigned nfixed = nobj->numFixedSlots();
            if (nslots > nfixed) {
                pushValueArray(nobj, vp, vp + nfixed);
                vp = nobj->slots_;
                end = vp + (nslots - nfixed);
                goto scan_value_array;
            }
        }
        MOZ_ASSERT(nslots <= nobj->numFixedSlots());
        end = vp + nslots;
        goto scan_value_array;
    }
}

bool
GCMarker::drainMarkStack(SliceBudget& budget)
{
#ifdef DEBUG
    struct AutoCheckCompartment {
        bool& flag;
        explicit AutoCheckCompartment(bool& comparmentCheckFlag) : flag(comparmentCheckFlag) {
            MOZ_ASSERT(!flag);
            flag = true;
        }
        ~AutoCheckCompartment() { flag = false; }
    } acc(strictCompartmentChecking);
#endif

    if (budget.isOverBudget())
        return false;

    for (;;) {
        while (!stack.isEmpty()) {
            processMarkStackTop(budget);
            if (budget.isOverBudget()) {
                saveValueRanges();
                return false;
            }
        }

        if (!hasDelayedChildren())
            break;

        /*
         * Mark children of things that caused too deep recursion during the
         * above tracing. Don't do this until we're done with everything
         * else.
         */
        if (!markDelayedChildren(budget)) {
            saveValueRanges();
            return false;
        }
    }

    return true;
}

struct TraceChildrenFunctor {
    template <typename T>
    void operator()(JSTracer* trc, void* thing) {
        static_cast<T*>(thing)->traceChildren(trc);
    }
};

void
js::TraceChildren(JSTracer* trc, void* thing, JSGCTraceKind kind)
{
    MOZ_ASSERT(thing);
    TraceChildrenFunctor f;
    CallTyped(f, kind, trc, thing);
}

#ifdef DEBUG
static void
AssertNonGrayGCThing(JS::CallbackTracer* trc, void** thingp, JSGCTraceKind kind)
{
    DebugOnly<Cell*> thing(static_cast<Cell*>(*thingp));
    MOZ_ASSERT_IF(thing->isTenured(), !thing->asTenured().isMarked(js::gc::GRAY));
}

template <typename T>
bool
gc::ZoneIsGCMarking(T* thing)
{
    return thing->zone()->isGCMarking();
}

template <typename T>
bool
js::gc::ZoneIsAtomsZoneForString(JSRuntime* rt, T* thing)
{
    JSGCTraceKind kind = GetGCThingTraceKind(thing);
    if (kind == JSTRACE_STRING || kind == JSTRACE_SYMBOL)
        return rt->isAtomsZone(thing->zone());
    return false;
}
#endif

static void
UnmarkGrayChildren(JS::CallbackTracer* trc, void** thingp, JSGCTraceKind kind);

struct UnmarkGrayTracer : public JS::CallbackTracer
{
    /*
     * We set eagerlyTraceWeakMaps to false because the cycle collector will fix
     * up any color mismatches involving weakmaps when it runs.
     */
    explicit UnmarkGrayTracer(JSRuntime* rt)
      : JS::CallbackTracer(rt, UnmarkGrayChildren, DoNotTraceWeakMaps),
        tracingShape(false),
        previousShape(nullptr),
        unmarkedAny(false)
    {}

    UnmarkGrayTracer(JSTracer* trc, bool tracingShape)
      : JS::CallbackTracer(trc->runtime(), UnmarkGrayChildren, DoNotTraceWeakMaps),
        tracingShape(tracingShape),
        previousShape(nullptr),
        unmarkedAny(false)
    {}

    /* True iff we are tracing the immediate children of a shape. */
    bool tracingShape;

    /* If tracingShape, shape child or nullptr. Otherwise, nullptr. */
    Shape* previousShape;

    /* Whether we unmarked anything. */
    bool unmarkedAny;
};

/*
 * The GC and CC are run independently. Consequently, the following sequence of
 * events can occur:
 * 1. GC runs and marks an object gray.
 * 2. Some JS code runs that creates a pointer from a JS root to the gray
 *    object. If we re-ran a GC at this point, the object would now be black.
 * 3. Now we run the CC. It may think it can collect the gray object, even
 *    though it's reachable from the JS heap.
 *
 * To prevent this badness, we unmark the gray bit of an object when it is
 * accessed by callers outside XPConnect. This would cause the object to go
 * black in step 2 above. This must be done on everything reachable from the
 * object being returned. The following code takes care of the recursive
 * re-coloring.
 *
 * There is an additional complication for certain kinds of edges that are not
 * contained explicitly in the source object itself, such as from a weakmap key
 * to its value, and from an object being watched by a watchpoint to the
 * watchpoint's closure. These "implicit edges" are represented in some other
 * container object, such as the weakmap or the watchpoint itself. In these
 * cases, calling unmark gray on an object won't find all of its children.
 *
 * Handling these implicit edges has two parts:
 * - A special pass enumerating all of the containers that know about the
 *   implicit edges to fix any black-gray edges that have been created. This
 *   is implemented in nsXPConnect::FixWeakMappingGrayBits.
 * - To prevent any incorrectly gray objects from escaping to live JS outside
 *   of the containers, we must add unmark-graying read barriers to these
 *   containers.
 */
static void
UnmarkGrayChildren(JS::CallbackTracer* trc, void** thingp, JSGCTraceKind kind)
{
    int stackDummy;
    if (!JS_CHECK_STACK_SIZE(trc->runtime()->mainThread.nativeStackLimit[StackForSystemCode],
                             &stackDummy))
    {
        /*
         * If we run out of stack, we take a more drastic measure: require that
         * we GC again before the next CC.
         */
        trc->runtime()->gc.setGrayBitsInvalid();
        return;
    }

    Cell* cell = static_cast<Cell*>(*thingp);

    // Cells in the nursery cannot be gray, and therefore must necessarily point
    // to only black edges.
    if (!cell->isTenured()) {
#ifdef DEBUG
        JS::CallbackTracer nongray(trc->runtime(), AssertNonGrayGCThing);
        TraceChildren(&nongray, cell, kind);
#endif
        return;
    }

    TenuredCell& tenured = cell->asTenured();
    if (!tenured.isMarked(js::gc::GRAY))
        return;
    tenured.unmark(js::gc::GRAY);

    UnmarkGrayTracer* tracer = static_cast<UnmarkGrayTracer*>(trc);
    tracer->unmarkedAny = true;

    // Trace children of |tenured|. If |tenured| and its parent are both
    // shapes, |tenured| will get saved to mPreviousShape without being traced.
    // The parent will later trace |tenured|. This is done to avoid increasing
    // the stack depth during shape tracing. It is safe to do because a shape
    // can only have one child that is a shape.
    UnmarkGrayTracer childTracer(tracer, kind == JSTRACE_SHAPE);

    if (kind != JSTRACE_SHAPE) {
        TraceChildren(&childTracer, &tenured, kind);
        MOZ_ASSERT(!childTracer.previousShape);
        tracer->unmarkedAny |= childTracer.unmarkedAny;
        return;
    }

    MOZ_ASSERT(kind == JSTRACE_SHAPE);
    Shape* shape = static_cast<Shape*>(&tenured);
    if (tracer->tracingShape) {
        MOZ_ASSERT(!tracer->previousShape);
        tracer->previousShape = shape;
        return;
    }

    do {
        MOZ_ASSERT(!shape->isMarked(js::gc::GRAY));
        TraceChildren(&childTracer, shape, JSTRACE_SHAPE);
        shape = childTracer.previousShape;
        childTracer.previousShape = nullptr;
    } while (shape);
    tracer->unmarkedAny |= childTracer.unmarkedAny;
}

bool
js::UnmarkGrayCellRecursively(gc::Cell* cell, JSGCTraceKind kind)
{
    MOZ_ASSERT(cell);

    JSRuntime* rt = cell->runtimeFromMainThread();

    // When the ReadBarriered type is used in a HashTable, it is difficult or
    // impossible to suppress the implicit cast operator while iterating for GC.
    if (rt->isHeapBusy())
        return false;

    bool unmarkedArg = false;
    if (cell->isTenured()) {
        if (!cell->asTenured().isMarked(GRAY))
            return false;

        cell->asTenured().unmark(GRAY);
        unmarkedArg = true;
    }

    UnmarkGrayTracer trc(rt);
    TraceChildren(&trc, cell, kind);

    return unmarkedArg || trc.unmarkedAny;
}

bool
js::UnmarkGrayShapeRecursively(Shape* shape)
{
    return js::UnmarkGrayCellRecursively(shape, JSTRACE_SHAPE);
}

JS_FRIEND_API(bool)
JS::UnmarkGrayGCThingRecursively(JS::GCCellPtr thing)
{
    return js::UnmarkGrayCellRecursively(thing.asCell(), thing.kind());
}
