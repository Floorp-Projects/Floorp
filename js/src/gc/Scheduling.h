/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * [SMDOC] GC Scheduling
 *
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
 *  bunch of JS::GCReason reasons in GCAPI.h. These fall into a few categories
 *  that generally coincide with one or more of the above factors.
 *
 *  Embedding reasons:
 *
 *   1) Do a GC now because the embedding knows something useful about the
 *      zone's memory retention state. These are GCReasons like LOAD_END,
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
 *      to be set correct. Also, EVICT_NURSERY where we need to work on the
 *      tenured heap.
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
 *  11) Do an incremental, zonal GC with reason TOO_MUCH_MALLOC when the total
 * amount of malloced memory is greater than the malloc trigger limit for the
 * zone.
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
 *               10% of the current triggerBytes worth of GC memory.
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
 *      is, unfortunately, not usefully able to augment our other GC heap
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

#ifndef gc_Scheduling_h
#define gc_Scheduling_h

#include "mozilla/Atomics.h"
#include "mozilla/DebugOnly.h"

#include "gc/GCEnum.h"
#include "js/AllocPolicy.h"
#include "js/GCAPI.h"
#include "js/HashTable.h"
#include "js/HeapAPI.h"
#include "js/SliceBudget.h"
#include "threading/ProtectedData.h"

namespace js {

class AutoLockGC;
class ZoneAllocPolicy;

namespace gc {

struct Cell;

/*
 * Default settings for tuning the GC.  Some of these can be set at runtime,
 * This list is not complete, some tuning parameters are not listed here.
 *
 * If you change the values here, please also consider changing them in
 * modules/libpref/init/all.js where they are duplicated for the Firefox
 * preferences.
 */
namespace TuningDefaults {

/* JSGC_ALLOCATION_THRESHOLD */
static const size_t GCZoneAllocThresholdBase = 27 * 1024 * 1024;

/*
 * JSGC_MIN_NURSERY_BYTES
 *
 * With some testing (Bug 1532838) we increased this to 256K from 192K
 * which improves performance.  We should try to reduce this for background
 * tabs.
 */
static const size_t GCMinNurseryBytes = 256 * 1024;

/* JSGC_NON_INCREMENTAL_FACTOR */
static const float NonIncrementalFactor = 1.12f;

/* JSGC_ZONE_ALLOC_DELAY_KB */
static const size_t ZoneAllocDelayBytes = 1024 * 1024;

/* JSGC_DYNAMIC_HEAP_GROWTH */
static const bool DynamicHeapGrowthEnabled = false;

/* JSGC_HIGH_FREQUENCY_TIME_LIMIT */
static const auto HighFrequencyThreshold = 1;  // in seconds

/* JSGC_HIGH_FREQUENCY_LOW_LIMIT */
static const size_t HighFrequencyLowLimitBytes = 100 * 1024 * 1024;

/* JSGC_HIGH_FREQUENCY_HIGH_LIMIT */
static const size_t HighFrequencyHighLimitBytes = 500 * 1024 * 1024;

/* JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX */
static const float HighFrequencyHeapGrowthMax = 3.0f;

/* JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN */
static const float HighFrequencyHeapGrowthMin = 1.5f;

/* JSGC_LOW_FREQUENCY_HEAP_GROWTH */
static const float LowFrequencyHeapGrowth = 1.5f;

/* JSGC_DYNAMIC_MARK_SLICE */
static const bool DynamicMarkSliceEnabled = false;

/* JSGC_MIN_EMPTY_CHUNK_COUNT */
static const uint32_t MinEmptyChunkCount = 1;

/* JSGC_MAX_EMPTY_CHUNK_COUNT */
static const uint32_t MaxEmptyChunkCount = 30;

/* JSGC_SLICE_TIME_BUDGET_MS */
static const int64_t DefaultTimeBudgetMS = SliceBudget::UnlimitedTimeBudget;

/* JSGC_MODE */
static const JSGCMode Mode = JSGC_MODE_ZONE_INCREMENTAL;

/* JSGC_COMPACTING_ENABLED */
static const bool CompactingEnabled = true;

/* JSGC_INCREMENTAL_WEAKMAP_ENABLED */
static const bool IncrementalWeakMapMarkingEnabled = false;

/* JSGC_NURSERY_FREE_THRESHOLD_FOR_IDLE_COLLECTION */
static const uint32_t NurseryFreeThresholdForIdleCollection = ChunkSize / 4;

/* JSGC_NURSERY_FREE_THRESHOLD_FOR_IDLE_COLLECTION_PERCENT */
static const float NurseryFreeThresholdForIdleCollectionFraction = 0.25f;

/* JSGC_PRETENURE_THRESHOLD */
static const float PretenureThreshold = 0.6f;

/* JSGC_PRETENURE_GROUP_THRESHOLD */
static const float PretenureGroupThreshold = 3000;

/* JSGC_MIN_LAST_DITCH_GC_PERIOD */
static const auto MinLastDitchGCPeriod = 60;  // in seconds

/* JSGC_MALLOC_THRESHOLD_BASE */
static const size_t MallocThresholdBase = 38 * 1024 * 1024;

/* JSGC_MALLOC_GROWTH_FACTOR */
static const float MallocGrowthFactor = 1.5f;

}  // namespace TuningDefaults

/*
 * Encapsulates all of the GC tunables. These are effectively constant and
 * should only be modified by setParameter.
 */
class GCSchedulingTunables {
  /*
   * JSGC_MAX_BYTES
   *
   * Maximum nominal heap before last ditch GC.
   */
  UnprotectedData<size_t> gcMaxBytes_;

  /*
   * JSGC_MIN_NURSERY_BYTES
   * JSGC_MAX_NURSERY_BYTES
   *
   * Minimum and maximum nursery size for each runtime.
   */
  MainThreadData<size_t> gcMinNurseryBytes_;
  MainThreadData<size_t> gcMaxNurseryBytes_;

  /*
   * JSGC_ALLOCATION_THRESHOLD
   *
   * The base value used to compute zone->threshold.bytes(). When
   * gcHeapSize.bytes() exceeds threshold.bytes() for a zone, the zone may be
   * scheduled for a GC, depending on the exact circumstances.
   */
  MainThreadOrGCTaskData<size_t> gcZoneAllocThresholdBase_;

  /*
   * JSGC_NON_INCREMENTAL_FACTOR
   *
   * Multiple of threshold.bytes() which triggers a non-incremental GC.
   */
  UnprotectedData<float> nonIncrementalFactor_;

  /*
   * Number of bytes to allocate between incremental slices in GCs triggered by
   * the zone allocation threshold.
   *
   * This value does not have a JSGCParamKey parameter yet.
   */
  UnprotectedData<size_t> zoneAllocDelayBytes_;

  /*
   * JSGC_DYNAMIC_HEAP_GROWTH
   *
   * Totally disables |highFrequencyGC|, the HeapGrowthFactor, and other
   * tunables that make GC non-deterministic.
   */
  MainThreadOrGCTaskData<bool> dynamicHeapGrowthEnabled_;

  /*
   * JSGC_HIGH_FREQUENCY_TIME_LIMIT
   *
   * We enter high-frequency mode if we GC a twice within this many
   * microseconds.
   */
  MainThreadOrGCTaskData<mozilla::TimeDuration> highFrequencyThreshold_;

  /*
   * JSGC_HIGH_FREQUENCY_LOW_LIMIT
   * JSGC_HIGH_FREQUENCY_HIGH_LIMIT
   * JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX
   * JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN
   *
   * When in the |highFrequencyGC| mode, these parameterize the per-zone
   * "HeapGrowthFactor" computation.
   */
  MainThreadOrGCTaskData<size_t> highFrequencyLowLimitBytes_;
  MainThreadOrGCTaskData<size_t> highFrequencyHighLimitBytes_;
  MainThreadOrGCTaskData<float> highFrequencyHeapGrowthMax_;
  MainThreadOrGCTaskData<float> highFrequencyHeapGrowthMin_;

  /*
   * JSGC_LOW_FREQUENCY_HEAP_GROWTH
   *
   * When not in |highFrequencyGC| mode, this is the global (stored per-zone)
   * "HeapGrowthFactor".
   */
  MainThreadOrGCTaskData<float> lowFrequencyHeapGrowth_;

  /*
   * JSGC_DYNAMIC_MARK_SLICE
   *
   * Doubles the length of IGC slices when in the |highFrequencyGC| mode.
   */
  MainThreadData<bool> dynamicMarkSliceEnabled_;

  /*
   * JSGC_MIN_EMPTY_CHUNK_COUNT
   * JSGC_MAX_EMPTY_CHUNK_COUNT
   *
   * Controls the number of empty chunks reserved for future allocation.
   */
  UnprotectedData<uint32_t> minEmptyChunkCount_;
  UnprotectedData<uint32_t> maxEmptyChunkCount_;

  /*
   * JSGC_NURSERY_FREE_THRESHOLD_FOR_IDLE_COLLECTION
   * JSGC_NURSERY_FREE_THRESHOLD_FOR_IDLE_COLLECTION_FRACTION
   *
   * Attempt to run a minor GC in the idle time if the free space falls
   * below this threshold. The absolute threshold is used when the nursery is
   * large and the percentage when it is small.  See Nursery::shouldCollect()
   */
  UnprotectedData<uint32_t> nurseryFreeThresholdForIdleCollection_;
  UnprotectedData<float> nurseryFreeThresholdForIdleCollectionFraction_;

  /*
   * JSGC_PRETENURE_THRESHOLD
   *
   * Fraction of objects tenured to trigger pretenuring (between 0 and 1). If
   * this fraction is met, the GC proceeds to calculate which objects will be
   * tenured. If this is 1.0f (actually if it is not < 1.0f) then pretenuring
   * is disabled.
   */
  UnprotectedData<float> pretenureThreshold_;

  /*
   * JSGC_PRETENURE_GROUP_THRESHOLD
   *
   * During a single nursery collection, if this many objects from the same
   * object group are tenured, then that group will be pretenured.
   */
  UnprotectedData<uint32_t> pretenureGroupThreshold_;

  /*
   * JSGC_MIN_LAST_DITCH_GC_PERIOD
   *
   * Last ditch GC is skipped if allocation failure occurs less than this many
   * seconds from the previous one.
   */
  MainThreadData<mozilla::TimeDuration> minLastDitchGCPeriod_;

  /*
   * JSGC_MALLOC_THRESHOLD_BASE
   *
   * The base value used to compute the GC trigger for malloc allocated memory.
   */
  MainThreadOrGCTaskData<size_t> mallocThresholdBase_;

  /*
   * JSGC_MALLOC_GROWTH_FACTOR
   *
   * Malloc memory growth factor.
   */
  MainThreadOrGCTaskData<float> mallocGrowthFactor_;

 public:
  GCSchedulingTunables();

  size_t gcMaxBytes() const { return gcMaxBytes_; }
  size_t gcMinNurseryBytes() const { return gcMinNurseryBytes_; }
  size_t gcMaxNurseryBytes() const { return gcMaxNurseryBytes_; }
  size_t gcZoneAllocThresholdBase() const { return gcZoneAllocThresholdBase_; }
  double nonIncrementalFactor() const { return nonIncrementalFactor_; }
  size_t zoneAllocDelayBytes() const { return zoneAllocDelayBytes_; }
  bool isDynamicHeapGrowthEnabled() const { return dynamicHeapGrowthEnabled_; }
  const mozilla::TimeDuration& highFrequencyThreshold() const {
    return highFrequencyThreshold_;
  }
  size_t highFrequencyLowLimitBytes() const {
    return highFrequencyLowLimitBytes_;
  }
  size_t highFrequencyHighLimitBytes() const {
    return highFrequencyHighLimitBytes_;
  }
  double highFrequencyHeapGrowthMax() const {
    return highFrequencyHeapGrowthMax_;
  }
  double highFrequencyHeapGrowthMin() const {
    return highFrequencyHeapGrowthMin_;
  }
  double lowFrequencyHeapGrowth() const { return lowFrequencyHeapGrowth_; }
  bool isDynamicMarkSliceEnabled() const { return dynamicMarkSliceEnabled_; }
  unsigned minEmptyChunkCount(const AutoLockGC&) const {
    return minEmptyChunkCount_;
  }
  unsigned maxEmptyChunkCount() const { return maxEmptyChunkCount_; }
  uint32_t nurseryFreeThresholdForIdleCollection() const {
    return nurseryFreeThresholdForIdleCollection_;
  }
  float nurseryFreeThresholdForIdleCollectionFraction() const {
    return nurseryFreeThresholdForIdleCollectionFraction_;
  }

  bool attemptPretenuring() const { return pretenureThreshold_ < 1.0f; }
  float pretenureThreshold() const { return pretenureThreshold_; }
  uint32_t pretenureGroupThreshold() const { return pretenureGroupThreshold_; }

  mozilla::TimeDuration minLastDitchGCPeriod() const {
    return minLastDitchGCPeriod_;
  }

  size_t mallocThresholdBase() const { return mallocThresholdBase_; }
  float mallocGrowthFactor() const { return mallocGrowthFactor_; }

  MOZ_MUST_USE bool setParameter(JSGCParamKey key, uint32_t value,
                                 const AutoLockGC& lock);
  void resetParameter(JSGCParamKey key, const AutoLockGC& lock);

 private:
  void setHighFrequencyLowLimit(size_t value);
  void setHighFrequencyHighLimit(size_t value);
  void setHighFrequencyHeapGrowthMin(float value);
  void setHighFrequencyHeapGrowthMax(float value);
  void setLowFrequencyHeapGrowth(float value);
  void setMinEmptyChunkCount(uint32_t value);
  void setMaxEmptyChunkCount(uint32_t value);
};

class GCSchedulingState {
  /*
   * Influences how we schedule and run GC's in several subtle ways. The most
   * important factor is in how it controls the "HeapGrowthFactor". The
   * growth factor is a measure of how large (as a percentage of the last GC)
   * the heap is allowed to grow before we try to schedule another GC.
   */
  MainThreadOrGCTaskData<bool> inHighFrequencyGCMode_;

 public:
  /*
   * Influences the GC thresholds for the atoms zone to discourage collection of
   * this zone during page load.
   */
  MainThreadOrGCTaskData<bool> inPageLoad;

  GCSchedulingState() : inHighFrequencyGCMode_(false) {}

  bool inHighFrequencyGCMode() const { return inHighFrequencyGCMode_; }

  void updateHighFrequencyMode(const mozilla::TimeStamp& lastGCTime,
                               const mozilla::TimeStamp& currentTime,
                               const GCSchedulingTunables& tunables) {
    inHighFrequencyGCMode_ =
        tunables.isDynamicHeapGrowthEnabled() && !lastGCTime.IsNull() &&
        lastGCTime + tunables.highFrequencyThreshold() > currentTime;
  }
};

enum class TriggerKind { None, Incremental, NonIncremental };

struct TriggerResult {
  TriggerKind kind;
  size_t usedBytes;
  size_t thresholdBytes;
};

using AtomicByteCount = mozilla::Atomic<size_t, mozilla::ReleaseAcquire>;

/*
 * Tracks the size of allocated data. This is used for both GC and malloc data.
 * It automatically maintains the memory usage relationship between parent and
 * child instances, i.e. between those in a GCRuntime and its Zones.
 */
class HeapSize {
  /*
   * An instance that contains our parent's heap usage, or null if this is the
   * top-level usage container.
   */
  HeapSize* const parent_;

  /*
   * The number of bytes in use. For GC heaps this is approximate to the nearest
   * ArenaSize. It is atomic because it is updated by both the active and GC
   * helper threads.
   */
  AtomicByteCount bytes_;

  /*
   * The number of bytes retained after the last collection. This is updated
   * dynamically during incremental GC. It does not include allocations that
   * happen during a GC.
   */
  AtomicByteCount retainedBytes_;

 public:
  explicit HeapSize(HeapSize* parent) : parent_(parent), bytes_(0) {}

  size_t bytes() const { return bytes_; }
  size_t retainedBytes() const { return retainedBytes_; }

  void updateOnGCStart() { retainedBytes_ = size_t(bytes_); }

  void addGCArena() { addBytes(ArenaSize); }
  void removeGCArena() {
    MOZ_ASSERT(retainedBytes_ >= ArenaSize);
    removeBytes(ArenaSize, true /* only sweeping removes arenas */);
  }

  void addBytes(size_t nbytes) {
    mozilla::DebugOnly<size_t> initialBytes(bytes_);
    MOZ_ASSERT(initialBytes + nbytes > initialBytes);
    bytes_ += nbytes;
    if (parent_) {
      parent_->addBytes(nbytes);
    }
  }
  void removeBytes(size_t nbytes, bool wasSwept) {
    if (wasSwept) {
      // TODO: We would like to assert that retainedBytes_ >= nbytes is here but
      // we can't do that yet, so clamp the result to zero.
      retainedBytes_ = nbytes <= retainedBytes_ ? retainedBytes_ - nbytes : 0;
    }
    MOZ_ASSERT(bytes_ >= nbytes);
    bytes_ -= nbytes;
    if (parent_) {
      parent_->removeBytes(nbytes, wasSwept);
    }
  }

  /* Pair to adoptArenas. Adopts the attendant usage statistics. */
  void adopt(HeapSize& source) {
    // Skip retainedBytes_: we never adopt zones that are currently being
    // collected.
    bytes_ += source.bytes_;
    source.retainedBytes_ = 0;
    source.bytes_ = 0;
  }
};

// A heap size threshold used to trigger GC. This is an abstract base class for
// GC heap and malloc thresholds defined below.
class HeapThreshold {
 protected:
  HeapThreshold() = default;

  // GC trigger threshold.
  AtomicByteCount bytes_;

 public:
  size_t bytes() const { return bytes_; }
  size_t nonIncrementalTriggerBytes(GCSchedulingTunables& tunables) const {
    return bytes_ * tunables.nonIncrementalFactor();
  }
  float eagerAllocTrigger(bool highFrequencyGC) const;
};

// A heap threshold that is based on a multiple of the retained size after the
// last collection adjusted based on collection frequency and retained
// size. This is used to determine when to do a zone GC based on GC heap size.
class GCHeapThreshold : public HeapThreshold {
 public:
  void updateAfterGC(size_t lastBytes, JSGCInvocationKind gckind,
                     const GCSchedulingTunables& tunables,
                     const GCSchedulingState& state, bool isAtomsZone,
                     const AutoLockGC& lock);

 private:
  static float computeZoneHeapGrowthFactorForHeapSize(
      size_t lastBytes, const GCSchedulingTunables& tunables,
      const GCSchedulingState& state);
  static size_t computeZoneTriggerBytes(float growthFactor, size_t lastBytes,
                                        JSGCInvocationKind gckind,
                                        const GCSchedulingTunables& tunables,
                                        const AutoLockGC& lock);
};

// A heap threshold that is calculated as a constant multiple of the retained
// size after the last collection. This is used to determines when to do a zone
// GC based on malloc data.
class MallocHeapThreshold : public HeapThreshold {
 public:
  void updateAfterGC(size_t lastBytes, size_t baseBytes, float growthFactor,
                     const AutoLockGC& lock);

 private:
  static size_t computeZoneTriggerBytes(float growthFactor, size_t lastBytes,
                                        size_t baseBytes,
                                        const AutoLockGC& lock);
};

// A fixed threshold that's used to determine when we need to do a zone GC based
// on allocated JIT code.
class JitHeapThreshold : public HeapThreshold {
 public:
  explicit JitHeapThreshold(size_t bytes) { bytes_ = bytes; }
};

struct SharedMemoryUse {
  explicit SharedMemoryUse(MemoryUse use) : count(0), nbytes(0) {
#ifdef DEBUG
    this->use = use;
#endif
  }

  size_t count;
  size_t nbytes;
#ifdef DEBUG
  MemoryUse use;
#endif
};

// A map which tracks shared memory uses (shared in the sense that an allocation
// can be referenced by more than one GC thing in a zone). This allows us to
// only account for the memory once.
using SharedMemoryMap =
    HashMap<void*, SharedMemoryUse, DefaultHasher<void*>, SystemAllocPolicy>;

#ifdef DEBUG

// Counts memory associated with GC things in a zone.
//
// This records details of the cell (or non-cell pointer) the memory allocation
// is associated with to check the correctness of the information provided. This
// is not present in opt builds.
class MemoryTracker {
 public:
  MemoryTracker();
  void fixupAfterMovingGC();
  void checkEmptyOnDestroy();

  void adopt(MemoryTracker& other);

  // Track memory by associated GC thing pointer.
  void trackGCMemory(Cell* cell, size_t nbytes, MemoryUse use);
  void untrackGCMemory(Cell* cell, size_t nbytes, MemoryUse use);
  void swapGCMemory(Cell* a, Cell* b, MemoryUse use);

  // Track memory by associated non-GC thing pointer.
  void registerNonGCMemory(void* ptr, MemoryUse use);
  void unregisterNonGCMemory(void* ptr, MemoryUse use);
  void moveNonGCMemory(void* dst, void* src, MemoryUse use);
  void incNonGCMemory(void* ptr, size_t nbytes, MemoryUse use);
  void decNonGCMemory(void* ptr, size_t nbytes, MemoryUse use);

 private:
  template <typename Ptr>
  struct Key {
    Key(Ptr* ptr, MemoryUse use);
    Ptr* ptr() const;
    MemoryUse use() const;

   private:
#  ifdef JS_64BIT
    // Pack this into a single word on 64 bit platforms.
    uintptr_t ptr_ : 56;
    uintptr_t use_ : 8;
#  else
    uintptr_t ptr_ : 32;
    uintptr_t use_ : 8;
#  endif
  };

  template <typename Ptr>
  struct Hasher {
    using KeyT = Key<Ptr>;
    using Lookup = KeyT;
    static HashNumber hash(const Lookup& l);
    static bool match(const KeyT& key, const Lookup& l);
    static void rekey(KeyT& k, const KeyT& newKey);
  };

  template <typename Ptr>
  using Map = HashMap<Key<Ptr>, size_t, Hasher<Ptr>, SystemAllocPolicy>;
  using GCMap = Map<Cell>;
  using NonGCMap = Map<void>;

  static bool isGCMemoryUse(MemoryUse use);
  static bool isNonGCMemoryUse(MemoryUse use);
  static bool allowMultipleAssociations(MemoryUse use);

  size_t getAndRemoveEntry(const Key<Cell>& key, LockGuard<Mutex>& lock);

  Mutex mutex;

  // Map containing the allocated size associated with (cell, use) pairs.
  GCMap gcMap;

  // Map containing the allocated size associated (non-cell pointer, use) pairs.
  NonGCMap nonGCMap;
};

#endif  // DEBUG

}  // namespace gc
}  // namespace js

#endif  // gc_Scheduling_h
