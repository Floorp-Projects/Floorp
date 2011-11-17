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

#include "jscntxt.h"
#include "jscrashformat.h"
#include "jscrashreport.h"
#include "jsprf.h"
#include "jsprobes.h"
#include "jsutil.h"
#include "prmjtime.h"

#include "gc/Statistics.h"

namespace js {
namespace gcstats {

Statistics::ColumnInfo::ColumnInfo(const char *title, double t, double total)
  : title(title)
{
    JS_snprintf(str, sizeof(str), "%.1f", t);
    JS_snprintf(totalStr, sizeof(totalStr), "%.1f", total);
    width = 6;
}

Statistics::ColumnInfo::ColumnInfo(const char *title, double t)
  : title(title)
{
    JS_snprintf(str, sizeof(str), "%.1f", t);
    strcpy(totalStr, "n/a");
    width = 6;
}

Statistics::ColumnInfo::ColumnInfo(const char *title, unsigned int data)
  : title(title)
{
    JS_snprintf(str, sizeof(str), "%d", data);
    strcpy(totalStr, "n/a");
    width = 4;
}

Statistics::ColumnInfo::ColumnInfo(const char *title, const char *data)
  : title(title)
{
    JS_ASSERT(strlen(data) < sizeof(str));
    strcpy(str, data);
    strcpy(totalStr, "n/a ");
    width = 0;
}

static const int NUM_COLUMNS = 17;

void
Statistics::makeTable(ColumnInfo *cols)
{
    int i = 0;

    cols[i++] = ColumnInfo("Type", compartment ? "Comp" : "Glob");

    cols[i++] = ColumnInfo("Total", t(PHASE_GC), total(PHASE_GC));
    cols[i++] = ColumnInfo("Wait", beginDelay(PHASE_MARK, PHASE_GC));
    cols[i++] = ColumnInfo("Mark", t(PHASE_MARK), total(PHASE_MARK));
    cols[i++] = ColumnInfo("Sweep", t(PHASE_SWEEP), total(PHASE_SWEEP));
    cols[i++] = ColumnInfo("FinObj", t(PHASE_SWEEP_OBJECT), total(PHASE_SWEEP_OBJECT));
    cols[i++] = ColumnInfo("FinStr", t(PHASE_SWEEP_STRING), total(PHASE_SWEEP_STRING));
    cols[i++] = ColumnInfo("FinScr", t(PHASE_SWEEP_SCRIPT), total(PHASE_SWEEP_SCRIPT));
    cols[i++] = ColumnInfo("FinShp", t(PHASE_SWEEP_SHAPE), total(PHASE_SWEEP_SHAPE));
    cols[i++] = ColumnInfo("DisCod", t(PHASE_DISCARD_CODE), total(PHASE_DISCARD_CODE));
    cols[i++] = ColumnInfo("DisAnl", t(PHASE_DISCARD_ANALYSIS), total(PHASE_DISCARD_ANALYSIS));
    cols[i++] = ColumnInfo("XPCnct", t(PHASE_XPCONNECT), total(PHASE_XPCONNECT));
    cols[i++] = ColumnInfo("Destry", t(PHASE_DESTROY), total(PHASE_DESTROY));
    cols[i++] = ColumnInfo("End", endDelay(PHASE_GC, PHASE_DESTROY));

    cols[i++] = ColumnInfo("+Chu", counts[STAT_NEW_CHUNK]);
    cols[i++] = ColumnInfo("-Chu", counts[STAT_DESTROY_CHUNK]);

    cols[i++] = ColumnInfo("Reason", ExplainReason(triggerReason));

    JS_ASSERT(i == NUM_COLUMNS);
}

Statistics::Statistics(JSRuntime *rt)
  : runtime(rt)
  , triggerReason(PUBLIC_API) //dummy reason to satisfy makeTable
{
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

        fprintf(fp, "     AppTime");

        ColumnInfo cols[NUM_COLUMNS];
        makeTable(cols);
        for (int i = 0; i < NUM_COLUMNS; i++)
            fprintf(fp, ", %*s", cols[i].width, cols[i].title);
        fprintf(fp, "\n");
    }

    PodArrayZero(counts);
    PodArrayZero(totals);

    startupTime = PRMJ_Now();
}

Statistics::~Statistics()
{
    if (fp) {
        if (fullFormat) {
            fprintf(fp, "------>TOTAL");

            ColumnInfo cols[NUM_COLUMNS];
            makeTable(cols);
            for (int i = 0; i < NUM_COLUMNS && cols[i].totalStr[0]; i++)
                fprintf(fp, ", %*s", cols[i].width, cols[i].totalStr);
            fprintf(fp, "\n");
        }

        if (fp != stdout && fp != stderr)
            fclose(fp);
    }
}

struct GCCrashData
{
    int isRegen;
    int isCompartment;
};

void
Statistics::beginGC(JSCompartment *comp, Reason reason)
{
    compartment = comp;

    PodArrayZero(phaseStarts);
    PodArrayZero(phaseEnds);
    PodArrayZero(phaseTimes);

    triggerReason = reason;

    beginPhase(PHASE_GC);
    Probes::GCStart(compartment);

    GCCrashData crashData;
    crashData.isRegen = runtime->shapeGen & SHAPE_OVERFLOW_BIT;
    crashData.isCompartment = !!compartment;
    crash::SaveCrashData(crash::JS_CRASH_TAG_GC, &crashData, sizeof(crashData));
}

double
Statistics::t(Phase phase)
{
    return double(phaseTimes[phase]) / PRMJ_USEC_PER_MSEC;
}

double
Statistics::total(Phase phase)
{
    return double(totals[phase]) / PRMJ_USEC_PER_MSEC;
}

double
Statistics::beginDelay(Phase phase1, Phase phase2)
{
    return double(phaseStarts[phase1] - phaseStarts[phase2]) / PRMJ_USEC_PER_MSEC;
}

double
Statistics::endDelay(Phase phase1, Phase phase2)
{
    return double(phaseEnds[phase1] - phaseEnds[phase2]) / PRMJ_USEC_PER_MSEC;
}

void
Statistics::statsToString(char *buffer, size_t size)
{
    JS_ASSERT(size);
    buffer[0] = 0x00;

    ColumnInfo cols[NUM_COLUMNS];
    makeTable(cols);

    size_t pos = 0;
    for (int i = 0; i < NUM_COLUMNS; i++) {
        int len = strlen(cols[i].title) + 1 + strlen(cols[i].str);
        if (i > 0)
            len += 2;
        if (pos + len >= size)
            break;
        if (i > 0)
            strcat(buffer, ", ");
        strcat(buffer, cols[i].title);
        strcat(buffer, ":");
        strcat(buffer, cols[i].str);
        pos += len;
    }
}

void
Statistics::printStats()
{
    if (fullFormat) {
        fprintf(fp, "%12.0f", double(phaseStarts[PHASE_GC] - startupTime) / PRMJ_USEC_PER_MSEC);

        ColumnInfo cols[NUM_COLUMNS];
        makeTable(cols);
        for (int i = 0; i < NUM_COLUMNS; i++)
            fprintf(fp, ", %*s", cols[i].width, cols[i].str);
        fprintf(fp, "\n");
    } else {
        fprintf(fp, "%f %f %f\n",
                t(PHASE_GC), t(PHASE_MARK), t(PHASE_SWEEP));
    }
    fflush(fp);
}

void
Statistics::endGC()
{
    Probes::GCEnd(compartment);
    endPhase(PHASE_GC);
    crash::SnapshotGCStack();

    for (int i = 0; i < PHASE_LIMIT; i++)
        totals[i] += phaseTimes[i];

    if (JSAccumulateTelemetryDataCallback cb = runtime->telemetryCallback) {
        (*cb)(JS_TELEMETRY_GC_REASON, triggerReason);
        (*cb)(JS_TELEMETRY_GC_IS_COMPARTMENTAL, compartment ? 1 : 0);
        (*cb)(JS_TELEMETRY_GC_IS_SHAPE_REGEN,
              runtime->shapeGen & SHAPE_OVERFLOW_BIT ? 1 : 0);
        (*cb)(JS_TELEMETRY_GC_MS, t(PHASE_GC));
        (*cb)(JS_TELEMETRY_GC_MARK_MS, t(PHASE_MARK));
        (*cb)(JS_TELEMETRY_GC_SWEEP_MS, t(PHASE_SWEEP));
    }

    if (JSGCFinishedCallback cb = runtime->gcFinishedCallback) {
        char buffer[1024];
        statsToString(buffer, sizeof(buffer));
        (*cb)(runtime, compartment, buffer);
    }

    if (fp)
        printStats();

    PodArrayZero(counts);
}

void
Statistics::beginPhase(Phase phase)
{
    phaseStarts[phase] = PRMJ_Now();

    if (phase == gcstats::PHASE_SWEEP) {
        Probes::GCStartSweepPhase(NULL);
        if (!compartment) {
            for (JSCompartment **c = runtime->compartments.begin();
                 c != runtime->compartments.end(); ++c)
            {
                Probes::GCStartSweepPhase(*c);
            }
        }
    }
}

void
Statistics::endPhase(Phase phase)
{
    phaseEnds[phase] = PRMJ_Now();
    phaseTimes[phase] += phaseEnds[phase] - phaseStarts[phase];

    if (phase == gcstats::PHASE_SWEEP) {
        if (!compartment) {
            for (JSCompartment **c = runtime->compartments.begin();
                 c != runtime->compartments.end(); ++c)
            {
                Probes::GCEndSweepPhase(*c);
            }
        }
        Probes::GCEndSweepPhase(NULL);
    }
}

} /* namespace gcstats */
} /* namespace js */
