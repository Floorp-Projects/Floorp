/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#ifdef MOZ_VALGRIND
# include <valgrind/memcheck.h>
#endif

#include "jscntxt.h"
#include "jsgc.h"
#include "jsprf.h"
#include "jstypes.h"
#include "jswatchpoint.h"

#include "builtin/MapObject.h"
#include "frontend/BytecodeCompiler.h"
#include "gc/GCInternals.h"
#include "gc/Marking.h"
#include "jit/MacroAssembler.h"
#include "js/HashTable.h"
#include "vm/Debugger.h"
#include "vm/JSONParser.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace js::gc;

using mozilla::ArrayEnd;

using JS::AutoGCRooter;

typedef RootedValueMap::Range RootRange;
typedef RootedValueMap::Entry RootEntry;
typedef RootedValueMap::Enum RootEnum;

template <typename T>
using TraceFunction = void (*)(JSTracer* trc, T* ref, const char* name);

template <typename T, TraceFunction<T> TraceFn = TraceNullableRoot>
static inline void
MarkExactStackRootList(JSTracer* trc, JS::Rooted<void*>* rooter, const char* name)
{
    while (rooter) {
        T* addr = reinterpret_cast<JS::Rooted<T>*>(rooter)->address();
        TraceFn(trc, addr, name);
        rooter = rooter->previous();
    }
}

void
js::RootLists::traceStackRoots(JSTracer* trc)
{
#define MARK_ROOTS(name, type, _) \
    MarkExactStackRootList<type*>(trc, stackRoots_[JS::RootKind::name], "exact-" #name);
JS_FOR_EACH_TRACEKIND(MARK_ROOTS)
#undef MARK_ROOTS
    MarkExactStackRootList<jsid>(trc, stackRoots_[JS::RootKind::Id], "exact-id");
    MarkExactStackRootList<Value>(trc, stackRoots_[JS::RootKind::Value], "exact-value");
    MarkExactStackRootList<JS::Traceable, js::DispatchWrapper<JS::Traceable>::TraceWrapped>(
        trc, stackRoots_[JS::RootKind::Traceable], "Traceable");
}

static void
MarkExactStackRoots(JSRuntime* rt, JSTracer* trc)
{
    for (ContextIter cx(rt); !cx.done(); cx.next())
        cx->roots.traceStackRoots(trc);
    rt->mainThread.roots.traceStackRoots(trc);
}

template <typename T, TraceFunction<T> TraceFn = TraceNullableRoot>
static inline void
MarkPersistentRootedList(JSTracer* trc, mozilla::LinkedList<PersistentRooted<void*>>& list,
                         const char* name)
{
    for (PersistentRooted<void*>* r : list)
        TraceFn(trc, reinterpret_cast<PersistentRooted<T>*>(r)->address(), name);
}

void
js::RootLists::tracePersistentRoots(JSTracer* trc)
{
#define MARK_ROOTS(name, type, _) \
    MarkPersistentRootedList<type*>(trc, heapRoots_[JS::RootKind::name], "persistent-" #name);
JS_FOR_EACH_TRACEKIND(MARK_ROOTS)
#undef MARK_ROOTS
    MarkPersistentRootedList<jsid>(trc, heapRoots_[JS::RootKind::Id], "persistent-id");
    MarkPersistentRootedList<Value>(trc, heapRoots_[JS::RootKind::Value], "persistent-value");
    MarkPersistentRootedList<JS::Traceable, js::DispatchWrapper<JS::Traceable>::TraceWrapped>(trc,
            heapRoots_[JS::RootKind::Traceable], "persistent-traceable");
}

static void
MarkPersistentRooted(JSRuntime* rt, JSTracer* trc)
{
    for (ContextIter cx(rt); !cx.done(); cx.next())
        cx->roots.tracePersistentRoots(trc);
    rt->mainThread.roots.tracePersistentRoots(trc);
}

template <typename T>
static void
FinishPersistentRootedChain(mozilla::LinkedList<PersistentRooted<void*>>& listArg)
{
    while (!listArg.isEmpty())
        listArg.getFirst()->reset();
}

void
js::RootLists::finishPersistentRoots()
{
#define FINISH_ROOT_LIST(name, type, _) \
    FinishPersistentRootedChain<type*>(heapRoots_[JS::RootKind::name]);
JS_FOR_EACH_TRACEKIND(FINISH_ROOT_LIST)
#undef FINISH_ROOT_LIST
    FinishPersistentRootedChain<jsid>(heapRoots_[JS::RootKind::Id]);
    FinishPersistentRootedChain<Value>(heapRoots_[JS::RootKind::Value]);
    FinishPersistentRootedChain<JS::Traceable>(heapRoots_[JS::RootKind::Traceable]);
}

inline void
AutoGCRooter::trace(JSTracer* trc)
{
    switch (tag_) {
      case PARSER:
        frontend::MarkParser(trc, this);
        return;

      case VALVECTOR: {
        AutoValueVector::VectorImpl& vector = static_cast<AutoValueVector*>(this)->vector;
        TraceRootRange(trc, vector.length(), vector.begin(), "JS::AutoValueVector.vector");
        return;
      }

      case IDVECTOR: {
        AutoIdVector::VectorImpl& vector = static_cast<AutoIdVector*>(this)->vector;
        TraceRootRange(trc, vector.length(), vector.begin(), "JS::AutoIdVector.vector");
        return;
      }

      case OBJVECTOR: {
        AutoObjectVector::VectorImpl& vector = static_cast<AutoObjectVector*>(this)->vector;
        TraceRootRange(trc, vector.length(), vector.begin(), "JS::AutoObjectVector.vector");
        return;
      }

      case VALARRAY: {
        /*
         * We don't know the template size parameter, but we can safely treat it
         * as an AutoValueArray<1> because the length is stored separately.
         */
        AutoValueArray<1>* array = static_cast<AutoValueArray<1>*>(this);
        TraceRootRange(trc, array->length(), array->begin(), "js::AutoValueArray");
        return;
      }

      case IONMASM: {
        static_cast<js::jit::MacroAssembler::AutoRooter*>(this)->masm()->trace(trc);
        return;
      }

      case WRAPPER: {
        /*
         * We need to use TraceManuallyBarrieredEdge here because we mark
         * wrapper roots in every slice. This is because of some rule-breaking
         * in RemapAllWrappersForObject; see comment there.
         */
        TraceManuallyBarrieredEdge(trc, &static_cast<AutoWrapperRooter*>(this)->value.get(),
                                   "JS::AutoWrapperRooter.value");
        return;
      }

      case WRAPVECTOR: {
        AutoWrapperVector::VectorImpl& vector = static_cast<AutoWrapperVector*>(this)->vector;
        /*
         * We need to use TraceManuallyBarrieredEdge here because we mark
         * wrapper roots in every slice. This is because of some rule-breaking
         * in RemapAllWrappersForObject; see comment there.
         */
        for (WrapperValue* p = vector.begin(); p < vector.end(); p++)
            TraceManuallyBarrieredEdge(trc, &p->get(), "js::AutoWrapperVector.vector");
        return;
      }

      case CUSTOM:
        static_cast<JS::CustomAutoRooter*>(this)->trace(trc);
        return;
    }

    MOZ_ASSERT(tag_ >= 0);
    if (Value* vp = static_cast<AutoArrayRooter*>(this)->array)
        TraceRootRange(trc, tag_, vp, "JS::AutoArrayRooter.array");
}

/* static */ void
AutoGCRooter::traceAll(JSTracer* trc)
{
    for (ContextIter cx(trc->runtime()); !cx.done(); cx.next())
        traceAllInContext(&*cx, trc);
}

/* static */ void
AutoGCRooter::traceAllWrappers(JSTracer* trc)
{
    for (ContextIter cx(trc->runtime()); !cx.done(); cx.next()) {
        for (AutoGCRooter* gcr = cx->roots.autoGCRooters_; gcr; gcr = gcr->down) {
            if (gcr->tag_ == WRAPVECTOR || gcr->tag_ == WRAPPER)
                gcr->trace(trc);
        }
    }
}

void
StackShape::trace(JSTracer* trc)
{
    if (base)
        TraceRoot(trc, &base, "StackShape base");

    TraceRoot(trc, (jsid*) &propid, "StackShape id");

    if ((attrs & JSPROP_GETTER) && rawGetter)
        TraceRoot(trc, (JSObject**)&rawGetter, "StackShape getter");

    if ((attrs & JSPROP_SETTER) && rawSetter)
        TraceRoot(trc, (JSObject**)&rawSetter, "StackShape setter");
}

void
PropertyDescriptor::trace(JSTracer* trc)
{
    if (obj)
        TraceRoot(trc, &obj, "Descriptor::obj");
    TraceRoot(trc, &value, "Descriptor::value");
    if ((attrs & JSPROP_GETTER) && getter) {
        JSObject* tmp = JS_FUNC_TO_DATA_PTR(JSObject*, getter);
        TraceRoot(trc, &tmp, "Descriptor::get");
        getter = JS_DATA_TO_FUNC_PTR(JSGetterOp, tmp);
    }
    if ((attrs & JSPROP_SETTER) && setter) {
        JSObject* tmp = JS_FUNC_TO_DATA_PTR(JSObject*, setter);
        TraceRoot(trc, &tmp, "Descriptor::set");
        setter = JS_DATA_TO_FUNC_PTR(JSSetterOp, tmp);
    }
}

void
js::gc::GCRuntime::markRuntime(JSTracer* trc, TraceOrMarkRuntime traceOrMark)
{
    gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_ROOTS);

    MOZ_ASSERT(traceOrMark == TraceRuntime || traceOrMark == MarkRuntime);

    MOZ_ASSERT(!rt->mainThread.suppressGC);

    if (traceOrMark == MarkRuntime) {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_CCWS);
        JSCompartment::traceIncomingCrossCompartmentEdgesForZoneGC(trc);
    }

    {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_ROOTERS);

        AutoGCRooter::traceAll(trc);

        if (!rt->isBeingDestroyed()) {
            MarkExactStackRoots(rt, trc);
            rt->markSelfHostingGlobal(trc);
        }

        for (RootRange r = rootsHash.all(); !r.empty(); r.popFront()) {
            const RootEntry& entry = r.front();
            TraceRoot(trc, entry.key(), entry.value());
        }

        MarkPersistentRooted(rt, trc);
    }

    if (!rt->isBeingDestroyed() && !rt->isHeapMinorCollecting()) {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_RUNTIME_DATA);

        if (traceOrMark == TraceRuntime || rt->atomsCompartment()->zone()->isCollecting()) {
            MarkPermanentAtoms(trc);
            MarkAtoms(trc);
            MarkWellKnownSymbols(trc);
            jit::JitRuntime::Mark(trc);
        }
    }

    if (rt->isHeapMinorCollecting())
        jit::JitRuntime::MarkJitcodeGlobalTableUnconditionally(trc);

    for (ContextIter acx(rt); !acx.done(); acx.next())
        acx->mark(trc);

    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next())
        c->traceRoots(trc, traceOrMark);

    MarkInterpreterActivations(rt, trc);

    jit::MarkJitActivations(rt, trc);

    if (!rt->isHeapMinorCollecting()) {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_EMBEDDING);

        /*
         * The embedding can register additional roots here.
         *
         * We don't need to trace these in a minor GC because all pointers into
         * the nursery should be in the store buffer, and we want to avoid the
         * time taken to trace all these roots.
         */
        for (size_t i = 0; i < blackRootTracers.length(); i++) {
            const Callback<JSTraceDataOp>& e = blackRootTracers[i];
            (*e.op)(trc, e.data);
        }

        /* During GC, we don't mark gray roots at this stage. */
        if (JSTraceDataOp op = grayRootTracer.op) {
            if (traceOrMark == TraceRuntime)
                (*op)(trc, grayRootTracer.data);
        }
    }
}

// Append traced things to a buffer on the zone for use later in the GC.
// See the comment in GCRuntime.h above grayBufferState for details.
class BufferGrayRootsTracer : public JS::CallbackTracer
{
    // Set to false if we OOM while buffering gray roots.
    bool bufferingGrayRootsFailed;

    void onChild(const JS::GCCellPtr& thing) override;

  public:
    explicit BufferGrayRootsTracer(JSRuntime* rt)
      : JS::CallbackTracer(rt), bufferingGrayRootsFailed(false)
    {}

    bool failed() const { return bufferingGrayRootsFailed; }

#ifdef DEBUG
    TracerKind getTracerKind() const override { return TracerKind::GrayBuffering; }
#endif
};

#ifdef DEBUG
// Return true if this trace is happening on behalf of gray buffering during
// the marking phase of incremental GC.
bool
js::IsBufferGrayRootsTracer(JSTracer* trc)
{
    return trc->isCallbackTracer() &&
           trc->asCallbackTracer()->getTracerKind() == JS::CallbackTracer::TracerKind::GrayBuffering;
}
#endif

void
js::gc::GCRuntime::bufferGrayRoots()
{
    // Precondition: the state has been reset to "unused" after the last GC
    //               and the zone's buffers have been cleared.
    MOZ_ASSERT(grayBufferState == GrayBufferState::Unused);
    for (GCZonesIter zone(rt); !zone.done(); zone.next())
        MOZ_ASSERT(zone->gcGrayRoots.empty());


    BufferGrayRootsTracer grayBufferer(rt);
    if (JSTraceDataOp op = grayRootTracer.op)
        (*op)(&grayBufferer, grayRootTracer.data);

    // Propagate the failure flag from the marker to the runtime.
    if (grayBufferer.failed()) {
      grayBufferState = GrayBufferState::Failed;
      resetBufferedGrayRoots();
    } else {
      grayBufferState = GrayBufferState::Okay;
    }
}

struct SetMaybeAliveFunctor {
    template <typename T> void operator()(T* t) { SetMaybeAliveFlag(t); }
};

void
BufferGrayRootsTracer::onChild(const JS::GCCellPtr& thing)
{
    MOZ_ASSERT(runtime()->isHeapBusy());

    if (bufferingGrayRootsFailed)
        return;

    gc::TenuredCell* tenured = gc::TenuredCell::fromPointer(thing.asCell());

    Zone* zone = tenured->zone();
    if (zone->isCollecting()) {
        // See the comment on SetMaybeAliveFlag to see why we only do this for
        // objects and scripts. We rely on gray root buffering for this to work,
        // but we only need to worry about uncollected dead compartments during
        // incremental GCs (when we do gray root buffering).
        DispatchTyped(SetMaybeAliveFunctor(), thing);

        if (!zone->gcGrayRoots.append(tenured))
            bufferingGrayRootsFailed = true;
    }
}

void
GCRuntime::markBufferedGrayRoots(JS::Zone* zone)
{
    MOZ_ASSERT(grayBufferState == GrayBufferState::Okay);
    MOZ_ASSERT(zone->isGCMarkingGray() || zone->isGCCompacting());

    for (auto cell : zone->gcGrayRoots)
        TraceManuallyBarrieredGenericPointerEdge(&marker, &cell, "buffered gray root");
}

void
GCRuntime::resetBufferedGrayRoots() const
{
    MOZ_ASSERT(grayBufferState != GrayBufferState::Okay,
               "Do not clear the gray buffers unless we are Failed or becoming Unused");
    for (GCZonesIter zone(rt); !zone.done(); zone.next())
        zone->gcGrayRoots.clearAndFree();
}

