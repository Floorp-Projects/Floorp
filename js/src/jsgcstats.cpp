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

#if defined(JS_DUMP_CONSERVATIVE_GC_ROOTS)

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
          case JSTRACE_OBJECT: {
            JSObject *obj = (JSObject *) thing;
            fprintf(fp, "object %s", obj->getClass()->name);
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
          case JSTRACE_SCRIPT: {
            fprintf(fp, "shape");
            break;
          }
          case JSTRACE_SHAPE: {
            fprintf(fp, "shape");
            break;
          }
          case JSTRACE_TYPE_OBJECT: {
            fprintf(fp, "type_object");
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

volatile GCTimer::JSGCReason gcReason = GCTimer::NOREASON;
const char *gcReasons[] = {"  API", "Maybe", "LastC", "DestC", "Compa", "LastD",
                          "Malloc", "Alloc", "Chunk", "Shape", "  None"};

jsrefcount newChunkCount = 0;
jsrefcount destroyChunkCount = 0;

#ifdef MOZ_GCTIMER
static const char *gcTimerStatPath = NULL;
#endif

GCTimer::GCTimer(JSRuntime *rt, JSCompartment *comp)
  : rt(rt), isCompartmental(comp),
    enabled(rt->gcData.isTimerEnabled())
{
#ifdef MOZ_GCTIMER
    if (!gcTimerStatPath) {
        gcTimerStatPath = getenv("MOZ_GCTIMER");
        if (!gcTimerStatPath || !gcTimerStatPath[0])
            gcTimerStatPath = "gcTimer.dat";
    }
    if (!strcmp(gcTimerStatPath, "none"))
        enabled = false;
#endif
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
        double sweepScriptTime = TIMEDIFF(sweepStringEnd, sweepScriptEnd);
        double sweepShapeTime = TIMEDIFF(sweepScriptEnd, sweepShapeEnd);
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
        info.sweepScriptTime = sweepScriptTime;
        info.sweepShapeTime = sweepShapeTime;
        info.destroyTime = destroyTime;
        info.endTime = endTime;
        info.isCompartmental = isCompartmental;
#endif

#if defined(MOZ_GCTIMER)
        static FILE *gcFile;
        static bool fullFormat;

        if (!gcFile) {
            if (!strcmp(gcTimerStatPath, "stdout")) {
                gcFile = stdout;
                fullFormat = false;
            } else if (!strcmp(gcTimerStatPath, "stderr")) {
                gcFile = stderr;
                fullFormat = false;
            } else {
                gcFile = fopen(gcTimerStatPath, "a");
                JS_ASSERT(gcFile);
                fullFormat = true;
                fprintf(gcFile, "     AppTime,  Total,   Wait,   Mark,  Sweep, FinObj,"
                        " FinStr, SwScripts, SwShapes, Destroy,    End, +Chu, -Chu, T, Reason\n");
            }
        }

        if (!fullFormat) {
            fprintf(stderr, "%f %f %f\n",
                    TIMEDIFF(enter, end),
                    TIMEDIFF(startMark, startSweep),
                    TIMEDIFF(startSweep, sweepDestroyEnd));
        } else {
            /*               App   , Tot  , Wai  , Mar  , Swe  , FiO  , FiS  , SwScr , SwS  , Des   , End */
            fprintf(gcFile, "%12.0f, %6.1f, %6.1f, %6.1f, %6.1f, %6.1f, %6.1f, %6.1f, %8.1f,  %6.1f, %6.1f, ",
                    appTime, gcTime, waitTime, markTime, sweepTime, sweepObjTime, sweepStringTime,
                    sweepScriptTime, sweepShapeTime, destroyTime, endTime);
            fprintf(gcFile, "%4d, %4d,", newChunkCount, destroyChunkCount);
            fprintf(gcFile, " %s, %s\n", isCompartmental ? "C" : "G", gcReasons[gcReason]);
        }
        fflush(gcFile);
        
        if (lastGC && gcFile != stdout && gcFile != stderr)
            fclose(gcFile);
#endif
    }
    newChunkCount = 0;
    destroyChunkCount = 0;
    gcReason = NOREASON;
}

#undef TIMEDIFF

#endif

} //js

#undef UL
#undef PERCENT
