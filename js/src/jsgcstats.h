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

#ifndef jsgcstats_h___
#define jsgcstats_h___

#if !defined JS_DUMP_CONSERVATIVE_GC_ROOTS && defined DEBUG
# define JS_DUMP_CONSERVATIVE_GC_ROOTS 1
#endif

#ifdef JSGC_TESTPILOT
JS_BEGIN_EXTERN_C

struct JSGCInfo
{
    double appTime, gcTime, waitTime, markTime, sweepTime;
    double sweepObjTime, sweepStringTime, sweepScriptTime, sweepShapeTime;
    double destroyTime, endTime;
    bool isCompartmental;
};

extern JS_PUBLIC_API(void)
JS_SetGCInfoEnabled(JSRuntime *rt, bool enabled);

extern JS_PUBLIC_API(bool)
JS_GetGCInfoEnabled(JSRuntime *rt);

/*
 * Data in the circular buffer may end up clobbered before the API client
 * consumes it. Because of this we have a multi-part API. The client uses code
 * like the following:
 *
 * - Call GetInfo, which provides an info pointer.
 * - Read data out of the info pointer to a location the client owns.
 * - Call PopInfo, which provides a "did info get dropped?" value. If that
 *   value is true, the data read out of the info pointer may be tainted, and
 *   must be thrown out. Otherwise, the data was definitely safe to read, and
 *   may be committed to a database or some such.
 *
 * When PopInfo indicates that data has been dropped, all of the information in
 * the circular buffer is reset.
 */

extern JS_PUBLIC_API(JSGCInfo *)
JS_GCInfoFront(JSRuntime *rt);

/* Return whether info has dropped. See comment above. */
extern JS_PUBLIC_API(bool)
JS_GCInfoPopFront(JSRuntime *rt);

JS_END_EXTERN_C
#endif

namespace js {
namespace gc {
/*
 * The conservative GC test for a word shows that it is either a valid GC
 * thing or is not for one of the following reasons.
 */
enum ConservativeGCTest
{
    CGCT_VALID,
    CGCT_LOWBITSET, /* excluded because one of the low bits was set */
    CGCT_NOTARENA,  /* not within arena range in a chunk */
    CGCT_NOTCHUNK,  /* not within a valid chunk */
    CGCT_FREEARENA, /* within arena containing only free things */
    CGCT_NOTLIVE,   /* gcthing is not allocated */
    CGCT_END
};

struct ConservativeGCStats
{
    uint32  counter[gc::CGCT_END];  /* ConservativeGCTest classification
                                       counters */
    uint32  unaligned;              /* number of valid but not aligned on
                                       thing start pointers */ 

    void add(const ConservativeGCStats &another) {
        for (size_t i = 0; i != JS_ARRAY_LENGTH(counter); ++i)
            counter[i] += another.counter[i];
    }

    void dump(FILE *fp);
};

} //gc

#if defined(MOZ_GCTIMER) || defined(JSGC_TESTPILOT)

extern jsrefcount newChunkCount;
extern jsrefcount destroyChunkCount;

struct GCTimer
{
    JSRuntime *rt;

    uint64 enter;
    uint64 startMark;
    uint64 startSweep;
    uint64 sweepObjectEnd;
    uint64 sweepStringEnd;
    uint64 sweepScriptEnd;
    uint64 sweepShapeEnd;
    uint64 sweepDestroyEnd;
    uint64 sweepIonCodeEnd;
    uint64 end;

    bool isCompartmental;
    bool enabled; /* Disabled timers should cause no PRMJ calls. */

    GCTimer(JSRuntime *rt, JSCompartment *comp);

    uint64 getFirstEnter();

    void clearTimestamps() {
        memset(&enter, 0, &end - &enter + sizeof(end));
    }

    void finish(bool lastGC);

    enum JSGCReason {
        PUBLIC_API,
        MAYBEGC,
        LASTCONTEXT,
        DESTROYCONTEXT,
        COMPARTMENT,
        LASTDITCH,
        TOOMUCHMALLOC,
        ALLOCTRIGGER,
        REFILL,
        SHAPE,
        NOREASON
    };
};

/* We accept the possiblility of races for this variable. */
extern volatile GCTimer::JSGCReason gcReason;

#define GCREASON(x) ((gcReason == GCTimer::NOREASON) ? gcReason = GCTimer::x : gcReason = gcReason)

# define GCTIMER_PARAM              , GCTimer &gcTimer
# define GCTIMER_ARG                , gcTimer
# define GCTIMESTAMP(stamp_name_) \
    JS_BEGIN_MACRO \
        if (gcTimer.enabled) \
            gcTimer.stamp_name_ = PRMJ_Now(); \
    JS_END_MACRO
# define GCTIMER_BEGIN(rt, comp)    GCTimer gcTimer(rt, comp)
# define GCTIMER_END(last)          (gcTimer.finish(last))
#else
# define GCREASON(x)                ((void) 0)
# define GCTIMER_PARAM
# define GCTIMER_ARG
# define GCTIMESTAMP(x)             ((void) 0)
# define GCTIMER_BEGIN(rt, comp)    ((void) 0)
# define GCTIMER_END(last)          ((void) 0)
#endif

} //js

extern JS_FRIEND_API(void)
js_DumpGCStats(JSRuntime *rt, FILE *fp);

#endif /* jsgcstats_h__ */
