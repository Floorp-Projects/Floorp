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

// For more detail see JS::Rooted::ptr and js::DispatchWrapper.
//
// The JS::RootKind::Traceable list contains a bunch of totally disparate
// types, but the instantiations of DispatchWrapper below need /something/ in
// the type field. We use the following type as a compatible stand-in. No
// actual methods from ConcreteTraceable type are actually used at runtime --
// the real trace function has been stored inline in the DispatchWrapper.
struct ConcreteTraceable {
    ConcreteTraceable() { MOZ_CRASH("instantiation of ConcreteTraceable"); }
    void trace(JSTracer*) {}
};

template <typename T, TraceFunction<T> TraceFn = TraceNullableRoot>
static inline void
TraceExactStackRootList(JSTracer* trc, JS::Rooted<void*>* rooter, const char* name)
{
    while (rooter) {
        T* addr = reinterpret_cast<JS::Rooted<T>*>(rooter)->address();
        TraceFn(trc, addr, name);
        rooter = rooter->previous();
    }
}

static inline void
TraceStackRoots(JSTracer* trc, RootedListHeads& stackRoots)
{
#define TRACE_ROOTS(name, type, _) \
    TraceExactStackRootList<type*>(trc, stackRoots[JS::RootKind::name], "exact-" #name);
JS_FOR_EACH_TRACEKIND(TRACE_ROOTS)
#undef TRACE_ROOTS
    TraceExactStackRootList<jsid>(trc, stackRoots[JS::RootKind::Id], "exact-id");
    TraceExactStackRootList<Value>(trc, stackRoots[JS::RootKind::Value], "exact-value");
    TraceExactStackRootList<ConcreteTraceable,
                           js::DispatchWrapper<ConcreteTraceable>::TraceWrapped>(
        trc, stackRoots[JS::RootKind::Traceable], "Traceable");
}

void
js::RootLists::traceStackRoots(JSTracer* trc)
{
    TraceStackRoots(trc, stackRoots_);
}

static void
TraceExactStackRoots(JSRuntime* rt, JSTracer* trc)
{
    for (ZonesIter zone(rt, SkipAtoms); !zone.done(); zone.next())
        TraceStackRoots(trc, zone->stackRoots_);
    rt->contextFromMainThread()->roots.traceStackRoots(trc);
}

template <typename T, TraceFunction<T> TraceFn = TraceNullableRoot>
static inline void
TracePersistentRootedList(JSTracer* trc, mozilla::LinkedList<PersistentRooted<void*>>& list,
                         const char* name)
{
    for (PersistentRooted<void*>* r : list)
        TraceFn(trc, reinterpret_cast<PersistentRooted<T>*>(r)->address(), name);
}

void
js::RootLists::tracePersistentRoots(JSTracer* trc)
{
#define TRACE_ROOTS(name, type, _) \
    TracePersistentRootedList<type*>(trc, heapRoots_[JS::RootKind::name], "persistent-" #name);
JS_FOR_EACH_TRACEKIND(TRACE_ROOTS)
#undef TRACE_ROOTS
    TracePersistentRootedList<jsid>(trc, heapRoots_[JS::RootKind::Id], "persistent-id");
    TracePersistentRootedList<Value>(trc, heapRoots_[JS::RootKind::Value], "persistent-value");
    TracePersistentRootedList<ConcreteTraceable,
                             js::DispatchWrapper<ConcreteTraceable>::TraceWrapped>(trc,
            heapRoots_[JS::RootKind::Traceable], "persistent-traceable");
}

static void
TracePersistentRooted(JSRuntime* rt, JSTracer* trc)
{
    rt->contextFromMainThread()->roots.tracePersistentRoots(trc);
}

template <typename T>
static void
FinishPersistentRootedChain(mozilla::LinkedList<PersistentRooted<void*>>& listArg)
{
    auto& list = reinterpret_cast<mozilla::LinkedList<PersistentRooted<T>>&>(listArg);
    while (!list.isEmpty())
        list.getFirst()->reset();
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

    // Note that we do not finalize the Traceable list as we do not know how to
    // safely clear memebers. We instead assert that none escape the RootLists.
    // See the comment on RootLists::~RootLists for details.
}

inline void
AutoGCRooter::trace(JSTracer* trc)
{
    switch (tag_) {
      case PARSER:
        frontend::TraceParser(trc, this);
        return;

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
         * We need to use TraceManuallyBarrieredEdge here because we trace
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
         * We need to use TraceManuallyBarrieredEdge here because we trace
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
    for (AutoGCRooter* gcr = trc->runtime()->contextFromMainThread()->roots.autoGCRooters_; gcr; gcr = gcr->down)
        gcr->trace(trc);
}

/* static */ void
AutoGCRooter::traceAllWrappers(JSTracer* trc)
{
    JSContext* cx = trc->runtime()->contextFromMainThread();

    for (AutoGCRooter* gcr = cx->roots.autoGCRooters_; gcr; gcr = gcr->down) {
        if (gcr->tag_ == WRAPVECTOR || gcr->tag_ == WRAPPER)
            gcr->trace(trc);
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
js::gc::GCRuntime::traceRuntimeForMajorGC(JSTracer* trc, AutoLockForExclusiveAccess& lock)
{
    // FinishRoots will have asserted that every root that we do not expect
    // is gone, so we can simply skip traceRuntime here.
    if (rt->isBeingDestroyed())
        return;

    gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_ROOTS);
    if (rt->atomsCompartment(lock)->zone()->isCollecting())
        traceRuntimeAtoms(trc, lock);
    JSCompartment::traceIncomingCrossCompartmentEdgesForZoneGC(trc);
    traceRuntimeCommon(trc, MarkRuntime, lock);
}

void
js::gc::GCRuntime::traceRuntimeForMinorGC(JSTracer* trc, AutoLockForExclusiveAccess& lock)
{
    // Note that we *must* trace the runtime during the SHUTDOWN_GC's minor GC
    // despite having called FinishRoots already. This is because FinishRoots
    // does not clear the crossCompartmentWrapper map. It cannot do this
    // because Proxy's trace for CrossCompartmentWrappers asserts presence in
    // the map. And we can reach its trace function despite having finished the
    // roots via the edges stored by the pre-barrier verifier when we finish
    // the verifier for the last time.
    gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_ROOTS);

    // FIXME: As per bug 1298816 comment 12, we should be able to remove this.
    jit::JitRuntime::TraceJitcodeGlobalTable(trc);

    traceRuntimeCommon(trc, TraceRuntime, lock);
}

void
js::TraceRuntime(JSTracer* trc)
{
    MOZ_ASSERT(!trc->isMarkingTracer());

    JSRuntime* rt = trc->runtime();
    rt->gc.evictNursery();
    AutoPrepareForTracing prep(rt->contextFromMainThread(), WithAtoms);
    gcstats::AutoPhase ap(rt->gc.stats, gcstats::PHASE_TRACE_HEAP);
    rt->gc.traceRuntime(trc, prep.session().lock);
}

void
js::gc::GCRuntime::traceRuntime(JSTracer* trc, AutoLockForExclusiveAccess& lock)
{
    MOZ_ASSERT(!rt->isBeingDestroyed());

    gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_ROOTS);
    traceRuntimeAtoms(trc, lock);
    traceRuntimeCommon(trc, TraceRuntime, lock);
}

void
js::gc::GCRuntime::traceRuntimeAtoms(JSTracer* trc, AutoLockForExclusiveAccess& lock)
{
    gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_RUNTIME_DATA);
    TracePermanentAtoms(trc);
    TraceAtoms(trc, lock);
    TraceWellKnownSymbols(trc);
    jit::JitRuntime::Trace(trc, lock);
}

void
js::gc::GCRuntime::traceRuntimeCommon(JSTracer* trc, TraceOrMarkRuntime traceOrMark,
                                      AutoLockForExclusiveAccess& lock)
{
    MOZ_ASSERT(!rt->mainThread.suppressGC);

    {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_STACK);

        // Trace active interpreter and JIT stack roots.
        TraceInterpreterActivations(rt, trc);
        jit::TraceJitActivations(rt, trc);

        // Trace legacy C stack roots.
        AutoGCRooter::traceAll(trc);

        for (RootRange r = rootsHash.all(); !r.empty(); r.popFront()) {
            const RootEntry& entry = r.front();
            TraceRoot(trc, entry.key(), entry.value());
        }

        // Trace C stack roots.
        TraceExactStackRoots(rt, trc);
    }

    // Trace runtime global roots.
    TracePersistentRooted(rt, trc);

    // Trace the self-hosting global compartment.
    rt->traceSelfHostingGlobal(trc);

    // Trace the shared Intl data.
    rt->traceSharedIntlData(trc);

    // Trace anything in the single context. Note that this is actually the
    // same struct as the JSRuntime, but is still split for historical reasons.
    rt->contextFromMainThread()->trace(trc);

    // Trace all compartment roots, but not the compartment itself; it is
    // traced via the parent pointer if traceRoots actually traces anything.
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next())
        c->traceRoots(trc, traceOrMark);

    // Trace SPS.
    rt->spsProfiler.trace(trc);

    // Trace helper thread roots.
    HelperThreadState().trace(trc);

    // Trace the embedding's black and gray roots.
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

        /* During GC, we don't trace gray roots at this stage. */
        if (JSTraceDataOp op = grayRootTracer.op) {
            if (traceOrMark == TraceRuntime)
                (*op)(trc, grayRootTracer.data);
        }
    }
}

#ifdef DEBUG
class AssertNoRootsTracer : public JS::CallbackTracer
{
    void onChild(const JS::GCCellPtr& thing) override {
        MOZ_CRASH("There should not be any roots after finishRoots");
    }

  public:
    AssertNoRootsTracer(JSRuntime* rt, WeakMapTraceKind weakTraceKind)
      : JS::CallbackTracer(rt, weakTraceKind)
    {}
};
#endif // DEBUG

void
js::gc::GCRuntime::finishRoots()
{
    rt->finishAtoms();

    if (rootsHash.initialized())
        rootsHash.clear();

    rt->contextFromMainThread()->roots.finishPersistentRoots();

    rt->finishSelfHosting();

    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next())
        c->finishRoots();

#ifdef DEBUG
    // The nsWrapperCache may not be empty before our shutdown GC, so we have
    // to skip that table when verifying that we are fully unrooted.
    auto prior = grayRootTracer;
    grayRootTracer = Callback<JSTraceDataOp>(nullptr, nullptr);

    AssertNoRootsTracer trc(rt, TraceWeakMapKeysValues);
    AutoPrepareForTracing prep(rt->contextFromMainThread(), WithAtoms);
    gcstats::AutoPhase ap(rt->gc.stats, gcstats::PHASE_TRACE_HEAP);
    traceRuntime(&trc, prep.session().lock);

    // Restore the wrapper tracing so that we leak instead of leaving dangling
    // pointers.
    grayRootTracer = prior;
#endif // DEBUG
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

    gcstats::AutoPhase ap(stats, gcstats::PHASE_BUFFER_GRAY_ROOTS);

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
    MOZ_RELEASE_ASSERT(thing);
    // Check if |thing| is corrupt by calling a method that touches the heap.
    MOZ_RELEASE_ASSERT(thing.asCell()->getTraceKind() <= JS::TraceKind::Null);

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

