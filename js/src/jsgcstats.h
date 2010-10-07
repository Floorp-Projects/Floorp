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

/* Define JS_GCMETER here if wanted */
#if defined JS_GCMETER
const bool JS_WANT_GC_METER_PRINT = true;
const bool JS_WANT_GC_PER_COMPARTMENT_PRINT = true;
const bool JS_WANT_CONSERVATIVE_GC_PRINT = true;
#elif defined DEBUG
# define JS_GCMETER 1
const bool JS_WANT_GC_METER_PRINT = false;
const bool JS_WANT_GC_PER_COMPARTMENT_PRINT = false;
const bool JS_WANT_CONSERVATIVE_GC_PRINT = false;
#endif

namespace js {
namespace gc {
/*
 * The conservative GC test for a word shows that it is either a valid GC
 * thing or is not for one of the following reasons.
 */
enum ConservativeGCTest {
    CGCT_VALID,
    CGCT_LOWBITSET, /* excluded because one of the low bits was set */
    CGCT_NOTARENA,  /* not within arena range in a chunk */
    CGCT_NOTCHUNK,  /* not within a valid chunk */
    CGCT_FREEARENA, /* within arena containing only free things */
    CGCT_WRONGTAG,  /* tagged pointer but wrong type */
    CGCT_NOTLIVE,   /* gcthing is not allocated */
    CGCT_END
};

struct ConservativeGCStats {
    uint32  counter[gc::CGCT_END];  /* ConservativeGCTest classification
                                   counters */

    void add(const ConservativeGCStats &another) {
        for (size_t i = 0; i != JS_ARRAY_LENGTH(counter); ++i)
            counter[i] += another.counter[i];
    }

    void dump(FILE *fp);
};

#ifdef JS_GCMETER
struct JSGCArenaStats {
    uint32  alloc;          /* allocation attempts */
    uint32  localalloc;     /* allocations from local lists */
    uint32  nthings;        /* live GC things */
    uint32  maxthings;      /* maximum of live GC cells */
    double  totalthings;    /* live GC things the GC scanned so far */
    uint32  narenas;        /* number of arena in list before the GC */
    uint32  newarenas;      /* new arenas allocated before the last GC */
    uint32  livearenas;     /* number of live arenas after the last GC */
    uint32  maxarenas;      /* maximum of allocated arenas */
    uint32  totalarenas;    /* total number of arenas with live things that
                               GC scanned so far */
};
#endif

#ifdef JS_GCMETER

struct JSGCStats {
    uint32  lock;       /* valid lock calls */
    uint32  unlock;     /* valid unlock calls */
    uint32  unmarked;   /* number of times marking of GC thing's children were
                           delayed due to a low C stack */
    uint32  retry;      /* allocation retries after running the GC */
    uint32  fail;       /* allocation failures */
#ifdef DEBUG
    uint32  maxunmarked;/* maximum number of things with children to mark
                           later */
#endif
    uint32  poke;           /* number of potentially useful GC calls */
    uint32  afree;          /* thing arenas freed so far */
    uint32  nallarenas;     /* number of all allocated arenas */
    uint32  maxnallarenas;  /* maximum number of all allocated arenas */
    uint32  nchunks;        /* number of allocated chunks */
    uint32  maxnchunks;     /* maximum number of allocated chunks */

    ConservativeGCStats conservative;
};

extern void
UpdateCompartmentStats(JSCompartment *comp, unsigned thingKind, uint32 nlivearenas,
                       uint32 nkilledArenas, uint32 nthings);
#endif /* JS_GCMETER */

} //gc

#ifdef MOZ_GCTIMER

const bool JS_WANT_GC_SUITE_PRINT = false;  //false for gnuplot output

extern jsrefcount newChunkCount;
extern jsrefcount destroyChunkCount;

struct GCTimer {
    uint64 enter;
    uint64 startMark;
    uint64 startSweep;
    uint64 sweepObjectEnd;
    uint64 sweepStringEnd;
    uint64 sweepDestroyEnd;
    uint64 end;

    GCTimer();

    uint64 getFirstEnter();

    void finish(bool lastGC);
};

# define GCTIMER_PARAM      , GCTimer &gcTimer
# define GCTIMER_ARG        , gcTimer
# define TIMESTAMP(x)       (gcTimer.x = rdtsc())
# define GCTIMER_BEGIN()    GCTimer gcTimer
# define GCTIMER_END(last)  (gcTimer.finish(last))
#else
# define GCTIMER_PARAM
# define GCTIMER_ARG
# define TIMESTAMP(x)       ((void) 0)
# define GCTIMER_BEGIN()    ((void) 0)
# define GCTIMER_END(last)  ((void) 0)
#endif

} //js

extern JS_FRIEND_API(void)
js_DumpGCStats(JSRuntime *rt, FILE *fp);

#endif /* jsgcstats_h__ */
