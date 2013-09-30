/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/Statistics.h"

#include "mozilla/PodOperations.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>

#include "jscrashreport.h"
#include "jsprf.h"
#include "jsutil.h"
#include "prmjtime.h"

#include "gc/Memory.h"
#include "vm/Runtime.h"

using namespace js;
using namespace js::gcstats;

using mozilla::PodArrayZero;

/* Except for the first and last, slices of less than 42ms are not reported. */
static const int64_t SLICE_MIN_REPORT_TIME = 42 * PRMJ_USEC_PER_MSEC;

class gcstats::StatisticsSerializer
{
    typedef Vector<char, 128, SystemAllocPolicy> CharBuffer;
    CharBuffer buf_;
    bool asJSON_;
    bool needComma_;
    bool oom_;

    static const int MaxFieldValueLength = 128;

  public:
    enum Mode {
        AsJSON = true,
        AsText = false
    };

    StatisticsSerializer(Mode asJSON)
      : buf_(), asJSON_(asJSON), needComma_(false), oom_(false)
    {}

    bool isJSON() { return asJSON_; }

    bool isOOM() { return oom_; }

    void endLine() {
        if (!asJSON_) {
            p("\n");
            needComma_ = false;
        }
    }

    void extra(const char *str) {
        if (!asJSON_) {
            needComma_ = false;
            p(str);
        }
    }

    void appendString(const char *name, const char *value) {
        put(name, value, "", true);
    }

    void appendNumber(const char *name, const char *vfmt, const char *units, ...) {
        va_list va;
        va_start(va, units);
        append(name, vfmt, va, units);
        va_end(va);
    }

    void appendDecimal(const char *name, const char *units, double d) {
        if (d < 0)
            d = 0;
        if (asJSON_)
            appendNumber(name, "%d.%d", units, (int)d, (int)(d * 10.) % 10);
        else
            appendNumber(name, "%.1f", units, d);
    }

    void appendIfNonzeroMS(const char *name, double v) {
        if (asJSON_ || v >= 0.1)
            appendDecimal(name, "ms", v);
    }

    void beginObject(const char *name) {
        if (needComma_)
            pJSON(", ");
        if (asJSON_ && name) {
            putKey(name);
            pJSON(": ");
        }
        pJSON("{");
        needComma_ = false;
    }

    void endObject() {
        needComma_ = false;
        pJSON("}");
        needComma_ = true;
    }

    void beginArray(const char *name) {
        if (needComma_)
            pJSON(", ");
        if (asJSON_)
            putKey(name);
        pJSON(": [");
        needComma_ = false;
    }

    void endArray() {
        needComma_ = false;
        pJSON("]");
        needComma_ = true;
    }

    jschar *finishJSString() {
        char *buf = finishCString();
        if (!buf)
            return nullptr;

        size_t nchars = strlen(buf);
        jschar *out = js_pod_malloc<jschar>(nchars + 1);
        if (!out) {
            oom_ = true;
            js_free(buf);
            return nullptr;
        }

        InflateStringToBuffer(buf, nchars, out);
        js_free(buf);

        out[nchars] = 0;
        return out;
    }

    char *finishCString() {
        if (oom_)
            return nullptr;

        buf_.append('\0');

        char *buf = buf_.extractRawBuffer();
        if (!buf)
            oom_ = true;

        return buf;
    }

  private:
    void append(const char *name, const char *vfmt,
                va_list va, const char *units)
    {
        char val[MaxFieldValueLength];
        JS_vsnprintf(val, MaxFieldValueLength, vfmt, va);
        put(name, val, units, false);
    }

    void p(const char *cstr) {
        if (oom_)
            return;

        if (!buf_.append(cstr, strlen(cstr)))
            oom_ = true;
    }

    void p(const char c) {
        if (oom_)
            return;

        if (!buf_.append(c))
            oom_ = true;
    }

    void pJSON(const char *str) {
        if (asJSON_)
            p(str);
    }

    void put(const char *name, const char *val, const char *units, bool valueIsQuoted) {
        if (needComma_)
            p(", ");
        needComma_ = true;

        putKey(name);
        p(": ");
        if (valueIsQuoted)
            putQuoted(val);
        else
            p(val);
        if (!asJSON_)
            p(units);
    }

    void putQuoted(const char *str) {
        pJSON("\"");
        p(str);
        pJSON("\"");
    }

    void putKey(const char *str) {
        if (!asJSON_) {
            p(str);
            return;
        }

        p("\"");
        const char *c = str;
        while (*c) {
            if (*c == ' ' || *c == '\t')
                p('_');
            else if (isupper(*c))
                p(tolower(*c));
            else if (*c == '+')
                p("added_");
            else if (*c == '-')
                p("removed_");
            else if (*c != '(' && *c != ')')
                p(*c);
            c++;
        }
        p("\"");
    }
};

/*
 * If this fails, then you can either delete this assertion and allow all
 * larger-numbered reasons to pile up in the last telemetry bucket, or switch
 * to GC_REASON_3 and bump the max value.
 */
JS_STATIC_ASSERT(JS::gcreason::NUM_TELEMETRY_REASONS >= JS::gcreason::NUM_REASONS);

static const char *
ExplainReason(JS::gcreason::Reason reason)
{
    switch (reason) {
#define SWITCH_REASON(name)                     \
        case JS::gcreason::name:                    \
          return #name;
        GCREASONS(SWITCH_REASON)

        default:
          MOZ_ASSUME_UNREACHABLE("bad GC reason");
#undef SWITCH_REASON
    }
}

static double
t(int64_t t)
{
    return double(t) / PRMJ_USEC_PER_MSEC;
}

struct PhaseInfo
{
    Phase index;
    const char *name;
    Phase parent;
};

static const Phase PHASE_NO_PARENT = PHASE_LIMIT;

static const PhaseInfo phases[] = {
    { PHASE_GC_BEGIN, "Begin Callback", PHASE_NO_PARENT },
    { PHASE_WAIT_BACKGROUND_THREAD, "Wait Background Thread", PHASE_NO_PARENT },
    { PHASE_MARK_DISCARD_CODE, "Mark Discard Code", PHASE_NO_PARENT },
    { PHASE_PURGE, "Purge", PHASE_NO_PARENT },
    { PHASE_MARK, "Mark", PHASE_NO_PARENT },
    { PHASE_MARK_ROOTS, "Mark Roots", PHASE_MARK },
    { PHASE_MARK_TYPES, "Mark Types", PHASE_MARK_ROOTS },
    { PHASE_MARK_DELAYED, "Mark Delayed", PHASE_MARK },
    { PHASE_SWEEP, "Sweep", PHASE_NO_PARENT },
    { PHASE_SWEEP_MARK, "Mark During Sweeping", PHASE_SWEEP },
    { PHASE_SWEEP_MARK_TYPES, "Mark Types During Sweeping", PHASE_SWEEP_MARK },
    { PHASE_SWEEP_MARK_DELAYED, "Mark Delayed During Sweeping", PHASE_SWEEP_MARK },
    { PHASE_SWEEP_MARK_INCOMING_BLACK, "Mark Incoming Black Pointers", PHASE_SWEEP_MARK },
    { PHASE_SWEEP_MARK_WEAK, "Mark Weak", PHASE_SWEEP_MARK },
    { PHASE_SWEEP_MARK_INCOMING_GRAY, "Mark Incoming Gray Pointers", PHASE_SWEEP_MARK },
    { PHASE_SWEEP_MARK_GRAY, "Mark Gray", PHASE_SWEEP_MARK },
    { PHASE_SWEEP_MARK_GRAY_WEAK, "Mark Gray and Weak", PHASE_SWEEP_MARK },
    { PHASE_FINALIZE_START, "Finalize Start Callback", PHASE_SWEEP },
    { PHASE_SWEEP_ATOMS, "Sweep Atoms", PHASE_SWEEP },
    { PHASE_SWEEP_COMPARTMENTS, "Sweep Compartments", PHASE_SWEEP },
    { PHASE_SWEEP_DISCARD_CODE, "Sweep Discard Code", PHASE_SWEEP_COMPARTMENTS },
    { PHASE_SWEEP_TABLES, "Sweep Tables", PHASE_SWEEP_COMPARTMENTS },
    { PHASE_SWEEP_TABLES_WRAPPER, "Sweep Cross Compartment Wrappers", PHASE_SWEEP_TABLES },
    { PHASE_SWEEP_TABLES_BASE_SHAPE, "Sweep Base Shapes", PHASE_SWEEP_TABLES },
    { PHASE_SWEEP_TABLES_INITIAL_SHAPE, "Sweep Initial Shapes", PHASE_SWEEP_TABLES },
    { PHASE_SWEEP_TABLES_TYPE_OBJECT, "Sweep Type Objects", PHASE_SWEEP_TABLES },
    { PHASE_SWEEP_TABLES_BREAKPOINT, "Sweep Breakpoints", PHASE_SWEEP_TABLES },
    { PHASE_SWEEP_TABLES_REGEXP, "Sweep Regexps", PHASE_SWEEP_TABLES },
    { PHASE_DISCARD_ANALYSIS, "Discard Analysis", PHASE_SWEEP_COMPARTMENTS },
    { PHASE_DISCARD_TI, "Discard TI", PHASE_DISCARD_ANALYSIS },
    { PHASE_FREE_TI_ARENA, "Free TI Arena", PHASE_DISCARD_ANALYSIS },
    { PHASE_SWEEP_TYPES, "Sweep Types", PHASE_DISCARD_ANALYSIS },
    { PHASE_CLEAR_SCRIPT_ANALYSIS, "Clear Script Analysis", PHASE_DISCARD_ANALYSIS },
    { PHASE_SWEEP_OBJECT, "Sweep Object", PHASE_SWEEP },
    { PHASE_SWEEP_STRING, "Sweep String", PHASE_SWEEP },
    { PHASE_SWEEP_SCRIPT, "Sweep Script", PHASE_SWEEP },
    { PHASE_SWEEP_SHAPE, "Sweep Shape", PHASE_SWEEP },
    { PHASE_SWEEP_IONCODE, "Sweep Ion code", PHASE_SWEEP },
    { PHASE_FINALIZE_END, "Finalize End Callback", PHASE_SWEEP },
    { PHASE_DESTROY, "Deallocate", PHASE_SWEEP },
    { PHASE_GC_END, "End Callback", PHASE_NO_PARENT },
    { PHASE_LIMIT, nullptr, PHASE_NO_PARENT }
};

static void
FormatPhaseTimes(StatisticsSerializer &ss, const char *name, int64_t *times)
{
    ss.beginObject(name);
    for (unsigned i = 0; phases[i].name; i++)
        ss.appendIfNonzeroMS(phases[i].name, t(times[phases[i].index]));
    ss.endObject();
}

void
Statistics::gcDuration(int64_t *total, int64_t *maxPause)
{
    *total = *maxPause = 0;
    for (SliceData *slice = slices.begin(); slice != slices.end(); slice++) {
        *total += slice->duration();
        if (slice->duration() > *maxPause)
            *maxPause = slice->duration();
    }
}

void
Statistics::sccDurations(int64_t *total, int64_t *maxPause)
{
    *total = *maxPause = 0;
    for (size_t i = 0; i < sccTimes.length(); i++) {
        *total += sccTimes[i];
        *maxPause = Max(*maxPause, sccTimes[i]);
    }
}

bool
Statistics::formatData(StatisticsSerializer &ss, uint64_t timestamp)
{
    int64_t total, longest;
    gcDuration(&total, &longest);

    int64_t sccTotal, sccLongest;
    sccDurations(&sccTotal, &sccLongest);

    double mmu20 = computeMMU(20 * PRMJ_USEC_PER_MSEC);
    double mmu50 = computeMMU(50 * PRMJ_USEC_PER_MSEC);

    ss.beginObject(nullptr);
    if (ss.isJSON())
        ss.appendNumber("Timestamp", "%llu", "", (unsigned long long)timestamp);
    if (slices.length() > 1 || ss.isJSON())
        ss.appendDecimal("Max Pause", "ms", t(longest));
    else
        ss.appendString("Reason", ExplainReason(slices[0].reason));
    ss.appendDecimal("Total Time", "ms", t(total));
    ss.appendNumber("Compartments Collected", "%d", "", collectedCount);
    ss.appendNumber("Total Compartments", "%d", "", compartmentCount);
    ss.appendNumber("Total Zones", "%d", "", zoneCount);
    ss.appendNumber("MMU (20ms)", "%d", "%", int(mmu20 * 100));
    ss.appendNumber("MMU (50ms)", "%d", "%", int(mmu50 * 100));
    ss.appendDecimal("SCC Sweep Total", "ms", t(sccTotal));
    ss.appendDecimal("SCC Sweep Max Pause", "ms", t(sccLongest));
    if (nonincrementalReason || ss.isJSON()) {
        ss.appendString("Nonincremental Reason",
                        nonincrementalReason ? nonincrementalReason : "none");
    }
    ss.appendNumber("Allocated", "%u", "MB", unsigned(preBytes / 1024 / 1024));
    ss.appendNumber("+Chunks", "%d", "", counts[STAT_NEW_CHUNK]);
    ss.appendNumber("-Chunks", "%d", "", counts[STAT_DESTROY_CHUNK]);
    ss.endLine();

    if (slices.length() > 1 || ss.isJSON()) {
        ss.beginArray("Slices");
        for (size_t i = 0; i < slices.length(); i++) {
            int64_t width = slices[i].duration();
            if (i != 0 && i != slices.length() - 1 && width < SLICE_MIN_REPORT_TIME &&
                !slices[i].resetReason && !ss.isJSON())
            {
                continue;
            }

            ss.beginObject(nullptr);
            ss.extra("    ");
            ss.appendNumber("Slice", "%d", "", i);
            ss.appendDecimal("Pause", "", t(width));
            ss.extra(" (");
            ss.appendDecimal("When", "ms", t(slices[i].start - slices[0].start));
            ss.appendString("Reason", ExplainReason(slices[i].reason));
            if (ss.isJSON()) {
                ss.appendDecimal("Page Faults", "",
                                 double(slices[i].endFaults - slices[i].startFaults));

                ss.appendNumber("Start Timestamp", "%llu", "", (unsigned long long)slices[i].start);
                ss.appendNumber("End Timestamp", "%llu", "", (unsigned long long)slices[i].end);
            }
            if (slices[i].resetReason)
                ss.appendString("Reset", slices[i].resetReason);
            ss.extra("): ");
            FormatPhaseTimes(ss, "Times", slices[i].phaseTimes);
            ss.endLine();
            ss.endObject();
        }
        ss.endArray();
    }
    ss.extra("    Totals: ");
    FormatPhaseTimes(ss, "Totals", phaseTimes);
    ss.endObject();

    return !ss.isOOM();
}

jschar *
Statistics::formatMessage()
{
    StatisticsSerializer ss(StatisticsSerializer::AsText);
    formatData(ss, 0);
    return ss.finishJSString();
}

jschar *
Statistics::formatJSON(uint64_t timestamp)
{
    StatisticsSerializer ss(StatisticsSerializer::AsJSON);
    formatData(ss, timestamp);
    return ss.finishJSString();
}

Statistics::Statistics(JSRuntime *rt)
  : runtime(rt),
    startupTime(PRMJ_Now()),
    fp(nullptr),
    fullFormat(false),
    gcDepth(0),
    collectedCount(0),
    zoneCount(0),
    compartmentCount(0),
    nonincrementalReason(nullptr),
    preBytes(0),
    phaseNestingDepth(0)
{
    PodArrayZero(phaseTotals);
    PodArrayZero(counts);

    char *env = getenv("MOZ_GCTIMER");
    if (!env || strcmp(env, "none") == 0) {
        fp = nullptr;
        return;
    }

    if (strcmp(env, "stdout") == 0) {
        fullFormat = false;
        fp = stdout;
    } else if (strcmp(env, "stderr") == 0) {
        fullFormat = false;
        fp = stderr;
    } else {
        fullFormat = true;

        fp = fopen(env, "a");
        JS_ASSERT(fp);
    }
}

Statistics::~Statistics()
{
    if (fp) {
        if (fullFormat) {
            StatisticsSerializer ss(StatisticsSerializer::AsText);
            FormatPhaseTimes(ss, "", phaseTotals);
            char *msg = ss.finishCString();
            if (msg) {
                fprintf(fp, "TOTALS\n%s\n\n-------\n", msg);
                js_free(msg);
            }
        }

        if (fp != stdout && fp != stderr)
            fclose(fp);
    }
}

void
Statistics::printStats()
{
    if (fullFormat) {
        StatisticsSerializer ss(StatisticsSerializer::AsText);
        formatData(ss, 0);
        char *msg = ss.finishCString();
        if (msg) {
            fprintf(fp, "GC(T+%.3fs) %s\n", t(slices[0].start - startupTime) / 1000.0, msg);
            js_free(msg);
        }
    } else {
        int64_t total, longest;
        gcDuration(&total, &longest);

        fprintf(fp, "%f %f %f\n",
                t(total),
                t(phaseTimes[PHASE_MARK]),
                t(phaseTimes[PHASE_SWEEP]));
    }
    fflush(fp);
}

void
Statistics::beginGC()
{
    PodArrayZero(phaseStartTimes);
    PodArrayZero(phaseTimes);

    slices.clearAndFree();
    sccTimes.clearAndFree();
    nonincrementalReason = nullptr;

    preBytes = runtime->gcBytes;
}

void
Statistics::endGC()
{
    crash::SnapshotGCStack();

    for (int i = 0; i < PHASE_LIMIT; i++)
        phaseTotals[i] += phaseTimes[i];

    if (JSAccumulateTelemetryDataCallback cb = runtime->telemetryCallback) {
        int64_t total, longest;
        gcDuration(&total, &longest);

        int64_t sccTotal, sccLongest;
        sccDurations(&sccTotal, &sccLongest);

        (*cb)(JS_TELEMETRY_GC_IS_COMPARTMENTAL, collectedCount == zoneCount ? 0 : 1);
        (*cb)(JS_TELEMETRY_GC_MS, t(total));
        (*cb)(JS_TELEMETRY_GC_MAX_PAUSE_MS, t(longest));
        (*cb)(JS_TELEMETRY_GC_MARK_MS, t(phaseTimes[PHASE_MARK]));
        (*cb)(JS_TELEMETRY_GC_SWEEP_MS, t(phaseTimes[PHASE_SWEEP]));
        (*cb)(JS_TELEMETRY_GC_MARK_ROOTS_MS, t(phaseTimes[PHASE_MARK_ROOTS]));
        (*cb)(JS_TELEMETRY_GC_MARK_GRAY_MS, t(phaseTimes[PHASE_SWEEP_MARK_GRAY]));
        (*cb)(JS_TELEMETRY_GC_NON_INCREMENTAL, !!nonincrementalReason);
        (*cb)(JS_TELEMETRY_GC_INCREMENTAL_DISABLED, !runtime->gcIncrementalEnabled);
        (*cb)(JS_TELEMETRY_GC_SCC_SWEEP_TOTAL_MS, t(sccTotal));
        (*cb)(JS_TELEMETRY_GC_SCC_SWEEP_MAX_PAUSE_MS, t(sccLongest));

        double mmu50 = computeMMU(50 * PRMJ_USEC_PER_MSEC);
        (*cb)(JS_TELEMETRY_GC_MMU_50, mmu50 * 100);
    }

    if (fp)
        printStats();
}

void
Statistics::beginSlice(int collectedCount, int zoneCount, int compartmentCount,
                       JS::gcreason::Reason reason)
{
    this->collectedCount = collectedCount;
    this->zoneCount = zoneCount;
    this->compartmentCount = compartmentCount;

    bool first = runtime->gcIncrementalState == gc::NO_INCREMENTAL;
    if (first)
        beginGC();

    SliceData data(reason, PRMJ_Now(), gc::GetPageFaultCount());
    (void) slices.append(data); /* Ignore any OOMs here. */

    if (JSAccumulateTelemetryDataCallback cb = runtime->telemetryCallback)
        (*cb)(JS_TELEMETRY_GC_REASON, reason);

    // Slice callbacks should only fire for the outermost level
    if (++gcDepth == 1) {
        bool wasFullGC = collectedCount == zoneCount;
        if (JS::GCSliceCallback cb = runtime->gcSliceCallback)
            (*cb)(runtime, first ? JS::GC_CYCLE_BEGIN : JS::GC_SLICE_BEGIN,
                  JS::GCDescription(!wasFullGC));
    }
}

void
Statistics::endSlice()
{
    slices.back().end = PRMJ_Now();
    slices.back().endFaults = gc::GetPageFaultCount();

    if (JSAccumulateTelemetryDataCallback cb = runtime->telemetryCallback) {
        (*cb)(JS_TELEMETRY_GC_SLICE_MS, t(slices.back().end - slices.back().start));
        (*cb)(JS_TELEMETRY_GC_RESET, !!slices.back().resetReason);
    }

    bool last = runtime->gcIncrementalState == gc::NO_INCREMENTAL;
    if (last)
        endGC();

    // Slice callbacks should only fire for the outermost level
    if (--gcDepth == 0) {
        bool wasFullGC = collectedCount == zoneCount;
        if (JS::GCSliceCallback cb = runtime->gcSliceCallback)
            (*cb)(runtime, last ? JS::GC_CYCLE_END : JS::GC_SLICE_END,
                  JS::GCDescription(!wasFullGC));
    }

    /* Do this after the slice callback since it uses these values. */
    if (last)
        PodArrayZero(counts);
}

void
Statistics::beginPhase(Phase phase)
{
    /* Guard against re-entry */
    JS_ASSERT(!phaseStartTimes[phase]);

#ifdef DEBUG
    JS_ASSERT(phases[phase].index == phase);
    Phase parent = phaseNestingDepth ? phaseNesting[phaseNestingDepth - 1] : PHASE_NO_PARENT;
    JS_ASSERT(phaseNestingDepth < MAX_NESTING);
    JS_ASSERT_IF(gcDepth == 1, phases[phase].parent == parent);
    phaseNesting[phaseNestingDepth] = phase;
    phaseNestingDepth++;
#endif

    phaseStartTimes[phase] = PRMJ_Now();
}

void
Statistics::endPhase(Phase phase)
{
    phaseNestingDepth--;

    int64_t t = PRMJ_Now() - phaseStartTimes[phase];
    slices.back().phaseTimes[phase] += t;
    phaseTimes[phase] += t;
    phaseStartTimes[phase] = 0;
}

int64_t
Statistics::beginSCC()
{
    return PRMJ_Now();
}

void
Statistics::endSCC(unsigned scc, int64_t start)
{
    if (scc >= sccTimes.length() && !sccTimes.resize(scc + 1))
        return;

    sccTimes[scc] += PRMJ_Now() - start;
}

/*
 * MMU (minimum mutator utilization) is a measure of how much garbage collection
 * is affecting the responsiveness of the system. MMU measurements are given
 * with respect to a certain window size. If we report MMU(50ms) = 80%, then
 * that means that, for any 50ms window of time, at least 80% of the window is
 * devoted to the mutator. In other words, the GC is running for at most 20% of
 * the window, or 10ms. The GC can run multiple slices during the 50ms window
 * as long as the total time it spends is at most 10ms.
 */
double
Statistics::computeMMU(int64_t window)
{
    JS_ASSERT(!slices.empty());

    int64_t gc = slices[0].end - slices[0].start;
    int64_t gcMax = gc;

    if (gc >= window)
        return 0.0;

    int startIndex = 0;
    for (size_t endIndex = 1; endIndex < slices.length(); endIndex++) {
        gc += slices[endIndex].end - slices[endIndex].start;

        while (slices[endIndex].end - slices[startIndex].end >= window) {
            gc -= slices[startIndex].end - slices[startIndex].start;
            startIndex++;
        }

        int64_t cur = gc;
        if (slices[endIndex].end - slices[startIndex].start > window)
            cur -= (slices[endIndex].end - slices[startIndex].start - window);
        if (cur > gcMax)
            gcMax = cur;
    }

    return double(window - gcMax) / window;
}
