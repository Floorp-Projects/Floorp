/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * This GC allocates fixed-sized things with sizes up to GC_NBYTES_MAX (see
 * jsgc.h). It allocates from a special GC arena pool with each arena allocated
 * using malloc. It uses an ideally parallel array of flag bytes to hold the
 * mark bit, finalizer type index, etc.
 *
 * XXX swizzle page to freelist for better locality of reference
 */
#include "jsstddef.h"
#include <stdlib.h>     /* for free */
#include <string.h>     /* for memset used when DEBUG */
#include "jstypes.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jshash.h" /* Added by JSIFY */
#include "jsapi.h"
#include "jsatom.h"
#include "jsbit.h"
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

/*
 * A GC arena contains one flag byte for each thing in its heap, and supports
 * O(1) lookup of a flag given its thing's address.
 *
 * To implement this, we take advantage of the thing/flags numerology: given
 * the 8K bytes worth of GC-things, there are 1K flag bytes. Within each 9K
 * allocation for things+flags there are always 8 consecutive 1K-pages each
 * aligned on 1K boundary. We use these pages to allocate things and the
 * remaining 1K of space before and after the aligned pages to store flags.
 * If we are really lucky and things+flags starts on a 1K boundary, then
 * flags would consist of a single 1K chunk that comes after 8K of things.
 * Otherwise there are 2 chunks of flags, one before and one after things.
 *
 * To be able to find the flag byte for a particular thing, we put a
 * JSGCPageInfo record at the beginning of each 1K-aligned page to hold that
 * page's offset from the beginning of things+flags allocation and we allocate
 * things after this record. Thus for each thing |thing_address & ~1023|
 * gives the address of a JSGCPageInfo record from which we read page_offset.
 * Due to page alignment
 *  (page_offset & ~1023) + (thing_address & 1023)
 * gives thing_offset from the beginning of 8K paged things. We then divide
 * thing_offset by sizeof(JSGCThing) to get thing_index.
 *
 * Now |page_address - page_offset| is things+flags arena_address and
 * (page_offset & 1023) is the offset of the first page from the start of
 * things+flags area. Thus if
 *  thing_index < (page_offset & 1023)
 * then
 *  allocation_start_address + thing_index < address_of_the_first_page
 * and we use
 *  allocation_start_address + thing_index
 * as the address to store thing's flags. If
 *  thing_index >= (page_offset & 1023),
 * then we use the chunk of flags that comes after the pages with things
 * and calculate the address for the flag byte as
 *  address_of_the_first_page + 8K + (thing_index - (page_offset & 1023))
 * which is just
 *  allocation_start_address + thing_index + 8K.
 *
 * When we allocate things with size equal to sizeof(JSGCThing), the overhead
 * of this scheme for 32 bit platforms is (8+8*(8+1))/(8+9K) or 0.87%
 * (assuming 4 bytes for each JSGCArena header, and 8 bytes for each
 * JSGCThing and JSGCPageInfo). When thing_size > 8, the scheme wastes the
 * flag byte for each extra 8 bytes beyond sizeof(JSGCThing) in thing_size
 * and the overhead is close to 1/8 or 12.5%.
 * FIXME: How can we avoid this overhead?
 *
 * Here's some ASCII art showing an arena:
 *
 *   split or the first 1-K aligned address.
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
 * side.  All the other pages have flags only below or only above.
 *
 * (If we need to implement card-marking for an incremental GC write barrier,
 * we can replace word-sized offsetInArena in JSGCPageInfo by pair of
 * uint8 card_mark and uint16 offsetInArena fields as the offset can not exceed
 * GC_THINGS_SIZE. This would gives an extremely efficient write barrier:
 * when mutating an object obj, just store a 1 byte at
 * (uint8 *) ((jsuword)obj & ~1023) on 32-bit platforms.)
 */
#define GC_PAGE_SHIFT   10
#define GC_PAGE_MASK    ((jsuword) JS_BITMASK(GC_PAGE_SHIFT))
#define GC_PAGE_SIZE    JS_BIT(GC_PAGE_SHIFT)
#define GC_PAGE_COUNT   (1 << (GC_THINGS_SHIFT - GC_PAGE_SHIFT))

typedef struct JSGCPageInfo {
    jsuword     offsetInArena;          /* offset from the arena start */
    jsuword     unscannedBitmap;        /* bitset for fast search of marked
                                           but not yet scanned GC things */
} JSGCPageInfo;

struct JSGCArena {
    JSGCArenaList       *list;          /* allocation list for the arena */
    JSGCArena           *prev;          /* link field for allocation list */
    JSGCArena           *prevUnscanned; /* link field for the list of arenas
                                           with marked but not yet scanned
                                           things */
    jsuword             unscannedPages; /* bitset for fast search of pages
                                           with marked but not yet scanned
                                           things */
    uint8               base[1];        /* things+flags allocation area */
};

#define GC_ARENA_SIZE                                                         \
    (offsetof(JSGCArena, base) + GC_THINGS_SIZE + GC_FLAGS_SIZE)

#define FIRST_THING_PAGE(a)                                                   \
    (((jsuword)(a)->base + GC_FLAGS_SIZE - 1) & ~GC_PAGE_MASK)

#define PAGE_TO_ARENA(pi)                                                     \
    ((JSGCArena *)((jsuword)(pi) - (pi)->offsetInArena                        \
                   - offsetof(JSGCArena, base)))

#define PAGE_INDEX(pi)                                                        \
    ((size_t)((pi)->offsetInArena >> GC_PAGE_SHIFT))

#define THING_TO_PAGE(thing)                                                  \
    ((JSGCPageInfo *)((jsuword)(thing) & ~GC_PAGE_MASK))

/*
 * Given a thing size n, return the size of the gap from the page start before
 * the first thing.  We know that any n not a power of two packs from
 * the end of the page leaving at least enough room for one JSGCPageInfo, but
 * not for another thing, at the front of the page (JS_ASSERTs below insist
 * on this).
 *
 * This works because all allocations are a multiple of sizeof(JSGCThing) ==
 * sizeof(JSGCPageInfo) in size.
 */
#define PAGE_THING_GAP(n) (((n) & ((n) - 1)) ? (GC_PAGE_SIZE % (n)) : (n))

#ifdef JS_THREADSAFE
/*
 * The maximum number of things to put to the local free list by taking
 * several things from the global free list or from the tail of the last
 * allocated arena to amortize the cost of rt->gcLock.
 *
 * We use number 8 based on benchmarks from bug 312238.
 */
#define MAX_THREAD_LOCAL_THINGS 8

#endif

JS_STATIC_ASSERT(sizeof(JSGCThing) == sizeof(JSGCPageInfo));
JS_STATIC_ASSERT(sizeof(JSGCThing) >= sizeof(JSObject));
JS_STATIC_ASSERT(sizeof(JSGCThing) >= sizeof(JSString));
JS_STATIC_ASSERT(sizeof(JSGCThing) >= sizeof(jsdouble));
JS_STATIC_ASSERT(GC_FLAGS_SIZE >= GC_PAGE_SIZE);
JS_STATIC_ASSERT(sizeof(JSStackHeader) >= 2 * sizeof(jsval));

#ifdef JS_GCMETER
# define METER(x) x
#else
# define METER(x) ((void) 0)
#endif

static JSBool
NewGCArena(JSRuntime *rt, JSGCArenaList *arenaList, JSBool triedGC)
{
    JSGCArena *a;
    jsuword offset;
    JSGCPageInfo *pi;
    uint32 *bytesptr;

    /* Check if we are allowed and can allocate a new arena. */
    if (rt->gcBytes >= rt->gcMaxBytes)
        return JS_FALSE;
    if (!triedGC && rt->gcMallocBytes >= rt->gcMaxMallocBytes)
        return JS_FALSE;
    a = (JSGCArena *)malloc(GC_ARENA_SIZE);
    if (!a)
        return JS_FALSE;

    /* Initialize the JSGCPageInfo records at the start of every thing page. */
    offset = (GC_PAGE_SIZE - ((jsuword)a->base & GC_PAGE_MASK)) & GC_PAGE_MASK;
    JS_ASSERT((jsuword)a->base + offset == FIRST_THING_PAGE(a));
    do {
        pi = (JSGCPageInfo *) (a->base + offset);
        pi->offsetInArena = offset;
        pi->unscannedBitmap = 0;
        offset += GC_PAGE_SIZE;
    } while (offset < GC_THINGS_SIZE);

    METER(++arenaList->stats.narenas);
    METER(arenaList->stats.maxarenas
          = JS_MAX(arenaList->stats.maxarenas, arenaList->stats.narenas));

    a->list = arenaList;
    a->prev = arenaList->last;
    a->prevUnscanned = NULL;
    a->unscannedPages = 0;
    arenaList->last = a;
    arenaList->lastLimit = 0;

    bytesptr = (arenaList == &rt->gcArenaList[0])
               ? &rt->gcBytes
               : &rt->gcPrivateBytes;
    *bytesptr += GC_ARENA_SIZE;

    return JS_TRUE;
}

static void
DestroyGCArena(JSRuntime *rt, JSGCArenaList *arenaList, JSGCArena **ap)
{
    JSGCArena *a;
    uint32 *bytesptr;

    a = *ap;
    JS_ASSERT(a);
    bytesptr = (arenaList == &rt->gcArenaList[0])
               ? &rt->gcBytes
               : &rt->gcPrivateBytes;
    JS_ASSERT(*bytesptr >= GC_ARENA_SIZE);
    *bytesptr -= GC_ARENA_SIZE;
    METER(rt->gcStats.afree++);
    METER(--arenaList->stats.narenas);
    if (a == arenaList->last)
        arenaList->lastLimit = (uint16)(a->prev ? GC_THINGS_SIZE : 0);
    *ap = a->prev;

#ifdef DEBUG
    memset(a, JS_FREE_PATTERN, GC_ARENA_SIZE);
#endif
    free(a);
}

static void
InitGCArenaLists(JSRuntime *rt)
{
    uintN i, thingSize;
    JSGCArenaList *arenaList;

    for (i = 0; i < GC_NUM_FREELISTS; i++) {
        arenaList = &rt->gcArenaList[i];
        thingSize = GC_FREELIST_NBYTES(i);
        JS_ASSERT((size_t)(uint16)thingSize == thingSize);
        arenaList->last = NULL;
        arenaList->lastLimit = 0;
        arenaList->thingSize = (uint16)thingSize;
        arenaList->freeList = NULL;
        METER(memset(&arenaList->stats, 0, sizeof arenaList->stats));
    }
}

static void
FinishGCArenaLists(JSRuntime *rt)
{
    uintN i;
    JSGCArenaList *arenaList;

    for (i = 0; i < GC_NUM_FREELISTS; i++) {
        arenaList = &rt->gcArenaList[i];
        while (arenaList->last)
            DestroyGCArena(rt, arenaList, &arenaList->last);
        arenaList->freeList = NULL;
    }
}

uint8 *
js_GetGCThingFlags(void *thing)
{
    JSGCPageInfo *pi;
    jsuword offsetInArena, thingIndex;

    pi = THING_TO_PAGE(thing);
    offsetInArena = pi->offsetInArena;
    JS_ASSERT(offsetInArena < GC_THINGS_SIZE);
    thingIndex = ((offsetInArena & ~GC_PAGE_MASK) |
                  ((jsuword)thing & GC_PAGE_MASK)) / sizeof(JSGCThing);
    JS_ASSERT(thingIndex < GC_PAGE_SIZE);
    if (thingIndex >= (offsetInArena & GC_PAGE_MASK))
        thingIndex += GC_THINGS_SIZE;
    return (uint8 *)pi - offsetInArena + thingIndex;
}

JSRuntime*
js_GetGCStringRuntime(JSString *str)
{
    JSGCPageInfo *pi;
    JSGCArenaList *list;

    pi = THING_TO_PAGE(str);
    list = PAGE_TO_ARENA(pi)->list;

    JS_ASSERT(list->thingSize == sizeof(JSGCThing));
    JS_ASSERT(GC_FREELIST_INDEX(sizeof(JSGCThing)) == 0);

    return (JSRuntime *)((uint8 *)list - offsetof(JSRuntime, gcArenaList));
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

/* Initial size of the gcRootsHash table (SWAG, small enough to amortize). */
#define GC_ROOTS_SIZE   256
#define GC_FINALIZE_LEN 1024

JSBool
js_InitGC(JSRuntime *rt, uint32 maxbytes)
{
    InitGCArenaLists(rt);
    if (!JS_DHashTableInit(&rt->gcRootsHash, JS_DHashGetStubOps(), NULL,
                           sizeof(JSGCRootHashEntry), GC_ROOTS_SIZE)) {
        rt->gcRootsHash.ops = NULL;
        return JS_FALSE;
    }
    rt->gcLocksHash = NULL;     /* create lazily */

    /*
     * Separate gcMaxMallocBytes from gcMaxBytes but initialize to maxbytes
     * for default backward API compatibility.
     */
    rt->gcMaxBytes = rt->gcMaxMallocBytes = maxbytes;
    return JS_TRUE;
}

#ifdef JS_GCMETER
JS_FRIEND_API(void)
js_DumpGCStats(JSRuntime *rt, FILE *fp)
{
    uintN i;
    size_t totalThings, totalMaxThings, totalBytes;

    fprintf(fp, "\nGC allocation statistics:\n");

#define UL(x)       ((unsigned long)(x))
#define ULSTAT(x)   UL(rt->gcStats.x)
    totalThings = 0;
    totalMaxThings = 0;
    totalBytes = 0;
    for (i = 0; i < GC_NUM_FREELISTS; i++) {
        JSGCArenaList *list = &rt->gcArenaList[i];
        JSGCArenaStats *stats = &list->stats;
        if (stats->maxarenas == 0) {
            fprintf(fp, "ARENA LIST %u (thing size %lu): NEVER USED\n",
                    i, UL(GC_FREELIST_NBYTES(i)));
            continue;
        }
        fprintf(fp, "ARENA LIST %u (thing size %lu):\n",
                i, UL(GC_FREELIST_NBYTES(i)));
        fprintf(fp, "                     arenas: %lu\n", UL(stats->narenas));
        fprintf(fp, "                 max arenas: %lu\n", UL(stats->maxarenas));
        fprintf(fp, "                     things: %lu\n", UL(stats->nthings));
        fprintf(fp, "                 max things: %lu\n", UL(stats->maxthings));
        fprintf(fp, "                  free list: %lu\n", UL(stats->freelen));
        fprintf(fp, "          free list density: %.1f%%\n",
                stats->narenas == 0
                ? 0.0
                : (100.0 * list->thingSize * (jsdouble)stats->freelen /
                   (GC_THINGS_SIZE * (jsdouble)stats->narenas)));
        fprintf(fp, "  average free list density: %.1f%%\n",
                stats->totalarenas == 0
                ? 0.0
                : (100.0 * list->thingSize * (jsdouble)stats->totalfreelen /
                   (GC_THINGS_SIZE * (jsdouble)stats->totalarenas)));
        fprintf(fp, "                   recycles: %lu\n", UL(stats->recycle));
        fprintf(fp, "        recycle/alloc ratio: %.2f\n",
                (jsdouble)stats->recycle /
                (jsdouble)(stats->totalnew - stats->recycle));
        totalThings += stats->nthings;
        totalMaxThings += stats->maxthings;
        totalBytes += GC_FREELIST_NBYTES(i) * stats->nthings;
    }
    fprintf(fp, "TOTAL STATS:\n");
    fprintf(fp, "     public bytes allocated: %lu\n", UL(rt->gcBytes));
    fprintf(fp, "    private bytes allocated: %lu\n", UL(rt->gcPrivateBytes));
    fprintf(fp, "             alloc attempts: %lu\n", ULSTAT(alloc));
#ifdef JS_THREADSAFE
    fprintf(fp, "        alloc without locks: %1u\n", ULSTAT(localalloc));
#endif
    fprintf(fp, "            total GC things: %lu\n", UL(totalThings));
    fprintf(fp, "        max total GC things: %lu\n", UL(totalMaxThings));
    fprintf(fp, "             GC things size: %lu\n", UL(totalBytes));
    fprintf(fp, "allocation retries after GC: %lu\n", ULSTAT(retry));
    fprintf(fp, "        allocation failures: %lu\n", ULSTAT(fail));
    fprintf(fp, "         things born locked: %lu\n", ULSTAT(lockborn));
    fprintf(fp, "           valid lock calls: %lu\n", ULSTAT(lock));
    fprintf(fp, "         valid unlock calls: %lu\n", ULSTAT(unlock));
    fprintf(fp, "       mark recursion depth: %lu\n", ULSTAT(depth));
    fprintf(fp, "     maximum mark recursion: %lu\n", ULSTAT(maxdepth));
    fprintf(fp, "     mark C recursion depth: %lu\n", ULSTAT(cdepth));
    fprintf(fp, "   maximum mark C recursion: %lu\n", ULSTAT(maxcdepth));
    fprintf(fp, "      delayed scan bag adds: %lu\n", ULSTAT(unscanned));
#ifdef DEBUG
    fprintf(fp, "  max delayed scan bag size: %lu\n", ULSTAT(maxunscanned));
#endif
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
#ifdef JS_ARENAMETER
    JS_DumpArenaStats(stdout);
#endif
#ifdef JS_GCMETER
    js_DumpGCStats(rt, stdout);
#endif
    FinishGCArenaLists(rt);

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
    if (rt->gcRunning && rt->gcThread->id != js_CurrentThreadId()) {
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
    if (rt->gcRunning && rt->gcThread->id != js_CurrentThreadId()) {
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

#if defined(DEBUG_brendan) || defined(DEBUG_timeless)
#define DEBUG_gchist
#endif

#ifdef DEBUG_gchist
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
    JSRuntime *rt;
    uintN flindex;
    JSBool triedGC;
    JSGCThing *thing;
    uint8 *flagp, *firstPage;
    JSGCArenaList *arenaList;
    jsuword offset;
    JSGCArena *a;
    JSLocalRootStack *lrs;
#ifdef JS_THREADSAFE
    JSBool gcLocked;
    JSGCThing **flbase, **lastptr;
    JSGCThing *tmpthing;
    uint8 *tmpflagp;
    uintN maxFreeThings;         /* max to take from the global free list */
    METER(size_t nfree);
#endif

    rt = cx->runtime;
    METER(rt->gcStats.alloc++);        /* this is not thread-safe */
    nbytes = JS_ROUNDUP(nbytes, sizeof(JSGCThing));
    flindex = GC_FREELIST_INDEX(nbytes);

#ifdef JS_THREADSAFE
    gcLocked = JS_FALSE;
    JS_ASSERT(cx->thread);
    flbase = cx->thread->gcFreeLists;
    JS_ASSERT(flbase);
    thing = flbase[flindex];
    if (thing) {
        flagp = thing->flagp;
        flbase[flindex] = thing->next;
        METER(rt->gcStats.localalloc++);  /* this is not thread-safe */
        goto success;
    }

    JS_LOCK_GC(rt);
    gcLocked = JS_TRUE;
#endif
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
    triedGC = JS_TRUE;
#else
    triedGC = JS_FALSE;
#endif

    arenaList = &rt->gcArenaList[flindex];

  retry:
    /* Try to get thing from the free list first. */
    thing = arenaList->freeList;
    if (thing) {
        arenaList->freeList = thing->next;
        flagp = thing->flagp;
        JS_ASSERT(*flagp & GCF_FINAL);
        METER(arenaList->stats.freelen--);
        METER(arenaList->stats.recycle++);

#ifdef JS_THREADSAFE
        /*
         * Fill the local free list by taking several things from the
         * global free list.
         */
        JS_ASSERT(!flbase[flindex]);
        tmpthing = arenaList->freeList;
        if (tmpthing) {
            maxFreeThings = MAX_THREAD_LOCAL_THINGS;
            do {
                if (!tmpthing->next)
                    break;
                tmpthing = tmpthing->next;
            } while (--maxFreeThings != 0);

            flbase[flindex] = arenaList->freeList;
            arenaList->freeList = tmpthing->next;
            tmpthing->next = NULL;
        }
#endif
    } else {
        if ((!arenaList->last || arenaList->lastLimit == GC_THINGS_SIZE) &&
            !NewGCArena(rt, arenaList, triedGC)) {

            /*
             * Consider doing a "last ditch" GC when the free list is empty,
             * the last arena is full and we can not allocate a new arena.
             *
             * Keep rt->gcLock across the call into js_GC so we don't starve
             * and lose to racing threads who deplete the heap just after
             * js_GC has replenished it (or has synchronized with a racing
             * GC that collected a bunch of garbage).  This unfair scheduling
             * can happen on certain operating systems. For the gory details,
             * see bug 162779 at https://bugzilla.mozilla.org/.
             */
            if (triedGC)
                goto fail;
            rt->gcPoke = JS_TRUE;
            js_GC(cx, GC_KEEP_ATOMS | GC_ALREADY_LOCKED);
            if (JS_HAS_NATIVE_BRANCH_CALLBACK_OPTION(cx) &&
                cx->branchCallback && !cx->branchCallback(cx, NULL)) {
                METER(rt->gcStats.retryhalt++);
                JS_UNLOCK_GC(rt);
                return NULL;
            }
            triedGC = JS_TRUE;
            METER(rt->gcStats.retry++);
            goto retry;
        }

        /* The last arena should have room for more things at this point. */
        offset = arenaList->lastLimit;
        if ((offset & GC_PAGE_MASK) == 0) {
            /* Skip JSGCPageInfo record located at GC_PAGE_SIZE boundary. */
            offset += PAGE_THING_GAP(nbytes);
        }
        JS_ASSERT(offset + nbytes <= GC_THINGS_SIZE);
        arenaList->lastLimit = (uint16)(offset + nbytes);
        a = arenaList->last;
        firstPage = (uint8 *)FIRST_THING_PAGE(a);
        thing = (JSGCThing *)(firstPage + offset);
        flagp = a->base + offset / sizeof(JSGCThing);
        if (flagp >= firstPage)
            flagp += GC_THINGS_SIZE;
        METER(++arenaList->stats.nthings);
        METER(arenaList->stats.maxthings =
              JS_MAX(arenaList->stats.nthings, arenaList->stats.maxthings));

#ifdef JS_THREADSAFE
        /*
         * Refill the local free list by taking free things from the
         * last arena. Prefer to order free things by ascending addresses
         * in the (unscientific) hope of better cache locality.
         */
        JS_ASSERT(!flbase[flindex]);
        METER(nfree = 0);
        lastptr = &flbase[flindex];
        maxFreeThings = MAX_THREAD_LOCAL_THINGS;
        for (offset = arenaList->lastLimit;
             offset != GC_THINGS_SIZE && maxFreeThings-- != 0;
             offset += nbytes) {
            if ((offset & GC_PAGE_MASK) == 0)
                offset += PAGE_THING_GAP(nbytes);
            JS_ASSERT(offset + nbytes <= GC_THINGS_SIZE);
            tmpflagp = a->base + offset / sizeof(JSGCThing);
            if (tmpflagp >= firstPage)
                tmpflagp += GC_THINGS_SIZE;

            tmpthing = (JSGCThing *)(firstPage + offset);
            tmpthing->flagp = tmpflagp;
            *tmpflagp = GCF_FINAL;    /* signifying that thing is free */

            *lastptr = tmpthing;
            lastptr = &tmpthing->next;
            METER(++nfree);
        }
        arenaList->lastLimit = offset;
        *lastptr = NULL;
        METER(arenaList->stats.freelen += nfree);
#endif
    }

    /* We successfully allocated the thing. */
#ifdef JS_THREADSAFE
  success:
#endif
    lrs = cx->localRootStack;
    if (lrs) {
        /*
         * If we're in a local root scope, don't set cx->newborn[type] at all,
         * to avoid entraining garbage from it for an unbounded amount of time
         * on this context.  A caller will leave the local root scope and pop
         * this reference, allowing thing to be GC'd if it has no other refs.
         * See JS_EnterLocalRootScope and related APIs.
         */
        if (js_PushLocalRoot(cx, lrs, (jsval) thing) < 0) {
            /*
             * When we fail for a thing allocated through the tail of
             * the last arena, thing's flag byte is not initialized. So
             * to prevent GC accessing the uninitialized flags during
             * the finalization, we always mark the thing as final. See
             * bug 337407.
             */
            *flagp = GCF_FINAL;
            goto fail;
        }
    } else {
        /*
         * No local root scope, so we're stuck with the old, fragile model of
         * depending on a pigeon-hole newborn per type per context.
         */
        cx->newborn[flags & GCF_TYPEMASK] = thing;
    }

    /* We can't fail now, so update flags and rt->gc{,Private}Bytes. */
    *flagp = (uint8)flags;

    /*
     * Clear thing before unlocking in case a GC run is about to scan it,
     * finding it via cx->newborn[].
     */
    thing->next = NULL;
    thing->flagp = NULL;
#ifdef DEBUG_gchist
    gchist[gchpos].lastDitch = triedGC;
    gchist[gchpos].freeList = &rt->gcArenaList[flindex];
    if (++gchpos == NGCHIST)
        gchpos = 0;
#endif
    METER(if (flags & GCF_LOCK) rt->gcStats.lockborn++);
    METER(++rt->gcArenaList[flindex].stats.totalnew);
#ifdef JS_THREADSAFE
    if (gcLocked)
        JS_UNLOCK_GC(rt);
#endif
    return thing;

fail:
#ifdef JS_THREADSAFE
    if (gcLocked)
        JS_UNLOCK_GC(rt);
#endif
    METER(rt->gcStats.fail++);
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
    uint8 *flagp;
    uintN flags, lock, type;
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

typedef struct GCMarkNode GCMarkNode;

struct GCMarkNode {
    void        *thing;
    const char  *name;
    GCMarkNode  *next;
    GCMarkNode  *prev;
};

JS_FRIEND_DATA(FILE *) js_DumpGCHeap;
JS_EXPORT_DATA(void *) js_LiveThingToFind;

#ifdef HAVE_XPCONNECT
#include "dump_xpc.h"
#endif

static void
GetObjSlotName(JSScope *scope, uint32 slot, char *buf, size_t bufsize)
{
    jsval nval;
    JSScopeProperty *sprop;

    if (!scope) {
        JS_snprintf(buf, bufsize, "**UNKNOWN OBJECT MAP ENTRY**");
        return;
    }

    sprop = SCOPE_LAST_PROP(scope);
    while (sprop && sprop->slot != slot)
        sprop = sprop->parent;

    if (!sprop) {
        switch (slot) {
          case JSSLOT_PROTO:
            JS_snprintf(buf, bufsize, "__proto__");
            break;
          case JSSLOT_PARENT:
            JS_snprintf(buf, bufsize, "__parent__");
            break;
          default:
            JS_snprintf(buf, bufsize, "**UNKNOWN SLOT %ld**", (long)slot);
            break;
        }
    } else {
        nval = ID_TO_VALUE(sprop->id);
        if (JSVAL_IS_INT(nval)) {
            JS_snprintf(buf, bufsize, "%ld", (long)JSVAL_TO_INT(nval));
        } else if (JSVAL_IS_STRING(nval)) {
            JS_snprintf(buf, bufsize, "%s",
                        JS_GetStringBytes(JSVAL_TO_STRING(nval)));
        } else {
            JS_snprintf(buf, bufsize, "**FINALIZED ATOM KEY**");
        }
    }
}

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
gc_dump_thing(JSContext *cx, JSGCThing *thing, FILE *fp)
{
    GCMarkNode *prev = (GCMarkNode *)cx->gcCurrentMarkNode;
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
    switch (*js_GetGCThingFlags(thing) & GCF_TYPEMASK) {
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

void
js_MarkNamedGCThing(JSContext *cx, void *thing, const char *name)
{
    GCMarkNode markNode;

    if (!thing)
        return;

    markNode.thing = thing;
    markNode.name  = name;
    markNode.next  = NULL;
    markNode.prev  = (GCMarkNode *)cx->gcCurrentMarkNode;
    if (markNode.prev)
        markNode.prev->next = &markNode;
    cx->gcCurrentMarkNode = &markNode;

    if (thing == js_LiveThingToFind) {
        /*
         * Dump js_LiveThingToFind each time we reach it during the marking
         * phase of GC to print all live references to the thing.
         */
        gc_dump_thing(cx, thing, stderr);
    }

    js_MarkGCThing(cx, thing);

    if (markNode.prev)
        markNode.prev->next = NULL;
    cx->gcCurrentMarkNode = markNode.prev;
}

#endif /* !GC_MARK_DEBUG */

static void
gc_mark_atom_key_thing(void *thing, void *arg)
{
    JSContext *cx = (JSContext *) arg;

    GC_MARK(cx, thing, "atom");
}

void
js_MarkAtom(JSContext *cx, JSAtom *atom)
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
        GC_MARK(cx, JSVAL_TO_GCTHING(key), name);
    }
    if (atom->flags & ATOM_HIDDEN)
        js_MarkAtom(cx, atom->entry.value);
}

static void
AddThingToUnscannedBag(JSRuntime *rt, void *thing, uint8 *flagp);

static void
MarkGCThingChildren(JSContext *cx, void *thing, uint8 *flagp,
                    JSBool shouldCheckRecursion)
{
    JSRuntime *rt;
    JSObject *obj;
    jsval v, *vp, *end;
    void *next_thing;
    uint8 *next_flagp;
    JSString *str;
#ifdef JS_GCMETER
    uint32 tailCallNesting;
#endif
#ifdef GC_MARK_DEBUG
    JSScope *scope;
    char name[32];
#endif

    /*
     * With JS_GC_ASSUME_LOW_C_STACK defined the mark phase of GC always
     * uses the non-recursive code that otherwise would be called only on
     * a low C stack condition.
     */
#ifdef JS_GC_ASSUME_LOW_C_STACK
# define RECURSION_TOO_DEEP() shouldCheckRecursion
#else
    int stackDummy;
# define RECURSION_TOO_DEEP() (shouldCheckRecursion &&                        \
                               !JS_CHECK_STACK_SIZE(cx, stackDummy))
#endif

    rt = cx->runtime;
    METER(tailCallNesting = 0);
    METER(if (++rt->gcStats.cdepth > rt->gcStats.maxcdepth)
              rt->gcStats.maxcdepth = rt->gcStats.cdepth);

#ifndef GC_MARK_DEBUG
  start:
#endif
    JS_ASSERT(flagp);
    JS_ASSERT(*flagp & GCF_MARK); /* the caller must already mark the thing */
    METER(if (++rt->gcStats.depth > rt->gcStats.maxdepth)
              rt->gcStats.maxdepth = rt->gcStats.depth);
#ifdef GC_MARK_DEBUG
    if (js_DumpGCHeap)
        gc_dump_thing(cx, thing, js_DumpGCHeap);
#endif

    switch (*flagp & GCF_TYPEMASK) {
      case GCX_OBJECT:
        if (RECURSION_TOO_DEEP())
            goto add_to_unscanned_bag;
        /* If obj->slots is null, obj must be a newborn. */
        obj = (JSObject *) thing;
        vp = obj->slots;
        if (!vp)
            break;

        /* Mark slots if they are small enough to be GC-allocated. */
        if ((vp[-1] + 1) * sizeof(jsval) <= GC_NBYTES_MAX)
            GC_MARK(cx, vp - 1, "slots");

        /* Set up local variables to loop over unmarked things. */
        end = vp + ((obj->map->ops->mark)
                    ? obj->map->ops->mark(cx, obj, NULL)
                    : JS_MIN(obj->map->freeslot, obj->map->nslots));
        thing = NULL;
        flagp = NULL;
#ifdef GC_MARK_DEBUG
        scope = OBJ_IS_NATIVE(obj) ? OBJ_SCOPE(obj) : NULL;
#endif
        for (; vp != end; ++vp) {
            v = *vp;
            if (!JSVAL_IS_GCTHING(v) || v == JSVAL_NULL)
                continue;
            next_thing = JSVAL_TO_GCTHING(v);
            if (next_thing == thing)
                continue;
            next_flagp = js_GetGCThingFlags(next_thing);
            if (*next_flagp & GCF_MARK)
                continue;
            JS_ASSERT(*next_flagp != GCF_FINAL);
            if (thing) {
#ifdef GC_MARK_DEBUG
                GC_MARK(cx, thing, name);
#else
                *flagp |= GCF_MARK;
                MarkGCThingChildren(cx, thing, flagp, JS_TRUE);
#endif
                if (*next_flagp & GCF_MARK) {
                    /*
                     * This happens when recursive MarkGCThingChildren marks
                     * the thing with flags referred by *next_flagp.
                     */
                    thing = NULL;
                    continue;
                }
            }
#ifdef GC_MARK_DEBUG
            GetObjSlotName(scope, vp - obj->slots, name, sizeof name);
#endif
            thing = next_thing;
            flagp = next_flagp;
        }
        if (thing) {
            /*
             * thing came from the last unmarked GC-thing slot and we
             * can optimize tail recursion.
             *
             * Since we already know that there is enough C stack space,
             * we clear shouldCheckRecursion to avoid extra checking in
             * RECURSION_TOO_DEEP.
             */
            shouldCheckRecursion = JS_FALSE;
            goto on_tail_recursion;
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
        if (!JSSTRING_IS_DEPENDENT(str))
            break;
        thing = JSSTRDEP_BASE(str);
        flagp = js_GetGCThingFlags(thing);
        if (*flagp & GCF_MARK)
            break;
#ifdef GC_MARK_DEBUG
        strcpy(name, "base");
#endif
        /* Fallthrough to code to deal with the tail recursion. */

      on_tail_recursion:
#ifdef GC_MARK_DEBUG
        /*
         * Do not eliminate C recursion when debugging to allow
         * js_MarkNamedGCThing to build a full dump of live GC
         * things.
         */
        GC_MARK(cx, thing, name);
        break;
#else
        /* Eliminate tail recursion for the last unmarked child. */
        JS_ASSERT(*flagp != GCF_FINAL);
        METER(++tailCallNesting);
        *flagp |= GCF_MARK;
        goto start;
#endif

#if JS_HAS_XML_SUPPORT
      case GCX_NAMESPACE:
        if (RECURSION_TOO_DEEP())
            goto add_to_unscanned_bag;
        js_MarkXMLNamespace(cx, (JSXMLNamespace *)thing);
        break;

      case GCX_QNAME:
        if (RECURSION_TOO_DEEP())
            goto add_to_unscanned_bag;
        js_MarkXMLQName(cx, (JSXMLQName *)thing);
        break;

      case GCX_XML:
        if (RECURSION_TOO_DEEP())
            goto add_to_unscanned_bag;
        js_MarkXML(cx, (JSXML *)thing);
        break;
#endif
      add_to_unscanned_bag:
        AddThingToUnscannedBag(cx->runtime, thing, flagp);
        break;
    }

#undef RECURSION_TOO_DEEP

    METER(rt->gcStats.depth -= 1 + tailCallNesting);
    METER(rt->gcStats.cdepth--);
}

/*
 * Avoid using PAGE_THING_GAP inside this macro to optimize the
 * thingsPerUnscannedChunk calculation when thingSize is a power of two.
 */
#define GET_GAP_AND_CHUNK_SPAN(thingSize, thingsPerUnscannedChunk, pageGap)   \
    JS_BEGIN_MACRO                                                            \
        if (0 == ((thingSize) & ((thingSize) - 1))) {                         \
            pageGap = (thingSize);                                            \
            thingsPerUnscannedChunk = ((GC_PAGE_SIZE / (thingSize))           \
                                       + JS_BITS_PER_WORD - 1)                \
                                      >> JS_BITS_PER_WORD_LOG2;               \
        } else {                                                              \
            pageGap = GC_PAGE_SIZE % (thingSize);                             \
            thingsPerUnscannedChunk = JS_HOWMANY(GC_PAGE_SIZE / (thingSize),  \
                                                 JS_BITS_PER_WORD);           \
        }                                                                     \
    JS_END_MACRO

static void
AddThingToUnscannedBag(JSRuntime *rt, void *thing, uint8 *flagp)
{
    JSGCPageInfo *pi;
    JSGCArena *arena;
    size_t thingSize;
    size_t thingsPerUnscannedChunk;
    size_t pageGap;
    size_t chunkIndex;
    jsuword bit;

    /* Things from delayed scanning bag are marked as GCF_MARK | GCF_FINAL. */
    JS_ASSERT((*flagp & (GCF_MARK | GCF_FINAL)) == GCF_MARK);
    *flagp |= GCF_FINAL;

    METER(rt->gcStats.unscanned++);
#ifdef DEBUG
    ++rt->gcUnscannedBagSize;
    METER(if (rt->gcUnscannedBagSize > rt->gcStats.maxunscanned)
              rt->gcStats.maxunscanned = rt->gcUnscannedBagSize);
#endif

    pi = THING_TO_PAGE(thing);
    arena = PAGE_TO_ARENA(pi);
    thingSize = arena->list->thingSize;
    GET_GAP_AND_CHUNK_SPAN(thingSize, thingsPerUnscannedChunk, pageGap);
    chunkIndex = (((jsuword)thing & GC_PAGE_MASK) - pageGap) /
                 (thingSize * thingsPerUnscannedChunk);
    JS_ASSERT(chunkIndex < JS_BITS_PER_WORD);
    bit = (jsuword)1 << chunkIndex;
    if (pi->unscannedBitmap != 0) {
        JS_ASSERT(rt->gcUnscannedArenaStackTop);
        if (thingsPerUnscannedChunk != 1) {
            if (pi->unscannedBitmap & bit) {
                /* Chunk already contains things to scan later. */
                return;
            }
        } else {
            /*
             * The chunk must not contain things to scan later if there is
             * only one thing per chunk.
             */
            JS_ASSERT(!(pi->unscannedBitmap & bit));
        }
        pi->unscannedBitmap |= bit;
        JS_ASSERT(arena->unscannedPages & ((size_t)1 << PAGE_INDEX(pi)));
    } else {
        /*
         * The thing is the first unscanned thing in the page, set the bit
         * corresponding to this page arena->unscannedPages.
         */
        pi->unscannedBitmap = bit;
        JS_ASSERT(PAGE_INDEX(pi) < JS_BITS_PER_WORD);
        bit = (jsuword)1 << PAGE_INDEX(pi);
        JS_ASSERT(!(arena->unscannedPages & bit));
        if (arena->unscannedPages != 0) {
            arena->unscannedPages |= bit;
            JS_ASSERT(arena->prevUnscanned);
            JS_ASSERT(rt->gcUnscannedArenaStackTop);
        } else {
            /*
             * The thing is the first unscanned thing in the whole arena, push
             * the arena on the stack of unscanned arenas unless the arena
             * has already been pushed. We detect that through prevUnscanned
             * field which is NULL only for not yet pushed arenas. To ensure
             * that prevUnscanned != NULL even when the stack contains one
             * element, we make prevUnscanned for the arena at the bottom
             * to point to itself.
             *
             * See comments in ScanDelayedChildren.
             */
            arena->unscannedPages = bit;
            if (!arena->prevUnscanned) {
                if (!rt->gcUnscannedArenaStackTop) {
                    /* Stack was empty, mark the arena as bottom element. */
                    arena->prevUnscanned = arena;
                } else {
                    JS_ASSERT(rt->gcUnscannedArenaStackTop->prevUnscanned);
                    arena->prevUnscanned = rt->gcUnscannedArenaStackTop;
                }
                rt->gcUnscannedArenaStackTop = arena;
            }
         }
     }
    JS_ASSERT(rt->gcUnscannedArenaStackTop);
}

static void
ScanDelayedChildren(JSContext *cx)
{
    JSRuntime *rt;
    JSGCArena *arena;
    size_t thingSize;
    size_t thingsPerUnscannedChunk;
    size_t pageGap;
    size_t pageIndex;
    JSGCPageInfo *pi;
    size_t chunkIndex;
    size_t thingOffset, thingLimit;
    JSGCThing *thing;
    uint8 *flagp;
    JSGCArena *prevArena;

    rt = cx->runtime;
    arena = rt->gcUnscannedArenaStackTop;
    if (!arena) {
        JS_ASSERT(rt->gcUnscannedBagSize == 0);
        return;
    }

  init_size:
    thingSize = arena->list->thingSize;
    GET_GAP_AND_CHUNK_SPAN(thingSize, thingsPerUnscannedChunk, pageGap);
    for (;;) {
        /*
         * The following assert verifies that the current arena belongs to
         * the unscan stack since AddThingToUnscannedBag ensures that even
         * for stack's bottom prevUnscanned != NULL but rather points to self.
         */
        JS_ASSERT(arena->prevUnscanned);
        JS_ASSERT(rt->gcUnscannedArenaStackTop->prevUnscanned);
        while (arena->unscannedPages != 0) {
            pageIndex = JS_FLOOR_LOG2W(arena->unscannedPages);
            JS_ASSERT(pageIndex < GC_PAGE_COUNT);
            pi = (JSGCPageInfo *)(FIRST_THING_PAGE(arena) +
                                  pageIndex * GC_PAGE_SIZE);
            JS_ASSERT(pi->unscannedBitmap);
            chunkIndex = JS_FLOOR_LOG2W(pi->unscannedBitmap);
            pi->unscannedBitmap &= ~((jsuword)1 << chunkIndex);
            if (pi->unscannedBitmap == 0)
                arena->unscannedPages &= ~((jsuword)1 << pageIndex);
            thingOffset = (pageGap
                           + chunkIndex * thingsPerUnscannedChunk * thingSize);
            JS_ASSERT(thingOffset >= sizeof(JSGCPageInfo));
            thingLimit = thingOffset + thingsPerUnscannedChunk * thingSize;
            if (thingsPerUnscannedChunk != 1) {
                /*
                 * thingLimit can go beyond the last allocated thing for the
                 * last chunk as the real limit can be inside the chunk.
                 */
                if (arena->list->last == arena &&
                    arena->list->lastLimit < (pageIndex * GC_PAGE_SIZE +
                                              thingLimit)) {
                    thingLimit = (arena->list->lastLimit -
                                  pageIndex * GC_PAGE_SIZE);
                } else if (thingLimit > GC_PAGE_SIZE) {
                    thingLimit = GC_PAGE_SIZE;
                }
                JS_ASSERT(thingLimit > thingOffset);
            }
            JS_ASSERT(arena->list->last != arena ||
                      arena->list->lastLimit >= (pageIndex * GC_PAGE_SIZE +
                                                 thingLimit));
            JS_ASSERT(thingLimit <= GC_PAGE_SIZE);

            for (; thingOffset != thingLimit; thingOffset += thingSize) {
                /*
                 * XXX: inline js_GetGCThingFlags() to use already available
                 * pi.
                 */
                thing = (void *)((jsuword)pi + thingOffset);
                flagp = js_GetGCThingFlags(thing);
                if (thingsPerUnscannedChunk != 1) {
                    /*
                     * Skip free or already scanned things that share the chunk
                     * with unscanned ones.
                     */
                    if ((*flagp & (GCF_MARK|GCF_FINAL)) != (GCF_MARK|GCF_FINAL))
                        continue;
                }
                JS_ASSERT((*flagp & (GCF_MARK|GCF_FINAL))
                              == (GCF_MARK|GCF_FINAL));
                *flagp &= ~GCF_FINAL;
#ifdef DEBUG
                JS_ASSERT(rt->gcUnscannedBagSize != 0);
                --rt->gcUnscannedBagSize;

                /*
                 * Check that GC thing type is consistent with the type of
                 * things that can be put to the unscanned bag.
                 */
                switch (*flagp & GCF_TYPEMASK) {
                  case GCX_OBJECT:
# if JS_HAS_XML_SUPPORT
                  case GCX_NAMESPACE:
                  case GCX_QNAME:
                  case GCX_XML:
# endif
                    break;
                  default:
                    JS_ASSERT(0);
                }
#endif
                MarkGCThingChildren(cx, thing, flagp, JS_FALSE);
            }
        }
        /*
         * We finished scanning of the arena but we can only pop it from
         * the stack if the arena is the stack's top.
         *
         * When MarkGCThingChildren from the above calls
         * AddThingToUnscannedBag and the latter pushes new arenas to the
         * stack, we have to skip popping of this arena until it becomes
         * the top of the stack again.
         */
        if (arena == rt->gcUnscannedArenaStackTop) {
            prevArena = arena->prevUnscanned;
            arena->prevUnscanned = NULL;
            if (arena == prevArena) {
                /*
                 * prevUnscanned points to itself and we reached the bottom
                 * of the stack.
                 */
                break;
            }
            rt->gcUnscannedArenaStackTop = arena = prevArena;
        } else {
            arena = rt->gcUnscannedArenaStackTop;
        }
        if (arena->list->thingSize != thingSize)
            goto init_size;
    }
    JS_ASSERT(rt->gcUnscannedArenaStackTop);
    JS_ASSERT(!rt->gcUnscannedArenaStackTop->prevUnscanned);
    rt->gcUnscannedArenaStackTop = NULL;
    JS_ASSERT(rt->gcUnscannedBagSize == 0);
}

void
js_MarkGCThing(JSContext *cx, void *thing)
{
    uint8 *flagp;

    if (!thing)
        return;

    flagp = js_GetGCThingFlags(thing);
    JS_ASSERT(*flagp != GCF_FINAL);
    if (*flagp & GCF_MARK)
        return;
    *flagp |= GCF_MARK;

    if (!cx->insideGCMarkCallback) {
        MarkGCThingChildren(cx, thing, flagp, JS_TRUE);
    } else {
        /*
         * For API compatibility we allow for the callback to assume that
         * after it calls js_MarkGCThing for the last time, the callback
         * can start to finalize its own objects that are only referenced
         * by unmarked GC things.
         *
         * Since we do not know which call from inside the callback is the
         * last, we ensure that the unscanned bag is always empty when we
         * return to the callback and all marked things are scanned.
         *
         * As an optimization we do not check for the stack size here and
         * pass JS_FALSE as the last argument to MarkGCThingChildren.
         * Otherwise with low C stack the thing would be pushed to the bag
         * just to be feed to MarkGCThingChildren from inside
         * ScanDelayedChildren.
         */
        cx->insideGCMarkCallback = JS_FALSE;
        MarkGCThingChildren(cx, thing, flagp, JS_FALSE);
        ScanDelayedChildren(cx);
        cx->insideGCMarkCallback = JS_TRUE;
    }
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
        JSBool root_points_to_gcArenaList = JS_FALSE;
        jsuword thing = (jsuword) JSVAL_TO_GCTHING(v);
        uintN i;
        JSGCArenaList *arenaList;
        JSGCArena *a;
        size_t limit;

        for (i = 0; i < GC_NUM_FREELISTS; i++) {
            arenaList = &cx->runtime->gcArenaList[i];
            limit = arenaList->lastLimit;
            for (a = arenaList->last; a; a = a->prev) {
                if (thing - FIRST_THING_PAGE(a) < limit) {
                    root_points_to_gcArenaList = JS_TRUE;
                    break;
                }
                limit = GC_THINGS_SIZE;
            }
        }
        if (!root_points_to_gcArenaList && rhe->name) {
            fprintf(stderr,
"JS API usage error: the address passed to JS_AddNamedRoot currently holds an\n"
"invalid jsval.  This is usually caused by a missing call to JS_RemoveRoot.\n"
"The root's name is \"%s\".\n",
                    rhe->name);
        }
        JS_ASSERT(root_points_to_gcArenaList);
#endif

        GC_MARK(cx, JSVAL_TO_GCTHING(v), rhe->name ? rhe->name : "root");
    }
    return JS_DHASH_NEXT;
}

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
gc_lock_marker(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 num, void *arg)
{
    JSGCLockHashEntry *lhe = (JSGCLockHashEntry *)hdr;
    void *thing = (void *)lhe->thing;
    JSContext *cx = (JSContext *)arg;

    GC_MARK(cx, thing, "locked object");
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
                GC_MARK(cx, JSVAL_TO_GCTHING(_v), name);                      \
        }                                                                     \
    JS_END_MACRO

void
js_MarkStackFrame(JSContext *cx, JSStackFrame *fp)
{
    uintN depth, nslots;

    if (fp->callobj)
        GC_MARK(cx, fp->callobj, "call object");
    if (fp->argsobj)
        GC_MARK(cx, fp->argsobj, "arguments object");
    if (fp->varobj)
        GC_MARK(cx, fp->varobj, "variables object");
    if (fp->script) {
        js_MarkScript(cx, fp->script);
        if (fp->spbase) {
            /*
             * Don't mark what has not been pushed yet, or what has been
             * popped already.
             */
            depth = fp->script->depth;
            nslots = (JS_UPTRDIFF(fp->sp, fp->spbase)
                      < depth * sizeof(jsval))
                     ? (uintN)(fp->sp - fp->spbase)
                     : depth;
            GC_MARK_JSVALS(cx, nslots, fp->spbase, "operand");
        }
    }
    JS_ASSERT(JSVAL_IS_OBJECT((jsval)fp->thisp) ||
              (fp->fun && (fp->fun->flags & JSFUN_THISP_PRIMITIVE)));
    if (JSVAL_IS_GCTHING((jsval)fp->thisp))
        GC_MARK(cx, fp->thisp, "this");
    if (fp->argv) {
        nslots = fp->argc;
        if (fp->fun) {
            if (fp->fun->nargs > nslots)
                nslots = fp->fun->nargs;
            if (!FUN_INTERPRETED(fp->fun))
                nslots += fp->fun->u.n.extra;
        }
        GC_MARK_JSVALS(cx, nslots, fp->argv, "arg");
    }
    if (JSVAL_IS_GCTHING(fp->rval))
        GC_MARK(cx, JSVAL_TO_GCTHING(fp->rval), "rval");
    if (fp->vars)
        GC_MARK_JSVALS(cx, fp->nvars, fp->vars, "var");
    GC_MARK(cx, fp->scopeChain, "scope chain");
    if (fp->sharpArray)
        GC_MARK(cx, fp->sharpArray, "sharp array");

    if (fp->xmlNamespace)
        GC_MARK(cx, fp->xmlNamespace, "xmlNamespace");
}

void
js_GC(JSContext *cx, uintN gcflags)
{
    JSRuntime *rt;
    JSContext *iter, *acx;
    JSStackFrame *fp, *chain;
    uintN i, type;
    JSStackHeader *sh;
    JSTempValueRooter *tvr;
    size_t nbytes, limit, offset;
    JSGCArena *a, **ap;
    uint8 flags, *flagp, *firstPage;
    JSGCThing *thing, *freeList;
    JSGCArenaList *arenaList;
    GCFinalizeOp finalizer;
    JSBool allClear;
#ifdef JS_THREADSAFE
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
    if (rt->gcThread && rt->gcThread->id == js_CurrentThreadId()) {
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
     * requests are inactive.  If cx->thread is NULL, then cx is not using
     * the request model, and does not contribute to rt->requestCount.
     */
    requestDebit = 0;
    if (cx->thread) {
        JSCList *head, *link;

        /*
         * Check all contexts on cx->thread->contextList for active requests,
         * counting each such context against requestDebit.
         */
        head = &cx->thread->contextList;
        for (link = head->next; link != head; link = link->next) {
            acx = CX_FROM_THREAD_LINKS(link);
            JS_ASSERT(acx->thread == cx->thread);
            if (acx->requestDepth)
                requestDebit++;
        }
    } else {
        /*
         * We assert, but check anyway, in case someone is misusing the API.
         * Avoiding the loop over all of rt's contexts is a win in the event
         * that the GC runs only on request-less contexts with null threads,
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
    rt->gcThread = js_GetCurrentThread(rt);
    JS_ASSERT(rt->gcThread);

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
#ifdef DEBUG_scopemeters
  { extern void js_DumpScopeMeters(JSRuntime *rt);
    js_DumpScopeMeters(rt);
  }
#endif

#ifdef JS_THREADSAFE
    /*
     * Set all thread local freelists to NULL. We may visit a thread's
     * freelist more than once. To avoid redundant clearing we unroll the
     * current thread's step.
     */
    memset(cx->thread->gcFreeLists, 0, sizeof cx->thread->gcFreeLists);
    iter = NULL;
    while ((acx = js_ContextIterator(rt, JS_FALSE, &iter)) != NULL) {
        if (acx->thread == cx->thread)
            continue;
        memset(acx->thread->gcFreeLists, 0, sizeof acx->thread->gcFreeLists);
    }
#endif

restart:
    rt->gcNumber++;
    JS_ASSERT(!rt->gcUnscannedArenaStackTop);
    JS_ASSERT(rt->gcUnscannedBagSize == 0);

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
                js_MarkStackFrame(cx, fp);
            } while ((fp = fp->down) != NULL);
        }

        /* Cleanup temporary "dormant" linkage. */
        if (acx->fp)
            acx->fp->dormantNext = NULL;

        /* Mark other roots-by-definition in acx. */
        GC_MARK(cx, acx->globalObject, "global object");
        for (i = 0; i < GCX_NTYPES; i++)
            GC_MARK(cx, acx->newborn[i], gc_typenames[i]);
        if (acx->lastAtom)
            GC_MARK_ATOM(cx, acx->lastAtom);
        if (JSVAL_IS_GCTHING(acx->lastInternalResult)) {
            thing = JSVAL_TO_GCTHING(acx->lastInternalResult);
            if (thing)
                GC_MARK(cx, thing, "lastInternalResult");
        }
        if (acx->throwing && JSVAL_IS_GCTHING(acx->exception))
            GC_MARK(cx, JSVAL_TO_GCTHING(acx->exception), "exception");
#if JS_HAS_LVALUE_RETURN
        if (acx->rval2set == JS_RVAL2_VALUE && JSVAL_IS_GCTHING(acx->rval2))
            GC_MARK(cx, JSVAL_TO_GCTHING(acx->rval2), "rval2");
#endif

        for (sh = acx->stackHeaders; sh; sh = sh->down) {
            METER(rt->gcStats.stackseg++);
            METER(rt->gcStats.segslots += sh->nslots);
            GC_MARK_JSVALS(cx, sh->nslots, JS_STACK_SEGMENT(sh), "stack");
        }

        if (acx->localRootStack)
            js_MarkLocalRoots(cx, acx->localRootStack);

        for (tvr = acx->tempValueRooters; tvr; tvr = tvr->down) {
            if (tvr->count < 0) {
                if (JSVAL_IS_GCTHING(tvr->u.value)) {
                    GC_MARK(cx, JSVAL_TO_GCTHING(tvr->u.value),
                            "tvr->u.value");
                }
            } else {
                GC_MARK_JSVALS(cx, tvr->count, tvr->u.array, "tvr->u.array");
            }
        }

        acx->cachedIterObj = NULL;
    }

#ifdef DUMP_CALL_TABLE
    js_DumpCallTable(cx);
#endif

    /*
     * Mark children of things that caused too deep recursion during above
     * marking phase.
     */
    ScanDelayedChildren(cx);

    JS_ASSERT(!cx->insideGCMarkCallback);
    if (rt->gcCallback) {
        cx->insideGCMarkCallback = JS_TRUE;
        (void) rt->gcCallback(cx, JSGC_MARK_END);
        JS_ASSERT(cx->insideGCMarkCallback);
        cx->insideGCMarkCallback = JS_FALSE;
    }
    JS_ASSERT(rt->gcUnscannedBagSize == 0);

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
    for (i = 0; i < GC_NUM_FREELISTS; i++) {
        arenaList = &rt->gcArenaList[i];
        nbytes = GC_FREELIST_NBYTES(i);
        limit = arenaList->lastLimit;
        for (a = arenaList->last; a; a = a->prev) {
            JS_ASSERT(!a->prevUnscanned);
            JS_ASSERT(a->unscannedPages == 0);
            firstPage = (uint8 *) FIRST_THING_PAGE(a);
            for (offset = 0; offset != limit; offset += nbytes) {
                if ((offset & GC_PAGE_MASK) == 0) {
                    JS_ASSERT(((JSGCPageInfo *)(firstPage + offset))->
                              unscannedBitmap == 0);
                    offset += PAGE_THING_GAP(nbytes);
                }
                JS_ASSERT(offset < limit);
                flagp = a->base + offset / sizeof(JSGCThing);
                if (flagp >= firstPage)
                    flagp += GC_THINGS_SIZE;
                flags = *flagp;
                if (flags & GCF_MARK) {
                    *flagp &= ~GCF_MARK;
                } else if (!(flags & (GCF_LOCK | GCF_FINAL))) {
                    /* Call the finalizer with GCF_FINAL ORed into flags. */
                    type = flags & GCF_TYPEMASK;
                    finalizer = gc_finalizers[type];
                    if (finalizer) {
                        thing = (JSGCThing *)(firstPage + offset);
                        *flagp = (uint8)(flags | GCF_FINAL);
                        if (type >= GCX_EXTERNAL_STRING)
                            js_PurgeDeflatedStringCache(rt, (JSString *)thing);
                        finalizer(cx, thing);
                    }

                    /* Set flags to GCF_FINAL, signifying that thing is free. */
                    *flagp = GCF_FINAL;
                }
            }
            limit = GC_THINGS_SIZE;
        }
    }

    /*
     * Sweep script filenames after sweeping functions in the generic loop
     * above. In this way when scripted function's finalizer destroys script
     * triggering a call to rt->destroyScriptHook, the hook can still access
     * script's filename. See bug 323267.
     */
    js_SweepScriptFilenames(rt);

    /*
     * Free phase.
     * Free any unused arenas and rebuild the JSGCThing freelist.
     */
    for (i = 0; i < GC_NUM_FREELISTS; i++) {
        arenaList = &rt->gcArenaList[i];
        ap = &arenaList->last;
        a = *ap;
        if (!a)
            continue;

        allClear = JS_TRUE;
        arenaList->freeList = NULL;
        freeList = NULL;
        METER(arenaList->stats.nthings = 0);
        METER(arenaList->stats.freelen = 0);

        nbytes = GC_FREELIST_NBYTES(i);
        limit = arenaList->lastLimit;
        do {
            METER(size_t nfree = 0);
            firstPage = (uint8 *) FIRST_THING_PAGE(a);
            for (offset = 0; offset != limit; offset += nbytes) {
                if ((offset & GC_PAGE_MASK) == 0)
                    offset += PAGE_THING_GAP(nbytes);
                JS_ASSERT(offset < limit);
                flagp = a->base + offset / sizeof(JSGCThing);
                if (flagp >= firstPage)
                    flagp += GC_THINGS_SIZE;

                if (*flagp != GCF_FINAL) {
                    allClear = JS_FALSE;
                    METER(++arenaList->stats.nthings);
                } else {
                    thing = (JSGCThing *)(firstPage + offset);
                    thing->flagp = flagp;
                    thing->next = freeList;
                    freeList = thing;
                    METER(++nfree);
                }
            }
            if (allClear) {
                /*
                 * Forget just assembled free list head for the arena
                 * and destroy the arena itself.
                 */
                freeList = arenaList->freeList;
                DestroyGCArena(rt, arenaList, ap);
            } else {
                allClear = JS_TRUE;
                arenaList->freeList = freeList;
                ap = &a->prev;
                METER(arenaList->stats.freelen += nfree);
                METER(arenaList->stats.totalfreelen += nfree);
                METER(++arenaList->stats.totalarenas);
            }
            limit = GC_THINGS_SIZE;
        } while ((a = *ap) != NULL);
    }

    if (rt->gcCallback)
        (void) rt->gcCallback(cx, JSGC_FINALIZE_END);
#ifdef DEBUG_srcnotesize
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
    rt->gcThread = NULL;
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
