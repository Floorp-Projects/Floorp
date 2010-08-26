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

using namespace js;

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
#undef ULSTAT
}
#endif

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

    for (ConservativeRoot *i = conservativeRoots.begin();
         i != conservativeRoots.end();
         ++i) {
        fprintf(fp, "  %p: ", i->thing);
        switch (i->traceKind) {
          default:
            JS_NOT_REACHED("Unknown trace kind");

          case JSTRACE_OBJECT: {
            JSObject *obj = (JSObject *) i->thing;
            fprintf(fp, "object %s", obj->getClass()->name);
            break;
          }
          case JSTRACE_STRING: {
            JSString *str = (JSString *) i->thing;
            char buf[50];
            js_PutEscapedString(buf, sizeof buf, str, '"');
            fprintf(fp, "string %s", buf);
            break;
          }
# if JS_HAS_XML_SUPPORT
          case JSTRACE_XML: {
            JSXML *xml = (JSXML *) i->thing;
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

#ifdef JS_GCMETER

void
UpdateArenaStats(JSGCArenaStats *st, uint32 nlivearenas, uint32 nkilledArenas,
                 uint32 nthings)
{
    size_t narenas;

    narenas = nlivearenas + nkilledArenas;
    JS_ASSERT(narenas >= st->livearenas);

    st->newarenas = narenas - st->livearenas;
    st->narenas = narenas;
    st->livearenas = nlivearenas;
    if (st->maxarenas < narenas)
        st->maxarenas = narenas;
    st->totalarenas += narenas;

    st->nthings = nthings;
    if (st->maxthings < nthings)
        st->maxthings = nthings;
    st->totalthings += nthings;
}

JS_FRIEND_API(void)
js_DumpGCStats(JSRuntime *rt, FILE *fp)
{
    static const char *const GC_ARENA_NAMES[] = {
        "object",
        "function",
#if JS_HAS_XML_SUPPORT
        "xml",
#endif
        "short string",
        "string",
        "external_string_0",
        "external_string_1",
        "external_string_2",
        "external_string_3",
        "external_string_4",
        "external_string_5",
        "external_string_6",
        "external_string_7",
    };

    fprintf(fp, "\nGC allocation statistics:\n\n");

#define UL(x)       ((unsigned long)(x))
#define ULSTAT(x)   UL(rt->gcStats.x)
#define PERCENT(x,y)  (100.0 * (double) (x) / (double) (y))

    size_t sumArenas = 0;
    size_t sumTotalArenas = 0;
    size_t sumThings = 0;
    size_t sumMaxThings = 0;
    size_t sumThingSize = 0;
    size_t sumTotalThingSize = 0;
    size_t sumArenaCapacity = 0;
    size_t sumTotalArenaCapacity = 0;
    size_t sumAlloc = 0;
    size_t sumLocalAlloc = 0;
    size_t sumFail = 0;
    size_t sumRetry = 0;
    for (int i = 0; i < (int) FINALIZE_LIMIT; i++) {
        size_t thingSize, thingsPerArena;
        JSGCArenaStats *st;
        thingSize = rt->gcArenaList[i].thingSize;
        thingsPerArena = ThingsPerArena(thingSize);
        st = &rt->gcArenaStats[i];
        if (st->maxarenas == 0)
            continue;
        fprintf(fp,
                "%s arenas (thing size %lu, %lu things per arena):",
                GC_ARENA_NAMES[i], UL(thingSize), UL(thingsPerArena));
        putc('\n', fp);
        fprintf(fp, "           arenas before GC: %lu\n", UL(st->narenas));
        fprintf(fp, "       new arenas before GC: %lu (%.1f%%)\n",
                UL(st->newarenas), PERCENT(st->newarenas, st->narenas));
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
        sumFail += st->fail;
        sumRetry += st->retry;
        putc('\n', fp);
    }

    fputs("Never used arenas:\n", fp);
    for (int i = 0; i < (int) FINALIZE_LIMIT; i++) {
        size_t thingSize, thingsPerArena;
        JSGCArenaStats *st;
        thingSize = rt->gcArenaList[i].thingSize;
        thingsPerArena = ThingsPerArena(thingSize);
        st = &rt->gcArenaStats[i];
        if (st->maxarenas != 0)
            continue;
        fprintf(fp,
                "%s (thing size %lu, %lu things per arena)\n",
                GC_ARENA_NAMES[i], UL(thingSize), UL(thingsPerArena));
    }
    fprintf(fp, "\nTOTAL STATS:\n");
    fprintf(fp, "            bytes allocated: %lu\n", UL(rt->gcBytes));
    fprintf(fp, "            total GC arenas: %lu\n", UL(sumArenas));
    fprintf(fp, "       max allocated arenas: %lu\n", ULSTAT(maxnallarenas));
    fprintf(fp, "       max allocated chunks: %lu\n", ULSTAT(maxnchunks));
    fprintf(fp, "            total GC things: %lu\n", UL(sumThings));
    fprintf(fp, "        max total GC things: %lu\n", UL(sumMaxThings));
    fprintf(fp, "        GC cell utilization: %.1f%%\n",
            PERCENT(sumThingSize, sumArenaCapacity));
    fprintf(fp, "   average cell utilization: %.1f%%\n",
            PERCENT(sumTotalThingSize, sumTotalArenaCapacity));
    fprintf(fp, "allocation retries after GC: %lu\n", UL(sumRetry));
    fprintf(fp, "             alloc attempts: %lu\n", UL(sumAlloc));
    fprintf(fp, "        alloc without locks: %lu  (%.1f%%)\n",
            UL(sumLocalAlloc), PERCENT(sumLocalAlloc, sumAlloc));
    fprintf(fp, "        allocation failures: %lu\n", UL(sumFail));
    fprintf(fp, "           valid lock calls: %lu\n", ULSTAT(lock));
    fprintf(fp, "         valid unlock calls: %lu\n", ULSTAT(unlock));
    fprintf(fp, "      delayed tracing calls: %lu\n", ULSTAT(unmarked));
#ifdef DEBUG
    fprintf(fp, "      max trace later count: %lu\n", ULSTAT(maxunmarked));
#endif
    fprintf(fp, "potentially useful GC calls: %lu\n", ULSTAT(poke));
    fprintf(fp, "  thing arenas freed so far: %lu\n", ULSTAT(afree));
    rt->gcStats.conservative.dump(fp);

#undef UL
#undef ULSTAT
#undef PERCENT
}
#endif

#ifdef MOZ_GCTIMER

namespace js {

jsrefcount newChunkCount = 0;
jsrefcount destroyChunkCount = 0;

GCTimer::GCTimer() {
    getFirstEnter();
    memset(this, 0, sizeof(GCTimer));
    enter = rdtsc();
}

uint64 
GCTimer::getFirstEnter() {
    static uint64 firstEnter = rdtsc();
    return firstEnter;
}

void 
GCTimer::finish(bool lastGC) {
    end = rdtsc();

    if (startMark > 0) {
        if (JS_WANT_GC_SUITE_PRINT) {
            fprintf(stderr, "%f %f %f\n",
                    (double)(end - enter) / 1e6,
                    (double)(startSweep - startMark) / 1e6,
                    (double)(sweepDestroyEnd - startSweep) / 1e6);
        } else {
            static FILE *gcFile;

            if (!gcFile) {
                gcFile = fopen("gcTimer.dat", "w");

                fprintf(gcFile, "     AppTime,  Total,   Mark,  Sweep, FinObj,");
                fprintf(gcFile, " FinStr,  Destroy,  newChunks, destoyChunks\n");
            }
            JS_ASSERT(gcFile);
            fprintf(gcFile, "%12.1f, %6.1f, %6.1f, %6.1f, %6.1f, %6.1f,  %7.1f, ",
                    (double)(enter - getFirstEnter()) / 1e6,
                    (double)(end - enter) / 1e6,
                    (double)(startSweep - startMark) / 1e6,
                    (double)(sweepDestroyEnd - startSweep) / 1e6,
                    (double)(sweepObjectEnd - startSweep) / 1e6,
                    (double)(sweepStringEnd - sweepObjectEnd) / 1e6,
                    (double)(sweepDestroyEnd - sweepStringEnd) / 1e6);
            fprintf(gcFile, "%10d, %10d \n", newChunkCount,
                    destroyChunkCount);
            fflush(gcFile);

            if (lastGC) {
                fclose(gcFile);
                gcFile = NULL;
            }
        }
    }
    newChunkCount = 0;
    destroyChunkCount = 0;
}

#ifdef JS_SCOPE_DEPTH_METER
void
DumpScopeDepthMeter(JSRuntime *rt)
{
    static FILE *fp;
    if (!fp)
        fp = fopen("/tmp/scopedepth.stats", "w");

    if (fp) {
        JS_DumpBasicStats(&rt->protoLookupDepthStats, "proto-lookup depth", fp);
        JS_DumpBasicStats(&rt->scopeSearchDepthStats, "scope-search depth", fp);
        JS_DumpBasicStats(&rt->hostenvScopeDepthStats, "hostenv scope depth", fp);
        JS_DumpBasicStats(&rt->lexicalScopeDepthStats, "lexical scope depth", fp);

        putc('\n', fp);
        fflush(fp);
    }
}
#endif

#ifdef JS_DUMP_LOOP_STATS
void
DumpLoopStats(JSRuntime *rt)
{
    static FILE *lsfp;
    if (!lsfp)
        lsfp = fopen("/tmp/loopstats", "w");
    if (lsfp) {
        JS_DumpBasicStats(&rt->loopStats, "loops", lsfp);
        fflush(lsfp);
    }
}
#endif

} /* namespace js */
#endif
