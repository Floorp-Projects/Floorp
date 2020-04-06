/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Marking-inl.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/ReentrancyGuard.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Unused.h"

#include <algorithm>
#include <type_traits>

#include "jsfriendapi.h"

#include "builtin/ModuleObject.h"
#include "debugger/DebugAPI.h"
#include "gc/GCInternals.h"
#include "gc/Policy.h"
#include "jit/IonCode.h"
#include "js/GCTypeMacros.h"  // JS_FOR_EACH_PUBLIC_{,TAGGED_}GC_POINTER_TYPE
#include "js/SliceBudget.h"
#include "util/DiagnosticAssertions.h"
#include "util/Memory.h"
#include "util/Poison.h"
#include "vm/ArgumentsObject.h"
#include "vm/ArrayObject.h"
#include "vm/BigIntType.h"
#include "vm/EnvironmentObject.h"
#include "vm/GeneratorObject.h"
#include "vm/RegExpShared.h"
#include "vm/Scope.h"
#include "vm/Shape.h"
#include "vm/SymbolType.h"
#include "vm/TypedArrayObject.h"
#include "wasm/WasmJS.h"

#include "gc/GC-inl.h"
#include "gc/Nursery-inl.h"
#include "gc/PrivateIterators-inl.h"
#include "gc/WeakMap-inl.h"
#include "gc/Zone-inl.h"
#include "vm/GeckoProfiler-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/Realm-inl.h"
#include "vm/StringType-inl.h"

using namespace js;
using namespace js::gc;

using JS::MapTypeToTraceKind;

using mozilla::DebugOnly;
using mozilla::IntegerRange;
using mozilla::PodCopy;

// [SMDOC] GC Tracing
//
// Tracing Overview
// ================
//
// Tracing, in this context, refers to an abstract visitation of some or all of
// the GC-controlled heap. The effect of tracing an edge of the graph depends
// on the subclass of the JSTracer on whose behalf we are tracing.
//
// Marking
// -------
//
// The primary JSTracer is the GCMarker. The marking tracer causes the target
// of each traversed edge to be marked black and the target edge's children to
// be marked either gray (in the gc algorithm sense) or immediately black.
//
// Callback
// --------
//
// The secondary JSTracer is the CallbackTracer. This simply invokes a callback
// on each edge in a child.
//
// The following is a rough outline of the general struture of the tracing
// internals.
//
/* clang-format off */
//                                                                                              //
//   .---------.    .---------.    .--------------------------.       .----------.              //
//   |TraceEdge|    |TraceRoot|    |TraceManuallyBarrieredEdge|  ...  |TraceRange|   ... etc.   //
//   '---------'    '---------'    '--------------------------'       '----------'              //
//        \              \                        /                        /                    //
//         \              \  .-----------------. /                        /                     //
//          o------------->o-|TraceEdgeInternal|-o<----------------------o                      //
//                           '-----------------'                                                //
//                              /          \                                                    //
//                             /            \                                                   //
//                       .---------.   .----------.         .-----------------.                 //
//                       |DoMarking|   |DoCallback|-------> |<JSTraceCallback>|----------->     //
//                       '---------'   '----------'         '-----------------'                 //
//                            |                                                                 //
//                            |                                                                 //
//                     .-----------.                                                            //
//      o------------->|traverse(T)| .                                                          //
//     /_\             '-----------'   ' .                                                      //
//      |                   .       .      ' .                                                  //
//      |                   .         .        ' .                                              //
//      |                   .           .          ' .                                          //
//      |          .--------------.  .--------------.  ' .     .-----------------------.        //
//      |          |markAndScan(T)|  |markAndPush(T)|      ' - |markAndTraceChildren(T)|        //
//      |          '--------------'  '--------------'          '-----------------------'        //
//      |                   |                  \                               |                //
//      |                   |                   \                              |                //
//      |       .----------------------.     .----------------.         .------------------.    //
//      |       |eagerlyMarkChildren(T)|     |pushMarkStackTop|<===Oo   |T::traceChildren()|--> //
//      |       '----------------------'     '----------------'    ||   '------------------'    //
//      |                  |                         ||            ||                           //
//      |                  |                         ||            ||                           //
//      |                  |                         ||            ||                           //
//      o<-----------------o<========================OO============Oo                           //
//                                                                                              //
//                                                                                              //
//   Legend:                                                                                    //
//     ------  Direct calls                                                                     //
//     . . .   Static dispatch                                                                  //
//     ======  Dispatch through a manual stack.                                                 //
//                                                                                              //
/* clang-format on */

/*** Tracing Invariants *****************************************************/

#if defined(DEBUG)
template <typename T>
static inline bool IsThingPoisoned(T* thing) {
  const uint8_t poisonBytes[] = {
      JS_FRESH_NURSERY_PATTERN,      JS_SWEPT_NURSERY_PATTERN,
      JS_ALLOCATED_NURSERY_PATTERN,  JS_FRESH_TENURED_PATTERN,
      JS_MOVED_TENURED_PATTERN,      JS_SWEPT_TENURED_PATTERN,
      JS_ALLOCATED_TENURED_PATTERN,  JS_FREED_HEAP_PTR_PATTERN,
      JS_FREED_CHUNK_PATTERN,        JS_FREED_ARENA_PATTERN,
      JS_SWEPT_TI_PATTERN,           JS_SWEPT_CODE_PATTERN,
      JS_RESET_VALUE_PATTERN,        JS_POISONED_JSSCRIPT_DATA_PATTERN,
      JS_OOB_PARSE_NODE_PATTERN,     JS_LIFO_UNDEFINED_PATTERN,
      JS_LIFO_UNINITIALIZED_PATTERN,
  };
  const int numPoisonBytes = sizeof(poisonBytes) / sizeof(poisonBytes[0]);
  uint32_t* p =
      reinterpret_cast<uint32_t*>(reinterpret_cast<FreeSpan*>(thing) + 1);
  // Note: all free patterns are odd to make the common, not-poisoned case a
  // single test.
  if ((*p & 1) == 0) {
    return false;
  }
  for (int i = 0; i < numPoisonBytes; ++i) {
    const uint8_t pb = poisonBytes[i];
    const uint32_t pw = pb | (pb << 8) | (pb << 16) | (pb << 24);
    if (*p == pw) {
      return true;
    }
  }
  return false;
}

bool js::IsTracerKind(JSTracer* trc, JS::CallbackTracer::TracerKind kind) {
  return trc->isCallbackTracer() &&
         trc->asCallbackTracer()->getTracerKind() == kind;
}
#endif

bool ThingIsPermanentAtomOrWellKnownSymbol(JSString* str) {
  return str->isPermanentAtom();
}
bool ThingIsPermanentAtomOrWellKnownSymbol(JSLinearString* str) {
  return str->isPermanentAtom();
}
bool ThingIsPermanentAtomOrWellKnownSymbol(JSAtom* atom) {
  return atom->isPermanent();
}
bool ThingIsPermanentAtomOrWellKnownSymbol(PropertyName* name) {
  return name->isPermanent();
}
bool ThingIsPermanentAtomOrWellKnownSymbol(JS::Symbol* sym) {
  return sym->isWellKnownSymbol();
}

template <typename T>
static inline bool IsOwnedByOtherRuntime(JSRuntime* rt, T thing) {
  bool other = thing->runtimeFromAnyThread() != rt;
  MOZ_ASSERT_IF(other, ThingIsPermanentAtomOrWellKnownSymbol(thing) ||
                           thing->zoneFromAnyThread()->isSelfHostingZone());
  return other;
}

template <typename T>
void js::CheckTracedThing(JSTracer* trc, T* thing) {
#ifdef DEBUG
  MOZ_ASSERT(trc);
  MOZ_ASSERT(thing);

  // Check that CellHeader is the first field in the cell.
  static_assert(
      std::is_base_of<CellHeader,
                      typename std::remove_const<typename std::remove_reference<
                          decltype(thing->cellHeader())>::type>::type>::value,
      "GC things must provide a cellHeader() method that returns a reference "
      "to the cell header");
  MOZ_ASSERT(static_cast<const void*>(&thing->cellHeader()) ==
             static_cast<const void*>(thing));

  if (!trc->checkEdges()) {
    return;
  }

  if (IsForwarded(thing)) {
    MOZ_ASSERT(IsTracerKind(trc, JS::CallbackTracer::TracerKind::Moving) ||
               trc->isTenuringTracer());
    thing = Forwarded(thing);
  }

  /* This function uses data that's not available in the nursery. */
  if (IsInsideNursery(thing)) {
    return;
  }

  /*
   * Permanent atoms and things in the self-hosting zone are not associated
   * with this runtime, but will be ignored during marking.
   */
  if (IsOwnedByOtherRuntime(trc->runtime(), thing)) {
    return;
  }

  Zone* zone = thing->zoneFromAnyThread();
  JSRuntime* rt = trc->runtime();
  MOZ_ASSERT(zone->runtimeFromAnyThread() == rt);

  bool isGcMarkingTracer = trc->isMarkingTracer();
  bool isUnmarkGrayTracer =
      IsTracerKind(trc, JS::CallbackTracer::TracerKind::UnmarkGray);
  bool isClearEdgesTracer =
      IsTracerKind(trc, JS::CallbackTracer::TracerKind::ClearEdges);

  if (TlsContext.get()->isMainThreadContext()) {
    // If we're on the main thread we must have access to the runtime and zone.
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
    MOZ_ASSERT(CurrentThreadCanAccessZone(zone));
  } else {
    MOZ_ASSERT(
        isGcMarkingTracer || isUnmarkGrayTracer || isClearEdgesTracer ||
        IsTracerKind(trc, JS::CallbackTracer::TracerKind::Moving) ||
        IsTracerKind(trc, JS::CallbackTracer::TracerKind::GrayBuffering) ||
        IsTracerKind(trc, JS::CallbackTracer::TracerKind::Sweeping));
    MOZ_ASSERT_IF(!isClearEdgesTracer, CurrentThreadIsPerformingGC());
  }

  // It shouldn't be possible to trace into zones used by helper threads, except
  // for use of ClearEdgesTracer by GCManagedDeletePolicy on a helper thread.
  MOZ_ASSERT_IF(!isClearEdgesTracer, !zone->usedByHelperThread());

  MOZ_ASSERT(thing->isAligned());
  MOZ_ASSERT(MapTypeToTraceKind<std::remove_pointer_t<T>>::kind ==
             thing->getTraceKind());

  if (isGcMarkingTracer) {
    GCMarker* gcMarker = GCMarker::fromTracer(trc);
    MOZ_ASSERT(zone->shouldMarkInZone());

    MOZ_ASSERT_IF(gcMarker->shouldCheckCompartments(),
                  zone->isCollectingFromAnyThread() || zone->isAtomsZone());

    MOZ_ASSERT_IF(gcMarker->markColor() == MarkColor::Gray,
                  !zone->isGCMarkingBlackOnly() || zone->isAtomsZone());

    MOZ_ASSERT(!(zone->isGCSweeping() || zone->isGCFinished() ||
                 zone->isGCCompacting()));

    // Check that we don't stray from the current compartment and zone without
    // using TraceCrossCompartmentEdge.
    Compartment* comp = thing->maybeCompartment();
    MOZ_ASSERT_IF(gcMarker->tracingCompartment && comp,
                  gcMarker->tracingCompartment == comp);
    MOZ_ASSERT_IF(gcMarker->tracingZone,
                  gcMarker->tracingZone == zone || zone->isAtomsZone());
  }

  /*
   * Try to assert that the thing is allocated.
   *
   * We would like to assert that the thing is not in the free list, but this
   * check is very slow. Instead we check whether the thing has been poisoned:
   * if it has not then we assume it is allocated, but if it has then it is
   * either free or uninitialized in which case we check the free list.
   *
   * Further complications are that background sweeping may be running and
   * concurrently modifiying the free list and that tracing is done off
   * thread during compacting GC and reading the contents of the thing by
   * IsThingPoisoned would be racy in this case.
   */
  MOZ_ASSERT_IF(JS::RuntimeHeapIsBusy() && !zone->isGCSweeping() &&
                    !zone->isGCFinished() && !zone->isGCCompacting(),
                !IsThingPoisoned(thing) ||
                    !InFreeList(thing->asTenured().arena(), thing));
#endif
}

template <typename T>
void js::CheckTracedThing(JSTracer* trc, T thing) {
  ApplyGCThingTyped(thing, [](auto t) { CheckTracedThing(t); });
}

namespace js {
#define IMPL_CHECK_TRACED_THING(_, type, _1, _2) \
  template void CheckTracedThing<type>(JSTracer*, type*);
JS_FOR_EACH_TRACEKIND(IMPL_CHECK_TRACED_THING);
#undef IMPL_CHECK_TRACED_THING
}  // namespace js

static inline bool ShouldMarkCrossCompartment(GCMarker* marker, JSObject* src,
                                              Cell* dstCell) {
  MarkColor color = marker->markColor();

  if (!dstCell->isTenured()) {
    MOZ_ASSERT(color == MarkColor::Black);
    return false;
  }
  TenuredCell& dst = dstCell->asTenured();

  JS::Zone* dstZone = dst.zone();
  if (!src->zone()->isGCMarking() && !dstZone->isGCMarking()) {
    return false;
  }

  if (color == MarkColor::Black) {
    // Check our sweep groups are correct: we should never have to
    // mark something in a zone that we have started sweeping.
    MOZ_ASSERT_IF(!dst.isMarkedBlack(), !dstZone->isGCSweeping());

    /*
     * Having black->gray edges violates our promise to the cycle collector so
     * we ensure that gray things we encounter when marking black end up getting
     * marked black.
     *
     * This can happen for two reasons:
     *
     * 1) If we're collecting a compartment and it has an edge to an uncollected
     * compartment it's possible that the source and destination of the
     * cross-compartment edge should be gray, but the source was marked black by
     * the write barrier.
     *
     * 2) If we yield during gray marking and the write barrier marks a gray
     * thing black.
     *
     * We handle the first case before returning whereas the second case happens
     * as part of normal marking.
     */
    if (dst.isMarkedGray() && !dstZone->isGCMarking()) {
      UnmarkGrayGCThingUnchecked(marker->runtime(),
                                 JS::GCCellPtr(&dst, dst.getTraceKind()));
      return false;
    }

    return dstZone->isGCMarking();
  } else {
    // Check our sweep groups are correct as above.
    MOZ_ASSERT_IF(!dst.isMarkedAny(), !dstZone->isGCSweeping());

    if (dstZone->isGCMarkingBlackOnly()) {
      /*
       * The destination compartment is being not being marked gray now,
       * but it will be later, so record the cell so it can be marked gray
       * at the appropriate time.
       */
      if (!dst.isMarkedAny()) {
        DelayCrossCompartmentGrayMarking(src);
      }
      return false;
    }

    return dstZone->isGCMarkingBlackAndGray();
  }
}

static bool ShouldTraceCrossCompartment(JSTracer* trc, JSObject* src,
                                        Cell* dstCell) {
  if (!trc->isMarkingTracer()) {
    return true;
  }

  return ShouldMarkCrossCompartment(GCMarker::fromTracer(trc), src, dstCell);
}

static bool ShouldTraceCrossCompartment(JSTracer* trc, JSObject* src,
                                        const Value& val) {
  return val.isGCThing() &&
         ShouldTraceCrossCompartment(trc, src, val.toGCThing());
}

static void AssertShouldMarkInZone(Cell* thing) {
  MOZ_ASSERT(thing->asTenured().zone()->shouldMarkInZone());
}

static void AssertShouldMarkInZone(JSString* str) {
#ifdef DEBUG
  Zone* zone = str->zone();
  MOZ_ASSERT(zone->shouldMarkInZone() || zone->isAtomsZone());
#endif
}

static void AssertShouldMarkInZone(JS::Symbol* sym) {
#ifdef DEBUG
  Zone* zone = sym->asTenured().zone();
  MOZ_ASSERT(zone->shouldMarkInZone() || zone->isAtomsZone());
#endif
}

#ifdef DEBUG
void js::gc::AssertRootMarkingPhase(JSTracer* trc) {
  MOZ_ASSERT_IF(trc->isMarkingTracer(),
                trc->runtime()->gc.state() == State::NotActive ||
                    trc->runtime()->gc.state() == State::MarkRoots);
}
#endif

/*** Tracing Interface ******************************************************/

template <typename T>
bool DoCallback(JS::CallbackTracer* trc, T** thingp, const char* name);
template <typename T>
bool DoCallback(JS::CallbackTracer* trc, T* thingp, const char* name);
template <typename T>
void DoMarking(GCMarker* gcmarker, T* thing);
template <typename T>
void DoMarking(GCMarker* gcmarker, const T& thing);

template <typename T>
static void TraceExternalEdgeHelper(JSTracer* trc, T* thingp,
                                    const char* name) {
  MOZ_ASSERT(InternalBarrierMethods<T>::isMarkable(*thingp));
  TraceEdgeInternal(trc, ConvertToBase(thingp), name);
}

JS_PUBLIC_API void js::UnsafeTraceManuallyBarrieredEdge(JSTracer* trc,
                                                        JSObject** thingp,
                                                        const char* name) {
  TraceEdgeInternal(trc, ConvertToBase(thingp), name);
}

template <typename T>
static void UnsafeTraceRootHelper(JSTracer* trc, T* thingp, const char* name) {
  MOZ_ASSERT(thingp);
  js::TraceNullableRoot(trc, thingp, name);
}

namespace js {
class AbstractGeneratorObject;
class SavedFrame;
}  // namespace js

#define DEFINE_TRACE_EXTERNAL_EDGE_FUNCTION(type)                           \
  JS_PUBLIC_API void js::gc::TraceExternalEdge(JSTracer* trc, type* thingp, \
                                               const char* name) {          \
    TraceExternalEdgeHelper(trc, thingp, name);                             \
  }

// Define TraceExternalEdge for each public GC pointer type.
JS_FOR_EACH_PUBLIC_GC_POINTER_TYPE(DEFINE_TRACE_EXTERNAL_EDGE_FUNCTION)
JS_FOR_EACH_PUBLIC_TAGGED_GC_POINTER_TYPE(DEFINE_TRACE_EXTERNAL_EDGE_FUNCTION)

// Also, for the moment, define TraceExternalEdge for internal GC pointer types.
DEFINE_TRACE_EXTERNAL_EDGE_FUNCTION(AbstractGeneratorObject*)
DEFINE_TRACE_EXTERNAL_EDGE_FUNCTION(SavedFrame*)

#undef DEFINE_TRACE_EXTERNAL_EDGE_FUNCTION

#define DEFINE_UNSAFE_TRACE_ROOT_FUNCTION(type)                       \
  JS_PUBLIC_API void JS::UnsafeTraceRoot(JSTracer* trc, type* thingp, \
                                         const char* name) {          \
    UnsafeTraceRootHelper(trc, thingp, name);                         \
  }

// Define UnsafeTraceRoot for each public GC pointer type.
JS_FOR_EACH_PUBLIC_GC_POINTER_TYPE(DEFINE_UNSAFE_TRACE_ROOT_FUNCTION)
JS_FOR_EACH_PUBLIC_TAGGED_GC_POINTER_TYPE(DEFINE_UNSAFE_TRACE_ROOT_FUNCTION)

// Also, for the moment, define UnsafeTraceRoot for internal GC pointer types.
DEFINE_UNSAFE_TRACE_ROOT_FUNCTION(AbstractGeneratorObject*)
DEFINE_UNSAFE_TRACE_ROOT_FUNCTION(SavedFrame*)

#undef DEFINE_UNSAFE_TRACE_ROOT_FUNCTION

namespace js {
namespace gc {

#define INSTANTIATE_INTERNAL_TRACE_FUNCTIONS(type)                      \
  template bool TraceEdgeInternal<type>(JSTracer*, type*, const char*); \
  template void TraceRangeInternal<type>(JSTracer*, size_t len, type*,  \
                                         const char*);

#define INSTANTIATE_INTERNAL_TRACE_FUNCTIONS_FROM_TRACEKIND(_1, type, _2, _3) \
  INSTANTIATE_INTERNAL_TRACE_FUNCTIONS(type*)

JS_FOR_EACH_TRACEKIND(INSTANTIATE_INTERNAL_TRACE_FUNCTIONS_FROM_TRACEKIND)
JS_FOR_EACH_PUBLIC_TAGGED_GC_POINTER_TYPE(INSTANTIATE_INTERNAL_TRACE_FUNCTIONS)

#undef INSTANTIATE_INTERNAL_TRACE_FUNCTIONS_FROM_TRACEKIND
#undef INSTANTIATE_INTERNAL_TRACE_FUNCTIONS

}  // namespace gc
}  // namespace js

// In debug builds, makes a note of the current compartment before calling a
// trace hook or traceChildren() method on a GC thing.
class MOZ_RAII AutoSetTracingSource {
#ifdef DEBUG
  GCMarker* marker = nullptr;
#endif

 public:
  template <typename T>
  AutoSetTracingSource(JSTracer* trc, T* thing) {
#ifdef DEBUG
    if (trc->isMarkingTracer() && thing) {
      marker = GCMarker::fromTracer(trc);
      MOZ_ASSERT(!marker->tracingZone);
      marker->tracingZone = thing->asTenured().zone();
      MOZ_ASSERT(!marker->tracingCompartment);
      marker->tracingCompartment = thing->maybeCompartment();
    }
#endif
  }

  ~AutoSetTracingSource() {
#ifdef DEBUG
    if (marker) {
      marker->tracingZone = nullptr;
      marker->tracingCompartment = nullptr;
    }
#endif
  }
};

// In debug builds, clear the trace hook compartment. This happens
// after the trace hook has called back into one of our trace APIs and we've
// checked the traced thing.
class MOZ_RAII AutoClearTracingSource {
#ifdef DEBUG
  GCMarker* marker = nullptr;
  JS::Zone* prevZone = nullptr;
  Compartment* prevCompartment = nullptr;
#endif

 public:
  explicit AutoClearTracingSource(JSTracer* trc) {
#ifdef DEBUG
    if (trc->isMarkingTracer()) {
      marker = GCMarker::fromTracer(trc);
      prevZone = marker->tracingZone;
      marker->tracingZone = nullptr;
      prevCompartment = marker->tracingCompartment;
      marker->tracingCompartment = nullptr;
    }
#endif
  }

  ~AutoClearTracingSource() {
#ifdef DEBUG
    if (marker) {
      marker->tracingZone = prevZone;
      marker->tracingCompartment = prevCompartment;
    }
#endif
  }
};

template <typename T>
void js::TraceManuallyBarrieredCrossCompartmentEdge(JSTracer* trc,
                                                    JSObject* src, T* dst,
                                                    const char* name) {
  // Clear expected compartment for cross-compartment edge.
  AutoClearTracingSource acts(trc);

  if (ShouldTraceCrossCompartment(trc, src, *dst)) {
    TraceEdgeInternal(trc, dst, name);
  }
}
template void js::TraceManuallyBarrieredCrossCompartmentEdge<Value>(
    JSTracer*, JSObject*, Value*, const char*);
template void js::TraceManuallyBarrieredCrossCompartmentEdge<JSObject*>(
    JSTracer*, JSObject*, JSObject**, const char*);
template void js::TraceManuallyBarrieredCrossCompartmentEdge<BaseScript*>(
    JSTracer*, JSObject*, BaseScript**, const char*);

template <typename T>
void js::TraceWeakMapKeyEdgeInternal(JSTracer* trc, Zone* weakMapZone,
                                     T** thingp, const char* name) {
  // We can't use ShouldTraceCrossCompartment here because that assumes the
  // source of the edge is a CCW object which could be used to delay gray
  // marking. Instead, assert that the weak map zone is in the same marking
  // state as the target thing's zone and therefore we can go ahead and mark it.
#ifdef DEBUG
  auto thing = *thingp;
  if (trc->isMarkingTracer()) {
    MOZ_ASSERT(weakMapZone->isGCMarking());
    MOZ_ASSERT(weakMapZone->gcState() == thing->zone()->gcState());
  }
#endif

  // Clear expected compartment for cross-compartment edge.
  AutoClearTracingSource acts(trc);

  TraceEdgeInternal(trc, thingp, name);
}

template void js::TraceWeakMapKeyEdgeInternal<JSObject>(JSTracer*, Zone*,
                                                        JSObject**,
                                                        const char*);
template void js::TraceWeakMapKeyEdgeInternal<BaseScript>(JSTracer*, Zone*,
                                                          BaseScript**,
                                                          const char*);

template <typename T>
void js::TraceProcessGlobalRoot(JSTracer* trc, T* thing, const char* name) {
  AssertRootMarkingPhase(trc);
  MOZ_ASSERT(ThingIsPermanentAtomOrWellKnownSymbol(thing));

  // We have to mark permanent atoms and well-known symbols through a special
  // method because the default DoMarking implementation automatically skips
  // them. Fortunately, atoms (permanent and non) cannot refer to other GC
  // things so they do not need to go through the mark stack and may simply
  // be marked directly.  Moreover, well-known symbols can refer only to
  // permanent atoms, so likewise require no subsquent marking.
  CheckTracedThing(trc, *ConvertToBase(&thing));
  AutoClearTracingSource acts(trc);
  if (trc->isMarkingTracer()) {
    thing->asTenured().markIfUnmarked(gc::MarkColor::Black);
  } else {
    DoCallback(trc->asCallbackTracer(), ConvertToBase(&thing), name);
  }
}
template void js::TraceProcessGlobalRoot<JSAtom>(JSTracer*, JSAtom*,
                                                 const char*);
template void js::TraceProcessGlobalRoot<JS::Symbol>(JSTracer*, JS::Symbol*,
                                                     const char*);

static Cell* TraceGenericPointerRootAndType(JSTracer* trc, Cell* thing,
                                            JS::TraceKind kind,
                                            const char* name) {
  return MapGCThingTyped(thing, kind, [trc, name](auto t) -> Cell* {
    TraceRoot(trc, &t, name);
    return t;
  });
}

void js::TraceGenericPointerRoot(JSTracer* trc, Cell** thingp,
                                 const char* name) {
  MOZ_ASSERT(thingp);
  Cell* thing = *thingp;
  if (!thing) {
    return;
  }

  Cell* traced =
      TraceGenericPointerRootAndType(trc, thing, thing->getTraceKind(), name);
  if (traced != thing) {
    *thingp = traced;
  }
}

void js::TraceManuallyBarrieredGenericPointerEdge(JSTracer* trc, Cell** thingp,
                                                  const char* name) {
  MOZ_ASSERT(thingp);
  Cell* thing = *thingp;
  if (!*thingp) {
    return;
  }

  auto traced = MapGCThingTyped(thing, thing->getTraceKind(),
                                [trc, name](auto t) -> Cell* {
                                  TraceManuallyBarrieredEdge(trc, &t, name);
                                  return t;
                                });
  if (traced != thing) {
    *thingp = traced;
  }
}

void js::TraceGCCellPtrRoot(JSTracer* trc, JS::GCCellPtr* thingp,
                            const char* name) {
  Cell* thing = thingp->asCell();
  if (!thing) {
    return;
  }

  Cell* traced =
      TraceGenericPointerRootAndType(trc, thing, thingp->kind(), name);

  if (!traced) {
    *thingp = JS::GCCellPtr();
  } else if (traced != thingp->asCell()) {
    *thingp = JS::GCCellPtr(traced, thingp->kind());
  }
}

// This method is responsible for dynamic dispatch to the real tracer
// implementation. Consider replacing this choke point with virtual dispatch:
// a sufficiently smart C++ compiler may be able to devirtualize some paths.
template <typename T>
bool js::gc::TraceEdgeInternal(JSTracer* trc, T* thingp, const char* name) {
#define IS_SAME_TYPE_OR(name, type, _, _1) std::is_same_v<type*, T> ||
  static_assert(JS_FOR_EACH_TRACEKIND(IS_SAME_TYPE_OR)
                        std::is_same_v<T, JS::Value> ||
                    std::is_same_v<T, jsid> || std::is_same_v<T, TaggedProto>,
                "Only the base cell layout types are allowed into "
                "marking/tracing internals");
#undef IS_SAME_TYPE_OR
  if (trc->isMarkingTracer()) {
    DoMarking(GCMarker::fromTracer(trc), *thingp);
    return true;
  }
  if (trc->isTenuringTracer()) {
    static_cast<TenuringTracer*>(trc)->traverse(thingp);
    return true;
  }
  MOZ_ASSERT(trc->isCallbackTracer());
  return DoCallback(trc->asCallbackTracer(), thingp, name);
}

template <typename T>
void js::gc::TraceRangeInternal(JSTracer* trc, size_t len, T* vec,
                                const char* name) {
  JS::AutoTracingIndex index(trc);
  for (auto i : IntegerRange(len)) {
    if (InternalBarrierMethods<T>::isMarkable(vec[i])) {
      TraceEdgeInternal(trc, &vec[i], name);
    }
    ++index;
  }
}

/*** GC Marking Interface ***************************************************/

namespace js {

using HasNoImplicitEdgesType = bool;

template <typename T>
struct ImplicitEdgeHolderType {
  using Type = HasNoImplicitEdgesType;
};

// For now, we only handle JSObject* and BaseScript* keys, but the linear time
// algorithm can be easily extended by adding in more types here, then making
// GCMarker::traverse<T> call markImplicitEdges.
template <>
struct ImplicitEdgeHolderType<JSObject*> {
  using Type = JSObject*;
};

template <>
struct ImplicitEdgeHolderType<BaseScript*> {
  using Type = BaseScript*;
};

void GCMarker::markEphemeronValues(gc::Cell* markedCell,
                                   WeakEntryVector& values) {
  DebugOnly<size_t> initialLen = values.length();

  for (const auto& markable : values) {
    markable.weakmap->markKey(this, markedCell, markable.key);
  }

  // The vector should not be appended to during iteration because the key is
  // already marked, and even in cases where we have a multipart key, we
  // should only be inserting entries for the unmarked portions.
  MOZ_ASSERT(values.length() == initialLen);
}

template <typename T>
void GCMarker::markImplicitEdgesHelper(T markedThing) {
  if (!isWeakMarking()) {
    return;
  }

  Zone* zone = markedThing->asTenured().zone();
  MOZ_ASSERT(zone->isGCMarking());
  MOZ_ASSERT(!zone->isGCSweeping());

  auto p = zone->gcWeakKeys().get(markedThing);
  if (!p) {
    return;
  }
  WeakEntryVector& markables = p->value;

  // markedThing might be a key in a debugger weakmap, which can end up marking
  // values that are in a different compartment.
  AutoClearTracingSource acts(this);

  markEphemeronValues(markedThing, markables);
  markables.clear();  // If key address is reused, it should do nothing
}

template <>
void GCMarker::markImplicitEdgesHelper(HasNoImplicitEdgesType) {}

template <typename T>
void GCMarker::markImplicitEdges(T* thing) {
  markImplicitEdgesHelper<typename ImplicitEdgeHolderType<T*>::Type>(thing);
}

template void GCMarker::markImplicitEdges(JSObject*);
template void GCMarker::markImplicitEdges(BaseScript*);

}  // namespace js

template <typename T>
static inline bool ShouldMark(GCMarker* gcmarker, T thing) {
  // Don't trace things that are owned by another runtime.
  if (IsOwnedByOtherRuntime(gcmarker->runtime(), thing)) {
    return false;
  }

  // Don't mark things outside a zone if we are in a per-zone GC.
  return thing->zone()->shouldMarkInZone();
}

template <>
bool ShouldMark<JSObject*>(GCMarker* gcmarker, JSObject* obj) {
  // Don't trace things that are owned by another runtime.
  if (IsOwnedByOtherRuntime(gcmarker->runtime(), obj)) {
    return false;
  }

  // We may mark a Nursery thing outside the context of the
  // MinorCollectionTracer because of a pre-barrier. The pre-barrier is not
  // needed in this case because we perform a minor collection before each
  // incremental slice.
  if (IsInsideNursery(obj)) {
    return false;
  }

  // Don't mark things outside a zone if we are in a per-zone GC. It is
  // faster to check our own arena, which we can do since we know that
  // the object is tenured.
  return obj->asTenured().zone()->shouldMarkInZone();
}

// JSStrings can also be in the nursery. See ShouldMark<JSObject*> for comments.
template <>
bool ShouldMark<JSString*>(GCMarker* gcmarker, JSString* str) {
  if (IsOwnedByOtherRuntime(gcmarker->runtime(), str)) {
    return false;
  }
  if (IsInsideNursery(str)) {
    return false;
  }
  return str->asTenured().zone()->shouldMarkInZone();
}

// BigInts can also be in the nursery. See ShouldMark<JSObject*> for comments.
template <>
bool ShouldMark<JS::BigInt*>(GCMarker* gcmarker, JS::BigInt* bi) {
  if (IsOwnedByOtherRuntime(gcmarker->runtime(), bi)) {
    return false;
  }
  if (IsInsideNursery(bi)) {
    return false;
  }
  return bi->asTenured().zone()->shouldMarkInZone();
}

template <typename T>
void DoMarking(GCMarker* gcmarker, T* thing) {
  // Do per-type marking precondition checks.
  if (!ShouldMark(gcmarker, thing)) {
    return;
  }

  CheckTracedThing(gcmarker, thing);
  AutoClearTracingSource acts(gcmarker);
  gcmarker->traverse(thing);

  // Mark the compartment as live.
  SetMaybeAliveFlag(thing);
}

template <typename T>
void DoMarking(GCMarker* gcmarker, const T& thing) {
  ApplyGCThingTyped(thing, [gcmarker](auto t) { DoMarking(gcmarker, t); });
}

JS_PUBLIC_API void js::gc::PerformIncrementalReadBarrier(JS::GCCellPtr thing) {
  // Optimized marking for read barriers. This is called from
  // ExposeGCThingToActiveJS which has already checked the prerequisites for
  // performing a read barrier. This means we can skip a bunch of checks and
  // call info the tracer directly.

  MOZ_ASSERT(thing);
  MOZ_ASSERT(!JS::RuntimeHeapIsMajorCollecting());

  TenuredCell* cell = &thing.asCell()->asTenured();
  Zone* zone = cell->zone();
  MOZ_ASSERT(zone->needsIncrementalBarrier());

  // Skip disptaching on known tracer type.
  GCMarker* gcmarker = GCMarker::fromTracer(zone->barrierTracer());

  // Mark the argument, as DoMarking above.
  ApplyGCThingTyped(thing, [gcmarker](auto thing) {
    MOZ_ASSERT(ShouldMark(gcmarker, thing));
    CheckTracedThing(gcmarker, thing);
    AutoClearTracingSource acts(gcmarker);
    gcmarker->traverse(thing);
  });
}

// The simplest traversal calls out to the fully generic traceChildren function
// to visit the child edges. In the absence of other traversal mechanisms, this
// function will rapidly grow the stack past its bounds and crash the process.
// Thus, this generic tracing should only be used in cases where subsequent
// tracing will not recurse.
template <typename T>
void js::GCMarker::markAndTraceChildren(T* thing) {
  if (ThingIsPermanentAtomOrWellKnownSymbol(thing)) {
    return;
  }
  if (mark(thing)) {
    AutoSetTracingSource asts(this, thing);
    thing->traceChildren(this);
  }
}
namespace js {
template <>
void GCMarker::traverse(BaseShape* thing) {
  markAndTraceChildren(thing);
}
template <>
void GCMarker::traverse(JS::Symbol* thing) {
  markAndTraceChildren(thing);
}
template <>
void GCMarker::traverse(JS::BigInt* thing) {
  markAndTraceChildren(thing);
}
template <>
void GCMarker::traverse(RegExpShared* thing) {
  markAndTraceChildren(thing);
}
}  // namespace js

// Strings, Shapes, and Scopes are extremely common, but have simple patterns of
// recursion. We traverse trees of these edges immediately, with aggressive,
// manual inlining, implemented by eagerlyTraceChildren.
template <typename T>
void js::GCMarker::markAndScan(T* thing) {
  if (ThingIsPermanentAtomOrWellKnownSymbol(thing)) {
    return;
  }
  if (mark(thing)) {
    eagerlyMarkChildren(thing);
  }
}
namespace js {
template <>
void GCMarker::traverse(JSString* thing) {
  markAndScan(thing);
}
template <>
void GCMarker::traverse(Shape* thing) {
  markAndScan(thing);
}
template <>
void GCMarker::traverse(js::Scope* thing) {
  markAndScan(thing);
}
}  // namespace js

// Object and ObjectGroup are extremely common and can contain arbitrarily
// nested graphs, so are not trivially inlined. In this case we use a mark
// stack to control recursion. JitCode shares none of these properties, but is
// included for historical reasons. JSScript normally cannot recurse, but may
// be used as a weakmap key and thereby recurse into weakmapped values.
template <typename T>
void js::GCMarker::markAndPush(T* thing) {
  if (!mark(thing)) {
    return;
  }
  pushTaggedPtr(thing);
}
namespace js {
template <>
void GCMarker::traverse(JSObject* thing) {
  markAndPush(thing);
}
template <>
void GCMarker::traverse(ObjectGroup* thing) {
  markAndPush(thing);
}
template <>
void GCMarker::traverse(jit::JitCode* thing) {
  markAndPush(thing);
}
template <>
void GCMarker::traverse(BaseScript* thing) {
  markAndPush(thing);
}
}  // namespace js

namespace js {
template <>
void GCMarker::traverse(AccessorShape* thing) {
  MOZ_CRASH("AccessorShape must be marked as a Shape");
}
}  // namespace js

template <typename S, typename T>
static void CheckTraversedEdge(S source, T* target) {
  // Atoms and Symbols do not have or mark their internal pointers,
  // respectively.
  MOZ_ASSERT(!ThingIsPermanentAtomOrWellKnownSymbol(source));

  // The Zones must match, unless the target is an atom.
  MOZ_ASSERT_IF(
      !ThingIsPermanentAtomOrWellKnownSymbol(target),
      target->zone()->isAtomsZone() || target->zone() == source->zone());

  // If we are marking an atom, that atom must be marked in the source zone's
  // atom bitmap.
  MOZ_ASSERT_IF(!ThingIsPermanentAtomOrWellKnownSymbol(target) &&
                    target->zone()->isAtomsZone() &&
                    !source->zone()->isAtomsZone(),
                target->runtimeFromAnyThread()->gc.atomMarking.atomIsMarked(
                    source->zone(), reinterpret_cast<TenuredCell*>(target)));

  // Atoms and Symbols do not have access to a compartment pointer, or we'd need
  // to adjust the subsequent check to catch that case.
  MOZ_ASSERT_IF(ThingIsPermanentAtomOrWellKnownSymbol(target),
                !target->maybeCompartment());
  MOZ_ASSERT_IF(target->zoneFromAnyThread()->isAtomsZone(),
                !target->maybeCompartment());
  // If we have access to a compartment pointer for both things, they must
  // match.
  MOZ_ASSERT_IF(source->maybeCompartment() && target->maybeCompartment(),
                source->maybeCompartment() == target->maybeCompartment());
}

template <typename S, typename T>
void js::GCMarker::traverseEdge(S source, T* target) {
  CheckTraversedEdge(source, target);
  traverse(target);
}

template <typename S, typename T>
void js::GCMarker::traverseEdge(S source, const T& thing) {
  ApplyGCThingTyped(thing,
                    [this, source](auto t) { this->traverseEdge(source, t); });
}

namespace {

template <typename T>
struct TraceKindCanBeGray {};
#define EXPAND_TRACEKIND_DEF(_, type, canBeGray, _1) \
  template <>                                        \
  struct TraceKindCanBeGray<type> {                  \
    static const bool value = canBeGray;             \
  };
JS_FOR_EACH_TRACEKIND(EXPAND_TRACEKIND_DEF)
#undef EXPAND_TRACEKIND_DEF

}  // namespace

struct TraceKindCanBeGrayFunctor {
  template <typename T>
  bool operator()() {
    return TraceKindCanBeGray<T>::value;
  }
};

static bool TraceKindCanBeMarkedGray(JS::TraceKind kind) {
  return DispatchTraceKindTyped(TraceKindCanBeGrayFunctor(), kind);
}

template <typename T>
bool js::GCMarker::mark(T* thing) {
  if (!thing->isTenured()) {
    return false;
  }

  AssertShouldMarkInZone(thing);
  TenuredCell* cell = &thing->asTenured();

  MarkColor color =
      TraceKindCanBeGray<T>::value ? markColor() : MarkColor::Black;
  bool marked = cell->markIfUnmarked(color);
  if (marked) {
    markCount++;
  }

  return marked;
}

/*** Inline, Eager GC Marking ***********************************************/

// Each of the eager, inline marking paths is directly preceeded by the
// out-of-line, generic tracing code for comparison. Both paths must end up
// traversing equivalent subgraphs.

void BaseScript::traceChildren(JSTracer* trc) {
  TraceEdge(trc, &functionOrGlobal_, "function");
  TraceEdge(trc, &sourceObject_, "sourceObject");

  warmUpData_.trace(trc);

  if (data_) {
    data_->trace(trc);
  }

  if (sharedData_) {
    sharedData_->traceChildren(trc);
  }

  // Scripts with bytecode may have optional data stored in per-runtime or
  // per-zone maps. Note that a failed compilation must not have entries since
  // the script itself will not be marked as having bytecode.
  if (hasBytecode()) {
    JSScript* script = this->asJSScript();

    if (hasDebugScript()) {
      DebugAPI::traceDebugScript(trc, script);
    }
  }

  if (trc->isMarkingTracer()) {
    GCMarker::fromTracer(trc)->markImplicitEdges(this);
  }
}

void Shape::traceChildren(JSTracer* trc) {
  TraceEdge(trc, &headerAndBase_, "base");
  TraceEdge(trc, &propidRef(), "propid");
  if (parent) {
    TraceEdge(trc, &parent, "parent");
  }
  if (dictNext.isObject()) {
    JSObject* obj = dictNext.toObject();
    TraceManuallyBarrieredEdge(trc, &obj, "dictNext object");
    if (obj != dictNext.toObject()) {
      dictNext.setObject(obj);
    }
  }

  if (hasGetterObject()) {
    TraceManuallyBarrieredEdge(trc, &asAccessorShape().getterObj, "getter");
  }
  if (hasSetterObject()) {
    TraceManuallyBarrieredEdge(trc, &asAccessorShape().setterObj, "setter");
  }
}
inline void js::GCMarker::eagerlyMarkChildren(Shape* shape) {
  MOZ_ASSERT(shape->isMarked(markColor()));

  do {
    // Special case: if a base shape has a shape table then all its pointers
    // must point to this shape or an anscestor.  Since these pointers will
    // be traced by this loop they do not need to be traced here as well.
    BaseShape* base = shape->base();
    CheckTraversedEdge(shape, base);
    if (mark(base)) {
      MOZ_ASSERT(base->canSkipMarkingShapeCache(shape));
      base->traceChildrenSkipShapeCache(this);
    }

    traverseEdge(shape, shape->propidRef().get());

    // Normally only the last shape in a dictionary list can have a pointer to
    // an object here, but it's possible that we can see this if we trace
    // barriers while removing a shape from a dictionary list.
    if (shape->dictNext.isObject()) {
      traverseEdge(shape, shape->dictNext.toObject());
    }

    // When triggered between slices on behalf of a barrier, these
    // objects may reside in the nursery, so require an extra check.
    // FIXME: Bug 1157967 - remove the isTenured checks.
    if (shape->hasGetterObject() && shape->getterObject()->isTenured()) {
      traverseEdge(shape, shape->getterObject());
    }
    if (shape->hasSetterObject() && shape->setterObject()->isTenured()) {
      traverseEdge(shape, shape->setterObject());
    }

    shape = shape->previous();
  } while (shape && mark(shape));
}

void JSString::traceChildren(JSTracer* trc) {
  if (hasBase()) {
    traceBase(trc);
  } else if (isRope()) {
    asRope().traceChildren(trc);
  }
}
inline void GCMarker::eagerlyMarkChildren(JSString* str) {
  if (str->isLinear()) {
    eagerlyMarkChildren(&str->asLinear());
  } else {
    eagerlyMarkChildren(&str->asRope());
  }
}

void JSString::traceBase(JSTracer* trc) {
  MOZ_ASSERT(hasBase());
  TraceManuallyBarrieredEdge(trc, &d.s.u3.base, "base");
}
inline void js::GCMarker::eagerlyMarkChildren(JSLinearString* linearStr) {
  AssertShouldMarkInZone(linearStr);
  MOZ_ASSERT(linearStr->isMarkedAny());
  MOZ_ASSERT(linearStr->JSString::isLinear());

  // Use iterative marking to avoid blowing out the stack.
  while (linearStr->hasBase()) {
    linearStr = linearStr->base();
    MOZ_ASSERT(linearStr->JSString::isLinear());
    if (linearStr->isPermanentAtom()) {
      break;
    }
    AssertShouldMarkInZone(linearStr);
    if (!mark(static_cast<JSString*>(linearStr))) {
      break;
    }
  }
}

void JSRope::traceChildren(JSTracer* trc) {
  js::TraceManuallyBarrieredEdge(trc, &d.s.u2.left, "left child");
  js::TraceManuallyBarrieredEdge(trc, &d.s.u3.right, "right child");
}
inline void js::GCMarker::eagerlyMarkChildren(JSRope* rope) {
  // This function tries to scan the whole rope tree using the marking stack
  // as temporary storage. If that becomes full, the unscanned ropes are
  // added to the delayed marking list. When the function returns, the
  // marking stack is at the same depth as it was on entry. This way we avoid
  // using tags when pushing ropes to the stack as ropes never leak to other
  // users of the stack. This also assumes that a rope can only point to
  // other ropes or linear strings, it cannot refer to GC things of other
  // types.
  gc::MarkStack& stack = currentStack();
  size_t savedPos = stack.position();
  JS_DIAGNOSTICS_ASSERT(rope->getTraceKind() == JS::TraceKind::String);
#ifdef JS_DEBUG
  static const size_t DEEP_ROPE_THRESHOLD = 100000;
  static const size_t ROPE_CYCLE_HISTORY = 100;
  DebugOnly<size_t> ropeDepth = 0;
  JSRope* history[ROPE_CYCLE_HISTORY];
#endif
  while (true) {
#ifdef JS_DEBUG
    if (++ropeDepth >= DEEP_ROPE_THRESHOLD) {
      // Bug 1011786 comment 294 - detect cyclic ropes. There are some
      // legitimate deep ropes, at least in tests. So if we hit a deep
      // rope, start recording the nodes we visit and check whether we
      // repeat. But do it on a finite window size W so that we're not
      // scanning the full history for every node. And only check every
      // Wth push, to add only constant overhead per node. This will only
      // catch cycles of size up to W (but it seems most likely that any
      // cycles will be size 1 or maybe 2.)
      if ((ropeDepth > DEEP_ROPE_THRESHOLD + ROPE_CYCLE_HISTORY) &&
          (ropeDepth % ROPE_CYCLE_HISTORY) == 0) {
        for (size_t i = 0; i < ROPE_CYCLE_HISTORY; i++) {
          MOZ_ASSERT(history[i] != rope, "cycle detected in rope");
        }
      }
      history[ropeDepth % ROPE_CYCLE_HISTORY] = rope;
    }
#endif

    JS_DIAGNOSTICS_ASSERT(rope->getTraceKind() == JS::TraceKind::String);
    JS_DIAGNOSTICS_ASSERT(rope->JSString::isRope());
    AssertShouldMarkInZone(rope);
    MOZ_ASSERT(rope->isMarkedAny());
    JSRope* next = nullptr;

    JSString* right = rope->rightChild();
    if (!right->isPermanentAtom() && mark(right)) {
      if (right->isLinear()) {
        eagerlyMarkChildren(&right->asLinear());
      } else {
        next = &right->asRope();
      }
    }

    JSString* left = rope->leftChild();
    if (!left->isPermanentAtom() && mark(left)) {
      if (left->isLinear()) {
        eagerlyMarkChildren(&left->asLinear());
      } else {
        // When both children are ropes, set aside the right one to
        // scan it later.
        if (next && !stack.pushTempRope(next)) {
          delayMarkingChildren(next);
        }
        next = &left->asRope();
      }
    }
    if (next) {
      rope = next;
    } else if (savedPos != stack.position()) {
      MOZ_ASSERT(savedPos < stack.position());
      rope = stack.popPtr().asTempRope();
    } else {
      break;
    }
  }
  MOZ_ASSERT(savedPos == stack.position());
}

static inline void TraceBindingNames(JSTracer* trc, BindingName* names,
                                     uint32_t length) {
  for (uint32_t i = 0; i < length; i++) {
    JSAtom* name = names[i].name();
    MOZ_ASSERT(name);
    TraceManuallyBarrieredEdge(trc, &name, "scope name");
  }
};
static inline void TraceNullableBindingNames(JSTracer* trc, BindingName* names,
                                             uint32_t length) {
  for (uint32_t i = 0; i < length; i++) {
    if (JSAtom* name = names[i].name()) {
      TraceManuallyBarrieredEdge(trc, &name, "scope name");
    }
  }
};
void BindingName::trace(JSTracer* trc) {
  if (JSAtom* atom = name()) {
    TraceManuallyBarrieredEdge(trc, &atom, "binding name");
  }
}
void BindingIter::trace(JSTracer* trc) {
  TraceNullableBindingNames(trc, names_, length_);
}
void LexicalScope::Data::trace(JSTracer* trc) {
  TraceBindingNames(trc, trailingNames.start(), length);
}
void FunctionScope::Data::trace(JSTracer* trc) {
  TraceNullableEdge(trc, &canonicalFunction, "scope canonical function");
  TraceNullableBindingNames(trc, trailingNames.start(), length);
}
void VarScope::Data::trace(JSTracer* trc) {
  TraceBindingNames(trc, trailingNames.start(), length);
}
void GlobalScope::Data::trace(JSTracer* trc) {
  TraceBindingNames(trc, trailingNames.start(), length);
}
void EvalScope::Data::trace(JSTracer* trc) {
  TraceBindingNames(trc, trailingNames.start(), length);
}
void ModuleScope::Data::trace(JSTracer* trc) {
  TraceNullableEdge(trc, &module, "scope module");
  TraceBindingNames(trc, trailingNames.start(), length);
}
void WasmInstanceScope::Data::trace(JSTracer* trc) {
  TraceNullableEdge(trc, &instance, "wasm instance");
  TraceBindingNames(trc, trailingNames.start(), length);
}
void WasmFunctionScope::Data::trace(JSTracer* trc) {
  TraceBindingNames(trc, trailingNames.start(), length);
}
void Scope::traceChildren(JSTracer* trc) {
  TraceNullableEdge(trc, &headerAndEnclosingScope_, "scope enclosing");
  TraceNullableEdge(trc, &environmentShape_, "scope env shape");
  applyScopeDataTyped([trc](auto data) { data->trace(trc); });
}
inline void js::GCMarker::eagerlyMarkChildren(Scope* scope) {
  do {
    if (scope->environmentShape()) {
      traverseEdge(scope, scope->environmentShape());
    }
    TrailingNamesArray* names = nullptr;
    uint32_t length = 0;
    switch (scope->kind()) {
      case ScopeKind::Function: {
        FunctionScope::Data& data = scope->as<FunctionScope>().data();
        traverseObjectEdge(scope, data.canonicalFunction);
        names = &data.trailingNames;
        length = data.length;
        break;
      }

      case ScopeKind::FunctionBodyVar: {
        VarScope::Data& data = scope->as<VarScope>().data();
        names = &data.trailingNames;
        length = data.length;
        break;
      }

      case ScopeKind::Lexical:
      case ScopeKind::SimpleCatch:
      case ScopeKind::Catch:
      case ScopeKind::NamedLambda:
      case ScopeKind::StrictNamedLambda:
      case ScopeKind::FunctionLexical: {
        LexicalScope::Data& data = scope->as<LexicalScope>().data();
        names = &data.trailingNames;
        length = data.length;
        break;
      }

      case ScopeKind::Global:
      case ScopeKind::NonSyntactic: {
        GlobalScope::Data& data = scope->as<GlobalScope>().data();
        names = &data.trailingNames;
        length = data.length;
        break;
      }

      case ScopeKind::Eval:
      case ScopeKind::StrictEval: {
        EvalScope::Data& data = scope->as<EvalScope>().data();
        names = &data.trailingNames;
        length = data.length;
        break;
      }

      case ScopeKind::Module: {
        ModuleScope::Data& data = scope->as<ModuleScope>().data();
        traverseObjectEdge(scope, data.module);
        names = &data.trailingNames;
        length = data.length;
        break;
      }

      case ScopeKind::With:
        break;

      case ScopeKind::WasmInstance: {
        WasmInstanceScope::Data& data = scope->as<WasmInstanceScope>().data();
        traverseObjectEdge(scope, data.instance);
        names = &data.trailingNames;
        length = data.length;
        break;
      }

      case ScopeKind::WasmFunction: {
        WasmFunctionScope::Data& data = scope->as<WasmFunctionScope>().data();
        names = &data.trailingNames;
        length = data.length;
        break;
      }
    }
    if (scope->kind_ == ScopeKind::Function) {
      for (uint32_t i = 0; i < length; i++) {
        if (JSAtom* name = names->get(i).name()) {
          traverseStringEdge(scope, name);
        }
      }
    } else {
      for (uint32_t i = 0; i < length; i++) {
        traverseStringEdge(scope, names->get(i).name());
      }
    }
    scope = scope->enclosing();
  } while (scope && mark(scope));
}

void js::ObjectGroup::traceChildren(JSTracer* trc) {
  AutoSweepObjectGroup sweep(this);

  if (!trc->canSkipJsids()) {
    unsigned count = getPropertyCount(sweep);
    for (unsigned i = 0; i < count; i++) {
      if (ObjectGroup::Property* prop = getProperty(sweep, i)) {
        TraceEdge(trc, &prop->id, "group_property");
      }
    }
  }

  if (proto().isObject()) {
    TraceEdge(trc, &proto(), "group_proto");
  }

  // Note: the realm's global can be nullptr if we GC while creating the global.
  if (JSObject* global = realm()->unsafeUnbarrieredMaybeGlobal()) {
    TraceManuallyBarrieredEdge(trc, &global, "group_global");
  }

  if (newScript(sweep)) {
    newScript(sweep)->trace(trc);
  }

  if (maybePreliminaryObjects(sweep)) {
    maybePreliminaryObjects(sweep)->trace(trc);
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
void js::GCMarker::lazilyMarkChildren(ObjectGroup* group) {
  AutoSweepObjectGroup sweep(group);
  unsigned count = group->getPropertyCount(sweep);
  for (unsigned i = 0; i < count; i++) {
    if (ObjectGroup::Property* prop = group->getProperty(sweep, i)) {
      traverseEdge(group, prop->id.get());
    }
  }

  if (group->proto().isObject()) {
    traverseEdge(group, group->proto().toObject());
  }

  // Note: the realm's global can be nullptr if we GC while creating the global.
  if (GlobalObject* global = group->realm()->unsafeUnbarrieredMaybeGlobal()) {
    traverseEdge(group, static_cast<JSObject*>(global));
  }

  if (group->newScript(sweep)) {
    group->newScript(sweep)->trace(this);
  }

  if (group->maybePreliminaryObjects(sweep)) {
    group->maybePreliminaryObjects(sweep)->trace(this);
  }

  if (TypeDescr* descr = group->maybeTypeDescr()) {
    traverseEdge(group, static_cast<JSObject*>(descr));
  }

  if (JSFunction* fun = group->maybeInterpretedFunction()) {
    traverseEdge(group, static_cast<JSObject*>(fun));
  }
}

void JS::BigInt::traceChildren(JSTracer* trc) {}

template <typename Functor>
static void VisitTraceList(const Functor& f, const uint32_t* traceList,
                           uint8_t* memory);

// Call the trace hook set on the object, if present. If further tracing of
// NativeObject fields is required, this will return the native object.
enum class CheckGeneration { DoChecks, NoChecks };
template <typename Functor>
static inline NativeObject* CallTraceHook(Functor&& f, JSTracer* trc,
                                          JSObject* obj,
                                          CheckGeneration check) {
  const JSClass* clasp = obj->getClass();
  MOZ_ASSERT(clasp);
  MOZ_ASSERT(obj->isNative() == clasp->isNative());

  if (!clasp->hasTrace()) {
    return &obj->as<NativeObject>();
  }

  if (clasp->isTrace(InlineTypedObject::obj_trace)) {
    Shape** pshape = obj->as<InlineTypedObject>().addressOfShapeFromGC();
    f(pshape);

    InlineTypedObject& tobj = obj->as<InlineTypedObject>();
    if (tobj.typeDescr().hasTraceList()) {
      VisitTraceList(f, tobj.typeDescr().traceList(),
                     tobj.inlineTypedMemForGC());
    }

    return nullptr;
  }

  AutoSetTracingSource asts(trc, obj);
  clasp->doTrace(trc, obj);

  if (!clasp->isNative()) {
    return nullptr;
  }
  return &obj->as<NativeObject>();
}

template <typename Functor>
static void VisitTraceList(const Functor& f, const uint32_t* traceList,
                           uint8_t* memory) {
  size_t stringCount = *traceList++;
  size_t objectCount = *traceList++;
  size_t valueCount = *traceList++;
  for (size_t i = 0; i < stringCount; i++) {
    f(reinterpret_cast<JSString**>(memory + *traceList));
    traceList++;
  }
  for (size_t i = 0; i < objectCount; i++) {
    JSObject** objp = reinterpret_cast<JSObject**>(memory + *traceList);
    if (*objp) {
      f(objp);
    }
    traceList++;
  }
  for (size_t i = 0; i < valueCount; i++) {
    f(reinterpret_cast<Value*>(memory + *traceList));
    traceList++;
  }
}

/*** Mark-stack Marking *****************************************************/

GCMarker::MarkQueueProgress GCMarker::processMarkQueue() {
#ifdef DEBUG
  if (markQueue.empty()) {
    return QueueComplete;
  }

  GCRuntime& gcrt = runtime()->gc;
  if (queueMarkColor == mozilla::Some(MarkColor::Gray) &&
      gcrt.state() != State::Sweep) {
    return QueueSuspended;
  }

  // If the queue wants to be gray marking, but we've pushed a black object
  // since set-color-gray was processed, then we can't switch to gray and must
  // again wait until gray marking is possible.
  //
  // Remove this code if the restriction against marking gray during black is
  // relaxed.
  if (queueMarkColor == mozilla::Some(MarkColor::Gray) && hasBlackEntries()) {
    return QueueSuspended;
  }

  // If the queue wants to be marking a particular color, switch to that color.
  // In any case, restore the mark color to whatever it was when we entered
  // this function.
  AutoSetMarkColor autoRevertColor(*this, queueMarkColor.valueOr(markColor()));

  // Process the mark queue by taking each object in turn, pushing it onto the
  // mark stack, and processing just the top element with processMarkStackTop
  // without recursing into reachable objects.
  while (queuePos < markQueue.length()) {
    Value val = markQueue[queuePos++].get().unbarrieredGet();
    if (val.isObject()) {
      JSObject* obj = &val.toObject();
      JS::Zone* zone = obj->zone();
      if (!zone->isGCMarking() || obj->isMarkedAtLeast(markColor())) {
        continue;
      }

      // If we have started sweeping, obey sweep group ordering. But note that
      // we will first be called during the initial sweep slice, when the sweep
      // group indexes have not yet been computed. In that case, we can mark
      // freely.
      if (gcrt.state() == State::Sweep && gcrt.initialState != State::Sweep) {
        if (zone->gcSweepGroupIndex < gcrt.getCurrentSweepGroupIndex()) {
          // Too late. This must have been added after we started collecting,
          // and we've already processed its sweep group. Skip it.
          continue;
        }
        if (zone->gcSweepGroupIndex > gcrt.getCurrentSweepGroupIndex()) {
          // Not ready yet. Wait until we reach the object's sweep group.
          queuePos--;
          return QueueSuspended;
        }
      }

      if (markColor() == MarkColor::Gray && zone->isGCMarkingBlackOnly()) {
        // Have not yet reached the point where we can mark this object, so
        // continue with the GC.
        queuePos--;
        return QueueSuspended;
      }

      // Mark the object and push it onto the stack.
      traverse(obj);

      if (isMarkStackEmpty()) {
        if (obj->asTenured().arena()->onDelayedMarkingList()) {
          AutoEnterOOMUnsafeRegion oomUnsafe;
          oomUnsafe.crash("mark queue OOM");
        }
      }

      // Process just the one object that is now on top of the mark stack,
      // possibly pushing more stuff onto the stack.
      if (isMarkStackEmpty()) {
        MOZ_ASSERT(obj->asTenured().arena()->onDelayedMarkingList());
        // If we overflow the stack here and delay marking, then we won't be
        // testing what we think we're testing.
        AutoEnterOOMUnsafeRegion oomUnsafe;
        oomUnsafe.crash("Overflowed stack while marking test queue");
      }

      SliceBudget unlimited = SliceBudget::unlimited();
      processMarkStackTop(unlimited);
    } else if (val.isString()) {
      JSLinearString* str = &val.toString()->asLinear();
      if (js::StringEqualsLiteral(str, "yield") && gcrt.isIncrementalGc()) {
        return QueueYielded;
      } else if (js::StringEqualsLiteral(str, "enter-weak-marking-mode") ||
                 js::StringEqualsLiteral(str, "abort-weak-marking-mode")) {
        if (state == MarkingState::RegularMarking) {
          // We can't enter weak marking mode at just any time, so instead
          // we'll stop processing the queue and continue on with the GC. Once
          // we enter weak marking mode, we can continue to the rest of the
          // queue. Note that we will also suspend for aborting, and then abort
          // the earliest following weak marking mode.
          queuePos--;
          return QueueSuspended;
        }
        if (js::StringEqualsLiteral(str, "abort-weak-marking-mode")) {
          abortLinearWeakMarking();
        }
      } else if (js::StringEqualsLiteral(str, "drain")) {
        auto unlimited = SliceBudget::unlimited();
        MOZ_RELEASE_ASSERT(markUntilBudgetExhausted(unlimited));
      } else if (js::StringEqualsLiteral(str, "set-color-gray")) {
        queueMarkColor = mozilla::Some(MarkColor::Gray);
        if (gcrt.state() != State::Sweep) {
          // Cannot mark gray yet, so continue with the GC.
          queuePos--;
          return QueueSuspended;
        }
        setMarkColor(MarkColor::Gray);
      } else if (js::StringEqualsLiteral(str, "set-color-black")) {
        queueMarkColor = mozilla::Some(MarkColor::Black);
        setMarkColor(MarkColor::Black);
      } else if (js::StringEqualsLiteral(str, "unset-color")) {
        queueMarkColor.reset();
      }
    }
  }
#endif

  return QueueComplete;
}

bool GCMarker::markUntilBudgetExhausted(SliceBudget& budget) {
#ifdef DEBUG
  MOZ_ASSERT(!strictCompartmentChecking);
  strictCompartmentChecking = true;
  auto acc = mozilla::MakeScopeExit([&] { strictCompartmentChecking = false; });
#endif

  if (budget.isOverBudget()) {
    return false;
  }

  // This method leaves the mark color as it found it.
  AutoSetMarkColor autoSetBlack(*this, MarkColor::Black);

  // Change representation of value arrays on the stack while the mutator
  // runs.
  auto svr = mozilla::MakeScopeExit([&] { saveValueRanges(); });

  for (;;) {
    while (hasBlackEntries()) {
      MOZ_ASSERT(markColor() == MarkColor::Black);
      processMarkStackTop(budget);
      if (budget.isOverBudget()) {
        return false;
      }
    }

    if (hasGrayEntries()) {
      AutoSetMarkColor autoSetGray(*this, MarkColor::Gray);
      do {
        processMarkStackTop(budget);
        if (budget.isOverBudget()) {
          return false;
        }
      } while (hasGrayEntries());
    }

    if (hasBlackEntries()) {
      // We can end up marking black during gray marking in the following case:
      // a WeakMap has a CCW key whose delegate (target) is black, and during
      // gray marking we mark the map (gray). The delegate's color will be
      // propagated to the key. (And we can't avoid this by marking the key
      // gray, because even though the value will end up gray in either case,
      // the WeakMap entry must be preserved because the CCW could get
      // collected and then we could re-wrap the delegate and look it up in the
      // map again, and need to get back the original value.)
      continue;
    }

    if (!hasDelayedChildren()) {
      break;
    }

    /*
     * Mark children of things that caused too deep recursion during the
     * above tracing. Don't do this until we're done with everything
     * else.
     */
    if (!markAllDelayedChildren(budget)) {
      return false;
    }
  }

  return true;
}

inline static bool ObjectDenseElementsMayBeMarkable(NativeObject* nobj) {
  /*
   * For arrays that are large enough it's worth checking the type information
   * to see if the object's elements contain any GC pointers.  If not, we
   * don't need to trace them.
   */
  const unsigned MinElementsLength = 32;
  if (nobj->getDenseInitializedLength() < MinElementsLength ||
      nobj->isSingleton()) {
    return true;
  }

  ObjectGroup* group = nobj->group();
  if (group->needsSweep() || group->unknownPropertiesDontCheckGeneration()) {
    return true;
  }

  MOZ_ASSERT(IsTypeInferenceEnabled());

  // This typeset doesn't escape this function so avoid sweeping here.
  HeapTypeSet* typeSet = group->maybeGetPropertyDontCheckGeneration(JSID_VOID);
  if (!typeSet) {
    return true;
  }

  static const uint32_t flagMask = TYPE_FLAG_STRING | TYPE_FLAG_SYMBOL |
                                   TYPE_FLAG_LAZYARGS | TYPE_FLAG_ANYOBJECT |
                                   TYPE_FLAG_BIGINT;
  bool mayBeMarkable =
      typeSet->hasAnyFlag(flagMask) || typeSet->getObjectCount() != 0;

#ifdef DEBUG
  if (!mayBeMarkable) {
    const Value* elements = nobj->getDenseElementsAllowCopyOnWrite();
    for (unsigned i = 0; i < nobj->getDenseInitializedLength(); i++) {
      MOZ_ASSERT(!elements[i].isGCThing());
    }
  }
#endif

  return mayBeMarkable;
}

static inline void CheckForCompartmentMismatch(JSObject* obj, JSObject* obj2) {
#ifdef DEBUG
  if (MOZ_UNLIKELY(obj->compartment() != obj2->compartment())) {
    fprintf(
        stderr,
        "Compartment mismatch in pointer from %s object slot to %s object\n",
        obj->getClass()->name, obj2->getClass()->name);
    MOZ_CRASH("Compartment mismatch");
  }
#endif
}

inline void GCMarker::processMarkStackTop(SliceBudget& budget) {
  /*
   * The function uses explicit goto and implements the scanning of the
   * object directly. It allows to eliminate the tail recursion and
   * significantly improve the marking performance, see bug 641025.
   */
  HeapSlot* vp;
  HeapSlot* end;
  JSObject* obj;

  gc::MarkStack& stack = currentStack();

  switch (stack.peekTag()) {
    case MarkStack::ValueArrayTag: {
      auto array = stack.popValueArray();
      obj = array.ptr.asValueArrayObject();
      vp = array.start;
      end = array.end;
      goto scan_value_array;
    }

    case MarkStack::ObjectTag: {
      obj = stack.popPtr().as<JSObject>();
      AssertShouldMarkInZone(obj);
      goto scan_obj;
    }

    case MarkStack::GroupTag: {
      auto group = stack.popPtr().as<ObjectGroup>();
      return lazilyMarkChildren(group);
    }

    case MarkStack::JitCodeTag: {
      auto code = stack.popPtr().as<jit::JitCode>();
      AutoSetTracingSource asts(this, code);
      return code->traceChildren(this);
    }

    case MarkStack::ScriptTag: {
      auto script = stack.popPtr().as<BaseScript>();
      AutoSetTracingSource asts(this, script);
      return script->traceChildren(this);
    }

    case MarkStack::SavedValueArrayTag: {
      auto savedArray = stack.popSavedValueArray();
      JSObject* obj = savedArray.ptr.asSavedValueArrayObject();
      if (restoreValueArray(savedArray, &vp, &end)) {
        pushValueArray(obj, vp, end);
      } else {
        repush(obj);
      }
      return;
    }

    default:
      MOZ_CRASH("Invalid tag in mark stack");
  }
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
      traverseEdge(obj, v.toString());
    } else if (v.isObject()) {
      JSObject* obj2 = &v.toObject();
#ifdef DEBUG
      if (!obj2) {
        fprintf(stderr,
                "processMarkStackTop found ObjectValue(nullptr) "
                "at %zu Values from end of array in object:\n",
                size_t(end - (vp - 1)));
        DumpObject(obj);
      }
#endif
      CheckForCompartmentMismatch(obj, obj2);
      if (mark(obj2)) {
        // Save the rest of this value array for later and start scanning obj2's
        // children.
        pushValueArray(obj, vp, end);
        obj = obj2;
        goto scan_obj;
      }
    } else if (v.isSymbol()) {
      traverseEdge(obj, v.toSymbol());
    } else if (v.isBigInt()) {
      traverseEdge(obj, v.toBigInt());
    } else if (v.isPrivateGCThing()) {
      // v.toGCCellPtr cannot be inlined, so construct one manually.
      Cell* cell = v.toGCThing();
      traverseEdge(obj, JS::GCCellPtr(cell, cell->getTraceKind()));
    }
  }
  return;

scan_obj : {
  AssertShouldMarkInZone(obj);

  budget.step();
  if (budget.isOverBudget()) {
    repush(obj);
    return;
  }

  markImplicitEdges(obj);
  traverseEdge(obj, obj->groupRaw());

  NativeObject* nobj = CallTraceHook(
      [this, obj](auto thingp) { this->traverseEdge(obj, *thingp); }, this, obj,
      CheckGeneration::DoChecks);
  if (!nobj) {
    return;
  }

  Shape* shape = nobj->lastProperty();
  traverseEdge(obj, shape);

  unsigned nslots = nobj->slotSpan();

  do {
    if (nobj->hasEmptyElements()) {
      break;
    }

    if (nobj->denseElementsAreCopyOnWrite()) {
      JSObject* owner = nobj->getElementsHeader()->ownerObject();
      if (owner != nobj) {
        traverseEdge(obj, owner);
        break;
      }
    }

    if (!ObjectDenseElementsMayBeMarkable(nobj)) {
      break;
    }

    vp = nobj->getDenseElementsAllowCopyOnWrite();
    end = vp + nobj->getDenseInitializedLength();

    if (!nslots) {
      goto scan_value_array;
    }
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

/*
 * During incremental GC, we return from drainMarkStack without having processed
 * the entire stack. At that point, JS code can run and reallocate slot arrays
 * that are stored on the stack. To prevent this from happening, we replace all
 * ValueArrayTag stack items with SavedValueArrayTag. In the latter, slots
 * pointers are replaced with slot indexes, and slot array end pointers are
 * replaced with the kind of index (properties vs. elements).
 */

void GCMarker::saveValueRanges() {
  gc::MarkStack* stacks[2] = {&stack, &auxStack};
  for (auto& stack : stacks) {
    MarkStackIter iter(*stack);
    while (!iter.done()) {
      auto tag = iter.peekTag();
      if (tag == MarkStack::ValueArrayTag) {
        const auto& array = iter.peekValueArray();
        auto savedArray = saveValueRange(array);
        iter.saveValueArray(savedArray);
        iter.nextArray();
      } else if (tag == MarkStack::SavedValueArrayTag) {
        iter.nextArray();
      } else {
        iter.nextPtr();
      }
    }

    // This is also a convenient point to poison unused stack memory.
    stack->poisonUnused();
  }
}

bool GCMarker::restoreValueArray(const MarkStack::SavedValueArray& savedArray,
                                 HeapSlot** vpp, HeapSlot** endp) {
  JSObject* objArg = savedArray.ptr.asSavedValueArrayObject();
  if (!objArg->isNative()) {
    return false;
  }

  auto array = restoreValueArray(savedArray);
  *vpp = array.start;
  *endp = array.end;
  return true;
}

MarkStack::SavedValueArray GCMarker::saveValueRange(
    const MarkStack::ValueArray& array) {
  NativeObject* obj = &array.ptr.asValueArrayObject()->as<NativeObject>();
  MOZ_ASSERT(obj->isNative());

  uintptr_t index;
  HeapSlot::Kind kind;
  HeapSlot* vp = obj->getDenseElementsAllowCopyOnWrite();
  if (array.end == vp + obj->getDenseInitializedLength()) {
    MOZ_ASSERT(array.start >= vp);
    // Add the number of shifted elements here (and subtract in
    // restoreValueArray) to ensure shift() calls on the array
    // are handled correctly.
    index = obj->unshiftedIndex(array.start - vp);
    kind = HeapSlot::Element;
  } else {
    HeapSlot* vp = obj->fixedSlots();
    unsigned nfixed = obj->numFixedSlots();
    if (array.start == array.end) {
      index = obj->slotSpan();
    } else if (array.start >= vp && array.start < vp + nfixed) {
      MOZ_ASSERT(array.end == vp + std::min(nfixed, obj->slotSpan()));
      index = array.start - vp;
    } else {
      MOZ_ASSERT(array.start >= obj->slots_ &&
                 array.end == obj->slots_ + obj->slotSpan() - nfixed);
      index = (array.start - obj->slots_) + nfixed;
    }
    kind = HeapSlot::Slot;
  }

  return MarkStack::SavedValueArray(obj, index, kind);
}

MarkStack::ValueArray GCMarker::restoreValueArray(
    const MarkStack::SavedValueArray& savedArray) {
  NativeObject* obj =
      &savedArray.ptr.asSavedValueArrayObject()->as<NativeObject>();
  HeapSlot* start = nullptr;
  HeapSlot* end = nullptr;

  uintptr_t index = savedArray.index;
  if (savedArray.kind == HeapSlot::Element) {
    uint32_t initlen = obj->getDenseInitializedLength();

    // Account for shifted elements.
    uint32_t numShifted = obj->getElementsHeader()->numShiftedElements();
    index = (numShifted < index) ? index - numShifted : 0;

    HeapSlot* vp = obj->getDenseElementsAllowCopyOnWrite();
    if (index < initlen) {
      start = vp + index;
      end = vp + initlen;
    } else {
      /* The object shrunk, in which case no scanning is needed. */
      start = end = vp;
    }
  } else {
    MOZ_ASSERT(savedArray.kind == HeapSlot::Slot);
    HeapSlot* vp = obj->fixedSlots();
    unsigned nfixed = obj->numFixedSlots();
    unsigned nslots = obj->slotSpan();
    if (index < nslots) {
      if (index < nfixed) {
        start = vp + index;
        end = vp + std::min(nfixed, nslots);
      } else {
        start = obj->slots_ + index - nfixed;
        end = obj->slots_ + nslots - nfixed;
      }
    } else {
      /* The object shrunk, in which case no scanning is needed. */
      start = end = vp;
    }
  }

  return MarkStack::ValueArray(obj, start, end);
}

/*** Mark Stack *************************************************************/

static_assert(sizeof(MarkStack::TaggedPtr) == sizeof(uintptr_t),
              "A TaggedPtr should be the same size as a pointer");
static_assert(sizeof(MarkStack::ValueArray) ==
                  sizeof(MarkStack::SavedValueArray),
              "ValueArray and SavedValueArray should be the same size");
static_assert(
    (sizeof(MarkStack::ValueArray) % sizeof(uintptr_t)) == 0,
    "ValueArray and SavedValueArray should be multiples of the pointer size");

static const size_t ValueArrayWords =
    sizeof(MarkStack::ValueArray) / sizeof(uintptr_t);

template <typename T>
struct MapTypeToMarkStackTag {};
template <>
struct MapTypeToMarkStackTag<JSObject*> {
  static const auto value = MarkStack::ObjectTag;
};
template <>
struct MapTypeToMarkStackTag<ObjectGroup*> {
  static const auto value = MarkStack::GroupTag;
};
template <>
struct MapTypeToMarkStackTag<jit::JitCode*> {
  static const auto value = MarkStack::JitCodeTag;
};
template <>
struct MapTypeToMarkStackTag<BaseScript*> {
  static const auto value = MarkStack::ScriptTag;
};

static inline bool TagIsArrayTag(MarkStack::Tag tag) {
  return tag == MarkStack::ValueArrayTag ||
         tag == MarkStack::SavedValueArrayTag;
}

inline MarkStack::TaggedPtr::TaggedPtr(Tag tag, Cell* ptr)
    : bits(tag | uintptr_t(ptr)) {
  assertValid();
}

inline MarkStack::Tag MarkStack::TaggedPtr::tag() const {
  auto tag = Tag(bits & TagMask);
  MOZ_ASSERT(tag <= LastTag);
  return tag;
}

inline Cell* MarkStack::TaggedPtr::ptr() const {
  return reinterpret_cast<Cell*>(bits & ~TagMask);
}

inline void MarkStack::TaggedPtr::assertValid() const {
  mozilla::Unused << tag();
  MOZ_ASSERT(IsCellPointerValid(ptr()));
}

template <typename T>
inline T* MarkStack::TaggedPtr::as() const {
  MOZ_ASSERT(tag() == MapTypeToMarkStackTag<T*>::value);
  MOZ_ASSERT(ptr()->isTenured());
  MOZ_ASSERT(ptr()->is<T>());
  return static_cast<T*>(ptr());
}

inline JSObject* MarkStack::TaggedPtr::asValueArrayObject() const {
  MOZ_ASSERT(tag() == ValueArrayTag);
  MOZ_ASSERT(ptr()->isTenured());
  MOZ_ASSERT(ptr()->is<JSObject>());
  return static_cast<JSObject*>(ptr());
}

inline JSObject* MarkStack::TaggedPtr::asSavedValueArrayObject() const {
  MOZ_ASSERT(tag() == SavedValueArrayTag);
  MOZ_ASSERT(ptr()->isTenured());
  MOZ_ASSERT(ptr()->is<JSObject>());
  return static_cast<JSObject*>(ptr());
}

inline JSRope* MarkStack::TaggedPtr::asTempRope() const {
  MOZ_ASSERT(tag() == TempRopeTag);
  MOZ_ASSERT(ptr()->is<JSString>());
  return static_cast<JSRope*>(ptr());
}

inline MarkStack::ValueArray::ValueArray(JSObject* obj, HeapSlot* startArg,
                                         HeapSlot* endArg)
    : end(endArg), start(startArg), ptr(ValueArrayTag, obj) {
  assertValid();
}

inline void MarkStack::ValueArray::assertValid() const {
  ptr.assertValid();
  MOZ_ASSERT(ptr.tag() == MarkStack::ValueArrayTag);
  MOZ_ASSERT(start);
  MOZ_ASSERT(end);
  MOZ_ASSERT(uintptr_t(start) <= uintptr_t(end));
  MOZ_ASSERT((uintptr_t(end) - uintptr_t(start)) % sizeof(Value) == 0);
}

inline MarkStack::SavedValueArray::SavedValueArray(JSObject* obj,
                                                   size_t indexArg,
                                                   HeapSlot::Kind kindArg)
    : kind(kindArg), index(indexArg), ptr(SavedValueArrayTag, obj) {
  assertValid();
}

inline void MarkStack::SavedValueArray::assertValid() const {
  ptr.assertValid();
  MOZ_ASSERT(ptr.tag() == MarkStack::SavedValueArrayTag);
  MOZ_ASSERT(kind == HeapSlot::Slot || kind == HeapSlot::Element);
}

MarkStack::MarkStack(size_t maxCapacity)
    : topIndex_(0),
      maxCapacity_(maxCapacity)
#ifdef DEBUG
      ,
      iteratorCount_(0)
#endif
{
}

MarkStack::~MarkStack() {
  MOZ_ASSERT(isEmpty());
  MOZ_ASSERT(iteratorCount_ == 0);
}

bool MarkStack::init(JSGCMode gcMode, StackType which) {
  MOZ_ASSERT(isEmpty());

  return setCapacityForMode(gcMode, which);
}

void MarkStack::setGCMode(JSGCMode gcMode) {
  // Ignore failure to resize the stack and keep using the existing stack.
  mozilla::Unused << setCapacityForMode(gcMode, MainStack);
}

bool MarkStack::setCapacityForMode(JSGCMode mode, StackType which) {
  size_t capacity;

  if (which == AuxiliaryStack) {
    capacity = SMALL_MARK_STACK_BASE_CAPACITY;
  } else {
    switch (mode) {
      case JSGC_MODE_GLOBAL:
      case JSGC_MODE_ZONE:
        capacity = NON_INCREMENTAL_MARK_STACK_BASE_CAPACITY;
        break;
      case JSGC_MODE_INCREMENTAL:
      case JSGC_MODE_ZONE_INCREMENTAL:
        capacity = INCREMENTAL_MARK_STACK_BASE_CAPACITY;
        break;
      default:
        MOZ_CRASH("bad gc mode");
    }
  }

  if (capacity > maxCapacity_) {
    capacity = maxCapacity_;
  }

  return resize(capacity);
}

void MarkStack::setMaxCapacity(size_t maxCapacity) {
  MOZ_ASSERT(maxCapacity != 0);
  MOZ_ASSERT(isEmpty());

  maxCapacity_ = maxCapacity;
  if (capacity() > maxCapacity_) {
    // If the realloc fails, just keep using the existing stack; it's
    // not ideal but better than failing.
    mozilla::Unused << resize(maxCapacity_);
  }
}

inline MarkStack::TaggedPtr* MarkStack::topPtr() { return &stack()[topIndex_]; }

inline bool MarkStack::pushTaggedPtr(Tag tag, Cell* ptr) {
  if (!ensureSpace(1)) {
    return false;
  }

  *topPtr() = TaggedPtr(tag, ptr);
  topIndex_++;
  return true;
}

template <typename T>
inline bool MarkStack::push(T* ptr) {
  return pushTaggedPtr(MapTypeToMarkStackTag<T*>::value, ptr);
}

inline bool MarkStack::pushTempRope(JSRope* rope) {
  return pushTaggedPtr(TempRopeTag, rope);
}

inline bool MarkStack::push(JSObject* obj, HeapSlot* start, HeapSlot* end) {
  return push(ValueArray(obj, start, end));
}

inline bool MarkStack::push(const ValueArray& array) {
  array.assertValid();

  if (!ensureSpace(ValueArrayWords)) {
    return false;
  }

  *reinterpret_cast<ValueArray*>(topPtr()) = array;
  topIndex_ += ValueArrayWords;
  MOZ_ASSERT(position() <= capacity());
  MOZ_ASSERT(peekTag() == ValueArrayTag);
  return true;
}

inline bool MarkStack::push(const SavedValueArray& array) {
  array.assertValid();

  if (!ensureSpace(ValueArrayWords)) {
    return false;
  }

  *reinterpret_cast<SavedValueArray*>(topPtr()) = array;
  topIndex_ += ValueArrayWords;
  MOZ_ASSERT(position() <= capacity());
  MOZ_ASSERT(peekTag() == SavedValueArrayTag);
  return true;
}

inline const MarkStack::TaggedPtr& MarkStack::peekPtr() const {
  return stack()[topIndex_ - 1];
}

inline MarkStack::Tag MarkStack::peekTag() const { return peekPtr().tag(); }

inline MarkStack::TaggedPtr MarkStack::popPtr() {
  MOZ_ASSERT(!isEmpty());
  MOZ_ASSERT(!TagIsArrayTag(peekTag()));
  peekPtr().assertValid();
  topIndex_--;
  return *topPtr();
}

inline MarkStack::ValueArray MarkStack::popValueArray() {
  MOZ_ASSERT(peekTag() == ValueArrayTag);
  MOZ_ASSERT(position() >= ValueArrayWords);

  topIndex_ -= ValueArrayWords;
  const auto& array = *reinterpret_cast<ValueArray*>(topPtr());
  array.assertValid();
  return array;
}

inline MarkStack::SavedValueArray MarkStack::popSavedValueArray() {
  MOZ_ASSERT(peekTag() == SavedValueArrayTag);
  MOZ_ASSERT(position() >= ValueArrayWords);

  topIndex_ -= ValueArrayWords;
  const auto& array = *reinterpret_cast<SavedValueArray*>(topPtr());
  array.assertValid();
  return array;
}

inline bool MarkStack::ensureSpace(size_t count) {
  if ((topIndex_ + count) <= capacity()) {
    return !js::oom::ShouldFailWithOOM();
  }

  return enlarge(count);
}

bool MarkStack::enlarge(size_t count) {
  size_t newCapacity = std::min(maxCapacity_.ref(), capacity() * 2);
  if (newCapacity < capacity() + count) {
    return false;
  }

  return resize(newCapacity);
}

bool MarkStack::resize(size_t newCapacity) {
  MOZ_ASSERT(newCapacity != 0);
  if (!stack().resize(newCapacity)) {
    return false;
  }

  poisonUnused();
  return true;
}

inline void MarkStack::poisonUnused() {
  static_assert((JS_FRESH_MARK_STACK_PATTERN & TagMask) > LastTag,
                "The mark stack poison pattern must not look like a valid "
                "tagged pointer");

  AlwaysPoison(stack().begin() + topIndex_, JS_FRESH_MARK_STACK_PATTERN,
               stack().capacity() - topIndex_, MemCheckKind::MakeUndefined);
}

size_t MarkStack::sizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return stack().sizeOfExcludingThis(mallocSizeOf);
}

MarkStackIter::MarkStackIter(MarkStack& stack)
    : stack_(stack), pos_(stack.position()) {
#ifdef DEBUG
  stack.iteratorCount_++;
#endif
}

MarkStackIter::~MarkStackIter() {
#ifdef DEBUG
  MOZ_ASSERT(stack_.iteratorCount_);
  stack_.iteratorCount_--;
#endif
}

inline size_t MarkStackIter::position() const { return pos_; }

inline bool MarkStackIter::done() const { return position() == 0; }

inline MarkStack::TaggedPtr MarkStackIter::peekPtr() const {
  MOZ_ASSERT(!done());
  return stack_.stack()[pos_ - 1];
}

inline MarkStack::Tag MarkStackIter::peekTag() const { return peekPtr().tag(); }

inline MarkStack::ValueArray MarkStackIter::peekValueArray() const {
  MOZ_ASSERT(peekTag() == MarkStack::ValueArrayTag);
  MOZ_ASSERT(position() >= ValueArrayWords);

  const MarkStack::TaggedPtr* ptr = &stack_.stack()[pos_ - ValueArrayWords];
  const auto& array = *reinterpret_cast<const MarkStack::ValueArray*>(ptr);
  array.assertValid();
  return array;
}

inline void MarkStackIter::nextPtr() {
  MOZ_ASSERT(!done());
  MOZ_ASSERT(!TagIsArrayTag(peekTag()));
  pos_--;
}

inline void MarkStackIter::next() {
  if (TagIsArrayTag(peekTag())) {
    nextArray();
  } else {
    nextPtr();
  }
}

inline void MarkStackIter::nextArray() {
  MOZ_ASSERT(TagIsArrayTag(peekTag()));
  MOZ_ASSERT(position() >= ValueArrayWords);
  pos_ -= ValueArrayWords;
}

void MarkStackIter::saveValueArray(
    const MarkStack::SavedValueArray& savedArray) {
  MOZ_ASSERT(peekTag() == MarkStack::ValueArrayTag);
  MOZ_ASSERT(peekPtr().asValueArrayObject() ==
             savedArray.ptr.asSavedValueArrayObject());
  MOZ_ASSERT(position() >= ValueArrayWords);

  MarkStack::TaggedPtr* ptr = &stack_.stack()[pos_ - ValueArrayWords];
  auto dest = reinterpret_cast<MarkStack::SavedValueArray*>(ptr);
  *dest = savedArray;
  MOZ_ASSERT(peekTag() == MarkStack::SavedValueArrayTag);
}

/*** GCMarker ***************************************************************/

/*
 * ExpandWeakMaps: the GC is recomputing the liveness of WeakMap entries by
 * expanding each live WeakMap into its constituent key->value edges, a table
 * of which will be consulted in a later phase whenever marking a potential
 * key.
 */
GCMarker::GCMarker(JSRuntime* rt)
    : JSTracer(rt, JSTracer::TracerKindTag::Marking, ExpandWeakMaps),
      stack(),
      auxStack(),
      color(MarkColor::Black),
      mainStackColor(MarkColor::Black),
      delayedMarkingList(nullptr),
      delayedMarkingWorkAdded(false),
      state(MarkingState::NotActive)
#ifdef DEBUG
      ,
      markLaterArenas(0),
      strictCompartmentChecking(false),
      markQueue(rt),
      queuePos(0)
#endif
{
  setTraceWeakEdges(false);
}

bool GCMarker::init(JSGCMode gcMode) {
  return stack.init(gcMode, gc::MarkStack::MainStack) &&
         auxStack.init(gcMode, gc::MarkStack::AuxiliaryStack);
}

void GCMarker::start() {
  MOZ_ASSERT(state == MarkingState::NotActive);
  state = MarkingState::RegularMarking;
  color = MarkColor::Black;

#ifdef DEBUG
  queuePos = 0;
  queueMarkColor.reset();
#endif

  MOZ_ASSERT(!delayedMarkingList);
  MOZ_ASSERT(markLaterArenas == 0);
}

void GCMarker::stop() {
  MOZ_ASSERT(isDrained());
  MOZ_ASSERT(!delayedMarkingList);
  MOZ_ASSERT(markLaterArenas == 0);
  MOZ_ASSERT(state != MarkingState::NotActive);
  state = MarkingState::NotActive;

  stack.clear();
  auxStack.clear();
  setMainStackColor(MarkColor::Black);
  AutoEnterOOMUnsafeRegion oomUnsafe;
  for (GCZonesIter zone(runtime()); !zone.done(); zone.next()) {
    if (!zone->gcWeakKeys().clear()) {
      oomUnsafe.crash("clearing weak keys in GCMarker::stop()");
    }
    if (!zone->gcNurseryWeakKeys().clear()) {
      oomUnsafe.crash("clearing (nursery) weak keys in GCMarker::stop()");
    }
  }
}

template <typename F>
inline void GCMarker::forEachDelayedMarkingArena(F&& f) {
  Arena* arena = delayedMarkingList;
  Arena* next;
  while (arena) {
    next = arena->getNextDelayedMarking();
    f(arena);
    arena = next;
  }
}

void GCMarker::reset() {
  color = MarkColor::Black;

  stack.clear();
  auxStack.clear();
  setMainStackColor(MarkColor::Black);
  MOZ_ASSERT(isMarkStackEmpty());

  forEachDelayedMarkingArena([&](Arena* arena) {
    MOZ_ASSERT(arena->onDelayedMarkingList());
    arena->clearDelayedMarkingState();
#ifdef DEBUG
    MOZ_ASSERT(markLaterArenas);
    markLaterArenas--;
#endif
  });
  delayedMarkingList = nullptr;

  MOZ_ASSERT(isDrained());
  MOZ_ASSERT(!markLaterArenas);
}

void GCMarker::setMarkColor(gc::MarkColor newColor) {
  if (color == newColor) {
    return;
  }
  if (newColor == gc::MarkColor::Black) {
    setMarkColorBlack();
  } else {
    setMarkColorGray();
  }
}

void GCMarker::setMarkColorGray() {
  MOZ_ASSERT(color == gc::MarkColor::Black);
  MOZ_ASSERT(runtime()->gc.state() == State::Sweep);

  color = gc::MarkColor::Gray;
}

void GCMarker::setMarkColorBlack() {
  MOZ_ASSERT(color == gc::MarkColor::Gray);
  MOZ_ASSERT(runtime()->gc.state() == State::Sweep);

  color = gc::MarkColor::Black;
}

void GCMarker::setMainStackColor(gc::MarkColor newColor) {
  if (newColor != mainStackColor) {
    MOZ_ASSERT(isMarkStackEmpty());
    mainStackColor = newColor;
  }
}

template <typename T>
void GCMarker::pushTaggedPtr(T* ptr) {
  checkZone(ptr);
  if (!currentStack().push(ptr)) {
    delayMarkingChildren(ptr);
  }
}

void GCMarker::pushValueArray(JSObject* obj, HeapSlot* start, HeapSlot* end) {
  checkZone(obj);

  if (start == end) {
    return;
  }

  if (!currentStack().push(obj, start, end)) {
    delayMarkingChildren(obj);
  }
}

void GCMarker::repush(JSObject* obj) {
  MOZ_ASSERT(obj->asTenured().isMarkedAtLeast(markColor()));
  pushTaggedPtr(obj);
}

bool GCMarker::enterWeakMarkingMode() {
  MOZ_ASSERT(runtime()->gc.nursery().isEmpty());

  MOZ_ASSERT(weakMapAction() == ExpandWeakMaps);
  MOZ_ASSERT(state != MarkingState::WeakMarking);
  if (state == MarkingState::IterativeMarking) {
    return false;
  }

  // During weak marking mode, we maintain a table mapping weak keys to
  // entries in known-live weakmaps. Initialize it with the keys of marked
  // weakmaps -- or more precisely, the keys of marked weakmaps that are
  // mapped to not yet live values. (Once bug 1167452 implements incremental
  // weakmap marking, this initialization step will become unnecessary, as
  // the table will already hold all such keys.)

  // Set state before doing anything else, so any new key that is marked
  // during the following gcWeakKeys scan will itself be looked up in
  // gcWeakKeys and marked according to ephemeron rules.
  state = MarkingState::WeakMarking;

  // If there was an 'enter-weak-marking-mode' token in the queue, then it
  // and everything after it will still be in the queue so we can process
  // them now.
  while (processMarkQueue() == QueueYielded) {
  };

  return true;
}

void JS::Zone::enterWeakMarkingMode(GCMarker* marker) {
  MOZ_ASSERT(marker->isWeakMarking());
  for (WeakMapBase* m : gcWeakMapList()) {
    if (m->mapColor) {
      mozilla::Unused << m->markEntries(marker);
    }
  }
}

#ifdef DEBUG
void JS::Zone::checkWeakMarkingMode() {
  for (auto r = gcWeakKeys().all(); !r.empty(); r.popFront()) {
    for (auto markable : r.front().value) {
      MOZ_ASSERT(markable.weakmap->mapColor,
                 "unmarked weakmaps in weak keys table");
    }
  }
}
#endif

void GCMarker::leaveWeakMarkingMode() {
  MOZ_ASSERT(state == MarkingState::WeakMarking ||
             state == MarkingState::IterativeMarking);

  if (state != MarkingState::IterativeMarking) {
    state = MarkingState::RegularMarking;
  }

  // Table is expensive to maintain when not in weak marking mode, so we'll
  // rebuild it upon entry rather than allow it to contain stale data.
  AutoEnterOOMUnsafeRegion oomUnsafe;
  for (GCZonesIter zone(runtime()); !zone.done(); zone.next()) {
    if (!zone->gcWeakKeys().clear()) {
      oomUnsafe.crash("clearing weak keys in GCMarker::leaveWeakMarkingMode()");
    }
  }
}

void GCMarker::delayMarkingChildren(Cell* cell) {
  Arena* arena = cell->asTenured().arena();
  if (!arena->onDelayedMarkingList()) {
    arena->setNextDelayedMarkingArena(delayedMarkingList);
    delayedMarkingList = arena;
#ifdef DEBUG
    markLaterArenas++;
#endif
  }
  JS::TraceKind kind = MapAllocToTraceKind(arena->getAllocKind());
  MarkColor colorToMark =
      TraceKindCanBeMarkedGray(kind) ? color : MarkColor::Black;
  if (!arena->hasDelayedMarking(colorToMark)) {
    arena->setHasDelayedMarking(colorToMark, true);
    delayedMarkingWorkAdded = true;
  }
}

void GCMarker::markDelayedChildren(Arena* arena, MarkColor color) {
  JS::TraceKind kind = MapAllocToTraceKind(arena->getAllocKind());
  MOZ_ASSERT_IF(color == MarkColor::Gray, TraceKindCanBeMarkedGray(kind));

  AutoSetMarkColor setColor(*this, color);
  for (ArenaCellIterUnderGC i(arena); !i.done(); i.next()) {
    TenuredCell* t = i.getCell();
    if (t->isMarked(color)) {
      js::TraceChildren(this, t, kind);
    }
  }
}

/*
 * Process arenas from |delayedMarkingList| by marking the unmarked children of
 * marked cells of color |color|. Return early if the |budget| is exceeded.
 *
 * This is called twice, first to mark gray children and then to mark black
 * children.
 */
bool GCMarker::processDelayedMarkingList(MarkColor color, SliceBudget& budget) {
  // Marking delayed children may add more arenas to the list, including arenas
  // we are currently processing or have previously processed. Handle this by
  // clearing a flag on each arena before marking its children. This flag will
  // be set again if the arena is re-added. Iterate the list until no new arenas
  // were added.

  do {
    delayedMarkingWorkAdded = false;
    for (Arena* arena = delayedMarkingList; arena;
         arena = arena->getNextDelayedMarking()) {
      if (!arena->hasDelayedMarking(color)) {
        continue;
      }
      arena->setHasDelayedMarking(color, false);
      markDelayedChildren(arena, color);
      budget.step(150);
      if (budget.isOverBudget()) {
        return false;
      }
    }
  } while (delayedMarkingWorkAdded);

  return true;
}

bool GCMarker::markAllDelayedChildren(SliceBudget& budget) {
  MOZ_ASSERT(!hasBlackEntries());
  MOZ_ASSERT(markColor() == MarkColor::Black);

  GCRuntime& gc = runtime()->gc;
  gcstats::AutoPhase ap(gc.stats(), gc.state() == State::Mark,
                        gcstats::PhaseKind::MARK_DELAYED);

  // We have a list of arenas containing marked cells with unmarked children
  // where we ran out of stack space during marking.
  //
  // Both black and gray cells in these arenas may have unmarked children, and
  // we must mark gray children first as gray entries always sit before black
  // entries on the mark stack. Therefore the list is processed in two stages.

  MOZ_ASSERT(delayedMarkingList);

  bool finished;
  finished = processDelayedMarkingList(MarkColor::Gray, budget);
  rebuildDelayedMarkingList();
  if (!finished) {
    return false;
  }

  finished = processDelayedMarkingList(MarkColor::Black, budget);
  rebuildDelayedMarkingList();

  MOZ_ASSERT_IF(finished, !delayedMarkingList);
  MOZ_ASSERT_IF(finished, !markLaterArenas);

  return finished;
}

void GCMarker::rebuildDelayedMarkingList() {
  // Rebuild the delayed marking list, removing arenas which do not need further
  // marking.

  Arena* listTail = nullptr;
  forEachDelayedMarkingArena([&](Arena* arena) {
    if (!arena->hasAnyDelayedMarking()) {
      arena->clearDelayedMarkingState();
#ifdef DEBUG
      MOZ_ASSERT(markLaterArenas);
      markLaterArenas--;
#endif
      return;
    }

    appendToDelayedMarkingList(&listTail, arena);
  });
  appendToDelayedMarkingList(&listTail, nullptr);
}

inline void GCMarker::appendToDelayedMarkingList(Arena** listTail,
                                                 Arena* arena) {
  if (*listTail) {
    (*listTail)->updateNextDelayedMarkingArena(arena);
  } else {
    delayedMarkingList = arena;
  }
  *listTail = arena;
}

#ifdef DEBUG
void GCMarker::checkZone(void* p) {
  MOZ_ASSERT(state != MarkingState::NotActive);
  DebugOnly<Cell*> cell = static_cast<Cell*>(p);
  MOZ_ASSERT_IF(cell->isTenured(),
                cell->asTenured().zone()->isCollectingFromAnyThread());
}
#endif

size_t GCMarker::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
  size_t size = stack.sizeOfExcludingThis(mallocSizeOf);
  size += auxStack.sizeOfExcludingThis(mallocSizeOf);
  for (ZonesIter zone(runtime(), WithAtoms); !zone.done(); zone.next()) {
    size += zone->gcGrayRoots().SizeOfExcludingThis(mallocSizeOf);
  }
  return size;
}

/*** Tenuring Tracer ********************************************************/

namespace js {
template <typename T>
void TenuringTracer::traverse(T** tp) {}

template <>
void TenuringTracer::traverse(JSObject** objp) {
  // We only ever visit the internals of objects after moving them to tenured.
  MOZ_ASSERT(!nursery().isInside(objp));

  Cell** cellp = reinterpret_cast<Cell**>(objp);
  if (!IsInsideNursery(*cellp) || nursery().getForwardedPointer(cellp)) {
    return;
  }

  // Take a fast path for tenuring a plain object which is by far the most
  // common case.
  JSObject* obj = *objp;
  if (obj->is<PlainObject>()) {
    *objp = movePlainObjectToTenured(&obj->as<PlainObject>());
    return;
  }

  *objp = moveToTenuredSlow(obj);
}

template <>
void TenuringTracer::traverse(JSString** strp) {
  // We only ever visit the internals of strings after moving them to tenured.
  MOZ_ASSERT(!nursery().isInside(strp));

  Cell** cellp = reinterpret_cast<Cell**>(strp);
  if (IsInsideNursery(*cellp) && !nursery().getForwardedPointer(cellp)) {
    *strp = moveToTenured(*strp);
  }
}

template <>
void TenuringTracer::traverse(JS::BigInt** bip) {
  // We only ever visit the internals of BigInts after moving them to tenured.
  MOZ_ASSERT(!nursery().isInside(bip));

  Cell** cellp = reinterpret_cast<Cell**>(bip);
  if (IsInsideNursery(*cellp) && !nursery().getForwardedPointer(cellp)) {
    *bip = moveToTenured(*bip);
  }
}

template <typename T>
void TenuringTracer::traverse(T* thingp) {
  auto tenured = MapGCThingTyped(*thingp, [this](auto t) {
    this->traverse(&t);
    return TaggedPtr<T>::wrap(t);
  });
  if (tenured.isSome() && tenured.value() != *thingp) {
    *thingp = tenured.value();
  }
}

}  // namespace js

template <typename T>
void js::gc::StoreBuffer::MonoTypeBuffer<T>::trace(TenuringTracer& mover) {
  mozilla::ReentrancyGuard g(*owner_);
  MOZ_ASSERT(owner_->isEnabled());
  if (last_) {
    last_.trace(mover);
  }
  for (typename StoreSet::Range r = stores_.all(); !r.empty(); r.popFront()) {
    r.front().trace(mover);
  }
}

namespace js {
namespace gc {
template void StoreBuffer::MonoTypeBuffer<StoreBuffer::ValueEdge>::trace(
    TenuringTracer&);
template void StoreBuffer::MonoTypeBuffer<StoreBuffer::SlotsEdge>::trace(
    TenuringTracer&);
template struct StoreBuffer::MonoTypeBuffer<StoreBuffer::StringPtrEdge>;
template struct StoreBuffer::MonoTypeBuffer<StoreBuffer::BigIntPtrEdge>;
template struct StoreBuffer::MonoTypeBuffer<StoreBuffer::ObjectPtrEdge>;
}  // namespace gc
}  // namespace js

void js::gc::StoreBuffer::SlotsEdge::trace(TenuringTracer& mover) const {
  NativeObject* obj = object();
  MOZ_ASSERT(IsCellPointerValid(obj));

  // Beware JSObject::swap exchanging a native object for a non-native one.
  if (!obj->isNative()) {
    return;
  }

  MOZ_ASSERT(!IsInsideNursery(obj), "obj shouldn't live in nursery.");

  if (kind() == ElementKind) {
    uint32_t initLen = obj->getDenseInitializedLength();
    uint32_t numShifted = obj->getElementsHeader()->numShiftedElements();
    uint32_t clampedStart = start_;
    clampedStart = numShifted < clampedStart ? clampedStart - numShifted : 0;
    clampedStart = std::min(clampedStart, initLen);
    uint32_t clampedEnd = start_ + count_;
    clampedEnd = numShifted < clampedEnd ? clampedEnd - numShifted : 0;
    clampedEnd = std::min(clampedEnd, initLen);
    MOZ_ASSERT(clampedStart <= clampedEnd);
    mover.traceSlots(
        static_cast<HeapSlot*>(obj->getDenseElements() + clampedStart)
            ->unsafeUnbarrieredForTracing(),
        clampedEnd - clampedStart);
  } else {
    uint32_t start = std::min(start_, obj->slotSpan());
    uint32_t end = std::min(start_ + count_, obj->slotSpan());
    MOZ_ASSERT(start <= end);
    mover.traceObjectSlots(obj, start, end - start);
  }
}

static inline void TraceWholeCell(TenuringTracer& mover, JSObject* object) {
  mover.traceObject(object);
}

static inline void TraceWholeCell(TenuringTracer& mover, JSString* str) {
  str->traceChildren(&mover);
}

static inline void TraceWholeCell(TenuringTracer& mover, JS::BigInt* bi) {
  bi->traceChildren(&mover);
}

static inline void TraceWholeCell(TenuringTracer& mover, BaseScript* script) {
  script->traceChildren(&mover);
}

static inline void TraceWholeCell(TenuringTracer& mover,
                                  jit::JitCode* jitcode) {
  jitcode->traceChildren(&mover);
}

template <typename T>
static void TraceBufferedCells(TenuringTracer& mover, Arena* arena,
                               ArenaCellSet* cells) {
  for (size_t i = 0; i < MaxArenaCellIndex; i += cells->BitsPerWord) {
    ArenaCellSet::WordT bitset = cells->getWord(i / cells->BitsPerWord);
    while (bitset) {
      size_t bit = i + js::detail::CountTrailingZeroes(bitset);
      auto cell =
          reinterpret_cast<T*>(uintptr_t(arena) + ArenaCellIndexBytes * bit);
      TraceWholeCell(mover, cell);
      bitset &= bitset - 1;  // Clear the low bit.
    }
  }
}

void js::gc::StoreBuffer::WholeCellBuffer::trace(TenuringTracer& mover) {
  MOZ_ASSERT(owner_->isEnabled());

  for (ArenaCellSet* cells = head_; cells; cells = cells->next) {
    cells->check();

    Arena* arena = cells->arena;
    arena->bufferedCells() = &ArenaCellSet::Empty;

    JS::TraceKind kind = MapAllocToTraceKind(arena->getAllocKind());
    switch (kind) {
      case JS::TraceKind::Object:
        TraceBufferedCells<JSObject>(mover, arena, cells);
        break;
      case JS::TraceKind::String:
        TraceBufferedCells<JSString>(mover, arena, cells);
        break;
      case JS::TraceKind::BigInt:
        TraceBufferedCells<JS::BigInt>(mover, arena, cells);
        break;
      case JS::TraceKind::Script:
        TraceBufferedCells<BaseScript>(mover, arena, cells);
        break;
      case JS::TraceKind::JitCode:
        TraceBufferedCells<jit::JitCode>(mover, arena, cells);
        break;
      default:
        MOZ_CRASH("Unexpected trace kind");
    }
  }

  head_ = nullptr;
}

template <typename T>
void js::gc::StoreBuffer::CellPtrEdge<T>::trace(TenuringTracer& mover) const {
  static_assert(std::is_base_of<Cell, T>::value, "T must be a Cell type");
  static_assert(!std::is_base_of<TenuredCell, T>::value,
                "T must not be a tenured Cell type");

  if (!*edge) {
    return;
  }

  MOZ_ASSERT(IsCellPointerValid(*edge));
  MOZ_ASSERT((*edge)->getTraceKind() == JS::MapTypeToTraceKind<T>::kind);

  mover.traverse(edge);
}

void js::gc::StoreBuffer::ValueEdge::trace(TenuringTracer& mover) const {
  if (deref()) {
    mover.traverse(edge);
  }
}

// Visit all object children of the object and trace them.
void js::TenuringTracer::traceObject(JSObject* obj) {
  NativeObject* nobj =
      CallTraceHook([this](auto thingp) { this->traverse(thingp); }, this, obj,
                    CheckGeneration::NoChecks);
  if (!nobj) {
    return;
  }

  // Note: the contents of copy on write elements pointers are filled in
  // during parsing and cannot contain nursery pointers.
  if (!nobj->hasEmptyElements() && !nobj->denseElementsAreCopyOnWrite() &&
      ObjectDenseElementsMayBeMarkable(nobj)) {
    Value* elems = static_cast<HeapSlot*>(nobj->getDenseElements())
                       ->unsafeUnbarrieredForTracing();
    traceSlots(elems, elems + nobj->getDenseInitializedLength());
  }

  traceObjectSlots(nobj, 0, nobj->slotSpan());
}

void js::TenuringTracer::traceObjectSlots(NativeObject* nobj, uint32_t start,
                                          uint32_t length) {
  HeapSlot* fixedStart;
  HeapSlot* fixedEnd;
  HeapSlot* dynStart;
  HeapSlot* dynEnd;
  nobj->getSlotRange(start, length, &fixedStart, &fixedEnd, &dynStart, &dynEnd);
  if (fixedStart) {
    traceSlots(fixedStart->unsafeUnbarrieredForTracing(),
               fixedEnd->unsafeUnbarrieredForTracing());
  }
  if (dynStart) {
    traceSlots(dynStart->unsafeUnbarrieredForTracing(),
               dynEnd->unsafeUnbarrieredForTracing());
  }
}

void js::TenuringTracer::traceSlots(Value* vp, Value* end) {
  for (; vp != end; ++vp) {
    traverse(vp);
  }
}

inline void js::TenuringTracer::traceSlots(JS::Value* vp, uint32_t nslots) {
  traceSlots(vp, vp + nslots);
}

void js::TenuringTracer::traceString(JSString* str) {
  str->traceChildren(this);
}

void js::TenuringTracer::traceBigInt(JS::BigInt* bi) {
  bi->traceChildren(this);
}

#ifdef DEBUG
static inline ptrdiff_t OffsetToChunkEnd(void* p) {
  return ChunkLocationOffset - (uintptr_t(p) & gc::ChunkMask);
}
#endif

/* Insert the given relocation entry into the list of things to visit. */
inline void js::TenuringTracer::insertIntoObjectFixupList(
    RelocationOverlay* entry) {
  *objTail = entry;
  objTail = &entry->nextRef();
  *objTail = nullptr;
}

template <typename T>
inline T* js::TenuringTracer::allocTenured(Zone* zone, AllocKind kind) {
  return static_cast<T*>(static_cast<Cell*>(AllocateCellInGC(zone, kind)));
}

JSObject* js::TenuringTracer::moveToTenuredSlow(JSObject* src) {
  MOZ_ASSERT(IsInsideNursery(src));
  MOZ_ASSERT(!src->zone()->usedByHelperThread());
  MOZ_ASSERT(!src->is<PlainObject>());

  AllocKind dstKind = src->allocKindForTenure(nursery());
  auto dst = allocTenured<JSObject>(src->zone(), dstKind);

  size_t srcSize = Arena::thingSize(dstKind);
  size_t dstSize = srcSize;

  /*
   * Arrays do not necessarily have the same AllocKind between src and dst.
   * We deal with this by copying elements manually, possibly re-inlining
   * them if there is adequate room inline in dst.
   *
   * For Arrays we're reducing tenuredSize to the smaller srcSize
   * because moveElementsToTenured() accounts for all Array elements,
   * even if they are inlined.
   */
  if (src->is<ArrayObject>()) {
    dstSize = srcSize = sizeof(NativeObject);
  } else if (src->is<TypedArrayObject>()) {
    TypedArrayObject* tarray = &src->as<TypedArrayObject>();
    // Typed arrays with inline data do not necessarily have the same
    // AllocKind between src and dst. The nursery does not allocate an
    // inline data buffer that has the same size as the slow path will do.
    // In the slow path, the Typed Array Object stores the inline data
    // in the allocated space that fits the AllocKind. In the fast path,
    // the nursery will allocate another buffer that is directly behind the
    // minimal JSObject. That buffer size plus the JSObject size is not
    // necessarily as large as the slow path's AllocKind size.
    if (tarray->hasInlineElements()) {
      AllocKind srcKind = GetGCObjectKind(TypedArrayObject::FIXED_DATA_START);
      size_t headerSize = Arena::thingSize(srcKind);
      srcSize = headerSize + tarray->byteLength();
    }
  }

  tenuredSize += dstSize;
  tenuredCells++;

  // Copy the Cell contents.
  MOZ_ASSERT(OffsetToChunkEnd(src) >= ptrdiff_t(srcSize));
  js_memcpy(dst, src, srcSize);

  // Move the slots and elements, if we need to.
  if (src->isNative()) {
    NativeObject* ndst = &dst->as<NativeObject>();
    NativeObject* nsrc = &src->as<NativeObject>();
    tenuredSize += moveSlotsToTenured(ndst, nsrc);
    tenuredSize += moveElementsToTenured(ndst, nsrc, dstKind);

    // There is a pointer into a dictionary mode object from the head of its
    // shape list. This is updated in Nursery::sweepDictionaryModeObjects().
  }

  JSObjectMovedOp op = dst->getClass()->extObjectMovedOp();
  MOZ_ASSERT_IF(src->is<ProxyObject>(), op == proxy_ObjectMoved);
  if (op) {
    // Tell the hazard analysis that the object moved hook can't GC.
    JS::AutoSuppressGCAnalysis nogc;
    tenuredSize += op(dst, src);
  } else {
    MOZ_ASSERT_IF(src->getClass()->hasFinalize(),
                  CanNurseryAllocateFinalizedClass(src->getClass()));
  }

  RelocationOverlay* overlay = RelocationOverlay::fromCell(src);
  overlay->forwardTo(dst);
  insertIntoObjectFixupList(overlay);

  gcTracer.tracePromoteToTenured(src, dst);
  return dst;
}

inline JSObject* js::TenuringTracer::movePlainObjectToTenured(
    PlainObject* src) {
  // Fast path version of moveToTenuredSlow() for specialized for PlainObject.

  MOZ_ASSERT(IsInsideNursery(src));
  MOZ_ASSERT(!src->zone()->usedByHelperThread());

  AllocKind dstKind = src->allocKindForTenure();
  auto dst = allocTenured<PlainObject>(src->zone(), dstKind);

  size_t srcSize = Arena::thingSize(dstKind);
  tenuredSize += srcSize;
  tenuredCells++;

  // Copy the Cell contents.
  MOZ_ASSERT(OffsetToChunkEnd(src) >= ptrdiff_t(srcSize));
  js_memcpy(dst, src, srcSize);

  // Move the slots and elements.
  tenuredSize += moveSlotsToTenured(dst, src);
  tenuredSize += moveElementsToTenured(dst, src, dstKind);

  MOZ_ASSERT(!dst->getClass()->extObjectMovedOp());

  RelocationOverlay* overlay = RelocationOverlay::fromCell(src);
  overlay->forwardTo(dst);
  insertIntoObjectFixupList(overlay);

  gcTracer.tracePromoteToTenured(src, dst);
  return dst;
}

size_t js::TenuringTracer::moveSlotsToTenured(NativeObject* dst,
                                              NativeObject* src) {
  /* Fixed slots have already been copied over. */
  if (!src->hasDynamicSlots()) {
    return 0;
  }

  Zone* zone = src->zone();
  size_t count = src->numDynamicSlots();

  if (!nursery().isInside(src->slots_)) {
    AddCellMemory(dst, count * sizeof(HeapSlot), MemoryUse::ObjectSlots);
    nursery().removeMallocedBuffer(src->slots_);
    return 0;
  }

  {
    AutoEnterOOMUnsafeRegion oomUnsafe;
    dst->slots_ = zone->pod_malloc<HeapSlot>(count);
    if (!dst->slots_) {
      oomUnsafe.crash(sizeof(HeapSlot) * count,
                      "Failed to allocate slots while tenuring.");
    }
  }

  AddCellMemory(dst, count * sizeof(HeapSlot), MemoryUse::ObjectSlots);

  PodCopy(dst->slots_, src->slots_, count);
  nursery().setSlotsForwardingPointer(src->slots_, dst->slots_, count);
  return count * sizeof(HeapSlot);
}

size_t js::TenuringTracer::moveElementsToTenured(NativeObject* dst,
                                                 NativeObject* src,
                                                 AllocKind dstKind) {
  if (src->hasEmptyElements() || src->denseElementsAreCopyOnWrite()) {
    return 0;
  }

  Zone* zone = src->zone();

  ObjectElements* srcHeader = src->getElementsHeader();
  size_t nslots = srcHeader->numAllocatedElements();

  void* srcAllocatedHeader = src->getUnshiftedElementsHeader();

  /* TODO Bug 874151: Prefer to put element data inline if we have space. */
  if (!nursery().isInside(srcAllocatedHeader)) {
    MOZ_ASSERT(src->elements_ == dst->elements_);
    nursery().removeMallocedBuffer(srcAllocatedHeader);

    AddCellMemory(dst, nslots * sizeof(HeapSlot), MemoryUse::ObjectElements);

    return 0;
  }

  // Shifted elements are copied too.
  uint32_t numShifted = srcHeader->numShiftedElements();

  /* Unlike other objects, Arrays can have fixed elements. */
  if (src->is<ArrayObject>() && nslots <= GetGCKindSlots(dstKind)) {
    dst->as<ArrayObject>().setFixedElements();
    js_memcpy(dst->getElementsHeader(), srcAllocatedHeader,
              nslots * sizeof(HeapSlot));
    dst->elements_ += numShifted;
    nursery().setElementsForwardingPointer(srcHeader, dst->getElementsHeader(),
                                           srcHeader->capacity);
    return nslots * sizeof(HeapSlot);
  }

  MOZ_ASSERT(nslots >= 2);

  ObjectElements* dstHeader;
  {
    AutoEnterOOMUnsafeRegion oomUnsafe;
    dstHeader =
        reinterpret_cast<ObjectElements*>(zone->pod_malloc<HeapSlot>(nslots));
    if (!dstHeader) {
      oomUnsafe.crash(sizeof(HeapSlot) * nslots,
                      "Failed to allocate elements while tenuring.");
    }
  }

  AddCellMemory(dst, nslots * sizeof(HeapSlot), MemoryUse::ObjectElements);

  js_memcpy(dstHeader, srcAllocatedHeader, nslots * sizeof(HeapSlot));
  dst->elements_ = dstHeader->elements() + numShifted;
  nursery().setElementsForwardingPointer(srcHeader, dst->getElementsHeader(),
                                         srcHeader->capacity);
  return nslots * sizeof(HeapSlot);
}

inline void js::TenuringTracer::insertIntoStringFixupList(
    RelocationOverlay* entry) {
  *stringTail = entry;
  stringTail = &entry->nextRef();
  *stringTail = nullptr;
}

JSString* js::TenuringTracer::moveToTenured(JSString* src) {
  MOZ_ASSERT(IsInsideNursery(src));
  MOZ_ASSERT(!src->zone()->usedByHelperThread());

  AllocKind dstKind = src->getAllocKind();
  Zone* zone = src->zone();
  zone->tenuredStrings++;

  JSString* dst = allocTenured<JSString>(zone, dstKind);
  tenuredSize += moveStringToTenured(dst, src, dstKind);
  tenuredCells++;

  RelocationOverlay* overlay = RelocationOverlay::fromCell(src);
  overlay->forwardTo(dst);
  insertIntoStringFixupList(overlay);

  gcTracer.tracePromoteToTenured(src, dst);
  return dst;
}

inline void js::TenuringTracer::insertIntoBigIntFixupList(
    RelocationOverlay* entry) {
  *bigIntTail = entry;
  bigIntTail = &entry->nextRef();
  *bigIntTail = nullptr;
}

JS::BigInt* js::TenuringTracer::moveToTenured(JS::BigInt* src) {
  MOZ_ASSERT(IsInsideNursery(src));
  MOZ_ASSERT(!src->zone()->usedByHelperThread());

  AllocKind dstKind = src->getAllocKind();
  Zone* zone = src->zone();
  zone->tenuredBigInts++;

  JS::BigInt* dst = allocTenured<JS::BigInt>(zone, dstKind);
  tenuredSize += moveBigIntToTenured(dst, src, dstKind);
  tenuredCells++;

  RelocationOverlay* overlay = RelocationOverlay::fromCell(src);
  overlay->forwardTo(dst);
  insertIntoBigIntFixupList(overlay);

  gcTracer.tracePromoteToTenured(src, dst);
  return dst;
}

void js::Nursery::collectToFixedPoint(TenuringTracer& mover,
                                      TenureCountCache& tenureCounts) {
  for (RelocationOverlay* p = mover.objHead; p; p = p->next()) {
    JSObject* obj = static_cast<JSObject*>(p->forwardingAddress());
    mover.traceObject(obj);

    TenureCount& entry = tenureCounts.findEntry(obj->groupRaw());
    if (entry.group == obj->groupRaw()) {
      entry.count++;
    } else if (!entry.group) {
      entry.group = obj->groupRaw();
      entry.count = 1;
    }
  }

  for (RelocationOverlay* p = mover.stringHead; p; p = p->next()) {
    mover.traceString(static_cast<JSString*>(p->forwardingAddress()));
  }

  for (RelocationOverlay* p = mover.bigIntHead; p; p = p->next()) {
    mover.traceBigInt(static_cast<JS::BigInt*>(p->forwardingAddress()));
  }
}

size_t js::TenuringTracer::moveStringToTenured(JSString* dst, JSString* src,
                                               AllocKind dstKind) {
  size_t size = Arena::thingSize(dstKind);

  // At the moment, strings always have the same AllocKind between src and
  // dst. This may change in the future.
  MOZ_ASSERT(dst->asTenured().getAllocKind() == src->getAllocKind());

  // Copy the Cell contents.
  MOZ_ASSERT(OffsetToChunkEnd(src) >= ptrdiff_t(size));
  js_memcpy(dst, src, size);

  if (src->ownsMallocedChars()) {
    void* chars = src->asLinear().nonInlineCharsRaw();
    nursery().removeMallocedBuffer(chars);
    AddCellMemory(dst, dst->asLinear().allocSize(), MemoryUse::StringContents);
  }

  return size;
}

size_t js::TenuringTracer::moveBigIntToTenured(JS::BigInt* dst, JS::BigInt* src,
                                               AllocKind dstKind) {
  size_t size = Arena::thingSize(dstKind);

  // At the moment, BigInts always have the same AllocKind between src and
  // dst. This may change in the future.
  MOZ_ASSERT(dst->asTenured().getAllocKind() == src->getAllocKind());

  // Copy the Cell contents.
  MOZ_ASSERT(OffsetToChunkEnd(src) >= ptrdiff_t(size));
  js_memcpy(dst, src, size);

  MOZ_ASSERT(dst->zone() == src->zone());

  if (src->hasHeapDigits()) {
    size_t length = dst->digitLength();
    if (!nursery().isInside(src->heapDigits_)) {
      nursery().removeMallocedBuffer(src->heapDigits_);
    } else {
      Zone* zone = src->zone();
      {
        AutoEnterOOMUnsafeRegion oomUnsafe;
        dst->heapDigits_ = zone->pod_malloc<JS::BigInt::Digit>(length);
        if (!dst->heapDigits_) {
          oomUnsafe.crash(sizeof(JS::BigInt::Digit) * length,
                          "Failed to allocate digits while tenuring.");
        }
      }

      PodCopy(dst->heapDigits_, src->heapDigits_, length);
      nursery().setDirectForwardingPointer(src->heapDigits_, dst->heapDigits_);

      size += length * sizeof(JS::BigInt::Digit);
    }

    AddCellMemory(dst, length * sizeof(JS::BigInt::Digit),
                  MemoryUse::BigIntDigits);
  }

  return size;
}

/*** IsMarked / IsAboutToBeFinalized ****************************************/

template <typename T>
static inline void CheckIsMarkedThing(T* thingp) {
#define IS_SAME_TYPE_OR(name, type, _, _1) std::is_same_v<type*, T> ||
  static_assert(JS_FOR_EACH_TRACEKIND(IS_SAME_TYPE_OR) false,
                "Only the base cell layout types are allowed into "
                "marking/tracing internals");
#undef IS_SAME_TYPE_OR

#ifdef DEBUG
  MOZ_ASSERT(thingp);
  MOZ_ASSERT(*thingp);

  // Allow any thread access to uncollected things.
  T thing = *thingp;
  if (ThingIsPermanentAtomOrWellKnownSymbol(thing)) {
    return;
  }

  // Allow the current thread access if it is sweeping or in sweep-marking, but
  // try to check the zone. Some threads have access to all zones when sweeping.
  JSContext* cx = TlsContext.get();
  MOZ_ASSERT(cx->gcUse != JSContext::GCUse::Finalizing);
  if (cx->gcUse == JSContext::GCUse::Sweeping ||
      cx->gcUse == JSContext::GCUse::Marking) {
    Zone* zone = thing->zoneFromAnyThread();
    MOZ_ASSERT_IF(cx->gcSweepZone,
                  cx->gcSweepZone == zone || zone->isAtomsZone());
    return;
  }

  // Otherwise only allow access from the main thread or this zone's associated
  // thread.
  MOZ_ASSERT(CurrentThreadCanAccessRuntime(thing->runtimeFromAnyThread()) ||
             CurrentThreadCanAccessZone(thing->zoneFromAnyThread()));
#endif
}

template <typename T>
static inline bool ShouldCheckMarkState(JSRuntime* rt, T** thingp) {
  CheckIsMarkedThing(thingp);
  MOZ_ASSERT(!IsInsideNursery(*thingp));

  TenuredCell& thing = (*thingp)->asTenured();
  Zone* zone = thing.zoneFromAnyThread();
  if (!zone->isCollectingFromAnyThread() || zone->isGCFinished()) {
    return false;
  }

  if (zone->isGCCompacting() && IsForwarded(*thingp)) {
    *thingp = Forwarded(*thingp);
    return false;
  }

  return true;
}

template <typename T>
struct MightBeNurseryAllocated {
  static const bool value = std::is_base_of<JSObject, T>::value ||
                            std::is_base_of<JSString, T>::value ||
                            std::is_base_of<JS::BigInt, T>::value;
};

template <typename T>
bool js::gc::IsMarkedInternal(JSRuntime* rt, T** thingp) {
  // Don't depend on the mark state of other cells during finalization.
  MOZ_ASSERT(!CurrentThreadIsGCFinalizing());

  if (IsOwnedByOtherRuntime(rt, *thingp)) {
    return true;
  }

  if (MightBeNurseryAllocated<T>::value && IsInsideNursery(*thingp)) {
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
    Cell** cellp = reinterpret_cast<Cell**>(thingp);
    return Nursery::getForwardedPointer(cellp);
  }

  if (!ShouldCheckMarkState(rt, thingp)) {
    return true;
  }

  return (*thingp)->asTenured().isMarkedAny();
}

bool js::gc::IsAboutToBeFinalizedDuringSweep(TenuredCell& tenured) {
  MOZ_ASSERT(!IsInsideNursery(&tenured));
  MOZ_ASSERT(tenured.zoneFromAnyThread()->isGCSweeping());

  // Don't depend on the mark state of other cells during finalization.
  MOZ_ASSERT(!CurrentThreadIsGCFinalizing());

  return !tenured.isMarkedAny();
}

template <typename T>
bool js::gc::IsAboutToBeFinalizedInternal(T** thingp) {
  // Don't depend on the mark state of other cells during finalization.
  MOZ_ASSERT(!CurrentThreadIsGCFinalizing());

  CheckIsMarkedThing(thingp);
  T* thing = *thingp;
  JSRuntime* rt = thing->runtimeFromAnyThread();

  /* Permanent atoms are never finalized by non-owning runtimes. */
  if (ThingIsPermanentAtomOrWellKnownSymbol(thing) &&
      TlsContext.get()->runtime() != rt) {
    return false;
  }

  if (IsInsideNursery(thing)) {
    return JS::RuntimeHeapIsMinorCollecting() &&
           !Nursery::getForwardedPointer(reinterpret_cast<Cell**>(thingp));
  }

  Zone* zone = thing->asTenured().zoneFromAnyThread();
  if (zone->isGCSweeping()) {
    return IsAboutToBeFinalizedDuringSweep(thing->asTenured());
  } else if (zone->isGCCompacting() && IsForwarded(thing)) {
    *thingp = Forwarded(thing);
    return false;
  }

  return false;
}

template <typename T>
bool js::gc::IsAboutToBeFinalizedInternal(T* thingp) {
  bool dying = false;
  auto thing = MapGCThingTyped(*thingp, [&dying](auto t) {
    dying = IsAboutToBeFinalizedInternal(&t);
    return TaggedPtr<T>::wrap(t);
  });
  if (thing.isSome() && thing.value() != *thingp) {
    *thingp = thing.value();
  }
  return dying;
}

template <typename T>
inline bool SweepingTracer::sweepEdge(T** thingp) {
  CheckIsMarkedThing(thingp);
  T* thing = *thingp;
  JSRuntime* rt = thing->runtimeFromAnyThread();

  if (ThingIsPermanentAtomOrWellKnownSymbol(thing) && runtime() != rt) {
    return true;
  }

  // TODO: We should assert the zone of the tenured cell is in Sweeping state,
  // however we need to fix atoms and JitcodeGlobalTable first.
  // Bug 1501334 : IsAboutToBeFinalized doesn't work for atoms
  // Bug 1071218 : Refactor Debugger::sweepAll and
  //               JitRuntime::SweepJitcodeGlobalTable to work per sweep group
  TenuredCell& tenured = thing->asTenured();
  if (!tenured.isMarkedAny()) {
    *thingp = nullptr;
    return false;
  }

  return true;
}

bool SweepingTracer::onObjectEdge(JSObject** objp) { return sweepEdge(objp); }
bool SweepingTracer::onShapeEdge(Shape** shapep) { return sweepEdge(shapep); }
bool SweepingTracer::onStringEdge(JSString** stringp) {
  return sweepEdge(stringp);
}
bool SweepingTracer::onScriptEdge(js::BaseScript** scriptp) {
  return sweepEdge(scriptp);
}
bool SweepingTracer::onBaseShapeEdge(BaseShape** basep) {
  return sweepEdge(basep);
}
bool SweepingTracer::onJitCodeEdge(jit::JitCode** jitp) {
  return sweepEdge(jitp);
}
bool SweepingTracer::onScopeEdge(Scope** scopep) { return sweepEdge(scopep); }
bool SweepingTracer::onRegExpSharedEdge(RegExpShared** sharedp) {
  return sweepEdge(sharedp);
}
bool SweepingTracer::onObjectGroupEdge(ObjectGroup** groupp) {
  return sweepEdge(groupp);
}
bool SweepingTracer::onBigIntEdge(BigInt** bip) { return sweepEdge(bip); }

namespace js {
namespace gc {

template <typename T>
JS_PUBLIC_API bool EdgeNeedsSweep(JS::Heap<T>* thingp) {
  return IsAboutToBeFinalizedInternal(ConvertToBase(thingp->unsafeGet()));
}

template <typename T>
JS_PUBLIC_API bool EdgeNeedsSweepUnbarrieredSlow(T* thingp) {
  return IsAboutToBeFinalizedInternal(ConvertToBase(thingp));
}

// Instantiate a copy of the Tracing templates for each public GC type.
#define INSTANTIATE_ALL_VALID_HEAP_TRACE_FUNCTIONS(type)             \
  template JS_PUBLIC_API bool EdgeNeedsSweep<type>(JS::Heap<type>*); \
  template JS_PUBLIC_API bool EdgeNeedsSweepUnbarrieredSlow<type>(type*);
JS_FOR_EACH_PUBLIC_GC_POINTER_TYPE(INSTANTIATE_ALL_VALID_HEAP_TRACE_FUNCTIONS)
JS_FOR_EACH_PUBLIC_TAGGED_GC_POINTER_TYPE(
    INSTANTIATE_ALL_VALID_HEAP_TRACE_FUNCTIONS)

#define INSTANTIATE_INTERNAL_IS_MARKED_FUNCTION(type) \
  template bool IsMarkedInternal(JSRuntime* rt, type* thing);

#define INSTANTIATE_INTERNAL_IATBF_FUNCTION(type) \
  template bool IsAboutToBeFinalizedInternal(type* thingp);

#define INSTANTIATE_INTERNAL_MARKING_FUNCTIONS_FROM_TRACEKIND(_1, type, _2, \
                                                              _3)           \
  INSTANTIATE_INTERNAL_IS_MARKED_FUNCTION(type*)                            \
  INSTANTIATE_INTERNAL_IATBF_FUNCTION(type*)

JS_FOR_EACH_TRACEKIND(INSTANTIATE_INTERNAL_MARKING_FUNCTIONS_FROM_TRACEKIND)

JS_FOR_EACH_PUBLIC_TAGGED_GC_POINTER_TYPE(INSTANTIATE_INTERNAL_IATBF_FUNCTION)

#undef INSTANTIATE_INTERNAL_IS_MARKED_FUNCTION
#undef INSTANTIATE_INTERNAL_IATBF_FUNCTION
#undef INSTANTIATE_INTERNAL_MARKING_FUNCTIONS_FROM_TRACEKIND

} /* namespace gc */
} /* namespace js */

/*** Cycle Collector Barrier Implementation *********************************/

/*
 * The GC and CC are run independently. Consequently, the following sequence of
 * events can occur:
 * 1. GC runs and marks an object gray.
 * 2. The mutator runs (specifically, some C++ code with access to gray
 *    objects) and creates a pointer from a JS root or other black object to
 *    the gray object. If we re-ran a GC at this point, the object would now be
 *    black.
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
 * to its value. These "implicit edges" are represented in some other
 * container object, such as the weakmap itself. In these
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

#ifdef DEBUG
struct AssertNonGrayTracer final : public JS::CallbackTracer {
  explicit AssertNonGrayTracer(JSRuntime* rt) : JS::CallbackTracer(rt) {}
  bool onChild(const JS::GCCellPtr& thing) override {
    MOZ_ASSERT(!thing.asCell()->isMarkedGray());
    return true;
  }
  // This is used by the UnmarkGray tracer only, and needs to report itself
  // as the non-gray tracer to not trigger assertions.  Do not use it in another
  // context without making this more generic.
  TracerKind getTracerKind() const override { return TracerKind::UnmarkGray; }
};
#endif

class UnmarkGrayTracer final : public JS::CallbackTracer {
 public:
  // We set weakMapAction to DoNotTraceWeakMaps because the cycle collector
  // will fix up any color mismatches involving weakmaps when it runs.
  explicit UnmarkGrayTracer(JSRuntime* rt)
      : JS::CallbackTracer(rt, DoNotTraceWeakMaps),
        unmarkedAny(false),
        oom(false),
        stack(rt->gc.unmarkGrayStack) {}

  void unmark(JS::GCCellPtr cell);

  // Whether we unmarked anything.
  bool unmarkedAny;

  // Whether we ran out of memory.
  bool oom;

 private:
  // Stack of cells to traverse.
  Vector<JS::GCCellPtr, 0, SystemAllocPolicy>& stack;

  bool onChild(const JS::GCCellPtr& thing) override;

#ifdef DEBUG
  TracerKind getTracerKind() const override { return TracerKind::UnmarkGray; }
#endif
};

bool UnmarkGrayTracer::onChild(const JS::GCCellPtr& thing) {
  Cell* cell = thing.asCell();

  // Cells in the nursery cannot be gray, and nor can certain kinds of tenured
  // cells. These must necessarily point only to black edges.
  if (!cell->isTenured() ||
      !TraceKindCanBeMarkedGray(cell->asTenured().getTraceKind())) {
#ifdef DEBUG
    MOZ_ASSERT(!cell->isMarkedGray());
    AssertNonGrayTracer nongray(runtime());
    TraceChildren(&nongray, cell, thing.kind());
#endif
    return true;
  }

  TenuredCell& tenured = cell->asTenured();

  // If the cell is in a zone that we're currently marking, then it's possible
  // that it is currently white but will end up gray. To handle this case, push
  // any cells in zones that are currently being marked onto the mark stack and
  // they will eventually get marked black.
  Zone* zone = tenured.zone();
  if (zone->isGCMarking()) {
    if (!cell->isMarkedBlack()) {
      Cell* tmp = cell;
      JSTracer* trc = &runtime()->gc.marker;
      TraceManuallyBarrieredGenericPointerEdge(trc, &tmp, "read barrier");
      MOZ_ASSERT(tmp == cell);
      unmarkedAny = true;
    }
    return true;
  }

  if (!tenured.isMarkedGray()) {
    return true;
  }

  tenured.markBlack();
  unmarkedAny = true;

  if (!stack.append(thing)) {
    oom = true;
  }
  return true;
}

void UnmarkGrayTracer::unmark(JS::GCCellPtr cell) {
  MOZ_ASSERT(stack.empty());

  onChild(cell);

  while (!stack.empty() && !oom) {
    TraceChildren(this, stack.popCopy());
  }

  if (oom) {
    // If we run out of memory, we take a drastic measure: require that we
    // GC again before the next CC.
    stack.clear();
    runtime()->gc.setGrayBitsInvalid();
    return;
  }
}

bool js::gc::UnmarkGrayGCThingUnchecked(JSRuntime* rt, JS::GCCellPtr thing) {
  MOZ_ASSERT(thing);
  MOZ_ASSERT(thing.asCell()->isMarkedGray());

  AutoGeckoProfilerEntry profilingStackFrame(
      TlsContext.get(), "UnmarkGrayGCThing", JS::ProfilingCategoryPair::GCCC);

  UnmarkGrayTracer unmarker(rt);
  // We don't record phaseTimes when we're running on a helper thread.
  bool enable = TlsContext.get()->isMainThreadContext();
  gcstats::AutoPhase innerPhase(rt->gc.stats(), enable,
                                gcstats::PhaseKind::UNMARK_GRAY);
  unmarker.unmark(thing);
  return unmarker.unmarkedAny;
}

JS_FRIEND_API bool JS::UnmarkGrayGCThingRecursively(JS::GCCellPtr thing) {
  MOZ_ASSERT(!JS::RuntimeHeapIsCollecting());
  MOZ_ASSERT(!JS::RuntimeHeapIsCycleCollecting());

  JSRuntime* rt = thing.asCell()->runtimeFromMainThread();
  gcstats::AutoPhase outerPhase(rt->gc.stats(), gcstats::PhaseKind::BARRIER);
  return UnmarkGrayGCThingUnchecked(rt, thing);
}

bool js::UnmarkGrayShapeRecursively(Shape* shape) {
  return JS::UnmarkGrayGCThingRecursively(JS::GCCellPtr(shape));
}

#ifdef DEBUG
Cell* js::gc::UninlinedForwarded(const Cell* cell) { return Forwarded(cell); }
#endif

namespace js {
namespace debug {

MarkInfo GetMarkInfo(Cell* rawCell) {
  if (!rawCell->isTenured()) {
    return MarkInfo::NURSERY;
  }

  TenuredCell* cell = &rawCell->asTenured();
  if (cell->isMarkedGray()) {
    return MarkInfo::GRAY;
  }
  if (cell->isMarkedBlack()) {
    return MarkInfo::BLACK;
  }
  return MarkInfo::UNMARKED;
}

uintptr_t* GetMarkWordAddress(Cell* cell) {
  if (!cell->isTenured()) {
    return nullptr;
  }

  uintptr_t* wordp;
  uintptr_t mask;
  js::gc::detail::GetGCThingMarkWordAndMask(uintptr_t(cell), ColorBit::BlackBit,
                                            &wordp, &mask);
  return wordp;
}

uintptr_t GetMarkMask(Cell* cell, uint32_t colorBit) {
  MOZ_ASSERT(colorBit == 0 || colorBit == 1);

  if (!cell->isTenured()) {
    return 0;
  }

  ColorBit bit = colorBit == 0 ? ColorBit::BlackBit : ColorBit::GrayOrBlackBit;
  uintptr_t* wordp;
  uintptr_t mask;
  js::gc::detail::GetGCThingMarkWordAndMask(uintptr_t(cell), bit, &wordp,
                                            &mask);
  return mask;
}

}  // namespace debug
}  // namespace js
