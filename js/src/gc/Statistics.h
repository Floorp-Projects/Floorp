/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Statistics_h
#define gc_Statistics_h

#include "mozilla/EnumeratedArray.h"
#include "mozilla/IntegerRange.h"
#include "mozilla/PodOperations.h"

#include "jsalloc.h"
#include "jsgc.h"
#include "jspubtd.h"

#include "js/GCAPI.h"
#include "js/Vector.h"

namespace js {

class GCParallelTask;

namespace gcstats {

enum Phase : uint8_t {
    PHASE_MUTATOR,
    PHASE_GC_BEGIN,
    PHASE_WAIT_BACKGROUND_THREAD,
    PHASE_MARK_DISCARD_CODE,
    PHASE_RELAZIFY_FUNCTIONS,
    PHASE_PURGE,
    PHASE_MARK,
    PHASE_UNMARK,
    PHASE_MARK_DELAYED,
    PHASE_SWEEP,
    PHASE_SWEEP_MARK,
    PHASE_SWEEP_MARK_TYPES,
    PHASE_SWEEP_MARK_INCOMING_BLACK,
    PHASE_SWEEP_MARK_WEAK,
    PHASE_SWEEP_MARK_INCOMING_GRAY,
    PHASE_SWEEP_MARK_GRAY,
    PHASE_SWEEP_MARK_GRAY_WEAK,
    PHASE_FINALIZE_START,
    PHASE_WEAK_ZONEGROUP_CALLBACK,
    PHASE_WEAK_COMPARTMENT_CALLBACK,
    PHASE_SWEEP_ATOMS,
    PHASE_SWEEP_SYMBOL_REGISTRY,
    PHASE_SWEEP_COMPARTMENTS,
    PHASE_SWEEP_DISCARD_CODE,
    PHASE_SWEEP_INNER_VIEWS,
    PHASE_SWEEP_CC_WRAPPER,
    PHASE_SWEEP_BASE_SHAPE,
    PHASE_SWEEP_INITIAL_SHAPE,
    PHASE_SWEEP_TYPE_OBJECT,
    PHASE_SWEEP_BREAKPOINT,
    PHASE_SWEEP_REGEXP,
    PHASE_SWEEP_MISC,
    PHASE_SWEEP_TYPES,
    PHASE_SWEEP_TYPES_BEGIN,
    PHASE_SWEEP_TYPES_END,
    PHASE_SWEEP_OBJECT,
    PHASE_SWEEP_STRING,
    PHASE_SWEEP_SCRIPT,
    PHASE_SWEEP_SCOPE,
    PHASE_SWEEP_SHAPE,
    PHASE_SWEEP_JITCODE,
    PHASE_FINALIZE_END,
    PHASE_DESTROY,
    PHASE_COMPACT,
    PHASE_COMPACT_MOVE,
    PHASE_COMPACT_UPDATE,
    PHASE_COMPACT_UPDATE_CELLS,
    PHASE_GC_END,
    PHASE_MINOR_GC,
    PHASE_EVICT_NURSERY,
    PHASE_TRACE_HEAP,
    PHASE_BARRIER,
    PHASE_UNMARK_GRAY,
    PHASE_MARK_ROOTS,
    PHASE_BUFFER_GRAY_ROOTS,
    PHASE_MARK_CCWS,
    PHASE_MARK_ROOTERS,
    PHASE_MARK_RUNTIME_DATA,
    PHASE_MARK_EMBEDDING,
    PHASE_MARK_COMPARTMENTS,

    PHASE_LIMIT,
    PHASE_NONE = PHASE_LIMIT,
    PHASE_EXPLICIT_SUSPENSION = PHASE_LIMIT,
    PHASE_IMPLICIT_SUSPENSION,
    PHASE_MULTI_PARENTS
};

enum Stat {
    STAT_NEW_CHUNK,
    STAT_DESTROY_CHUNK,
    STAT_MINOR_GC,

    // Number of times a 'put' into a storebuffer overflowed, triggering a
    // compaction
    STAT_STOREBUFFER_OVERFLOW,

    // Number of arenas relocated by compacting GC.
    STAT_ARENA_RELOCATED,

    STAT_LIMIT
};

struct ZoneGCStats
{
    /* Number of zones collected in this GC. */
    int collectedZoneCount;

    /* Total number of zones in the Runtime at the start of this GC. */
    int zoneCount;

    /* Number of zones swept in this GC. */
    int sweptZoneCount;

    /* Total number of compartments in all zones collected. */
    int collectedCompartmentCount;

    /* Total number of compartments in the Runtime at the start of this GC. */
    int compartmentCount;

    /* Total number of compartments swept by this GC. */
    int sweptCompartmentCount;

    bool isCollectingAllZones() const { return collectedZoneCount == zoneCount; }

    ZoneGCStats()
      : collectedZoneCount(0), zoneCount(0), sweptZoneCount(0),
        collectedCompartmentCount(0), compartmentCount(0), sweptCompartmentCount(0)
    {}
};

#define FOR_EACH_GC_PROFILE_TIME(_)                                           \
    _(BeginCallback, "beginCB",  PHASE_GC_BEGIN)                              \
    _(WaitBgThread,  "waitBG",   PHASE_WAIT_BACKGROUND_THREAD)                \
    _(DiscardCode,   "discard",  PHASE_MARK_DISCARD_CODE)                     \
    _(RelazifyFunc,  "relazify", PHASE_RELAZIFY_FUNCTIONS)                    \
    _(Purge,         "purge",    PHASE_PURGE)                                 \
    _(Mark,          "mark",     PHASE_MARK)                                  \
    _(Sweep,         "sweep",    PHASE_SWEEP)                                 \
    _(Compact,       "compact",  PHASE_COMPACT)                               \
    _(EndCallback,   "endCB",    PHASE_GC_END)                                \
    _(Barriers,      "barriers", PHASE_BARRIER)

/*
 * Struct for collecting timing statistics on a "phase tree". The tree is
 * specified as a limited DAG, but the timings are collected for the whole tree
 * that you would get by expanding out the DAG by duplicating subtrees rooted
 * at nodes with multiple parents.
 *
 * During execution, a child phase can be activated multiple times, and the
 * total time will be accumulated. (So for example, you can start and end
 * PHASE_MARK_ROOTS multiple times before completing the parent phase.)
 *
 * Incremental GC is represented by recording separate timing results for each
 * slice within the overall GC.
 */
struct Statistics
{
    /*
     * Phases are allowed to have multiple parents, though any path from root
     * to leaf is allowed at most one multi-parented phase. We keep a full set
     * of timings for each of the multi-parented phases, to be able to record
     * all the timings in the expanded tree induced by our dag.
     *
     * Note that this wastes quite a bit of space, since we have a whole
     * separate array of timing data containing all the phases. We could be
     * more clever and keep an array of pointers biased by the offset of the
     * multi-parented phase, and thereby preserve the simple
     * timings[slot][PHASE_*] indexing. But the complexity doesn't seem worth
     * the few hundred bytes of savings. If we want to extend things to full
     * DAGs, this decision should be reconsidered.
     */
    static const size_t MaxMultiparentPhases = 6;
    static const size_t NumTimingArrays = MaxMultiparentPhases + 1;

    /* Create a convenient type for referring to tables of phase times. */
    using PhaseTimeTable = int64_t[NumTimingArrays][PHASE_LIMIT];

    static MOZ_MUST_USE bool initialize();

    explicit Statistics(JSRuntime* rt);
    ~Statistics();

    void beginPhase(Phase phase);
    void endPhase(Phase phase);
    void endParallelPhase(Phase phase, const GCParallelTask* task);

    // Occasionally, we may be in the middle of something that is tracked by
    // this class, and we need to do something unusual (eg evict the nursery)
    // that doesn't normally nest within the current phase. Suspend the
    // currently tracked phase stack, at which time the caller is free to do
    // other tracked operations.
    //
    // This also happens internally with PHASE_GC_BEGIN and other "non-GC"
    // phases. While in these phases, any beginPhase will automatically suspend
    // the non-GC phase, until that inner stack is complete, at which time it
    // will automatically resume the non-GC phase. Explicit suspensions do not
    // get auto-resumed.
    void suspendPhases(Phase suspension = PHASE_EXPLICIT_SUSPENSION);

    // Resume a suspended stack of phases.
    void resumePhases();

    void beginSlice(const ZoneGCStats& zoneStats, JSGCInvocationKind gckind,
                    SliceBudget budget, JS::gcreason::Reason reason);
    void endSlice();

    MOZ_MUST_USE bool startTimingMutator();
    MOZ_MUST_USE bool stopTimingMutator(double& mutator_ms, double& gc_ms);

    // Note when we sweep a zone or compartment.
    void sweptZone() { ++zoneStats.sweptZoneCount; }
    void sweptCompartment() { ++zoneStats.sweptCompartmentCount; }

    void reset(const char* reason) {
        if (!aborted)
            slices.back().resetReason = reason;
    }

    void nonincremental(const char* reason) { nonincrementalReason_ = reason; }

    const char* nonincrementalReason() const { return nonincrementalReason_; }

    void count(Stat s) {
        MOZ_ASSERT(s < STAT_LIMIT);
        counts[s]++;
    }

    void beginNurseryCollection(JS::gcreason::Reason reason);
    void endNurseryCollection(JS::gcreason::Reason reason);

    int64_t beginSCC();
    void endSCC(unsigned scc, int64_t start);

    UniqueChars formatCompactSliceMessage() const;
    UniqueChars formatCompactSummaryMessage() const;
    UniqueChars formatJsonMessage(uint64_t timestamp);
    UniqueChars formatDetailedMessage();

    JS::GCSliceCallback setSliceCallback(JS::GCSliceCallback callback);
    JS::GCNurseryCollectionCallback setNurseryCollectionCallback(
        JS::GCNurseryCollectionCallback callback);

    int64_t clearMaxGCPauseAccumulator();
    int64_t getMaxGCPauseSinceClear();

    // Return the current phase, suppressing the synthetic PHASE_MUTATOR phase.
    Phase currentPhase() {
        if (phaseNestingDepth == 0)
            return PHASE_NONE;
        if (phaseNestingDepth == 1)
            return phaseNesting[0] == PHASE_MUTATOR ? PHASE_NONE : phaseNesting[0];
        return phaseNesting[phaseNestingDepth - 1];
    }

    static const size_t MAX_NESTING = 20;

    struct SliceData {
        SliceData(SliceBudget budget, JS::gcreason::Reason reason, int64_t start,
                  double startTimestamp, size_t startFaults, gc::State initialState)
          : budget(budget), reason(reason),
            initialState(initialState),
            finalState(gc::State::NotActive),
            resetReason(nullptr),
            start(start), startTimestamp(startTimestamp),
            startFaults(startFaults)
        {
            for (auto i : mozilla::MakeRange(NumTimingArrays))
                mozilla::PodArrayZero(phaseTimes[i]);
        }

        SliceBudget budget;
        JS::gcreason::Reason reason;
        gc::State initialState, finalState;
        const char* resetReason;
        int64_t start, end;
        double startTimestamp, endTimestamp;
        size_t startFaults, endFaults;
        PhaseTimeTable phaseTimes;

        int64_t duration() const { return end - start; }
    };

    typedef Vector<SliceData, 8, SystemAllocPolicy> SliceDataVector;
    typedef SliceDataVector::ConstRange SliceRange;

    SliceRange sliceRange() const { return slices.all(); }
    size_t slicesLength() const { return slices.length(); }

    /* Print total profile times on shutdown. */
    void printTotalProfileTimes();

  private:
    JSRuntime* runtime;

    int64_t startupTime;

    /* File pointer used for MOZ_GCTIMER output. */
    FILE* fp;

    /*
     * GCs can't really nest, but a second GC can be triggered from within the
     * JSGC_END callback.
     */
    int gcDepth;

    ZoneGCStats zoneStats;

    JSGCInvocationKind gckind;

    const char* nonincrementalReason_;

    SliceDataVector slices;

    /* Most recent time when the given phase started. */
    int64_t phaseStartTimes[PHASE_LIMIT];

    /* Bookkeeping for GC timings when timingMutator is true */
    int64_t timedGCStart;
    int64_t timedGCTime;

    /* Total time in a given phase for this GC. */
    PhaseTimeTable phaseTimes;

    /* Total time in a given phase over all GCs. */
    PhaseTimeTable phaseTotals;

    /* Number of events of this type for this GC. */
    unsigned int counts[STAT_LIMIT];

    /* Allocated space before the GC started. */
    size_t preBytes;

    /* Records the maximum GC pause in an API-controlled interval (in us). */
    mutable int64_t maxPauseInInterval;

    /* Phases that are currently on stack. */
    Phase phaseNesting[MAX_NESTING];
    size_t phaseNestingDepth;
    size_t activeDagSlot;

    /*
     * Certain phases can interrupt the phase stack, eg callback phases. When
     * this happens, we move the suspended phases over to a sepearate list,
     * terminated by a dummy PHASE_SUSPENSION phase (so that we can nest
     * suspensions by suspending multiple stacks with a PHASE_SUSPENSION in
     * between).
     */
    Phase suspendedPhases[MAX_NESTING * 3];
    size_t suspended;

    /* Sweep times for SCCs of compartments. */
    Vector<int64_t, 0, SystemAllocPolicy> sccTimes;

    JS::GCSliceCallback sliceCallback;
    JS::GCNurseryCollectionCallback nurseryCollectionCallback;

    /*
     * True if we saw an OOM while allocating slices. The statistics for this
     * GC will be invalid.
     */
    bool aborted;

    /* Profiling data. */

    enum class ProfileKey
    {
        Total,
#define DEFINE_TIME_KEY(name, text, phase)                                    \
        name,
FOR_EACH_GC_PROFILE_TIME(DEFINE_TIME_KEY)
#undef DEFINE_TIME_KEY
        KeyCount
    };

    using ProfileTimes = mozilla::EnumeratedArray<ProfileKey, ProfileKey::KeyCount, int64_t>;

    int64_t profileThreshold_;
    bool enableProfiling_;
    ProfileTimes totalTimes_;
    uint64_t sliceCount_;

    void beginGC(JSGCInvocationKind kind);
    void endGC();

    void recordPhaseEnd(Phase phase);

    void gcDuration(int64_t* total, int64_t* maxPause) const;
    void sccDurations(int64_t* total, int64_t* maxPause);
    void printStats();

    UniqueChars formatCompactSlicePhaseTimes(const PhaseTimeTable phaseTimes) const;

    UniqueChars formatDetailedDescription();
    UniqueChars formatDetailedSliceDescription(unsigned i, const SliceData& slice);
    UniqueChars formatDetailedPhaseTimes(const PhaseTimeTable phaseTimes);
    UniqueChars formatDetailedTotals();

    UniqueChars formatJsonDescription(uint64_t timestamp);
    UniqueChars formatJsonSliceDescription(unsigned i, const SliceData& slice);
    UniqueChars formatJsonPhaseTimes(const PhaseTimeTable phaseTimes);

    double computeMMU(int64_t resolution) const;

    void printSliceProfile();
    static void printProfileHeader();
    static void printProfileTimes(const ProfileTimes& times);
};

struct MOZ_RAII AutoGCSlice
{
    AutoGCSlice(Statistics& stats, const ZoneGCStats& zoneStats, JSGCInvocationKind gckind,
                SliceBudget budget, JS::gcreason::Reason reason)
      : stats(stats)
    {
        stats.beginSlice(zoneStats, gckind, budget, reason);
    }
    ~AutoGCSlice() { stats.endSlice(); }

    Statistics& stats;
};

struct MOZ_RAII AutoPhase
{
    AutoPhase(Statistics& stats, Phase phase)
      : stats(stats), task(nullptr), phase(phase), enabled(true)
    {
        stats.beginPhase(phase);
    }

    AutoPhase(Statistics& stats, bool condition, Phase phase)
      : stats(stats), task(nullptr), phase(phase), enabled(condition)
    {
        if (enabled)
            stats.beginPhase(phase);
    }

    AutoPhase(Statistics& stats, const GCParallelTask& task, Phase phase)
      : stats(stats), task(&task), phase(phase), enabled(true)
    {
        if (enabled)
            stats.beginPhase(phase);
    }

    ~AutoPhase() {
        if (enabled) {
            if (task)
                stats.endParallelPhase(phase, task);
            else
                stats.endPhase(phase);
        }
    }

    Statistics& stats;
    const GCParallelTask* task;
    Phase phase;
    bool enabled;
};

struct MOZ_RAII AutoSCC
{
    AutoSCC(Statistics& stats, unsigned scc)
      : stats(stats), scc(scc)
    {
        start = stats.beginSCC();
    }
    ~AutoSCC() {
        stats.endSCC(scc, start);
    }

    Statistics& stats;
    unsigned scc;
    int64_t start;
};

const char* ExplainInvocationKind(JSGCInvocationKind gckind);

} /* namespace gcstats */
} /* namespace js */

#endif /* gc_Statistics_h */
