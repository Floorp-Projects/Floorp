/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_GCRuntime_h
#define gc_GCRuntime_h

#include "mozilla/Atomics.h"
#include "mozilla/EnumSet.h"

#include "jsfriendapi.h"
#include "jsgc.h"

#include "gc/AtomMarking.h"
#include "gc/Heap.h"
#include "gc/Nursery.h"
#include "gc/Statistics.h"
#include "gc/StoreBuffer.h"
#include "gc/Tracer.h"
#include "js/GCAnnotations.h"

namespace js {

class AutoLockGC;
class AutoLockHelperThreadState;
class VerifyPreTracer;

namespace gc {

typedef Vector<ZoneGroup*, 4, SystemAllocPolicy> ZoneGroupVector;
using BlackGrayEdgeVector = Vector<TenuredCell*, 0, SystemAllocPolicy>;

class AutoMaybeStartBackgroundAllocation;
class MarkingValidator;
class AutoTraceSession;
struct MovingTracer;

class ChunkPool
{
    Chunk* head_;
    size_t count_;

  public:
    ChunkPool() : head_(nullptr), count_(0) {}

    size_t count() const { return count_; }

    Chunk* head() { MOZ_ASSERT(head_); return head_; }
    Chunk* pop();
    void push(Chunk* chunk);
    Chunk* remove(Chunk* chunk);

#ifdef DEBUG
    bool contains(Chunk* chunk) const;
    bool verify() const;
#endif

    // Pool mutation does not invalidate an Iter unless the mutation
    // is of the Chunk currently being visited by the Iter.
    class Iter {
      public:
        explicit Iter(ChunkPool& pool) : current_(pool.head_) {}
        bool done() const { return !current_; }
        void next();
        Chunk* get() const { return current_; }
        operator Chunk*() const { return get(); }
        Chunk* operator->() const { return get(); }
      private:
        Chunk* current_;
    };
};

// Performs extra allocation off thread so that when memory is required on the
// active thread it will already be available and waiting.
class BackgroundAllocTask : public GCParallelTask
{
    // Guarded by the GC lock.
    GCLockData<ChunkPool&> chunkPool_;

    const bool enabled_;

  public:
    BackgroundAllocTask(JSRuntime* rt, ChunkPool& pool);
    bool enabled() const { return enabled_; }

  protected:
    void run() override;
};

// Search the provided Chunks for free arenas and decommit them.
class BackgroundDecommitTask : public GCParallelTask
{
  public:
    using ChunkVector = mozilla::Vector<Chunk*>;

    explicit BackgroundDecommitTask(JSRuntime *rt) : GCParallelTask(rt) {}
    void setChunksToScan(ChunkVector &chunks);

  protected:
    void run() override;

  private:
    ActiveThreadOrGCTaskData<ChunkVector> toDecommit;
};

/*
 * Encapsulates all of the GC tunables. These are effectively constant and
 * should only be modified by setParameter.
 */
class GCSchedulingTunables
{
    /*
     * Soft limit on the number of bytes we are allowed to allocate in the GC
     * heap. Attempts to allocate gcthings over this limit will return null and
     * subsequently invoke the standard OOM machinery, independent of available
     * physical memory.
     */
    UnprotectedData<size_t> gcMaxBytes_;

    /* Maximum nursery size for each zone group. */
    ActiveThreadData<size_t> gcMaxNurseryBytes_;

    /*
     * The base value used to compute zone->trigger.gcBytes(). When
     * usage.gcBytes() surpasses threshold.gcBytes() for a zone, the zone may
     * be scheduled for a GC, depending on the exact circumstances.
     */
    ActiveThreadOrGCTaskData<size_t> gcZoneAllocThresholdBase_;

    /* Fraction of threshold.gcBytes() which triggers an incremental GC. */
    UnprotectedData<double> zoneAllocThresholdFactor_;

    /*
     * Number of bytes to allocate between incremental slices in GCs triggered
     * by the zone allocation threshold.
     */
    UnprotectedData<size_t> zoneAllocDelayBytes_;

    /*
     * Totally disables |highFrequencyGC|, the HeapGrowthFactor, and other
     * tunables that make GC non-deterministic.
     */
    ActiveThreadData<bool> dynamicHeapGrowthEnabled_;

    /*
     * We enter high-frequency mode if we GC a twice within this many
     * microseconds. This value is stored directly in microseconds.
     */
    ActiveThreadData<uint64_t> highFrequencyThresholdUsec_;

    /*
     * When in the |highFrequencyGC| mode, these parameterize the per-zone
     * "HeapGrowthFactor" computation.
     */
    ActiveThreadData<uint64_t> highFrequencyLowLimitBytes_;
    ActiveThreadData<uint64_t> highFrequencyHighLimitBytes_;
    ActiveThreadData<double> highFrequencyHeapGrowthMax_;
    ActiveThreadData<double> highFrequencyHeapGrowthMin_;

    /*
     * When not in |highFrequencyGC| mode, this is the global (stored per-zone)
     * "HeapGrowthFactor".
     */
    ActiveThreadData<double> lowFrequencyHeapGrowth_;

    /*
     * Doubles the length of IGC slices when in the |highFrequencyGC| mode.
     */
    ActiveThreadData<bool> dynamicMarkSliceEnabled_;

    /*
     * Controls whether painting can trigger IGC slices.
     */
    ActiveThreadData<bool> refreshFrameSlicesEnabled_;

    /*
     * Controls the number of empty chunks reserved for future allocation.
     */
    UnprotectedData<uint32_t> minEmptyChunkCount_;
    UnprotectedData<uint32_t> maxEmptyChunkCount_;

  public:
    GCSchedulingTunables()
      : gcMaxBytes_(0),
        gcMaxNurseryBytes_(0),
        gcZoneAllocThresholdBase_(30 * 1024 * 1024),
        zoneAllocThresholdFactor_(0.9),
        zoneAllocDelayBytes_(1024 * 1024),
        dynamicHeapGrowthEnabled_(false),
        highFrequencyThresholdUsec_(1000 * 1000),
        highFrequencyLowLimitBytes_(100 * 1024 * 1024),
        highFrequencyHighLimitBytes_(500 * 1024 * 1024),
        highFrequencyHeapGrowthMax_(3.0),
        highFrequencyHeapGrowthMin_(1.5),
        lowFrequencyHeapGrowth_(1.5),
        dynamicMarkSliceEnabled_(false),
        refreshFrameSlicesEnabled_(true),
        minEmptyChunkCount_(1),
        maxEmptyChunkCount_(30)
    {}

    size_t gcMaxBytes() const { return gcMaxBytes_; }
    size_t gcMaxNurseryBytes() const { return gcMaxNurseryBytes_; }
    size_t gcZoneAllocThresholdBase() const { return gcZoneAllocThresholdBase_; }
    double zoneAllocThresholdFactor() const { return zoneAllocThresholdFactor_; }
    size_t zoneAllocDelayBytes() const { return zoneAllocDelayBytes_; }
    bool isDynamicHeapGrowthEnabled() const { return dynamicHeapGrowthEnabled_; }
    uint64_t highFrequencyThresholdUsec() const { return highFrequencyThresholdUsec_; }
    uint64_t highFrequencyLowLimitBytes() const { return highFrequencyLowLimitBytes_; }
    uint64_t highFrequencyHighLimitBytes() const { return highFrequencyHighLimitBytes_; }
    double highFrequencyHeapGrowthMax() const { return highFrequencyHeapGrowthMax_; }
    double highFrequencyHeapGrowthMin() const { return highFrequencyHeapGrowthMin_; }
    double lowFrequencyHeapGrowth() const { return lowFrequencyHeapGrowth_; }
    bool isDynamicMarkSliceEnabled() const { return dynamicMarkSliceEnabled_; }
    bool areRefreshFrameSlicesEnabled() const { return refreshFrameSlicesEnabled_; }
    unsigned minEmptyChunkCount(const AutoLockGC&) const { return minEmptyChunkCount_; }
    unsigned maxEmptyChunkCount() const { return maxEmptyChunkCount_; }

    MOZ_MUST_USE bool setParameter(JSGCParamKey key, uint32_t value, const AutoLockGC& lock);
};

/*
 * GC Scheduling Overview
 * ======================
 *
 * Scheduling GC's in SpiderMonkey/Firefox is tremendously complicated because
 * of the large number of subtle, cross-cutting, and widely dispersed factors
 * that must be taken into account. A summary of some of the more important
 * factors follows.
 *
 * Cost factors:
 *
 *   * GC too soon and we'll revisit an object graph almost identical to the
 *     one we just visited; since we are unlikely to find new garbage, the
 *     traversal will be largely overhead. We rely heavily on external factors
 *     to signal us that we are likely to find lots of garbage: e.g. "a tab
 *     just got closed".
 *
 *   * GC too late and we'll run out of memory to allocate (e.g. Out-Of-Memory,
 *     hereafter simply abbreviated to OOM). If this happens inside
 *     SpiderMonkey we may be able to recover, but most embedder allocations
 *     will simply crash on OOM, even if the GC has plenty of free memory it
 *     could surrender.
 *
 *   * Memory fragmentation: if we fill the process with GC allocations, a
 *     request for a large block of contiguous memory may fail because no
 *     contiguous block is free, despite having enough memory available to
 *     service the request.
 *
 *   * Management overhead: if our GC heap becomes large, we create extra
 *     overhead when managing the GC's structures, even if the allocations are
 *     mostly unused.
 *
 * Heap Management Factors:
 *
 *   * GC memory: The GC has its own allocator that it uses to make fixed size
 *     allocations for GC managed things. In cases where the GC thing requires
 *     larger or variable sized memory to implement itself, it is responsible
 *     for using the system heap.
 *
 *   * C Heap Memory: Rather than allowing for large or variable allocations,
 *     the SpiderMonkey GC allows GC things to hold pointers to C heap memory.
 *     It is the responsibility of the thing to free this memory with a custom
 *     finalizer (with the sole exception of NativeObject, which knows about
 *     slots and elements for performance reasons). C heap memory has different
 *     performance and overhead tradeoffs than GC internal memory, which need
 *     to be considered with scheduling a GC.
 *
 * Application Factors:
 *
 *   * Most applications allocate heavily at startup, then enter a processing
 *     stage where memory utilization remains roughly fixed with a slower
 *     allocation rate. This is not always the case, however, so while we may
 *     optimize for this pattern, we must be able to handle arbitrary
 *     allocation patterns.
 *
 * Other factors:
 *
 *   * Other memory: This is memory allocated outside the purview of the GC.
 *     Data mapped by the system for code libraries, data allocated by those
 *     libraries, data in the JSRuntime that is used to manage the engine,
 *     memory used by the embedding that is not attached to a GC thing, memory
 *     used by unrelated processes running on the hardware that use space we
 *     could otherwise use for allocation, etc. While we don't have to manage
 *     it, we do have to take it into account when scheduling since it affects
 *     when we will OOM.
 *
 *   * Physical Reality: All real machines have limits on the number of bits
 *     that they are physically able to store. While modern operating systems
 *     can generally make additional space available with swapping, at some
 *     point there are simply no more bits to allocate. There is also the
 *     factor of address space limitations, particularly on 32bit machines.
 *
 *   * Platform Factors: Each OS makes use of wildly different memory
 *     management techniques. These differences result in different performance
 *     tradeoffs, different fragmentation patterns, and different hard limits
 *     on the amount of physical and/or virtual memory that we can use before
 *     OOMing.
 *
 *
 * Reasons for scheduling GC
 * -------------------------
 *
 *  While code generally takes the above factors into account in only an ad-hoc
 *  fashion, the API forces the user to pick a "reason" for the GC. We have a
 *  bunch of JS::gcreason reasons in GCAPI.h. These fall into a few categories
 *  that generally coincide with one or more of the above factors.
 *
 *  Embedding reasons:
 *
 *   1) Do a GC now because the embedding knows something useful about the
 *      zone's memory retention state. These are gcreasons like LOAD_END,
 *      PAGE_HIDE, SET_NEW_DOCUMENT, DOM_UTILS. Mostly, Gecko uses these to
 *      indicate that a significant fraction of the scheduled zone's memory is
 *      probably reclaimable.
 *
 *   2) Do some known amount of GC work now because the embedding knows now is
 *      a good time to do a long, unblockable operation of a known duration.
 *      These are INTER_SLICE_GC and REFRESH_FRAME.
 *
 *  Correctness reasons:
 *
 *   3) Do a GC now because correctness depends on some GC property. For
 *      example, CC_WAITING is where the embedding requires the mark bits
 *      to be set correct. Also, EVICT_NURSERY where we need to work on the tenured
 *      heap.
 *
 *   4) Do a GC because we are shutting down: e.g. SHUTDOWN_CC or DESTROY_*.
 *
 *   5) Do a GC because a compartment was accessed between GC slices when we
 *      would have otherwise discarded it. We have to do a second GC to clean
 *      it up: e.g. COMPARTMENT_REVIVED.
 *
 *  Emergency Reasons:
 *
 *   6) Do an all-zones, non-incremental GC now because the embedding knows it
 *      cannot wait: e.g. MEM_PRESSURE.
 *
 *   7) OOM when fetching a new Chunk results in a LAST_DITCH GC.
 *
 *  Heap Size Limitation Reasons:
 *
 *   8) Do an incremental, zonal GC with reason MAYBEGC when we discover that
 *      the gc's allocated size is approaching the current trigger. This is
 *      called MAYBEGC because we make this check in the MaybeGC function.
 *      MaybeGC gets called at the top of the main event loop. Normally, it is
 *      expected that this callback will keep the heap size limited. It is
 *      relatively inexpensive, because it is invoked with no JS running and
 *      thus few stack roots to scan. For this reason, the GC's "trigger" bytes
 *      is less than the GC's "max" bytes as used by the trigger below.
 *
 *   9) Do an incremental, zonal GC with reason MAYBEGC when we go to allocate
 *      a new GC thing and find that the GC heap size has grown beyond the
 *      configured maximum (JSGC_MAX_BYTES). We trigger this GC by returning
 *      nullptr and then calling maybeGC at the top level of the allocator.
 *      This is then guaranteed to fail the "size greater than trigger" check
 *      above, since trigger is always less than max. After performing the GC,
 *      the allocator unconditionally returns nullptr to force an OOM exception
 *      is raised by the script.
 *
 *      Note that this differs from a LAST_DITCH GC where we actually run out
 *      of memory (i.e., a call to a system allocator fails) when trying to
 *      allocate. Unlike above, LAST_DITCH GC only happens when we are really
 *      out of memory, not just when we cross an arbitrary trigger; despite
 *      this, it may still return an allocation at the end and allow the script
 *      to continue, if the LAST_DITCH GC was able to free up enough memory.
 *
 *  10) Do a GC under reason ALLOC_TRIGGER when we are over the GC heap trigger
 *      limit, but in the allocator rather than in a random call to maybeGC.
 *      This occurs if we allocate too much before returning to the event loop
 *      and calling maybeGC; this is extremely common in benchmarks and
 *      long-running Worker computations. Note that this uses a wildly
 *      different mechanism from the above in that it sets the interrupt flag
 *      and does the GC at the next loop head, before the next alloc, or
 *      maybeGC. The reason for this is that this check is made after the
 *      allocation and we cannot GC with an uninitialized thing in the heap.
 *
 *  11) Do an incremental, zonal GC with reason TOO_MUCH_MALLOC when we have
 *      malloced more than JSGC_MAX_MALLOC_BYTES in a zone since the last GC.
 *
 *
 * Size Limitation Triggers Explanation
 * ------------------------------------
 *
 *  The GC internally is entirely unaware of the context of the execution of
 *  the mutator. It sees only:
 *
 *   A) Allocated size: this is the amount of memory currently requested by the
 *      mutator. This quantity is monotonically increasing: i.e. the allocation
 *      rate is always >= 0. It is also easy for the system to track.
 *
 *   B) Retained size: this is the amount of memory that the mutator can
 *      currently reach. Said another way, it is the size of the heap
 *      immediately after a GC (modulo background sweeping). This size is very
 *      costly to know exactly and also extremely hard to estimate with any
 *      fidelity.
 *
 *   For reference, a common allocated vs. retained graph might look like:
 *
 *       |                                  **         **
 *       |                       **       ** *       **
 *       |                     ** *     **   *     **
 *       |           *       **   *   **     *   **
 *       |          **     **     * **       * **
 *      s|         * *   **       ** +  +    **
 *      i|        *  *  *      +  +       +  +     +
 *      z|       *   * * +  +                   +     +  +
 *      e|      *    **+
 *       |     *     +
 *       |    *    +
 *       |   *   +
 *       |  *  +
 *       | * +
 *       |*+
 *       +--------------------------------------------------
 *                               time
 *                                           *** = allocated
 *                                           +++ = retained
 *
 *           Note that this is a bit of a simplification
 *           because in reality we track malloc and GC heap
 *           sizes separately and have a different level of
 *           granularity and accuracy on each heap.
 *
 *   This presents some obvious implications for Mark-and-Sweep collectors.
 *   Namely:
 *       -> t[marking] ~= size[retained]
 *       -> t[sweeping] ~= size[allocated] - size[retained]
 *
 *   In a non-incremental collector, maintaining low latency and high
 *   responsiveness requires that total GC times be as low as possible. Thus,
 *   in order to stay responsive when we did not have a fully incremental
 *   collector, our GC triggers were focused on minimizing collection time.
 *   Furthermore, since size[retained] is not under control of the GC, all the
 *   GC could do to control collection times was reduce sweep times by
 *   minimizing size[allocated], per the equation above.
 *
 *   The result of the above is GC triggers that focus on size[allocated] to
 *   the exclusion of other important factors and default heuristics that are
 *   not optimal for a fully incremental collector. On the other hand, this is
 *   not all bad: minimizing size[allocated] also minimizes the chance of OOM
 *   and sweeping remains one of the hardest areas to further incrementalize.
 *
 *      EAGER_ALLOC_TRIGGER
 *      -------------------
 *      Occurs when we return to the event loop and find our heap is getting
 *      largish, but before t[marking] OR t[sweeping] is too large for a
 *      responsive non-incremental GC. This is intended to be the common case
 *      in normal web applications: e.g. we just finished an event handler and
 *      the few objects we allocated when computing the new whatzitz have
 *      pushed us slightly over the limit. After this GC we rescale the new
 *      EAGER_ALLOC_TRIGGER trigger to 150% of size[retained] so that our
 *      non-incremental GC times will always be proportional to this size
 *      rather than being dominated by sweeping.
 *
 *      As a concession to mutators that allocate heavily during their startup
 *      phase, we have a highFrequencyGCMode that ups the growth rate to 300%
 *      of the current size[retained] so that we'll do fewer longer GCs at the
 *      end of the mutator startup rather than more, smaller GCs.
 *
 *          Assumptions:
 *            -> Responsiveness is proportional to t[marking] + t[sweeping].
 *            -> size[retained] is proportional only to GC allocations.
 *
 *      ALLOC_TRIGGER (non-incremental)
 *      -------------------------------
 *      If we do not return to the event loop before getting all the way to our
 *      gc trigger bytes then MAYBEGC will never fire. To avoid OOMing, we
 *      succeed the current allocation and set the script interrupt so that we
 *      will (hopefully) do a GC before we overflow our max and have to raise
 *      an OOM exception for the script.
 *
 *          Assumptions:
 *            -> Common web scripts will return to the event loop before using
 *               10% of the current gcTriggerBytes worth of GC memory.
 *
 *      ALLOC_TRIGGER (incremental)
 *      ---------------------------
 *      In practice the above trigger is rough: if a website is just on the
 *      cusp, sometimes it will trigger a non-incremental GC moments before
 *      returning to the event loop, where it could have done an incremental
 *      GC. Thus, we recently added an incremental version of the above with a
 *      substantially lower threshold, so that we have a soft limit here. If
 *      IGC can collect faster than the allocator generates garbage, even if
 *      the allocator does not return to the event loop frequently, we should
 *      not have to fall back to a non-incremental GC.
 *
 *      INCREMENTAL_TOO_SLOW
 *      --------------------
 *      Do a full, non-incremental GC if we overflow ALLOC_TRIGGER during an
 *      incremental GC. When in the middle of an incremental GC, we suppress
 *      our other triggers, so we need a way to backstop the IGC if the
 *      mutator allocates faster than the IGC can clean things up.
 *
 *      TOO_MUCH_MALLOC
 *      ---------------
 *      Performs a GC before size[allocated] - size[retained] gets too large
 *      for non-incremental sweeping to be fast in the case that we have
 *      significantly more malloc allocation than GC allocation. This is meant
 *      to complement MAYBEGC triggers. We track this by counting malloced
 *      bytes; the counter gets reset at every GC since we do not always have a
 *      size at the time we call free. Because of this, the malloc heuristic
 *      is, unfortunatly, not usefully able to augment our other GC heap
 *      triggers and is limited to this singular heuristic.
 *
 *          Assumptions:
 *            -> EITHER size[allocated_by_malloc] ~= size[allocated_by_GC]
 *                 OR   time[sweeping] ~= size[allocated_by_malloc]
 *            -> size[retained] @ t0 ~= size[retained] @ t1
 *               i.e. That the mutator is in steady-state operation.
 *
 *      LAST_DITCH_GC
 *      -------------
 *      Does a GC because we are out of memory.
 *
 *          Assumptions:
 *            -> size[retained] < size[available_memory]
 */
class GCSchedulingState
{
    /*
     * Influences how we schedule and run GC's in several subtle ways. The most
     * important factor is in how it controls the "HeapGrowthFactor". The
     * growth factor is a measure of how large (as a percentage of the last GC)
     * the heap is allowed to grow before we try to schedule another GC.
     */
    ActiveThreadData<bool> inHighFrequencyGCMode_;

  public:
    GCSchedulingState()
      : inHighFrequencyGCMode_(false)
    {}

    bool inHighFrequencyGCMode() const { return inHighFrequencyGCMode_; }

    void updateHighFrequencyMode(uint64_t lastGCTime, uint64_t currentTime,
                                 const GCSchedulingTunables& tunables) {
        inHighFrequencyGCMode_ =
            tunables.isDynamicHeapGrowthEnabled() && lastGCTime &&
            lastGCTime + tunables.highFrequencyThresholdUsec() > currentTime;
    }
};

template<typename F>
struct Callback {
    ActiveThreadData<F> op;
    ActiveThreadData<void*> data;

    Callback()
      : op(nullptr), data(nullptr)
    {}
    Callback(F op, void* data)
      : op(op), data(data)
    {}
};

template<typename F>
using CallbackVector = ActiveThreadData<Vector<Callback<F>, 4, SystemAllocPolicy>>;

template <typename T, typename Iter0, typename Iter1>
class ChainedIter
{
    Iter0 iter0_;
    Iter1 iter1_;

  public:
    ChainedIter(const Iter0& iter0, const Iter1& iter1)
      : iter0_(iter0), iter1_(iter1)
    {}

    bool done() const { return iter0_.done() && iter1_.done(); }
    void next() {
        MOZ_ASSERT(!done());
        if (!iter0_.done()) {
            iter0_.next();
        } else {
            MOZ_ASSERT(!iter1_.done());
            iter1_.next();
        }
    }
    T get() const {
        MOZ_ASSERT(!done());
        if (!iter0_.done())
            return iter0_.get();
        MOZ_ASSERT(!iter1_.done());
        return iter1_.get();
    }

    operator T() const { return get(); }
    T operator->() const { return get(); }
};

typedef HashMap<Value*, const char*, DefaultHasher<Value*>, SystemAllocPolicy> RootedValueMap;

using AllocKinds = mozilla::EnumSet<AllocKind>;

template <typename T>
class MemoryCounter
{
    // Bytes counter to measure memory pressure for GC scheduling. It runs
    // from maxBytes down to zero.
    mozilla::Atomic<ptrdiff_t, mozilla::ReleaseAcquire> bytes_;

    // GC trigger threshold for memory allocations.
    js::ActiveThreadData<size_t> maxBytes_;

    // Whether a GC has been triggered as a result of bytes falling below
    // zero.
    //
    // This should be a bool, but Atomic only supports 32-bit and pointer-sized
    // types.
    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> triggered_;

  public:
    MemoryCounter()
      : bytes_(0),
        maxBytes_(0),
        triggered_(false)
    { }

    void reset() {
        bytes_ = maxBytes_;
        triggered_ = false;
    }

    void setMax(size_t newMax) {
        // For compatibility treat any value that exceeds PTRDIFF_T_MAX to
        // mean that value.
        maxBytes_ = (ptrdiff_t(newMax) >= 0) ? newMax : size_t(-1) >> 1;
        reset();
    }

    bool update(T* owner, size_t bytes) {
        bytes_ -= ptrdiff_t(bytes);
        if (MOZ_UNLIKELY(isTooMuchMalloc())) {
            if (!triggered_)
                triggered_ = owner->triggerGCForTooMuchMalloc();
        }
        return triggered_;
    }

    ptrdiff_t bytes() const { return bytes_; }
    size_t maxBytes() const { return maxBytes_; }
    bool isTooMuchMalloc() const { return bytes_ <= 0; }
};

class GCRuntime
{
  public:
    explicit GCRuntime(JSRuntime* rt);
    MOZ_MUST_USE bool init(uint32_t maxbytes, uint32_t maxNurseryBytes);
    void finishRoots();
    void finish();

    inline bool hasZealMode(ZealMode mode);
    inline void clearZealMode(ZealMode mode);
    inline bool upcomingZealousGC();
    inline bool needZealousGC();

    MOZ_MUST_USE bool addRoot(Value* vp, const char* name);
    void removeRoot(Value* vp);
    void setMarkStackLimit(size_t limit, AutoLockGC& lock);

    MOZ_MUST_USE bool setParameter(JSGCParamKey key, uint32_t value, AutoLockGC& lock);
    uint32_t getParameter(JSGCParamKey key, const AutoLockGC& lock);

    MOZ_MUST_USE bool triggerGC(JS::gcreason::Reason reason);
    void maybeAllocTriggerZoneGC(Zone* zone, const AutoLockGC& lock);
    // The return value indicates if we were able to do the GC.
    bool triggerZoneGC(Zone* zone, JS::gcreason::Reason reason);
    void maybeGC(Zone* zone);
    // The return value indicates whether a major GC was performed.
    bool gcIfRequested();
    void gc(JSGCInvocationKind gckind, JS::gcreason::Reason reason);
    void startGC(JSGCInvocationKind gckind, JS::gcreason::Reason reason, int64_t millis = 0);
    void gcSlice(JS::gcreason::Reason reason, int64_t millis = 0);
    void finishGC(JS::gcreason::Reason reason);
    void abortGC();
    void startDebugGC(JSGCInvocationKind gckind, SliceBudget& budget);
    void debugGCSlice(SliceBudget& budget);

    bool canChangeActiveContext(JSContext* cx);

    void triggerFullGCForAtoms() {
        MOZ_ASSERT(fullGCForAtomsRequested_);
        fullGCForAtomsRequested_ = false;
        MOZ_RELEASE_ASSERT(triggerGC(JS::gcreason::ALLOC_TRIGGER));
    }

    void runDebugGC();
    inline void poke();

    enum TraceOrMarkRuntime {
        TraceRuntime,
        MarkRuntime
    };
    void traceRuntime(JSTracer* trc, AutoLockForExclusiveAccess& lock);
    void traceRuntimeForMinorGC(JSTracer* trc, AutoLockForExclusiveAccess& lock);

    void notifyDidPaint();
    void shrinkBuffers();
    void onOutOfMallocMemory();
    void onOutOfMallocMemory(const AutoLockGC& lock);

#ifdef JS_GC_ZEAL
    const void* addressOfZealModeBits() { return &zealModeBits; }
    void getZealBits(uint32_t* zealBits, uint32_t* frequency, uint32_t* nextScheduled);
    void setZeal(uint8_t zeal, uint32_t frequency);
    bool parseAndSetZeal(const char* str);
    void setNextScheduled(uint32_t count);
    void verifyPreBarriers();
    void maybeVerifyPreBarriers(bool always);
    bool selectForMarking(JSObject* object);
    void clearSelectedForMarking();
    void setDeterministic(bool enable);
#endif

    uint64_t nextCellUniqueId() {
        MOZ_ASSERT(nextCellUniqueId_ > 0);
        uint64_t uid = ++nextCellUniqueId_;
        return uid;
    }

#ifdef DEBUG
    bool shutdownCollectedEverything() const {
        return arenasEmptyAtShutdown;
    }
#endif

  public:
    // Internal public interface
    State state() const { return incrementalState; }
    bool isHeapCompacting() const { return state() == State::Compact; }
    bool isForegroundSweeping() const { return state() == State::Sweep; }
    bool isBackgroundSweeping() { return helperState.isBackgroundSweeping(); }
    void waitBackgroundSweepEnd() { helperState.waitBackgroundSweepEnd(); }
    void waitBackgroundSweepOrAllocEnd() {
        helperState.waitBackgroundSweepEnd();
        allocTask.cancel(GCParallelTask::CancelAndWait);
    }

#ifdef DEBUG
    bool onBackgroundThread() { return helperState.onBackgroundThread(); }
#endif // DEBUG

    void lockGC() {
        lock.lock();
    }

    void unlockGC() {
        lock.unlock();
    }

#ifdef DEBUG
    bool currentThreadHasLockedGC() const {
        return lock.ownedByCurrentThread();
    }
#endif // DEBUG

    void setAlwaysPreserveCode() { alwaysPreserveCode = true; }

    bool isIncrementalGCAllowed() const { return incrementalAllowed; }
    void disallowIncrementalGC() { incrementalAllowed = false; }

    bool isIncrementalGCEnabled() const { return mode == JSGC_MODE_INCREMENTAL && incrementalAllowed; }
    bool isIncrementalGCInProgress() const { return state() != State::NotActive; }

    bool isCompactingGCEnabled() const;

    void setGrayRootsTracer(JSTraceDataOp traceOp, void* data);
    MOZ_MUST_USE bool addBlackRootsTracer(JSTraceDataOp traceOp, void* data);
    void removeBlackRootsTracer(JSTraceDataOp traceOp, void* data);

    bool triggerGCForTooMuchMalloc() { return triggerGC(JS::gcreason::TOO_MUCH_MALLOC); }
    int32_t getMallocBytes() const { return mallocCounter.bytes(); }
    size_t maxMallocBytesAllocated() const { return mallocCounter.maxBytes(); }
    bool isTooMuchMalloc() const { return mallocCounter.isTooMuchMalloc(); }
    void resetMallocBytes() { mallocCounter.reset(); }
    void setMaxMallocBytes(size_t value);
    void updateMallocCounter(JS::Zone* zone, size_t nbytes);

    void setGCCallback(JSGCCallback callback, void* data);
    void callGCCallback(JSGCStatus status) const;
    void setObjectsTenuredCallback(JSObjectsTenuredCallback callback,
                                   void* data);
    void callObjectsTenuredCallback();
    MOZ_MUST_USE bool addFinalizeCallback(JSFinalizeCallback callback, void* data);
    void removeFinalizeCallback(JSFinalizeCallback func);
    MOZ_MUST_USE bool addWeakPointerZoneGroupCallback(JSWeakPointerZoneGroupCallback callback,
                                                      void* data);
    void removeWeakPointerZoneGroupCallback(JSWeakPointerZoneGroupCallback callback);
    MOZ_MUST_USE bool addWeakPointerCompartmentCallback(JSWeakPointerCompartmentCallback callback,
                                                        void* data);
    void removeWeakPointerCompartmentCallback(JSWeakPointerCompartmentCallback callback);
    JS::GCSliceCallback setSliceCallback(JS::GCSliceCallback callback);
    JS::GCNurseryCollectionCallback setNurseryCollectionCallback(
        JS::GCNurseryCollectionCallback callback);
    JS::DoCycleCollectionCallback setDoCycleCollectionCallback(JS::DoCycleCollectionCallback callback);
    void callDoCycleCollectionCallback(JSContext* cx);

    void setFullCompartmentChecks(bool enable);

    JS::Zone* getCurrentZoneGroup() { return currentZoneGroup; }
    void setFoundBlackGrayEdges(TenuredCell& target) {
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!foundBlackGrayEdges.ref().append(&target))
            oomUnsafe.crash("OOM|small: failed to insert into foundBlackGrayEdges");
    }

    uint64_t gcNumber() const { return number; }

    uint64_t minorGCCount() const { return minorGCNumber; }
    void incMinorGcNumber() { ++minorGCNumber; ++number; }

    uint64_t majorGCCount() const { return majorGCNumber; }
    void incMajorGcNumber() { ++majorGCNumber; ++number; }

    int64_t defaultSliceBudget() const { return defaultTimeBudget_; }

    bool isIncrementalGc() const { return isIncremental; }
    bool isFullGc() const { return isFull; }
    bool isCompactingGc() const { return isCompacting; }

    bool areGrayBitsValid() const { return grayBitsValid; }
    void setGrayBitsInvalid() { grayBitsValid = false; }

    bool majorGCRequested() const { return majorGCTriggerReason != JS::gcreason::NO_REASON; }

    bool fullGCForAtomsRequested() const { return fullGCForAtomsRequested_; }

    double computeHeapGrowthFactor(size_t lastBytes);
    size_t computeTriggerBytes(double growthFactor, size_t lastBytes);

    JSGCMode gcMode() const { return mode; }
    void setGCMode(JSGCMode m) {
        mode = m;
        marker.setGCMode(mode);
    }

    inline void updateOnFreeArenaAlloc(const ChunkInfo& info);
    inline void updateOnArenaFree(const ChunkInfo& info);

    ChunkPool& fullChunks(const AutoLockGC& lock) { return fullChunks_.ref(); }
    ChunkPool& availableChunks(const AutoLockGC& lock) { return availableChunks_.ref(); }
    ChunkPool& emptyChunks(const AutoLockGC& lock) { return emptyChunks_.ref(); }
    const ChunkPool& fullChunks(const AutoLockGC& lock) const { return fullChunks_.ref(); }
    const ChunkPool& availableChunks(const AutoLockGC& lock) const { return availableChunks_.ref(); }
    const ChunkPool& emptyChunks(const AutoLockGC& lock) const { return emptyChunks_.ref(); }
    typedef ChainedIter<Chunk*, ChunkPool::Iter, ChunkPool::Iter> NonEmptyChunksIter;
    NonEmptyChunksIter allNonEmptyChunks() {
        return NonEmptyChunksIter(ChunkPool::Iter(availableChunks_.ref()), ChunkPool::Iter(fullChunks_.ref()));
    }

    Chunk* getOrAllocChunk(const AutoLockGC& lock,
                           AutoMaybeStartBackgroundAllocation& maybeStartBGAlloc);
    void recycleChunk(Chunk* chunk, const AutoLockGC& lock);

#ifdef JS_GC_ZEAL
    void startVerifyPreBarriers();
    void endVerifyPreBarriers();
    void finishVerifier();
    bool isVerifyPreBarriersEnabled() const { return !!verifyPreData; }
#else
    bool isVerifyPreBarriersEnabled() const { return false; }
#endif

    // Free certain LifoAlloc blocks when it is safe to do so.
    void freeUnusedLifoBlocksAfterSweeping(LifoAlloc* lifo);
    void freeAllLifoBlocksAfterSweeping(LifoAlloc* lifo);

    // Public here for ReleaseArenaLists and FinalizeTypedArenas.
    void releaseArena(Arena* arena, const AutoLockGC& lock);

    void releaseHeldRelocatedArenas();
    void releaseHeldRelocatedArenasWithoutUnlocking(const AutoLockGC& lock);

    // Allocator
    template <AllowGC allowGC>
    MOZ_MUST_USE bool checkAllocatorState(JSContext* cx, AllocKind kind);
    template <AllowGC allowGC>
    JSObject* tryNewNurseryObject(JSContext* cx, size_t thingSize, size_t nDynamicSlots,
                                  const Class* clasp);
    template <AllowGC allowGC>
    static JSObject* tryNewTenuredObject(JSContext* cx, AllocKind kind, size_t thingSize,
                                         size_t nDynamicSlots);
    template <typename T, AllowGC allowGC>
    static T* tryNewTenuredThing(JSContext* cx, AllocKind kind, size_t thingSize);
    static TenuredCell* refillFreeListInGC(Zone* zone, AllocKind thingKind);

  private:
    enum IncrementalProgress
    {
        NotFinished = 0,
        Finished
    };

    enum IncrementalResult
    {
        Reset = 0,
        Ok
    };

    // For ArenaLists::allocateFromArena()
    friend class ArenaLists;
    Chunk* pickChunk(const AutoLockGC& lock,
                     AutoMaybeStartBackgroundAllocation& maybeStartBGAlloc);
    Arena* allocateArena(Chunk* chunk, Zone* zone, AllocKind kind,
                         ShouldCheckThresholds checkThresholds, const AutoLockGC& lock);
    void arenaAllocatedDuringGC(JS::Zone* zone, Arena* arena);

    // Allocator internals
    MOZ_MUST_USE bool gcIfNeededPerAllocation(JSContext* cx);
    template <typename T>
    static void checkIncrementalZoneState(JSContext* cx, T* t);
    static TenuredCell* refillFreeListFromAnyThread(JSContext* cx, AllocKind thingKind,
                                                    size_t thingSize);
    static TenuredCell* refillFreeListFromActiveCooperatingThread(JSContext* cx, AllocKind thingKind,
                                                                  size_t thingSize);
    static TenuredCell* refillFreeListFromHelperThread(JSContext* cx, AllocKind thingKind);

    /*
     * Return the list of chunks that can be released outside the GC lock.
     * Must be called either during the GC or with the GC lock taken.
     */
    friend class BackgroundDecommitTask;
    ChunkPool expireEmptyChunkPool(const AutoLockGC& lock);
    void freeEmptyChunks(JSRuntime* rt, const AutoLockGC& lock);
    void prepareToFreeChunk(ChunkInfo& info);

    friend class BackgroundAllocTask;
    friend class AutoMaybeStartBackgroundAllocation;
    bool wantBackgroundAllocation(const AutoLockGC& lock) const;
    void startBackgroundAllocTaskIfIdle();

    void requestMajorGC(JS::gcreason::Reason reason);
    SliceBudget defaultBudget(JS::gcreason::Reason reason, int64_t millis);
    IncrementalResult budgetIncrementalGC(bool nonincrementalByAPI, JS::gcreason::Reason reason,
                                          SliceBudget& budget, AutoLockForExclusiveAccess& lock);
    IncrementalResult resetIncrementalGC(AbortReason reason, AutoLockForExclusiveAccess& lock);

    // Assert if the system state is such that we should never
    // receive a request to do GC work.
    void checkCanCallAPI();

    // Check if the system state is such that GC has been supressed
    // or otherwise delayed.
    MOZ_MUST_USE bool checkIfGCAllowedInCurrentState(JS::gcreason::Reason reason);

    gcstats::ZoneGCStats scanZonesBeforeGC();
    void collect(bool nonincrementalByAPI, SliceBudget budget, JS::gcreason::Reason reason) JS_HAZ_GC_CALL;
    MOZ_MUST_USE IncrementalResult gcCycle(bool nonincrementalByAPI, SliceBudget& budget,
                                           JS::gcreason::Reason reason);
    bool shouldRepeatForDeadZone(JS::gcreason::Reason reason);
    void incrementalCollectSlice(SliceBudget& budget, JS::gcreason::Reason reason,
                                 AutoLockForExclusiveAccess& lock);

    void pushZealSelectedObjects();
    void purgeRuntime(AutoLockForExclusiveAccess& lock);
    MOZ_MUST_USE bool beginMarkPhase(JS::gcreason::Reason reason, AutoLockForExclusiveAccess& lock);
    bool shouldPreserveJITCode(JSCompartment* comp, int64_t currentTime,
                               JS::gcreason::Reason reason, bool canAllocateMoreCode);
    void traceRuntimeForMajorGC(JSTracer* trc, AutoLockForExclusiveAccess& lock);
    void traceRuntimeAtoms(JSTracer* trc, AutoLockForExclusiveAccess& lock);
    void traceRuntimeCommon(JSTracer* trc, TraceOrMarkRuntime traceOrMark,
                            AutoLockForExclusiveAccess& lock);
    void bufferGrayRoots();
    void maybeDoCycleCollection();
    void markCompartments();
    IncrementalProgress drainMarkStack(SliceBudget& sliceBudget, gcstats::Phase phase);
    template <class CompartmentIterT> void markWeakReferences(gcstats::Phase phase);
    void markWeakReferencesInCurrentGroup(gcstats::Phase phase);
    template <class ZoneIterT, class CompartmentIterT> void markGrayReferences(gcstats::Phase phase);
    void markBufferedGrayRoots(JS::Zone* zone);
    void markGrayReferencesInCurrentGroup(gcstats::Phase phase);
    void markAllWeakReferences(gcstats::Phase phase);
    void markAllGrayReferences(gcstats::Phase phase);

    void beginSweepPhase(bool lastGC, AutoLockForExclusiveAccess& lock);
    void findZoneGroups(AutoLockForExclusiveAccess& lock);
    MOZ_MUST_USE bool findZoneEdgesForWeakMaps();
    void getNextZoneGroup();
    void endMarkingZoneGroup();
    void beginSweepingZoneGroup(AutoLockForExclusiveAccess& lock);
    bool shouldReleaseObservedTypes();
    void endSweepingZoneGroup();
    IncrementalProgress sweepPhase(SliceBudget& sliceBudget, AutoLockForExclusiveAccess& lock);
    void endSweepPhase(bool lastGC, AutoLockForExclusiveAccess& lock);
    void sweepZones(FreeOp* fop, ZoneGroup* group, bool lastGC);
    void sweepZoneGroups(FreeOp* fop, bool destroyingRuntime);
    void decommitAllWithoutUnlocking(const AutoLockGC& lock);
    void startDecommit();
    void queueZonesForBackgroundSweep(ZoneList& zones);
    void sweepBackgroundThings(ZoneList& zones, LifoAlloc& freeBlocks);
    void assertBackgroundSweepingFinished();
    bool shouldCompact();
    void beginCompactPhase();
    IncrementalProgress compactPhase(JS::gcreason::Reason reason, SliceBudget& sliceBudget,
                                     AutoLockForExclusiveAccess& lock);
    void endCompactPhase(JS::gcreason::Reason reason);
    void sweepTypesAfterCompacting(Zone* zone);
    void sweepZoneAfterCompacting(Zone* zone);
    MOZ_MUST_USE bool relocateArenas(Zone* zone, JS::gcreason::Reason reason,
                                     Arena*& relocatedListOut, SliceBudget& sliceBudget);
    void updateTypeDescrObjects(MovingTracer* trc, Zone* zone);
    void updateCellPointers(MovingTracer* trc, Zone* zone, AllocKinds kinds, size_t bgTaskCount);
    void updateAllCellPointers(MovingTracer* trc, Zone* zone);
    void updateZonePointersToRelocatedCells(Zone* zone, AutoLockForExclusiveAccess& lock);
    void updateRuntimePointersToRelocatedCells(AutoLockForExclusiveAccess& lock);
    void protectAndHoldArenas(Arena* arenaList);
    void unprotectHeldRelocatedArenas();
    void releaseRelocatedArenas(Arena* arenaList);
    void releaseRelocatedArenasWithoutUnlocking(Arena* arenaList, const AutoLockGC& lock);
    void finishCollection(JS::gcreason::Reason reason);

    void computeNonIncrementalMarkingForValidation(AutoLockForExclusiveAccess& lock);
    void validateIncrementalMarking();
    void finishMarkingValidation();

#ifdef DEBUG
    void checkForCompartmentMismatches();
#endif

    void callFinalizeCallbacks(FreeOp* fop, JSFinalizeStatus status) const;
    void callWeakPointerZoneGroupCallbacks() const;
    void callWeakPointerCompartmentCallbacks(JSCompartment* comp) const;

  public:
    JSRuntime* const rt;

    /* Embedders can use this zone and group however they wish. */
    UnprotectedData<JS::Zone*> systemZone;
    UnprotectedData<ZoneGroup*> systemZoneGroup;

    // List of all zone groups (protected by the GC lock).
    ActiveThreadOrGCTaskData<ZoneGroupVector> groups;

    // The unique atoms zone, which has no zone group.
    WriteOnceData<Zone*> atomsZone;

  private:
    UnprotectedData<gcstats::Statistics> stats_;
  public:
    gcstats::Statistics& stats() { return stats_.ref(); }

    GCMarker marker;

    Vector<JS::GCCellPtr, 0, SystemAllocPolicy> unmarkGrayStack;

    /* Track heap usage for this runtime. */
    HeapUsage usage;

    /* GC scheduling state and parameters. */
    GCSchedulingTunables tunables;
    GCSchedulingState schedulingState;

    MemProfiler mMemProfiler;

    // State used for managing atom mark bitmaps in each zone. Protected by the
    // exclusive access lock.
    AtomMarkingRuntime atomMarking;

  private:
    // When empty, chunks reside in the emptyChunks pool and are re-used as
    // needed or eventually expired if not re-used. The emptyChunks pool gets
    // refilled from the background allocation task heuristically so that empty
    // chunks should always available for immediate allocation without syscalls.
    GCLockData<ChunkPool> emptyChunks_;

    // Chunks which have had some, but not all, of their arenas allocated live
    // in the available chunk lists. When all available arenas in a chunk have
    // been allocated, the chunk is removed from the available list and moved
    // to the fullChunks pool. During a GC, if all arenas are free, the chunk
    // is moved back to the emptyChunks pool and scheduled for eventual
    // release.
    UnprotectedData<ChunkPool> availableChunks_;

    // When all arenas in a chunk are used, it is moved to the fullChunks pool
    // so as to reduce the cost of operations on the available lists.
    UnprotectedData<ChunkPool> fullChunks_;

    ActiveThreadData<RootedValueMap> rootsHash;

    // An incrementing id used to assign unique ids to cells that require one.
    mozilla::Atomic<uint64_t, mozilla::ReleaseAcquire> nextCellUniqueId_;

    /*
     * Number of the committed arenas in all GC chunks including empty chunks.
     */
    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> numArenasFreeCommitted;
    ActiveThreadData<VerifyPreTracer*> verifyPreData;

  private:
    UnprotectedData<bool> chunkAllocationSinceLastGC;
    ActiveThreadData<int64_t> lastGCTime;

    ActiveThreadData<JSGCMode> mode;

    mozilla::Atomic<size_t, mozilla::ReleaseAcquire> numActiveZoneIters;

    /* During shutdown, the GC needs to clean up every possible object. */
    ActiveThreadData<bool> cleanUpEverything;

    // Gray marking must be done after all black marking is complete. However,
    // we do not have write barriers on XPConnect roots. Therefore, XPConnect
    // roots must be accumulated in the first slice of incremental GC. We
    // accumulate these roots in each zone's gcGrayRoots vector and then mark
    // them later, after black marking is complete for each compartment. This
    // accumulation can fail, but in that case we switch to non-incremental GC.
    enum class GrayBufferState {
        Unused,
        Okay,
        Failed
    };
    ActiveThreadData<GrayBufferState> grayBufferState;
    bool hasBufferedGrayRoots() const { return grayBufferState == GrayBufferState::Okay; }

    // Clear each zone's gray buffers, but do not change the current state.
    void resetBufferedGrayRoots() const;

    // Reset the gray buffering state to Unused.
    void clearBufferedGrayRoots() {
        grayBufferState = GrayBufferState::Unused;
        resetBufferedGrayRoots();
    }

    /*
     * The gray bits can become invalid if UnmarkGray overflows the stack. A
     * full GC will reset this bit, since it fills in all the gray bits.
     */
    UnprotectedData<bool> grayBitsValid;

    mozilla::Atomic<JS::gcreason::Reason, mozilla::Relaxed> majorGCTriggerReason;

  private:
    /* Perform full GC if rt->keepAtoms() becomes false. */
    ActiveThreadData<bool> fullGCForAtomsRequested_;

    /* Incremented at the start of every minor GC. */
    ActiveThreadData<uint64_t> minorGCNumber;

    /* Incremented at the start of every major GC. */
    ActiveThreadData<uint64_t> majorGCNumber;

    /* The major GC number at which to release observed type information. */
    ActiveThreadData<uint64_t> jitReleaseNumber;

    /* Incremented on every GC slice. */
    ActiveThreadData<uint64_t> number;

    /* The number at the time of the most recent GC's first slice. */
    ActiveThreadData<uint64_t> startNumber;

    /* Whether the currently running GC can finish in multiple slices. */
    ActiveThreadData<bool> isIncremental;

    /* Whether all zones are being collected in first GC slice. */
    ActiveThreadData<bool> isFull;

    /* Whether the heap will be compacted at the end of GC. */
    ActiveThreadData<bool> isCompacting;

    /* The invocation kind of the current GC, taken from the first slice. */
    ActiveThreadData<JSGCInvocationKind> invocationKind;

    /* The initial GC reason, taken from the first slice. */
    ActiveThreadData<JS::gcreason::Reason> initialReason;

    /*
     * The current incremental GC phase. This is also used internally in
     * non-incremental GC.
     */
    ActiveThreadOrGCTaskData<State> incrementalState;

    /* Indicates that the last incremental slice exhausted the mark stack. */
    ActiveThreadData<bool> lastMarkSlice;

    /* Whether any sweeping will take place in the separate GC helper thread. */
    ActiveThreadData<bool> sweepOnBackgroundThread;

    /* Whether observed type information is being released in the current GC. */
    ActiveThreadData<bool> releaseObservedTypes;

    /* Whether any black->gray edges were found during marking. */
    ActiveThreadData<BlackGrayEdgeVector> foundBlackGrayEdges;

    /* Singly linked list of zones to be swept in the background. */
    ActiveThreadOrGCTaskData<ZoneList> backgroundSweepZones;

    /*
     * Free LIFO blocks are transferred to this allocator before being freed on
     * the background GC thread after sweeping.
     */
    ActiveThreadOrGCTaskData<LifoAlloc> blocksToFreeAfterSweeping;

  private:
    /* Index of current zone group (for stats). */
    ActiveThreadData<unsigned> zoneGroupIndex;

    /*
     * Incremental sweep state.
     */
    ActiveThreadData<JS::Zone*> zoneGroups;
    ActiveThreadOrGCTaskData<JS::Zone*> currentZoneGroup;
    ActiveThreadData<bool> sweepingTypes;
    ActiveThreadData<unsigned> finalizePhase;
    ActiveThreadData<JS::Zone*> sweepZone;
    ActiveThreadData<AllocKind> sweepKind;
    ActiveThreadData<bool> abortSweepAfterCurrentGroup;

    /*
     * Concurrent sweep infrastructure.
     */
    void startTask(GCParallelTask& task, gcstats::Phase phase, AutoLockHelperThreadState& locked);
    void joinTask(GCParallelTask& task, gcstats::Phase phase, AutoLockHelperThreadState& locked);

    /*
     * List head of arenas allocated during the sweep phase.
     */
    ActiveThreadData<Arena*> arenasAllocatedDuringSweep;

    /*
     * Incremental compacting state.
     */
    ActiveThreadData<bool> startedCompacting;
    ActiveThreadData<ZoneList> zonesToMaybeCompact;
    ActiveThreadData<Arena*> relocatedArenasToRelease;

#ifdef JS_GC_ZEAL
    ActiveThreadData<MarkingValidator*> markingValidator;
#endif

    /*
     * Indicates that a GC slice has taken place in the middle of an animation
     * frame, rather than at the beginning. In this case, the next slice will be
     * delayed so that we don't get back-to-back slices.
     */
    ActiveThreadData<bool> interFrameGC;

    /* Default budget for incremental GC slice. See js/SliceBudget.h. */
    ActiveThreadData<int64_t> defaultTimeBudget_;

    /*
     * We disable incremental GC if we encounter a Class with a trace hook
     * that does not implement write barriers.
     */
    ActiveThreadData<bool> incrementalAllowed;

    /*
     * Whether compacting GC can is enabled globally.
     */
    ActiveThreadData<bool> compactingEnabled;

    ActiveThreadData<bool> poked;

    /*
     * These options control the zealousness of the GC. At every allocation,
     * nextScheduled is decremented. When it reaches zero we do a full GC.
     *
     * At this point, if zeal_ is one of the types that trigger periodic
     * collection, then nextScheduled is reset to the value of zealFrequency.
     * Otherwise, no additional GCs take place.
     *
     * You can control these values in several ways:
     *   - Set the JS_GC_ZEAL environment variable
     *   - Call gczeal() or schedulegc() from inside shell-executed JS code
     *     (see the help for details)
     *
     * If gcZeal_ == 1 then we perform GCs in select places (during MaybeGC and
     * whenever a GC poke happens). This option is mainly useful to embedders.
     *
     * We use zeal_ == 4 to enable write barrier verification. See the comment
     * in jsgc.cpp for more information about this.
     *
     * zeal_ values from 8 to 10 periodically run different types of
     * incremental GC.
     *
     * zeal_ value 14 performs periodic shrinking collections.
     */
#ifdef JS_GC_ZEAL
    ActiveThreadData<uint32_t> zealModeBits;
    ActiveThreadData<int> zealFrequency;
    ActiveThreadData<int> nextScheduled;
    ActiveThreadData<bool> deterministicOnly;
    ActiveThreadData<int> incrementalLimit;

    ActiveThreadData<Vector<JSObject*, 0, SystemAllocPolicy>> selectedForMarking;
#endif

    ActiveThreadData<bool> fullCompartmentChecks;

    Callback<JSGCCallback> gcCallback;
    Callback<JS::DoCycleCollectionCallback> gcDoCycleCollectionCallback;
    Callback<JSObjectsTenuredCallback> tenuredCallback;
    CallbackVector<JSFinalizeCallback> finalizeCallbacks;
    CallbackVector<JSWeakPointerZoneGroupCallback> updateWeakPointerZoneGroupCallbacks;
    CallbackVector<JSWeakPointerCompartmentCallback> updateWeakPointerCompartmentCallbacks;

    MemoryCounter<GCRuntime> mallocCounter;

    /*
     * The trace operations to trace embedding-specific GC roots. One is for
     * tracing through black roots and the other is for tracing through gray
     * roots. The black/gray distinction is only relevant to the cycle
     * collector.
     */
    CallbackVector<JSTraceDataOp> blackRootTracers;
    Callback<JSTraceDataOp> grayRootTracer;

    /* Always preserve JIT code during GCs, for testing. */
    ActiveThreadData<bool> alwaysPreserveCode;

#ifdef DEBUG
    ActiveThreadData<bool> arenasEmptyAtShutdown;
#endif

    /* Synchronize GC heap access among GC helper threads and active threads. */
    friend class js::AutoLockGC;
    js::Mutex lock;

    BackgroundAllocTask allocTask;
    BackgroundDecommitTask decommitTask;

    GCHelperState helperState;

    /*
     * During incremental sweeping, this field temporarily holds the arenas of
     * the current AllocKind being swept in order of increasing free space.
     */
    ActiveThreadData<SortedArenaList> incrementalSweepList;

  private:
    ActiveThreadData<Nursery> nursery_;
    ActiveThreadData<gc::StoreBuffer> storeBuffer_;
  public:
    Nursery& nursery() { return nursery_.ref(); }
    gc::StoreBuffer& storeBuffer() { return storeBuffer_.ref(); }

    // Free LIFO blocks are transferred to this allocator before being freed
    // after minor GC.
    ActiveThreadData<LifoAlloc> blocksToFreeAfterMinorGC;

    const void* addressOfNurseryPosition() {
        return nursery_.refNoCheck().addressOfPosition();
    }
    const void* addressOfNurseryCurrentEnd() {
        return nursery_.refNoCheck().addressOfCurrentEnd();
    }

    void minorGC(JS::gcreason::Reason reason,
                 gcstats::Phase phase = gcstats::PHASE_MINOR_GC) JS_HAZ_GC_CALL;
    void evictNursery(JS::gcreason::Reason reason = JS::gcreason::EVICT_NURSERY) {
        minorGC(reason, gcstats::PHASE_EVICT_NURSERY);
    }
    void freeAllLifoBlocksAfterMinorGC(LifoAlloc* lifo);

    friend class js::GCHelperState;
    friend class MarkingValidator;
    friend class AutoTraceSession;
    friend class AutoEnterIteration;
};

/* Prevent compartments and zones from being collected during iteration. */
class MOZ_RAII AutoEnterIteration {
    GCRuntime* gc;

  public:
    explicit AutoEnterIteration(GCRuntime* gc_) : gc(gc_) {
        ++gc->numActiveZoneIters;
    }

    ~AutoEnterIteration() {
        MOZ_ASSERT(gc->numActiveZoneIters);
        --gc->numActiveZoneIters;
    }
};

// After pulling a Chunk out of the empty chunks pool, we want to run the
// background allocator to refill it. The code that takes Chunks does so under
// the GC lock. We need to start the background allocation under the helper
// threads lock. To avoid lock inversion we have to delay the start until after
// we are outside the GC lock. This class handles that delay automatically.
class MOZ_RAII AutoMaybeStartBackgroundAllocation
{
    GCRuntime* gc;

  public:
    AutoMaybeStartBackgroundAllocation()
      : gc(nullptr)
    {}

    void tryToStartBackgroundAllocation(GCRuntime& gc) {
        this->gc = &gc;
    }

    ~AutoMaybeStartBackgroundAllocation() {
        if (gc)
            gc->startBackgroundAllocTaskIfIdle();
    }
};

#ifdef JS_GC_ZEAL

inline bool
GCRuntime::hasZealMode(ZealMode mode)
{
    static_assert(size_t(ZealMode::Limit) < sizeof(zealModeBits) * 8,
                  "Zeal modes must fit in zealModeBits");
    return zealModeBits & (1 << uint32_t(mode));
}

inline void
GCRuntime::clearZealMode(ZealMode mode)
{
    zealModeBits &= ~(1 << uint32_t(mode));
    MOZ_ASSERT(!hasZealMode(mode));
}

inline bool
GCRuntime::upcomingZealousGC() {
    return nextScheduled == 1;
}

inline bool
GCRuntime::needZealousGC() {
    if (nextScheduled > 0 && --nextScheduled == 0) {
        if (hasZealMode(ZealMode::Alloc) ||
            hasZealMode(ZealMode::GenerationalGC) ||
            hasZealMode(ZealMode::IncrementalRootsThenFinish) ||
            hasZealMode(ZealMode::IncrementalMarkAllThenFinish) ||
            hasZealMode(ZealMode::IncrementalMultipleSlices) ||
            hasZealMode(ZealMode::Compact))
        {
            nextScheduled = zealFrequency;
        }
        return true;
    }
    return false;
}
#else
inline bool GCRuntime::hasZealMode(ZealMode mode) { return false; }
inline void GCRuntime::clearZealMode(ZealMode mode) { }
inline bool GCRuntime::upcomingZealousGC() { return false; }
inline bool GCRuntime::needZealousGC() { return false; }
#endif

} /* namespace gc */

} /* namespace js */

#endif
