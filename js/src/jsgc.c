/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

/*
 * JS Mark-and-Sweep Garbage Collector.
 *
 * This GC allocates only fixed-sized things big enough to contain two words
 * (pointers) on any host architecture.  It allocates from an arena pool (see
 * jsarena.h).  It uses an ideally parallel array of flag bytes to hold the
 * mark bit, finalizer type index, etc.
 *
 * XXX swizzle page to freelist for better locality of reference
 */
#include "jsstddef.h"
#include <stdlib.h>     /* for free, called by JS_ARENA_DESTROY */
#include <string.h>     /* for memset, called by jsarena.h macros if DEBUG */
#include "jstypes.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jshash.h" /* Added by JSIFY */
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsdbgapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

/*
 * GC arena sizing depends on amortizing arena overhead using a large number
 * of things per arena, and on the thing/flags ratio of 8:1 on most platforms.
 *
 * On 64-bit platforms, we would have half as many things per arena because
 * pointers are twice as big, so we double the bytes for things per arena.
 * This preserves the 1024 byte flags sub-arena size, which relates to the
 * GC_PAGE_SIZE (see below for why).
 */
#if JS_BYTES_PER_WORD == 8
# define GC_THINGS_SHIFT 14     /* 16KB for things on Alpha, etc. */
#else
# define GC_THINGS_SHIFT 13     /* 8KB for things on most platforms */
#endif
#define GC_THINGS_SIZE  JS_BIT(GC_THINGS_SHIFT)
#define GC_FLAGS_SIZE   (GC_THINGS_SIZE / sizeof(JSGCThing))
#define GC_ARENA_SIZE   (GC_THINGS_SIZE + GC_FLAGS_SIZE)

/*
 * A GC arena contains one flag byte for each thing in its heap, and supports
 * O(1) lookup of a flag given its thing's address.
 *
 * To implement this, we take advantage of the thing/flags numerology: given
 * the 8K bytes worth of GC-things, there are 1K flag bytes.  We mask a thing's
 * address with ~1023 to find a JSGCPageInfo record at the front of a mythical
 * "GC page" within the larger 8K thing arena.  That JSGCPageInfo contains a
 * pointer to the 128 flag bytes corresponding to the things in the page, so we
 * index into this flags array using the thing's index within its page.
 *
 * To align thing pages on 1024-byte boundaries, we must allocate the 9KB of
 * flags+things arena payload, then find the first 0 mod 1024 boundary after
 * the first payload address.  That's where things start, with a JSGCPageInfo
 * taking up the first thing-slot, as usual for 0 mod 1024 byte boundaries.
 * The effect of this alignment trick is to split the flags into at most 2
 * discontiguous spans, one before the things and one after (if we're really
 * lucky, and the arena payload starts on a 0 mod 1024 byte boundary, no need
 * to split).
 *
 * The overhead of this scheme for most platforms is (16+8*(8+1))/(16+9K) or
 * .95% (assuming 16 byte JSArena header size, and 8 byte JSGCThing size).
 *
 * Here's some ASCII art showing an arena:
 *
 *   split
 *     |
 *     V
 *  +--+-------+-------+-------+-------+-------+-------+-------+-------+-----+
 *  |fB|  tp0  |  tp1  |  tp2  |  tp3  |  tp4  |  tp5  |  tp6  |  tp7  | fA  |
 *  +--+-------+-------+-------+-------+-------+-------+-------+-------+-----+
 *              ^                                 ^
 *  tI ---------+                                 |
 *  tJ -------------------------------------------+
 *
 *  - fB are the "before split" flags, fA are the "after split" flags
 *  - tp0-tp7 are the 8 thing pages
 *  - thing tI points into tp1, whose flags are below the split, in fB
 *  - thing tJ points into tp5, clearly above the split
 *
 * In general, one of the thing pages will have some of its things' flags on
 * the low side of the split, and the rest of its things' flags on the high
 * side.  All the other pages have flags only below or only above.  Therefore
 * we'll have to test something to decide whether the split divides flags in
 * a given thing's page.  So we store the split pointer (the pointer to tp0)
 * in each JSGCPageInfo, along with the flags pointer for the 128 flag bytes
 * ideally starting, for tp0 things, at the beginning of the arena's payload
 * (at the start of fB).
 *
 * That is, each JSGCPageInfo's flags pointer is 128 bytes from the previous,
 * or at the start of the arena if there is no previous page in this arena.
 * Thus these ideal 128-byte flag pages run contiguously from the start of the
 * arena (right over the split!), and the JSGCPageInfo flags pointers contain
 * no discontinuities over the split created by the thing pages.  So if, for a
 * given JSGCPageInfo *pi, we find that
 *
 *  pi->flags + ((jsuword)thing % 1024) / sizeof(JSGCThing) >= pi->split
 *
 * then we must add GC_THINGS_SIZE to the nominal flags pointer to jump over
 * all the thing pages that split the flags into two discontiguous spans.
 *
 * (If we need to implement card-marking for an incremental GC write barrier,
 * we can use the low byte of the pi->split pointer as the card-mark, for an
 * extremely efficient write barrier: when mutating an object obj, just store
 * a 1 byte at (uint8 *) ((jsuword)obj & ~1023) for little-endian platforms.
 * When finding flags, we'll of course have to mask split with ~255, but it is
 * guaranteed to be 1024-byte aligned, so no information is lost by overlaying
 * the card-mark byte on split's low byte.)
 */
#define GC_PAGE_SHIFT   10
#define GC_PAGE_MASK    ((jsuword) JS_BITMASK(GC_PAGE_SHIFT))
#define GC_PAGE_SIZE    JS_BIT(GC_PAGE_SHIFT)

typedef struct JSGCPageInfo {
    uint8       *split;
    uint8       *flags;
} JSGCPageInfo;

#define FIRST_THING_PAGE(a)     (((a)->base + GC_FLAGS_SIZE) & ~GC_PAGE_MASK)

/*
 * Given a jsuword page pointer p and a thing size n, return the address of
 * the first thing in p.  We know that any n not a power of two packs from
 * the end of the page leaving at least enough room for one JSGCPageInfo, but
 * not for another thing, at the front of the page (JS_ASSERTs below insist
 * on this).
 *
 * This works because all allocations are a multiple of sizeof(JSGCThing) ==
 * sizeof(JSGCPageInfo) in size.
 */
#define FIRST_THING(p,n)        (((n) & ((n) - 1))                            \
                                 ? (p) + (uint32)(GC_PAGE_SIZE % (n))         \
                                 : (p) + (n))

static JSGCThing *
gc_new_arena(JSArenaPool *pool, size_t nbytes)
{
    uint8 *flagp, *split, *pagep, *limit;
    JSArena *a;
    jsuword p;
    JSGCThing *thing;
    JSGCPageInfo *pi;

    /* Use JS_ArenaAllocate to grab another 9K-net-size hunk of space. */
    flagp = (uint8 *) JS_ArenaAllocate(pool, GC_ARENA_SIZE);
    if (!flagp)
        return NULL;
    a = pool->current;

    /* Reset a->avail to start at the flags split, aka the first thing page. */
    p = FIRST_THING_PAGE(a);
    split = pagep = (uint8 *) p;
    a->avail = FIRST_THING(p, nbytes);
    JS_ASSERT(a->avail >= p + sizeof(JSGCPageInfo));
    thing = (JSGCThing *) a->avail;
    JS_ArenaCountAllocation(pool, a->avail - p);
    a->avail += nbytes;

    /* Initialize the JSGCPageInfo records at the start of every thing page. */
    limit = pagep + GC_THINGS_SIZE;
    do {
        pi = (JSGCPageInfo *) pagep;
        pi->split = split;
        pi->flags = flagp;
        flagp += GC_PAGE_SIZE >> (GC_THINGS_SHIFT -  GC_PAGE_SHIFT);
        pagep += GC_PAGE_SIZE;
    } while (pagep < limit);
    return thing;
}

uint8 *
js_GetGCThingFlags(void *thing)
{
    JSGCPageInfo *pi;
    uint8 *flagp;

    pi = (JSGCPageInfo *) ((jsuword)thing & ~GC_PAGE_MASK);
    flagp = pi->flags + ((jsuword)thing & GC_PAGE_MASK) / sizeof(JSGCThing);
    if (flagp >= pi->split)
        flagp += GC_THINGS_SIZE;
    return flagp;
}

JSBool
js_IsAboutToBeFinalized(JSContext *cx, void *thing)
{
    uint8 flags = *js_GetGCThingFlags(thing);

    return !(flags & (GCF_MARK | GCF_LOCK | GCF_FINAL));
}

typedef void (*GCFinalizeOp)(JSContext *cx, JSGCThing *thing);

#ifndef DEBUG
# define js_FinalizeDouble       NULL
#endif

#if !JS_HAS_XML_SUPPORT
# define js_FinalizeXMLNamespace NULL
# define js_FinalizeXMLQName     NULL
# define js_FinalizeXML          NULL
#endif

static GCFinalizeOp gc_finalizers[GCX_NTYPES] = {
    (GCFinalizeOp) js_FinalizeObject,           /* GCX_OBJECT */
    (GCFinalizeOp) js_FinalizeString,           /* GCX_STRING */
    (GCFinalizeOp) js_FinalizeDouble,           /* GCX_DOUBLE */
    (GCFinalizeOp) js_FinalizeString,           /* GCX_MUTABLE_STRING */
    NULL,                                       /* GCX_PRIVATE */
    (GCFinalizeOp) js_FinalizeXMLNamespace,     /* GCX_NAMESPACE */
    (GCFinalizeOp) js_FinalizeXMLQName,         /* GCX_QNAME */
    (GCFinalizeOp) js_FinalizeXML,              /* GCX_XML */
    NULL,                                       /* GCX_EXTERNAL_STRING */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

#ifdef GC_MARK_DEBUG
static const char newborn_external_string[] = "newborn external string";

static const char *gc_typenames[GCX_NTYPES] = {
    "newborn object",
    "newborn string",
    "newborn double",
    "newborn mutable string",
    "newborn private",
    "newborn Namespace",
    "newborn QName",
    "newborn XML",
    newborn_external_string,
    newborn_external_string,
    newborn_external_string,
    newborn_external_string,
    newborn_external_string,
    newborn_external_string,
    newborn_external_string,
    newborn_external_string
};
#endif

intN
js_ChangeExternalStringFinalizer(JSStringFinalizeOp oldop,
                                 JSStringFinalizeOp newop)
{
    uintN i;

    for (i = GCX_EXTERNAL_STRING; i < GCX_NTYPES; i++) {
        if (gc_finalizers[i] == (GCFinalizeOp) oldop) {
            gc_finalizers[i] = (GCFinalizeOp) newop;
            return (intN) i;
        }
    }
    return -1;
}

#ifdef JS_GCMETER
#define METER(x) x
#else
#define METER(x) /* nothing */
#endif

/* Initial size of the gcRootsHash table (SWAG, small enough to amortize). */
#define GC_ROOTS_SIZE   256
#define GC_FINALIZE_LEN 1024

JSBool
js_InitGC(JSRuntime *rt, uint32 maxbytes)
{
    uintN i;

    JS_ASSERT(sizeof(JSGCThing) == sizeof(JSGCPageInfo));
    JS_ASSERT(sizeof(JSGCThing) >= sizeof(JSObject));
    JS_ASSERT(sizeof(JSGCThing) >= sizeof(JSString));
    JS_ASSERT(sizeof(JSGCThing) >= sizeof(jsdouble));
    JS_ASSERT(GC_FLAGS_SIZE >= GC_PAGE_SIZE);
    JS_ASSERT(sizeof(JSStackHeader) >= 2 * sizeof(jsval));

    for (i = 0; i < GC_NUM_FREELISTS; i++) {
        JS_InitArenaPool(&rt->gcArenaPool[i], "gc-arena", GC_ARENA_SIZE,
                         GC_FREELIST_NBYTES(i));
    }
    if (!JS_DHashTableInit(&rt->gcRootsHash, JS_DHashGetStubOps(), NULL,
                           sizeof(JSGCRootHashEntry), GC_ROOTS_SIZE)) {
        rt->gcRootsHash.ops = NULL;
        return JS_FALSE;
    }
    rt->gcLocksHash = NULL;     /* create lazily */
    rt->gcMaxBytes = maxbytes;
    return JS_TRUE;
}

#ifdef JS_GCMETER
JS_FRIEND_API(void)
js_DumpGCStats(JSRuntime *rt, FILE *fp)
{
    uintN i;

    fprintf(fp, "\nGC allocation statistics:\n");

#define UL(x)       ((unsigned long)(x))
#define ULSTAT(x)   UL(rt->gcStats.x)
    fprintf(fp, "     public bytes allocated: %lu\n", UL(rt->gcBytes));
    fprintf(fp, "    private bytes allocated: %lu\n", UL(rt->gcPrivateBytes));
    fprintf(fp, "             alloc attempts: %lu\n", ULSTAT(alloc));
    for (i = 0; i < GC_NUM_FREELISTS; i++) {
        fprintf(fp, "       GC freelist %u length: %lu\n",
                i, ULSTAT(freelen[i]));
        fprintf(fp, " recycles via GC freelist %u: %lu\n",
                i, ULSTAT(recycle[i]));
    }
    fprintf(fp, "allocation retries after GC: %lu\n", ULSTAT(retry));
    fprintf(fp, "        allocation failures: %lu\n", ULSTAT(fail));
    fprintf(fp, "         things born locked: %lu\n", ULSTAT(lockborn));
    fprintf(fp, "           valid lock calls: %lu\n", ULSTAT(lock));
    fprintf(fp, "         valid unlock calls: %lu\n", ULSTAT(unlock));
    fprintf(fp, "       mark recursion depth: %lu\n", ULSTAT(depth));
    fprintf(fp, "     maximum mark recursion: %lu\n", ULSTAT(maxdepth));
    fprintf(fp, "     mark C recursion depth: %lu\n", ULSTAT(cdepth));
    fprintf(fp, "   maximum mark C recursion: %lu\n", ULSTAT(maxcdepth));
    fprintf(fp, "     mark C stack overflows: %lu\n", ULSTAT(dswmark));
    fprintf(fp, "   mark DSW recursion depth: %lu\n", ULSTAT(dswdepth));
    fprintf(fp, " maximum mark DSW recursion: %lu\n", ULSTAT(maxdswdepth));
    fprintf(fp, "  mark DSW up-tree movement: %lu\n", ULSTAT(dswup));
    fprintf(fp, "DSW up-tree obj->slot steps: %lu\n", ULSTAT(dswupstep));
    fprintf(fp, "   maximum GC nesting level: %lu\n", ULSTAT(maxlevel));
    fprintf(fp, "potentially useful GC calls: %lu\n", ULSTAT(poke));
    fprintf(fp, "           useless GC calls: %lu\n", ULSTAT(nopoke));
    fprintf(fp, "  thing arenas freed so far: %lu\n", ULSTAT(afree));
    fprintf(fp, "     stack segments scanned: %lu\n", ULSTAT(stackseg));
    fprintf(fp, "stack segment slots scanned: %lu\n", ULSTAT(segslots));
#undef UL
#undef US

#ifdef JS_ARENAMETER
    JS_DumpArenaStats(fp);
#endif
}
#endif

#ifdef DEBUG
JS_STATIC_DLL_CALLBACK(JSDHashOperator)
js_root_printer(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 i, void *arg)
{
    uint32 *leakedroots = (uint32 *)arg;
    JSGCRootHashEntry *rhe = (JSGCRootHashEntry *)hdr;

    (*leakedroots)++;
    fprintf(stderr,
            "JS engine warning: leaking GC root \'%s\' at %p\n",
            rhe->name ? (char *)rhe->name : "", rhe->root);

    return JS_DHASH_NEXT;
}
#endif

void
js_FinishGC(JSRuntime *rt)
{
    uintN i;

#ifdef JS_ARENAMETER
    JS_DumpArenaStats(stdout);
#endif
#ifdef JS_GCMETER
    js_DumpGCStats(rt, stdout);
#endif
    for (i = 0; i < GC_NUM_FREELISTS; i++) {
        JS_FinishArenaPool(&rt->gcArenaPool[i]);
        rt->gcFreeList[i] = NULL;
    }
    JS_ArenaFinish();

    if (rt->gcRootsHash.ops) {
#ifdef DEBUG
        uint32 leakedroots = 0;

        /* Warn (but don't assert) debug builds of any remaining roots. */
        JS_DHashTableEnumerate(&rt->gcRootsHash, js_root_printer,
                               &leakedroots);
        if (leakedroots > 0) {
            if (leakedroots == 1) {
                fprintf(stderr,
"JS engine warning: 1 GC root remains after destroying the JSRuntime.\n"
"                   This root may point to freed memory. Objects reachable\n"
"                   through it have not been finalized.\n");
            } else {
                fprintf(stderr,
"JS engine warning: %lu GC roots remain after destroying the JSRuntime.\n"
"                   These roots may point to freed memory. Objects reachable\n"
"                   through them have not been finalized.\n",
                        (unsigned long) leakedroots);
            }
        }
#endif

        JS_DHashTableFinish(&rt->gcRootsHash);
        rt->gcRootsHash.ops = NULL;
    }
    if (rt->gcLocksHash) {
        JS_DHashTableDestroy(rt->gcLocksHash);
        rt->gcLocksHash = NULL;
    }
}

JSBool
js_AddRoot(JSContext *cx, void *rp, const char *name)
{
    JSBool ok = js_AddRootRT(cx->runtime, rp, name);
    if (!ok)
        JS_ReportOutOfMemory(cx);
    return ok;
}

JSBool
js_AddRootRT(JSRuntime *rt, void *rp, const char *name)
{
    JSBool ok;
    JSGCRootHashEntry *rhe;

    /*
     * Due to the long-standing, but now removed, use of rt->gcLock across the
     * bulk of js_GC, API users have come to depend on JS_AddRoot etc. locking
     * properly with a racing GC, without calling JS_AddRoot from a request.
     * We have to preserve API compatibility here, now that we avoid holding
     * rt->gcLock across the mark phase (including the root hashtable mark).
     *
     * If the GC is running and we're called on another thread, wait for this
     * GC activation to finish.  We can safely wait here (in the case where we
     * are called within a request on another thread's context) without fear
     * of deadlock because the GC doesn't set rt->gcRunning until after it has
     * waited for all active requests to end.
     */
    JS_LOCK_GC(rt);
#ifdef JS_THREADSAFE
    JS_ASSERT(!rt->gcRunning || rt->gcLevel > 0);
    if (rt->gcRunning && rt->gcThread != js_CurrentThreadId()) {
        do {
            JS_AWAIT_GC_DONE(rt);
        } while (rt->gcLevel > 0);
    }
#endif
    rhe = (JSGCRootHashEntry *) JS_DHashTableOperate(&rt->gcRootsHash, rp,
                                                     JS_DHASH_ADD);
    if (rhe) {
        rhe->root = rp;
        rhe->name = name;
        ok = JS_TRUE;
    } else {
        ok = JS_FALSE;
    }
    JS_UNLOCK_GC(rt);
    return ok;
}

JSBool
js_RemoveRoot(JSRuntime *rt, void *rp)
{
    /*
     * Due to the JS_RemoveRootRT API, we may be called outside of a request.
     * Same synchronization drill as above in js_AddRoot.
     */
    JS_LOCK_GC(rt);
#ifdef JS_THREADSAFE
    JS_ASSERT(!rt->gcRunning || rt->gcLevel > 0);
    if (rt->gcRunning && rt->gcThread != js_CurrentThreadId()) {
        do {
            JS_AWAIT_GC_DONE(rt);
        } while (rt->gcLevel > 0);
    }
#endif
    (void) JS_DHashTableOperate(&rt->gcRootsHash, rp, JS_DHASH_REMOVE);
    rt->gcPoke = JS_TRUE;
    JS_UNLOCK_GC(rt);
    return JS_TRUE;
}

#ifdef DEBUG_brendan
#define NGCHIST 64

static struct GCHist {
    JSBool      lastDitch;
    JSGCThing   *freeList;
} gchist[NGCHIST];

unsigned gchpos;
#endif

void *
js_NewGCThing(JSContext *cx, uintN flags, size_t nbytes)
{
    JSBool tried_gc;
    JSRuntime *rt;
    size_t nflags;
    uintN i;
    JSGCThing *thing, **flp;
    uint8 *flagp;
    JSLocalRootStack *lrs;
    uint32 *bytesptr;

    rt = cx->runtime;
    JS_LOCK_GC(rt);
    JS_ASSERT(!rt->gcRunning);
    if (rt->gcRunning) {
        METER(rt->gcStats.finalfail++);
        JS_UNLOCK_GC(rt);
        return NULL;
    }

#ifdef TOO_MUCH_GC
#ifdef WAY_TOO_MUCH_GC
    rt->gcPoke = JS_TRUE;
#endif
    js_GC(cx, GC_KEEP_ATOMS | GC_ALREADY_LOCKED);
    tried_gc = JS_TRUE;
#else
    tried_gc = JS_FALSE;
#endif

    METER(rt->gcStats.alloc++);
    nbytes = JS_ROUNDUP(nbytes, sizeof(JSGCThing));
    nflags = nbytes / sizeof(JSGCThing);
    i = GC_FREELIST_INDEX(nbytes);
    flp = &rt->gcFreeList[i];

retry:
    thing = *flp;
    if (thing) {
        *flp = thing->next;
        flagp = thing->flagp;
        METER(rt->gcStats.freelen[i]--);
        METER(rt->gcStats.recycle[i]++);
    } else {
        if (rt->gcBytes < rt->gcMaxBytes &&
            (tried_gc || rt->gcMallocBytes < rt->gcMaxBytes))
        {
            /*
             * Inline form of JS_ARENA_ALLOCATE adapted to truncate the current
             * arena's limit to a GC_PAGE_SIZE boundary, and to skip over every
             * GC_PAGE_SIZE-byte-aligned thing (which is actually not a thing,
             * it's a JSGCPageInfo record).
             */
            JSArenaPool *pool = &rt->gcArenaPool[i];
            JSArena *a = pool->current;
            jsuword p = a->avail;
            jsuword q = p + nbytes;

            if (q > (a->limit & ~GC_PAGE_MASK)) {
                thing = gc_new_arena(pool, nbytes);
            } else {
                if ((p & GC_PAGE_MASK) == 0) {
                    /* Beware, p points to a JSGCPageInfo record! */
                    p = FIRST_THING(p, nbytes);
                    q = p + nbytes;
                    JS_ArenaCountAllocation(pool, p & GC_PAGE_MASK);
                }
                a->avail = q;
                thing = (JSGCThing *)p;
            }
            JS_ArenaCountAllocation(pool, nbytes);
        }

        /*
         * Consider doing a "last ditch" GC if thing couldn't be allocated.
         *
         * Keep rt->gcLock across the call into js_GC so we don't starve and
         * lose to racing threads who deplete the heap just after js_GC has
         * replenished it (or has synchronized with a racing GC that collected
         * a bunch of garbage).  This unfair scheduling can happen on certain
         * operating systems.  For the gory details, see Mozilla bug 162779
         * (http://bugzilla.mozilla.org/show_bug.cgi?id=162779).
         */
        if (!thing) {
            if (!tried_gc) {
                rt->gcPoke = JS_TRUE;
                js_GC(cx, GC_KEEP_ATOMS | GC_ALREADY_LOCKED);
                if (JS_HAS_NATIVE_BRANCH_CALLBACK_OPTION(cx) &&
                    cx->branchCallback &&
                    !cx->branchCallback(cx, NULL)) {
                    METER(rt->gcStats.retryhalt++);
                    JS_UNLOCK_GC(rt);
                    return NULL;
                }
                tried_gc = JS_TRUE;
                METER(rt->gcStats.retry++);
                goto retry;
            }
            goto fail;
        }

        /* Find the flags pointer given thing's address. */
        flagp = js_GetGCThingFlags(thing);
    }

    lrs = cx->localRootStack;
    if (lrs) {
        /*
         * If we're in a local root scope, don't set cx->newborn[type] at all,
         * to avoid entraining garbage from it for an unbounded amount of time
         * on this context.  A caller will leave the local root scope and pop
         * this reference, allowing thing to be GC'd if it has no other refs.
         * See JS_EnterLocalRootScope and related APIs.
         */
        if (js_PushLocalRoot(cx, lrs, (jsval) thing) < 0)
            goto fail;
    } else {
        /*
         * No local root scope, so we're stuck with the old, fragile model of
         * depending on a pigeon-hole newborn per type per context.
         */
        cx->newborn[flags & GCF_TYPEMASK] = thing;
    }

    /* We can't fail now, so update flags and rt->gc{,Private}Bytes. */
    *flagp = (uint8)flags;
    bytesptr = ((flags & GCF_TYPEMASK) == GCX_PRIVATE)
               ? &rt->gcPrivateBytes
               : &rt->gcBytes;
    *bytesptr += nbytes + nflags;

    /*
     * Clear thing before unlocking in case a GC run is about to scan it,
     * finding it via cx->newborn[].
     */
    thing->next = NULL;
    thing->flagp = NULL;
#ifdef DEBUG_brendan
    gchist[gchpos].lastDitch = tried_gc;
    gchist[gchpos].freeList = *flp;
    if (++gchpos == NGCHIST)
        gchpos = 0;
#endif
    METER(if (flags & GCF_LOCK) rt->gcStats.lockborn++);
    JS_UNLOCK_GC(rt);
    return thing;

fail:
    METER(rt->gcStats.fail++);
    JS_UNLOCK_GC(rt);
    JS_ReportOutOfMemory(cx);
    return NULL;
}

JSBool
js_LockGCThing(JSContext *cx, void *thing)
{
    JSBool ok = js_LockGCThingRT(cx->runtime, thing);
    if (!ok)
        JS_ReportOutOfMemory(cx);
    return ok;
}

/*
 * Deep GC-things can't be locked just by setting the GCF_LOCK bit, because
 * their descendants must be marked by the GC.  To find them during the mark
 * phase, they are added to rt->gcLocksHash, which is created lazily.
 *
 * NB: we depend on the order of GC-thing type indexes here!
 */
#define GC_TYPE_IS_STRING(t)    ((t) == GCX_STRING ||                         \
                                 (t) >= GCX_EXTERNAL_STRING)
#define GC_TYPE_IS_XML(t)       ((unsigned)((t) - GCX_NAMESPACE) <=           \
                                 (unsigned)(GCX_XML - GCX_NAMESPACE))
#define GC_TYPE_IS_DEEP(t)      ((t) == GCX_OBJECT || GC_TYPE_IS_XML(t))

#define IS_DEEP_STRING(t,o)     (GC_TYPE_IS_STRING(t) &&                      \
                                 JSSTRING_IS_DEPENDENT((JSString *)(o)))

#define GC_THING_IS_DEEP(t,o)   (GC_TYPE_IS_DEEP(t) || IS_DEEP_STRING(t, o))

JSBool
js_LockGCThingRT(JSRuntime *rt, void *thing)
{
    JSBool ok, deep;
    uint8 *flagp, flags, lock, type;
    JSGCLockHashEntry *lhe;

    ok = JS_TRUE;
    if (!thing)
        return ok;

    flagp = js_GetGCThingFlags(thing);

    JS_LOCK_GC(rt);
    flags = *flagp;
    lock = (flags & GCF_LOCK);
    type = (flags & GCF_TYPEMASK);
    deep = GC_THING_IS_DEEP(type, thing);

    /*
     * Avoid adding a rt->gcLocksHash entry for shallow things until someone
     * nests a lock -- then start such an entry with a count of 2, not 1.
     */
    if (lock || deep) {
        if (!rt->gcLocksHash) {
            rt->gcLocksHash =
                JS_NewDHashTable(JS_DHashGetStubOps(), NULL,
                                 sizeof(JSGCLockHashEntry),
                                 GC_ROOTS_SIZE);
            if (!rt->gcLocksHash) {
                ok = JS_FALSE;
                goto done;
            }
        } else if (lock == 0) {
#ifdef DEBUG
            JSDHashEntryHdr *hdr =
                JS_DHashTableOperate(rt->gcLocksHash, thing,
                                     JS_DHASH_LOOKUP);
            JS_ASSERT(JS_DHASH_ENTRY_IS_FREE(hdr));
#endif
        }

        lhe = (JSGCLockHashEntry *)
            JS_DHashTableOperate(rt->gcLocksHash, thing, JS_DHASH_ADD);
        if (!lhe) {
            ok = JS_FALSE;
            goto done;
        }
        if (!lhe->thing) {
            lhe->thing = thing;
            lhe->count = deep ? 1 : 2;
        } else {
            JS_ASSERT(lhe->count >= 1);
            lhe->count++;
        }
    }

    *flagp = (uint8)(flags | GCF_LOCK);
    METER(rt->gcStats.lock++);
    ok = JS_TRUE;
done:
    JS_UNLOCK_GC(rt);
    return ok;
}

JSBool
js_UnlockGCThingRT(JSRuntime *rt, void *thing)
{
    uint8 *flagp, flags;
    JSGCLockHashEntry *lhe;

    if (!thing)
        return JS_TRUE;

    flagp = js_GetGCThingFlags(thing);
    JS_LOCK_GC(rt);
    flags = *flagp;

    if (flags & GCF_LOCK) {
        if (!rt->gcLocksHash ||
            (lhe = (JSGCLockHashEntry *)
                   JS_DHashTableOperate(rt->gcLocksHash, thing,
                                        JS_DHASH_LOOKUP),
             JS_DHASH_ENTRY_IS_FREE(&lhe->hdr))) {
            /* Shallow GC-thing with an implicit lock count of 1. */
            JS_ASSERT(!GC_THING_IS_DEEP(flags & GCF_TYPEMASK, thing));
        } else {
            /* Basis or nested unlock of a deep thing, or nested of shallow. */
            if (--lhe->count != 0)
                goto out;
            JS_DHashTableOperate(rt->gcLocksHash, thing, JS_DHASH_REMOVE);
        }
        *flagp = (uint8)(flags & ~GCF_LOCK);
    }

    rt->gcPoke = JS_TRUE;
out:
    METER(rt->gcStats.unlock++);
    JS_UNLOCK_GC(rt);
    return JS_TRUE;
}

#ifdef GC_MARK_DEBUG

#include <stdio.h>
#include "jsprf.h"

JS_FRIEND_DATA(FILE *) js_DumpGCHeap;
JS_EXPORT_DATA(void *) js_LiveThingToFind;

#ifdef HAVE_XPCONNECT
#include "dump_xpc.h"
#endif

static const char *
gc_object_class_name(void* thing)
{
    uint8 *flagp = js_GetGCThingFlags(thing);
    const char *className = "";
    static char depbuf[32];

    switch (*flagp & GCF_TYPEMASK) {
      case GCX_OBJECT: {
        JSObject  *obj = (JSObject *)thing;
        JSClass   *clasp = JSVAL_TO_PRIVATE(obj->slots[JSSLOT_CLASS]);
        className = clasp->name;
#ifdef HAVE_XPCONNECT
        if (clasp->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS) {
            jsval privateValue = obj->slots[JSSLOT_PRIVATE];

            JS_ASSERT(clasp->flags & JSCLASS_HAS_PRIVATE);
            if (!JSVAL_IS_VOID(privateValue)) {
                void  *privateThing = JSVAL_TO_PRIVATE(privateValue);
                const char *xpcClassName = GetXPCObjectClassName(privateThing);

                if (xpcClassName)
                    className = xpcClassName;
            }
        }
#endif
        break;
      }

      case GCX_STRING:
      case GCX_MUTABLE_STRING: {
        JSString *str = (JSString *)thing;
        if (JSSTRING_IS_DEPENDENT(str)) {
            JS_snprintf(depbuf, sizeof depbuf, "start:%u, length:%u",
                        JSSTRDEP_START(str), JSSTRDEP_LENGTH(str));
            className = depbuf;
        } else {
            className = "string";
        }
        break;
      }

      case GCX_DOUBLE:
        className = "double";
        break;
    }

    return className;
}

static void
gc_dump_thing(JSGCThing *thing, uint8 flags, GCMarkNode *prev, FILE *fp)
{
    GCMarkNode *next = NULL;
    char *path = NULL;

    while (prev) {
        next = prev;
        prev = prev->prev;
    }
    while (next) {
        uint8 nextFlags = *js_GetGCThingFlags(next->thing);
        if ((nextFlags & GCF_TYPEMASK) == GCX_OBJECT) {
            path = JS_sprintf_append(path, "%s(%s @ 0x%08p).",
                                     next->name,
                                     gc_object_class_name(next->thing),
                                     (JSObject*)next->thing);
        } else {
            path = JS_sprintf_append(path, "%s(%s).",
                                     next->name,
                                     gc_object_class_name(next->thing));
        }
        next = next->next;
    }
    if (!path)
        return;

    fprintf(fp, "%08lx ", (long)thing);
    switch (flags & GCF_TYPEMASK) {
      case GCX_OBJECT:
      {
        JSObject  *obj = (JSObject *)thing;
        jsval     privateValue = obj->slots[JSSLOT_PRIVATE];
        void      *privateThing = JSVAL_IS_VOID(privateValue)
                                  ? NULL
                                  : JSVAL_TO_PRIVATE(privateValue);
        const char *className = gc_object_class_name(thing);
        fprintf(fp, "object %8p %s", privateThing, className);
        break;
      }
#if JS_HAS_XML_SUPPORT
      case GCX_NAMESPACE:
      {
        JSXMLNamespace *ns = (JSXMLNamespace *)thing;
        fprintf(fp, "namespace %s:%s",
                JS_GetStringBytes(ns->prefix), JS_GetStringBytes(ns->uri));
        break;
      }
      case GCX_QNAME:
      {
        JSXMLQName *qn = (JSXMLQName *)thing;
        fprintf(fp, "qname %s(%s):%s",
                JS_GetStringBytes(qn->prefix), JS_GetStringBytes(qn->uri),
                JS_GetStringBytes(qn->localName));
        break;
      }
      case GCX_XML:
      {
        extern const char *js_xml_class_str[];
        JSXML *xml = (JSXML *)thing;
        fprintf(fp, "xml %8p %s", xml, js_xml_class_str[xml->xml_class]);
        break;
      }
#endif
      case GCX_DOUBLE:
        fprintf(fp, "double %g", *(jsdouble *)thing);
        break;
      case GCX_PRIVATE:
        fprintf(fp, "private %8p", (void *)thing);
        break;
      default:
        fprintf(fp, "string %s", JS_GetStringBytes((JSString *)thing));
        break;
    }
    fprintf(fp, " via %s\n", path);
    free(path);
}

#endif /* !GC_MARK_DEBUG */

static void
gc_mark_atom_key_thing(void *thing, void *arg)
{
    JSContext *cx = (JSContext *) arg;

    GC_MARK(cx, thing, "atom", NULL);
}

void
js_MarkAtom(JSContext *cx, JSAtom *atom, void *arg)
{
    jsval key;

    if (atom->flags & ATOM_MARK)
        return;
    atom->flags |= ATOM_MARK;
    key = ATOM_KEY(atom);
    if (JSVAL_IS_GCTHING(key)) {
#ifdef GC_MARK_DEBUG
        char name[32];

        if (JSVAL_IS_STRING(key)) {
            JS_snprintf(name, sizeof name, "'%s'",
                        JS_GetStringBytes(JSVAL_TO_STRING(key)));
        } else {
            JS_snprintf(name, sizeof name, "<%x>", key);
        }
#endif
        GC_MARK(cx, JSVAL_TO_GCTHING(key), name, arg);
    }
    if (atom->flags & ATOM_HIDDEN)
        js_MarkAtom(cx, atom->entry.value, arg);
}

/*
 * These macros help avoid passing the GC_MARK_DEBUG-only |arg| parameter
 * during recursive calls when GC_MARK_DEBUG is not defined.
 */
#ifdef GC_MARK_DEBUG
# define UNMARKED_GC_THING_FLAGS(thing, arg)                                  \
    UnmarkedGCThingFlags(thing, arg)
# define NEXT_UNMARKED_GC_THING(vp, end, thingp, flagpp, arg)                 \
    NextUnmarkedGCThing(vp, end, thingp, flagpp, arg)
# define MARK_GC_THING(cx, thing, flagp, arg)                                 \
    MarkGCThing(cx, thing, flagp, arg)
# define CALL_GC_THING_MARKER(marker, cx, thing, arg)                         \
    marker(cx, thing, arg)
#else
# define UNMARKED_GC_THING_FLAGS(thing, arg)                                  \
    UnmarkedGCThingFlags(thing)
# define NEXT_UNMARKED_GC_THING(vp, end, thingp, flagpp, arg)                 \
    NextUnmarkedGCThing(vp, end, thingp, flagpp)
# define MARK_GC_THING(cx, thing, flagp, arg)                                 \
    MarkGCThing(cx, thing, flagp)
# define CALL_GC_THING_MARKER(marker, cx, thing, arg)                         \
    marker(cx, thing, NULL)
#endif

static uint8 *
UNMARKED_GC_THING_FLAGS(void *thing, void *arg)
{
    uint8 flags, *flagp;

    if (!thing)
        return NULL;

    flagp = js_GetGCThingFlags(thing);
    flags = *flagp;
    JS_ASSERT(flags != GCF_FINAL);
#ifdef GC_MARK_DEBUG
    if (js_LiveThingToFind == thing)
        gc_dump_thing(thing, flags, arg, stderr);
#endif

    if (flags & GCF_MARK)
        return NULL;

    return flagp;
}

static jsval *
NEXT_UNMARKED_GC_THING(jsval *vp, jsval *end, void **thingp, uint8 **flagpp,
                       void *arg)
{
    jsval v;
    void *thing;
    uint8 *flagp;

    while (vp < end) {
        v = *vp;
        if (JSVAL_IS_GCTHING(v)) {
            thing = JSVAL_TO_GCTHING(v);
            flagp = UNMARKED_GC_THING_FLAGS(thing, arg);
            if (flagp) {
                *thingp = thing;
                *flagpp = flagp;
                return vp;
            }
        }
        vp++;
    }
    return NULL;
}

static void
DeutschSchorrWaite(JSContext *cx, void *thing, uint8 *flagp);

static JSBool
MARK_GC_THING(JSContext *cx, void *thing, uint8 *flagp, void *arg)
{
    JSRuntime *rt;
    JSObject *obj;
    jsval v, *vp, *end;
    JSString *str;
    void *next_thing;
    uint8 *next_flagp;
#ifdef JS_GCMETER
    uint32 tailCallNesting;
#endif
#ifdef GC_MARK_DEBUG
    JSScope *scope;
    JSScopeProperty *sprop;
    char name[32];
#endif
    int stackDummy;

    rt = cx->runtime;
    METER(tailCallNesting = 0);
    METER(if (++rt->gcStats.cdepth > rt->gcStats.maxcdepth)
              rt->gcStats.maxcdepth = rt->gcStats.cdepth);

#ifndef GC_MARK_DEBUG
  start:
#endif
    JS_ASSERT(flagp);
    METER(if (++rt->gcStats.depth > rt->gcStats.maxdepth)
              rt->gcStats.maxdepth = rt->gcStats.depth);
    if (*flagp & GCF_MARK) {
        /*
         * This should happen only if recursive MARK_GC_THING marks flags
         * already stored in the caller's *next_flagp.
         */
        goto out;
    }

    *flagp |= GCF_MARK;

#ifdef GC_MARK_DEBUG
    if (js_DumpGCHeap)
        gc_dump_thing(thing, *flagp, arg, js_DumpGCHeap);
#endif

    switch (*flagp & GCF_TYPEMASK) {
      case GCX_OBJECT:
        /* If obj->slots is null, obj must be a newborn. */
        obj = (JSObject *) thing;
        vp = obj->slots;
        if (!vp)
            goto out;

        /* Switch to Deutsch-Schorr-Waite if we exhaust our stack quota. */
        if (!JS_CHECK_STACK_SIZE(cx, stackDummy)) {
            METER(rt->gcStats.dswmark++);
            DeutschSchorrWaite(cx, thing, flagp);
            goto out;
        }

        /* Mark slots if they are small enough to be GC-allocated. */
        if ((vp[-1] + 1) * sizeof(jsval) <= GC_NBYTES_MAX)
            GC_MARK(cx, vp - 1, "slots", arg);

        /* Set up local variables to loop over unmarked things. */
        end = vp + ((obj->map->ops->mark)
                    ? CALL_GC_THING_MARKER(obj->map->ops->mark, cx, obj, arg)
                    : JS_MIN(obj->map->freeslot, obj->map->nslots));

        vp = NEXT_UNMARKED_GC_THING(vp, end, &thing, &flagp, arg);
        if (!vp)
            goto out;
        v = *vp;

        /*
         * Here, thing is the first value in obj->slots referring to an
         * unmarked GC-thing.
         */
#ifdef GC_MARK_DEBUG
        scope = OBJ_IS_NATIVE(obj) ? OBJ_SCOPE(obj) : NULL;
#endif
        for (;;) {
            /* Check loop invariants. */
            JS_ASSERT(v == *vp && JSVAL_IS_GCTHING(v));
            JS_ASSERT(thing == JSVAL_TO_GCTHING(v));
            JS_ASSERT(flagp == js_GetGCThingFlags(thing));

#ifdef GC_MARK_DEBUG
            if (scope) {
                uint32 slot;
                jsval nval;

                slot = vp - obj->slots;
                for (sprop = SCOPE_LAST_PROP(scope); ; sprop = sprop->parent) {
                    if (!sprop) {
                        switch (slot) {
                          case JSSLOT_PROTO:
                            strcpy(name, js_proto_str);
                            break;
                          case JSSLOT_PARENT:
                            strcpy(name, js_parent_str);
                            break;
                          default:
                            JS_snprintf(name, sizeof name,
                                        "**UNKNOWN SLOT %ld**",
                                        (long)slot);
                            break;
                        }
                        break;
                    }
                    if (sprop->slot == slot) {
                        nval = ID_TO_VALUE(sprop->id);
                        if (JSVAL_IS_INT(nval)) {
                            JS_snprintf(name, sizeof name, "%ld",
                                        (long)JSVAL_TO_INT(nval));
                        } else if (JSVAL_IS_STRING(nval)) {
                            JS_snprintf(name, sizeof name, "%s",
                              JS_GetStringBytes(JSVAL_TO_STRING(nval)));
                        } else {
                            strcpy(name, "**FINALIZED ATOM KEY**");
                        }
                        break;
                    }
                }
            } else {
                strcpy(name, "**UNKNOWN OBJECT MAP ENTRY**");
            }
#endif

            do {
                vp = NEXT_UNMARKED_GC_THING(vp+1, end, &next_thing, &next_flagp,
                                            arg);
                if (!vp) {
                    /*
                     * Here thing came from the last unmarked GC-thing slot.
                     * We can eliminate tail recursion unless GC_MARK_DEBUG
                     * is defined.
                     */
#ifdef GC_MARK_DEBUG
                    GC_MARK(cx, thing, name, arg);
                    goto out;
#else
                    METER(++tailCallNesting);
                    goto start;
#endif
                }
            } while (next_thing == thing);
            v = *vp;

#ifdef GC_MARK_DEBUG
            GC_MARK(cx, thing, name, arg);
#else
            MARK_GC_THING(cx, thing, flagp, arg);
#endif
            thing = next_thing;
            flagp = next_flagp;
        }
        break;

#ifdef DEBUG
      case GCX_STRING:
        str = (JSString *)thing;
        JS_ASSERT(!JSSTRING_IS_DEPENDENT(str));
        break;
#endif

      case GCX_MUTABLE_STRING:
        str = (JSString *)thing;
        if (JSSTRING_IS_DEPENDENT(str)) {
            thing = JSSTRDEP_BASE(str);
            flagp = UNMARKED_GC_THING_FLAGS(thing, arg);
            if (flagp) {
#ifdef GC_MARK_DEBUG
                GC_MARK(cx, thing, "base", arg);
                goto out;
#else
                METER(++tailCallNesting);
                goto start;
#endif
            }
        }
        break;

#if JS_HAS_XML_SUPPORT
      case GCX_NAMESPACE:
        CALL_GC_THING_MARKER(js_MarkXMLNamespace, cx, (JSXMLNamespace *)thing,
                             arg);
        break;

      case GCX_QNAME:
        CALL_GC_THING_MARKER(js_MarkXMLQName, cx, (JSXMLQName *)thing, arg);
        break;

      case GCX_XML:
        CALL_GC_THING_MARKER(js_MarkXML, cx, (JSXML *)thing, arg);
        break;
#endif
    }

out:
    METER(rt->gcStats.depth -= 1 + tailCallNesting);
    METER(rt->gcStats.cdepth--);
    return JS_TRUE;
}

/*
 * An invalid object reference that's distinct from JSVAL_TRUE and JSVAL_FALSE
 * when tagged as a boolean.  Used to indicate empty DSW mark stack.
 *
 * Reversed pointers that link the DSW mark stack through obj->slots entries
 * are also tagged as booleans so we can find each pointer and unreverse it.
 * Because no object pointer is <= 16, these values can be distinguished from
 * JSVAL_EMPTY, JSVAL_TRUE, and JSVAL_FALSE.
 */
#define JSVAL_EMPTY (2 << JSVAL_TAGBITS)

/*
 * To optimize native objects to avoid O(n^2) explosion in pathological cases,
 * we use a dswIndex member of JSScope to tell where in obj->slots to find the
 * reversed pointer.  Scrounging space in JSScope by packing existing members
 * tighter yielded 16 bits of index, which we use directly if obj->slots has
 * 64K or fewer slots.  Otherwise we make scope->dswIndex a fixed-point 16-bit
 * fraction of the number of slots.
 */
static JS_INLINE uint16
EncodeDSWIndex(jsval *vp, jsval *slots)
{
    uint32 nslots, limit, index;
    jsdouble d;

    nslots = slots[-1];
    limit = JS_BIT(16);
    index = PTRDIFF(vp, slots, jsval);
    JS_ASSERT(index < nslots);
    if (nslots > limit) {
        d = ((jsdouble)index / nslots) * limit;
        JS_ASSERT(0 <= d && d < limit);
        return (uint16) d;
    }
    return (uint16) index;
}

static JS_INLINE uint32
DecodeDSWIndex(uint16 dswIndex, jsval *slots)
{
    uint32 nslots, limit;
    jsdouble d;

    nslots = slots[-1];
    limit = JS_BIT(16);
    JS_ASSERT(dswIndex < nslots);
    if (nslots > limit) {
        d = ((jsdouble)dswIndex * nslots) / limit;
        JS_ASSERT(0 <= d && d < nslots);
        return (uint32) d;
    }
    return dswIndex;
}

static void
DeutschSchorrWaite(JSContext *cx, void *thing, uint8 *flagp)
{
    jsval top, parent, v, *vp, *end;
    JSObject *obj;
    JSScope *scope;
#ifdef JS_GCMETER
    JSRuntime *rt = cx->runtime;
#endif

    top = JSVAL_EMPTY;

down:
    METER(if (++rt->gcStats.dswdepth > rt->gcStats.maxdswdepth)
              rt->gcStats.maxdswdepth = rt->gcStats.dswdepth);
    obj = (JSObject *) thing;
    parent = OBJECT_TO_JSVAL(obj);

    /* Precompute for quick testing to set and get scope->dswIndex. */
    scope = (OBJ_IS_NATIVE(obj) && OBJ_SCOPE(obj)->object == obj)
            ? OBJ_SCOPE(obj)
            : NULL;

    /* Mark slots if they are small enough to be GC-allocated. */
    vp = obj->slots;
    if ((vp[-1] + 1) * sizeof(jsval) <= GC_NBYTES_MAX)
        GC_MARK(cx, vp - 1, "slots", NULL);

    end = vp + ((obj->map->ops->mark)
                ? obj->map->ops->mark(cx, obj, NULL)
                : JS_MIN(obj->map->freeslot, obj->map->nslots));

    *flagp |= GCF_MARK;

    for (;;) {
        while ((vp = NEXT_UNMARKED_GC_THING(vp, end, &thing, &flagp, NULL))
               != NULL) {
            v = *vp;
            JS_ASSERT(JSVAL_TO_GCTHING(v) == thing);

            if (JSVAL_IS_OBJECT(v)) {
                *vp = JSVAL_SETTAG(top, JSVAL_BOOLEAN);
                top = parent;
                if (scope)
                    scope->dswIndex = EncodeDSWIndex(vp, obj->slots);
                goto down;
            }

            /* Handle string and double GC-things. */
            MARK_GC_THING(cx, thing, flagp, NULL);
        }

        /* If we are back at the root (or we never left it), we're done. */
        METER(rt->gcStats.dswdepth--);
        if (scope)
            scope->dswIndex = 0;
        if (top == JSVAL_EMPTY)
            return;

        /* Time to go back up the spanning tree. */
        METER(rt->gcStats.dswup++);
        obj = JSVAL_TO_OBJECT(top);
        vp = obj->slots;
        end = vp + vp[-1];

        /*
         * If obj is native and owns its own scope, we can minimize the cost
         * of searching for the reversed pointer.
         */
        scope = (OBJ_IS_NATIVE(obj) && OBJ_SCOPE(obj)->object == obj)
                ? OBJ_SCOPE(obj)
                : NULL;
        if (scope)
            vp += DecodeDSWIndex(scope->dswIndex, vp);

        /*
         * Alas, we must search for the reversed pointer.  If we used the
         * scope->dswIndex hint, we'll step over a few slots for objects with
         * a few times 64K slots, etc.  For more typical (that is, far fewer
         * than 64K slots) native objects that own their own scopes, this loop
         * won't iterate at all.  The order of complexity for host objects and
         * unmutated native objects is O(n^2), but n (4 or 5 in most cases) is
         * low enough that we don't care.
         *
         * We cannot use a reversed pointer into obj->slots, because there
         * is no way to find an object from an address within its slots.
         */
        v = *vp;
        while (v <= JSVAL_TRUE || !JSVAL_IS_BOOLEAN(v)) {
            METER(rt->gcStats.dswupstep++);
            JS_ASSERT(vp + 1 < end);
            v = *++vp;
        }

        *vp++ = parent;
        parent = top;
        top = JSVAL_CLRTAG(v);
    }
}

void
js_MarkGCThing(JSContext *cx, void *thing, void *arg)
{
    uint8 *flagp;

    flagp = UNMARKED_GC_THING_FLAGS(thing, arg);
    if (!flagp)
        return;
    MARK_GC_THING(cx, thing, flagp, arg);
}

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
gc_root_marker(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 num, void *arg)
{
    JSGCRootHashEntry *rhe = (JSGCRootHashEntry *)hdr;
    jsval *rp = (jsval *)rhe->root;
    jsval v = *rp;

    /* Ignore null object and scalar values. */
    if (!JSVAL_IS_NULL(v) && JSVAL_IS_GCTHING(v)) {
        JSContext *cx = (JSContext *)arg;
#ifdef DEBUG
        uintN i;
        JSArena *a;
        jsuword firstpage;
        JSBool root_points_to_gcArenaPool = JS_FALSE;
        void *thing = JSVAL_TO_GCTHING(v);

        for (i = 0; i < GC_NUM_FREELISTS; i++) {
            for (a = cx->runtime->gcArenaPool[i].first.next; a; a = a->next) {
                firstpage = FIRST_THING_PAGE(a);
                if (JS_UPTRDIFF(thing, firstpage) < a->avail - firstpage) {
                    root_points_to_gcArenaPool = JS_TRUE;
                    break;
                }
            }
        }
        if (!root_points_to_gcArenaPool && rhe->name) {
            fprintf(stderr,
"JS API usage error: the address passed to JS_AddNamedRoot currently holds an\n"
"invalid jsval.  This is usually caused by a missing call to JS_RemoveRoot.\n"
"The root's name is \"%s\".\n",
                    rhe->name);
        }
        JS_ASSERT(root_points_to_gcArenaPool);
#endif

        GC_MARK(cx, JSVAL_TO_GCTHING(v), rhe->name ? rhe->name : "root", NULL);
    }
    return JS_DHASH_NEXT;
}

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
gc_lock_marker(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 num, void *arg)
{
    JSGCLockHashEntry *lhe = (JSGCLockHashEntry *)hdr;
    void *thing = (void *)lhe->thing;
    JSContext *cx = (JSContext *)arg;

    GC_MARK(cx, thing, "locked object", NULL);
    return JS_DHASH_NEXT;
}

void
js_ForceGC(JSContext *cx, uintN gcflags)
{
    uintN i;

    for (i = 0; i < GCX_NTYPES; i++)
        cx->newborn[i] = NULL;
    cx->lastAtom = NULL;
    cx->runtime->gcPoke = JS_TRUE;
    js_GC(cx, gcflags);
    JS_ArenaFinish();
}

#define GC_MARK_JSVALS(cx, len, vec, name)                                    \
    JS_BEGIN_MACRO                                                            \
        jsval _v, *_vp, *_end;                                                \
                                                                              \
        for (_vp = vec, _end = _vp + len; _vp < _end; _vp++) {                \
            _v = *_vp;                                                        \
            if (JSVAL_IS_GCTHING(_v))                                         \
                GC_MARK(cx, JSVAL_TO_GCTHING(_v), name, NULL);                \
        }                                                                     \
    JS_END_MACRO

void
js_GC(JSContext *cx, uintN gcflags)
{
    JSRuntime *rt;
    JSContext *iter, *acx;
    JSStackFrame *fp, *chain;
    uintN i, depth, nslots, type;
    JSStackHeader *sh;
    size_t nbytes, nflags;
    JSArena *a, **ap;
    uint8 flags, *flagp, *split;
    JSGCThing *thing, *limit, **flp, **oflp;
    GCFinalizeOp finalizer;
    uint32 *bytesptr;
    JSBool all_clear;
#ifdef JS_THREADSAFE
    jsword currentThread;
    uint32 requestDebit;
#endif

    rt = cx->runtime;
#ifdef JS_THREADSAFE
    /* Avoid deadlock. */
    JS_ASSERT(!JS_IS_RUNTIME_LOCKED(rt));
#endif

    /*
     * Don't collect garbage if the runtime isn't up, and cx is not the last
     * context in the runtime.  The last context must force a GC, and nothing
     * should suppress that final collection or there may be shutdown leaks,
     * or runtime bloat until the next context is created.
     */
    if (rt->state != JSRTS_UP && !(gcflags & GC_LAST_CONTEXT))
        return;

    /*
     * Let the API user decide to defer a GC if it wants to (unless this
     * is the last context).  Invoke the callback regardless.
     */
    if (rt->gcCallback) {
        if (!rt->gcCallback(cx, JSGC_BEGIN) && !(gcflags & GC_LAST_CONTEXT))
            return;
    }

    /* Lock out other GC allocator and collector invocations. */
    if (!(gcflags & GC_ALREADY_LOCKED))
        JS_LOCK_GC(rt);

    /* Do nothing if no mutator has executed since the last GC. */
    if (!rt->gcPoke) {
        METER(rt->gcStats.nopoke++);
        if (!(gcflags & GC_ALREADY_LOCKED))
            JS_UNLOCK_GC(rt);
        return;
    }
    METER(rt->gcStats.poke++);
    rt->gcPoke = JS_FALSE;

#ifdef JS_THREADSAFE
    /* Bump gcLevel and return rather than nest on this thread. */
    currentThread = js_CurrentThreadId();
    if (rt->gcThread == currentThread) {
        JS_ASSERT(rt->gcLevel > 0);
        rt->gcLevel++;
        METER(if (rt->gcLevel > rt->gcStats.maxlevel)
                  rt->gcStats.maxlevel = rt->gcLevel);
        if (!(gcflags & GC_ALREADY_LOCKED))
            JS_UNLOCK_GC(rt);
        return;
    }

    /*
     * If we're in one or more requests (possibly on more than one context)
     * running on the current thread, indicate, temporarily, that all these
     * requests are inactive.  NB: if cx->thread is 0, then cx is not using
     * the request model, and does not contribute to rt->requestCount.
     */
    requestDebit = 0;
    if (cx->thread) {
        /*
         * Check all contexts for any with the same thread-id.  XXX should we
         * keep a sub-list of contexts having the same id?
         */
        iter = NULL;
        while ((acx = js_ContextIterator(rt, JS_FALSE, &iter)) != NULL) {
            if (acx->thread == cx->thread && acx->requestDepth)
                requestDebit++;
        }
    } else {
        /*
         * We assert, but check anyway, in case someone is misusing the API.
         * Avoiding the loop over all of rt's contexts is a win in the event
         * that the GC runs only on request-less contexts with 0 thread-ids,
         * in a special thread such as might be used by the UI/DOM/Layout
         * "mozilla" or "main" thread in Mozilla-the-browser.
         */
        JS_ASSERT(cx->requestDepth == 0);
        if (cx->requestDepth)
            requestDebit = 1;
    }
    if (requestDebit) {
        JS_ASSERT(requestDebit <= rt->requestCount);
        rt->requestCount -= requestDebit;
        if (rt->requestCount == 0)
            JS_NOTIFY_REQUEST_DONE(rt);
    }

    /* If another thread is already in GC, don't attempt GC; wait instead. */
    if (rt->gcLevel > 0) {
        /* Bump gcLevel to restart the current GC, so it finds new garbage. */
        rt->gcLevel++;
        METER(if (rt->gcLevel > rt->gcStats.maxlevel)
                  rt->gcStats.maxlevel = rt->gcLevel);

        /* Wait for the other thread to finish, then resume our request. */
        while (rt->gcLevel > 0)
            JS_AWAIT_GC_DONE(rt);
        if (requestDebit)
            rt->requestCount += requestDebit;
        if (!(gcflags & GC_ALREADY_LOCKED))
            JS_UNLOCK_GC(rt);
        return;
    }

    /* No other thread is in GC, so indicate that we're now in GC. */
    rt->gcLevel = 1;
    rt->gcThread = currentThread;

    /* Wait for all other requests to finish. */
    while (rt->requestCount > 0)
        JS_AWAIT_REQUEST_DONE(rt);

#else  /* !JS_THREADSAFE */

    /* Bump gcLevel and return rather than nest; the outer gc will restart. */
    rt->gcLevel++;
    METER(if (rt->gcLevel > rt->gcStats.maxlevel)
              rt->gcStats.maxlevel = rt->gcLevel);
    if (rt->gcLevel > 1)
        return;

#endif /* !JS_THREADSAFE */

    /*
     * Set rt->gcRunning here within the GC lock, and after waiting for any
     * active requests to end, so that new requests that try to JS_AddRoot,
     * JS_RemoveRoot, or JS_RemoveRootRT block in JS_BeginRequest waiting for
     * rt->gcLevel to drop to zero, while request-less calls to the *Root*
     * APIs block in js_AddRoot or js_RemoveRoot (see above in this file),
     * waiting for GC to finish.
     */
    rt->gcRunning = JS_TRUE;
    JS_UNLOCK_GC(rt);

    /* If a suspended compile is running on another context, keep atoms. */
    if (rt->gcKeepAtoms)
        gcflags |= GC_KEEP_ATOMS;

    /* Reset malloc counter. */
    rt->gcMallocBytes = 0;

    /* Drop atoms held by the property cache, and clear property weak links. */
    js_DisablePropertyCache(cx);
    js_FlushPropertyCache(cx);
#ifdef DEBUG_notme
  { extern void js_DumpScopeMeters(JSRuntime *rt);
    js_DumpScopeMeters(rt);
  }
#endif

restart:
    rt->gcNumber++;

    /*
     * Mark phase.
     */
    JS_DHashTableEnumerate(&rt->gcRootsHash, gc_root_marker, cx);
    if (rt->gcLocksHash)
        JS_DHashTableEnumerate(rt->gcLocksHash, gc_lock_marker, cx);
    js_MarkAtomState(&rt->atomState, gcflags, gc_mark_atom_key_thing, cx);
    js_MarkWatchPoints(rt);
    js_MarkScriptFilenames(rt, gcflags);

    iter = NULL;
    while ((acx = js_ContextIterator(rt, JS_TRUE, &iter)) != NULL) {
        /*
         * Iterate frame chain and dormant chains. Temporarily tack current
         * frame onto the head of the dormant list to ease iteration.
         *
         * (NB: see comment on this whole "dormant" thing in js_Execute.)
         */
        chain = acx->fp;
        if (chain) {
            JS_ASSERT(!chain->dormantNext);
            chain->dormantNext = acx->dormantFrameChain;
        } else {
            chain = acx->dormantFrameChain;
        }

        for (fp = chain; fp; fp = chain = chain->dormantNext) {
            do {
                if (fp->callobj)
                    GC_MARK(cx, fp->callobj, "call object", NULL);
                if (fp->argsobj)
                    GC_MARK(cx, fp->argsobj, "arguments object", NULL);
                if (fp->varobj)
                    GC_MARK(cx, fp->varobj, "variables object", NULL);
                if (fp->script) {
                    js_MarkScript(cx, fp->script, NULL);
                    if (fp->spbase) {
                        /*
                         * Don't mark what has not been pushed yet, or what
                         * has been popped already.
                         */
                        depth = fp->script->depth;
                        nslots = (JS_UPTRDIFF(fp->sp, fp->spbase)
                                  < depth * sizeof(jsval))
                                 ? (uintN)(fp->sp - fp->spbase)
                                 : depth;
                        GC_MARK_JSVALS(cx, nslots, fp->spbase, "operand");
                    }
                }
                GC_MARK(cx, fp->thisp, "this", NULL);
                if (fp->argv) {
                    nslots = fp->argc;
                    if (fp->fun) {
                        if (fp->fun->nargs > nslots)
                            nslots = fp->fun->nargs;
                        nslots += fp->fun->extra;
                    }
                    GC_MARK_JSVALS(cx, nslots, fp->argv, "arg");
                }
                if (JSVAL_IS_GCTHING(fp->rval))
                    GC_MARK(cx, JSVAL_TO_GCTHING(fp->rval), "rval", NULL);
                if (fp->vars)
                    GC_MARK_JSVALS(cx, fp->nvars, fp->vars, "var");
                GC_MARK(cx, fp->scopeChain, "scope chain", NULL);
                if (fp->sharpArray)
                    GC_MARK(cx, fp->sharpArray, "sharp array", NULL);

                if (fp->xmlNamespace)
                    GC_MARK(cx, fp->xmlNamespace, "xmlNamespace", NULL);
            } while ((fp = fp->down) != NULL);
        }

        /* Cleanup temporary "dormant" linkage. */
        if (acx->fp)
            acx->fp->dormantNext = NULL;

        /* Mark other roots-by-definition in acx. */
        GC_MARK(cx, acx->globalObject, "global object", NULL);
        for (i = 0; i < GCX_NTYPES; i++)
            GC_MARK(cx, acx->newborn[i], gc_typenames[i], NULL);
        if (acx->lastAtom)
            GC_MARK_ATOM(cx, acx->lastAtom, NULL);
        if (JSVAL_IS_GCTHING(acx->lastInternalResult)) {
            thing = JSVAL_TO_GCTHING(acx->lastInternalResult);
            if (thing)
                GC_MARK(cx, thing, "lastInternalResult", NULL);
        }
#if JS_HAS_EXCEPTIONS
        if (acx->throwing && JSVAL_IS_GCTHING(acx->exception))
            GC_MARK(cx, JSVAL_TO_GCTHING(acx->exception), "exception", NULL);
#endif
#if JS_HAS_LVALUE_RETURN
        if (acx->rval2set && JSVAL_IS_GCTHING(acx->rval2))
            GC_MARK(cx, JSVAL_TO_GCTHING(acx->rval2), "rval2", NULL);
#endif

        for (sh = acx->stackHeaders; sh; sh = sh->down) {
            METER(rt->gcStats.stackseg++);
            METER(rt->gcStats.segslots += sh->nslots);
            GC_MARK_JSVALS(cx, sh->nslots, JS_STACK_SEGMENT(sh), "stack");
        }

        if (acx->localRootStack)
            js_MarkLocalRoots(cx, acx->localRootStack);
    }
#ifdef DUMP_CALL_TABLE
    js_DumpCallTable(cx);
#endif

    if (rt->gcCallback)
        (void) rt->gcCallback(cx, JSGC_MARK_END);

    /*
     * Sweep phase.
     *
     * Finalize as we sweep, outside of rt->gcLock, but with rt->gcRunning set
     * so that any attempt to allocate a GC-thing from a finalizer will fail,
     * rather than nest badly and leave the unmarked newborn to be swept.
     *
     * Finalize smaller objects before larger, to guarantee finalization of
     * GC-allocated obj->slots after obj.  See FreeSlots in jsobj.c.
     */
    js_SweepAtomState(&rt->atomState);
    js_SweepScopeProperties(rt);
    js_SweepScriptFilenames(rt);
    for (i = 0; i < GC_NUM_FREELISTS; i++) {
        nbytes = GC_FREELIST_NBYTES(i);
        nflags = nbytes / sizeof(JSGCThing);

        for (a = rt->gcArenaPool[i].first.next; a; a = a->next) {
            flagp = (uint8 *) a->base;
            split = (uint8 *) FIRST_THING_PAGE(a);
            limit = (JSGCThing *) a->avail;
            for (thing = (JSGCThing *) split; thing < limit; thing += nflags) {
                if (((jsuword)thing & GC_PAGE_MASK) == 0) {
                    thing = (JSGCThing *) FIRST_THING((jsuword)thing, nbytes);
                    flagp = js_GetGCThingFlags(thing);
                }
                flags = *flagp;
                if (flags & GCF_MARK) {
                    *flagp &= ~GCF_MARK;
                } else if (!(flags & (GCF_LOCK | GCF_FINAL))) {
                    /* Call the finalizer with GCF_FINAL ORed into flags. */
                    type = flags & GCF_TYPEMASK;
                    finalizer = gc_finalizers[type];
                    if (finalizer) {
                        *flagp = (uint8)(flags | GCF_FINAL);
                        if (type >= GCX_EXTERNAL_STRING)
                            js_PurgeDeflatedStringCache((JSString *)thing);
                        finalizer(cx, thing);
                    }

                    /* Set flags to GCF_FINAL, signifying that thing is free. */
                    *flagp = GCF_FINAL;

                    bytesptr = (type == GCX_PRIVATE)
                               ? &rt->gcPrivateBytes
                               : &rt->gcBytes;
                    JS_ASSERT(*bytesptr >= nbytes + nflags);
                    *bytesptr -= nbytes + nflags;
                }
                flagp += nflags;
                if (JS_UPTRDIFF(flagp, split) < nflags)
                    flagp += GC_THINGS_SIZE;
            }
        }
    }

    /*
     * Free phase.
     * Free any unused arenas and rebuild the JSGCThing freelist.
     */
    for (i = 0; i < GC_NUM_FREELISTS; i++) {
        ap = &rt->gcArenaPool[i].first.next;
        a = *ap;
        if (!a)
            continue;

        all_clear = JS_TRUE;
        flp = oflp = &rt->gcFreeList[i];
        *flp = NULL;
        METER(rt->gcStats.freelen[i] = 0);

        nbytes = GC_FREELIST_NBYTES(i);
        nflags = nbytes / sizeof(JSGCThing);
        do {
            flagp = (uint8 *) a->base;
            split = (uint8 *) FIRST_THING_PAGE(a);
            limit = (JSGCThing *) a->avail;
            for (thing = (JSGCThing *) split; thing < limit; thing += nflags) {
                if (((jsuword)thing & GC_PAGE_MASK) == 0) {
                    thing = (JSGCThing *) FIRST_THING((jsuword)thing, nbytes);
                    flagp = js_GetGCThingFlags(thing);
                }
                if (*flagp != GCF_FINAL) {
                    all_clear = JS_FALSE;
                } else {
                    thing->flagp = flagp;
                    *flp = thing;
                    flp = &thing->next;
                    METER(rt->gcStats.freelen[i]++);
                }
                flagp += nflags;
                if (JS_UPTRDIFF(flagp, split) < nflags)
                    flagp += GC_THINGS_SIZE;
            }

            if (all_clear) {
                JS_ARENA_DESTROY(&rt->gcArenaPool[i], a, ap);
                flp = oflp;
                METER(rt->gcStats.afree++);
            } else {
                ap = &a->next;
                all_clear = JS_TRUE;
                oflp = flp;
            }
        } while ((a = *ap) != NULL);

        /* Terminate the new freelist. */
        *flp = NULL;
    }

    if (rt->gcCallback)
        (void) rt->gcCallback(cx, JSGC_FINALIZE_END);
#ifdef DEBUG_notme
  { extern void DumpSrcNoteSizeHist();
    DumpSrcNoteSizeHist();
    printf("GC HEAP SIZE %lu (%lu)\n",
           (unsigned long)rt->gcBytes, (unsigned long)rt->gcPrivateBytes);
  }
#endif

    JS_LOCK_GC(rt);
    if (rt->gcLevel > 1 || rt->gcPoke) {
        rt->gcLevel = 1;
        rt->gcPoke = JS_FALSE;
        JS_UNLOCK_GC(rt);
        goto restart;
    }
    js_EnablePropertyCache(cx);
    rt->gcLevel = 0;
    rt->gcLastBytes = rt->gcBytes;
    rt->gcRunning = JS_FALSE;

#ifdef JS_THREADSAFE
    /* If we were invoked during a request, pay back the temporary debit. */
    if (requestDebit)
        rt->requestCount += requestDebit;
    rt->gcThread = 0;
    JS_NOTIFY_GC_DONE(rt);
    if (!(gcflags & GC_ALREADY_LOCKED))
        JS_UNLOCK_GC(rt);
#endif

    if (rt->gcCallback) {
        if (gcflags & GC_ALREADY_LOCKED)
            JS_UNLOCK_GC(rt);
        (void) rt->gcCallback(cx, JSGC_END);
        if (gcflags & GC_ALREADY_LOCKED)
            JS_LOCK_GC(rt);
    }
}
