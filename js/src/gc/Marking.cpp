/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Marking.h"

#include "mozilla/DebugOnly.h"

#include "jit/IonCode.h"
#include "vm/ArgumentsObject.h"
#include "vm/Shape.h"
#include "vm/TypedArrayObject.h"

#include "jscompartmentinlines.h"
#include "jsinferinlines.h"

#include "gc/Nursery-inl.h"
#include "vm/Shape-inl.h"
#include "vm/String-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::DebugOnly;

void * const js::NullPtr::constNullValue = NULL;

JS_PUBLIC_DATA(void * const) JS::NullPtr::constNullValue = NULL;

/*
 * There are two mostly separate mark paths. The first is a fast path used
 * internally in the GC. The second is a slow path used for root marking and
 * for API consumers like the cycle collector or Class::trace implementations.
 *
 * The fast path uses explicit stacks. The basic marking process during a GC is
 * that all roots are pushed on to a mark stack, and then each item on the
 * stack is scanned (possibly pushing more stuff) until the stack is empty.
 *
 * PushMarkStack pushes a GC thing onto the mark stack. In some cases (shapes
 * or strings) it eagerly marks the object rather than pushing it. Popping and
 * scanning is done by the processMarkStackTop method. For efficiency reasons
 * like tail recursion elimination that method also implements the scanning of
 * objects. For other GC things it uses helper methods.
 *
 * Most of the marking code outside Marking.cpp uses functions like MarkObject,
 * MarkString, etc. These functions check if an object is in the compartment
 * currently being GCed. If it is, they call PushMarkStack. Roots are pushed
 * this way as well as pointers traversed inside trace hooks (for things like
 * PropertyIteratorObjects). It is always valid to call a MarkX function
 * instead of PushMarkStack, although it may be slower.
 *
 * The MarkX functions also handle non-GC object traversal. In this case, they
 * call a callback for each object visited. This is a recursive process; the
 * mark stacks are not involved. These callbacks may ask for the outgoing
 * pointers to be visited. Eventually, this leads to the MarkChildren functions
 * being called. These functions duplicate much of the functionality of
 * scanning functions, but they don't push onto an explicit stack.
 */

static inline void
PushMarkStack(GCMarker *gcmarker, JSObject *thing);

static inline void
PushMarkStack(GCMarker *gcmarker, JSFunction *thing);

static inline void
PushMarkStack(GCMarker *gcmarker, JSScript *thing);

static inline void
PushMarkStack(GCMarker *gcmarker, Shape *thing);

static inline void
PushMarkStack(GCMarker *gcmarker, JSString *thing);

static inline void
PushMarkStack(GCMarker *gcmarker, types::TypeObject *thing);

namespace js {
namespace gc {

static void MarkChildren(JSTracer *trc, JSString *str);
static void MarkChildren(JSTracer *trc, JSScript *script);
static void MarkChildren(JSTracer *trc, LazyScript *lazy);
static void MarkChildren(JSTracer *trc, Shape *shape);
static void MarkChildren(JSTracer *trc, BaseShape *base);
static void MarkChildren(JSTracer *trc, types::TypeObject *type);
static void MarkChildren(JSTracer *trc, jit::IonCode *code);

} /* namespace gc */
} /* namespace js */

/*** Object Marking ***/

#ifdef DEBUG
template<typename T>
static inline bool
IsThingPoisoned(T *thing)
{
    const uint8_t pb = JS_FREE_PATTERN;
    const uint32_t pw = pb | (pb << 8) | (pb << 16) | (pb << 24);
    JS_STATIC_ASSERT(sizeof(T) >= sizeof(FreeSpan) + sizeof(uint32_t));
    uint32_t *p =
        reinterpret_cast<uint32_t *>(reinterpret_cast<FreeSpan *>(thing) + 1);
    return *p == pw;
}
#endif

static GCMarker *
AsGCMarker(JSTracer *trc)
{
    JS_ASSERT(IS_GC_MARKING_TRACER(trc));
    return static_cast<GCMarker *>(trc);
}

template<typename T>
static inline void
CheckMarkedThing(JSTracer *trc, T *thing)
{
#ifdef DEBUG
    JS_ASSERT(trc);
    JS_ASSERT(thing);

    /* This function uses data that's not available in the nursery. */
    if (IsInsideNursery(trc->runtime, thing))
        return;

    JS_ASSERT(thing->zone());
    JS_ASSERT(thing->zone()->runtimeFromMainThread() == trc->runtime);
    JS_ASSERT(trc->debugPrinter || trc->debugPrintArg);

    DebugOnly<JSRuntime *> rt = trc->runtime;

    JS_ASSERT_IF(IS_GC_MARKING_TRACER(trc) && rt->gcManipulatingDeadZones,
                 !thing->zone()->scheduledForDestruction);

    JS_ASSERT(CurrentThreadCanAccessRuntime(rt));

    JS_ASSERT_IF(thing->zone()->requireGCTracer(),
                 IS_GC_MARKING_TRACER(trc));

    JS_ASSERT(thing->isAligned());

    JS_ASSERT(MapTypeToTraceKind<T>::kind == GetGCThingTraceKind(thing));

    JS_ASSERT_IF(rt->gcStrictCompartmentChecking,
                 thing->zone()->isCollecting() || rt->isAtomsZone(thing->zone()));

    JS_ASSERT_IF(IS_GC_MARKING_TRACER(trc) && AsGCMarker(trc)->getMarkColor() == GRAY,
                 thing->zone()->isGCMarkingGray() || rt->isAtomsZone(thing->zone()));

    /*
     * Try to assert that the thing is allocated.  This is complicated by the
     * fact that allocated things may still contain the poison pattern if that
     * part has not been overwritten, and that the free span list head in the
     * ArenaHeader may not be synced with the real one in ArenaLists.
     */
    JS_ASSERT_IF(IsThingPoisoned(thing) && rt->isHeapBusy(),
                 !InFreeList(thing->arenaHeader(), thing));
#endif
}

template<typename T>
static void
MarkInternal(JSTracer *trc, T **thingp)
{
    JS_ASSERT(thingp);
    T *thing = *thingp;

    CheckMarkedThing(trc, thing);

    if (!trc->callback) {
        /*
         * We may mark a Nursery thing outside the context of the
         * MinorCollectionTracer because of a pre-barrier. The pre-barrier is
         * not needed in this case because we perform a minor collection before
         * each incremental slice.
         */
        if (IsInsideNursery(trc->runtime, thing))
            return;

        /*
         * Don't mark things outside a compartment if we are in a
         * per-compartment GC.
         */
        if (!thing->zone()->isGCMarking())
            return;

        PushMarkStack(AsGCMarker(trc), thing);
        thing->zone()->maybeAlive = true;
    } else {
        trc->callback(trc, (void **)thingp, MapTypeToTraceKind<T>::kind);
        JS_UNSET_TRACING_LOCATION(trc);
    }

    trc->debugPrinter = NULL;
    trc->debugPrintArg = NULL;
}

#define JS_ROOT_MARKING_ASSERT(trc)                                     \
    JS_ASSERT_IF(IS_GC_MARKING_TRACER(trc),                             \
                 trc->runtime->gcIncrementalState == NO_INCREMENTAL ||  \
                 trc->runtime->gcIncrementalState == MARK_ROOTS);

template <typename T>
static void
MarkUnbarriered(JSTracer *trc, T **thingp, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkInternal(trc, thingp);
}

namespace js {
namespace gc {

template <typename T>
static void
Mark(JSTracer *trc, EncapsulatedPtr<T> *thing, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkInternal(trc, thing->unsafeGet());
}

} /* namespace gc */
} /* namespace js */

template <typename T>
static void
MarkRoot(JSTracer *trc, T **thingp, const char *name)
{
    JS_ROOT_MARKING_ASSERT(trc);
    JS_SET_TRACING_NAME(trc, name);
    MarkInternal(trc, thingp);
}

template <typename T>
static void
MarkRange(JSTracer *trc, size_t len, HeapPtr<T> *vec, const char *name)
{
    for (size_t i = 0; i < len; ++i) {
        if (vec[i].get()) {
            JS_SET_TRACING_INDEX(trc, name, i);
            MarkInternal(trc, vec[i].unsafeGet());
        }
    }
}

template <typename T>
static void
MarkRootRange(JSTracer *trc, size_t len, T **vec, const char *name)
{
    JS_ROOT_MARKING_ASSERT(trc);
    for (size_t i = 0; i < len; ++i) {
        if (vec[i]) {
            JS_SET_TRACING_INDEX(trc, name, i);
            MarkInternal(trc, &vec[i]);
        }
    }
}

namespace js {
namespace gc {

template <typename T>
static bool
IsMarked(T **thingp)
{
    JS_ASSERT(thingp);
    JS_ASSERT(*thingp);
#ifdef JSGC_GENERATIONAL
    Nursery &nursery = (*thingp)->runtimeFromMainThread()->gcNursery;
    if (nursery.isInside(*thingp))
        return nursery.getForwardedPointer(thingp);
#endif
    Zone *zone = (*thingp)->tenuredZone();
    if (!zone->isCollecting() || zone->isGCFinished())
        return true;
    return (*thingp)->isMarked();
}

template <typename T>
static bool
IsAboutToBeFinalized(T **thingp)
{
    JS_ASSERT(thingp);
    JS_ASSERT(*thingp);

#ifdef JSGC_GENERATIONAL
    Nursery &nursery = (*thingp)->runtimeFromMainThread()->gcNursery;
    if (nursery.isInside(*thingp))
        return !nursery.getForwardedPointer(thingp);
#endif
    if (!(*thingp)->tenuredZone()->isGCSweeping())
        return false;

    /*
     * We should return false for things that have been allocated during
     * incremental sweeping, but this possibility doesn't occur at the moment
     * because this function is only called at the very start of the sweeping a
     * compartment group.  Rather than do the extra check, we just assert that
     * it's not necessary.
     */
    JS_ASSERT(!(*thingp)->arenaHeader()->allocatedDuringIncremental);

    return !(*thingp)->isMarked();
}

#define DeclMarkerImpl(base, type)                                                                \
void                                                                                              \
Mark##base(JSTracer *trc, EncapsulatedPtr<type> *thing, const char *name)                         \
{                                                                                                 \
    Mark<type>(trc, thing, name);                                                                 \
}                                                                                                 \
                                                                                                  \
void                                                                                              \
Mark##base##Root(JSTracer *trc, type **thingp, const char *name)                                  \
{                                                                                                 \
    MarkRoot<type>(trc, thingp, name);                                                            \
}                                                                                                 \
                                                                                                  \
void                                                                                              \
Mark##base##Unbarriered(JSTracer *trc, type **thingp, const char *name)                           \
{                                                                                                 \
    MarkUnbarriered<type>(trc, thingp, name);                                                     \
}                                                                                                 \
                                                                                                  \
void                                                                                              \
Mark##base##Range(JSTracer *trc, size_t len, HeapPtr<type> *vec, const char *name)                \
{                                                                                                 \
    MarkRange<type>(trc, len, vec, name);                                                         \
}                                                                                                 \
                                                                                                  \
void                                                                                              \
Mark##base##RootRange(JSTracer *trc, size_t len, type **vec, const char *name)                    \
{                                                                                                 \
    MarkRootRange<type>(trc, len, vec, name);                                                     \
}                                                                                                 \
                                                                                                  \
bool                                                                                              \
Is##base##Marked(type **thingp)                                                                   \
{                                                                                                 \
    return IsMarked<type>(thingp);                                                                \
}                                                                                                 \
                                                                                                  \
bool                                                                                              \
Is##base##Marked(EncapsulatedPtr<type> *thingp)                                                   \
{                                                                                                 \
    return IsMarked<type>(thingp->unsafeGet());                                                   \
}                                                                                                 \
                                                                                                  \
bool                                                                                              \
Is##base##AboutToBeFinalized(type **thingp)                                                       \
{                                                                                                 \
    return IsAboutToBeFinalized<type>(thingp);                                                    \
}                                                                                                 \
                                                                                                  \
bool                                                                                              \
Is##base##AboutToBeFinalized(EncapsulatedPtr<type> *thingp)                                       \
{                                                                                                 \
    return IsAboutToBeFinalized<type>(thingp->unsafeGet());                                       \
}

DeclMarkerImpl(BaseShape, BaseShape)
DeclMarkerImpl(BaseShape, UnownedBaseShape)
DeclMarkerImpl(IonCode, jit::IonCode)
DeclMarkerImpl(Object, ArgumentsObject)
DeclMarkerImpl(Object, ArrayBufferObject)
DeclMarkerImpl(Object, ArrayBufferViewObject)
DeclMarkerImpl(Object, DebugScopeObject)
DeclMarkerImpl(Object, GlobalObject)
DeclMarkerImpl(Object, JSObject)
DeclMarkerImpl(Object, JSFunction)
DeclMarkerImpl(Object, ScopeObject)
DeclMarkerImpl(Script, JSScript)
DeclMarkerImpl(LazyScript, LazyScript)
DeclMarkerImpl(Shape, Shape)
DeclMarkerImpl(String, JSAtom)
DeclMarkerImpl(String, JSString)
DeclMarkerImpl(String, JSFlatString)
DeclMarkerImpl(String, JSLinearString)
DeclMarkerImpl(String, PropertyName)
DeclMarkerImpl(TypeObject, js::types::TypeObject)

} /* namespace gc */
} /* namespace js */

/*** Externally Typed Marking ***/

void
gc::MarkKind(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    JS_ASSERT(thingp);
    JS_ASSERT(*thingp);
    DebugOnly<Cell *> cell = static_cast<Cell *>(*thingp);
    JS_ASSERT_IF(cell->isTenured(), kind == MapAllocToTraceKind(cell->tenuredGetAllocKind()));
    switch (kind) {
      case JSTRACE_OBJECT:
        MarkInternal(trc, reinterpret_cast<JSObject **>(thingp));
        break;
      case JSTRACE_STRING:
        MarkInternal(trc, reinterpret_cast<JSString **>(thingp));
        break;
      case JSTRACE_SCRIPT:
        MarkInternal(trc, reinterpret_cast<JSScript **>(thingp));
        break;
      case JSTRACE_LAZY_SCRIPT:
        MarkInternal(trc, reinterpret_cast<LazyScript **>(thingp));
        break;
      case JSTRACE_SHAPE:
        MarkInternal(trc, reinterpret_cast<Shape **>(thingp));
        break;
      case JSTRACE_BASE_SHAPE:
        MarkInternal(trc, reinterpret_cast<BaseShape **>(thingp));
        break;
      case JSTRACE_TYPE_OBJECT:
        MarkInternal(trc, reinterpret_cast<types::TypeObject **>(thingp));
        break;
      case JSTRACE_IONCODE:
        MarkInternal(trc, reinterpret_cast<jit::IonCode **>(thingp));
        break;
    }
}

static void
MarkGCThingInternal(JSTracer *trc, void **thingp, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    JS_ASSERT(thingp);
    if (!*thingp)
        return;
    MarkKind(trc, thingp, GetGCThingTraceKind(*thingp));
}

void
gc::MarkGCThingRoot(JSTracer *trc, void **thingp, const char *name)
{
    JS_ROOT_MARKING_ASSERT(trc);
    MarkGCThingInternal(trc, thingp, name);
}

void
gc::MarkGCThingUnbarriered(JSTracer *trc, void **thingp, const char *name)
{
    MarkGCThingInternal(trc, thingp, name);
}

/*** ID Marking ***/

static inline void
MarkIdInternal(JSTracer *trc, jsid *id)
{
    if (JSID_IS_STRING(*id)) {
        JSString *str = JSID_TO_STRING(*id);
        JS_SET_TRACING_LOCATION(trc, (void *)id);
        MarkInternal(trc, &str);
        *id = NON_INTEGER_ATOM_TO_JSID(reinterpret_cast<JSAtom *>(str));
    } else if (JS_UNLIKELY(JSID_IS_OBJECT(*id))) {
        JSObject *obj = JSID_TO_OBJECT(*id);
        JS_SET_TRACING_LOCATION(trc, (void *)id);
        MarkInternal(trc, &obj);
        *id = OBJECT_TO_JSID(obj);
    } else {
        /* Unset realLocation manually if we do not call MarkInternal. */
        JS_UNSET_TRACING_LOCATION(trc);
    }
}

void
gc::MarkId(JSTracer *trc, EncapsulatedId *id, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkIdInternal(trc, id->unsafeGet());
}

void
gc::MarkIdRoot(JSTracer *trc, jsid *id, const char *name)
{
    JS_ROOT_MARKING_ASSERT(trc);
    JS_SET_TRACING_NAME(trc, name);
    MarkIdInternal(trc, id);
}

void
gc::MarkIdUnbarriered(JSTracer *trc, jsid *id, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkIdInternal(trc, id);
}

void
gc::MarkIdRange(JSTracer *trc, size_t len, HeapId *vec, const char *name)
{
    for (size_t i = 0; i < len; ++i) {
        JS_SET_TRACING_INDEX(trc, name, i);
        MarkIdInternal(trc, vec[i].unsafeGet());
    }
}

void
gc::MarkIdRootRange(JSTracer *trc, size_t len, jsid *vec, const char *name)
{
    JS_ROOT_MARKING_ASSERT(trc);
    for (size_t i = 0; i < len; ++i) {
        JS_SET_TRACING_INDEX(trc, name, i);
        MarkIdInternal(trc, &vec[i]);
    }
}

/*** Value Marking ***/

static inline void
MarkValueInternal(JSTracer *trc, Value *v)
{
    if (v->isMarkable()) {
        JS_ASSERT(v->toGCThing());
        void *thing = v->toGCThing();
        JS_SET_TRACING_LOCATION(trc, (void *)v);
        MarkKind(trc, &thing, v->gcKind());
        if (v->isString())
            v->setString((JSString *)thing);
        else
            v->setObjectOrNull((JSObject *)thing);
    } else {
        /* Unset realLocation manually if we do not call MarkInternal. */
        JS_UNSET_TRACING_LOCATION(trc);
    }
}

void
gc::MarkValue(JSTracer *trc, EncapsulatedValue *v, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkValueInternal(trc, v->unsafeGet());
}

void
gc::MarkValueRoot(JSTracer *trc, Value *v, const char *name)
{
    JS_ROOT_MARKING_ASSERT(trc);
    JS_SET_TRACING_NAME(trc, name);
    MarkValueInternal(trc, v);
}

void
gc::MarkTypeRoot(JSTracer *trc, types::Type *v, const char *name)
{
    JS_ROOT_MARKING_ASSERT(trc);
    JS_SET_TRACING_NAME(trc, name);
    if (v->isSingleObject()) {
        JSObject *obj = v->singleObject();
        MarkInternal(trc, &obj);
        *v = types::Type::ObjectType(obj);
    } else if (v->isTypeObject()) {
        types::TypeObject *typeObj = v->typeObject();
        MarkInternal(trc, &typeObj);
        *v = types::Type::ObjectType(typeObj);
    }
}

void
gc::MarkValueRange(JSTracer *trc, size_t len, EncapsulatedValue *vec, const char *name)
{
    for (size_t i = 0; i < len; ++i) {
        JS_SET_TRACING_INDEX(trc, name, i);
        MarkValueInternal(trc, vec[i].unsafeGet());
    }
}

void
gc::MarkValueRootRange(JSTracer *trc, size_t len, Value *vec, const char *name)
{
    JS_ROOT_MARKING_ASSERT(trc);
    for (size_t i = 0; i < len; ++i) {
        JS_SET_TRACING_INDEX(trc, name, i);
        MarkValueInternal(trc, &vec[i]);
    }
}

bool
gc::IsValueMarked(Value *v)
{
    JS_ASSERT(v->isMarkable());
    bool rv;
    if (v->isString()) {
        JSString *str = (JSString *)v->toGCThing();
        rv = IsMarked<JSString>(&str);
        v->setString(str);
    } else {
        JSObject *obj = (JSObject *)v->toGCThing();
        rv = IsMarked<JSObject>(&obj);
        v->setObject(*obj);
    }
    return rv;
}

bool
gc::IsValueAboutToBeFinalized(Value *v)
{
    JS_ASSERT(v->isMarkable());
    bool rv;
    if (v->isString()) {
        JSString *str = (JSString *)v->toGCThing();
        rv = IsAboutToBeFinalized<JSString>(&str);
        v->setString(str);
    } else {
        JSObject *obj = (JSObject *)v->toGCThing();
        rv = IsAboutToBeFinalized<JSObject>(&obj);
        v->setObject(*obj);
    }
    return rv;
}

/*** Slot Marking ***/

bool
gc::IsSlotMarked(HeapSlot *s)
{
    return IsMarked(s);
}

void
gc::MarkSlot(JSTracer *trc, HeapSlot *s, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkValueInternal(trc, s->unsafeGet());
}

void
gc::MarkArraySlots(JSTracer *trc, size_t len, HeapSlot *vec, const char *name)
{
    for (size_t i = 0; i < len; ++i) {
        JS_SET_TRACING_INDEX(trc, name, i);
        MarkValueInternal(trc, vec[i].unsafeGet());
    }
}

void
gc::MarkObjectSlots(JSTracer *trc, JSObject *obj, uint32_t start, uint32_t nslots)
{
    JS_ASSERT(obj->isNative());
    for (uint32_t i = start; i < (start + nslots); ++i) {
        JS_SET_TRACING_DETAILS(trc, js_GetObjectSlotName, obj, i);
        MarkValueInternal(trc, obj->nativeGetSlotRef(i).unsafeGet());
    }
}

static bool
ShouldMarkCrossCompartment(JSTracer *trc, JSObject *src, Cell *cell)
{
    if (!IS_GC_MARKING_TRACER(trc))
        return true;

    uint32_t color = AsGCMarker(trc)->getMarkColor();
    JS_ASSERT(color == BLACK || color == GRAY);

    if (IsInsideNursery(trc->runtime, cell)) {
        JS_ASSERT(color == BLACK);
        return false;
    }

    JS::Zone *zone = cell->tenuredZone();
    if (color == BLACK) {
        /*
         * Having black->gray edges violates our promise to the cycle
         * collector. This can happen if we're collecting a compartment and it
         * has an edge to an uncollected compartment: it's possible that the
         * source and destination of the cross-compartment edge should be gray,
         * but the source was marked black by the conservative scanner.
         */
        if (cell->isMarked(GRAY)) {
            JS_ASSERT(!zone->isCollecting());
            trc->runtime->gcFoundBlackGrayEdges = true;
        }
        return zone->isGCMarking();
    } else {
        if (zone->isGCMarkingBlack()) {
            /*
             * The destination compartment is being not being marked gray now,
             * but it will be later, so record the cell so it can be marked gray
             * at the appropriate time.
             */
            if (!cell->isMarked())
                DelayCrossCompartmentGrayMarking(src);
            return false;
        }
        return zone->isGCMarkingGray();
    }
}

void
gc::MarkCrossCompartmentObjectUnbarriered(JSTracer *trc, JSObject *src, JSObject **dst, const char *name)
{
    if (ShouldMarkCrossCompartment(trc, src, *dst))
        MarkObjectUnbarriered(trc, dst, name);
}

void
gc::MarkCrossCompartmentScriptUnbarriered(JSTracer *trc, JSObject *src, JSScript **dst,
                                          const char *name)
{
    if (ShouldMarkCrossCompartment(trc, src, *dst))
        MarkScriptUnbarriered(trc, dst, name);
}

void
gc::MarkCrossCompartmentSlot(JSTracer *trc, JSObject *src, HeapSlot *dst, const char *name)
{
    if (dst->isMarkable() && ShouldMarkCrossCompartment(trc, src, (Cell *)dst->toGCThing()))
        MarkSlot(trc, dst, name);
}

/*** Special Marking ***/

void
gc::MarkObject(JSTracer *trc, HeapPtr<GlobalObject, JSScript *> *thingp, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkInternal(trc, thingp->unsafeGet());
}

void
gc::MarkValueUnbarriered(JSTracer *trc, Value *v, const char *name)
{
    JS_SET_TRACING_NAME(trc, name);
    MarkValueInternal(trc, v);
}

bool
gc::IsCellMarked(Cell **thingp)
{
    return IsMarked<Cell>(thingp);
}

bool
gc::IsCellAboutToBeFinalized(Cell **thingp)
{
    return IsAboutToBeFinalized<Cell>(thingp);
}

/*** Push Mark Stack ***/

#define JS_COMPARTMENT_ASSERT(rt, thing)                                \
    JS_ASSERT((thing)->zone()->isGCMarking())

#define JS_COMPARTMENT_ASSERT_STR(rt, thing)                            \
    JS_ASSERT((thing)->zone()->isGCMarking() ||                         \
              (rt)->isAtomsZone((thing)->zone()));

static void
PushMarkStack(GCMarker *gcmarker, JSObject *thing)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime, thing);
    JS_ASSERT(!IsInsideNursery(gcmarker->runtime, thing));

    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushObject(thing);
}

/*
 * PushMarkStack for BaseShape unpacks its children directly onto the mark
 * stack. For a pre-barrier between incremental slices, this may result in
 * objects in the nursery getting pushed onto the mark stack. It is safe to
 * ignore these objects because they will be marked by the matching
 * post-barrier during the minor GC at the start of each incremental slice.
 */
static void
MaybePushMarkStackBetweenSlices(GCMarker *gcmarker, JSObject *thing)
{
    JSRuntime *rt = gcmarker->runtime;
    JS_COMPARTMENT_ASSERT(rt, thing);
    JS_ASSERT_IF(rt->isHeapBusy(), !IsInsideNursery(rt, thing));

    if (!IsInsideNursery(rt, thing) && thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushObject(thing);
}

static void
PushMarkStack(GCMarker *gcmarker, JSFunction *thing)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime, thing);
    JS_ASSERT(!IsInsideNursery(gcmarker->runtime, thing));

    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushObject(thing);
}

static void
PushMarkStack(GCMarker *gcmarker, types::TypeObject *thing)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime, thing);
    JS_ASSERT(!IsInsideNursery(gcmarker->runtime, thing));

    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushType(thing);
}

static void
PushMarkStack(GCMarker *gcmarker, JSScript *thing)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime, thing);
    JS_ASSERT(!IsInsideNursery(gcmarker->runtime, thing));

    /*
     * We mark scripts directly rather than pushing on the stack as they can
     * refer to other scripts only indirectly (like via nested functions) and
     * we cannot get to deep recursion.
     */
    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        MarkChildren(gcmarker, thing);
}

static void
PushMarkStack(GCMarker *gcmarker, LazyScript *thing)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime, thing);
    JS_ASSERT(!IsInsideNursery(gcmarker->runtime, thing));

    /*
     * We mark lazy scripts directly rather than pushing on the stack as they
     * only refer to normal scripts and to strings, and cannot recurse.
     */
    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        MarkChildren(gcmarker, thing);
}

static void
ScanShape(GCMarker *gcmarker, Shape *shape);

static void
PushMarkStack(GCMarker *gcmarker, Shape *thing)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime, thing);
    JS_ASSERT(!IsInsideNursery(gcmarker->runtime, thing));

    /* We mark shapes directly rather than pushing on the stack. */
    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        ScanShape(gcmarker, thing);
}

static void
PushMarkStack(GCMarker *gcmarker, jit::IonCode *thing)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime, thing);
    JS_ASSERT(!IsInsideNursery(gcmarker->runtime, thing));

    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        gcmarker->pushIonCode(thing);
}

static inline void
ScanBaseShape(GCMarker *gcmarker, BaseShape *base);

static void
PushMarkStack(GCMarker *gcmarker, BaseShape *thing)
{
    JS_COMPARTMENT_ASSERT(gcmarker->runtime, thing);
    JS_ASSERT(!IsInsideNursery(gcmarker->runtime, thing));

    /* We mark base shapes directly rather than pushing on the stack. */
    if (thing->markIfUnmarked(gcmarker->getMarkColor()))
        ScanBaseShape(gcmarker, thing);
}

static void
ScanShape(GCMarker *gcmarker, Shape *shape)
{
  restart:
    PushMarkStack(gcmarker, shape->base());

    const EncapsulatedId &id = shape->propidRef();
    if (JSID_IS_STRING(id))
        PushMarkStack(gcmarker, JSID_TO_STRING(id));
    else if (JS_UNLIKELY(JSID_IS_OBJECT(id)))
        PushMarkStack(gcmarker, JSID_TO_OBJECT(id));

    shape = shape->previous();
    if (shape && shape->markIfUnmarked(gcmarker->getMarkColor()))
        goto restart;
}

static inline void
ScanBaseShape(GCMarker *gcmarker, BaseShape *base)
{
    base->assertConsistency();

    base->compartment()->mark();

    if (base->hasGetterObject())
        MaybePushMarkStackBetweenSlices(gcmarker, base->getterObject());

    if (base->hasSetterObject())
        MaybePushMarkStackBetweenSlices(gcmarker, base->setterObject());

    if (JSObject *parent = base->getObjectParent()) {
        MaybePushMarkStackBetweenSlices(gcmarker, parent);
    } else if (GlobalObject *global = base->compartment()->maybeGlobal()) {
        PushMarkStack(gcmarker, global);
    }

    if (JSObject *metadata = base->getObjectMetadata())
        MaybePushMarkStackBetweenSlices(gcmarker, metadata);

    /*
     * All children of the owned base shape are consistent with its
     * unowned one, thus we do not need to trace through children of the
     * unowned base shape.
     */
    if (base->isOwned()) {
        UnownedBaseShape *unowned = base->baseUnowned();
        JS_ASSERT(base->compartment() == unowned->compartment());
        unowned->markIfUnmarked(gcmarker->getMarkColor());
    }
}

static inline void
ScanLinearString(GCMarker *gcmarker, JSLinearString *str)
{
    JS_COMPARTMENT_ASSERT_STR(gcmarker->runtime, str);
    JS_ASSERT(str->isMarked());

    /*
     * Add extra asserts to confirm the static type to detect incorrect string
     * mutations.
     */
    JS_ASSERT(str->JSString::isLinear());
    while (str->hasBase()) {
        str = str->base();
        JS_ASSERT(str->JSString::isLinear());
        JS_COMPARTMENT_ASSERT_STR(gcmarker->runtime, str);
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
ScanRope(GCMarker *gcmarker, JSRope *rope)
{
    ptrdiff_t savedPos = gcmarker->stack.position();
    JS_DIAGNOSTICS_ASSERT(GetGCThingTraceKind(rope) == JSTRACE_STRING);
    for (;;) {
        JS_DIAGNOSTICS_ASSERT(GetGCThingTraceKind(rope) == JSTRACE_STRING);
        JS_DIAGNOSTICS_ASSERT(rope->JSString::isRope());
        JS_COMPARTMENT_ASSERT_STR(gcmarker->runtime, rope);
        JS_ASSERT(rope->isMarked());
        JSRope *next = NULL;

        JSString *right = rope->rightChild();
        if (right->markIfUnmarked()) {
            if (right->isLinear())
                ScanLinearString(gcmarker, &right->asLinear());
            else
                next = &right->asRope();
        }

        JSString *left = rope->leftChild();
        if (left->markIfUnmarked()) {
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
            JS_ASSERT(savedPos < gcmarker->stack.position());
            rope = reinterpret_cast<JSRope *>(gcmarker->stack.pop());
        } else {
            break;
        }
    }
    JS_ASSERT(savedPos == gcmarker->stack.position());
 }

static inline void
ScanString(GCMarker *gcmarker, JSString *str)
{
    if (str->isLinear())
        ScanLinearString(gcmarker, &str->asLinear());
    else
        ScanRope(gcmarker, &str->asRope());
}

static inline void
PushMarkStack(GCMarker *gcmarker, JSString *str)
{
    JS_COMPARTMENT_ASSERT_STR(gcmarker->runtime, str);

    /*
     * As string can only refer to other strings we fully scan its GC graph
     * using the explicit stack when navigating the rope tree to avoid
     * dealing with strings on the stack in drainMarkStack.
     */
    if (str->markIfUnmarked())
        ScanString(gcmarker, str);
}

void
gc::MarkChildren(JSTracer *trc, JSObject *obj)
{
    obj->markChildren(trc);
}

static void
gc::MarkChildren(JSTracer *trc, JSString *str)
{
    if (str->hasBase())
        str->markBase(trc);
    else if (str->isRope())
        str->asRope().markChildren(trc);
}

static void
gc::MarkChildren(JSTracer *trc, JSScript *script)
{
    script->markChildren(trc);
}

static void
gc::MarkChildren(JSTracer *trc, LazyScript *lazy)
{
    lazy->markChildren(trc);
}

static void
gc::MarkChildren(JSTracer *trc, Shape *shape)
{
    shape->markChildren(trc);
}

static void
gc::MarkChildren(JSTracer *trc, BaseShape *base)
{
    base->markChildren(trc);
}

/*
 * This function is used by the cycle collector to trace through the
 * children of a BaseShape (and its baseUnowned(), if any). The cycle
 * collector does not directly care about BaseShapes, so only the
 * getter, setter, and parent are marked. Furthermore, the parent is
 * marked only if it isn't the same as prevParent, which will be
 * updated to the current shape's parent.
 */
static inline void
MarkCycleCollectorChildren(JSTracer *trc, BaseShape *base, JSObject **prevParent)
{
    JS_ASSERT(base);

    /*
     * The cycle collector does not need to trace unowned base shapes,
     * as they have the same getter, setter and parent as the original
     * base shape.
     */
    base->assertConsistency();

    if (base->hasGetterObject()) {
        JSObject *tmp = base->getterObject();
        MarkObjectUnbarriered(trc, &tmp, "getter");
        JS_ASSERT(tmp == base->getterObject());
    }

    if (base->hasSetterObject()) {
        JSObject *tmp = base->setterObject();
        MarkObjectUnbarriered(trc, &tmp, "setter");
        JS_ASSERT(tmp == base->setterObject());
    }

    JSObject *parent = base->getObjectParent();
    if (parent && parent != *prevParent) {
        MarkObjectUnbarriered(trc, &parent, "parent");
        JS_ASSERT(parent == base->getObjectParent());
        *prevParent = parent;
    }
}

/*
 * This function is used by the cycle collector to trace through a
 * shape. The cycle collector does not care about shapes or base
 * shapes, so those are not marked. Instead, any shapes or base shapes
 * that are encountered have their children marked. Stack space is
 * bounded. If two shapes in a row have the same parent pointer, the
 * parent pointer will only be marked once.
 */
void
gc::MarkCycleCollectorChildren(JSTracer *trc, Shape *shape)
{
    JSObject *prevParent = NULL;
    do {
        MarkCycleCollectorChildren(trc, shape->base(), &prevParent);
        MarkId(trc, &shape->propidRef(), "propid");
        shape = shape->previous();
    } while (shape);
}

static void
ScanTypeObject(GCMarker *gcmarker, types::TypeObject *type)
{
    /* Don't mark properties for singletons. They'll be purged by the GC. */
    if (!type->singleton) {
        unsigned count = type->getPropertyCount();
        for (unsigned i = 0; i < count; i++) {
            types::Property *prop = type->getProperty(i);
            if (prop && JSID_IS_STRING(prop->id))
                PushMarkStack(gcmarker, JSID_TO_STRING(prop->id));
        }
    }

    if (TaggedProto(type->proto).isObject())
        PushMarkStack(gcmarker, type->proto);

    if (type->singleton && !type->lazy())
        PushMarkStack(gcmarker, type->singleton);

    if (type->addendum) {
        switch (type->addendum->kind) {
          case types::TypeObjectAddendum::NewScript:
            PushMarkStack(gcmarker, type->newScript()->fun);
            PushMarkStack(gcmarker, type->newScript()->shape.get());
            break;
        }
    }

    if (type->interpretedFunction)
        PushMarkStack(gcmarker, type->interpretedFunction);
}

static void
gc::MarkChildren(JSTracer *trc, types::TypeObject *type)
{
    unsigned count = type->getPropertyCount();
    for (unsigned i = 0; i < count; i++) {
        types::Property *prop = type->getProperty(i);
        if (prop)
            MarkId(trc, &prop->id, "type_prop");
    }

    if (TaggedProto(type->proto).isObject())
        MarkObject(trc, &type->proto, "type_proto");

    if (type->singleton && !type->lazy())
        MarkObject(trc, &type->singleton, "type_singleton");

    if (type->addendum) {
        switch (type->addendum->kind) {
          case types::TypeObjectAddendum::NewScript:
            MarkObject(trc, &type->newScript()->fun, "type_new_function");
            MarkShape(trc, &type->newScript()->shape, "type_new_shape");
            break;
        }
    }

    if (type->interpretedFunction)
        MarkObject(trc, &type->interpretedFunction, "type_function");
}

static void
gc::MarkChildren(JSTracer *trc, jit::IonCode *code)
{
#ifdef JS_ION
    code->trace(trc);
#endif
}

template<typename T>
static void
PushArenaTyped(GCMarker *gcmarker, ArenaHeader *aheader)
{
    for (CellIterUnderGC i(aheader); !i.done(); i.next())
        PushMarkStack(gcmarker, i.get<T>());
}

void
gc::PushArena(GCMarker *gcmarker, ArenaHeader *aheader)
{
    switch (MapAllocToTraceKind(aheader->getAllocKind())) {
      case JSTRACE_OBJECT:
        PushArenaTyped<JSObject>(gcmarker, aheader);
        break;

      case JSTRACE_STRING:
        PushArenaTyped<JSString>(gcmarker, aheader);
        break;

      case JSTRACE_SCRIPT:
        PushArenaTyped<JSScript>(gcmarker, aheader);
        break;

      case JSTRACE_LAZY_SCRIPT:
        PushArenaTyped<LazyScript>(gcmarker, aheader);
        break;

      case JSTRACE_SHAPE:
        PushArenaTyped<js::Shape>(gcmarker, aheader);
        break;

      case JSTRACE_BASE_SHAPE:
        PushArenaTyped<js::BaseShape>(gcmarker, aheader);
        break;

      case JSTRACE_TYPE_OBJECT:
        PushArenaTyped<js::types::TypeObject>(gcmarker, aheader);
        break;

      case JSTRACE_IONCODE:
        PushArenaTyped<js::jit::IonCode>(gcmarker, aheader);
        break;
    }
}

struct SlotArrayLayout
{
    union {
        HeapSlot *end;
        uintptr_t kind;
    };
    union {
        HeapSlot *start;
        uintptr_t index;
    };
    JSObject *obj;

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
    for (uintptr_t *p = stack.tos; p > stack.stack; ) {
        uintptr_t tag = *--p & StackTagMask;
        if (tag == ValueArrayTag) {
            *p &= ~StackTagMask;
            p -= 2;
            SlotArrayLayout *arr = reinterpret_cast<SlotArrayLayout *>(p);
            JSObject *obj = arr->obj;
            JS_ASSERT(obj->isNative());

            HeapSlot *vp = obj->getDenseElements();
            if (arr->end == vp + obj->getDenseInitializedLength()) {
                JS_ASSERT(arr->start >= vp);
                arr->index = arr->start - vp;
                arr->kind = HeapSlot::Element;
            } else {
                HeapSlot *vp = obj->fixedSlots();
                unsigned nfixed = obj->numFixedSlots();
                if (arr->start == arr->end) {
                    arr->index = obj->slotSpan();
                } else if (arr->start >= vp && arr->start < vp + nfixed) {
                    JS_ASSERT(arr->end == vp + Min(nfixed, obj->slotSpan()));
                    arr->index = arr->start - vp;
                } else {
                    JS_ASSERT(arr->start >= obj->slots &&
                              arr->end == obj->slots + obj->slotSpan() - nfixed);
                    arr->index = (arr->start - obj->slots) + nfixed;
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
GCMarker::restoreValueArray(JSObject *obj, void **vpp, void **endp)
{
    uintptr_t start = stack.pop();
    HeapSlot::Kind kind = (HeapSlot::Kind) stack.pop();

    if (kind == HeapSlot::Element) {
        if (!obj->is<ArrayObject>())
            return false;

        uint32_t initlen = obj->getDenseInitializedLength();
        HeapSlot *vp = obj->getDenseElements();
        if (start < initlen) {
            *vpp = vp + start;
            *endp = vp + initlen;
        } else {
            /* The object shrunk, in which case no scanning is needed. */
            *vpp = *endp = vp;
        }
    } else {
        JS_ASSERT(kind == HeapSlot::Slot);
        HeapSlot *vp = obj->fixedSlots();
        unsigned nfixed = obj->numFixedSlots();
        unsigned nslots = obj->slotSpan();
        if (start < nslots) {
            if (start < nfixed) {
                *vpp = vp + start;
                *endp = vp + Min(nfixed, nslots);
            } else {
                *vpp = obj->slots + start - nfixed;
                *endp = obj->slots + nslots - nfixed;
            }
        } else {
            /* The object shrunk, in which case no scanning is needed. */
            *vpp = *endp = vp;
        }
    }

    JS_ASSERT(*vpp <= *endp);
    return true;
}

void
GCMarker::processMarkStackOther(SliceBudget &budget, uintptr_t tag, uintptr_t addr)
{
    if (tag == TypeTag) {
        ScanTypeObject(this, reinterpret_cast<types::TypeObject *>(addr));
    } else if (tag == SavedValueArrayTag) {
        JS_ASSERT(!(addr & CellMask));
        JSObject *obj = reinterpret_cast<JSObject *>(addr);
        HeapValue *vp, *end;
        if (restoreValueArray(obj, (void **)&vp, (void **)&end))
            pushValueArray(obj, vp, end);
        else
            pushObject(obj);
    } else if (tag == IonCodeTag) {
        MarkChildren(this, reinterpret_cast<jit::IonCode *>(addr));
    } else if (tag == ArenaTag) {
        ArenaHeader *aheader = reinterpret_cast<ArenaHeader *>(addr);
        AllocKind thingKind = aheader->getAllocKind();
        size_t thingSize = Arena::thingSize(thingKind);

        for ( ; aheader; aheader = aheader->next) {
            Arena *arena = aheader->getArena();
            FreeSpan firstSpan(aheader->getFirstFreeSpan());
            const FreeSpan *span = &firstSpan;

            for (uintptr_t thing = arena->thingsStart(thingKind); ; thing += thingSize) {
                JS_ASSERT(thing <= arena->thingsEnd());
                if (thing == span->first) {
                    if (!span->hasNext())
                        break;
                    thing = span->last;
                    span = span->nextSpan();
                } else {
                    JSObject *object = reinterpret_cast<JSObject *>(thing);
                    if (object->hasSingletonType() && object->markIfUnmarked(getMarkColor()))
                        pushObject(object);
                    budget.step();
                }
            }
            if (budget.isOverBudget()) {
                pushArenaList(aheader);
                return;
            }
        }
    }
}

inline void
GCMarker::processMarkStackTop(SliceBudget &budget)
{
    /*
     * The function uses explicit goto and implements the scanning of the
     * object directly. It allows to eliminate the tail recursion and
     * significantly improve the marking performance, see bug 641025.
     */
    HeapSlot *vp, *end;
    JSObject *obj;

    uintptr_t addr = stack.pop();
    uintptr_t tag = addr & StackTagMask;
    addr &= ~StackTagMask;

    if (tag == ValueArrayTag) {
        JS_STATIC_ASSERT(ValueArrayTag == 0);
        JS_ASSERT(!(addr & CellMask));
        obj = reinterpret_cast<JSObject *>(addr);
        uintptr_t addr2 = stack.pop();
        uintptr_t addr3 = stack.pop();
        JS_ASSERT(addr2 <= addr3);
        JS_ASSERT((addr3 - addr2) % sizeof(Value) == 0);
        vp = reinterpret_cast<HeapSlot *>(addr2);
        end = reinterpret_cast<HeapSlot *>(addr3);
        goto scan_value_array;
    }

    if (tag == ObjectTag) {
        obj = reinterpret_cast<JSObject *>(addr);
        JS_COMPARTMENT_ASSERT(runtime, obj);
        goto scan_obj;
    }

    processMarkStackOther(budget, tag, addr);
    return;

  scan_value_array:
    JS_ASSERT(vp <= end);
    while (vp != end) {
        const Value &v = *vp++;
        if (v.isString()) {
            JSString *str = v.toString();
            JS_COMPARTMENT_ASSERT_STR(runtime, str);
            JS_ASSERT(runtime->isAtomsZone(str->zone()) || str->zone() == obj->zone());
            if (str->markIfUnmarked())
                ScanString(this, str);
        } else if (v.isObject()) {
            JSObject *obj2 = &v.toObject();
            JS_COMPARTMENT_ASSERT(runtime, obj2);
            JS_ASSERT(obj->compartment() == obj2->compartment());
            if (obj2->markIfUnmarked(getMarkColor())) {
                pushValueArray(obj, vp, end);
                obj = obj2;
                goto scan_obj;
            }
        }
    }
    return;

  scan_obj:
    {
        JS_COMPARTMENT_ASSERT(runtime, obj);

        budget.step();
        if (budget.isOverBudget()) {
            pushObject(obj);
            return;
        }

        types::TypeObject *type = obj->typeFromGC();
        PushMarkStack(this, type);

        Shape *shape = obj->lastProperty();
        PushMarkStack(this, shape);

        /* Call the trace hook if necessary. */
        Class *clasp = type->clasp;
        if (clasp->trace) {
            JS_ASSERT_IF(runtime->gcMode == JSGC_MODE_INCREMENTAL &&
                         runtime->gcIncrementalEnabled,
                         clasp->flags & JSCLASS_IMPLEMENTS_BARRIERS);
            clasp->trace(this, obj);
        }

        if (!shape->isNative())
            return;

        unsigned nslots = obj->slotSpan();

        if (!obj->hasEmptyElements()) {
            vp = obj->getDenseElements();
            end = vp + obj->getDenseInitializedLength();
            if (!nslots)
                goto scan_value_array;
            pushValueArray(obj, vp, end);
        }

        vp = obj->fixedSlots();
        if (obj->slots) {
            unsigned nfixed = obj->numFixedSlots();
            if (nslots > nfixed) {
                pushValueArray(obj, vp, vp + nfixed);
                vp = obj->slots;
                end = vp + (nslots - nfixed);
                goto scan_value_array;
            }
        }
        JS_ASSERT(nslots <= obj->numFixedSlots());
        end = vp + nslots;
        goto scan_value_array;
    }
}

bool
GCMarker::drainMarkStack(SliceBudget &budget)
{
#ifdef DEBUG
    JSRuntime *rt = runtime;

    struct AutoCheckCompartment {
        JSRuntime *runtime;
        AutoCheckCompartment(JSRuntime *rt) : runtime(rt) {
            JS_ASSERT(!rt->gcStrictCompartmentChecking);
            runtime->gcStrictCompartmentChecking = true;
        }
        ~AutoCheckCompartment() { runtime->gcStrictCompartmentChecking = false; }
    } acc(rt);
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

void
js::TraceChildren(JSTracer *trc, void *thing, JSGCTraceKind kind)
{
    switch (kind) {
      case JSTRACE_OBJECT:
        MarkChildren(trc, static_cast<JSObject *>(thing));
        break;

      case JSTRACE_STRING:
        MarkChildren(trc, static_cast<JSString *>(thing));
        break;

      case JSTRACE_SCRIPT:
        MarkChildren(trc, static_cast<JSScript *>(thing));
        break;

      case JSTRACE_LAZY_SCRIPT:
        MarkChildren(trc, static_cast<LazyScript *>(thing));
        break;

      case JSTRACE_SHAPE:
        MarkChildren(trc, static_cast<Shape *>(thing));
        break;

      case JSTRACE_IONCODE:
        MarkChildren(trc, (js::jit::IonCode *)thing);
        break;

      case JSTRACE_BASE_SHAPE:
        MarkChildren(trc, static_cast<BaseShape *>(thing));
        break;

      case JSTRACE_TYPE_OBJECT:
        MarkChildren(trc, (types::TypeObject *)thing);
        break;
    }
}

static void
UnmarkGrayGCThing(void *thing)
{
    static_cast<js::gc::Cell *>(thing)->unmark(js::gc::GRAY);
}

static void
UnmarkGrayChildren(JSTracer *trc, void **thingp, JSGCTraceKind kind);

struct UnmarkGrayTracer : public JSTracer
{
    /*
     * We set eagerlyTraceWeakMaps to false because the cycle collector will fix
     * up any color mismatches involving weakmaps when it runs.
     */
    UnmarkGrayTracer(JSRuntime *rt)
      : tracingShape(false), previousShape(NULL), unmarkedAny(false)
    {
        JS_TracerInit(this, rt, UnmarkGrayChildren);
        eagerlyTraceWeakMaps = DoNotTraceWeakMaps;
    }

    UnmarkGrayTracer(JSTracer *trc, bool tracingShape)
      : tracingShape(tracingShape), previousShape(NULL), unmarkedAny(false)
    {
        JS_TracerInit(this, trc->runtime, UnmarkGrayChildren);
        eagerlyTraceWeakMaps = DoNotTraceWeakMaps;
    }

    /* True iff we are tracing the immediate children of a shape. */
    bool tracingShape;

    /* If tracingShape, shape child or NULL. Otherwise, NULL. */
    void *previousShape;

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
UnmarkGrayChildren(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    void *thing = *thingp;
    int stackDummy;
    if (!JS_CHECK_STACK_SIZE(js::GetNativeStackLimit(trc->runtime), &stackDummy)) {
        /*
         * If we run out of stack, we take a more drastic measure: require that
         * we GC again before the next CC.
         */
        trc->runtime->gcGrayBitsValid = false;
        return;
    }

    UnmarkGrayTracer *tracer = static_cast<UnmarkGrayTracer *>(trc);
    if (!IsInsideNursery(trc->runtime, thing)) {
        if (!JS::GCThingIsMarkedGray(thing))
            return;

        UnmarkGrayGCThing(thing);
        tracer->unmarkedAny = true;
    }

    /*
     * Trace children of |thing|. If |thing| and its parent are both shapes,
     * |thing| will get saved to mPreviousShape without being traced. The parent
     * will later trace |thing|. This is done to avoid increasing the stack
     * depth during shape tracing. It is safe to do because a shape can only
     * have one child that is a shape.
     */
    UnmarkGrayTracer childTracer(tracer, kind == JSTRACE_SHAPE);

    if (kind != JSTRACE_SHAPE) {
        JS_TraceChildren(&childTracer, thing, kind);
        JS_ASSERT(!childTracer.previousShape);
        tracer->unmarkedAny |= childTracer.unmarkedAny;
        return;
    }

    if (tracer->tracingShape) {
        JS_ASSERT(!tracer->previousShape);
        tracer->previousShape = thing;
        return;
    }

    do {
        JS_ASSERT(!JS::GCThingIsMarkedGray(thing));
        JS_TraceChildren(&childTracer, thing, JSTRACE_SHAPE);
        thing = childTracer.previousShape;
        childTracer.previousShape = NULL;
    } while (thing);
    tracer->unmarkedAny |= childTracer.unmarkedAny;
}

JS_FRIEND_API(bool)
JS::UnmarkGrayGCThingRecursively(void *thing, JSGCTraceKind kind)
{
    JS_ASSERT(kind != JSTRACE_SHAPE);

    JSRuntime *rt = static_cast<Cell *>(thing)->runtimeFromMainThread();

    bool unmarkedArg = false;
    if (!IsInsideNursery(rt, thing)) {
        if (!JS::GCThingIsMarkedGray(thing))
            return false;

        UnmarkGrayGCThing(thing);
        unmarkedArg = true;
    }

    UnmarkGrayTracer trc(rt);
    JS_TraceChildren(&trc, thing, kind);

    return unmarkedArg || trc.unmarkedAny;
}
