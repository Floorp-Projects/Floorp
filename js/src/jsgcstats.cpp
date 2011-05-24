/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * June 30, 2010
 *
 * The Initial Developer of the Original Code is
 *   the Mozilla Corporation.
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

#include "jstypes.h"
#include "jscntxt.h"
#include "jsgcstats.h"
#include "jsgc.h"
#include "jsxml.h"
#include "jsbuiltins.h"
#include "jscompartment.h"

#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;

#define UL(x)       ((unsigned long)(x))
#define PERCENT(x,y)  (100.0 * (double) (x) / (double) (y))

namespace js {
namespace gc {

#if defined(JS_DUMP_CONSERVATIVE_GC_ROOTS) || defined(JS_GCMETER)

void
ConservativeGCStats::dump(FILE *fp)
{
    size_t words = 0;
    for (size_t i = 0; i != JS_ARRAY_LENGTH(counter); ++i)
        words += counter[i];
   
#define ULSTAT(x)       ((unsigned long)(x))
    fprintf(fp, "CONSERVATIVE STACK SCANNING:\n");
    fprintf(fp, "      number of stack words: %lu\n", ULSTAT(words));
    fprintf(fp, "      excluded, low bit set: %lu\n", ULSTAT(counter[CGCT_LOWBITSET]));
    fprintf(fp, "        not withing a chunk: %lu\n", ULSTAT(counter[CGCT_NOTCHUNK]));
    fprintf(fp, "     not within arena range: %lu\n", ULSTAT(counter[CGCT_NOTARENA]));
    fprintf(fp, "       points to free arena: %lu\n", ULSTAT(counter[CGCT_FREEARENA]));
    fprintf(fp, "        excluded, wrong tag: %lu\n", ULSTAT(counter[CGCT_WRONGTAG]));
    fprintf(fp, "         excluded, not live: %lu\n", ULSTAT(counter[CGCT_NOTLIVE]));
    fprintf(fp, "            valid GC things: %lu\n", ULSTAT(counter[CGCT_VALID]));
    fprintf(fp, "      valid but not aligned: %lu\n", ULSTAT(unaligned));
#undef ULSTAT
}
#endif

#ifdef JS_GCMETER
void
UpdateCompartmentGCStats(JSCompartment *comp, unsigned thingKind)
{
    JSGCArenaStats *compSt = &comp->arenas[thingKind].stats;
    JSGCArenaStats *globSt = &comp->rt->globalArenaStats[thingKind];
    JS_ASSERT(compSt->narenas >= compSt->livearenas);
    compSt->newarenas     = compSt->narenas - compSt->livearenas;
    if (compSt->maxarenas < compSt->narenas)
        compSt->maxarenas = compSt->narenas;
    compSt->totalarenas  += compSt->narenas;

    if (compSt->maxthings < compSt->nthings)
        compSt->maxthings = compSt->nthings;
    compSt->totalthings  += compSt->nthings;
    globSt->newarenas    += compSt->newarenas;
    globSt->narenas      += compSt->narenas;
    globSt->livearenas   += compSt->livearenas;
    globSt->totalarenas  += compSt->totalarenas;
    globSt->nthings      += compSt->nthings;
    globSt->totalthings  += compSt->totalthings;
    if (globSt->maxarenas < compSt->maxarenas)
        globSt->maxarenas = compSt->maxarenas;
    if (globSt->maxthings < compSt->maxthings)
        globSt->maxthings = compSt->maxthings;
}

void
UpdateAllCompartmentGCStats(JSCompartment *comp)
{
    /*
     * The stats for the list arenas scheduled for the background finalization
     * are updated after that finishes.
     */
    JS_ASSERT(comp->rt->gcRunning);
    for (unsigned i = 0; i != JS_ARRAY_LENGTH(comp->arenas); ++i) {
#ifdef JS_THREADSAFE
        if (comp->arenas[i].willBeFinalizedLater())
            continue;
#endif
        UpdateCompartmentGCStats(comp, i);
    }
}

static const char *const GC_ARENA_NAMES[] = {
    "object_0",
    "object_0_background",
    "object_2",
    "object_2_background",
    "object_4",
    "object_4_background",
    "object_8",
    "object_8_background",
    "object_12",
    "object_12_background",
    "object_16",
    "object_16_background",
    "function",
    "shape",
#if JS_HAS_XML_SUPPORT
    "xml",
#endif
    "short string",
    "string",
    "external_string",
};
JS_STATIC_ASSERT(JS_ARRAY_LENGTH(GC_ARENA_NAMES) == FINALIZE_LIMIT);

template <typename T>
static inline void
GetSizeAndThings(size_t &thingSize, size_t &thingsPerArena)
{
    thingSize = sizeof(T);
    thingsPerArena = Arena<T>::ThingsPerArena;
}

void GetSizeAndThingsPerArena(int thingKind, size_t &thingSize, size_t &thingsPerArena)
{
    switch (thingKind) {
        case FINALIZE_OBJECT0:
        case FINALIZE_OBJECT0_BACKGROUND:
            GetSizeAndThings<JSObject>(thingSize, thingsPerArena);
            break;
        case FINALIZE_OBJECT2:
        case FINALIZE_OBJECT2_BACKGROUND:
            GetSizeAndThings<JSObject_Slots2>(thingSize, thingsPerArena);
            break;
        case FINALIZE_OBJECT4:
        case FINALIZE_OBJECT4_BACKGROUND:
            GetSizeAndThings<JSObject_Slots4>(thingSize, thingsPerArena);
            break;
        case FINALIZE_OBJECT8:
        case FINALIZE_OBJECT8_BACKGROUND:
            GetSizeAndThings<JSObject_Slots8>(thingSize, thingsPerArena);
            break;
        case FINALIZE_OBJECT12:
        case FINALIZE_OBJECT12_BACKGROUND:
            GetSizeAndThings<JSObject_Slots12>(thingSize, thingsPerArena);
            break;
        case FINALIZE_OBJECT16:
        case FINALIZE_OBJECT16_BACKGROUND:
            GetSizeAndThings<JSObject_Slots16>(thingSize, thingsPerArena);
            break;
        case FINALIZE_EXTERNAL_STRING:
        case FINALIZE_STRING:
            GetSizeAndThings<JSString>(thingSize, thingsPerArena);
            break;
        case FINALIZE_SHORT_STRING:
            GetSizeAndThings<JSShortString>(thingSize, thingsPerArena);
            break;
        case FINALIZE_FUNCTION:
            GetSizeAndThings<JSFunction>(thingSize, thingsPerArena);
            break;
#if JS_HAS_XML_SUPPORT
        case FINALIZE_XML:
            GetSizeAndThings<JSXML>(thingSize, thingsPerArena);
            break;
#endif
        default:
            JS_NOT_REACHED("wrong kind");
    }
}

void
DumpArenaStats(JSGCArenaStats *stp, FILE *fp)
{
    size_t sumArenas = 0, sumTotalArenas = 0, sumThings =0,  sumMaxThings = 0;
    size_t sumThingSize = 0, sumTotalThingSize = 0, sumArenaCapacity = 0;
    size_t sumTotalArenaCapacity = 0, sumAlloc = 0, sumLocalAlloc = 0;

    for (int i = 0; i < (int) FINALIZE_LIMIT; i++) {
        JSGCArenaStats *st = &stp[i];
        if (st->maxarenas == 0)
            continue;
        size_t thingSize = 0, thingsPerArena = 0;
        GetSizeAndThingsPerArena(i, thingSize, thingsPerArena);

        fprintf(fp, "%s arenas (thing size %lu, %lu things per arena):\n",
                GC_ARENA_NAMES[i], UL(thingSize), UL(thingsPerArena));
        fprintf(fp, "           arenas before GC: %lu\n", UL(st->narenas));
        fprintf(fp, "            arenas after GC: %lu (%.1f%%)\n",
                UL(st->livearenas), PERCENT(st->livearenas, st->narenas));
        fprintf(fp, "                 max arenas: %lu\n", UL(st->maxarenas));
        fprintf(fp, "                     things: %lu\n", UL(st->nthings));
        fprintf(fp, "        GC cell utilization: %.1f%%\n",
                PERCENT(st->nthings, thingsPerArena * st->narenas));
        fprintf(fp, "   average cell utilization: %.1f%%\n",
                PERCENT(st->totalthings, thingsPerArena * st->totalarenas));
        fprintf(fp, "                 max things: %lu\n", UL(st->maxthings));
        fprintf(fp, "             alloc attempts: %lu\n", UL(st->alloc));
        fprintf(fp, "        alloc without locks: %lu  (%.1f%%)\n",
                UL(st->localalloc), PERCENT(st->localalloc, st->alloc));
        sumArenas += st->narenas;
        sumTotalArenas += st->totalarenas;
        sumThings += st->nthings;
        sumMaxThings += st->maxthings;
        sumThingSize += thingSize * st->nthings;
        sumTotalThingSize += size_t(thingSize * st->totalthings);
        sumArenaCapacity += thingSize * thingsPerArena * st->narenas;
        sumTotalArenaCapacity += thingSize * thingsPerArena * st->totalarenas;
        sumAlloc += st->alloc;
        sumLocalAlloc += st->localalloc;
        putc('\n', fp);
    }

    fputs("Never used arenas:\n", fp);
    for (int i = 0; i < (int) FINALIZE_LIMIT; i++) {
        JSGCArenaStats *st = &stp[i];
        if (st->maxarenas != 0)
            continue;
        fprintf(fp, "%s\n", GC_ARENA_NAMES[i]);
    }
    fprintf(fp, "\nTOTAL STATS:\n");
    fprintf(fp, "            total GC arenas: %lu\n", UL(sumArenas));
    fprintf(fp, "            total GC things: %lu\n", UL(sumThings));
    fprintf(fp, "        max total GC things: %lu\n", UL(sumMaxThings));
    fprintf(fp, "        GC cell utilization: %.1f%%\n",
            PERCENT(sumThingSize, sumArenaCapacity));
    fprintf(fp, "   average cell utilization: %.1f%%\n",
            PERCENT(sumTotalThingSize, sumTotalArenaCapacity));
    fprintf(fp, "             alloc attempts: %lu\n", UL(sumAlloc));
    fprintf(fp, "        alloc without locks: %lu  (%.1f%%)\n",
            UL(sumLocalAlloc), PERCENT(sumLocalAlloc, sumAlloc));
}

void
DumpCompartmentStats(JSCompartment *comp, FILE *fp)
{
    if (comp->rt->atomsCompartment == comp)
        fprintf(fp, "\n**** AtomsCompartment Allocation Statistics: %p ****\n\n", (void *) comp);
    else
        fprintf(fp, "\n**** Compartment Allocation Statistics: %p ****\n\n", (void *) comp);

    for (unsigned i = 0; i != FINALIZE_LIMIT; ++i)
        DumpArenaStats(&comp->arenas[i].stats, fp);
}

#endif

} //gc
} //js

#ifdef JSGC_TESTPILOT
typedef JSRuntime::GCData GCData;

JS_PUBLIC_API(bool)
JS_GetGCInfoEnabled(JSRuntime *rt)
{
    return rt->gcData.infoEnabled;
}

JS_PUBLIC_API(void)
JS_SetGCInfoEnabled(JSRuntime *rt, bool enabled)
{
    rt->gcData.infoEnabled = enabled;
}

JS_PUBLIC_API(JSGCInfo *)
JS_GCInfoFront(JSRuntime *rt)
{
    GCData &data = rt->gcData;
    JS_ASSERT(data.infoEnabled);
    if (!data.count)
        return NULL;

    return &data.info[data.start];
}

JS_PUBLIC_API(bool)
JS_GCInfoPopFront(JSRuntime *rt)
{
    GCData &data = rt->gcData;
    JS_ASSERT(data.infoEnabled);
    JS_ASSERT(data.count);

    if (data.count >= GCData::INFO_LIMIT) {
        data.count = data.start = 0;
        return true;
    }

    data.start = (data.start + 1) % GCData::INFO_LIMIT;
    data.count -= 1;
    return false;
}
#endif


#ifdef JS_GCMETER

JS_FRIEND_API(void)
js_DumpGCStats(JSRuntime *rt, FILE *fp)
{
#define ULSTAT(x)   UL(rt->gcStats.x)
    if (JS_WANT_GC_METER_PRINT) {
        fprintf(fp, "\n**** Global Arena Allocation Statistics: ****\n");
        DumpArenaStats(&rt->globalArenaStats[0], fp);
        fprintf(fp, "            bytes allocated: %lu\n", UL(rt->gcBytes));
        fprintf(fp, "        allocation failures: %lu\n", ULSTAT(fail));
        fprintf(fp, "         last ditch GC runs: %lu\n", ULSTAT(lastditch));
        fprintf(fp, "           valid lock calls: %lu\n", ULSTAT(lock));
        fprintf(fp, "         valid unlock calls: %lu\n", ULSTAT(unlock));
        fprintf(fp, "      delayed tracing calls: %lu\n", ULSTAT(unmarked));
#ifdef DEBUG
        fprintf(fp, "      max trace later count: %lu\n", ULSTAT(maxunmarked));
#endif
        fprintf(fp, "  thing arenas freed so far: %lu\n\n", ULSTAT(afree));
    }

    if (JS_WANT_GC_PER_COMPARTMENT_PRINT)
        for (JSCompartment **c = rt->compartments.begin(); c != rt->compartments.end(); ++c)
            DumpCompartmentStats(*c, fp);
    PodZero(&rt->globalArenaStats);
    if (JS_WANT_CONSERVATIVE_GC_PRINT)
        rt->gcStats.conservative.dump(fp);
#undef ULSTAT
}
#endif

namespace js {

#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
void
GCMarker::dumpConservativeRoots()
{
    if (!conservativeDumpFileName)
        return;

    FILE *fp;
    if (!strcmp(conservativeDumpFileName, "stdout")) {
        fp = stdout;
    } else if (!strcmp(conservativeDumpFileName, "stderr")) {
        fp = stderr;
    } else if (!(fp = fopen(conservativeDumpFileName, "aw"))) {
        fprintf(stderr,
                "Warning: cannot open %s to dump the conservative roots\n",
                conservativeDumpFileName);
        return;
    }

    conservativeStats.dump(fp);

    for (void **thingp = conservativeRoots.begin(); thingp != conservativeRoots.end(); ++thingp) {
        void *thing = thingp;
        fprintf(fp, "  %p: ", thing);
        
        switch (GetGCThingTraceKind(thing)) {
          default:
            JS_NOT_REACHED("Unknown trace kind");

          case JSTRACE_OBJECT: {
            JSObject *obj = (JSObject *) thing;
            fprintf(fp, "object %s", obj->getClass()->name);
            break;
          }
          case JSTRACE_SHAPE: {
            fprintf(fp, "shape");
            break;
          }
          case JSTRACE_STRING: {
            JSString *str = (JSString *) thing;
            if (str->isLinear()) {
                char buf[50];
                PutEscapedString(buf, sizeof buf, &str->asLinear(), '"');
                fprintf(fp, "string %s", buf);
            } else {
                fprintf(fp, "rope: length %d", (int)str->length());
            }
            break;
          }
# if JS_HAS_XML_SUPPORT
          case JSTRACE_XML: {
            JSXML *xml = (JSXML *) thing;
            fprintf(fp, "xml %u", (unsigned)xml->xml_class);
            break;
          }
# endif
        }
        fputc('\n', fp);
    }
    fputc('\n', fp);

    if (fp != stdout && fp != stderr)
        fclose(fp);
}
#endif /* JS_DUMP_CONSERVATIVE_GC_ROOTS */

#if defined(MOZ_GCTIMER) || defined(JSGC_TESTPILOT)

jsrefcount newChunkCount = 0;
jsrefcount destroyChunkCount = 0;

GCTimer::GCTimer(JSRuntime *rt, JSCompartment *comp)
  : rt(rt), isCompartmental(comp),
    enabled(rt->gcData.isTimerEnabled())
{
    clearTimestamps();
    getFirstEnter();
    enter = PRMJ_Now();
}

uint64
GCTimer::getFirstEnter()
{
    JSRuntime::GCData &data = rt->gcData;
    if (enabled && !data.firstEnterValid)
        data.setFirstEnter(PRMJ_Now());

    return data.firstEnter;
}

#define TIMEDIFF(start, end) ((double)(end - start) / PRMJ_USEC_PER_MSEC)

void
GCTimer::finish(bool lastGC)
{
#if defined(JSGC_TESTPILOT)
    if (!enabled) {
        newChunkCount = 0;
        destroyChunkCount = 0;
        return;
    }
#endif
    end = PRMJ_Now();

    if (startMark > 0) {
        double appTime = TIMEDIFF(getFirstEnter(), enter);
        double gcTime = TIMEDIFF(enter, end);
        double waitTime = TIMEDIFF(enter, startMark);
        double markTime = TIMEDIFF(startMark, startSweep);
        double sweepTime = TIMEDIFF(startSweep, sweepDestroyEnd);
        double sweepObjTime = TIMEDIFF(startSweep, sweepObjectEnd);
        double sweepStringTime = TIMEDIFF(sweepObjectEnd, sweepStringEnd);
        double sweepShapeTime = TIMEDIFF(sweepStringEnd, sweepShapeEnd);
        double destroyTime = TIMEDIFF(sweepShapeEnd, sweepDestroyEnd);
        double endTime = TIMEDIFF(sweepDestroyEnd, end);

#if defined(JSGC_TESTPILOT)
        GCData &data = rt->gcData;
        size_t oldLimit = (data.start + data.count) % GCData::INFO_LIMIT;
        data.count += 1;

        JSGCInfo &info = data.info[oldLimit];
        info.appTime = appTime;
        info.gcTime = gcTime;
        info.waitTime = waitTime;
        info.markTime = markTime;
        info.sweepTime = sweepTime;
        info.sweepObjTime = sweepObjTime;
        info.sweepStringTime = sweepStringTime;
        info.sweepShapeTime = sweepShapeTime;
        info.destroyTime = destroyTime;
        info.endTime = endTime;
        info.isCompartmental = isCompartmental;
#endif

#if defined(MOZ_GCTIMER)
        if (JS_WANT_GC_SUITE_PRINT) {
            fprintf(stderr, "%f %f %f\n",
                    TIMEDIFF(enter, end),
                    TIMEDIFF(startMark, startSweep),
                    TIMEDIFF(startSweep, sweepDestroyEnd));
        } else {
            static FILE *gcFile;

            if (!gcFile) {
                gcFile = fopen("gcTimer.dat", "a");

                fprintf(gcFile, "     AppTime,  Total,   Wait,   Mark,  Sweep, FinObj,"
                                " FinStr, SwShapes, Destroy,    End, +Chu, -Chu\n");
            }
            JS_ASSERT(gcFile);
            /*               App   , Tot  , Wai  , Mar  , Swe  , FiO  , FiS  , SwS  , Des   , End */
            fprintf(gcFile, "%12.0f, %6.1f, %6.1f, %6.1f, %6.1f, %6.1f, %6.1f, %8.1f,  %6.1f, %6.1f, ",
                    appTime, gcTime, waitTime, markTime, sweepTime, sweepObjTime, sweepStringTime,
                    sweepShapeTime, destroyTime, endTime);
            fprintf(gcFile, "%4d, %4d\n", newChunkCount, destroyChunkCount);
            fflush(gcFile);

            if (lastGC) {
                fclose(gcFile);
                gcFile = NULL;
            }
        }
#endif
    }
    newChunkCount = 0;
    destroyChunkCount = 0;
}

#undef TIMEDIFF

#endif

} //js

#undef UL
#undef PERCENT
