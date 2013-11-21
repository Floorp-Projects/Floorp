/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Zone_h
#define gc_Zone_h

#include "mozilla/Atomics.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Util.h"

#include "jscntxt.h"
#include "jsgc.h"
#include "jsinfer.h"

#include "gc/FindSCCs.h"

namespace js {

/*
 * Encapsulates the data needed to perform allocation.  Typically there is
 * precisely one of these per zone (|cx->zone().allocator|).  However, in
 * parallel execution mode, there will be one per worker thread.
 */
class Allocator
{
    /*
     * Since allocators can be accessed from worker threads, the parent zone_
     * should not be accessed in general. ArenaLists is allowed to actually do
     * the allocation, however.
     */
    friend class gc::ArenaLists;

    JS::Zone *zone_;

  public:
    explicit Allocator(JS::Zone *zone);

    js::gc::ArenaLists arenas;
};

typedef Vector<JSCompartment *, 1, SystemAllocPolicy> CompartmentVector;

} /* namespace js */

namespace JS {

/*
 * A zone is a collection of compartments. Every compartment belongs to exactly
 * one zone. In Firefox, there is roughly one zone per tab along with a system
 * zone for everything else. Zones mainly serve as boundaries for garbage
 * collection. Unlike compartments, they have no special security properties.
 *
 * Every GC thing belongs to exactly one zone. GC things from the same zone but
 * different compartments can share an arena (4k page). GC things from different
 * zones cannot be stored in the same arena. The garbage collector is capable of
 * collecting one zone at a time; it cannot collect at the granularity of
 * compartments.
 *
 * GC things are tied to zones and compartments as follows:
 *
 * - JSObjects belong to a compartment and cannot be shared between
 *   compartments. If an object needs to point to a JSObject in a different
 *   compartment, regardless of zone, it must go through a cross-compartment
 *   wrapper. Each compartment keeps track of its outgoing wrappers in a table.
 *
 * - JSStrings do not belong to any particular compartment, but they do belong
 *   to a zone. Thus, two different compartments in the same zone can point to a
 *   JSString. When a string needs to be wrapped, we copy it if it's in a
 *   different zone and do nothing if it's in the same zone. Thus, transferring
 *   strings within a zone is very efficient.
 *
 * - Shapes and base shapes belong to a compartment and cannot be shared between
 *   compartments. A base shape holds a pointer to its compartment. Shapes find
 *   their compartment via their base shape. JSObjects find their compartment
 *   via their shape.
 *
 * - Scripts are also compartment-local and cannot be shared. A script points to
 *   its compartment.
 *
 * - Type objects and IonCode objects belong to a compartment and cannot be
 *   shared. However, there is no mechanism to obtain their compartments.
 *
 * A zone remains alive as long as any GC things in the zone are alive. A
 * compartment remains alive as long as any JSObjects, scripts, shapes, or base
 * shapes within it are alive.
 *
 * We always guarantee that a zone has at least one live compartment by refusing
 * to delete the last compartment in a live zone. (This could happen, for
 * example, if the conservative scanner marks a string in an otherwise dead
 * zone.)
 */

struct Zone : public JS::shadow::Zone,
              public js::gc::GraphNodeBase<JS::Zone>,
              public js::MallocProvider<JS::Zone>
{
  private:
    friend bool js::CurrentThreadCanAccessZone(Zone *zone);

  public:
    js::Allocator                allocator;

    js::CompartmentVector        compartments;

    bool                         hold;

  private:
    bool                         ionUsingBarriers_;

  public:
    bool                         active;  // GC flag, whether there are active frames

    bool compileBarriers(bool needsBarrier) const {
        return needsBarrier || runtimeFromMainThread()->gcZeal() == js::gc::ZealVerifierPreValue;
    }

    bool compileBarriers() const {
        return compileBarriers(needsBarrier());
    }

    enum ShouldUpdateIon {
        DontUpdateIon,
        UpdateIon
    };

    void setNeedsBarrier(bool needs, ShouldUpdateIon updateIon);

    const bool *addressOfNeedsBarrier() const {
        return &needsBarrier_;
    }

  public:
    enum GCState {
        NoGC,
        Mark,
        MarkGray,
        Sweep,
        Finished
    };

  private:
    bool                         gcScheduled;
    GCState                      gcState;
    bool                         gcPreserveCode;

  public:
    bool isCollecting() const {
        if (runtimeFromMainThread()->isHeapCollecting())
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
        return runtimeFromMainThread()->isHeapMajorCollecting() && gcState != NoGC;
    }

    void setGCState(GCState state) {
        JS_ASSERT(runtimeFromMainThread()->isHeapBusy());
        JS_ASSERT_IF(state != NoGC, canCollect());
        gcState = state;
    }

    void scheduleGC() {
        JS_ASSERT(!runtimeFromMainThread()->isHeapBusy());
        gcScheduled = true;
    }

    void unscheduleGC() {
        gcScheduled = false;
    }

    bool isGCScheduled() {
        return gcScheduled && canCollect();
    }

    void setPreservingCode(bool preserving) {
        gcPreserveCode = preserving;
    }

    bool canCollect() {
        // Zones cannot be collected while in use by other threads.
        if (usedByExclusiveThread)
            return false;
        JSRuntime *rt = runtimeFromAnyThread();
        if (rt->isAtomsZone(this) && rt->exclusiveThreadsPresent())
            return false;
        return true;
    }

    bool wasGCStarted() const {
        return gcState != NoGC;
    }

    bool isGCMarking() {
        if (runtimeFromMainThread()->isHeapCollecting())
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

    /* This is updated by both the main and GC helper threads. */
    mozilla::Atomic<size_t, mozilla::ReleaseAcquire> gcBytes;

    size_t                       gcTriggerBytes;
    size_t                       gcMaxMallocBytes;
    double                       gcHeapGrowthFactor;

    bool                         isSystem;

    /* Whether this zone is being used by a thread with an ExclusiveContext. */
    bool usedByExclusiveThread;

    /*
     * Get a number that is incremented whenever this zone is collected, and
     * possibly at other times too.
     */
    uint64_t gcNumber();

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
    mozilla::Atomic<ptrdiff_t, mozilla::ReleaseAcquire> gcMallocBytes;

    /*
     * Whether a GC has been triggered as a result of gcMallocBytes falling
     * below zero.
     *
     * This should be a bool, but Atomic only supports 32-bit and pointer-sized
     * types.
     */
    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> gcMallocGCTriggered;

    /* This compartment's gray roots. */
    js::Vector<js::GrayRoot, 0, js::SystemAllocPolicy> gcGrayRoots;

    /* Per-zone data for use by an embedder. */
    void *data;

    Zone(JSRuntime *rt);
    ~Zone();
    bool init(JSContext *cx);

    void findOutgoingEdges(js::gc::ComponentFinder<JS::Zone> &finder);

    void discardJitCode(js::FreeOp *fop);

    void addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf, size_t *typePool);

    void setGCLastBytes(size_t lastBytes, js::JSGCInvocationKind gckind);
    void reduceGCTriggerBytes(size_t amount);

    void resetGCMallocBytes();
    void setGCMaxMallocBytes(size_t value);
    void updateMallocCounter(size_t nbytes) {
        /*
         * Note: this code may be run from worker threads.  We
         * tolerate any thread races when updating gcMallocBytes.
         */
        gcMallocBytes -= ptrdiff_t(nbytes);
        if (JS_UNLIKELY(isTooMuchMalloc()))
            onTooMuchMalloc();
    }

    bool isTooMuchMalloc() const {
        return gcMallocBytes <= 0;
     }

    void onTooMuchMalloc();

    void *onOutOfMemory(void *p, size_t nbytes) {
        return runtimeFromMainThread()->onOutOfMemory(p, nbytes);
    }
    void reportAllocationOverflow() {
        js_ReportAllocationOverflow(nullptr);
    }

    void markTypes(JSTracer *trc);

    js::types::TypeZone types;

    void sweep(js::FreeOp *fop, bool releaseTypes);

  private:
    void sweepBreakpoints(js::FreeOp *fop);
};

} /* namespace JS */

namespace js {

/*
 * Using the atoms zone without holding the exclusive access lock is dangerous
 * because worker threads may be using it simultaneously. Therefore, it's
 * better to skip the atoms zone when iterating over zones. If you need to
 * iterate over the atoms zone, consider taking the exclusive access lock first.
 */
enum ZoneSelector {
    WithAtoms,
    SkipAtoms
};

class ZonesIter {
  private:
    JS::Zone **it, **end;

  public:
    ZonesIter(JSRuntime *rt, ZoneSelector selector) {
        it = rt->zones.begin();
        end = rt->zones.end();

        if (selector == SkipAtoms) {
            JS_ASSERT(rt->isAtomsZone(*it));
            it++;
        }
    }

    bool done() const { return it == end; }

    void next() {
        JS_ASSERT(!done());
        do {
            it++;
        } while (!done() && (*it)->usedByExclusiveThread);
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
    // This is for the benefit of CompartmentsIterT::comp.
    friend class mozilla::Maybe<CompartmentsInZoneIter>;
  private:
    JSCompartment **it, **end;

    CompartmentsInZoneIter()
      : it(nullptr), end(nullptr)
    {}

  public:
    explicit CompartmentsInZoneIter(JS::Zone *zone) {
        it = zone->compartments.begin();
        end = zone->compartments.end();
    }

    bool done() const {
        JS_ASSERT(it);
        return it == end;
    }
    void next() {
        JS_ASSERT(!done());
        it++;
    }

    JSCompartment *get() const {
        JS_ASSERT(it);
        return *it;
    }

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
    explicit CompartmentsIterT(JSRuntime *rt)
      : zone(rt)
    {
        if (zone.done())
            comp.construct();
        else
            comp.construct(zone);
    }

    CompartmentsIterT(JSRuntime *rt, ZoneSelector selector)
      : zone(rt, selector)
    {
        if (zone.done())
            comp.construct();
        else
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

#endif /* gc_Zone_h */
