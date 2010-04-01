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
#include <stdlib.h>     /* for free */
#include <math.h>
#include <string.h>     /* for memset used when DEBUG */
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jshash.h" /* Added by JSIFY */
#include "jsbit.h"
#include "jsclist.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsdbgapi.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsparse.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstaticcheck.h"
#include "jsstr.h"
#include "jstask.h"
#include "jstracer.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#ifdef INCLUDE_MOZILLA_DTRACE
#include "jsdtracef.h"
#endif

#include "jscntxtinlines.h"
#include "jsobjinlines.h"

/*
 * Include the headers for mmap.
 */
#if defined(XP_WIN)
# include <windows.h>
#endif
#if defined(XP_UNIX) || defined(XP_BEOS)
# include <unistd.h>
# include <sys/mman.h>
#endif
/* On Mac OS X MAP_ANONYMOUS is not defined. */
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
# define MAP_ANONYMOUS MAP_ANON
#endif
#if !defined(MAP_ANONYMOUS)
# define MAP_ANONYMOUS 0
#endif

using namespace js;

/*
 * Check that JSTRACE_XML follows JSTRACE_OBJECT, JSTRACE_DOUBLE and
 * JSTRACE_STRING.
 */
JS_STATIC_ASSERT(JSTRACE_OBJECT == 0);
JS_STATIC_ASSERT(JSTRACE_DOUBLE == 1);
JS_STATIC_ASSERT(JSTRACE_STRING == 2);
JS_STATIC_ASSERT(JSTRACE_XML    == 3);

/*
 * JS_IS_VALID_TRACE_KIND assumes that JSTRACE_STRING is the last non-xml
 * trace kind when JS_HAS_XML_SUPPORT is false.
 */
JS_STATIC_ASSERT(JSTRACE_STRING + 1 == JSTRACE_XML);

/*
 * Check that we can use memset(p, 0, ...) to implement JS_CLEAR_WEAK_ROOTS.
 */
JS_STATIC_ASSERT(JSVAL_NULL == 0);

/*
 * Check consistency of external string constants from JSFinalizeGCThingKind.
 */
JS_STATIC_ASSERT(FINALIZE_EXTERNAL_STRING_LAST - FINALIZE_EXTERNAL_STRING0 ==
                 JS_EXTERNAL_STRING_LIMIT - 1);

JS_STATIC_ASSERT(sizeof(JSStackHeader) >= 2 * sizeof(jsval));

/*
 * A GC arena contains GC_ARENA_SIZE bytes aligned on GC_ARENA_SIZE boundary.
 * The arena holds thing of the same size, a JSGCArenaInfo descriptor and a
 * mark bitmap.
 *
 * The size of each thing must be divisible by GC_CELL_SIZE, the minimal
 * allocation unit, and the size of the mark bitmap is fixed and is
 * independent of the thing's size with one bit per each GC_CELL_SIZE bytes.
 * For thing sizes that exceed GC_CELL_SIZE this implies that we waste space
 * in the mark bitmap. The advantage is that we can find the mark bit for the
 * thing using just integer shifts avoiding an expensive integer division. We
 * trade some space for speed here.
 *
 * Another advantage of the fixed size of the bitmap is that it allows us to
 * put it at the end of the arena where it ends on a CPU cache line boundary.
 * This minimizes the number of cache lines that are necessary to access
 * during the marking phase of the GC.
 *
 * The following picture demonstrates arena's layout:
 *
 *  +------------------------------+---------------+-------------+
 *  | allocation area for GC thing | JSGCArenaInfo | mark bitmap |
 *  +------------------------------+---------------+-------------+
 *
 * The allocation area contains GC_CELLS_PER_ARENA. We find that number as the
 * following. Let n be the number of cells in the arena. Together with the
 * word-aligned mark bitmap and JSGCArenaInfo they should fit the arena. Hence
 * GC_CELLS_PER_ARENA or n_max is the maximum value of n for which the
 * following holds:
  *
 *   n*s + ceil(n/B) <= M                                               (1)
 *
 * where "/" denotes normal real division,
 *       ceil(r) gives the least integer not smaller than the number r,
 *       s is the number of words in the GC cell,
 *       B is number of bits per word or B == JS_BITS_PER_WORD
 *       M is the number of words in the arena without JSGCArenaInfo or
 *       M == (GC_ARENA_SIZE - sizeof(JSGCArenaInfo)) / sizeof(jsuword).
 *
 * We rewrite the inequality as
 *
 *   n*B*s/B + ceil(n/B) <= M,
 *   ceil(n*B*s/B + n/B) <= M,
 *   ceil(n*(B*s + 1)/B) <= M                                           (2)
 *
 * We define a helper function e(n, s, B),
 *
 *   e(n, s, B) := ceil(n*(B*s + 1)/B) - n*(B*s + 1)/B, 0 <= e(n, s, B) < 1.
 *
 * It gives:
 *
 *   n*(B*s + 1)/B + e(n, s, B) <= M,
 *   n + e*B/(B*s + 1) <= M*B/(B*s + 1)
 *
 * We apply the floor function to both sides of the last equation, where
 * floor(r) gives the biggest integer not greater than r. As a consequence we
 * have:
 *
 *   floor(n + e*B/(B*s + 1)) <= floor(M*B/(B*s + 1)),
 *   n + floor(e*B/(B*s + 1)) <= floor(M*B/(B*s + 1)),
 *   n <= floor(M*B/(B*s + 1)),                                         (3)
 *
 * where floor(e*B/(B*s + 1)) is zero as e*B/(B*s + 1) < B/(B*s + 1) < 1.
 * Thus any n that satisfies the original constraint (1) or its equivalent (2),
 * must also satisfy (3). That is, we got an upper estimate for the maximum
 * value of n. Lets show that this upper estimate,
 *
 *   floor(M*B/(B*s + 1)),                                              (4)
 *
 * also satisfies (1) and, as such, gives the required maximum value.
 * Substituting it into (2) gives:
 *
 *   ceil(floor(M*B/(B*s + 1))*(B*s + 1)/B) == ceil(floor(M/X)*X)
 *
 * where X == (B*s + 1)/B > 1. But then floor(M/X)*X <= M/X*X == M and
 *
 *   ceil(floor(M/X)*X) <= ceil(M) == M.
 *
 * Thus the value of (4) gives the maximum n satisfying (1).
 *
 * For the final result we observe that in (4)
 *
 *    M*B == ARENA_INFO_OFFSET / sizeof(jsuword) * JS_BITS_PER_WORD
 *        == ARENA_INFO_OFFSET * JS_BITS_PER_BYTE
 *
 *  and
 *
 *    B*s == JS_BITS_PER_WORD * GC_CELL_SIZE / sizeof(jsuword)
 *        == BITS_PER_GC_CELL.
 */

static const jsuword GC_ARENAS_PER_CHUNK = 16;
static const jsuword GC_ARENA_SHIFT = 12;
static const jsuword GC_ARENA_MASK = JS_BITMASK(GC_ARENA_SHIFT);
static const jsuword GC_ARENA_SIZE = JS_BIT(GC_ARENA_SHIFT);
static const jsuword GC_CHUNK_SIZE = GC_ARENAS_PER_CHUNK << GC_ARENA_SHIFT;

const size_t GC_CELL_SHIFT = 3;
const size_t GC_CELL_SIZE = size_t(1) << GC_CELL_SHIFT;
const size_t GC_CELL_MASK = GC_CELL_SIZE - 1;

const size_t BITS_PER_GC_CELL = GC_CELL_SIZE * JS_BITS_PER_BYTE;

struct JSGCArenaInfo {
    /*
     * Allocation list for the arena or NULL if the arena holds double values.
     */
    JSGCArenaList   *list;

    /*
     * Pointer to the previous arena in a linked list. The arena can either
     * belong to one of JSContext.gcArenaList lists or, when it does not have
     * any allocated GC things, to the list of free arenas in the chunk with
     * head stored in JSGCChunkInfo.lastFreeArena.
     */
    JSGCArena       *prev;

    /*
     * A link field for the list of arenas with marked things that haven't yet
     * been scanned for live children. The field is encoded as arena's page to
     * to hold only the high-order arena-counting bits to share the space with
     * firstArena and arenaIndex fields. For details see comments before
     * DelayMarkingChildren.
     */
    jsuword         prevUnmarkedPage :  JS_BITS_PER_WORD - GC_ARENA_SHIFT;

    /*
     * When firstArena is false, the index of arena in the chunk. When
     * firstArena is true, the index of a free arena holding JSGCChunkInfo or
     * NO_FREE_ARENAS if there are no free arenas in the chunk.
     *
     * GetArenaIndex() and GetChunkInfoIndex() below are convenience functions
     * to access either of indexes.
     */
    jsuword         arenaIndex :        GC_ARENA_SHIFT - 1;

    /* Flag indicating if the arena is the first in the chunk. */
    jsuword         firstArena :        1;

    JSGCThing       *freeList;

    union {
        /* See comments before DelayMarkingChildren. */
        jsuword     unmarkedChildren;

        /* The arena has marked doubles. */
        bool        hasMarkedDoubles;
    };
};

const size_t GC_CELLS_PER_ARENA = (GC_ARENA_SIZE - sizeof(JSGCArenaInfo)) *
                                   JS_BITS_PER_BYTE / (BITS_PER_GC_CELL + 1);

const size_t GC_ARENA_MARK_BITMAP_WORDS =
    JS_HOWMANY(GC_CELLS_PER_ARENA, JS_BITS_PER_WORD);

/* Check that GC_CELLS_PER_ARENA indeed maximises (1). */
JS_STATIC_ASSERT(GC_CELLS_PER_ARENA * GC_CELL_SIZE +
                 GC_ARENA_MARK_BITMAP_WORDS * sizeof(jsuword) <=
                 GC_ARENA_SIZE - sizeof(JSGCArenaInfo));

JS_STATIC_ASSERT((GC_CELLS_PER_ARENA + 1) * GC_CELL_SIZE +
                 sizeof(jsuword) *
                 JS_HOWMANY((GC_CELLS_PER_ARENA + 1), JS_BITS_PER_WORD) >
                 GC_ARENA_SIZE - sizeof(JSGCArenaInfo));

const size_t GC_ARENA_MARK_BITMAP_SIZE = GC_ARENA_MARK_BITMAP_WORDS *
                                         sizeof(jsuword);

const size_t GC_ARENA_CELLS_SIZE = GC_CELLS_PER_ARENA * GC_CELL_SIZE;

JS_STATIC_ASSERT(sizeof(jsbitmap) == sizeof(jsuword));

struct JSGCArena {
    /*
     * The size of the data may exceed GC_ARENA_CELLS_SIZE as, whenever the
     * size of the system word is less than GC_CELL_SIZE, the sum
     *
     *   GC_ARENA_CELLS_SIZE + sizeof(JSGCArenaInfo) + GC_ARENA_MARK_BITMAP_SIZE
     *
     * could be less then the arena size. We add the extra space to data.
     */
    uint8           data[GC_ARENA_SIZE - sizeof(JSGCArenaInfo) -
                         GC_ARENA_MARK_BITMAP_SIZE];
    JSGCArenaInfo   info;
    jsbitmap        markBitmap[GC_ARENA_MARK_BITMAP_WORDS];

    void checkAddress() const {
        JS_ASSERT(!(reinterpret_cast<jsuword>(this) & GC_ARENA_MASK));
    }

    jsuword toPageStart() const {
        checkAddress();
        return reinterpret_cast<jsuword>(this);
    }

    static JSGCArena *fromPageStart(jsuword pageStart) {
        JS_ASSERT(!(pageStart & GC_ARENA_MASK));
        return reinterpret_cast<JSGCArena *>(pageStart);
    }

    bool hasPrevUnmarked() const { return !!info.prevUnmarkedPage; }

    JSGCArena *getPrevUnmarked() const {
        JS_ASSERT(hasPrevUnmarked());
        return fromPageStart(info.prevUnmarkedPage << GC_ARENA_SHIFT);
    }

    void clearPrevUnmarked() { info.prevUnmarkedPage = 0; }

    void setPrevUnmarked(JSGCArena *a) {
        JS_ASSERT(a);
        info.prevUnmarkedPage = a->toPageStart() >> GC_ARENA_SHIFT;
    }

    static JSGCArena *fromGCThing(void *thing) {
        JS_ASSERT(!JSString::isStatic(thing));
        return fromPageStart(reinterpret_cast<jsuword>(thing) & ~GC_ARENA_MASK);
    }

    void clearMarkBitmap() {
        PodArrayZero(markBitmap);
    }

    jsbitmap *getMarkBitmapEnd() {
        return markBitmap + GC_ARENA_MARK_BITMAP_WORDS;
    }
};

JS_STATIC_ASSERT(sizeof(JSGCArena) == GC_ARENA_SIZE);
JS_STATIC_ASSERT(GC_ARENA_SIZE - GC_ARENA_CELLS_SIZE - sizeof(JSGCArenaInfo) -
                 GC_ARENA_MARK_BITMAP_SIZE < GC_CELL_SIZE);
JS_STATIC_ASSERT((GC_ARENA_SIZE - GC_ARENA_CELLS_SIZE - sizeof(JSGCArenaInfo) -
                  GC_ARENA_MARK_BITMAP_SIZE) % sizeof(jsuword) == 0);

JS_STATIC_ASSERT(sizeof(JSString) % GC_CELL_SIZE == 0);
JS_STATIC_ASSERT(sizeof(JSObject) % GC_CELL_SIZE == 0);
JS_STATIC_ASSERT(sizeof(JSFunction) % GC_CELL_SIZE == 0);
#ifdef JSXML
JS_STATIC_ASSERT(sizeof(JSXML) % GC_CELL_SIZE == 0);
#endif

JS_STATIC_ASSERT(GC_CELL_SIZE == sizeof(jsdouble));
const size_t DOUBLES_PER_ARENA = GC_CELLS_PER_ARENA;

/*
 * The private JSGCThing struct, which describes a JSRuntime.gcFreeList element.
 */
struct JSGCThing {
    JSGCThing   *link;
};

/*
 * Definitions for allocating arenas in chunks.
 *
 * All chunks that have at least one free arena are put on the doubly-linked
 * list with the head stored in JSRuntime.gcChunkList. JSGCChunkInfo contains
 * the head of the chunk's free arena list together with the link fields for
 * gcChunkList.
 *
 * Structure stored in one of chunk's free arenas. GetChunkInfoIndex gives the
 * index of this arena. When all arenas in the chunk are used, it is removed
 * from the list and the index is set to NO_FREE_ARENAS indicating that the
 * chunk is not on gcChunkList and has no JSGCChunkInfo available.
 */

struct JSGCChunkInfo {
    JSGCChunkInfo   **prevp;
    JSGCChunkInfo   *next;
    JSGCArena       *lastFreeArena;
    uint32          numFreeArenas;
};

#define NO_FREE_ARENAS              JS_BITMASK(GC_ARENA_SHIFT - 1)

JS_STATIC_ASSERT(1 <= GC_ARENAS_PER_CHUNK &&
                 GC_ARENAS_PER_CHUNK <= NO_FREE_ARENAS);

inline unsigned
GetArenaIndex(JSGCArena *a)
{
    return a->info.firstArena ? 0 : unsigned(a->info.arenaIndex);
}

inline jsuword
GetArenaChunk(JSGCArena *a, unsigned index)
{
    JS_ASSERT(index == GetArenaIndex(a));
    return a->toPageStart() - (index << GC_ARENA_SHIFT);
}

inline unsigned
GetChunkInfoIndex(jsuword chunk)
{
    JSGCArena *a = JSGCArena::fromPageStart(chunk);
    JS_ASSERT(a->info.firstArena);
    return a->info.arenaIndex;
}

inline void
SetChunkInfoIndex(jsuword chunk, unsigned index)
{
    JS_ASSERT(index < GC_ARENAS_PER_CHUNK || index == NO_FREE_ARENAS);
    JSGCArena *a = JSGCArena::fromPageStart(chunk);
    JS_ASSERT(a->info.firstArena);
    a->info.arenaIndex = jsuword(index);
}

inline JSGCChunkInfo *
GetChunkInfo(jsuword chunk, unsigned infoIndex)
{
    JS_ASSERT(GetChunkInfoIndex(chunk) == infoIndex);
    JS_ASSERT(infoIndex < GC_ARENAS_PER_CHUNK);
    return reinterpret_cast<JSGCChunkInfo *>(chunk +
                                             (infoIndex << GC_ARENA_SHIFT));
}

inline JSGCArena *
InitChunkArena(jsuword chunk, unsigned index)
{
    JS_ASSERT(index < GC_ARENAS_PER_CHUNK);
    JSGCArena *a = JSGCArena::fromPageStart(chunk + (index << GC_ARENA_SHIFT));
    a->info.firstArena = (index == 0);
    a->info.arenaIndex = index;
    return a;
}

/*
 * Helpers for GC-thing operations.
 */
inline JSGCThing *
NextThing(JSGCThing *thing, size_t thingSize)
{
    return reinterpret_cast<JSGCThing *>(reinterpret_cast<jsuword>(thing) +
                                         thingSize);
}

inline size_t
ThingsPerArena(size_t thingSize)
{
    JS_ASSERT(!(thingSize & GC_CELL_MASK));
    JS_ASSERT(thingSize <= GC_ARENA_CELLS_SIZE);
    return GC_ARENA_CELLS_SIZE / thingSize;
}

inline jsuword
ThingToOffset(void *thing)
{
    JS_ASSERT(!JSString::isStatic(thing));
    jsuword offset = reinterpret_cast<jsuword>(thing) & GC_ARENA_MASK;
    JS_ASSERT(offset < GC_ARENA_CELLS_SIZE);
    JS_ASSERT(!(offset & GC_CELL_MASK));
    return offset;
}

inline JSGCThing *
OffsetToThing(JSGCArena *a, jsuword offset)
{
    JS_ASSERT(offset < GC_ARENA_CELLS_SIZE);
    JS_ASSERT(!(offset & GC_CELL_MASK));
    return reinterpret_cast<JSGCThing *>(a->toPageStart() | offset);
}

inline jsuword
ThingToGCCellIndex(void *thing)
{
    jsuword offset = ThingToOffset(thing);
    return offset >> GC_CELL_SHIFT;
}

inline bool
IsMarkedGCThing(JSGCArena *a, void *thing)
{
    JS_ASSERT(a == JSGCArena::fromGCThing(thing));
    jsuword index = ThingToGCCellIndex(thing);
    return !!JS_TEST_BIT(a->markBitmap, index);
}

inline bool
IsMarkedGCThing(JSGCArena *a, jsuword thingOffset)
{
    JS_ASSERT(thingOffset < GC_ARENA_CELLS_SIZE);
    JS_ASSERT(!(thingOffset & GC_CELL_MASK));
    jsuword index = thingOffset >> GC_CELL_SHIFT;
    return !!JS_TEST_BIT(a->markBitmap, index);
}

inline bool
MarkIfUnmarkedGCThing(JSGCArena *a, void *thing)
{
    JS_ASSERT(a == JSGCArena::fromGCThing(thing));
    jsuword index = ThingToGCCellIndex(thing);
    if (JS_TEST_BIT(a->markBitmap, index))
        return false;
    JS_SET_BIT(a->markBitmap, index);
    return true;
}

static inline JSGCThing *
MakeNewArenaFreeList(JSGCArena *a, size_t thingSize)
{
    jsuword thingsStart = a->toPageStart();
    jsuword lastThingMinAddr = thingsStart + GC_ARENA_CELLS_SIZE -
                               thingSize * 2 + 1;
    jsuword thingPtr = thingsStart;
    do {
        jsuword nextPtr = thingPtr + thingSize;
        JS_ASSERT((nextPtr & GC_ARENA_MASK) + thingSize <= GC_ARENA_CELLS_SIZE);
        JSGCThing *thing = reinterpret_cast<JSGCThing *>(thingPtr);
        thing->link = reinterpret_cast<JSGCThing *>(nextPtr);
        thingPtr = nextPtr;
    } while (thingPtr < lastThingMinAddr);

    JSGCThing *lastThing = reinterpret_cast<JSGCThing *>(thingPtr);
    lastThing->link = NULL;
    return reinterpret_cast<JSGCThing *>(thingsStart);
}

#ifdef JS_GCMETER
# define METER(x)               ((void) (x))
# define METER_IF(condition, x) ((void) ((condition) && (x)))
#else
# define METER(x)               ((void) 0)
# define METER_IF(condition, x) ((void) 0)
#endif

#define METER_UPDATE_MAX(maxLval, rval)                                       \
    METER_IF((maxLval) < (rval), (maxLval) = (rval))

#ifdef MOZ_GCTIMER
static jsrefcount newChunkCount = 0;
static jsrefcount destroyChunkCount = 0;
#endif

static jsuword
NewGCChunk(void)
{
    void *p;
#ifdef MOZ_GCTIMER
    JS_ATOMIC_INCREMENT(&newChunkCount);
#endif
#if defined(XP_WIN)
    p = VirtualAlloc(NULL, GC_CHUNK_SIZE,
                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return (jsuword) p;
#elif defined(XP_OS2)
    if (DosAllocMem(&p, GC_CHUNK_SIZE,
                    OBJ_ANY | PAG_COMMIT | PAG_READ | PAG_WRITE)) {
        if (DosAllocMem(&p, GC_CHUNK_SIZE, PAG_COMMIT | PAG_READ | PAG_WRITE))
            return 0;
    }
    return (jsuword) p;
#else
    p = mmap(NULL, GC_CHUNK_SIZE,
             PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? 0 : (jsuword) p;
#endif
}

static void
DestroyGCChunk(jsuword chunk)
{
#ifdef MOZ_GCTIMER
    JS_ATOMIC_INCREMENT(&destroyChunkCount);
#endif
    JS_ASSERT((chunk & GC_ARENA_MASK) == 0);
#if defined(XP_WIN)
    VirtualFree((void *) chunk, 0, MEM_RELEASE);
#elif defined(XP_OS2)
    DosFreeMem((void *) chunk);
#elif defined(SOLARIS)
    munmap((char *) chunk, GC_CHUNK_SIZE);
#else
    munmap((void *) chunk, GC_CHUNK_SIZE);
#endif
}

static void
AddChunkToList(JSRuntime *rt, JSGCChunkInfo *ci)
{
    ci->prevp = &rt->gcChunkList;
    ci->next = rt->gcChunkList;
    if (rt->gcChunkList) {
        JS_ASSERT(rt->gcChunkList->prevp == &rt->gcChunkList);
        rt->gcChunkList->prevp = &ci->next;
    }
    rt->gcChunkList = ci;
}

static void
RemoveChunkFromList(JSRuntime *rt, JSGCChunkInfo *ci)
{
    *ci->prevp = ci->next;
    if (ci->next) {
        JS_ASSERT(ci->next->prevp == &ci->next);
        ci->next->prevp = ci->prevp;
    }
}

static JSGCArena *
NewGCArena(JSContext *cx)
{
    jsuword chunk;
    JSGCArena *a;

    JSRuntime *rt = cx->runtime;
    if (!JS_THREAD_DATA(cx)->waiveGCQuota && rt->gcBytes >= rt->gcMaxBytes) {
        /*
         * FIXME bug 524051 We cannot run a last-ditch GC on trace for now, so
         * just pretend we are out of memory which will throw us off trace and
         * we will re-try this code path from the interpreter.
         */
        if (!JS_ON_TRACE(cx))
            return NULL;
        js_TriggerGC(cx, true);
    }

    JSGCChunkInfo *ci;
    unsigned i;
    JSGCArena *aprev;

    ci = rt->gcChunkList;
    if (!ci) {
        chunk = NewGCChunk();
        if (chunk == 0)
            return NULL;
        JS_ASSERT((chunk & GC_ARENA_MASK) == 0);
        a = InitChunkArena(chunk, 0);
        aprev = NULL;
        i = 0;
        do {
            a->info.prev = aprev;
            aprev = a;
            ++i;
            a = InitChunkArena(chunk, i);
        } while (i != GC_ARENAS_PER_CHUNK - 1);
        ci = GetChunkInfo(chunk, 0);
        ci->lastFreeArena = aprev;
        ci->numFreeArenas = GC_ARENAS_PER_CHUNK - 1;
        AddChunkToList(rt, ci);
    } else {
        JS_ASSERT(ci->prevp == &rt->gcChunkList);
        a = ci->lastFreeArena;
        aprev = a->info.prev;
        if (!aprev) {
            JS_ASSERT(ci->numFreeArenas == 1);
            JS_ASSERT(a->toPageStart() == (jsuword) ci);
            RemoveChunkFromList(rt, ci);
            chunk = GetArenaChunk(a, GetArenaIndex(a));
            SetChunkInfoIndex(chunk, NO_FREE_ARENAS);
        } else {
            JS_ASSERT(ci->numFreeArenas >= 2);
            JS_ASSERT(a->toPageStart() != (jsuword) ci);
            ci->lastFreeArena = aprev;
            ci->numFreeArenas--;
        }
    }

    rt->gcBytes += GC_ARENA_SIZE;

    return a;
}

static void
DestroyGCArenas(JSRuntime *rt, JSGCArena *last)
{
    JSGCArena *a;

    while (last) {
        a = last;
        last = last->info.prev;

        METER(rt->gcStats.afree++);
        JS_ASSERT(rt->gcBytes >= GC_ARENA_SIZE);
        rt->gcBytes -= GC_ARENA_SIZE;

        uint32 arenaIndex;
        jsuword chunk;
        uint32 chunkInfoIndex;
        JSGCChunkInfo *ci;
#ifdef DEBUG
        jsuword firstArena;

        firstArena = a->info.firstArena;
        arenaIndex = a->info.arenaIndex;
        memset(a, JS_FREE_PATTERN, GC_ARENA_SIZE);
        a->info.firstArena = firstArena;
        a->info.arenaIndex = arenaIndex;
#endif
        arenaIndex = GetArenaIndex(a);
        chunk = GetArenaChunk(a, arenaIndex);
        chunkInfoIndex = GetChunkInfoIndex(chunk);
        if (chunkInfoIndex == NO_FREE_ARENAS) {
            chunkInfoIndex = arenaIndex;
            SetChunkInfoIndex(chunk, arenaIndex);
            ci = GetChunkInfo(chunk, chunkInfoIndex);
            a->info.prev = NULL;
            ci->lastFreeArena = a;
            ci->numFreeArenas = 1;
            AddChunkToList(rt, ci);
        } else {
            JS_ASSERT(chunkInfoIndex != arenaIndex);
            ci = GetChunkInfo(chunk, chunkInfoIndex);
            JS_ASSERT(ci->numFreeArenas != 0);
            JS_ASSERT(ci->lastFreeArena);
            JS_ASSERT(a != ci->lastFreeArena);
            if (ci->numFreeArenas == GC_ARENAS_PER_CHUNK - 1) {
                RemoveChunkFromList(rt, ci);
                DestroyGCChunk(chunk);
            } else {
                ++ci->numFreeArenas;
                a->info.prev = ci->lastFreeArena;
                ci->lastFreeArena = a;
            }
        }
    }
}

static inline size_t
GetFinalizableThingSize(unsigned thingKind)
{
    JS_STATIC_ASSERT(JS_EXTERNAL_STRING_LIMIT == 8);

    static const uint8 map[FINALIZE_LIMIT] = {
        sizeof(JSObject),   /* FINALIZE_OBJECT */
        sizeof(JSFunction), /* FINALIZE_FUNCTION */
#if JS_HAS_XML_SUPPORT
        sizeof(JSXML),      /* FINALIZE_XML */
#endif
        sizeof(JSString),   /* FINALIZE_STRING */
        sizeof(JSString),   /* FINALIZE_EXTERNAL_STRING0 */
        sizeof(JSString),   /* FINALIZE_EXTERNAL_STRING1 */
        sizeof(JSString),   /* FINALIZE_EXTERNAL_STRING2 */
        sizeof(JSString),   /* FINALIZE_EXTERNAL_STRING3 */
        sizeof(JSString),   /* FINALIZE_EXTERNAL_STRING4 */
        sizeof(JSString),   /* FINALIZE_EXTERNAL_STRING5 */
        sizeof(JSString),   /* FINALIZE_EXTERNAL_STRING6 */
        sizeof(JSString),   /* FINALIZE_EXTERNAL_STRING7 */
    };

    JS_ASSERT(thingKind < FINALIZE_LIMIT);
    return map[thingKind];
}

static inline size_t
GetFinalizableTraceKind(size_t thingKind)
{
    JS_STATIC_ASSERT(JS_EXTERNAL_STRING_LIMIT == 8);

    static const uint8 map[FINALIZE_LIMIT] = {
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT */
        JSTRACE_OBJECT,     /* FINALIZE_FUNCTION */
#if JS_HAS_XML_SUPPORT      /* FINALIZE_XML */
        JSTRACE_XML,
#endif                      /* FINALIZE_STRING */
        JSTRACE_STRING,
        JSTRACE_STRING,     /* FINALIZE_EXTERNAL_STRING0 */
        JSTRACE_STRING,     /* FINALIZE_EXTERNAL_STRING1 */
        JSTRACE_STRING,     /* FINALIZE_EXTERNAL_STRING2 */
        JSTRACE_STRING,     /* FINALIZE_EXTERNAL_STRING3 */
        JSTRACE_STRING,     /* FINALIZE_EXTERNAL_STRING4 */
        JSTRACE_STRING,     /* FINALIZE_EXTERNAL_STRING5 */
        JSTRACE_STRING,     /* FINALIZE_EXTERNAL_STRING6 */
        JSTRACE_STRING,     /* FINALIZE_EXTERNAL_STRING7 */
    };

    JS_ASSERT(thingKind < FINALIZE_LIMIT);
    return map[thingKind];
}

static inline size_t
GetFinalizableArenaTraceKind(JSGCArena *a)
{
    JS_ASSERT(a->info.list);
    return GetFinalizableTraceKind(a->info.list->thingKind);
}

static void
InitGCArenaLists(JSRuntime *rt)
{
    for (unsigned i = 0; i != FINALIZE_LIMIT; ++i) {
        JSGCArenaList *arenaList = &rt->gcArenaList[i];
        arenaList->head = NULL;
        arenaList->cursor = NULL;
        arenaList->thingKind = i;
        arenaList->thingSize = GetFinalizableThingSize(i);
    }
    rt->gcDoubleArenaList.head = NULL;
    rt->gcDoubleArenaList.cursor = NULL;
}

static void
FinishGCArenaLists(JSRuntime *rt)
{
    for (unsigned i = 0; i < FINALIZE_LIMIT; i++) {
        JSGCArenaList *arenaList = &rt->gcArenaList[i];
        DestroyGCArenas(rt, arenaList->head);
        arenaList->head = NULL;
        arenaList->cursor = NULL;
    }
    DestroyGCArenas(rt, rt->gcDoubleArenaList.head);
    rt->gcDoubleArenaList.head = NULL;
    rt->gcDoubleArenaList.cursor = NULL;

    rt->gcBytes = 0;
    JS_ASSERT(rt->gcChunkList == 0);
}

intN
js_GetExternalStringGCType(JSString *str)
{
    JS_STATIC_ASSERT(FINALIZE_STRING + 1 == FINALIZE_EXTERNAL_STRING0);
    JS_ASSERT(!JSString::isStatic(str));

    unsigned thingKind = JSGCArena::fromGCThing(str)->info.list->thingKind;
    JS_ASSERT(IsFinalizableStringKind(thingKind));
    return intN(thingKind) - intN(FINALIZE_EXTERNAL_STRING0);
}

JS_FRIEND_API(uint32)
js_GetGCThingTraceKind(void *thing)
{
    if (JSString::isStatic(thing))
        return JSTRACE_STRING;

    JSGCArena *a = JSGCArena::fromGCThing(thing);
    if (!a->info.list)
        return JSTRACE_DOUBLE;
    return GetFinalizableArenaTraceKind(a);
}

JSRuntime*
js_GetGCStringRuntime(JSString *str)
{
    JSGCArenaList *list = JSGCArena::fromGCThing(str)->info.list;
    JS_ASSERT(list->thingSize == sizeof(JSString));

    unsigned i = list->thingKind;
    JS_ASSERT(i == FINALIZE_STRING ||
              (FINALIZE_EXTERNAL_STRING0 <= i &&
               i < FINALIZE_EXTERNAL_STRING0 + JS_EXTERNAL_STRING_LIMIT));
    return (JSRuntime *)((uint8 *)(list - i) -
                         offsetof(JSRuntime, gcArenaList));
}

bool
js_IsAboutToBeFinalized(void *thing)
{
    if (JSString::isStatic(thing))
        return false;

    JSGCArena *a = JSGCArena::fromGCThing(thing);
    if (!a->info.list) {
        /*
         * Check if arena has no marked doubles. In that case the bitmap with
         * the mark flags contains all garbage as it is initialized only when
         * marking the first double in the arena.
         */
        if (!a->info.hasMarkedDoubles)
            return true;
    }
    return !IsMarkedGCThing(a, thing);
}

/* This is compatible with JSDHashEntryStub. */
typedef struct JSGCRootHashEntry {
    JSDHashEntryHdr hdr;
    void            *root;
    const char      *name;
} JSGCRootHashEntry;

/*
 * Initial size of the gcRootsHash and gcLocksHash tables (SWAG, small enough
 * to amortize).
 */
const uint32 GC_ROOTS_SIZE = 256;

struct JSGCLockHashEntry : public JSDHashEntryHdr
{
    const void      *thing;
    uint32          count;
};

JSBool
js_InitGC(JSRuntime *rt, uint32 maxbytes)
{
    InitGCArenaLists(rt);
    if (!JS_DHashTableInit(&rt->gcRootsHash, JS_DHashGetStubOps(), NULL,
                           sizeof(JSGCRootHashEntry), GC_ROOTS_SIZE)) {
        rt->gcRootsHash.ops = NULL;
        return false;
    }
    if (!JS_DHashTableInit(&rt->gcLocksHash, JS_DHashGetStubOps(), NULL,
                           sizeof(JSGCLockHashEntry), GC_ROOTS_SIZE)) {
        rt->gcLocksHash.ops = NULL;
        return false;
    }

    /*
     * Separate gcMaxMallocBytes from gcMaxBytes but initialize to maxbytes
     * for default backward API compatibility.
     */
    rt->gcMaxBytes = maxbytes;
    rt->setGCMaxMallocBytes(maxbytes);

    rt->gcEmptyArenaPoolLifespan = 30000;

    /*
     * By default the trigger factor gets maximum possible value. This
     * means that GC will not be triggered by growth of GC memory (gcBytes).
     */
    rt->setGCTriggerFactor((uint32) -1);

    /*
     * The assigned value prevents GC from running when GC memory is too low
     * (during JS engine start).
     */
    rt->setGCLastBytes(8192);

    METER(PodZero(&rt->gcStats));
    return true;
}

#ifdef JS_GCMETER

static void
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
        "double",
        "object",
        "function",
#if JS_HAS_XML_SUPPORT
        "xml",
#endif
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
    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(GC_ARENA_NAMES) == FINALIZE_LIMIT + 1);

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
    for (int i = -1; i < (int) FINALIZE_LIMIT; i++) {
        size_t thingSize, thingsPerArena;
        JSGCArenaStats *st;
        if (i == -1) {
            thingSize = sizeof(jsdouble);
            thingsPerArena = DOUBLES_PER_ARENA;
            st = &rt->gcStats.doubleArenaStats;
        } else {
            thingSize = rt->gcArenaList[i].thingSize;
            thingsPerArena = ThingsPerArena(thingSize);
            st = &rt->gcStats.arenaStats[i];
        }
        if (st->maxarenas == 0)
            continue;
        fprintf(fp,
                "%s arenas (thing size %lu, %lu things per arena):",
                GC_ARENA_NAMES[i + 1], UL(thingSize), UL(thingsPerArena));
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
    for (int i = -1; i < (int) FINALIZE_LIMIT; i++) {
        size_t thingSize, thingsPerArena;
        JSGCArenaStats *st;
        if (i == -1) {
            thingSize = sizeof(jsdouble);
            thingsPerArena = DOUBLES_PER_ARENA;
            st = &rt->gcStats.doubleArenaStats;
        } else {
            thingSize = rt->gcArenaList[i].thingSize;
            thingsPerArena = ThingsPerArena(thingSize);
            st = &rt->gcStats.arenaStats[i];
        }
        if (st->maxarenas != 0)
            continue;
        fprintf(fp,
                "%s (thing size %lu, %lu things per arena)\n",
                GC_ARENA_NAMES[i + 1], UL(thingSize), UL(thingsPerArena));
    }
    fprintf(fp, "\nTOTAL STATS:\n");
    fprintf(fp, "            bytes allocated: %lu\n", UL(rt->gcBytes));
    fprintf(fp, "            total GC arenas: %lu\n", UL(sumArenas));
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
    fprintf(fp, "         things born locked: %lu\n", ULSTAT(lockborn));
    fprintf(fp, "           valid lock calls: %lu\n", ULSTAT(lock));
    fprintf(fp, "         valid unlock calls: %lu\n", ULSTAT(unlock));
    fprintf(fp, "       mark recursion depth: %lu\n", ULSTAT(depth));
    fprintf(fp, "     maximum mark recursion: %lu\n", ULSTAT(maxdepth));
    fprintf(fp, "     mark C recursion depth: %lu\n", ULSTAT(cdepth));
    fprintf(fp, "   maximum mark C recursion: %lu\n", ULSTAT(maxcdepth));
    fprintf(fp, "      delayed tracing calls: %lu\n", ULSTAT(unmarked));
#ifdef DEBUG
    fprintf(fp, "      max trace later count: %lu\n", ULSTAT(maxunmarked));
#endif
    fprintf(fp, "   maximum GC nesting level: %lu\n", ULSTAT(maxlevel));
    fprintf(fp, "potentially useful GC calls: %lu\n", ULSTAT(poke));
    fprintf(fp, "  thing arenas freed so far: %lu\n", ULSTAT(afree));
    fprintf(fp, "     stack segments scanned: %lu\n", ULSTAT(stackseg));
    fprintf(fp, "stack segment slots scanned: %lu\n", ULSTAT(segslots));
    fprintf(fp, "reachable closeable objects: %lu\n", ULSTAT(nclose));
    fprintf(fp, "    max reachable closeable: %lu\n", ULSTAT(maxnclose));
    fprintf(fp, "      scheduled close hooks: %lu\n", ULSTAT(closelater));
    fprintf(fp, "  max scheduled close hooks: %lu\n", ULSTAT(maxcloselater));

#undef UL
#undef ULSTAT
#undef PERCENT
}
#endif

#ifdef DEBUG
static void
CheckLeakedRoots(JSRuntime *rt);
#endif

void
js_FinishGC(JSRuntime *rt)
{
#ifdef JS_ARENAMETER
    JS_DumpArenaStats(stdout);
#endif
#ifdef JS_GCMETER
    if (JS_WANT_GC_METER_PRINT)
        js_DumpGCStats(rt, stdout);
#endif

    rt->gcIteratorTable.clear();
    FinishGCArenaLists(rt);

    if (rt->gcRootsHash.ops) {
#ifdef DEBUG
        CheckLeakedRoots(rt);
#endif
        JS_DHashTableFinish(&rt->gcRootsHash);
        rt->gcRootsHash.ops = NULL;
    }
    if (rt->gcLocksHash.ops) {
        JS_DHashTableFinish(&rt->gcLocksHash);
        rt->gcLocksHash.ops = NULL;
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
     */
    JS_LOCK_GC(rt);
    js_WaitForGC(rt);
    rhe = (JSGCRootHashEntry *)
          JS_DHashTableOperate(&rt->gcRootsHash, rp, JS_DHASH_ADD);
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
    js_WaitForGC(rt);
    (void) JS_DHashTableOperate(&rt->gcRootsHash, rp, JS_DHASH_REMOVE);
    rt->gcPoke = JS_TRUE;
    JS_UNLOCK_GC(rt);
    return JS_TRUE;
}

#ifdef DEBUG

static JSDHashOperator
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

static void
CheckLeakedRoots(JSRuntime *rt)
{
    uint32 leakedroots = 0;

    /* Warn (but don't assert) debug builds of any remaining roots. */
    JS_DHashTableEnumerate(&rt->gcRootsHash, js_root_printer,
                           &leakedroots);
    if (leakedroots > 0) {
        if (leakedroots == 1) {
            fprintf(stderr,
"JS engine warning: 1 GC root remains after destroying the JSRuntime at %p.\n"
"                   This root may point to freed memory. Objects reachable\n"
"                   through it have not been finalized.\n",
                    (void *) rt);
        } else {
            fprintf(stderr,
"JS engine warning: %lu GC roots remain after destroying the JSRuntime at %p.\n"
"                   These roots may point to freed memory. Objects reachable\n"
"                   through them have not been finalized.\n",
                    (unsigned long) leakedroots, (void *) rt);
        }
    }
}

typedef struct NamedRootDumpArgs {
    void (*dump)(const char *name, void *rp, void *data);
    void *data;
} NamedRootDumpArgs;

static JSDHashOperator
js_named_root_dumper(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 number,
                     void *arg)
{
    NamedRootDumpArgs *args = (NamedRootDumpArgs *) arg;
    JSGCRootHashEntry *rhe = (JSGCRootHashEntry *)hdr;

    if (rhe->name)
        args->dump(rhe->name, rhe->root, args->data);
    return JS_DHASH_NEXT;
}

JS_BEGIN_EXTERN_C
void
js_DumpNamedRoots(JSRuntime *rt,
                  void (*dump)(const char *name, void *rp, void *data),
                  void *data)
{
    NamedRootDumpArgs args;

    args.dump = dump;
    args.data = data;
    JS_DHashTableEnumerate(&rt->gcRootsHash, js_named_root_dumper, &args);
}
JS_END_EXTERN_C

#endif /* DEBUG */

typedef struct GCRootMapArgs {
    JSGCRootMapFun map;
    void *data;
} GCRootMapArgs;

static JSDHashOperator
js_gcroot_mapper(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 number,
                 void *arg)
{
    GCRootMapArgs *args = (GCRootMapArgs *) arg;
    JSGCRootHashEntry *rhe = (JSGCRootHashEntry *)hdr;
    intN mapflags;
    int op;

    mapflags = args->map(rhe->root, rhe->name, args->data);

#if JS_MAP_GCROOT_NEXT == JS_DHASH_NEXT &&                                     \
    JS_MAP_GCROOT_STOP == JS_DHASH_STOP &&                                     \
    JS_MAP_GCROOT_REMOVE == JS_DHASH_REMOVE
    op = (JSDHashOperator)mapflags;
#else
    op = JS_DHASH_NEXT;
    if (mapflags & JS_MAP_GCROOT_STOP)
        op |= JS_DHASH_STOP;
    if (mapflags & JS_MAP_GCROOT_REMOVE)
        op |= JS_DHASH_REMOVE;
#endif

    return (JSDHashOperator) op;
}

uint32
js_MapGCRoots(JSRuntime *rt, JSGCRootMapFun map, void *data)
{
    GCRootMapArgs args;
    uint32 rv;

    args.map = map;
    args.data = data;
    JS_LOCK_GC(rt);
    rv = JS_DHashTableEnumerate(&rt->gcRootsHash, js_gcroot_mapper, &args);
    JS_UNLOCK_GC(rt);
    return rv;
}

JSBool
js_RegisterCloseableIterator(JSContext *cx, JSObject *obj)
{
    JSRuntime *rt;
    JSBool ok;

    rt = cx->runtime;
    JS_ASSERT(!rt->gcRunning);

    JS_LOCK_GC(rt);
    ok = rt->gcIteratorTable.append(obj);
    JS_UNLOCK_GC(rt);
    return ok;
}

static void
CloseNativeIterators(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    size_t length = rt->gcIteratorTable.length();
    JSObject **array = rt->gcIteratorTable.begin();

    size_t newLength = 0;
    for (size_t i = 0; i < length; ++i) {
        JSObject *obj = array[i];
        if (js_IsAboutToBeFinalized(obj))
            js_CloseNativeIterator(cx, obj);
        else
            array[newLength++] = obj;
    }
    rt->gcIteratorTable.resize(newLength);
}

void
JSRuntime::setGCTriggerFactor(uint32 factor)
{
    JS_ASSERT(factor >= 100);

    gcTriggerFactor = factor;
    setGCLastBytes(gcLastBytes);
}

void
JSRuntime::setGCLastBytes(size_t lastBytes)
{
    gcLastBytes = lastBytes;
    uint64 triggerBytes = uint64(lastBytes) * uint64(gcTriggerFactor / 100);
    if (triggerBytes != size_t(triggerBytes))
        triggerBytes = size_t(-1);
    gcTriggerBytes = size_t(triggerBytes);
}

void
JSGCFreeLists::purge()
{
    /*
     * Return the free list back to the arena so the GC finalization will not
     * run the finalizers over unitialized bytes from free things.
     */
    for (JSGCThing **p = finalizables; p != JS_ARRAY_END(finalizables); ++p) {
        JSGCThing *freeListHead = *p;
        if (freeListHead) {
            JSGCArena *a = JSGCArena::fromGCThing(freeListHead);
            JS_ASSERT(!a->info.freeList);
            a->info.freeList = freeListHead;
            *p = NULL;
        }
    }
    doubles = NULL;
}

void
JSGCFreeLists::moveTo(JSGCFreeLists *another)
{
    *another = *this;
    doubles = NULL;
    PodArrayZero(finalizables);
    JS_ASSERT(isEmpty());
}

static inline bool
IsGCThresholdReached(JSRuntime *rt)
{
#ifdef JS_GC_ZEAL
    if (rt->gcZeal >= 1)
        return true;
#endif

    /*
     * Since the initial value of the gcLastBytes parameter is not equal to
     * zero (see the js_InitGC function) the return value is false when
     * the gcBytes value is close to zero at the JS engine start.
     */
    return rt->isGCMallocLimitReached() || rt->gcBytes >= rt->gcTriggerBytes;
}

static inline JSGCFreeLists *
GetGCFreeLists(JSContext *cx)
{
    JSThreadData *td = JS_THREAD_DATA(cx);
    if (!td->localRootStack)
        return &td->gcFreeLists;
    JS_ASSERT(td->gcFreeLists.isEmpty());
    return &td->localRootStack->gcFreeLists;
}

static JSGCThing *
RefillFinalizableFreeList(JSContext *cx, unsigned thingKind)
{
    JS_ASSERT(!GetGCFreeLists(cx)->finalizables[thingKind]);
    JSRuntime *rt = cx->runtime;
    JS_LOCK_GC(rt);
    JS_ASSERT(!rt->gcRunning);
    if (rt->gcRunning) {
        METER(rt->gcStats.finalfail++);
        JS_UNLOCK_GC(rt);
        return NULL;
    }

    bool canGC = !JS_ON_TRACE(cx) && !JS_THREAD_DATA(cx)->waiveGCQuota;
    bool doGC = canGC && IsGCThresholdReached(rt);
    JSGCArenaList *arenaList = &rt->gcArenaList[thingKind];
    JSGCArena *a;
    for (;;) {
        if (doGC) {
            /*
             * Keep rt->gcLock across the call into js_GC so we don't starve
             * and lose to racing threads who deplete the heap just after
             * js_GC has replenished it (or has synchronized with a racing
             * GC that collected a bunch of garbage).  This unfair scheduling
             * can happen on certain operating systems. For the gory details,
             * see bug 162779 at https://bugzilla.mozilla.org/.
             */
            js_GC(cx, GC_LAST_DITCH);
            METER(cx->runtime->gcStats.arenaStats[thingKind].retry++);
            canGC = false;

            /*
             * The JSGC_END callback can legitimately allocate new GC things
             * and populate the free list. If that happens, just return that
             * list head.
             */
            JSGCThing *freeList = GetGCFreeLists(cx)->finalizables[thingKind];
            if (freeList) {
                JS_UNLOCK_GC(rt);
                return freeList;
            }
        }

        while ((a = arenaList->cursor) != NULL) {
            arenaList->cursor = a->info.prev;
            JSGCThing *freeList = a->info.freeList;
            if (freeList) {
                a->info.freeList = NULL;
                JS_UNLOCK_GC(rt);
                return freeList;
            }
        }

        a = NewGCArena(cx);
        if (a)
            break;
        if (!canGC) {
            METER(cx->runtime->gcStats.arenaStats[thingKind].fail++);
            JS_UNLOCK_GC(rt);
            return NULL;
        }
        doGC = true;
    }

    /*
     * Do only minimal initialization of the arena inside the GC lock. We
     * can do the rest outside the lock because no other threads will see
     * the arena until the GC is run.
     */
    a->info.list = arenaList;
    a->info.prev = arenaList->head;
    a->clearPrevUnmarked();
    a->info.freeList = NULL;
    a->info.unmarkedChildren = 0;
    arenaList->head = a;
    JS_UNLOCK_GC(rt);

    a->clearMarkBitmap();
    return MakeNewArenaFreeList(a, arenaList->thingSize);
}

static inline void
CheckGCFreeListLink(JSGCThing *thing)
{
    /*
     * The GC things on the free lists come from one arena and the things on
     * the free list are linked in ascending address order.
     */
    JS_ASSERT_IF(thing->link,
                 JSGCArena::fromGCThing(thing) ==
                 JSGCArena::fromGCThing(thing->link));
    JS_ASSERT_IF(thing->link, thing < thing->link);
}

void *
js_NewFinalizableGCThing(JSContext *cx, unsigned thingKind)
{
    JS_ASSERT(thingKind < FINALIZE_LIMIT);
#ifdef JS_THREADSAFE
    JS_ASSERT(cx->thread);
#endif

    /* Updates of metering counters here may not be thread-safe. */
    METER(cx->runtime->gcStats.arenaStats[thingKind].alloc++);

    JSGCThing **freeListp =
        JS_THREAD_DATA(cx)->gcFreeLists.finalizables + thingKind;
    JSGCThing *thing = *freeListp;
    if (thing) {
        JS_ASSERT(!JS_THREAD_DATA(cx)->localRootStack);
        *freeListp = thing->link;
        cx->weakRoots.finalizableNewborns[thingKind] = thing;
        CheckGCFreeListLink(thing);
        METER(cx->runtime->gcStats.arenaStats[thingKind].localalloc++);
        return thing;
    }

    /*
     * To avoid for the local roots on each GC allocation when the local roots
     * are not active we move the GC free lists from JSThreadData to lrs in
     * JS_EnterLocalRootScope(). This way with inactive local roots we only
     * check for non-null lrs only when we exhaust the free list.
     */
    JSLocalRootStack *lrs = JS_THREAD_DATA(cx)->localRootStack;
    for (;;) {
        if (lrs) {
            freeListp = lrs->gcFreeLists.finalizables + thingKind;
            thing = *freeListp;
            if (thing) {
                *freeListp = thing->link;
                METER(cx->runtime->gcStats.arenaStats[thingKind].localalloc++);
                break;
            }
        }

        thing = RefillFinalizableFreeList(cx, thingKind);
        if (thing) {
            /*
             * See comments in RefillFinalizableFreeList about a possibility
             * of *freeListp == thing.
             */
            JS_ASSERT(!*freeListp || *freeListp == thing);
            *freeListp = thing->link;
            break;
        }

        js_ReportOutOfMemory(cx);
        return NULL;
    }

    CheckGCFreeListLink(thing);
    if (lrs) {
        /*
         * If we're in a local root scope, don't set newborn[type] at all, to
         * avoid entraining garbage from it for an unbounded amount of time
         * on this context.  A caller will leave the local root scope and pop
         * this reference, allowing thing to be GC'd if it has no other refs.
         * See JS_EnterLocalRootScope and related APIs.
         */
        if (js_PushLocalRoot(cx, lrs, (jsval) thing) < 0) {
            JS_ASSERT(thing->link == *freeListp);
            *freeListp = thing;
            return NULL;
        }
    } else {
        /*
         * No local root scope, so we're stuck with the old, fragile model of
         * depending on a pigeon-hole newborn per type per context.
         */
        cx->weakRoots.finalizableNewborns[thingKind] = thing;
    }

    return thing;
}

static JSGCThing *
TurnUsedArenaIntoDoubleList(JSGCArena *a)
{
    JSGCThing *head;
    JSGCThing **tailp = &head;
    jsuword thing = a->toPageStart();
    jsbitmap *lastMarkWord = a->getMarkBitmapEnd() - 1;

    for (jsbitmap *m = a->markBitmap; m <= lastMarkWord; ++m) {
        JS_ASSERT(thing < a->toPageStart() + GC_ARENA_CELLS_SIZE);
        JS_ASSERT((thing - a->toPageStart()) %
                  (JS_BITS_PER_WORD * sizeof(jsdouble)) == 0);

        jsbitmap bits = *m;
        if (bits == jsbitmap(-1)) {
            thing += JS_BITS_PER_WORD * sizeof(jsdouble);
        } else {
            /*
             * We have some zero bits. Turn corresponding cells into a list
             * unrolling the loop for better performance.
             *
             * When m points the last bitmap's word in the arena, its high
             * bits corresponds to non-existing cells and thingptr is outside
             * the space allocated for doubles. For code simplicity we set
             * such bits to 1 here. Thus code below can assume a bit is unset
             * iff the corresponding cell exists and free.
             */
            if (m == lastMarkWord) {
                const size_t unusedBits =
                    GC_ARENA_MARK_BITMAP_WORDS * JS_BITS_PER_WORD -
                    DOUBLES_PER_ARENA;
                JS_STATIC_ASSERT(unusedBits < JS_BITS_PER_WORD);

                const jsbitmap mask = (jsbitmap(1) << unusedBits) - 1;
                const size_t nused = JS_BITS_PER_WORD - unusedBits;
                bits |= mask << nused;
            }
            const unsigned unroll = 4;
            const jsbitmap unrollMask = (jsbitmap(1) << unroll) - 1;
            JS_STATIC_ASSERT((JS_BITS_PER_WORD & unrollMask) == 0);

            for (unsigned n = 0; n != JS_BITS_PER_WORD; n += unroll) {
                jsbitmap bitsChunk = bits & unrollMask;
                bits >>= unroll;
                if (bitsChunk == unrollMask) {
                    thing += unroll * sizeof(jsdouble);
                } else {
#define DO_BIT(bit)                                                           \
                    if (!(bitsChunk & (jsbitmap(1) << (bit)))) {              \
                        JS_ASSERT(thing - a->toPageStart() <=                 \
                                  (DOUBLES_PER_ARENA - 1) * sizeof(jsdouble));\
                        JSGCThing *t = reinterpret_cast<JSGCThing *>(thing);  \
                        *tailp = t;                                           \
                        tailp = &t->link;                                     \
                    }                                                         \
                    thing += sizeof(jsdouble);
                    DO_BIT(0);
                    DO_BIT(1);
                    DO_BIT(2);
                    DO_BIT(3);
#undef DO_BIT
                }
            }
        }
    }
    *tailp = NULL;
    return head;
}

static JSGCThing *
RefillDoubleFreeList(JSContext *cx)
{
    JS_ASSERT(!GetGCFreeLists(cx)->doubles);

    JSRuntime *rt = cx->runtime;
    JS_ASSERT(!rt->gcRunning);

    JS_LOCK_GC(rt);

    JSGCArena *a;
    bool canGC = !JS_ON_TRACE(cx) && !JS_THREAD_DATA(cx)->waiveGCQuota;
    bool doGC = canGC && IsGCThresholdReached(rt);
    for (;;) {
        if (doGC) {
            js_GC(cx, GC_LAST_DITCH);
            METER(rt->gcStats.doubleArenaStats.retry++);
            canGC = false;

            /* See comments in RefillFinalizableFreeList. */
            JSGCThing *freeList = GetGCFreeLists(cx)->doubles;
            if (freeList) {
                JS_UNLOCK_GC(rt);
                return freeList;
            }
        }

        /*
         * Loop until we find arena with some free doubles. We turn arenas
         * into free lists outside the lock to minimize contention between
         * threads.
         */
        while (!!(a = rt->gcDoubleArenaList.cursor)) {
            rt->gcDoubleArenaList.cursor = a->info.prev;
            JS_UNLOCK_GC(rt);
            JSGCThing *list = TurnUsedArenaIntoDoubleList(a);
            if (list)
                return list;
            JS_LOCK_GC(rt);
        }
        a = NewGCArena(cx);
        if (a)
            break;
        if (!canGC) {
            METER(rt->gcStats.doubleArenaStats.fail++);
            JS_UNLOCK_GC(rt);
            return NULL;
        }
        doGC = true;
    }

    a->info.list = NULL;
    a->info.freeList = NULL;
    a->info.prev = rt->gcDoubleArenaList.head;
    rt->gcDoubleArenaList.head = a;
    JS_UNLOCK_GC(rt);

    a->info.hasMarkedDoubles = false;
    return MakeNewArenaFreeList(a, sizeof(jsdouble));
}

JSBool
js_NewDoubleInRootedValue(JSContext *cx, jsdouble d, jsval *vp)
{
    /* Updates of metering counters here are not thread-safe. */
    METER(cx->runtime->gcStats.doubleArenaStats.alloc++);

    JSGCThing **freeListp = &JS_THREAD_DATA(cx)->gcFreeLists.doubles;
    JSGCThing *thing = *freeListp;
    if (thing) {
        METER(cx->runtime->gcStats.doubleArenaStats.localalloc++);
        JS_ASSERT(!JS_THREAD_DATA(cx)->localRootStack);
        CheckGCFreeListLink(thing);
        *freeListp = thing->link;

        jsdouble *dp = reinterpret_cast<jsdouble *>(thing);
        *dp = d;
        *vp = DOUBLE_TO_JSVAL(dp);
        return true;
    }

    JSLocalRootStack *lrs = JS_THREAD_DATA(cx)->localRootStack;
    for (;;) {
        if (lrs) {
            freeListp = &lrs->gcFreeLists.doubles;
            thing = *freeListp;
            if (thing) {
                METER(cx->runtime->gcStats.doubleArenaStats.localalloc++);
                break;
            }
        }
        thing = RefillDoubleFreeList(cx);
        if (thing) {
            JS_ASSERT(!*freeListp || *freeListp == thing);
            break;
        }

        if (!JS_ON_TRACE(cx)) {
            /* Trace code handle this on its own. */
            js_ReportOutOfMemory(cx);
            METER(cx->runtime->gcStats.doubleArenaStats.fail++);
        }
        return false;
    }

    CheckGCFreeListLink(thing);
    *freeListp = thing->link;

    jsdouble *dp = reinterpret_cast<jsdouble *>(thing);
    *dp = d;
    *vp = DOUBLE_TO_JSVAL(dp);
    return !lrs || js_PushLocalRoot(cx, lrs, *vp) >= 0;
}

jsdouble *
js_NewWeaklyRootedDouble(JSContext *cx, jsdouble d)
{
    jsval v;
    if (!js_NewDoubleInRootedValue(cx, d, &v))
        return NULL;

    jsdouble *dp = JSVAL_TO_DOUBLE(v);
    cx->weakRoots.newbornDouble = dp;
    return dp;
}

JSBool
js_LockGCThingRT(JSRuntime *rt, void *thing)
{
    if (!thing)
        return true;

    JS_LOCK_GC(rt);
    JSGCLockHashEntry *lhe = (JSGCLockHashEntry *)
                             JS_DHashTableOperate(&rt->gcLocksHash, thing,
                                                  JS_DHASH_ADD);
    bool ok = !!lhe;
    if (ok) {
        if (!lhe->thing) {
            lhe->thing = thing;
            lhe->count = 1;
        } else {
            JS_ASSERT(lhe->count >= 1);
            lhe->count++;
        }
        METER(rt->gcStats.lock++);
    }
    JS_UNLOCK_GC(rt);
    return ok;
}

void
js_UnlockGCThingRT(JSRuntime *rt, void *thing)
{
    if (!thing)
        return;

    JS_LOCK_GC(rt);
    JSGCLockHashEntry *lhe = (JSGCLockHashEntry *)
                             JS_DHashTableOperate(&rt->gcLocksHash, thing,
                                                  JS_DHASH_LOOKUP);
    if (JS_DHASH_ENTRY_IS_BUSY(lhe)) {
        rt->gcPoke = true;
        if (--lhe->count == 0)
            JS_DHashTableOperate(&rt->gcLocksHash, thing, JS_DHASH_REMOVE);
        METER(rt->gcStats.unlock++);
    }
    JS_UNLOCK_GC(rt);
}

JS_PUBLIC_API(void)
JS_TraceChildren(JSTracer *trc, void *thing, uint32 kind)
{
    switch (kind) {
      case JSTRACE_OBJECT: {
        /* If obj has no map, it must be a newborn. */
        JSObject *obj = (JSObject *) thing;
        if (!obj->map)
            break;
        obj->map->ops->trace(trc, obj);
        break;
      }

      case JSTRACE_STRING: {
        JSString *str = (JSString *) thing;
        if (str->isDependent())
            JS_CALL_STRING_TRACER(trc, str->dependentBase(), "base");
        break;
      }

#if JS_HAS_XML_SUPPORT
      case JSTRACE_XML:
        js_TraceXML(trc, (JSXML *)thing);
        break;
#endif
    }
}

/*
 * When the native stack is low, the GC does not call JS_TraceChildren to mark
 * the reachable "children" of the thing. Rather the thing is put aside and
 * JS_TraceChildren is called later with more space on the C stack.
 *
 * To implement such delayed marking of the children with minimal overhead for
 * the normal case of sufficient native stack, the code adds two fields to
 * JSGCArenaInfo. The first field, JSGCArenaInfo::prevUnmarkedPage, links all
 * arenas with delayed things into a stack list with the pointer to stack top
 * in JSRuntime::gcUnmarkedArenaStackTop. DelayMarkingChildren adds arenas to
 * the stack as necessary while MarkDelayedChildren pops the arenas from the
 * stack until it empties.
 *
 * The second field, JSGCArenaInfo::unmarkedChildren, is a bitmap that tells
 * for which things the GC should call JS_TraceChildren later. The bitmap is
 * a single word. As such it does not pinpoint the delayed things in the arena
 * but rather tells the intervals containing ThingsPerUnmarkedBit(thingSize)
 * things. Later the code in MarkDelayedChildren discovers such intervals
 * and calls JS_TraceChildren on any marked thing in the interval. This
 * implies that JS_TraceChildren can be called many times for a single thing
 * if the thing shares the same interval with some delayed things. This should
 * be fine as any GC graph marking/traversing hooks must allow repeated calls
 * during the same GC cycle. In particular, xpcom cycle collector relies on
 * this.
 *
 * Note that such repeated scanning may slow down the GC. In particular, it is
 * possible to construct an object graph where the GC calls JS_TraceChildren
 * ThingsPerUnmarkedBit(thingSize) for almost all things in the graph. We
 * tolerate this as the max value for ThingsPerUnmarkedBit(thingSize) is 4.
 * This is archived for JSObject on 32 bit system as it is exactly JSObject
 * that has the smallest size among the GC things that can be delayed. On 32
 * bit CPU we have less than 128 objects per 4K GC arena so each bit in
 * unmarkedChildren covers 4 objects.
 */
inline unsigned
ThingsPerUnmarkedBit(unsigned thingSize)
{
    return JS_HOWMANY(ThingsPerArena(thingSize), JS_BITS_PER_WORD);
}

static void
DelayMarkingChildren(JSRuntime *rt, void *thing)
{
    METER(rt->gcStats.unmarked++);
    JSGCArena *a = JSGCArena::fromGCThing(thing);
    JS_ASSERT(IsMarkedGCThing(a, thing));

    size_t thingIndex = ThingToOffset(thing) / a->info.list->thingSize;
    size_t unmarkedBitIndex = thingIndex /
                              ThingsPerUnmarkedBit(a->info.list->thingSize);
    JS_ASSERT(unmarkedBitIndex < JS_BITS_PER_WORD);

    jsuword bit = jsuword(1) << unmarkedBitIndex;
    if (a->info.unmarkedChildren != 0) {
        JS_ASSERT(rt->gcUnmarkedArenaStackTop);
        if (a->info.unmarkedChildren & bit) {
            /* bit already covers things with children to mark later. */
            return;
        }
        a->info.unmarkedChildren |= bit;
    } else {
        /*
         * The thing is the first thing with not yet marked children in the
         * whole arena, so push the arena on the stack of arenas with things
         * to be marked later unless the arena has already been pushed. We
         * detect that through checking prevUnmarkedPage as the field is 0
         * only for not yet pushed arenas. To ensure that
         *   prevUnmarkedPage != 0
         * even when the stack contains one element, we make prevUnmarkedPage
         * for the arena at the bottom to point to itself.
         *
         * See comments in MarkDelayedChildren.
         */
        a->info.unmarkedChildren = bit;
        if (!a->hasPrevUnmarked()) {
            if (!rt->gcUnmarkedArenaStackTop) {
                /* Stack was empty, mark the arena as the bottom element. */
                a->setPrevUnmarked(a);
            } else {
                JS_ASSERT(rt->gcUnmarkedArenaStackTop->hasPrevUnmarked());
                a->setPrevUnmarked(rt->gcUnmarkedArenaStackTop);
            }
            rt->gcUnmarkedArenaStackTop = a;
        }
        JS_ASSERT(rt->gcUnmarkedArenaStackTop);
    }
#ifdef DEBUG
    rt->gcMarkLaterCount += ThingsPerUnmarkedBit(a->info.list->thingSize);
    METER_UPDATE_MAX(rt->gcStats.maxunmarked, rt->gcMarkLaterCount);
#endif
}

static void
MarkDelayedChildren(JSTracer *trc)
{
    JSRuntime *rt;
    JSGCArena *a, *aprev;
    unsigned thingSize, traceKind;
    unsigned thingsPerUnmarkedBit;
    unsigned unmarkedBitIndex, thingIndex, indexLimit, endIndex;
    JSGCThing *thing;

    rt = trc->context->runtime;
    a = rt->gcUnmarkedArenaStackTop;
    if (!a) {
        JS_ASSERT(rt->gcMarkLaterCount == 0);
        return;
    }

    for (;;) {
        /*
         * The following assert verifies that the current arena belongs to the
         * unmarked stack, since DelayMarkingChildren ensures that even for
         * the stack's bottom, prevUnmarkedPage != 0 but rather points to
         * itself.
         */
        JS_ASSERT(a->hasPrevUnmarked());
        JS_ASSERT(rt->gcUnmarkedArenaStackTop->hasPrevUnmarked());
        thingSize = a->info.list->thingSize;
        traceKind = GetFinalizableArenaTraceKind(a);
        indexLimit = ThingsPerArena(thingSize);
        thingsPerUnmarkedBit = ThingsPerUnmarkedBit(thingSize);

        /*
         * We cannot use do-while loop here as a->unmarkedChildren can be zero
         * before the loop as a leftover from the previous iterations. See
         * comments after the loop.
         */
        while (a->info.unmarkedChildren != 0) {
            unmarkedBitIndex = JS_FLOOR_LOG2W(a->info.unmarkedChildren);
            a->info.unmarkedChildren &= ~((jsuword)1 << unmarkedBitIndex);
#ifdef DEBUG
            JS_ASSERT(rt->gcMarkLaterCount >= thingsPerUnmarkedBit);
            rt->gcMarkLaterCount -= thingsPerUnmarkedBit;
#endif
            thingIndex = unmarkedBitIndex * thingsPerUnmarkedBit;
            endIndex = thingIndex + thingsPerUnmarkedBit;

            /*
             * endIndex can go beyond the last allocated thing as the real
             * limit can be "inside" the bit.
             */
            if (endIndex > indexLimit)
                endIndex = indexLimit;
            JS_ASSERT(thingIndex < indexLimit);
            unsigned thingOffset = thingIndex * thingSize;
            unsigned endOffset = endIndex * thingSize;
            do {
                if (IsMarkedGCThing(a, thingOffset)) {
                    thing = OffsetToThing(a, thingOffset);
                    JS_TraceChildren(trc, thing, traceKind);
                }
                thingOffset += thingSize;
            } while (thingOffset != endOffset);
        }

        /*
         * We finished tracing of all things in the the arena but we can only
         * pop it from the stack if the arena is the stack's top.
         *
         * When JS_TraceChildren from the above calls JS_CallTracer that in
         * turn on low C stack calls DelayMarkingChildren and the latter
         * pushes new arenas to the unmarked stack, we have to skip popping
         * of this arena until it becomes the top of the stack again.
         */
        if (a == rt->gcUnmarkedArenaStackTop) {
            aprev = a->getPrevUnmarked();
            a->clearPrevUnmarked();
            if (a == aprev) {
                /*
                 * prevUnmarkedPage points to itself and we reached the
                 * bottom of the stack.
                 */
                break;
            }
            rt->gcUnmarkedArenaStackTop = a = aprev;
        } else {
            a = rt->gcUnmarkedArenaStackTop;
        }
    }
    JS_ASSERT(rt->gcUnmarkedArenaStackTop);
    JS_ASSERT(!rt->gcUnmarkedArenaStackTop->hasPrevUnmarked());
    rt->gcUnmarkedArenaStackTop = NULL;
    JS_ASSERT(rt->gcMarkLaterCount == 0);
}

void
js_CallGCMarker(JSTracer *trc, void *thing, uint32 kind)
{
    JSContext *cx;
    JSRuntime *rt;
    JSGCArena *a;

    JS_ASSERT(thing);
    JS_ASSERT(JS_IS_VALID_TRACE_KIND(kind));
    JS_ASSERT(trc->debugPrinter || trc->debugPrintArg);

    if (!IS_GC_MARKING_TRACER(trc)) {
        trc->callback(trc, thing, kind);
        goto out;
    }

    cx = trc->context;
    rt = cx->runtime;
    JS_ASSERT(rt->gcMarkingTracer == trc);
    JS_ASSERT(rt->gcLevel > 0);

    /*
     * Optimize for string and double as their size is known and their tracing
     * is not recursive.
     */
    switch (kind) {
      case JSTRACE_DOUBLE:
        a = JSGCArena::fromGCThing(thing);
        JS_ASSERT(!a->info.list);
        if (!a->info.hasMarkedDoubles) {
            a->info.hasMarkedDoubles = true;
            a->clearMarkBitmap();
        }
        MarkIfUnmarkedGCThing(a, thing);
        goto out;

      case JSTRACE_STRING:
        for (;;) {
            if (JSString::isStatic(thing))
                goto out;
            a = JSGCArena::fromGCThing(thing);
            JS_ASSERT(kind == GetFinalizableArenaTraceKind(a));
            if (!MarkIfUnmarkedGCThing(a, thing))
                goto out;
            if (!((JSString *) thing)->isDependent())
                goto out;
            thing = ((JSString *) thing)->dependentBase();
        }
        /* NOTREACHED */
    }

    a = JSGCArena::fromGCThing(thing);
    JS_ASSERT(kind == GetFinalizableArenaTraceKind(a));
    if (!MarkIfUnmarkedGCThing(a, thing))
        goto out;

    if (!cx->insideGCMarkCallback) {
        /*
         * With JS_GC_ASSUME_LOW_C_STACK defined the mark phase of GC always
         * uses the non-recursive code that otherwise would be called only on
         * a low C stack condition.
         */
#ifdef JS_GC_ASSUME_LOW_C_STACK
# define RECURSION_TOO_DEEP() JS_TRUE
#else
        int stackDummy;
# define RECURSION_TOO_DEEP() (!JS_CHECK_STACK_SIZE(cx, stackDummy))
#endif
        if (RECURSION_TOO_DEEP())
            DelayMarkingChildren(rt, thing);
        else
            JS_TraceChildren(trc, thing, kind);
    } else {
        /*
         * For API compatibility we allow for the callback to assume that
         * after it calls JS_MarkGCThing for the last time, the callback can
         * start to finalize its own objects that are only referenced by
         * unmarked GC things.
         *
         * Since we do not know which call from inside the callback is the
         * last, we ensure that children of all marked things are traced and
         * call MarkDelayedChildren(trc) after tracing the thing.
         *
         * As MarkDelayedChildren unconditionally invokes JS_TraceChildren
         * for the things with unmarked children, calling DelayMarkingChildren
         * is useless here. Hence we always trace thing's children even with a
         * low native stack.
         */
        cx->insideGCMarkCallback = false;
        JS_TraceChildren(trc, thing, kind);
        MarkDelayedChildren(trc);
        cx->insideGCMarkCallback = true;
    }

  out:
#ifdef DEBUG
    trc->debugPrinter = NULL;
    trc->debugPrintArg = NULL;
#endif
    return;     /* to avoid out: right_curl when DEBUG is not defined */
}

void
js_CallValueTracerIfGCThing(JSTracer *trc, jsval v)
{
    void *thing;
    uint32 kind;

    if (JSVAL_IS_DOUBLE(v) || JSVAL_IS_STRING(v)) {
        thing = JSVAL_TO_TRACEABLE(v);
        kind = JSVAL_TRACE_KIND(v);
        JS_ASSERT(kind == js_GetGCThingTraceKind(thing));
    } else if (JSVAL_IS_OBJECT(v) && v != JSVAL_NULL) {
        /* v can be an arbitrary GC thing reinterpreted as an object. */
        thing = JSVAL_TO_OBJECT(v);
        kind = js_GetGCThingTraceKind(thing);
    } else {
        return;
    }
    js_CallGCMarker(trc, thing, kind);
}

static JSDHashOperator
gc_root_traversal(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 num,
                  void *arg)
{
    JSGCRootHashEntry *rhe = (JSGCRootHashEntry *)hdr;
    JSTracer *trc = (JSTracer *)arg;
    jsval *rp = (jsval *)rhe->root;
    jsval v = *rp;

    /* Ignore null reference, scalar values, and static strings. */
    if (JSVAL_IS_TRACEABLE(v)) {
#ifdef DEBUG
        if (!JSString::isStatic(JSVAL_TO_GCTHING(v))) {
            bool root_points_to_gcArenaList = false;
            jsuword thing = (jsuword) JSVAL_TO_GCTHING(v);
            JSRuntime *rt = trc->context->runtime;
            for (unsigned i = 0; i != FINALIZE_LIMIT; i++) {
                JSGCArenaList *arenaList = &rt->gcArenaList[i];
                size_t thingSize = arenaList->thingSize;
                size_t limit = ThingsPerArena(thingSize) * thingSize;
                for (JSGCArena *a = arenaList->head; a; a = a->info.prev) {
                    if (thing - a->toPageStart() < limit) {
                        root_points_to_gcArenaList = true;
                        break;
                    }
                }
            }
            if (!root_points_to_gcArenaList) {
                for (JSGCArena *a = rt->gcDoubleArenaList.head;
                     a;
                     a = a->info.prev) {
                    if (thing - a->toPageStart() <
                        DOUBLES_PER_ARENA * sizeof(jsdouble)) {
                        root_points_to_gcArenaList = true;
                        break;
                    }
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
        }
#endif
        JS_SET_TRACING_NAME(trc, rhe->name ? rhe->name : "root");
        js_CallValueTracerIfGCThing(trc, v);
    }

    return JS_DHASH_NEXT;
}

static JSDHashOperator
gc_lock_traversal(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 num,
                  void *arg)
{
    JSGCLockHashEntry *lhe = (JSGCLockHashEntry *)hdr;
    void *thing = (void *)lhe->thing;
    JSTracer *trc = (JSTracer *)arg;
    uint32 traceKind;

    JS_ASSERT(lhe->count >= 1);
    traceKind = js_GetGCThingTraceKind(thing);
    JS_CALL_TRACER(trc, thing, traceKind, "locked object");
    return JS_DHASH_NEXT;
}

namespace js {

void
TraceObjectVector(JSTracer *trc, JSObject **vec, uint32 len)
{
    for (uint32 i = 0; i < len; i++) {
        if (JSObject *obj = vec[i]) {
            JS_SET_TRACING_INDEX(trc, "vector", i);
            js_CallGCMarker(trc, obj, JSTRACE_OBJECT);
        }
    }
}

}

void
js_TraceStackFrame(JSTracer *trc, JSStackFrame *fp)
{
    uintN nslots, minargs, skip;

    if (fp->callobj)
        JS_CALL_OBJECT_TRACER(trc, fp->callobj, "call");
    if (fp->argsobj)
        JS_CALL_OBJECT_TRACER(trc, JSVAL_TO_OBJECT(fp->argsobj), "arguments");
    if (fp->script) {
        js_TraceScript(trc, fp->script);

        /* fp->slots is null for watch pseudo-frames, see js_watch_set. */
        if (fp->slots) {
            /*
             * Don't mark what has not been pushed yet, or what has been
             * popped already.
             */
            if (fp->regs && fp->regs->sp) {
                nslots = (uintN) (fp->regs->sp - fp->slots);
                JS_ASSERT(nslots >= fp->script->nfixed);
            } else {
                nslots = fp->script->nfixed;
            }
            TraceValues(trc, nslots, fp->slots, "slot");
        }
    } else {
        JS_ASSERT(!fp->slots);
        JS_ASSERT(!fp->regs);
    }

    /* Allow for primitive this parameter due to JSFUN_THISP_* flags. */
    JS_CALL_VALUE_TRACER(trc, fp->thisv, "this");

    if (fp->argv) {
        JS_CALL_VALUE_TRACER(trc, fp->calleeValue(), "callee");
        nslots = fp->argc;
        skip = 0;
        if (fp->fun) {
            minargs = FUN_MINARGS(fp->fun);
            if (minargs > nslots)
                nslots = minargs;
            if (!FUN_INTERPRETED(fp->fun)) {
                JS_ASSERT(!(fp->fun->flags & JSFUN_FAST_NATIVE));
                nslots += fp->fun->u.n.extra;
            }
            if (fp->fun->flags & JSFRAME_ROOTED_ARGV)
                skip = 2 + fp->argc;
        }
        TraceValues(trc, 2 + nslots - skip, fp->argv - 2 + skip, "operand");
    }

    JS_CALL_VALUE_TRACER(trc, fp->rval, "rval");
    if (fp->scopeChain)
        JS_CALL_OBJECT_TRACER(trc, fp->scopeChain, "scope chain");
}

void
JSWeakRoots::mark(JSTracer *trc)
{
#ifdef DEBUG
    const char * const newbornNames[] = {
        "newborn_object",             /* FINALIZE_OBJECT */
        "newborn_function",           /* FINALIZE_FUNCTION */
#if JS_HAS_XML_SUPPORT
        "newborn_xml",                /* FINALIZE_XML */
#endif
        "newborn_string",             /* FINALIZE_STRING */
        "newborn_external_string0",   /* FINALIZE_EXTERNAL_STRING0 */
        "newborn_external_string1",   /* FINALIZE_EXTERNAL_STRING1 */
        "newborn_external_string2",   /* FINALIZE_EXTERNAL_STRING2 */
        "newborn_external_string3",   /* FINALIZE_EXTERNAL_STRING3 */
        "newborn_external_string4",   /* FINALIZE_EXTERNAL_STRING4 */
        "newborn_external_string5",   /* FINALIZE_EXTERNAL_STRING5 */
        "newborn_external_string6",   /* FINALIZE_EXTERNAL_STRING6 */
        "newborn_external_string7",   /* FINALIZE_EXTERNAL_STRING7 */
    };
#endif
    for (size_t i = 0; i != JS_ARRAY_LENGTH(finalizableNewborns); ++i) {
        void *newborn = finalizableNewborns[i];
        if (newborn) {
            JS_CALL_TRACER(trc, newborn, GetFinalizableTraceKind(i),
                           newbornNames[i]);
        }
    }
    if (newbornDouble)
        JS_CALL_DOUBLE_TRACER(trc, newbornDouble, "newborn_double");
    JS_CALL_VALUE_TRACER(trc, lastAtom, "lastAtom");
    JS_SET_TRACING_NAME(trc, "lastInternalResult");
    js_CallValueTracerIfGCThing(trc, lastInternalResult);
}

static void inline
TraceFrameChain(JSTracer *trc, JSStackFrame *fp)
{
    do {
        js_TraceStackFrame(trc, fp);
    } while ((fp = fp->down) != NULL);
}

JS_REQUIRES_STACK JS_FRIEND_API(void)
js_TraceContext(JSTracer *trc, JSContext *acx)
{
    JSStackHeader *sh;

    /*
     * Trace active and suspended callstacks.
     *
     * Since js_GetTopStackFrame needs to dereference cx->thread to check for
     * JIT frames, we check for non-null thread here and avoid null checks
     * there. See bug 471197.
     */
#ifdef JS_THREADSAFE
    if (acx->thread)
#endif
    {
        /* If |cx->fp|, the active callstack has newest (top) frame |cx->fp|. */
        JSStackFrame *fp = js_GetTopStackFrame(acx);
        if (fp) {
            JS_ASSERT(!acx->activeCallStack()->isSuspended());
            TraceFrameChain(trc, fp);
            if (JSObject *o = acx->activeCallStack()->getInitialVarObj())
                JS_CALL_OBJECT_TRACER(trc, o, "variables");
        }

        /* Trace suspended frames. */
        CallStack *cur = acx->currentCallStack;
        CallStack *cs = fp ? cur->getPrevious() : cur;
        for (; cs; cs = cs->getPrevious()) {
            TraceFrameChain(trc, cs->getSuspendedFrame());
            if (cs->getInitialVarObj())
                JS_CALL_OBJECT_TRACER(trc, cs->getInitialVarObj(), "var env");
        }
    }

    /* Mark other roots-by-definition in acx. */
    if (acx->globalObject && !JS_HAS_OPTION(acx, JSOPTION_UNROOTED_GLOBAL))
        JS_CALL_OBJECT_TRACER(trc, acx->globalObject, "global object");
    acx->weakRoots.mark(trc);
    if (acx->throwing) {
        JS_CALL_VALUE_TRACER(trc, acx->exception, "exception");
    } else {
        /* Avoid keeping GC-ed junk stored in JSContext.exception. */
        acx->exception = JSVAL_NULL;
    }

    for (sh = acx->stackHeaders; sh; sh = sh->down) {
        METER(trc->context->runtime->gcStats.stackseg++);
        METER(trc->context->runtime->gcStats.segslots += sh->nslots);
        TraceValues(trc, sh->nslots, JS_STACK_SEGMENT(sh), "stack");
    }

    for (js::AutoGCRooter *gcr = acx->autoGCRooters; gcr; gcr = gcr->down)
        gcr->trace(trc);

    if (acx->sharpObjectMap.depth > 0)
        js_TraceSharpMap(trc, &acx->sharpObjectMap);

    js_TraceRegExpStatics(trc, acx);

#ifdef JS_TRACER
    InterpState* state = acx->interpState;
    while (state) {
        if (state->nativeVp)
            TraceValues(trc, state->nativeVpLen, state->nativeVp, "nativeVp");
        state = state->prev;
    }
#endif
}

JS_REQUIRES_STACK void
js_TraceRuntime(JSTracer *trc, JSBool allAtoms)
{
    JSRuntime *rt = trc->context->runtime;
    JSContext *iter, *acx;

    JS_DHashTableEnumerate(&rt->gcRootsHash, gc_root_traversal, trc);
    JS_DHashTableEnumerate(&rt->gcLocksHash, gc_lock_traversal, trc);
    js_TraceAtomState(trc, allAtoms);
    js_TraceRuntimeNumberState(trc);
    js_MarkTraps(trc);

    iter = NULL;
    while ((acx = js_ContextIterator(rt, JS_TRUE, &iter)) != NULL)
        js_TraceContext(trc, acx);

    js_TraceThreads(rt, trc);

    if (rt->gcExtraRootsTraceOp)
        rt->gcExtraRootsTraceOp(trc, rt->gcExtraRootsData);

#ifdef JS_TRACER
    for (int i = 0; i < JSBUILTIN_LIMIT; i++) {
        if (rt->builtinFunctions[i])
            JS_CALL_OBJECT_TRACER(trc, rt->builtinFunctions[i], "builtin function");
    }
#endif
}

void
js_TriggerGC(JSContext *cx, JSBool gcLocked)
{
    JSRuntime *rt = cx->runtime;

#ifdef JS_THREADSAFE
    JS_ASSERT(cx->requestDepth > 0);
#endif
    JS_ASSERT(!rt->gcRunning);
    if (rt->gcIsNeeded)
        return;

    js_TriggerAllOperationCallbacks(rt, gcLocked);
}

static void
ProcessSetSlotRequest(JSContext *cx, JSSetSlotRequest *ssr)
{
    JSObject *obj = ssr->obj;
    JSObject *pobj = ssr->pobj;
    uint32 slot = ssr->slot;

    while (pobj) {
        pobj = js_GetWrappedObject(cx, pobj);
        if (pobj == obj) {
            ssr->cycle = true;
            return;
        }
        pobj = JSVAL_TO_OBJECT(pobj->getSlot(slot));
    }

    pobj = ssr->pobj;
    if (slot == JSSLOT_PROTO) {
        obj->setProto(pobj);
    } else {
        JS_ASSERT(slot == JSSLOT_PARENT);
        obj->setParent(pobj);
    }
}

void
js_DestroyScriptsToGC(JSContext *cx, JSThreadData *data)
{
    JSScript **listp, *script;

    for (size_t i = 0; i != JS_ARRAY_LENGTH(data->scriptsToGC); ++i) {
        listp = &data->scriptsToGC[i];
        while ((script = *listp) != NULL) {
            *listp = script->u.nextToGC;
            script->u.nextToGC = NULL;
            js_DestroyScript(cx, script);
        }
    }
}

inline void
FinalizeObject(JSContext *cx, JSObject *obj, unsigned thingKind)
{
    JS_ASSERT(thingKind == FINALIZE_FUNCTION || thingKind == FINALIZE_OBJECT);

    /* Cope with stillborn objects that have no map. */
    if (!obj->map)
        return;

    /* Finalize obj first, in case it needs map and slots. */
    JSClass *clasp = obj->getClass();
    if (clasp->finalize)
        clasp->finalize(cx, obj);

#ifdef INCLUDE_MOZILLA_DTRACE
    if (JAVASCRIPT_OBJECT_FINALIZE_ENABLED())
        jsdtrace_object_finalize(obj);
#endif

    if (JS_LIKELY(obj->isNative())) {
        JSScope *scope = OBJ_SCOPE(obj);
        if (scope->isSharedEmpty())
            static_cast<JSEmptyScope *>(scope)->dropFromGC(cx);
        else
            scope->destroy(cx);
    }
    if (obj->hasSlotsArray())
        obj->freeSlotsArray(cx);
}

inline void
FinalizeFunction(JSContext *cx, JSFunction *fun, unsigned thingKind)
{
    FinalizeObject(cx, FUN_OBJECT(fun), thingKind);
}

inline void
FinalizeHookedObject(JSContext *cx, JSObject *obj, unsigned thingKind)
{
    if (!obj->map)
        return;

    if (cx->debugHooks->objectHook) {
        cx->debugHooks->objectHook(cx, obj, JS_FALSE,
                                   cx->debugHooks->objectHookData);
    }
    FinalizeObject(cx, obj, thingKind);
}

inline void
FinalizeHookedFunction(JSContext *cx, JSFunction *fun, unsigned thingKind)
{
    FinalizeHookedObject(cx, FUN_OBJECT(fun), thingKind);
}

#if JS_HAS_XML_SUPPORT
inline void
FinalizeXML(JSContext *cx, JSXML *xml, unsigned thingKind)
{
    js_FinalizeXML(cx, xml);
}
#endif

JS_STATIC_ASSERT(JS_EXTERNAL_STRING_LIMIT == 8);
static JSStringFinalizeOp str_finalizers[JS_EXTERNAL_STRING_LIMIT] = {
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

intN
js_ChangeExternalStringFinalizer(JSStringFinalizeOp oldop,
                                 JSStringFinalizeOp newop)
{
    for (uintN i = 0; i != JS_ARRAY_LENGTH(str_finalizers); i++) {
        if (str_finalizers[i] == oldop) {
            str_finalizers[i] = newop;
            return intN(i);
        }
    }
    return -1;
}

inline void
FinalizeString(JSContext *cx, JSString *str, unsigned thingKind)
{
    JS_ASSERT(FINALIZE_STRING == thingKind);
    JS_ASSERT(!JSString::isStatic(str));
    JS_RUNTIME_UNMETER(cx->runtime, liveStrings);
    if (str->isDependent()) {
        JS_ASSERT(str->dependentBase());
        JS_RUNTIME_UNMETER(cx->runtime, liveDependentStrings);
    } else {
        /*
         * flatChars for stillborn string is null, but cx->free would checks
         * for a null pointer on its own.
         */
        cx->free(str->flatChars());
    }
}

inline void
FinalizeExternalString(JSContext *cx, JSString *str, unsigned thingKind)
{
    unsigned type = thingKind - FINALIZE_EXTERNAL_STRING0;
    JS_ASSERT(type < JS_ARRAY_LENGTH(str_finalizers));
    JS_ASSERT(!JSString::isStatic(str));
    JS_ASSERT(!str->isDependent());

    JS_RUNTIME_UNMETER(cx->runtime, liveStrings);

    /* A stillborn string has null chars. */
    jschar *chars = str->flatChars();
    if (!chars)
        return;
    JSStringFinalizeOp finalizer = str_finalizers[type];
    if (finalizer)
        finalizer(cx, str);
}

/*
 * This function is called from js_FinishAtomState to force the finalization
 * of the permanently interned strings when cx is not available.
 */
void
js_FinalizeStringRT(JSRuntime *rt, JSString *str)
{
    JS_RUNTIME_UNMETER(rt, liveStrings);
    JS_ASSERT(!JSString::isStatic(str));

    if (str->isDependent()) {
        /* A dependent string can not be external and must be valid. */
        JS_ASSERT(JSGCArena::fromGCThing(str)->info.list->thingKind ==
                  FINALIZE_STRING);
        JS_ASSERT(str->dependentBase());
        JS_RUNTIME_UNMETER(rt, liveDependentStrings);
    } else {
        unsigned thingKind = JSGCArena::fromGCThing(str)->info.list->thingKind;
        JS_ASSERT(IsFinalizableStringKind(thingKind));

        /* A stillborn string has null chars, so is not valid. */
        jschar *chars = str->flatChars();
        if (!chars)
            return;
        if (thingKind == FINALIZE_STRING) {
            rt->free(chars);
        } else {
            unsigned type = thingKind - FINALIZE_EXTERNAL_STRING0;
            JS_ASSERT(type < JS_ARRAY_LENGTH(str_finalizers));
            JSStringFinalizeOp finalizer = str_finalizers[type];
            if (finalizer) {
                /*
                 * Assume that the finalizer for the permanently interned
                 * string knows how to deal with null context.
                 */
                finalizer(NULL, str);
            }
        }
    }
}

template<typename T,
         void finalizer(JSContext *cx, T *thing, unsigned thingKind)>
static void
FinalizeArenaList(JSContext *cx, unsigned thingKind, JSGCArena **emptyArenas)
{
    JS_STATIC_ASSERT(!(sizeof(T) & GC_CELL_MASK));
    JSGCArenaList *arenaList = &cx->runtime->gcArenaList[thingKind];
    JS_ASSERT(sizeof(T) == arenaList->thingSize);

    JSGCArena **ap = &arenaList->head;
    JSGCArena *a = *ap;
    if (!a)
        return;

#ifdef JS_GCMETER
    uint32 nlivearenas = 0, nkilledarenas = 0, nthings = 0;
#endif
    for (;;) {
        JS_ASSERT(a->info.list == arenaList);
        JS_ASSERT(!a->hasPrevUnmarked());
        JS_ASSERT(a->info.unmarkedChildren == 0);

        JSGCThing *freeList = NULL;
        JSGCThing **tailp = &freeList;
        bool allClear = true;

        JSGCThing *thing = reinterpret_cast<JSGCThing *>(a->toPageStart());
        jsuword endOffset =  GC_ARENA_CELLS_SIZE / sizeof(T) * sizeof(T);
        JSGCThing *thingsEnd = reinterpret_cast<JSGCThing *>(a->toPageStart() +
                                                             endOffset);

        JSGCThing *nextFree = a->info.freeList;
        if (!nextFree) {
            nextFree = thingsEnd;
        } else {
            JS_ASSERT(thing <= nextFree);
            JS_ASSERT(nextFree < thingsEnd);
        }

        jsuword gcCellIndex = 0;
        jsbitmap *bitmap = a->markBitmap;
        for (;; thing = NextThing(thing, sizeof(T)),
                gcCellIndex += sizeof(T) >> GC_CELL_SHIFT) {
            if (thing == nextFree) {
                if (thing == thingsEnd)
                    break;
                nextFree = nextFree->link;
                if (!nextFree) {
                    nextFree = thingsEnd;
                } else {
                    JS_ASSERT(thing < nextFree);
                    JS_ASSERT(nextFree < thingsEnd);
                }
            } else if (JS_TEST_BIT(bitmap, gcCellIndex)) {
                allClear = false;
                METER(nthings++);
                continue;
            } else {
                finalizer(cx, reinterpret_cast<T *>(thing), thingKind);
#ifdef DEBUG
                memset(thing, JS_FREE_PATTERN, sizeof(T));
#endif
            }
            *tailp = thing;
            tailp = &thing->link;
        }

#ifdef DEBUG
        /* Check that the free list is consistent. */
        unsigned nfree = 0;
        if (freeList) {
            JS_ASSERT(tailp != &freeList);
            JSGCThing *thing = freeList;
            for (;;) {
                ++nfree;
                if (&thing->link == tailp)
                    break;
                JS_ASSERT(thing < thing->link);
                thing = thing->link;
            }
        }
#endif
        if (allClear) {
            /*
             * Forget just assembled free list head for the arena and
             * add the arena itself to the destroy list.
             */
            JS_ASSERT(nfree == ThingsPerArena(sizeof(T)));
            *ap = a->info.prev;
            a->info.prev = *emptyArenas;
            *emptyArenas = a;
            METER(nkilledarenas++);
        } else {
            JS_ASSERT(nfree < ThingsPerArena(sizeof(T)));
            a->clearMarkBitmap();
            *tailp = NULL;
            a->info.freeList = freeList;
            ap = &a->info.prev;
            METER(nlivearenas++);
        }
        if (!(a = *ap))
            break;
    }
    arenaList->cursor = arenaList->head;

    METER(UpdateArenaStats(&cx->runtime->gcStats.arenaStats[thingKind],
                           nlivearenas, nkilledarenas, nthings));
}

#ifdef MOZ_GCTIMER
struct GCTimer {
    uint64 enter;
    uint64 startMark;
    uint64 startSweep;
    uint64 sweepObjectEnd;
    uint64 sweepStringEnd;
    uint64 sweepDoubleEnd;
    uint64 sweepDestroyEnd;
    uint64 end;
};

void dumpGCTimer(GCTimer *gcT, uint64 firstEnter, bool lastGC)
{
    static FILE *gcFile;

    if (!gcFile) {
        gcFile = fopen("gcTimer.dat", "w");
        JS_ASSERT(gcFile);
        
        fprintf(gcFile, "     AppTime,  Total,   Mark,  Sweep, FinObj, ");
        fprintf(gcFile, "FinStr, FinDbl, Destroy,  newChunks, destoyChunks\n");
    }
    fprintf(gcFile, "%12.1f, %6.1f, %6.1f, %6.1f, %6.1f, %6.1f, %6.1f, %7.1f, ",
            (double)(gcT->enter - firstEnter) / 1E6, 
            (double)(gcT->end-gcT->enter) / 1E6, 
            (double)(gcT->startSweep - gcT->startMark) / 1E6, 
            (double)(gcT->sweepDestroyEnd - gcT->startSweep) / 1E6, 
            (double)(gcT->sweepObjectEnd - gcT->startSweep) / 1E6, 
            (double)(gcT->sweepStringEnd - gcT->sweepObjectEnd) / 1E6,
            (double)(gcT->sweepDoubleEnd - gcT->sweepStringEnd) / 1E6,
            (double)(gcT->sweepDestroyEnd - gcT->sweepDoubleEnd) / 1E6);
    fprintf(gcFile, "%10d, %10d \n", newChunkCount, destroyChunkCount);
    fflush(gcFile);

    if (lastGC)
        fclose(gcFile);
}
#endif

/*
 * The gckind flag bit GC_LOCK_HELD indicates a call from js_NewGCThing with
 * rt->gcLock already held, so the lock should be kept on return.
 */
void
js_GC(JSContext *cx, JSGCInvocationKind gckind)
{
    JSRuntime *rt;
    JSGCCallback callback;
    JSTracer trc;
    JSGCArena *emptyArenas, *a, **ap;
#ifdef JS_THREADSAFE
    size_t requestDebit;
#endif
#ifdef JS_GCMETER
    uint32 nlivearenas, nkilledarenas, nthings;
#endif

    JS_ASSERT_IF(gckind == GC_LAST_DITCH, !JS_ON_TRACE(cx));
    rt = cx->runtime;

#ifdef JS_THREADSAFE
    /*
     * We allow js_GC calls outside a request but the context must be bound
     * to the current thread.
     */
    JS_ASSERT(CURRENT_THREAD_IS_ME(cx->thread));

    /* Avoid deadlock. */
    JS_ASSERT(!JS_IS_RUNTIME_LOCKED(rt));
#endif

    /*
     * Don't collect garbage if the runtime isn't up, and cx is not the last
     * context in the runtime.  The last context must force a GC, and nothing
     * should suppress that final collection or there may be shutdown leaks,
     * or runtime bloat until the next context is created.
     */
    if (rt->state != JSRTS_UP && gckind != GC_LAST_CONTEXT)
        return;
    
#ifdef MOZ_GCTIMER
    static uint64 firstEnter = rdtsc();
    GCTimer gcTimer;
    memset(&gcTimer, 0, sizeof(GCTimer));
# define TIMESTAMP(x) (x = rdtsc())
#else
# define TIMESTAMP(x) ((void) 0)
#endif
    TIMESTAMP(gcTimer.enter);

  restart_at_beginning:
    /*
     * Let the API user decide to defer a GC if it wants to (unless this
     * is the last context).  Invoke the callback regardless. Sample the
     * callback in case we are freely racing with a JS_SetGCCallback{,RT} on
     * another thread.
     */
    if (gckind != GC_SET_SLOT_REQUEST && (callback = rt->gcCallback)) {
        JSBool ok;

        if (gckind & GC_LOCK_HELD)
            JS_UNLOCK_GC(rt);
        ok = callback(cx, JSGC_BEGIN);
        if (gckind & GC_LOCK_HELD)
            JS_LOCK_GC(rt);
        if (!ok && gckind != GC_LAST_CONTEXT) {
            /*
             * It's possible that we've looped back to this code from the 'goto
             * restart_at_beginning' below in the GC_SET_SLOT_REQUEST code and
             * that rt->gcLevel is now 0. Don't return without notifying!
             */
            if (rt->gcLevel == 0 && (gckind & GC_LOCK_HELD))
                JS_NOTIFY_GC_DONE(rt);
            return;
        }
    }

    /* Lock out other GC allocator and collector invocations. */
    if (!(gckind & GC_LOCK_HELD))
        JS_LOCK_GC(rt);

    METER(rt->gcStats.poke++);
    rt->gcPoke = JS_FALSE;

#ifdef JS_THREADSAFE
    /*
     * Check if the GC is already running on this or another thread and
     * delegate the job to it.
     */
    if (rt->gcLevel > 0) {
        JS_ASSERT(rt->gcThread);

        /* Bump gcLevel to restart the current GC, so it finds new garbage. */
        rt->gcLevel++;
        METER_UPDATE_MAX(rt->gcStats.maxlevel, rt->gcLevel);

        /*
         * If the GC runs on another thread, temporarily suspend all requests
         * running on the current thread and wait until the GC is done.
         */
        if (rt->gcThread != cx->thread) {
            requestDebit = js_CountThreadRequests(cx);
            JS_ASSERT(requestDebit <= rt->requestCount);
#ifdef JS_TRACER
            JS_ASSERT_IF(requestDebit == 0, !JS_ON_TRACE(cx));
#endif
            if (requestDebit != 0) {
#ifdef JS_TRACER
                if (JS_ON_TRACE(cx)) {
                    /*
                     * Leave trace before we decrease rt->requestCount and
                     * notify the GC. Otherwise the GC may start immediately
                     * after we unlock while this thread is still on trace.
                     */
                    JS_UNLOCK_GC(rt);
                    LeaveTrace(cx);
                    JS_LOCK_GC(rt);
                }
#endif
                rt->requestCount -= requestDebit;
                if (rt->requestCount == 0)
                    JS_NOTIFY_REQUEST_DONE(rt);

                /*
                 * See comments before another call to js_ShareWaitingTitles
                 * below.
                 */
                cx->thread->gcWaiting = true;
                js_ShareWaitingTitles(cx);

                /*
                 * Make sure that the GC from another thread respects
                 * GC_KEEP_ATOMS.
                 */
                if (gckind & GC_KEEP_ATOMS)
                    JS_KEEP_ATOMS(rt);

                /*
                 * Check that we did not release the GC lock above and let the
                 * GC to finish before we wait.
                 */
                JS_ASSERT(rt->gcLevel > 0);
                do {
                    JS_AWAIT_GC_DONE(rt);
                } while (rt->gcLevel > 0);

                cx->thread->gcWaiting = false;
                if (gckind & GC_KEEP_ATOMS)
                    JS_UNKEEP_ATOMS(rt);
                rt->requestCount += requestDebit;
            }
        }
        if (!(gckind & GC_LOCK_HELD))
            JS_UNLOCK_GC(rt);
        return;
    }

    /* No other thread is in GC, so indicate that we're now in GC. */
    rt->gcLevel = 1;
    rt->gcThread = cx->thread;

    /*
     * Notify all operation callbacks, which will give them a chance to
     * yield their current request. Contexts that are not currently
     * executing will perform their callback at some later point,
     * which then will be unnecessary, but harmless.
     */
    js_NudgeOtherContexts(cx);

    /*
     * Discount all the requests on the current thread from contributing
     * to rt->requestCount before we wait for all other requests to finish.
     * JS_NOTIFY_REQUEST_DONE, which will wake us up, is only called on
     * rt->requestCount transitions to 0.
     */
    requestDebit = js_CountThreadRequests(cx);
    JS_ASSERT_IF(cx->requestDepth != 0, requestDebit >= 1);
    JS_ASSERT(requestDebit <= rt->requestCount);
    if (requestDebit != rt->requestCount) {
        rt->requestCount -= requestDebit;

        /*
         * Share any title that is owned by the GC thread before we wait, to
         * avoid a deadlock with ClaimTitle. We also set the gcWaiting flag so
         * that ClaimTitle can claim the title ownership from the GC thread if
         * that function is called while the GC is waiting.
         */
        cx->thread->gcWaiting = true;
        js_ShareWaitingTitles(cx);
        do {
            JS_AWAIT_REQUEST_DONE(rt);
        } while (rt->requestCount > 0);
        cx->thread->gcWaiting = false;
        rt->requestCount += requestDebit;
    }

#else  /* !JS_THREADSAFE */

    /* Bump gcLevel and return rather than nest; the outer gc will restart. */
    rt->gcLevel++;
    METER_UPDATE_MAX(rt->gcStats.maxlevel, rt->gcLevel);
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

    if (gckind == GC_SET_SLOT_REQUEST) {
        JSSetSlotRequest *ssr;

        while ((ssr = rt->setSlotRequests) != NULL) {
            rt->setSlotRequests = ssr->next;
            JS_UNLOCK_GC(rt);
            ssr->next = NULL;
            ProcessSetSlotRequest(cx, ssr);
            JS_LOCK_GC(rt);
        }

        /*
         * We assume here that killing links to parent and prototype objects
         * does not create garbage (such objects typically are long-lived and
         * widely shared, e.g. global objects, Function.prototype, etc.). We
         * collect garbage only if a racing thread attempted GC and is waiting
         * for us to finish (gcLevel > 1) or if someone already poked us.
         */
        if (rt->gcLevel == 1 && !rt->gcPoke && !rt->gcIsNeeded)
            goto done_running;

        rt->gcLevel = 0;
        rt->gcPoke = JS_FALSE;
        rt->gcRunning = JS_FALSE;
#ifdef JS_THREADSAFE
        rt->gcThread = NULL;
#endif
        gckind = GC_LOCK_HELD;
        goto restart_at_beginning;
    }

    JS_UNLOCK_GC(rt);

#ifdef JS_TRACER
    if (JS_ON_TRACE(cx))
        goto out;
#endif
    VOUCH_HAVE_STACK();

    /* Clear gcIsNeeded now, when we are about to start a normal GC cycle. */
    rt->gcIsNeeded = JS_FALSE;

    /* Reset malloc counter. */
    rt->resetGCMallocBytes();

#ifdef JS_DUMP_SCOPE_METERS
  { extern void js_DumpScopeMeters(JSRuntime *rt);
    js_DumpScopeMeters(rt);
  }
#endif

#ifdef JS_TRACER
    PurgeJITOracle();
#endif

    /*
     * Reset the property cache's type id generator so we can compress ids.
     * Same for the protoHazardShape proxy-shape standing in for all object
     * prototypes having readonly or setter properties.
     */
    if (rt->shapeGen & SHAPE_OVERFLOW_BIT
#ifdef JS_GC_ZEAL
        || rt->gcZeal >= 1
#endif
        ) {
        rt->gcRegenShapes = true;
        rt->gcRegenShapesScopeFlag ^= JSScope::SHAPE_REGEN;
        rt->shapeGen = 0;
        rt->protoHazardShape = 0;
    }

    js_PurgeThreads(cx);
    {
        JSContext *iter = NULL, *acx;
        while ((acx = js_ContextIterator(rt, JS_TRUE, &iter)) != NULL)
            acx->purge();
    }

#ifdef JS_TRACER
    if (gckind == GC_LAST_CONTEXT) {
        /* Clear builtin functions, which are recreated on demand. */
        PodArrayZero(rt->builtinFunctions);
    }
#endif

    /* The last ditch GC preserves weak roots. */
    if (!(gckind & GC_KEEP_ATOMS))
        JS_CLEAR_WEAK_ROOTS(&cx->weakRoots);

    TIMESTAMP(gcTimer.startMark);

  restart:
    rt->gcNumber++;
    JS_ASSERT(!rt->gcUnmarkedArenaStackTop);
    JS_ASSERT(rt->gcMarkLaterCount == 0);

    /*
     * Mark phase.
     */
    JS_TRACER_INIT(&trc, cx, NULL);
    rt->gcMarkingTracer = &trc;
    JS_ASSERT(IS_GC_MARKING_TRACER(&trc));

#ifdef DEBUG
    for (a = rt->gcDoubleArenaList.head; a; a = a->info.prev)
        JS_ASSERT(!a->info.hasMarkedDoubles);
#endif

    {
        /*
         * Query rt->gcKeepAtoms only when we know that all other threads are
         * suspended, see bug 541790.
         */
        bool keepAtoms = (gckind & GC_KEEP_ATOMS) || rt->gcKeepAtoms != 0;
        js_TraceRuntime(&trc, keepAtoms);
        js_MarkScriptFilenames(rt, keepAtoms);
    }

    /*
     * Mark children of things that caused too deep recursion during the above
     * tracing.
     */
    MarkDelayedChildren(&trc);

    JS_ASSERT(!cx->insideGCMarkCallback);
    if (rt->gcCallback) {
        cx->insideGCMarkCallback = JS_TRUE;
        (void) rt->gcCallback(cx, JSGC_MARK_END);
        JS_ASSERT(cx->insideGCMarkCallback);
        cx->insideGCMarkCallback = JS_FALSE;
    }
    JS_ASSERT(rt->gcMarkLaterCount == 0);

    rt->gcMarkingTracer = NULL;

#ifdef JS_THREADSAFE
    cx->createDeallocatorTask();
#endif

    /*
     * Sweep phase.
     *
     * Finalize as we sweep, outside of rt->gcLock but with rt->gcRunning set
     * so that any attempt to allocate a GC-thing from a finalizer will fail,
     * rather than nest badly and leave the unmarked newborn to be swept.
     *
     * We first sweep atom state so we can use js_IsAboutToBeFinalized on
     * JSString or jsdouble held in a hashtable to check if the hashtable
     * entry can be freed. Note that even after the entry is freed, JSObject
     * finalizers can continue to access the corresponding jsdouble* and
     * JSString* assuming that they are unique. This works since the
     * atomization API must not be called during GC.
     */
    TIMESTAMP(gcTimer.startSweep);
    js_SweepAtomState(cx);

    /* Finalize iterator states before the objects they iterate over. */
    CloseNativeIterators(cx);

    /* Finalize watch points associated with unreachable objects. */
    js_SweepWatchPoints(cx);

#ifdef DEBUG
    /* Save the pre-sweep count of scope-mapped properties. */
    rt->liveScopePropsPreSweep = rt->liveScopeProps;
#endif

    /*
     * We finalize JSObject instances before JSString, double and other GC
     * things to ensure that object's finalizer can access them even if they
     * will be freed.
     *
     * To minimize the number of checks per each to be freed object and
     * function we use separated list finalizers when a debug hook is
     * installed.
     */
    emptyArenas = NULL;
    if (!cx->debugHooks->objectHook) {
        FinalizeArenaList<JSObject, FinalizeObject>
            (cx, FINALIZE_OBJECT, &emptyArenas);
        FinalizeArenaList<JSFunction, FinalizeFunction>
            (cx, FINALIZE_FUNCTION, &emptyArenas);
    } else {
        FinalizeArenaList<JSObject, FinalizeHookedObject>
            (cx, FINALIZE_OBJECT, &emptyArenas);
        FinalizeArenaList<JSFunction, FinalizeHookedFunction>
            (cx, FINALIZE_FUNCTION, &emptyArenas);
    }
#if JS_HAS_XML_SUPPORT
    FinalizeArenaList<JSXML, FinalizeXML>(cx, FINALIZE_XML, &emptyArenas);
#endif
    TIMESTAMP(gcTimer.sweepObjectEnd);

    /*
     * We sweep the deflated cache before we finalize the strings so the
     * cache can safely use js_IsAboutToBeFinalized..
     */
    rt->deflatedStringCache->sweep(cx);

    FinalizeArenaList<JSString, FinalizeString>
        (cx, FINALIZE_STRING, &emptyArenas);
    for (unsigned i = FINALIZE_EXTERNAL_STRING0;
         i <= FINALIZE_EXTERNAL_STRING_LAST;
         ++i) {
        FinalizeArenaList<JSString, FinalizeExternalString>
            (cx, i, &emptyArenas);
    }
    TIMESTAMP(gcTimer.sweepStringEnd);

    ap = &rt->gcDoubleArenaList.head;
    METER((nlivearenas = 0, nkilledarenas = 0, nthings = 0));
    while ((a = *ap) != NULL) {
        if (!a->info.hasMarkedDoubles) {
            /* No marked double values in the arena. */
            *ap = a->info.prev;
            a->info.prev = emptyArenas;
            emptyArenas = a;
            METER(nkilledarenas++);
        } else {
#ifdef JS_GCMETER
            for (jsuword offset = 0;
                 offset != DOUBLES_PER_ARENA * sizeof(jsdouble);
                 offset += sizeof(jsdouble)) {
                if (IsMarkedGCThing(a, offset))
                    METER(nthings++);
            }
            METER(nlivearenas++);
#endif
            a->info.hasMarkedDoubles = false;
            ap = &a->info.prev;
        }
    }
    METER(UpdateArenaStats(&rt->gcStats.doubleArenaStats,
                           nlivearenas, nkilledarenas, nthings));
    rt->gcDoubleArenaList.cursor = rt->gcDoubleArenaList.head;
    TIMESTAMP(gcTimer.sweepDoubleEnd);
    /*
     * Sweep the runtime's property tree after finalizing objects, in case any
     * had watchpoints referencing tree nodes.
     */
    js::SweepScopeProperties(cx);

    /*
     * Sweep script filenames after sweeping functions in the generic loop
     * above. In this way when a scripted function's finalizer destroys the
     * script and calls rt->destroyScriptHook, the hook can still access the
     * script's filename. See bug 323267.
     */
    js_SweepScriptFilenames(rt);

    /*
     * Destroy arenas after we finished the sweeping so finalizers can safely
     * use js_IsAboutToBeFinalized().
     */
    DestroyGCArenas(rt, emptyArenas);
    TIMESTAMP(gcTimer.sweepDestroyEnd);

#ifdef JS_THREADSAFE
    cx->submitDeallocatorTask();
#endif

    if (rt->gcCallback)
        (void) rt->gcCallback(cx, JSGC_FINALIZE_END);
#ifdef DEBUG_srcnotesize
  { extern void DumpSrcNoteSizeHist();
    DumpSrcNoteSizeHist();
    printf("GC HEAP SIZE %lu\n", (unsigned long)rt->gcBytes);
  }
#endif

#ifdef JS_SCOPE_DEPTH_METER
  { static FILE *fp;
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
#endif /* JS_SCOPE_DEPTH_METER */

#ifdef JS_DUMP_LOOP_STATS
  { static FILE *lsfp;
    if (!lsfp)
        lsfp = fopen("/tmp/loopstats", "w");
    if (lsfp) {
        JS_DumpBasicStats(&rt->loopStats, "loops", lsfp);
        fflush(lsfp);
    }
  }
#endif /* JS_DUMP_LOOP_STATS */

#ifdef JS_TRACER
out:
#endif
    JS_LOCK_GC(rt);

    /*
     * We want to restart GC if js_GC was called recursively or if any of the
     * finalizers called js_RemoveRoot or js_UnlockGCThingRT.
     */
    if (!JS_ON_TRACE(cx) && (rt->gcLevel > 1 || rt->gcPoke)) {
        VOUCH_HAVE_STACK();
        rt->gcLevel = 1;
        rt->gcPoke = JS_FALSE;
        JS_UNLOCK_GC(rt);
        goto restart;
    }

    rt->setGCLastBytes(rt->gcBytes);
  done_running:
    rt->gcLevel = 0;
    rt->gcRunning = rt->gcRegenShapes = false;

#ifdef JS_THREADSAFE
    rt->gcThread = NULL;
    JS_NOTIFY_GC_DONE(rt);

    /*
     * Unlock unless we have GC_LOCK_HELD which requires locked GC on return.
     */
    if (!(gckind & GC_LOCK_HELD))
        JS_UNLOCK_GC(rt);
#endif

    /*
     * Execute JSGC_END callback outside the lock. Again, sample the callback
     * pointer in case it changes, since we are outside of the GC vs. requests
     * interlock mechanism here.
     */
    if (gckind != GC_SET_SLOT_REQUEST && (callback = rt->gcCallback)) {
        if (!(gckind & GC_KEEP_ATOMS)) {
            (void) callback(cx, JSGC_END);

            /*
             * On shutdown iterate until JSGC_END callback stops creating
             * garbage.
             */
            if (gckind == GC_LAST_CONTEXT && rt->gcPoke)
                goto restart_at_beginning;
        } else {
            /*
             * We allow JSGC_END implementation to force a full GC or allocate
             * new GC things. Thus we must protect the weak roots from garbage
             * collection and overwrites.
             */
            AutoSaveWeakRoots save(cx);

            JS_KEEP_ATOMS(rt);
            JS_UNLOCK_GC(rt);

            (void) callback(cx, JSGC_END);

            JS_LOCK_GC(rt);
            JS_UNKEEP_ATOMS(rt);
        }
    }
    TIMESTAMP(gcTimer.end);

#ifdef MOZ_GCTIMER
    if (gcTimer.startMark > 0)
        dumpGCTimer(&gcTimer, firstEnter, gckind == GC_LAST_CONTEXT);
    newChunkCount = 0;
    destroyChunkCount = 0;
#endif
}
