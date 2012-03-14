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

/* Except for the first and last, slices of less than 12ms are not reported. */
static const int64_t SLICE_MIN_REPORT_TIME = 12 * PRMJ_USEC_PER_MSEC;

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

    void appendIfNonzeroMS(const char *name, double v) {
        if (asJSON_ || v)
            appendNumber(name, "%.1f", "ms", v);
    }

    void beginObject(const char *name) {
        if (needComma_)
            pJSON(", ");
        if (asJSON_ && name) {
            putQuoted(name);
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
            putQuoted(name);
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

static void
formatPhases(StatisticsSerializer &ss, const char *name, int64_t *times)
{
    ss.beginObject(name);
    ss.appendIfNonzeroMS("Mark", t(times[PHASE_MARK]));
    ss.appendIfNonzeroMS("Mark Roots", t(times[PHASE_MARK_ROOTS]));
    ss.appendIfNonzeroMS("Mark Delayed", t(times[PHASE_MARK_DELAYED]));
    ss.appendIfNonzeroMS("Mark Other", t(times[PHASE_MARK_OTHER]));
    ss.appendIfNonzeroMS("Sweep", t(times[PHASE_SWEEP]));
    ss.appendIfNonzeroMS("Sweep Object", t(times[PHASE_SWEEP_OBJECT]));
    ss.appendIfNonzeroMS("Sweep String", t(times[PHASE_SWEEP_STRING]));
    ss.appendIfNonzeroMS("Sweep Script", t(times[PHASE_SWEEP_SCRIPT]));
    ss.appendIfNonzeroMS("Sweep Shape", t(times[PHASE_SWEEP_SHAPE]));
    ss.appendIfNonzeroMS("Discard Code", t(times[PHASE_DISCARD_CODE]));
    ss.appendIfNonzeroMS("Discard Analysis", t(times[PHASE_DISCARD_ANALYSIS]));
    ss.appendIfNonzeroMS("XPConnect", t(times[PHASE_XPCONNECT]));
    ss.appendIfNonzeroMS("Deallocate", t(times[PHASE_DESTROY]));
    ss.endObject();
}

bool
Statistics::formatData(StatisticsSerializer &ss)
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
    ss.appendNumber("Total Time", "%.1f", "ms", t(total));
    ss.appendString("Type", compartment ? "compartment" : "global");
    ss.appendNumber("MMU (20ms)", "%d", "%", int(mmu20 * 100));
    ss.appendNumber("MMU (50ms)", "%d", "%", int(mmu50 * 100));
    if (slices.length() > 1 || ss.isJSON())
        ss.appendNumber("Max Pause", "%.1f", "ms", t(longest));
    else
        ss.appendString("Reason", ExplainReason(slices[0].reason));
    if (nonincrementalReason || ss.isJSON()) {
        ss.appendString("Nonincremental Reason",
                        nonincrementalReason ? nonincrementalReason : "none");
    }
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
            ss.appendNumber("Time", "%.1f", "ms", t(slices[i].end - slices[0].start));
            ss.extra(" (");
            ss.appendNumber("Pause", "%.1f", "", t(width));
            ss.appendString("Reason", ExplainReason(slices[i].reason));
            if (slices[i].resetReason)
                ss.appendString("Reset", slices[i].resetReason);
            ss.extra("): ");
            formatPhases(ss, "times", slices[i].phaseTimes);
            ss.endLine();
            ss.endObject();
        }
        ss.endArray();
    }
    ss.extra("    Totals: ");
    formatPhases(ss, "totals", phaseTimes);
    ss.endObject();

    return !ss.isOOM();
}

jschar *
Statistics::formatMessage()
{
    StatisticsSerializer ss(StatisticsSerializer::AsText);
    formatData(ss);
    return ss.finishJSString();
}

jschar *
Statistics::formatJSON()
{
    StatisticsSerializer ss(StatisticsSerializer::AsJSON);
    formatData(ss);
    return ss.finishJSString();
}

Statistics::Statistics(JSRuntime *rt)
  : runtime(rt),
    startupTime(PRMJ_Now()),
    fp(NULL),
    fullFormat(false),
    compartment(NULL),
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
            formatPhases(ss, "", phaseTotals);
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
        formatData(ss);
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

    if (GCSliceCallback cb = runtime->gcSliceCallback)
        (*cb)(runtime, first ? GC_CYCLE_BEGIN : GC_SLICE_BEGIN, GCDescription(!!compartment));
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
            (*cb)(runtime, GC_CYCLE_END, GCDescription(!!compartment));
        else
            (*cb)(runtime, GC_SLICE_END, GCDescription(!!compartment));
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
