/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Zone_h
#define gc_Zone_h

#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"

#include "jscntxt.h"
#include "jsgc.h"
#include "jsinfer.h"

#include "gc/FindSCCs.h"

namespace js {

namespace jit {
class JitZone;
}

// Encapsulates the data needed to perform allocation. Typically there is
// precisely one of these per zone (|cx->zone().allocator|). However, in
// parallel execution mode, there will be one per worker thread.
class Allocator
{
  public:
    explicit Allocator(JS::Zone *zone);

    js::gc::ArenaLists arenas;

  private:
    // Since allocators can be accessed from worker threads, the parent zone_
    // should not be accessed in general. ArenaLists is allowed to actually do
    // the allocation, however.
    friend class gc::ArenaLists;

    JS::Zone *zone_;
};

} // namespace js

namespace JS {

// A zone is a collection of compartments. Every compartment belongs to exactly
// one zone. In Firefox, there is roughly one zone per tab along with a system
// zone for everything else. Zones mainly serve as boundaries for garbage
// collection. Unlike compartments, they have no special security properties.
//
// Every GC thing belongs to exactly one zone. GC things from the same zone but
// different compartments can share an arena (4k page). GC things from different
// zones cannot be stored in the same arena. The garbage collector is capable of
// collecting one zone at a time; it cannot collect at the granularity of
// compartments.
//
// GC things are tied to zones and compartments as follows:
//
// - JSObjects belong to a compartment and cannot be shared between
//   compartments. If an object needs to point to a JSObject in a different
//   compartment, regardless of zone, it must go through a cross-compartment
//   wrapper. Each compartment keeps track of its outgoing wrappers in a table.
//
// - JSStrings do not belong to any particular compartment, but they do belong
//   to a zone. Thus, two different compartments in the same zone can point to a
//   JSString. When a string needs to be wrapped, we copy it if it's in a
//   different zone and do nothing if it's in the same zone. Thus, transferring
//   strings within a zone is very efficient.
//
// - Shapes and base shapes belong to a compartment and cannot be shared between
//   compartments. A base shape holds a pointer to its compartment. Shapes find
//   their compartment via their base shape. JSObjects find their compartment
//   via their shape.
//
// - Scripts are also compartment-local and cannot be shared. A script points to
//   its compartment.
//
// - Type objects and JitCode objects belong to a compartment and cannot be
//   shared. However, there is no mechanism to obtain their compartments.
//
// A zone remains alive as long as any GC things in the zone are alive. A
// compartment remains alive as long as any JSObjects, scripts, shapes, or base
// shapes within it are alive.
//
// We always guarantee that a zone has at least one live compartment by refusing
// to delete the last compartment in a live zone. (This could happen, for
// example, if the conservative scanner marks a string in an otherwise dead
// zone.)
struct Zone : public JS::shadow::Zone,
              public js::gc::GraphNodeBase<JS::Zone>,
              public js::MallocProvider<JS::Zone>
{
    explicit Zone(JSRuntime *rt);
    ~Zone();
    bool init();

    void findOutgoingEdges(js::gc::ComponentFinder<JS::Zone> &finder);

    void discardJitCode(js::FreeOp *fop);

    void addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                size_t *typePool,
                                size_t *baselineStubsOptimized);

    void setGCLastBytes(size_t lastBytes, js::JSGCInvocationKind gckind);
    void reduceGCTriggerBytes(size_t amount);

    void resetGCMallocBytes();
    void setGCMaxMallocBytes(size_t value);
    void updateMallocCounter(size_t nbytes) {
        // Note: this code may be run from worker threads. We tolerate any
        // thread races when updating gcMallocBytes.
        gcMallocBytes -= ptrdiff_t(nbytes);
        if (MOZ_UNLIKELY(isTooMuchMalloc()))
            onTooMuchMalloc();
    }

    bool isTooMuchMalloc() const { return gcMallocBytes <= 0; }
    void onTooMuchMalloc();

    void *onOutOfMemory(void *p, size_t nbytes) {
        return runtimeFromMainThread()->onOutOfMemory(p, nbytes);
    }
    void reportAllocationOverflow() { js_ReportAllocationOverflow(nullptr); }

    void sweep(js::FreeOp *fop, bool releaseTypes, bool *oom);

    bool hasMarkedCompartments();

    void scheduleGC() { JS_ASSERT(!runtimeFromMainThread()->isHeapBusy()); gcScheduled_ = true; }
    void unscheduleGC() { gcScheduled_ = false; }
    bool isGCScheduled() { return gcScheduled_ && canCollect(); }

    void setPreservingCode(bool preserving) { gcPreserveCode_ = preserving; }
    bool isPreservingCode() const { return gcPreserveCode_; }

    bool canCollect();

    enum GCState {
        NoGC,
        Mark,
        MarkGray,
        Sweep,
        Finished
    };
    void setGCState(GCState state) {
        JS_ASSERT(runtimeFromMainThread()->isHeapBusy());
        JS_ASSERT_IF(state != NoGC, canCollect());
        gcState_ = state;
    }

    bool isCollecting() const {
        if (runtimeFromMainThread()->isHeapCollecting())
            return gcState_ != NoGC;
        else
            return needsBarrier();
    }

    // If this returns true, all object tracing must be done with a GC marking
    // tracer.
    bool requireGCTracer() const {
        return runtimeFromMainThread()->isHeapMajorCollecting() && gcState_ != NoGC;
    }

    bool isGCMarking() {
        if (runtimeFromMainThread()->isHeapCollecting())
            return gcState_ == Mark || gcState_ == MarkGray;
        else
            return needsBarrier();
    }

    bool wasGCStarted() const { return gcState_ != NoGC; }
    bool isGCMarkingBlack() { return gcState_ == Mark; }
    bool isGCMarkingGray() { return gcState_ == MarkGray; }
    bool isGCSweeping() { return gcState_ == Sweep; }
    bool isGCFinished() { return gcState_ == Finished; }

    // Get a number that is incremented whenever this zone is collected, and
    // possibly at other times too.
    uint64_t gcNumber();

    bool compileBarriers() const { return compileBarriers(needsBarrier()); }
    bool compileBarriers(bool needsBarrier) const {
        return needsBarrier || runtimeFromMainThread()->gcZeal() == js::gc::ZealVerifierPreValue;
    }

    enum ShouldUpdateJit { DontUpdateJit, UpdateJit };
    void setNeedsBarrier(bool needs, ShouldUpdateJit updateJit);
    const bool *addressOfNeedsBarrier() const { return &needsBarrier_; }

    js::jit::JitZone *getJitZone(JSContext *cx) { return jitZone_ ? jitZone_ : createJitZone(cx); }
    js::jit::JitZone *jitZone() { return jitZone_; }

#ifdef DEBUG
    // For testing purposes, return the index of the zone group which this zone
    // was swept in in the last GC.
    unsigned lastZoneGroupIndex() { return gcLastZoneGroupIndex; }
#endif

  private:
    void sweepBreakpoints(js::FreeOp *fop);
    void sweepCompartments(js::FreeOp *fop, bool keepAtleastOne, bool lastGC);

    js::jit::JitZone *createJitZone(JSContext *cx);

  public:
    js::Allocator allocator;

    js::types::TypeZone types;

    // The set of compartments in this zone.
    typedef js::Vector<JSCompartment *, 1, js::SystemAllocPolicy> CompartmentVector;
    CompartmentVector compartments;

    // This compartment's gray roots.
    typedef js::Vector<js::GrayRoot, 0, js::SystemAllocPolicy> GrayRootVector;
    GrayRootVector gcGrayRoots;

    // A set of edges from this zone to other zones.
    //
    // This is used during GC while calculating zone groups to record edges that
    // can't be determined by examining this zone by itself.
    typedef js::HashSet<Zone *, js::DefaultHasher<Zone *>, js::SystemAllocPolicy> ZoneSet;
    ZoneSet gcZoneGroupEdges;

    // The "growth factor" for computing our next thresholds after a GC.
    double gcHeapGrowthFactor;

    // Malloc counter to measure memory pressure for GC scheduling. It runs from
    // gcMaxMallocBytes down to zero. This counter should be used only when it's
    // not possible to know the size of a free.
    mozilla::Atomic<ptrdiff_t, mozilla::ReleaseAcquire> gcMallocBytes;

    // GC trigger threshold for allocations on the C heap.
    size_t gcMaxMallocBytes;

    // Whether a GC has been triggered as a result of gcMallocBytes falling
    // below zero.
    //
    // This should be a bool, but Atomic only supports 32-bit and pointer-sized
    // types.
    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> gcMallocGCTriggered;

    // Counts the number of bytes allocated in the GC heap for this zone. It is
    // updated by both the main and GC helper threads.
    mozilla::Atomic<size_t, mozilla::ReleaseAcquire> gcBytes;

    // GC trigger threshold for allocations on the GC heap.
    size_t gcTriggerBytes;

    // Per-zone data for use by an embedder.
    void *data;

    bool isSystem;

    bool usedByExclusiveThread;

    // These flags help us to discover if a compartment that shouldn't be alive
    // manages to outlive a GC.
    bool scheduledForDestruction;
    bool maybeAlive;

    // True when there are active frames.
    bool active;

    mozilla::DebugOnly<unsigned> gcLastZoneGroupIndex;

  private:
    js::jit::JitZone *jitZone_;

    GCState gcState_;
    bool gcScheduled_;
    bool gcPreserveCode_;
    bool jitUsingBarriers_;

    friend bool js::CurrentThreadCanAccessZone(Zone *zone);
    friend class js::gc::GCRuntime;
};

} // namespace JS

namespace js {

// Using the atoms zone without holding the exclusive access lock is dangerous
// because worker threads may be using it simultaneously. Therefore, it's
// better to skip the atoms zone when iterating over zones. If you need to
// iterate over the atoms zone, consider taking the exclusive access lock first.
enum ZoneSelector {
    WithAtoms,
    SkipAtoms
};

class ZonesIter
{
    JS::Zone **it, **end;

  public:
    ZonesIter(JSRuntime *rt, ZoneSelector selector) {
        it = rt->gc.zones.begin();
        end = rt->gc.zones.end();

        if (selector == SkipAtoms) {
            MOZ_ASSERT(atAtomsZone(rt));
            it++;
        }
    }

    bool atAtomsZone(JSRuntime *rt);

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

  private:
    JSCompartment **it, **end;

    CompartmentsInZoneIter()
      : it(nullptr), end(nullptr)
    {}

    // This is for the benefit of CompartmentsIterT::comp.
    friend class mozilla::Maybe<CompartmentsInZoneIter>;
};

// This iterator iterates over all the compartments in a given set of zones. The
// set of zones is determined by iterating ZoneIterT.
template<class ZonesIterT>
class CompartmentsIterT
{
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

// Return the Zone* of a Value. Asserts if the Value is not a GC thing.
Zone *
ZoneOfValue(const JS::Value &value);

} // namespace js

#endif // gc_Zone_h
