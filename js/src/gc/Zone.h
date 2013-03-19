/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_zone_h___
#define gc_zone_h___

#include "mozilla/Attributes.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/Util.h"

#include "jscntxt.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinfer.h"
#include "jsobj.h"

#include "gc/StoreBuffer.h"
#include "gc/FindSCCs.h"
#include "vm/GlobalObject.h"
#include "vm/RegExpObject.h"
#include "vm/Shape.h"

namespace js {

/*
 * Encapsulates the data needed to perform allocation.  Typically
 * there is precisely one of these per compartment
 * (|compartment.allocator|).  However, in parallel execution mode,
 * there will be one per worker thread.  In general, if a piece of
 * code must perform execution and should work safely either in
 * parallel or sequential mode, you should make it take an
 * |Allocator*| rather than a |JSContext*|.
 */
class Allocator : public MallocProvider<Allocator>
{
    JS::Zone *zone;

  public:
    explicit Allocator(JS::Zone *zone);

    js::gc::ArenaLists arenas;

    inline void *parallelNewGCThing(gc::AllocKind thingKind, size_t thingSize);

    inline void *onOutOfMemory(void *p, size_t nbytes);
    inline void updateMallocCounter(size_t nbytes);
    inline void reportAllocationOverflow();
};

typedef Vector<JSCompartment *, 1, SystemAllocPolicy> CompartmentVector;

} /* namespace js */

namespace JS {

struct Zone : private JS::shadow::Zone, public js::gc::GraphNodeBase<JS::Zone>
{
    JSRuntime                    *rt;
    js::Allocator                allocator;

    js::CompartmentVector        compartments;

    bool                         hold;

#ifdef JSGC_GENERATIONAL
    js::gc::Nursery              gcNursery;
    js::gc::StoreBuffer          gcStoreBuffer;
#endif

  private:
    bool                         ionUsingBarriers_;
  public:

    bool                         active;  // GC flag, whether there are active frames

    bool needsBarrier() const {
        return needsBarrier_;
    }

    bool compileBarriers(bool needsBarrier) const {
        return needsBarrier || rt->gcZeal() == js::gc::ZealVerifierPreValue;
    }

    bool compileBarriers() const {
        return compileBarriers(needsBarrier());
    }

    enum ShouldUpdateIon {
        DontUpdateIon,
        UpdateIon
    };

    void setNeedsBarrier(bool needs, ShouldUpdateIon updateIon);

    static size_t OffsetOfNeedsBarrier() {
        return offsetof(Zone, needsBarrier_);
    }

    js::GCMarker *barrierTracer() {
        JS_ASSERT(needsBarrier_);
        return &rt->gcMarker;
    }

  public:
    enum CompartmentGCState {
        NoGC,
        Mark,
        MarkGray,
        Sweep,
        Finished
    };

  private:
    bool                         gcScheduled;
    CompartmentGCState           gcState;
    bool                         gcPreserveCode;

  public:
    bool isCollecting() const {
        if (rt->isHeapCollecting())
            return gcState != NoGC;
        else
            return needsBarrier();
    }

    bool isPreservingCode() const {
        return gcPreserveCode;
    }

    /*
     * If this returns true, all object tracing must be done with a GC marking
     * tracer.
     */
    bool requireGCTracer() const {
        return rt->isHeapCollecting() && gcState != NoGC;
    }

    void setGCState(CompartmentGCState state) {
        JS_ASSERT(rt->isHeapBusy());
        gcState = state;
    }

    void scheduleGC() {
        JS_ASSERT(!rt->isHeapBusy());
        gcScheduled = true;
    }

    void unscheduleGC() {
        gcScheduled = false;
    }

    bool isGCScheduled() const {
        return gcScheduled;
    }

    void setPreservingCode(bool preserving) {
        gcPreserveCode = preserving;
    }

    bool wasGCStarted() const {
        return gcState != NoGC;
    }

    bool isGCMarking() {
        if (rt->isHeapCollecting())
            return gcState == Mark || gcState == MarkGray;
        else
            return needsBarrier();
    }

    bool isGCMarkingBlack() {
        return gcState == Mark;
    }

    bool isGCMarkingGray() {
        return gcState == MarkGray;
    }

    bool isGCSweeping() {
        return gcState == Sweep;
    }

    bool isGCFinished() {
        return gcState == Finished;
    }

    volatile size_t              gcBytes;
    size_t                       gcTriggerBytes;
    size_t                       gcMaxMallocBytes;
    double                       gcHeapGrowthFactor;

    bool                         isSystem;

    /*
     * These flags help us to discover if a compartment that shouldn't be alive
     * manages to outlive a GC.
     */
    bool                         scheduledForDestruction;
    bool                         maybeAlive;

    /*
     * Malloc counter to measure memory pressure for GC scheduling. It runs from
     * gcMaxMallocBytes down to zero. This counter should be used only when it's
     * not possible to know the size of a free.
     */
    ptrdiff_t                    gcMallocBytes;

    /* This compartment's gray roots. */
    js::Vector<js::GrayRoot, 0, js::SystemAllocPolicy> gcGrayRoots;

    Zone(JSRuntime *rt);
    ~Zone();
    bool init(JSContext *cx);

    void findOutgoingEdges(js::gc::ComponentFinder<JS::Zone> &finder);

    void discardJitCode(js::FreeOp *fop, bool discardConstraints);

    void sizeOfIncludingThis(JSMallocSizeOfFun mallocSizeOf, size_t *typePool);

    void setGCLastBytes(size_t lastBytes, js::JSGCInvocationKind gckind);
    void reduceGCTriggerBytes(size_t amount);

    void resetGCMallocBytes();
    void setGCMaxMallocBytes(size_t value);
    void updateMallocCounter(size_t nbytes) {
        /*
         * Note: this code may be run from worker threads.  We
         * tolerate any thread races when updating gcMallocBytes.
         */
        ptrdiff_t oldCount = gcMallocBytes;
        ptrdiff_t newCount = oldCount - ptrdiff_t(nbytes);
        gcMallocBytes = newCount;
        if (JS_UNLIKELY(newCount <= 0 && oldCount > 0))
            onTooMuchMalloc();
    }

    bool isTooMuchMalloc() const {
        return gcMallocBytes <= 0;
     }

    void onTooMuchMalloc();

    void markTypes(JSTracer *trc);

    js::types::TypeZone types;

    void sweep(js::FreeOp *fop, bool releaseTypes);
};

} /* namespace JS */

namespace js {

class ZonesIter {
  private:
    JS::Zone **it, **end;

  public:
    ZonesIter(JSRuntime *rt) {
        it = rt->zones.begin();
        end = rt->zones.end();
    }

    bool done() const { return it == end; }

    void next() {
        JS_ASSERT(!done());
        it++;
    }

    JS::Zone *get() const {
        JS_ASSERT(!done());
        return *it;
    }

    operator JS::Zone *() const { return get(); }
    JS::Zone *operator->() const { return get(); }
};

struct CompartmentsInZoneIter
{
  private:
    JSCompartment **it, **end;

  public:
    CompartmentsInZoneIter(JS::Zone *zone) {
        it = zone->compartments.begin();
        end = zone->compartments.end();
    }

    bool done() const { return it == end; }
    void next() {
        JS_ASSERT(!done());
        it++;
    }

    JSCompartment *get() const { return *it; }

    operator JSCompartment *() const { return get(); }
    JSCompartment *operator->() const { return get(); }
};

/*
 * This iterator iterates over all the compartments in a given set of zones. The
 * set of zones is determined by iterating ZoneIterT.
 */
template<class ZonesIterT>
class CompartmentsIterT
{
  private:
    ZonesIterT zone;
    mozilla::Maybe<CompartmentsInZoneIter> comp;

  public:
    CompartmentsIterT(JSRuntime *rt)
      : zone(rt)
    {
        JS_ASSERT(!zone.done());
        comp.construct(zone);
    }

    bool done() const { return zone.done(); }

    void next() {
        JS_ASSERT(!done());
        JS_ASSERT(!comp.ref().done());
        comp.ref().next();
        if (comp.ref().done()) {
            comp.destroy();
            zone.next();
            if (!zone.done())
                comp.construct(zone);
        }
    }

    JSCompartment *get() const {
        JS_ASSERT(!done());
        return comp.ref();
    }

    operator JSCompartment *() const { return get(); }
    JSCompartment *operator->() const { return get(); }
};

typedef CompartmentsIterT<ZonesIter> CompartmentsIter;

} /* namespace js */

#endif /* gc_zone_h___ */
