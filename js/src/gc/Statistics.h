/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Statistics_h
#define gc_Statistics_h

#include "mozilla/DebugOnly.h"
#include "mozilla/PodOperations.h"
#include "mozilla/UniquePtr.h"

#include "jsalloc.h"
#include "jspubtd.h"

#include "js/GCAPI.h"
#include "js/Vector.h"

struct JSCompartment;

namespace js {
namespace gcstats {

enum Phase {
    PHASE_GC_BEGIN,
    PHASE_WAIT_BACKGROUND_THREAD,
    PHASE_MARK_DISCARD_CODE,
    PHASE_PURGE,
    PHASE_MARK,
    PHASE_MARK_ROOTS,
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
    PHASE_DISCARD_ANALYSIS,
    PHASE_DISCARD_TI,
    PHASE_FREE_TI_ARENA,
    PHASE_SWEEP_TYPES,
    PHASE_SWEEP_OBJECT,
    PHASE_SWEEP_STRING,
    PHASE_SWEEP_SCRIPT,
    PHASE_SWEEP_SHAPE,
    PHASE_SWEEP_JITCODE,
    PHASE_FINALIZE_END,
    PHASE_DESTROY,
    PHASE_COMPACT,
    PHASE_COMPACT_MOVE,
    PHASE_COMPACT_UPDATE,
    PHASE_COMPACT_UPDATE_GRAY,
    PHASE_GC_END,

    PHASE_LIMIT
};

enum Stat {
    STAT_NEW_CHUNK,
    STAT_DESTROY_CHUNK,
    STAT_MINOR_GC,

    STAT_LIMIT
};

class StatisticsSerializer;

struct ZoneGCStats
{
    /* Number of zones collected in this GC. */
    int collectedZoneCount;

    /* Total number of zones in the Runtime at the start of this GC. */
    int zoneCount;

    /* Total number of comaprtments in all zones collected. */
    int collectedCompartmentCount;

    /* Total number of compartments in the Runtime at the start of this GC. */
    int compartmentCount;

    bool isCollectingAllZones() const { return collectedZoneCount == zoneCount; }

    ZoneGCStats()
      : collectedZoneCount(0), zoneCount(0), collectedCompartmentCount(0), compartmentCount(0)
    {}
};

struct Statistics
{
    explicit Statistics(JSRuntime *rt);
    ~Statistics();

    void beginPhase(Phase phase);
    void endPhase(Phase phase);

    void beginSlice(const ZoneGCStats &zoneStats, JS::gcreason::Reason reason);
    void endSlice();

    void reset(const char *reason) { slices.back().resetReason = reason; }
    void nonincremental(const char *reason) { nonincrementalReason = reason; }

    void count(Stat s) {
        JS_ASSERT(s < STAT_LIMIT);
        counts[s]++;
    }

    int64_t beginSCC();
    void endSCC(unsigned scc, int64_t start);

    char16_t *formatMessage();
    char16_t *formatJSON(uint64_t timestamp);
    UniqueChars formatDetailedMessage();

    JS::GCSliceCallback setSliceCallback(JS::GCSliceCallback callback);

    int64_t clearMaxGCPauseAccumulator();
    int64_t getMaxGCPauseSinceClear();

  private:
    JSRuntime *runtime;

    int64_t startupTime;

    FILE *fp;
    bool fullFormat;

    /*
     * GCs can't really nest, but a second GC can be triggered from within the
     * JSGC_END callback.
     */
    int gcDepth;

    ZoneGCStats zoneStats;

    const char *nonincrementalReason;

    struct SliceData {
        SliceData(JS::gcreason::Reason reason, int64_t start, size_t startFaults)
          : reason(reason), resetReason(nullptr), start(start), startFaults(startFaults)
        {
            mozilla::PodArrayZero(phaseTimes);
        }

        JS::gcreason::Reason reason;
        const char *resetReason;
        int64_t start, end;
        size_t startFaults, endFaults;
        int64_t phaseTimes[PHASE_LIMIT];

        int64_t duration() const { return end - start; }
    };

    Vector<SliceData, 8, SystemAllocPolicy> slices;

    /* Most recent time when the given phase started. */
    int64_t phaseStartTimes[PHASE_LIMIT];

    /* Total time in a given phase for this GC. */
    int64_t phaseTimes[PHASE_LIMIT];

    /* Total time in a given phase over all GCs. */
    int64_t phaseTotals[PHASE_LIMIT];

    /* Number of events of this type for this GC. */
    unsigned int counts[STAT_LIMIT];

    /* Allocated space before the GC started. */
    size_t preBytes;

    /* Records the maximum GC pause in an API-controlled interval (in us). */
    int64_t maxPauseInInterval;

#ifdef DEBUG
    /* Phases that are currently on stack. */
    static const size_t MAX_NESTING = 8;
    Phase phaseNesting[MAX_NESTING];
#endif
    mozilla::DebugOnly<size_t> phaseNestingDepth;

    /* Sweep times for SCCs of compartments. */
    Vector<int64_t, 0, SystemAllocPolicy> sccTimes;

    JS::GCSliceCallback sliceCallback;

    void beginGC();
    void endGC();

    void gcDuration(int64_t *total, int64_t *maxPause);
    void sccDurations(int64_t *total, int64_t *maxPause);
    void printStats();
    bool formatData(StatisticsSerializer &ss, uint64_t timestamp);

    UniqueChars formatDescription();
    UniqueChars formatSliceDescription(unsigned i, const SliceData &slice);
    UniqueChars formatTotals();
    UniqueChars formatPhaseTimes(int64_t *phaseTimes);

    double computeMMU(int64_t resolution);
};

struct AutoGCSlice
{
    AutoGCSlice(Statistics &stats, const ZoneGCStats &zoneStats, JS::gcreason::Reason reason
                MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : stats(stats)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        stats.beginSlice(zoneStats, reason);
    }
    ~AutoGCSlice() { stats.endSlice(); }

    Statistics &stats;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

struct AutoPhase
{
    AutoPhase(Statistics &stats, Phase phase
              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : stats(stats), phase(phase), enabled(true)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        stats.beginPhase(phase);
    }
    AutoPhase(Statistics &stats, bool condition, Phase phase
              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : stats(stats), phase(phase), enabled(condition)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        if (enabled)
            stats.beginPhase(phase);
    }
    ~AutoPhase() {
        if (enabled)
            stats.endPhase(phase);
    }

    Statistics &stats;
    Phase phase;
    bool enabled;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

struct AutoSCC
{
    AutoSCC(Statistics &stats, unsigned scc
            MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : stats(stats), scc(scc)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        start = stats.beginSCC();
    }
    ~AutoSCC() {
        stats.endSCC(scc, start);
    }

    Statistics &stats;
    unsigned scc;
    int64_t start;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

const char *ExplainReason(JS::gcreason::Reason reason);

} /* namespace gcstats */
} /* namespace js */

#endif /* gc_Statistics_h */
