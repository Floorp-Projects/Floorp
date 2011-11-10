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

#ifndef jsgc_statistics_h___
#define jsgc_statistics_h___

#include <string.h>

#include "jspubtd.h"
#include "jsutil.h"

struct JSCompartment;

namespace js {
namespace gcstats {

enum Reason {
    PUBLIC_API,
    MAYBEGC,
    LASTCONTEXT,
    DESTROYCONTEXT,
    LASTDITCH,
    TOOMUCHMALLOC,
    ALLOCTRIGGER,
    CHUNK,
    SHAPE,
    REFILL
};
static const int NUM_REASONS = REFILL + 1;

static inline const char *
ExplainReason(Reason r)
{
    static const char *strs[] = {"  API", "Maybe", "LastC", "DestC", "LastD",
                                 "Mallc", "Alloc", "Chunk", "Shape", "Refil"};

    JS_ASSERT(strcmp(strs[SHAPE], "Shape") == 0 &&
              sizeof(strs) / sizeof(strs[0]) == NUM_REASONS);

    return strs[r];
}

enum Phase {
    PHASE_GC,
    PHASE_MARK,
    PHASE_SWEEP,
    PHASE_SWEEP_OBJECT,
    PHASE_SWEEP_STRING,
    PHASE_SWEEP_SCRIPT,
    PHASE_SWEEP_SHAPE,
    PHASE_DISCARD_CODE,
    PHASE_DISCARD_ANALYSIS,
    PHASE_XPCONNECT,
    PHASE_DESTROY,

    PHASE_LIMIT
};

enum Stat {
    STAT_NEW_CHUNK,
    STAT_DESTROY_CHUNK,

    STAT_LIMIT
};

struct Statistics {
    Statistics(JSRuntime *rt);
    ~Statistics();

    void beginGC(JSCompartment *comp, Reason reason);
    void endGC();

    void beginPhase(Phase phase);
    void endPhase(Phase phase);

    void count(Stat s) {
        JS_ASSERT(s < STAT_LIMIT);
        counts[s]++;
    }

  private:
    JSRuntime *runtime;

    uint64 startupTime;

    FILE *fp;
    bool fullFormat;

    Reason triggerReason;
    JSCompartment *compartment;

    uint64 phaseStarts[PHASE_LIMIT];
    uint64 phaseEnds[PHASE_LIMIT];
    uint64 phaseTimes[PHASE_LIMIT];
    uint64 totals[PHASE_LIMIT];
    unsigned int counts[STAT_LIMIT];

    double t(Phase phase);
    double total(Phase phase);
    double beginDelay(Phase phase1, Phase phase2);
    double endDelay(Phase phase1, Phase phase2);
    void printStats();
    void statsToString(char *buffer, size_t size);

    struct ColumnInfo {
        const char *title;
        char str[12];
        char totalStr[12];
        int width;

        ColumnInfo() {}
        ColumnInfo(const char *title, double t, double total);
        ColumnInfo(const char *title, double t);
        ColumnInfo(const char *title, unsigned int data);
        ColumnInfo(const char *title, const char *data);
    };

    void makeTable(ColumnInfo *cols);
};

struct AutoGC {
    AutoGC(Statistics &stats, JSCompartment *comp, Reason reason JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : stats(stats) { JS_GUARD_OBJECT_NOTIFIER_INIT; stats.beginGC(comp, reason); }
    ~AutoGC() { stats.endGC(); }

    Statistics &stats;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

struct AutoPhase {
    AutoPhase(Statistics &stats, Phase phase JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : stats(stats), phase(phase) { JS_GUARD_OBJECT_NOTIFIER_INIT; stats.beginPhase(phase); }
    ~AutoPhase() { stats.endPhase(phase); }

    Statistics &stats;
    Phase phase;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} /* namespace gcstats */
} /* namespace js */

#endif /* jsgc_statistics_h___ */
