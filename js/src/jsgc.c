/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

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
#include <string.h>	/* for memset, called by jsarena.h macros if DEBUG */
#include "jstypes.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jshash.h" /* Added by JSIFY */
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

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
 * The private JSGCThing struct, which describes a gcFreelist element.
 */
struct JSGCThing {
    JSGCThing   *next;
    uint8       *flagp;
};

/*
 * A GC arena contains one flag byte for every thing in its heap, and supports
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
 *  pi->flags + ((jsuword)thing % 1023) / sizeof(JSGCThing) >= pi->split
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

static JSGCThing *
gc_new_arena(JSArenaPool *pool)
{
    uint8 *flagp, *split, *pagep, *limit;
    JSArena *a;
    JSGCThing *thing;
    JSGCPageInfo *pi;

    /* Use JS_ArenaAllocate to grab another 9K-net-size hunk of space. */
    flagp = (uint8 *) JS_ArenaAllocate(pool, GC_ARENA_SIZE);
    if (!flagp)
        return NULL;
    a = pool->current;

    /* Reset a->avail to start at the flags split, aka the first thing page. */
    a->avail = FIRST_THING_PAGE(a);
    split = pagep = (uint8 *) a->avail;
    a->avail += sizeof(JSGCPageInfo);
    thing = (JSGCThing *) a->avail;
    a->avail += sizeof(JSGCThing);

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

    return !(flags & (GCF_MARK | GCF_LOCKMASK | GCF_FINAL));
}

typedef void (*GCFinalizeOp)(JSContext *cx, JSGCThing *thing);

static GCFinalizeOp gc_finalizers[GCX_NTYPES];

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
    JS_ASSERT(sizeof(JSGCThing) == sizeof(JSGCPageInfo));
    JS_ASSERT(sizeof(JSGCThing) >= sizeof(JSObject));
    JS_ASSERT(sizeof(JSGCThing) >= sizeof(JSString));
    JS_ASSERT(sizeof(JSGCThing) >= sizeof(jsdouble));
    JS_ASSERT(GC_FLAGS_SIZE >= GC_PAGE_SIZE);
    JS_ASSERT(sizeof(JSStackHeader) >= 2 * sizeof(jsval));

    if (!gc_finalizers[GCX_OBJECT]) {
	gc_finalizers[GCX_OBJECT] = (GCFinalizeOp)js_FinalizeObject;
	gc_finalizers[GCX_STRING] = (GCFinalizeOp)js_FinalizeString;
#ifdef DEBUG
	gc_finalizers[GCX_DOUBLE] = (GCFinalizeOp)js_FinalizeDouble;
#endif
    }

    JS_InitArenaPool(&rt->gcArenaPool, "gc-arena", GC_ARENA_SIZE,
		     sizeof(JSGCThing));
    if (!JS_DHashTableInit(&rt->gcRootsHash, JS_DHashGetStubOps(), NULL,
                           sizeof(JSGCRootHashEntry), GC_ROOTS_SIZE)) {
        return JS_FALSE;
    }
    rt->gcLocksHash = NULL;     /* create lazily */
    rt->gcMaxBytes = maxbytes;
    return JS_TRUE;
}

#ifdef JS_GCMETER
void
js_DumpGCStats(JSRuntime *rt, FILE *fp)
{
    fprintf(fp, "\nGC allocation statistics:\n");
    fprintf(fp, "     bytes currently allocated: %lu\n", rt->gcBytes);
    fprintf(fp, "                alloc attempts: %lu\n", rt->gcStats.alloc);
    fprintf(fp, "            GC freelist length: %lu\n", rt->gcStats.freelen);
    fprintf(fp, "  recycles through GC freelist: %lu\n", rt->gcStats.recycle);
    fprintf(fp, "alloc retries after running GC: %lu\n", rt->gcStats.retry);
    fprintf(fp, "           allocation failures: %lu\n", rt->gcStats.fail);
    fprintf(fp, "              valid lock calls: %lu\n", rt->gcStats.lock);
    fprintf(fp, "            valid unlock calls: %lu\n", rt->gcStats.unlock);
    fprintf(fp, "   locks that hit stuck counts: %lu\n", rt->gcStats.stuck);
    fprintf(fp, " unlocks that saw stuck counts: %lu\n", rt->gcStats.unstuck);
    fprintf(fp, "          mark recursion depth: %lu\n", rt->gcStats.depth);
    fprintf(fp, "  maximum mark recursion depth: %lu\n", rt->gcStats.maxdepth);
    fprintf(fp, "      maximum GC nesting level: %lu\n", rt->gcStats.maxlevel);
    fprintf(fp, "   potentially useful GC calls: %lu\n", rt->gcStats.poke);
    fprintf(fp, "              useless GC calls: %lu\n", rt->gcStats.nopoke);
    fprintf(fp, "     thing arenas freed so far: %lu\n", rt->gcStats.afree);
    fprintf(fp, "  extra stack segments scanned: %lu\n", rt->gcStats.stackseg);
    fprintf(fp, "   stack segment slots scanned: %lu\n", rt->gcStats.segslots);
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
#ifdef JS_ARENAMETER
    JS_DumpArenaStats(stdout);
#endif
#ifdef JS_GCMETER
    js_DumpGCStats(rt, stdout);
#endif
    JS_FinishArenaPool(&rt->gcArenaPool);
    JS_ArenaFinish();

#ifdef DEBUG
    {
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
    }
#endif

    JS_DHashTableFinish(&rt->gcRootsHash);
    if (rt->gcLocksHash) {
        JS_DHashTableDestroy(rt->gcLocksHash);
        rt->gcLocksHash = NULL;
    }
    rt->gcFreeList = NULL;
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

void *
js_AllocGCThing(JSContext *cx, uintN flags)
{
    JSBool tried_gc;
    JSRuntime *rt;
    JSGCThing *thing;
    uint8 *flagp;

#ifdef TOO_MUCH_GC
    js_GC(cx, GC_KEEP_ATOMS);
    tried_gc = JS_TRUE;
#else
    tried_gc = JS_FALSE;
#endif

    rt = cx->runtime;
    JS_LOCK_GC(rt);
    JS_ASSERT(!rt->gcRunning);
    if (rt->gcRunning) {
        METER(rt->gcStats.finalfail++);
        JS_UNLOCK_GC(rt);
        return NULL;
    }
    METER(rt->gcStats.alloc++);
retry:
    thing = rt->gcFreeList;
    if (thing) {
	rt->gcFreeList = thing->next;
	flagp = thing->flagp;
	METER(rt->gcStats.freelen--);
	METER(rt->gcStats.recycle++);
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
            JSArenaPool *pool = &rt->gcArenaPool;
            JSArena *a = pool->current;
            size_t nb = sizeof(JSGCThing);
            jsuword p = a->avail;
            jsuword q = p + nb;

            if (q > (a->limit & ~GC_PAGE_MASK)) {
                thing = gc_new_arena(pool);
            } else {
                if ((p & GC_PAGE_MASK) == 0) {
                    /* Beware, p points to a JSGCPageInfo record! */
                    p = q;
                    q += nb;
                    JS_ArenaCountAllocation(pool, nb);
                }
                a->avail = q;
                thing = (JSGCThing *)p;
            }
            JS_ArenaCountAllocation(pool, nb);
        }

        /* Consider doing a "last ditch" GC if thing couldn't be allocated. */
        if (!thing) {
            if (!tried_gc) {
                JS_UNLOCK_GC(rt);
                js_GC(cx, GC_KEEP_ATOMS);
                tried_gc = JS_TRUE;
                JS_LOCK_GC(rt);
                METER(rt->gcStats.retry++);
                goto retry;
            }
            METER(rt->gcStats.fail++);
            JS_UNLOCK_GC(rt);
            JS_ReportOutOfMemory(cx);
            return NULL;
        }

        /* Find the flags pointer given thing's address. */
        flagp = js_GetGCThingFlags(thing);
    }
    *flagp = (uint8)flags;
    rt->gcBytes += sizeof(JSGCThing) + sizeof(uint8);
    cx->newborn[flags & GCF_TYPEMASK] = thing;

    /*
     * Clear thing before unlocking in case a GC run is about to scan it,
     * finding it via cx->newborn[].
     */
    thing->next = NULL;
    thing->flagp = NULL;
    JS_UNLOCK_GC(rt);
    return thing;
}

JSBool
js_LockGCThing(JSContext *cx, void *thing)
{
    JSRuntime *rt;
    uint8 *flagp, flags, lockbits;
    JSBool ok;
    JSGCLockHashEntry *lhe;

    if (!thing)
	return JS_TRUE;
    flagp = js_GetGCThingFlags(thing);
    flags = *flagp;

    ok = JS_TRUE;
    rt = cx->runtime;
    JS_LOCK_GC(rt);
    lockbits = (flags & GCF_LOCKMASK);

    if (lockbits != GCF_LOCKMASK) {
        if ((flags & GCF_TYPEMASK) == GCX_OBJECT) {
            /* Objects may require "deep locking", i.e., rooting by value. */
            if (lockbits == 0) {
                if (!rt->gcLocksHash) {
                    rt->gcLocksHash = 
                        JS_NewDHashTable(JS_DHashGetStubOps(), NULL,
                                         sizeof(JSGCLockHashEntry),
                                         GC_ROOTS_SIZE);
                    if (!rt->gcLocksHash)
                        goto outofmem;
                } else {
#ifdef DEBUG
                    JSDHashEntryHdr *hdr = 
                        JS_DHashTableOperate(rt->gcLocksHash, thing,
                                             JS_DHASH_LOOKUP);
                    JS_ASSERT(JS_DHASH_ENTRY_IS_FREE(hdr));
#endif
                }
                lhe = (JSGCLockHashEntry *)
                    JS_DHashTableOperate(rt->gcLocksHash, thing, JS_DHASH_ADD);
                if (!lhe)
                    goto outofmem;
                lhe->thing = thing;
                lhe->count = 1;
                *flagp = (uint8)(flags + GCF_LOCK);
            } else {
                JS_ASSERT(lockbits == GCF_LOCK);
                lhe = (JSGCLockHashEntry *)
                    JS_DHashTableOperate(rt->gcLocksHash, thing,
                                         JS_DHASH_LOOKUP);
                JS_ASSERT(JS_DHASH_ENTRY_IS_BUSY(&lhe->hdr));
                if (JS_DHASH_ENTRY_IS_BUSY(&lhe->hdr)) {
                    JS_ASSERT(lhe->count >= 1);
                    lhe->count++;
                }
            }
        } else {
            *flagp = (uint8)(flags + GCF_LOCK);
        }
    } else {
	METER(rt->gcStats.stuck++);
    }

    METER(rt->gcStats.lock++);
out:
    JS_UNLOCK_GC(rt);
    return ok;

outofmem:
    JS_ReportOutOfMemory(cx);
    ok = JS_FALSE;
    goto out;
}

JSBool
js_UnlockGCThing(JSContext *cx, void *thing)
{
    JSRuntime *rt;
    uint8 *flagp, flags, lockbits;
    JSGCLockHashEntry *lhe;

    if (!thing)
	return JS_TRUE;
    flagp = js_GetGCThingFlags(thing);
    flags = *flagp;

    rt = cx->runtime;
    JS_LOCK_GC(rt);
    lockbits = (flags & GCF_LOCKMASK);

    if (lockbits != GCF_LOCKMASK) {
        if ((flags & GCF_TYPEMASK) == GCX_OBJECT) {
            /* Defend against a call on an unlocked object. */
            if (lockbits != 0) {
                JS_ASSERT(lockbits == GCF_LOCK);
                lhe = (JSGCLockHashEntry *)
                    JS_DHashTableOperate(rt->gcLocksHash, thing,
                                         JS_DHASH_LOOKUP);
                JS_ASSERT(JS_DHASH_ENTRY_IS_BUSY(&lhe->hdr));
                if (JS_DHASH_ENTRY_IS_BUSY(&lhe->hdr) &&
                    --lhe->count == 0) {
                    (void) JS_DHashTableOperate(rt->gcLocksHash, thing,
                                                JS_DHASH_REMOVE);
                    *flagp = (uint8)(flags & ~GCF_LOCKMASK);
                }
            }
        } else {
            *flagp = (uint8)(flags - GCF_LOCK);
        }
    } else {
	METER(rt->gcStats.unstuck++);
    }

    rt->gcPoke = JS_TRUE;
    METER(rt->gcStats.unlock++);
    JS_UNLOCK_GC(rt);
    return JS_TRUE;
}

#ifdef GC_MARK_DEBUG

#include <stdio.h>
#include <stdlib.h>
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

    if (flagp && ((*flagp & GCF_TYPEMASK) == GCX_OBJECT)) {
        JSObject  *obj = (JSObject *)thing;
        JSClass   *clasp = JSVAL_TO_PRIVATE(obj->slots[JSSLOT_CLASS]);
        className = clasp->name;
#ifdef HAVE_XPCONNECT
        if ((clasp->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS) &&
            (clasp->flags & JSCLASS_HAS_PRIVATE)) {
            jsval privateValue = obj->slots[JSSLOT_PRIVATE];
            void  *privateThing = JSVAL_IS_VOID(privateValue)
                                  ? NULL
                                  : JSVAL_TO_PRIVATE(privateValue);

            const char *xpcClassName = GetXPCObjectClassName(privateThing);
            if (xpcClassName)
                className = xpcClassName;
        }
#endif
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
        path = JS_sprintf_append(path, "%s(%s).",
                                 next->name,
                                 gc_object_class_name(next->thing));
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
        fprintf(fp, "object %08p %s", privateThing, className);
        break;
      }
      case GCX_STRING:
      case GCX_EXTERNAL_STRING:
      default:
        fprintf(fp, "string %s", JS_GetStringBytes((JSString *)thing));
        break;
      case GCX_DOUBLE:
        fprintf(fp, "double %g", *(jsdouble *)thing);
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
}

void
js_MarkGCThing(JSContext *cx, void *thing, void *arg)
{
    uint8 flags, *flagp;
    JSRuntime *rt;
    JSObject *obj;
    uint32 nslots;
    jsval v, *vp, *end;
#ifdef GC_MARK_DEBUG
    JSScope *scope;
    JSScopeProperty *sprop;
#endif

    if (!thing)
	return;

    flagp = js_GetGCThingFlags(thing);
    flags = *flagp;
    JS_ASSERT(flags != GCF_FINAL);
#ifdef GC_MARK_DEBUG
    if (js_LiveThingToFind == thing)
        gc_dump_thing(thing, flags, arg, stderr);
#endif

    if (flags & GCF_MARK)
	return;

    *flagp |= GCF_MARK;
    rt = cx->runtime;
    METER(if (++rt->gcStats.depth > rt->gcStats.maxdepth)
	      rt->gcStats.maxdepth = rt->gcStats.depth);

#ifdef GC_MARK_DEBUG
    if (js_DumpGCHeap)
        gc_dump_thing(thing, flags, arg, js_DumpGCHeap);
#endif

    if ((flags & GCF_TYPEMASK) == GCX_OBJECT) {
	obj = (JSObject *) thing;
	vp = obj->slots;
        if (!vp) {
            /* If obj->slots is null, obj must be a newborn. */
            JS_ASSERT(!obj->map);
            goto out;
        }
        nslots = (obj->map->ops->mark)
                 ? obj->map->ops->mark(cx, obj, arg)
                 : JS_MIN(obj->map->freeslot, obj->map->nslots);
#ifdef GC_MARK_DEBUG
        scope = OBJ_IS_NATIVE(obj) ? OBJ_SCOPE(obj) : NULL;
#endif
        for (end = vp + nslots; vp < end; vp++) {
            v = *vp;
            if (JSVAL_IS_GCTHING(v)) {
#ifdef GC_MARK_DEBUG
                char name[32];

                if (scope) {
                    uint32 slot;
                    jsval nval;

                    slot = vp - obj->slots;
                    for (sprop = scope->props; ; sprop = sprop->next) {
                        if (!sprop) {
                            switch (slot) {
                              case JSSLOT_PROTO:
                                strcpy(name, "__proto__");
                                break;
                              case JSSLOT_PARENT:
                                strcpy(name, "__parent__");
                                break;
                              case JSSLOT_PRIVATE:
                                strcpy(name, "__private__");
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
                            nval = sprop->symbols
                                   ? js_IdToValue(sym_id(sprop->symbols))
                                   : sprop->id;
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
                GC_MARK(cx, JSVAL_TO_GCTHING(v), name, arg);
            }
        }
    }

out:
    METER(rt->gcStats.depth--);
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
        JSArena *a;
        jsuword firstpage;
        JSBool root_points_to_gcArenaPool = JS_FALSE;
	void *thing = JSVAL_TO_GCTHING(v);

        for (a = cx->runtime->gcArenaPool.first.next; a; a = a->next) {
            firstpage = FIRST_THING_PAGE(a);
	    if (JS_UPTRDIFF(thing, firstpage) < a->avail - firstpage) {
                root_points_to_gcArenaPool = JS_TRUE;
                break;
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

JS_FRIEND_API(void)
js_ForceGC(JSContext *cx)
{
    uintN i;

    for (i = 0; i < GCX_NTYPES; i++)
        cx->newborn[i] = NULL;
    cx->runtime->gcPoke = JS_TRUE;
    js_GC(cx, 0);
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
    uintN i, depth, nslots;
    JSStackHeader *sh;
    JSArena *a, **ap;
    uint8 flags, *flagp, *split;
    JSGCThing *thing, *limit, **flp, **oflp;
    GCFinalizeOp finalizer;
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

    /* Don't run gc if it is disabled (unless this is the last context). */
    if (rt->gcDisabled && !(gcflags & GC_LAST_CONTEXT))
        return;

    /*
     * Let the API user decide to defer a GC if it wants to (unless this
     * is the last context). Call the callback regardless.
     */
    if (rt->gcCallback) {
        if (!rt->gcCallback(cx, JSGC_BEGIN) && !(gcflags & GC_LAST_CONTEXT))
            return;
    }

    /* Lock out other GC allocator and collector invocations. */
    JS_LOCK_GC(rt);

    /* Do nothing if no assignment has executed since the last GC. */
    if (!rt->gcPoke) {
	METER(rt->gcStats.nopoke++);
	JS_UNLOCK_GC(rt);
	return;
    }
    rt->gcPoke = JS_FALSE;
    METER(rt->gcStats.poke++);

#ifdef JS_THREADSAFE
    /* Bump gcLevel and return rather than nest on this thread. */
    currentThread = js_CurrentThreadId();
    if (rt->gcThread == currentThread) {
	JS_ASSERT(rt->gcLevel > 0);
	rt->gcLevel++;
	METER(if (rt->gcLevel > rt->gcStats.maxlevel)
		  rt->gcStats.maxlevel = rt->gcLevel);
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
        while ((acx = js_ContextIterator(rt, &iter)) != NULL) {
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

    /* Reset malloc counter */
    rt->gcMallocBytes = 0;

    /* Drop atoms held by the property cache, and clear property weak links. */
    js_DisablePropertyCache(cx);
    js_FlushPropertyCache(cx);
restart:
    rt->gcNumber++;

    /*
     * Mark phase.
     */
    JS_DHashTableEnumerate(&rt->gcRootsHash, gc_root_marker, cx);
    if (rt->gcLocksHash)
        JS_DHashTableEnumerate(rt->gcLocksHash, gc_lock_marker, cx);
    js_MarkAtomState(&rt->atomState, gcflags, gc_mark_atom_key_thing, cx);
    iter = NULL;
    while ((acx = js_ContextIterator(rt, &iter)) != NULL) {
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
                                 ? fp->sp - fp->spbase
                                 : depth;
                        GC_MARK_JSVALS(cx, nslots, fp->spbase, "operand");
                    }
                }
                GC_MARK(cx, fp->thisp, "this", NULL);
                if (fp->argv)
                    GC_MARK_JSVALS(cx, fp->argc, fp->argv, "arg");
                if (JSVAL_IS_GCTHING(fp->rval))
                    GC_MARK(cx, JSVAL_TO_GCTHING(fp->rval), "rval", NULL);
                if (fp->vars)
                    GC_MARK_JSVALS(cx, fp->nvars, fp->vars, "var");
                GC_MARK(cx, fp->scopeChain, "scope chain", NULL);
                if (fp->sharpArray)
                    GC_MARK(cx, fp->sharpArray, "sharp array", NULL);
            } while ((fp = fp->down) != NULL);
	}

	/* Cleanup temporary "dormant" linkage. */
	if (acx->fp)
	    acx->fp->dormantNext = NULL;

        /* Mark other roots-by-definition in acx. */
	GC_MARK(cx, acx->globalObject, "global object", NULL);
	GC_MARK(cx, acx->newborn[GCX_OBJECT], "newborn object", NULL);
	GC_MARK(cx, acx->newborn[GCX_STRING], "newborn string", NULL);
	GC_MARK(cx, acx->newborn[GCX_DOUBLE], "newborn double", NULL);
	for (i = GCX_EXTERNAL_STRING; i < GCX_NTYPES; i++)
            GC_MARK(cx, acx->newborn[i], "newborn external string", NULL);
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
    }

    if (rt->gcCallback)
        (void) rt->gcCallback(cx, JSGC_MARK_END);

    /*
     * Sweep phase.
     * Finalize as we sweep, outside of rt->gcLock, but with rt->gcRunning set
     * so that any attempt to allocate a GC-thing from a finalizer will fail,
     * rather than nest badly and leave the unmarked newborn to be swept.
     */
    js_SweepAtomState(&rt->atomState);
    for (a = rt->gcArenaPool.first.next; a; a = a->next) {
        flagp = (uint8 *) a->base;
        split = (uint8 *) FIRST_THING_PAGE(a);
        limit = (JSGCThing *) a->avail;
        for (thing = (JSGCThing *) split; thing < limit; thing++) {
            if (((jsuword)thing & GC_PAGE_MASK) == 0) {
                flagp++;
                thing++;
            }
            flags = *flagp;
            if (flags & GCF_MARK) {
                *flagp &= ~GCF_MARK;
            } else if (!(flags & (GCF_LOCKMASK | GCF_FINAL))) {
                /* Call the finalizer with GCF_FINAL ORed into flags. */
                finalizer = gc_finalizers[flags & GCF_TYPEMASK];
                if (finalizer) {
                    *flagp = (uint8)(flags | GCF_FINAL);
                    finalizer(cx, thing);
                }

                /* Set flags to GCF_FINAL, signifying that thing is free. */
                *flagp = GCF_FINAL;

                JS_ASSERT(rt->gcBytes >= sizeof(JSGCThing) + sizeof(uint8));
                rt->gcBytes -= sizeof(JSGCThing) + sizeof(uint8);
            }
            if (++flagp == split)
                flagp += GC_THINGS_SIZE;
        }
    }

    /*
     * Free phase.
     * Free any unused arenas and rebuild the JSGCThing freelist.
     */
    ap = &rt->gcArenaPool.first.next;
    a = *ap;
    if (!a)
	goto out;
    all_clear = JS_TRUE;
    flp = oflp = &rt->gcFreeList;
    *flp = NULL;
    METER(rt->gcStats.freelen = 0);

    do {
        flagp = (uint8 *) a->base;
        split = (uint8 *) FIRST_THING_PAGE(a);
        limit = (JSGCThing *) a->avail;
        for (thing = (JSGCThing *) split; thing < limit; thing++) {
            if (((jsuword)thing & GC_PAGE_MASK) == 0) {
                flagp++;
                thing++;
            }
            if (*flagp != GCF_FINAL) {
                all_clear = JS_FALSE;
            } else {
                thing->flagp = flagp;
                *flp = thing;
                flp = &thing->next;
                METER(rt->gcStats.freelen++);
            }
            if (++flagp == split)
                flagp += GC_THINGS_SIZE;
        }

        if (all_clear) {
            JS_ARENA_DESTROY(&rt->gcArenaPool, a, ap);
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

    if (rt->gcCallback)
        (void) rt->gcCallback(cx, JSGC_FINALIZE_END);

out:
    JS_LOCK_GC(rt);
    if (rt->gcLevel > 1) {
        rt->gcLevel = 1;
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
    JS_UNLOCK_GC(rt);
#endif

    if (rt->gcCallback)
        (void) rt->gcCallback(cx, JSGC_END);
}
