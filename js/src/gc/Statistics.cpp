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
#include "jsprobes.h"
#include "jsutil.h"
#include "jscrashformat.h"
#include "jscrashreport.h"
#include "prmjtime.h"

#include "gc/Statistics.h"

namespace js {
namespace gcstats {

Statistics::Statistics(JSRuntime *rt)
  : runtime(rt)
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

        fprintf(fp, "     AppTime,  Total,   Wait,   Mark,  Sweep, FinObj,"
                " FinStr, FinScr, FinShp, Destry,    End, +Chu, -Chu, T, Reason\n");
    }

    PodArrayZero(counts);

    startupTime = PRMJ_Now();
}

Statistics::~Statistics()
{
    if (fp && fp != stdout && fp != stderr)
        fclose(fp);
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
Statistics::printStats()
{
    if (fullFormat) {
        /*       App   , Total, Wait , Mark , Sweep, FinOb, FinSt, FinSc, FinSh, Destry, End */
        fprintf(fp,
                "%12.0f, %6.1f, %6.1f, %6.1f, %6.1f, %6.1f, %6.1f, %6.1f, %6.1f, %6.1f, %6.1f, ",
                double(phaseStarts[PHASE_GC] - startupTime) / PRMJ_USEC_PER_MSEC,
                t(PHASE_GC),
                beginDelay(PHASE_MARK, PHASE_GC),
                t(PHASE_MARK), t(PHASE_SWEEP),
                t(PHASE_SWEEP_OBJECT), t(PHASE_SWEEP_STRING),
                t(PHASE_SWEEP_SCRIPT), t(PHASE_SWEEP_SHAPE),
                t(PHASE_DESTROY),
                endDelay(PHASE_GC, PHASE_DESTROY));

        fprintf(fp, "%4d, %4d,", counts[STAT_NEW_CHUNK], counts[STAT_DESTROY_CHUNK]);
        fprintf(fp, " %s, %s\n", compartment ? "C" : "G", ExplainReason(triggerReason));
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
