/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SpiderMonkey JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#include "gc/Statistics.h"

namespace js {
namespace gcstats {

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

void
Statistics::fmt(const char *f, ...)
{
    va_list va;
    size_t off = strlen(buffer);

    va_start(va, f);
    JS_vsnprintf(buffer + off, BUFFER_SIZE - off, f, va);
    va_end(va);
}

void
Statistics::fmtIfNonzero(const char *name, double t)
{
    if (t) {
        if (needComma)
            fmt(", ");
        fmt("%s: %.1f", name, t);
        needComma = true;
    }
}

void
Statistics::formatPhases(int64_t *times)
{
    needComma = false;
    fmtIfNonzero("mark", t(times[PHASE_MARK]));
    fmtIfNonzero("mark-roots", t(times[PHASE_MARK_ROOTS]));
    fmtIfNonzero("mark-delayed", t(times[PHASE_MARK_DELAYED]));
    fmtIfNonzero("mark-other", t(times[PHASE_MARK_OTHER]));
    fmtIfNonzero("sweep", t(times[PHASE_SWEEP]));
    fmtIfNonzero("sweep-obj", t(times[PHASE_SWEEP_OBJECT]));
    fmtIfNonzero("sweep-string", t(times[PHASE_SWEEP_STRING]));
    fmtIfNonzero("sweep-script", t(times[PHASE_SWEEP_SCRIPT]));
    fmtIfNonzero("sweep-shape", t(times[PHASE_SWEEP_SHAPE]));
    fmtIfNonzero("discard-code", t(times[PHASE_DISCARD_CODE]));
    fmtIfNonzero("discard-analysis", t(times[PHASE_DISCARD_ANALYSIS]));
    fmtIfNonzero("xpconnect", t(times[PHASE_XPCONNECT]));
    fmtIfNonzero("deallocate", t(times[PHASE_DESTROY]));
}

/* Except for the first and last, slices of less than 12ms are not reported. */
static const int64_t SLICE_MIN_REPORT_TIME = 12 * PRMJ_USEC_PER_MSEC;

const char *
Statistics::formatData()
{
    buffer[0] = 0x00;

    int64_t total = 0, longest = 0;

    for (SliceData *slice = slices.begin(); slice != slices.end(); slice++) {
        total += slice->duration();
        if (slice->duration() > longest)
            longest = slice->duration();
    }

    double mmu20 = computeMMU(20 * PRMJ_USEC_PER_MSEC);
    double mmu50 = computeMMU(50 * PRMJ_USEC_PER_MSEC);

    fmt("TotalTime: %.1fms, Type: %s", t(total), compartment ? "compartment" : "global");
    fmt(", MMU(20ms): %d%%, MMU(50ms): %d%%", int(mmu20 * 100), int(mmu50 * 100));

    if (slices.length() > 1)
        fmt(", MaxPause: %.1f", t(longest));
    else
        fmt(", Reason: %s", ExplainReason(slices[0].reason));

    if (nonincrementalReason)
        fmt(", NonIncrementalReason: %s", nonincrementalReason);

    fmt(", +chunks: %d, -chunks: %d\n", counts[STAT_NEW_CHUNK], counts[STAT_DESTROY_CHUNK]);

    if (slices.length() > 1) {
        for (size_t i = 0; i < slices.length(); i++) {
            int64_t width = slices[i].duration();
            if (i != 0 && i != slices.length() - 1 && width < SLICE_MIN_REPORT_TIME &&
                !slices[i].resetReason)
            {
                continue;
            }

            fmt("    Slice %d @ %.1fms (Pause: %.1f, Reason: %s",
                i,
                t(slices[i].end - slices[0].start),
                t(width),
                ExplainReason(slices[i].reason));
            if (slices[i].resetReason)
                fmt(", Reset: %s", slices[i].resetReason);
            fmt("): ");
            formatPhases(slices[i].phaseTimes);
            fmt("\n");
        }

        fmt("    Totals: ");
    }

    formatPhases(phaseTimes);
    fmt("\n");

    return buffer;
}

Statistics::Statistics(JSRuntime *rt)
  : runtime(rt),
    startupTime(PRMJ_Now()),
    fp(NULL),
    fullFormat(false),
    compartment(NULL),
    nonincrementalReason(NULL),
    needComma(false)
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
            buffer[0] = 0x00;
            formatPhases(phaseTotals);
            fprintf(fp, "TOTALS\n%s\n\n-------\n", buffer);
        }

        if (fp != stdout && fp != stderr)
            fclose(fp);
    }
}

double
Statistics::t(int64_t t)
{
    return double(t) / PRMJ_USEC_PER_MSEC;
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
        fprintf(fp, "GC(T+%.3fs) %s\n",
                t(slices[0].start - startupTime) / 1000.0,
                formatData());
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
    PodArrayZero(phaseStarts);
    PodArrayZero(phaseTimes);

    slices.clearAndFree();
    nonincrementalReason = NULL;

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
        (*cb)(JS_TELEMETRY_GC_IS_COMPARTMENTAL, compartment ? 1 : 0);
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

    PodArrayZero(counts);
}

void
Statistics::beginSlice(JSCompartment *comp, gcreason::Reason reason)
{
    compartment = comp;

    bool first = runtime->gcIncrementalState == gc::NO_INCREMENTAL;
    if (first)
        beginGC();

    SliceData data(reason, PRMJ_Now());
    (void) slices.append(data); /* Ignore any OOMs here. */

    if (JSAccumulateTelemetryDataCallback cb = runtime->telemetryCallback)
        (*cb)(JS_TELEMETRY_GC_REASON, reason);

    if (GCSliceCallback cb = runtime->gcSliceCallback) {
        GCDescription desc(NULL, !!compartment);
        (*cb)(runtime, first ? GC_CYCLE_BEGIN : GC_SLICE_BEGIN, desc);
    }
}

void
Statistics::endSlice()
{
    slices.back().end = PRMJ_Now();

    if (JSAccumulateTelemetryDataCallback cb = runtime->telemetryCallback) {
        (*cb)(JS_TELEMETRY_GC_SLICE_MS, t(slices.back().end - slices.back().start));
        (*cb)(JS_TELEMETRY_GC_RESET, !!slices.back().resetReason);
    }

    bool last = runtime->gcIncrementalState == gc::NO_INCREMENTAL;
    if (last)
        endGC();

    if (GCSliceCallback cb = runtime->gcSliceCallback) {
        if (last)
            (*cb)(runtime, GC_CYCLE_END, GCDescription(formatData(), !!compartment));
        else
            (*cb)(runtime, GC_SLICE_END, GCDescription(NULL, !!compartment));
    }
}

void
Statistics::beginPhase(Phase phase)
{
    phaseStarts[phase] = PRMJ_Now();

    if (phase == gcstats::PHASE_MARK)
        Probes::GCStartMarkPhase();
    else if (phase == gcstats::PHASE_SWEEP)
        Probes::GCStartSweepPhase();
}

void
Statistics::endPhase(Phase phase)
{
    int64_t now = PRMJ_Now();
    int64_t t = now - phaseStarts[phase];
    slices.back().phaseTimes[phase] += t;
    phaseTimes[phase] += t;

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
