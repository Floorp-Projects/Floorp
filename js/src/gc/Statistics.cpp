/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdarg.h>

#include "jscntxt.h"
#include "jscompartment.h"
#include "jscrashformat.h"
#include "jscrashreport.h"
#include "jsprf.h"
#include "jsprobes.h"
#include "jsutil.h"
#include "prmjtime.h"

#include "gc/Memory.h"
#include "gc/Statistics.h"

#include "gc/Barrier-inl.h"

namespace js {
namespace gcstats {

/* Except for the first and last, slices of less than 42ms are not reported. */
static const int64_t SLICE_MIN_REPORT_TIME = 42 * PRMJ_USEC_PER_MSEC;

class StatisticsSerializer
{
    typedef Vector<char, 128, SystemAllocPolicy> CharBuffer;
    CharBuffer buf_;
    bool asJSON_;
    bool needComma_;
    bool oom_;

    const static int MaxFieldValueLength = 128;

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
            return NULL;

        size_t nchars = strlen(buf);
        jschar *out = (jschar *)js_malloc(sizeof(jschar) * (nchars + 1));
        if (!out) {
            oom_ = true;
            js_free(buf);
            return NULL;
        }

        size_t outlen = nchars;
        bool ok = InflateStringToBuffer(NULL, buf, nchars, out, &outlen);
        js_free(buf);
        if (!ok) {
            oom_ = true;
            js_free(out);
            return NULL;
        }
        out[nchars] = 0;

        return out;
    }

    char *finishCString() {
        if (oom_)
            return NULL;

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
JS_STATIC_ASSERT(gcreason::NUM_TELEMETRY_REASONS >= gcreason::NUM_REASONS);

static const char *
ExplainReason(gcreason::Reason reason)
{
    switch (reason) {
#define SWITCH_REASON(name)                     \
        case gcreason::name:                    \
          return #name;
        GCREASONS(SWITCH_REASON)

        default:
          JS_NOT_REACHED("bad GC reason");
          return "?";
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
    unsigned index;
    const char *name;
};

static PhaseInfo phases[] = {
    { PHASE_GC_BEGIN, "Begin Callback" },
    { PHASE_WAIT_BACKGROUND_THREAD, "Wait Background Thread" },
    { PHASE_PURGE, "Purge" },
    { PHASE_MARK, "Mark" },
    { PHASE_MARK_DISCARD_CODE, "Mark Discard Code" },
    { PHASE_MARK_ROOTS, "Mark Roots" },
    { PHASE_MARK_TYPES, "Mark Types" },
    { PHASE_MARK_DELAYED, "Mark Delayed" },
    { PHASE_MARK_WEAK, "Mark Weak" },
    { PHASE_MARK_GRAY, "Mark Gray" },
    { PHASE_MARK_GRAY_WEAK, "Mark Gray and Weak" },
    { PHASE_FINALIZE_START, "Finalize Start Callback" },
    { PHASE_SWEEP, "Sweep" },
    { PHASE_SWEEP_ATOMS, "Sweep Atoms" },
    { PHASE_SWEEP_COMPARTMENTS, "Sweep Compartments" },
    { PHASE_SWEEP_TABLES, "Sweep Tables" },
    { PHASE_SWEEP_OBJECT, "Sweep Object" },
    { PHASE_SWEEP_STRING, "Sweep String" },
    { PHASE_SWEEP_SCRIPT, "Sweep Script" },
    { PHASE_SWEEP_SHAPE, "Sweep Shape" },
    { PHASE_SWEEP_DISCARD_CODE, "Sweep Discard Code" },
    { PHASE_DISCARD_ANALYSIS, "Discard Analysis" },
    { PHASE_DISCARD_TI, "Discard TI" },
    { PHASE_FREE_TI_ARENA, "Free TI Arena" },
    { PHASE_SWEEP_TYPES, "Sweep Types" },
    { PHASE_CLEAR_SCRIPT_ANALYSIS, "Clear Script Analysis" },
    { PHASE_FINALIZE_END, "Finalize End Callback" },
    { PHASE_DESTROY, "Deallocate" },
    { PHASE_GC_END, "End Callback" },
    { 0, NULL }
};

static void
FormatPhaseTimes(StatisticsSerializer &ss, const char *name, int64_t *times)
{
    ss.beginObject(name);
    for (unsigned i = 0; phases[i].name; i++)
        ss.appendIfNonzeroMS(phases[i].name, t(times[phases[i].index]));
    ss.endObject();
}

bool
Statistics::formatData(StatisticsSerializer &ss, uint64_t timestamp)
{
    int64_t total = 0, longest = 0;
    for (SliceData *slice = slices.begin(); slice != slices.end(); slice++) {
        total += slice->duration();
        if (slice->duration() > longest)
            longest = slice->duration();
    }

    double mmu20 = computeMMU(20 * PRMJ_USEC_PER_MSEC);
    double mmu50 = computeMMU(50 * PRMJ_USEC_PER_MSEC);

    ss.beginObject(NULL);
    if (ss.isJSON())
        ss.appendNumber("Timestamp", "%llu", "", (unsigned long long)timestamp);
    ss.appendDecimal("Total Time", "ms", t(total));
    ss.appendNumber("Compartments Collected", "%d", "", collectedCount);
    ss.appendNumber("Total Compartments", "%d", "", compartmentCount);
    ss.appendNumber("MMU (20ms)", "%d", "%", int(mmu20 * 100));
    ss.appendNumber("MMU (50ms)", "%d", "%", int(mmu50 * 100));
    if (slices.length() > 1 || ss.isJSON())
        ss.appendDecimal("Max Pause", "ms", t(longest));
    else
        ss.appendString("Reason", ExplainReason(slices[0].reason));
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

            ss.beginObject(NULL);
            ss.extra("    ");
            ss.appendNumber("Slice", "%d", "", i);
            ss.appendDecimal("Pause", "", t(width));
            ss.extra(" (");
            ss.appendDecimal("When", "ms", t(slices[i].start - slices[0].start));
            ss.appendString("Reason", ExplainReason(slices[i].reason));
            if (ss.isJSON()) {
                ss.appendDecimal("Page Faults", "",
                                 double(slices[i].endFaults - slices[i].startFaults));
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
    fp(NULL),
    fullFormat(false),
    gcDepth(0),
    collectedCount(0),
    compartmentCount(0),
    nonincrementalReason(NULL)
{
    PodArrayZero(phaseTotals);
    PodArrayZero(counts);

    char *env = getenv("MOZ_GCTIMER");
    if (!env || strcmp(env, "none") == 0) {
        fp = NULL;
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

int64_t
Statistics::gcDuration()
{
    return slices.back().end - slices[0].start;
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
        fprintf(fp, "%f %f %f\n",
                t(gcDuration()),
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
    nonincrementalReason = NULL;

    preBytes = runtime->gcBytes;

    Probes::GCStart();
}

void
Statistics::endGC()
{
    Probes::GCEnd();
    crash::SnapshotGCStack();

    for (int i = 0; i < PHASE_LIMIT; i++)
        phaseTotals[i] += phaseTimes[i];

    if (JSAccumulateTelemetryDataCallback cb = runtime->telemetryCallback) {
        (*cb)(JS_TELEMETRY_GC_IS_COMPARTMENTAL, collectedCount == compartmentCount ? 0 : 1);
        (*cb)(JS_TELEMETRY_GC_MS, t(gcDuration()));
        (*cb)(JS_TELEMETRY_GC_MARK_MS, t(phaseTimes[PHASE_MARK]));
        (*cb)(JS_TELEMETRY_GC_SWEEP_MS, t(phaseTimes[PHASE_SWEEP]));
        (*cb)(JS_TELEMETRY_GC_NON_INCREMENTAL, !!nonincrementalReason);
        (*cb)(JS_TELEMETRY_GC_INCREMENTAL_DISABLED, !runtime->gcIncrementalEnabled);

        double mmu50 = computeMMU(50 * PRMJ_USEC_PER_MSEC);
        (*cb)(JS_TELEMETRY_GC_MMU_50, mmu50 * 100);
    }

    if (fp)
        printStats();
}

void
Statistics::beginSlice(int collectedCount, int compartmentCount, gcreason::Reason reason)
{
    this->collectedCount = collectedCount;
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
        bool wasFullGC = collectedCount == compartmentCount;
        if (GCSliceCallback cb = runtime->gcSliceCallback)
            (*cb)(runtime, first ? GC_CYCLE_BEGIN : GC_SLICE_BEGIN, GCDescription(!wasFullGC));
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
        bool wasFullGC = collectedCount == compartmentCount;
        if (GCSliceCallback cb = runtime->gcSliceCallback)
            (*cb)(runtime, last ? GC_CYCLE_END : GC_SLICE_END, GCDescription(!wasFullGC));
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

    phaseStartTimes[phase] = PRMJ_Now();

    if (phase == gcstats::PHASE_MARK)
        Probes::GCStartMarkPhase();
    else if (phase == gcstats::PHASE_SWEEP)
        Probes::GCStartSweepPhase();
}

void
Statistics::endPhase(Phase phase)
{
    int64_t t = PRMJ_Now() - phaseStartTimes[phase];
    slices.back().phaseTimes[phase] += t;
    phaseTimes[phase] += t;
    phaseStartTimes[phase] = 0;

    if (phase == gcstats::PHASE_MARK)
        Probes::GCEndMarkPhase();
    else if (phase == gcstats::PHASE_SWEEP)
        Probes::GCEndSweepPhase();
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

} /* namespace gcstats */
} /* namespace js */
