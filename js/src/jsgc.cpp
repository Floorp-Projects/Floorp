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
#include "jsgcchunk.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsparse.h"
#include "jsproxy.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstaticcheck.h"
#include "jsstr.h"
#include "jstask.h"
#include "jstracer.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#include "jsdtracef.h"
#include "jscntxtinlines.h"
#include "jsobjinlines.h"
#include "jshashtable.h"

#ifdef MOZ_VALGRIND
# define JS_VALGRIND
#endif
#ifdef JS_VALGRIND
# include <valgrind/memcheck.h>
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

/*
 * GC memory is allocated in chunks. The size of each chunk is GC_CHUNK_SIZE.
 * The chunk contains an array of GC arenas holding GC things, an array of
 * the mark bitmaps for each arena, an array of JSGCArenaInfo arena
 * descriptors, an array of JSGCMarkingDelay descriptors, the JSGCChunkInfo
 * chunk descriptor and a bitmap indicating free arenas in the chunk. The
 * following picture demonstrates the layout:
 *
 *  +--------+--------------+-------+--------+------------+-----------------+
 *  | arenas | mark bitmaps | infos | delays | chunk info | free arena bits |
 *  +--------+--------------+-------+--------+------------+-----------------+
 *
 * To ensure fast O(1) lookup of mark bits and arena descriptors each chunk is
 * allocated on GC_CHUNK_SIZE boundary. This way a simple mask and shift
 * operation gives an arena index into the mark and JSGCArenaInfo arrays.
 *
 * All chunks that have at least one free arena are put on the doubly-linked
 * list with the head stored in JSRuntime.gcChunkList. JSGCChunkInfo contains
 * the head of the chunk's free arena list together with the link fields for
 * gcChunkList.
 *
 * A GC arena contains GC_ARENA_SIZE bytes aligned on GC_ARENA_SIZE boundary
 * and holds things of the same size and kind. The size of each thing in the
 * arena must be divisible by GC_CELL_SIZE, the minimal allocation unit, and
 * the size of the mark bitmap is fixed and is independent of the thing's
 * size with one bit per each GC_CELL_SIZE bytes. For thing sizes that exceed
 * GC_CELL_SIZE this implies that we waste space in the mark bitmap. The
 * advantage is that we can find the mark bit for the thing using just
 * integer shifts avoiding an expensive integer division. We trade some space
 * for speed here.
 *
 * The number of arenas in the chunk is given by GC_ARENAS_PER_CHUNK. We find
 * that number as follows. Suppose chunk contains n arenas. Together with the
 * word-aligned free arena bitmap and JSGCChunkInfo they should fit into the
 * chunk. Hence GC_ARENAS_PER_CHUNK or n_max is the maximum value of n for
 * which the following holds:
  *
 *   n*s + ceil(n/B) <= M                                               (1)
 *
 * where "/" denotes normal real division,
 *       ceil(r) gives the least integer not smaller than the number r,
 *       s is the number of words in the GC arena, arena's mark bitmap,
 *         JSGCArenaInfo and JSGCMarkingDelay or GC_ARENA_ALL_WORDS.
 *       B is number of bits per word or B == JS_BITS_PER_WORD
 *       M is the number of words in the chunk without JSGCChunkInfo or
 *       M == (GC_CHUNK_SIZE - sizeof(JSGCArenaInfo)) / sizeof(jsuword).
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
 *    M*B == (GC_CHUNK_SIZE - sizeof(JSGCChunkInfo)) / sizeof(jsuword) *
 *           JS_BITS_PER_WORD
 *        == (GC_CHUNK_SIZE - sizeof(JSGCChunkInfo)) * JS_BITS_PER_BYTE
 *
 * since GC_CHUNK_SIZE and sizeof(JSGCChunkInfo) are at least word-aligned.
 */

const jsuword GC_ARENA_SHIFT = 12;
const jsuword GC_ARENA_MASK = JS_BITMASK(GC_ARENA_SHIFT);
const jsuword GC_ARENA_SIZE = JS_BIT(GC_ARENA_SHIFT);

const jsuword GC_MAX_CHUNK_AGE = 3;

const size_t GC_CELL_SHIFT = 3;
const size_t GC_CELL_SIZE = size_t(1) << GC_CELL_SHIFT;
const size_t GC_CELL_MASK = GC_CELL_SIZE - 1;

const size_t BITS_PER_GC_CELL = GC_CELL_SIZE * JS_BITS_PER_BYTE;

const size_t GC_CELLS_PER_ARENA = size_t(1) << (GC_ARENA_SHIFT - GC_CELL_SHIFT);
const size_t GC_MARK_BITMAP_SIZE = GC_CELLS_PER_ARENA / JS_BITS_PER_BYTE;
const size_t GC_MARK_BITMAP_WORDS = GC_CELLS_PER_ARENA / JS_BITS_PER_WORD;

JS_STATIC_ASSERT(sizeof(jsbitmap) == sizeof(jsuword));

JS_STATIC_ASSERT(sizeof(JSString) % GC_CELL_SIZE == 0);
JS_STATIC_ASSERT(sizeof(JSObject) % GC_CELL_SIZE == 0);
JS_STATIC_ASSERT(sizeof(JSFunction) % GC_CELL_SIZE == 0);
#ifdef JSXML
JS_STATIC_ASSERT(sizeof(JSXML) % GC_CELL_SIZE == 0);
#endif

JS_STATIC_ASSERT(GC_CELL_SIZE == sizeof(jsdouble));
const size_t DOUBLES_PER_ARENA = GC_CELLS_PER_ARENA;

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

    JSGCThing       *freeList;

    static inline JSGCArenaInfo *fromGCThing(void* thing);
};

/* See comments before ThingsPerUnmarkedBit below. */
struct JSGCMarkingDelay {
    JSGCArena       *link;
    jsuword         unmarkedChildren;
};

struct JSGCArena {
    uint8 data[GC_ARENA_SIZE];

    void checkAddress() const {
        JS_ASSERT(!(reinterpret_cast<jsuword>(this) & GC_ARENA_MASK));
    }

    jsuword toPageStart() const {
        checkAddress();
        return reinterpret_cast<jsuword>(this);
    }

    static inline JSGCArena *fromGCThing(void* thing);

    static inline JSGCArena *fromChunkAndIndex(jsuword chunk, size_t index);

    jsuword getChunk() {
        return toPageStart() & ~GC_CHUNK_MASK;
    }

    jsuword getIndex() {
        return (toPageStart() & GC_CHUNK_MASK) >> GC_ARENA_SHIFT;
    }

    inline JSGCArenaInfo *getInfo();

    inline JSGCMarkingDelay *getMarkingDelay();

    inline jsbitmap *getMarkBitmap();
};

struct JSGCChunkInfo {
    JSRuntime       *runtime;
    size_t          numFreeArenas;
    size_t          gcChunkAge;

    inline void init(JSRuntime *rt);

    inline jsbitmap *getFreeArenaBitmap();

    inline jsuword getChunk();

    inline void clearMarkBitmap();

    static inline JSGCChunkInfo *fromChunk(jsuword chunk);
};

/* Check that all chunk arrays at least word-aligned. */
JS_STATIC_ASSERT(sizeof(JSGCArena) == GC_ARENA_SIZE);
JS_STATIC_ASSERT(GC_MARK_BITMAP_WORDS % sizeof(jsuword) == 0);
JS_STATIC_ASSERT(sizeof(JSGCArenaInfo) % sizeof(jsuword) == 0);
JS_STATIC_ASSERT(sizeof(JSGCMarkingDelay) % sizeof(jsuword) == 0);

const size_t GC_ARENA_ALL_WORDS = (GC_ARENA_SIZE + GC_MARK_BITMAP_SIZE +
                                   sizeof(JSGCArenaInfo) +
                                   sizeof(JSGCMarkingDelay)) / sizeof(jsuword);

/* The value according (4) above. */
const size_t GC_ARENAS_PER_CHUNK =
    (GC_CHUNK_SIZE - sizeof(JSGCChunkInfo)) * JS_BITS_PER_BYTE /
    (JS_BITS_PER_WORD * GC_ARENA_ALL_WORDS + 1);

const size_t GC_FREE_ARENA_BITMAP_WORDS = (GC_ARENAS_PER_CHUNK +
                                           JS_BITS_PER_WORD - 1) /
                                          JS_BITS_PER_WORD;

const size_t GC_FREE_ARENA_BITMAP_SIZE = GC_FREE_ARENA_BITMAP_WORDS *
                                         sizeof(jsuword);

/* Check that GC_ARENAS_PER_CHUNK indeed maximises (1). */
JS_STATIC_ASSERT(GC_ARENAS_PER_CHUNK * GC_ARENA_ALL_WORDS +
                 GC_FREE_ARENA_BITMAP_WORDS <=
                 (GC_CHUNK_SIZE - sizeof(JSGCChunkInfo)) / sizeof(jsuword));

JS_STATIC_ASSERT((GC_ARENAS_PER_CHUNK + 1) * GC_ARENA_ALL_WORDS +
                 (GC_ARENAS_PER_CHUNK + 1 + JS_BITS_PER_WORD - 1) /
                 JS_BITS_PER_WORD >
                 (GC_CHUNK_SIZE - sizeof(JSGCChunkInfo)) / sizeof(jsuword));


const size_t GC_MARK_BITMAP_ARRAY_OFFSET = GC_ARENAS_PER_CHUNK
                                           << GC_ARENA_SHIFT;

const size_t GC_ARENA_INFO_ARRAY_OFFSET =
    GC_MARK_BITMAP_ARRAY_OFFSET + GC_MARK_BITMAP_SIZE * GC_ARENAS_PER_CHUNK;

const size_t GC_MARKING_DELAY_ARRAY_OFFSET =
    GC_ARENA_INFO_ARRAY_OFFSET + sizeof(JSGCArenaInfo) * GC_ARENAS_PER_CHUNK;

const size_t GC_CHUNK_INFO_OFFSET = GC_CHUNK_SIZE - GC_FREE_ARENA_BITMAP_SIZE -
                                    sizeof(JSGCChunkInfo);

inline jsuword
JSGCChunkInfo::getChunk() {
    jsuword addr = reinterpret_cast<jsuword>(this);
    JS_ASSERT((addr & GC_CHUNK_MASK) == GC_CHUNK_INFO_OFFSET);
    jsuword chunk = addr & ~GC_CHUNK_MASK;
    return chunk;
}

inline void
JSGCChunkInfo::clearMarkBitmap()
{
    PodZero(reinterpret_cast<jsbitmap *>(getChunk() + GC_MARK_BITMAP_ARRAY_OFFSET),
            GC_MARK_BITMAP_WORDS * GC_ARENAS_PER_CHUNK);
}

/* static */
inline JSGCChunkInfo *
JSGCChunkInfo::fromChunk(jsuword chunk) {
    JS_ASSERT(!(chunk & GC_CHUNK_MASK));
    jsuword addr = chunk | GC_CHUNK_INFO_OFFSET;
    return reinterpret_cast<JSGCChunkInfo *>(addr);
}

inline jsbitmap *
JSGCChunkInfo::getFreeArenaBitmap()
{
    jsuword addr = reinterpret_cast<jsuword>(this);
    return reinterpret_cast<jsbitmap *>(addr + sizeof(JSGCChunkInfo));
}

inline void
JSGCChunkInfo::init(JSRuntime *rt)
{
    runtime = rt;
    numFreeArenas = GC_ARENAS_PER_CHUNK;
    gcChunkAge = 0;

    /*
     * For simplicity we set all bits to 1 including the high bits in the
     * last word that corresponds to nonexistent arenas. This is fine since
     * the arena scans the bitmap words from lowest to highest bits and the
     * allocation checks numFreeArenas before doing the search.
     */
    memset(getFreeArenaBitmap(), 0xFF, GC_FREE_ARENA_BITMAP_SIZE);
}

inline void
CheckValidGCThingPtr(void *thing)
{
#ifdef DEBUG
    JS_ASSERT(!JSString::isStatic(thing));
    jsuword addr = reinterpret_cast<jsuword>(thing);
    JS_ASSERT(!(addr & GC_CELL_MASK));
    JS_ASSERT((addr & GC_CHUNK_MASK) < GC_MARK_BITMAP_ARRAY_OFFSET);
#endif
}

/* static */
inline JSGCArenaInfo *
JSGCArenaInfo::fromGCThing(void* thing)
{
    CheckValidGCThingPtr(thing);
    jsuword addr = reinterpret_cast<jsuword>(thing);
    jsuword chunk = addr & ~GC_CHUNK_MASK;
    JSGCArenaInfo *array =
        reinterpret_cast<JSGCArenaInfo *>(chunk | GC_ARENA_INFO_ARRAY_OFFSET);
    size_t arenaIndex = (addr & GC_CHUNK_MASK) >> GC_ARENA_SHIFT;
    return array + arenaIndex;
}

/* static */
inline JSGCArena *
JSGCArena::fromGCThing(void* thing)
{
    CheckValidGCThingPtr(thing);
    jsuword addr = reinterpret_cast<jsuword>(thing);
    return reinterpret_cast<JSGCArena *>(addr & ~GC_ARENA_MASK);
}

/* static */
inline JSGCArena *
JSGCArena::fromChunkAndIndex(jsuword chunk, size_t index) {
    JS_ASSERT(chunk);
    JS_ASSERT(!(chunk & GC_CHUNK_MASK));
    JS_ASSERT(index < GC_ARENAS_PER_CHUNK);
    return reinterpret_cast<JSGCArena *>(chunk | (index << GC_ARENA_SHIFT));
}

inline JSGCArenaInfo *
JSGCArena::getInfo()
{
    jsuword chunk = getChunk();
    jsuword index = getIndex();
    jsuword offset = GC_ARENA_INFO_ARRAY_OFFSET + index * sizeof(JSGCArenaInfo);
    return reinterpret_cast<JSGCArenaInfo *>(chunk | offset);
}

inline JSGCMarkingDelay *
JSGCArena::getMarkingDelay()
{
    jsuword chunk = getChunk();
    jsuword index = getIndex();
    jsuword offset = GC_MARKING_DELAY_ARRAY_OFFSET +
                     index * sizeof(JSGCMarkingDelay);
    return reinterpret_cast<JSGCMarkingDelay *>(chunk | offset);
}

inline jsbitmap *
JSGCArena::getMarkBitmap()
{
    jsuword chunk = getChunk();
    jsuword index = getIndex();
    jsuword offset = GC_MARK_BITMAP_ARRAY_OFFSET + index * GC_MARK_BITMAP_SIZE;
    return reinterpret_cast<jsbitmap *>(chunk | offset);
}

/*
 * Helpers for GC-thing operations.
 */

inline jsbitmap *
GetGCThingMarkBit(void *thing, size_t &bitIndex)
{
    CheckValidGCThingPtr(thing);
    jsuword addr = reinterpret_cast<jsuword>(thing);
    jsuword chunk = addr & ~GC_CHUNK_MASK;
    bitIndex = (addr & GC_CHUNK_MASK) >> GC_CELL_SHIFT;
    return reinterpret_cast<jsbitmap *>(chunk | GC_MARK_BITMAP_ARRAY_OFFSET);
}

inline bool
IsMarkedGCThing(void *thing)
{
    size_t index;
    jsbitmap *markBitmap = GetGCThingMarkBit(thing, index);
    return !!JS_TEST_BIT(markBitmap, index);
}

inline bool
MarkIfUnmarkedGCThing(void *thing)
{
    size_t index;
    jsbitmap *markBitmap = GetGCThingMarkBit(thing, index);
    if (JS_TEST_BIT(markBitmap, index))
        return false;
    JS_SET_BIT(markBitmap, index);
    return true;
}

inline size_t
ThingsPerArena(size_t thingSize)
{
    JS_ASSERT(!(thingSize & GC_CELL_MASK));
    JS_ASSERT(thingSize <= GC_ARENA_SIZE);
    return GC_ARENA_SIZE / thingSize;
}

/* Can only be called if thing belongs to an arena where a->list is not null. */
inline size_t
GCThingToArenaIndex(void *thing)
{
    CheckValidGCThingPtr(thing);
    jsuword addr = reinterpret_cast<jsuword>(thing);
    jsuword offsetInArena = addr & GC_ARENA_MASK;
    JSGCArenaInfo *a = JSGCArenaInfo::fromGCThing(thing);
    JS_ASSERT(a->list);
    JS_ASSERT(offsetInArena % a->list->thingSize == 0);
    return offsetInArena / a->list->thingSize;
}

/* Can only be applicable to arena where a->list is not null. */
inline uint8 *
GCArenaIndexToThing(JSGCArena *a, JSGCArenaInfo *ainfo, size_t index)
{
    JS_ASSERT(a->getInfo() == ainfo);

    /*
     * We use "<=" and not "<" in the assert so index can mean the limit.
     * For the same reason we use "+", not "|" when finding the thing address
     * as the limit address can start at the next arena.
     */
    JS_ASSERT(index <= ThingsPerArena(ainfo->list->thingSize));
    jsuword offsetInArena = index * ainfo->list->thingSize;
    return reinterpret_cast<uint8 *>(a->toPageStart() + offsetInArena);
}

/*
 * The private JSGCThing struct, which describes a JSRuntime.gcFreeList element.
 */
union JSGCThing {
    JSGCThing   *link;
    double      asDouble;
};

static inline JSGCThing *
MakeNewArenaFreeList(JSGCArena *a, size_t thingSize)
{
    jsuword thingsStart = a->toPageStart();
    jsuword lastThingMinAddr = thingsStart + GC_ARENA_SIZE - thingSize * 2 + 1;
    jsuword thingPtr = thingsStart;
    do {
        jsuword nextPtr = thingPtr + thingSize;
        JS_ASSERT((nextPtr & GC_ARENA_MASK) + thingSize <= GC_ARENA_SIZE);
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

inline void *
GetGCChunk(JSRuntime *rt)
{
    void *p = rt->gcChunkAllocator->alloc();
#ifdef MOZ_GCTIMER
    if (p)
        JS_ATOMIC_INCREMENT(&newChunkCount);
#endif
    METER_IF(p, rt->gcStats.nchunks++);
    METER_UPDATE_MAX(rt->gcStats.maxnchunks, rt->gcStats.nchunks);
    return p;
}

inline void
ReleaseGCChunk(JSRuntime *rt, jsuword chunk)
{
    void *p = reinterpret_cast<void *>(chunk);
    JS_ASSERT(p);
#ifdef MOZ_GCTIMER
    JS_ATOMIC_INCREMENT(&destroyChunkCount);
#endif
    JS_ASSERT(rt->gcStats.nchunks != 0);
    METER(rt->gcStats.nchunks--);
    rt->gcChunkAllocator->free(p);
}

static JSGCArena *
NewGCArena(JSContext *cx)
{
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

    size_t nchunks = rt->gcChunks.length();

    JSGCChunkInfo *ci;
    for (;; ++rt->gcChunkCursor) {
        if (rt->gcChunkCursor == nchunks) {
            ci = NULL;
            break;
        }
        ci = rt->gcChunks[rt->gcChunkCursor];
        if (ci->numFreeArenas != 0)
            break;
    }
    if (!ci) {
        if (!rt->gcChunks.reserve(nchunks + 1))
            return NULL;
        void *chunkptr = GetGCChunk(rt);
        if (!chunkptr)
            return NULL;
        ci = JSGCChunkInfo::fromChunk(reinterpret_cast<jsuword>(chunkptr));
        ci->init(rt);
        JS_ALWAYS_TRUE(rt->gcChunks.append(ci));
    }

    /* Scan the bitmap for the first non-zero bit. */
    jsbitmap *freeArenas = ci->getFreeArenaBitmap();
    size_t arenaIndex = 0;
    while (!*freeArenas) {
        arenaIndex += JS_BITS_PER_WORD;
        freeArenas++;
    }
    size_t bit = CountTrailingZeros(*freeArenas);
    arenaIndex += bit;
    JS_ASSERT(arenaIndex < GC_ARENAS_PER_CHUNK);
    JS_ASSERT(*freeArenas & (jsuword(1) << bit));
    *freeArenas &= ~(jsuword(1) << bit);
    --ci->numFreeArenas;

    rt->gcBytes += GC_ARENA_SIZE;
    METER(rt->gcStats.nallarenas++);
    METER_UPDATE_MAX(rt->gcStats.maxnallarenas, rt->gcStats.nallarenas);

    return JSGCArena::fromChunkAndIndex(ci->getChunk(), arenaIndex);
}

/*
 * This function does not touch the arena or release its memory so code can
 * still refer into it.
 */
static void
ReleaseGCArena(JSRuntime *rt, JSGCArena *a)
{
    METER(rt->gcStats.afree++);
    JS_ASSERT(rt->gcBytes >= GC_ARENA_SIZE);
    rt->gcBytes -= GC_ARENA_SIZE;
    JS_ASSERT(rt->gcStats.nallarenas != 0);
    METER(rt->gcStats.nallarenas--);

    jsuword chunk = a->getChunk();
    JSGCChunkInfo *ci = JSGCChunkInfo::fromChunk(chunk);
    JS_ASSERT(ci->numFreeArenas <= GC_ARENAS_PER_CHUNK - 1);
    jsbitmap *freeArenas = ci->getFreeArenaBitmap();
    JS_ASSERT(!JS_TEST_BIT(freeArenas, a->getIndex()));
    JS_SET_BIT(freeArenas, a->getIndex());
    ci->numFreeArenas++;
    if (ci->numFreeArenas == GC_ARENAS_PER_CHUNK)
        ci->gcChunkAge = 0;

#ifdef DEBUG
    a->getInfo()->prev = rt->gcEmptyArenaList;
    rt->gcEmptyArenaList = a;
#endif
}

static void
FreeGCChunks(JSRuntime *rt)
{
#ifdef DEBUG
    while (rt->gcEmptyArenaList) {
        JSGCArena *next = rt->gcEmptyArenaList->getInfo()->prev;
        memset(rt->gcEmptyArenaList, JS_FREE_PATTERN, GC_ARENA_SIZE);
        rt->gcEmptyArenaList = next;
    }
#endif

    /* Remove unused chunks. */
    size_t available = 0;
    for (JSGCChunkInfo **i = rt->gcChunks.begin(); i != rt->gcChunks.end(); ++i) {
        JSGCChunkInfo *ci = *i;
        JS_ASSERT(ci->runtime == rt);
        if (ci->numFreeArenas == GC_ARENAS_PER_CHUNK) {
            if (ci->gcChunkAge > GC_MAX_CHUNK_AGE) {
                ReleaseGCChunk(rt, ci->getChunk());
                continue;
            }
            ci->gcChunkAge++;
        }
        rt->gcChunks[available++] = ci;
    }
    rt->gcChunks.resize(available);
    rt->gcChunkCursor = 0;
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
GetFinalizableArenaTraceKind(JSGCArenaInfo *ainfo)
{
    JS_ASSERT(ainfo->list);
    return GetFinalizableTraceKind(ainfo->list->thingKind);
}

static inline size_t
GetArenaTraceKind(JSGCArenaInfo *ainfo)
{
    if (!ainfo->list)
        return JSTRACE_DOUBLE;
    return GetFinalizableArenaTraceKind(ainfo);
}

static inline size_t
GetFinalizableThingTraceKind(void *thing)
{
    JSGCArenaInfo *ainfo = JSGCArenaInfo::fromGCThing(thing);
    return GetFinalizableArenaTraceKind(ainfo);
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
        rt->gcArenaList[i].head = NULL;
        rt->gcArenaList[i].cursor = NULL;
    }
    rt->gcDoubleArenaList.head = NULL;
    rt->gcDoubleArenaList.cursor = NULL;

    rt->gcBytes = 0;

    for (JSGCChunkInfo **i = rt->gcChunks.begin(); i != rt->gcChunks.end(); ++i)
        ReleaseGCChunk(rt, (*i)->getChunk());
    rt->gcChunks.clear();
    rt->gcChunkCursor = 0;
}

intN
js_GetExternalStringGCType(JSString *str)
{
    JS_STATIC_ASSERT(FINALIZE_STRING + 1 == FINALIZE_EXTERNAL_STRING0);
    JS_ASSERT(!JSString::isStatic(str));

    unsigned thingKind = JSGCArenaInfo::fromGCThing(str)->list->thingKind;
    JS_ASSERT(IsFinalizableStringKind(thingKind));
    return intN(thingKind) - intN(FINALIZE_EXTERNAL_STRING0);
}

JS_FRIEND_API(uint32)
js_GetGCThingTraceKind(void *thing)
{
    if (JSString::isStatic(thing))
        return JSTRACE_STRING;

    JSGCArenaInfo *ainfo = JSGCArenaInfo::fromGCThing(thing);
    return GetArenaTraceKind(ainfo);
}

JSRuntime *
js_GetGCThingRuntime(void *thing)
{
    jsuword chunk = JSGCArena::fromGCThing(thing)->getChunk();
    return JSGCChunkInfo::fromChunk(chunk)->runtime;
}

bool
js_IsAboutToBeFinalized(void *thing)
{
    if (JSString::isStatic(thing))
        return false;

    return !IsMarkedGCThing(thing);
}

JSBool
js_InitGC(JSRuntime *rt, uint32 maxbytes)
{
    InitGCArenaLists(rt);

    if (!rt->gcRootsHash.init(256))
        return false;

    if (!rt->gcLocksHash.init(256))
        return false;

#ifdef JS_THREADSAFE
    if (!rt->gcHelperThread.init())
        return false;
#endif

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

namespace js {

struct GCChunkHasher
{
    typedef jsuword Lookup;
    static HashNumber hash(jsuword chunk) {
        /*
         * Strip zeros for better distribution after multiplying by the golden
         * ratio.
         */
        JS_ASSERT(!(chunk & GC_CHUNK_MASK));
        return HashNumber(chunk >> GC_CHUNK_SHIFT);
    }
    static bool match(jsuword k, jsuword l) {
        JS_ASSERT(!(k & GC_CHUNK_MASK));
        JS_ASSERT(!(l & GC_CHUNK_MASK));
        return k == l;
    }
};

class ConservativeGCStackMarker {
  public:
    ConservativeGCStackMarker(JSTracer *trc);

    ~ConservativeGCStackMarker() {
#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
        dumpConservativeRoots();
#endif
#ifdef JS_GCMETER
        JSConservativeGCStats *total = &trc->context->runtime->gcStats.conservative;
        total->words        += stats.words;
        total->oddaddress   += stats.oddaddress;
        total->special      += stats.special;
        total->notarena     += stats.notarena;
        total->notchunk     += stats.notchunk;
        total->freearena    += stats.freearena;
        total->wrongtag     += stats.wrongtag;
        total->notlive      += stats.notlive;
        total->gcthings     += stats.gcthings;
        total->raw          += stats.raw;
        total->unmarked     += stats.unmarked;
#endif
    }

    void markRoots();

  private:
    void markRange(jsuword *begin, jsuword *end);
    void markWord(jsuword w);

    JSTracer *trc;
    HashSet<jsuword, GCChunkHasher, SystemAllocPolicy> chunkSet;

#if defined(JS_DUMP_CONSERVATIVE_GC_ROOTS) || defined(JS_GCMETER)
    JSConservativeGCStats stats;

  public:
    static void dumpStats(FILE *fp, JSConservativeGCStats *stats);

# define CONSERVATIVE_METER(x)  ((void) (x))
# define CONSERVATIVE_METER_IF(condition, x) ((void) ((condition) && (x)))

#else

# define CONSERVATIVE_METER(x)                  ((void) 0)
# define CONSERVATIVE_METER_IF(condition, x)    ((void) 0)

#endif

#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
  private:
    struct ConservativeRoot { void *thing; uint32 traceKind; };
    Vector<ConservativeRoot, 0, SystemAllocPolicy> conservativeRoots;
    const char *dumpFileName;

    void dumpConservativeRoots();
#endif
};

ConservativeGCStackMarker::ConservativeGCStackMarker(JSTracer *trc)
  : trc(trc)
{
    /*
     * If initializing fails because we are out of memory, stack scanning
     * slows down but is otherwise unaffected.
     */
    JSRuntime *rt = trc->context->runtime;
    if (chunkSet.init(rt->gcChunks.length())) {
        for (JSGCChunkInfo **i = rt->gcChunks.begin(); i != rt->gcChunks.end(); ++i) {
            jsuword chunk = (*i)->getChunk();
            JS_ASSERT(!chunkSet.has(chunk));
            JS_ALWAYS_TRUE(chunkSet.put(chunk));
        }
    }

#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    dumpFileName = getenv("JS_DUMP_CONSERVATIVE_GC_ROOTS");
    memset(&stats, 0, sizeof(stats));
#endif
}

#if defined(JS_DUMP_CONSERVATIVE_GC_ROOTS) || defined(JS_GCMETER)
/* static */
void
ConservativeGCStackMarker::dumpStats(FILE *fp, JSConservativeGCStats *stats)
{
#define ULSTAT(x)       ((unsigned long)(stats->x))
    fprintf(fp, "CONSERVATIVE STACK SCANNING:\n");
    fprintf(fp, "      number of stack words: %lu\n", ULSTAT(words));
    fprintf(fp, "      excluded, low bit set: %lu\n", ULSTAT(oddaddress));
    fprintf(fp, "          excluded, special: %lu\n", ULSTAT(special));
    fprintf(fp, "        not withing a chunk: %lu\n", ULSTAT(notchunk));
    fprintf(fp, "     not within arena range: %lu\n", ULSTAT(notarena));
    fprintf(fp, "       points to free arena: %lu\n", ULSTAT(freearena));
    fprintf(fp, "        excluded, wrong tag: %lu\n", ULSTAT(wrongtag));
    fprintf(fp, "         excluded, not live: %lu\n", ULSTAT(notlive));
    fprintf(fp, "              things marked: %lu\n", ULSTAT(gcthings));
    fprintf(fp, "        raw pointers marked: %lu\n", ULSTAT(raw));
    fprintf(fp, "         conservative roots: %lu\n", ULSTAT(unmarked));
#undef ULSTAT
}
#endif

#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
void
ConservativeGCStackMarker::dumpConservativeRoots()
{
    if (!dumpFileName)
        return;

    JS_ASSERT(stats.unmarked == conservativeRoots.length());

    FILE *fp;
    if (!strcmp(dumpFileName, "stdout")) {
        fp = stdout;
    } else if (!strcmp(dumpFileName, "stderr")) {
        fp = stderr;
    } else if (!(fp = fopen(dumpFileName, "aw"))) {
        fprintf(stderr,
                "Warning: cannot open %s to dump the conservative roots\n",
                dumpFileName);
        return;
    }

    dumpStats(fp, &stats);
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
          case JSTRACE_DOUBLE: {
            jsdouble *dp = (jsdouble *) i->thing;
            fprintf(fp, "double %e", *dp);
            break;
          }
# if JS_HAS_XML_SUPPORT
          case JSTRACE_XML: {
            JSXML *xml = (JSXML *) i->thing;
            fprintf(fp, "xml %u", xml->xml_class);
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

void
ConservativeGCStackMarker::markWord(jsuword w)
{
    /*
     * The conservative scanner may access words that valgrind considers as
     * undefined. To avoid false positives and not to alter valgrind view of
     * the memory we make as memcheck-defined the argument, a copy of the
     * original word. See bug 572678.
     */
#ifdef JS_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED(&w, sizeof(w));
#endif

#define RETURN(x) do { CONSERVATIVE_METER(stats.x++); return; } while (0)
    /*
     * We assume that the compiler never uses sub-word alignment to store
     * pointers and does not tag pointers on its own. Thus we exclude words
     * with JSVAL_INT (any odd words) or JSVAL_SPECIAL tags as they never
     * point to GC things. We also exclude words with a double tag that point
     * into a non-double. But, for example, on 32-bit platforms we cannot
     * exclude a pointer into an object arena tagged with JSVAL_STRING. The
     * latter is 4 and a compiler can store a pointer not to the object but
     * rather a pointer to its second field.
     */
    JS_STATIC_ASSERT(JSVAL_INT == 1);
    JS_STATIC_ASSERT(JSVAL_DOUBLE == 2);
    JS_STATIC_ASSERT(JSVAL_STRING == 4);
    JS_STATIC_ASSERT(JSVAL_SPECIAL == 6);

    if (w & 1)
        RETURN(oddaddress);

    /* Strip off the tag bits. */
    jsuword tag = w & JSVAL_TAGMASK;

    if (tag == JSVAL_SPECIAL)
        RETURN(special);

    jsuword chunk = w & ~GC_CHUNK_MASK;
    JSGCChunkInfo *ci;
    if (JS_LIKELY(chunkSet.initialized())) {
        if (!chunkSet.has(chunk))
            RETURN(notchunk);
        ci = JSGCChunkInfo::fromChunk(chunk);
    } else {
        ci = JSGCChunkInfo::fromChunk(chunk);
        for (JSGCChunkInfo **i = trc->context->runtime->gcChunks.begin(); ; ++i) {
            if (i == trc->context->runtime->gcChunks.end())
                RETURN(notchunk);
            if (*i == ci)
                break;
        }
    }

    if ((w & GC_CHUNK_MASK) >= GC_MARK_BITMAP_ARRAY_OFFSET)
        RETURN(notarena);

    size_t arenaIndex = (w & GC_CHUNK_MASK) >> GC_ARENA_SHIFT;
    if (JS_TEST_BIT(ci->getFreeArenaBitmap(), arenaIndex))
        RETURN(freearena);

    JSGCArena *a = JSGCArena::fromChunkAndIndex(chunk, arenaIndex);
    JSGCArenaInfo *ainfo = a->getInfo();

    JSGCThing *thing;
    uint32 traceKind;
    if (!ainfo->list) { /* doubles */
        if (tag && tag != JSVAL_DOUBLE)
            RETURN(wrongtag);
        JS_STATIC_ASSERT(JSVAL_TAGMASK == 7 && (sizeof(double) - 1) == 7);
        thing = (JSGCThing *) (w & ~JSVAL_TAGMASK);
        traceKind = JSTRACE_DOUBLE;
    } else {
        if (tag == JSVAL_DOUBLE)
            RETURN(wrongtag);
        traceKind = GetFinalizableArenaTraceKind(ainfo);
#if JS_BYTES_PER_WORD == 8
        if (tag == JSVAL_STRING && traceKind != JSTRACE_STRING)
            RETURN(wrongtag);
#endif

        jsuword start = a->toPageStart();
        jsuword offset = w - start;
        size_t thingSize = ainfo->list->thingSize;
        offset -= offset % thingSize;

        /*
         * If GC_ARENA_SIZE % thingSize != 0 or when thingSize is not a power
         * of two, thingSize-aligned pointer may point at the end of the last
         * thing yet be inside the arena.
         */
        if (offset + thingSize > GC_ARENA_SIZE) {
            JS_ASSERT(thingSize & (thingSize - 1));
            RETURN(notarena);
        }
        thing = (JSGCThing *) (start + offset);

        /* Make sure the thing is not on the freelist of the arena. */
        JSGCThing *cursor = ainfo->freeList;
        while (cursor) {
            JS_ASSERT((((jsuword) cursor) & GC_ARENA_MASK) % thingSize == 0);
            JS_ASSERT(!IsMarkedGCThing(cursor));

            /* If the cursor moves past the thing, it's not in the freelist. */
            if (thing < cursor)
                break;

            /* If we find it on the freelist, it's dead. */
            if (thing == cursor)
                RETURN(notlive);
            JS_ASSERT_IF(cursor->link, cursor < cursor->link);
            cursor = cursor->link;
        }
    }

    CONSERVATIVE_METER(stats.gcthings++);
    CONSERVATIVE_METER_IF(!tag, stats.raw++);

    /*
     * We have now a valid pointer, that is either raw or tagged properly.
     * Since we do not rely on the conservative scanning yet and assume that
     * all the roots are precisely reported, any unmarked GC things here mean
     * those things leaked.
     */
    if (IS_GC_MARKING_TRACER(trc)) {
        if (!js_IsAboutToBeFinalized(thing))
            return;
        CONSERVATIVE_METER(stats.unmarked++);
    }

#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    if (IS_GC_MARKING_TRACER(trc) && dumpFileName) {
        ConservativeRoot root = {thing, traceKind};
        conservativeRoots.append(root);
    }
#endif
    JS_SET_TRACING_NAME(trc, "machine stack");
    js_CallGCMarker(trc, thing, traceKind);

#undef RETURN
}

void
ConservativeGCStackMarker::markRange(jsuword *begin, jsuword *end)
{
    JS_ASSERT(begin <= end);
    for (jsuword *i = begin; i != end; ++i) {
        CONSERVATIVE_METER(stats.words++);
        markWord(*i);
    }
}

void
ConservativeGCStackMarker::markRoots()
{
    /* Do conservative scanning of the stack and registers. */
    for (ThreadDataIter i(trc->context->runtime); !i.empty(); i.popFront()) {
        JSThreadData *td = i.threadData();
        ConservativeGCThreadData *ctd = &td->conservativeGC;
        if (ctd->isEnabled()) {
            jsuword *stackMin, *stackEnd;
#if JS_STACK_GROWTH_DIRECTION > 0
            stackMin = td->nativeStackBase;
            stackEnd = ctd->nativeStackTop;
#else
            stackMin = ctd->nativeStackTop + 1;
            stackEnd = td->nativeStackBase;
#endif
            JS_ASSERT(stackMin <= stackEnd);
            markRange(stackMin, stackEnd);
            markRange(ctd->registerSnapshot.words,
                      JS_ARRAY_END(ctd->registerSnapshot.words));
        }
    }
}

/* static */
JS_NEVER_INLINE JS_FRIEND_API(void)
ConservativeGCThreadData::enable(bool knownStackBoundary)
{
    ++enableCount;
    if (enableCount <= 0)
        return;

    /* Update the native stack pointer if it points to a bigger stack. */
#if JS_STACK_GROWTH_DIRECTION > 0
# define CMP >
#else
# define CMP <
#endif
    jsuword dummy;
    if (knownStackBoundary || enableCount == 1 || &dummy CMP nativeStackTop)
        nativeStackTop = &dummy;
#undef CMP

    /* Update the register snapshot with the latest values. */
#if defined(_MSC_VER)
# pragma warning(push)
# pragma warning(disable: 4611)
#endif
    setjmp(registerSnapshot.jmpbuf);
#if defined(_MSC_VER)
# pragma warning(pop)
#endif

}

JS_NEVER_INLINE JS_FRIEND_API(void)
ConservativeGCThreadData::disable()
{
    --enableCount;
#ifdef DEBUG
    if (enableCount == 0)
        nativeStackTop = NULL;
#endif
}

} /* namespace js */


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
    fprintf(fp, "potentially useful GC calls: %lu\n", ULSTAT(poke));
    fprintf(fp, "  thing arenas freed so far: %lu\n", ULSTAT(afree));
    fprintf(fp, "     stack segments scanned: %lu\n", ULSTAT(stackseg));
    fprintf(fp, "stack segment slots scanned: %lu\n", ULSTAT(segslots));
    fprintf(fp, "reachable closeable objects: %lu\n", ULSTAT(nclose));
    fprintf(fp, "    max reachable closeable: %lu\n", ULSTAT(maxnclose));
    fprintf(fp, "      scheduled close hooks: %lu\n", ULSTAT(closelater));
    fprintf(fp, "  max scheduled close hooks: %lu\n", ULSTAT(maxcloselater));

    ConservativeGCStackMarker::dumpStats(fp, &rt->gcStats.conservative);

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

#ifdef JS_THREADSAFE
    rt->gcHelperThread.cancel();
#endif
    FinishGCArenaLists(rt);

#ifdef DEBUG
    if (!rt->gcRootsHash.empty())
        CheckLeakedRoots(rt);
#endif
    rt->gcRootsHash.clear();
    rt->gcLocksHash.clear();
}

JSBool
js_AddRoot(JSContext *cx, jsval *vp, const char *name)
{
    JSBool ok = js_AddRootRT(cx->runtime, vp, name);
    if (!ok)
        JS_ReportOutOfMemory(cx);
    return ok;
}

JSBool
js_AddGCThingRoot(JSContext *cx, void **rp, const char *name)
{
    JSBool ok = js_AddGCThingRootRT(cx->runtime, rp, name);
    if (!ok)
        JS_ReportOutOfMemory(cx);
    return ok;
}

static JSBool
AddRoot(JSRuntime *rt, void *rp, const char *name)
{
    js::GCRoots *roots = &rt->gcRootsHash;

    /*
     * Due to the long-standing, but now removed, use of rt->gcLock across the
     * bulk of js_GC, API users have come to depend on JS_AddRoot etc. locking
     * properly with a racing GC, without calling JS_AddRoot from a request.
     * We have to preserve API compatibility here, now that we avoid holding
     * rt->gcLock across the mark phase (including the root hashtable mark).
     */
    AutoLockGC lock(rt);
    js_WaitForGC(rt);
    return !!roots->put(rp, name);
}

JS_FRIEND_API(JSBool)
js_AddRootRT(JSRuntime *rt, jsval *vp, const char *name)
{
    return AddRoot(rt, vp, name);
}

JS_FRIEND_API(JSBool)
js_AddGCThingRootRT(JSRuntime *rt, void **rp, const char *name)
{
    return AddRoot(rt, rp, name);
}

JS_FRIEND_API(JSBool)
js_RemoveRoot(JSRuntime *rt, void *rp)
{
    /*
     * Due to the JS_RemoveRoot API, we may be called outside of a request.
     * Same synchronization drill as above in js_AddRoot.
     */
    AutoLockGC lock(rt);
    js_WaitForGC(rt);
    rt->gcRootsHash.remove(rp);
    rt->gcPoke = JS_TRUE;
    return JS_TRUE;
}

#ifdef DEBUG

static void
CheckLeakedRoots(JSRuntime *rt)
{
    uint32 leakedroots = 0;

    /* Warn (but don't assert) debug builds of any remaining roots. */
    for (GCRoots::Range r = rt->gcRootsHash.all(); !r.empty(); r.popFront()) {
        leakedroots++;
        fprintf(stderr,
                "JS engine warning: leaking GC root \'%s\' at %p\n",
                r.front().value ? r.front().value : "", r.front().key);
    }
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

void
js_DumpNamedRoots(JSRuntime *rt,
                  void (*dump)(const char *name, void *rp, void *data),
                  void *data)
{
    for (GCRoots::Range r = rt->gcRootsHash.all(); !r.empty(); r.popFront()) {
        if (r.front().value)
            dump(r.front().value, r.front().key, data);
    }
}

#endif /* DEBUG */

uint32
js_MapGCRoots(JSRuntime *rt, JSGCRootMapFun map, void *data)
{
    AutoLockGC lock(rt);
    intN mapflags;
    uint32 i = 0;

    for (GCRoots::Enum e(rt->gcRootsHash); !e.empty(); e.popFront()) {
        mapflags = map(e.front().key, e.front().value, data);
        i++;
        if (mapflags & JS_MAP_GCROOT_REMOVE)
            e.removeFront();
        if (mapflags & JS_MAP_GCROOT_STOP)
            break;
    }
    return i;
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
            JSGCArenaInfo *ainfo = JSGCArenaInfo::fromGCThing(freeListHead);
            JS_ASSERT(!ainfo->freeList);
            ainfo->freeList = freeListHead;
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

static void
LastDitchGC(JSContext *cx)
{
    JS_ASSERT(!JS_ON_TRACE(cx));

    /* The last ditch GC preserves weak roots and all atoms. */
    AutoPreserveWeakRoots save(cx);
    AutoKeepAtoms keep(cx->runtime);

    /*
     * Keep rt->gcLock across the call into the GC so we don't starve and
     * lose to racing threads who deplete the heap just after the GC has
     * replenished it (or has synchronized with a racing GC that collected a
     * bunch of garbage).  This unfair scheduling can happen on certain
     * operating systems. For the gory details, see bug 162779.
     */
    js_GC(cx, GC_LOCK_HELD);
}

static JSGCThing *
RefillFinalizableFreeList(JSContext *cx, unsigned thingKind)
{
    JS_ASSERT(!JS_THREAD_DATA(cx)->gcFreeLists.finalizables[thingKind]);
    JSRuntime *rt = cx->runtime;
    JSGCArenaList *arenaList;
    JSGCArena *a;

    {
        AutoLockGC lock(rt);
        JS_ASSERT(!rt->gcRunning);
        if (rt->gcRunning) {
            METER(rt->gcStats.finalfail++);
            return NULL;
        }

        bool canGC = !JS_ON_TRACE(cx) && !JS_THREAD_DATA(cx)->waiveGCQuota;
        bool doGC = canGC && IsGCThresholdReached(rt);
        arenaList = &rt->gcArenaList[thingKind];
        for (;;) {
            if (doGC) {
                LastDitchGC(cx);
                METER(cx->runtime->gcStats.arenaStats[thingKind].retry++);
                canGC = false;

                /*
                 * The JSGC_END callback can legitimately allocate new GC
                 * things and populate the free list. If that happens, just
                 * return that list head.
                 */
                JSGCThing *freeList = JS_THREAD_DATA(cx)->gcFreeLists.finalizables[thingKind];
                if (freeList)
                    return freeList;
            }

            while ((a = arenaList->cursor) != NULL) {
                JSGCArenaInfo *ainfo = a->getInfo();
                arenaList->cursor = ainfo->prev;
                JSGCThing *freeList = ainfo->freeList;
                if (freeList) {
                    ainfo->freeList = NULL;
                    return freeList;
                }
            }

            a = NewGCArena(cx);
            if (a)
                break;
            if (!canGC) {
                METER(cx->runtime->gcStats.arenaStats[thingKind].fail++);
                return NULL;
            }
            doGC = true;
        }

        /*
         * Do only minimal initialization of the arena inside the GC lock. We
         * can do the rest outside the lock because no other threads will see
         * the arena until the GC is run.
         */
        JSGCArenaInfo *ainfo = a->getInfo();
        ainfo->list = arenaList;
        ainfo->prev = arenaList->head;
        ainfo->freeList = NULL;
        arenaList->head = a;
    }

    JSGCMarkingDelay *markingDelay = a->getMarkingDelay();
    markingDelay->link = NULL;
    markingDelay->unmarkedChildren = 0;

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
        *freeListp = thing->link;
        cx->weakRoots.finalizableNewborns[thingKind] = thing;
        CheckGCFreeListLink(thing);
        METER(cx->runtime->gcStats.arenaStats[thingKind].localalloc++);
        return thing;
    }

    thing = RefillFinalizableFreeList(cx, thingKind);
    if (!thing) {
        js_ReportOutOfMemory(cx);
        return NULL;
    }

    /*
     * See comments in RefillFinalizableFreeList about a possibility
     * of *freeListp == thing.
     */
    JS_ASSERT(!*freeListp || *freeListp == thing);
    *freeListp = thing->link;

    CheckGCFreeListLink(thing);

    cx->weakRoots.finalizableNewborns[thingKind] = thing;

    return thing;
}

static JSGCThing *
TurnUsedArenaIntoDoubleList(JSGCArena *a)
{
    JSGCThing *head;
    JSGCThing **tailp = &head;
    jsuword thing = a->toPageStart();
    jsbitmap *markBitmap = a->getMarkBitmap();
    jsbitmap *lastMarkWord = markBitmap + GC_MARK_BITMAP_WORDS - 1;

    for (jsbitmap *m = markBitmap; m <= lastMarkWord; ++m) {
        JS_ASSERT(thing < a->toPageStart() + GC_ARENA_SIZE);
        JS_ASSERT((thing - a->toPageStart()) %
                  (JS_BITS_PER_WORD * sizeof(jsdouble)) == 0);

        jsbitmap bits = *m;
        if (bits == jsbitmap(-1)) {
            thing += JS_BITS_PER_WORD * sizeof(jsdouble);
        } else {
            /*
             * We have some zero bits. Turn corresponding cells into a list
             * unrolling the loop for better performance.
             */
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
    JS_ASSERT(!JS_THREAD_DATA(cx)->gcFreeLists.doubles);

    JSRuntime *rt = cx->runtime;
    JS_ASSERT(!rt->gcRunning);

    JS_LOCK_GC(rt);

    bool canGC = !JS_ON_TRACE(cx) && !JS_THREAD_DATA(cx)->waiveGCQuota;
    bool doGC = canGC && IsGCThresholdReached(rt);
    JSGCArena *a;
    for (;;) {
        if (doGC) {
            LastDitchGC(cx);
            METER(rt->gcStats.doubleArenaStats.retry++);
            canGC = false;

            /* See comments in RefillFinalizableFreeList. */
            JSGCThing *freeList = JS_THREAD_DATA(cx)->gcFreeLists.doubles;
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
            rt->gcDoubleArenaList.cursor = a->getInfo()->prev;
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

    JSGCArenaInfo *ainfo = a->getInfo();
    ainfo->list = NULL;
    ainfo->freeList = NULL;
    ainfo->prev = rt->gcDoubleArenaList.head;
    rt->gcDoubleArenaList.head = a;
    JS_UNLOCK_GC(rt);

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
        CheckGCFreeListLink(thing);
        *freeListp = thing->link;

        jsdouble *dp = &thing->asDouble;
        *dp = d;
        *vp = DOUBLE_TO_JSVAL(dp);
        return true;
    }

    thing = RefillDoubleFreeList(cx);
    if (!thing) {
        if (!JS_ON_TRACE(cx)) {
            /* Trace code handle this on its own. */
            js_ReportOutOfMemory(cx);
            METER(cx->runtime->gcStats.doubleArenaStats.fail++);
        }
        return false;
    }

    JS_ASSERT(!*freeListp || *freeListp == thing);

    CheckGCFreeListLink(thing);
    *freeListp = thing->link;

    jsdouble *dp = reinterpret_cast<jsdouble *>(thing);
    *dp = d;
    *vp = DOUBLE_TO_JSVAL(dp);
    return true;
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
    GCLocks *locks;

    if (!thing)
        return true;
    locks = &rt->gcLocksHash;
    AutoLockGC lock(rt);
    GCLocks::AddPtr p = locks->lookupForAdd(thing);

    if (!p) {
        if (!locks->add(p, thing, 1))
            return false;
    } else {
        JS_ASSERT(p->value >= 1);
        p->value++;
    }

    METER(rt->gcStats.lock++);
    return true;
}

void
js_UnlockGCThingRT(JSRuntime *rt, void *thing)
{
    if (!thing)
        return;

    AutoLockGC lock(rt);
    GCLocks::Ptr p = rt->gcLocksHash.lookup(thing);

    if (p) {
        rt->gcPoke = true;
        if (--p->value == 0)
            rt->gcLocksHash.remove(p);

        METER(rt->gcStats.unlock++);
    }
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
 * the normal case of sufficient native stack, the code uses two fields per
 * arena stored in JSGCMarkingDelay. The first field, JSGCMarkingDelay::link,
 * links all arenas with delayed things into a stack list with the pointer to
 * stack top in JSRuntime::gcUnmarkedArenaStackTop. DelayMarkingChildren adds
 * arenas to the stack as necessary while MarkDelayedChildren pops the arenas
 * from the stack until it empties.
 *
 * The second field, JSGCMarkingDelay::unmarkedChildren, is a bitmap that
 * tells for which things the GC should call JS_TraceChildren later. The
 * bitmap is a single word. As such it does not pinpoint the delayed things
 * in the arena but rather tells the intervals containing
 * ThingsPerUnmarkedBit(thingSize) things. Later the code in
 * MarkDelayedChildren discovers such intervals and calls JS_TraceChildren on
 * any marked thing in the interval. This implies that JS_TraceChildren can be
 * called many times for a single thing if the thing shares the same interval
 * with some delayed things. This should be fine as any GC graph
 * marking/traversing hooks must allow repeated calls during the same GC cycle.
 * In particular, xpcom cycle collector relies on this.
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
    JS_ASSERT(IsMarkedGCThing(thing));
    METER(rt->gcStats.unmarked++);

    JSGCArena *a = JSGCArena::fromGCThing(thing);
    JSGCArenaInfo *ainfo = a->getInfo();
    JSGCMarkingDelay *markingDelay = a->getMarkingDelay();

    size_t thingArenaIndex = GCThingToArenaIndex(thing);
    size_t unmarkedBitIndex = thingArenaIndex /
                              ThingsPerUnmarkedBit(ainfo->list->thingSize);
    JS_ASSERT(unmarkedBitIndex < JS_BITS_PER_WORD);

    jsuword bit = jsuword(1) << unmarkedBitIndex;
    if (markingDelay->unmarkedChildren != 0) {
        JS_ASSERT(rt->gcUnmarkedArenaStackTop);
        if (markingDelay->unmarkedChildren & bit) {
            /* bit already covers things with children to mark later. */
            return;
        }
        markingDelay->unmarkedChildren |= bit;
    } else {
        /*
         * The thing is the first thing with not yet marked children in the
         * whole arena, so push the arena on the stack of arenas with things
         * to be marked later unless the arena has already been pushed. We
         * detect that through checking prevUnmarked as the field is 0
         * only for not yet pushed arenas. To ensure that
         *   prevUnmarked != 0
         * even when the stack contains one element, we make prevUnmarked
         * for the arena at the bottom to point to itself.
         *
         * See comments in MarkDelayedChildren.
         */
        markingDelay->unmarkedChildren = bit;
        if (!markingDelay->link) {
            if (!rt->gcUnmarkedArenaStackTop) {
                /* Stack was empty, mark the arena as the bottom element. */
                markingDelay->link = a;
            } else {
                JS_ASSERT(rt->gcUnmarkedArenaStackTop->getMarkingDelay()->link);
                markingDelay->link = rt->gcUnmarkedArenaStackTop;
            }
            rt->gcUnmarkedArenaStackTop = a;
        }
        JS_ASSERT(rt->gcUnmarkedArenaStackTop);
    }
#ifdef DEBUG
    rt->gcMarkLaterCount += ThingsPerUnmarkedBit(ainfo->list->thingSize);
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
         * the stack's bottom, prevUnmarked != 0 but rather points to
         * itself.
         */
        JSGCArenaInfo *ainfo = a->getInfo();
        JSGCMarkingDelay *markingDelay = a->getMarkingDelay();
        JS_ASSERT(markingDelay->link);
        JS_ASSERT(rt->gcUnmarkedArenaStackTop->getMarkingDelay()->link);
        thingSize = ainfo->list->thingSize;
        traceKind = GetFinalizableArenaTraceKind(ainfo);
        indexLimit = ThingsPerArena(thingSize);
        thingsPerUnmarkedBit = ThingsPerUnmarkedBit(thingSize);

        /*
         * We cannot use do-while loop here as a->unmarkedChildren can be zero
         * before the loop as a leftover from the previous iterations. See
         * comments after the loop.
         */
        while (markingDelay->unmarkedChildren != 0) {
            unmarkedBitIndex = JS_FLOOR_LOG2W(markingDelay->unmarkedChildren);
            markingDelay->unmarkedChildren &= ~(jsuword(1) << unmarkedBitIndex);
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
            uint8 *thing = GCArenaIndexToThing(a, ainfo, thingIndex);
            uint8 *end = GCArenaIndexToThing(a, ainfo, endIndex);
            do {
                JS_ASSERT(thing < end);
                if (IsMarkedGCThing(thing))
                    JS_TraceChildren(trc, thing, traceKind);
                thing += thingSize;
            } while (thing != end);
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
            aprev = markingDelay->link;
            markingDelay->link = NULL;
            if (a == aprev) {
                /*
                 * prevUnmarked points to itself and we reached the bottom of
                 * the stack.
                 */
                break;
            }
            rt->gcUnmarkedArenaStackTop = a = aprev;
        } else {
            a = rt->gcUnmarkedArenaStackTop;
        }
    }
    JS_ASSERT(rt->gcUnmarkedArenaStackTop);
    JS_ASSERT(!rt->gcUnmarkedArenaStackTop->getMarkingDelay()->link);
    rt->gcUnmarkedArenaStackTop = NULL;
    JS_ASSERT(rt->gcMarkLaterCount == 0);
}

void
js_CallGCMarker(JSTracer *trc, void *thing, uint32 kind)
{
    JSContext *cx;
    JSRuntime *rt;

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
    JS_ASSERT(rt->gcRunning);

    /*
     * Optimize for string and double as their size is known and their tracing
     * is not recursive.
     */
    switch (kind) {
      case JSTRACE_DOUBLE: {
        MarkIfUnmarkedGCThing(thing);
        goto out;
      }

      case JSTRACE_STRING:
        for (;;) {
            if (JSString::isStatic(thing))
                goto out;
            JS_ASSERT(kind == GetFinalizableThingTraceKind(thing));
            if (!MarkIfUnmarkedGCThing(thing))
                goto out;
            if (!((JSString *) thing)->isDependent())
                goto out;
            thing = ((JSString *) thing)->dependentBase();
        }
        /* NOTREACHED */
    }

    JS_ASSERT(kind == GetFinalizableThingTraceKind(thing));
    if (!MarkIfUnmarkedGCThing(thing))
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

static void
gc_root_traversal(const GCRoots::Entry &entry, JSTracer *trc)
{
    jsval *rp = (jsval *)entry.key;
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
                for (JSGCArena *a = arenaList->head;
                     a;
                     a = a->getInfo()->prev) {
                    if (thing - a->toPageStart() < limit) {
                        root_points_to_gcArenaList = true;
                        break;
                    }
                }
            }
            if (!root_points_to_gcArenaList) {
                for (JSGCArena *a = rt->gcDoubleArenaList.head;
                     a;
                     a = a->getInfo()->prev) {
                    if (thing - a->toPageStart() <
                        DOUBLES_PER_ARENA * sizeof(jsdouble)) {
                        root_points_to_gcArenaList = true;
                        break;
                    }
                }
            }
            if (!root_points_to_gcArenaList && entry.value) {
                fprintf(stderr,
"JS API usage error: the address passed to JS_AddNamedRoot currently holds an\n"
"invalid jsval.  This is usually caused by a missing call to JS_RemoveRoot.\n"
"The root's name is \"%s\".\n",
                        entry.value);
            }
            JS_ASSERT(root_points_to_gcArenaList);
        }
#endif
        JS_SET_TRACING_NAME(trc, entry.value ? entry.value : "root");
        js_CallValueTracerIfGCThing(trc, v);
    }
}

static void
gc_lock_traversal(const GCLocks::Entry &entry, JSTracer *trc)
{
    uint32 traceKind;

    JS_ASSERT(entry.value >= 1);
    traceKind = js_GetGCThingTraceKind(entry.key);
    JS_CALL_TRACER(trc, entry.key, traceKind, "locked object");
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

    if (fp->callobj)
        JS_CALL_OBJECT_TRACER(trc, fp->callobj, "call");
    if (fp->argsobj)
        JS_CALL_OBJECT_TRACER(trc, JSVAL_TO_OBJECT(fp->argsobj), "arguments");
    if (fp->script)
        js_TraceScript(trc, fp->script);

    /* Allow for primitive this parameter due to JSFUN_THISP_* flags. */
    JS_CALL_VALUE_TRACER(trc, fp->thisv, "this");

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

inline void
AutoGCRooter::trace(JSTracer *trc)
{
    switch (tag) {
      case JSVAL:
        JS_SET_TRACING_NAME(trc, "js::AutoValueRooter.val");
        js_CallValueTracerIfGCThing(trc, static_cast<AutoValueRooter *>(this)->val);
        return;

      case SPROP:
        static_cast<AutoScopePropertyRooter *>(this)->sprop->trace(trc);
        return;

      case WEAKROOTS:
        static_cast<AutoPreserveWeakRoots *>(this)->savedRoots.mark(trc);
        return;

      case PARSER:
        static_cast<Parser *>(this)->trace(trc);
        return;

      case SCRIPT:
        if (JSScript *script = static_cast<AutoScriptRooter *>(this)->script)
            js_TraceScript(trc, script);
        return;

      case ENUMERATOR:
        static_cast<AutoEnumStateRooter *>(this)->trace(trc);
        return;

      case IDARRAY: {
        JSIdArray *ida = static_cast<AutoIdArray *>(this)->idArray;
        TraceValues(trc, ida->length, ida->vector, "js::AutoIdArray.idArray");
        return;
      }

      case DESCRIPTORS: {
        PropertyDescriptorArray &descriptors =
            static_cast<AutoDescriptorArray *>(this)->descriptors;
        for (size_t i = 0, len = descriptors.length(); i < len; i++) {
            PropertyDescriptor &desc = descriptors[i];

            JS_CALL_VALUE_TRACER(trc, desc.pd, "PropertyDescriptor::pd");
            JS_CALL_VALUE_TRACER(trc, desc.value, "PropertyDescriptor::value");
            JS_CALL_VALUE_TRACER(trc, desc.get, "PropertyDescriptor::get");
            JS_CALL_VALUE_TRACER(trc, desc.set, "PropertyDescriptor::set");
            js_TraceId(trc, desc.id);
        }
        return;
      }

      case DESCRIPTOR : {
        AutoDescriptor &desc = *static_cast<AutoDescriptor *>(this);
        if (desc.obj)
            JS_CALL_OBJECT_TRACER(trc, desc.obj, "Descriptor::obj");
        JS_CALL_VALUE_TRACER(trc, desc.value, "Descriptor::value");
        if (desc.attrs & JSPROP_GETTER)
            JS_CALL_VALUE_TRACER(trc, jsval(desc.getter), "Descriptor::get");
        if (desc.attrs & JSPROP_SETTER)
            JS_CALL_VALUE_TRACER(trc, jsval(desc.setter), "Descriptor::set");
        return;
      }

      case NAMESPACES: {
        JSXMLArray &array = static_cast<AutoNamespaceArray *>(this)->array;
        TraceObjectVector(trc, reinterpret_cast<JSObject **>(array.vector), array.length);
        array.cursors->trace(trc);
        return;
      }

      case XML:
        js_TraceXML(trc, static_cast<AutoXMLRooter *>(this)->xml);
        return;

      case OBJECT:
        if (JSObject *obj = static_cast<AutoObjectRooter *>(this)->obj) {
            JS_SET_TRACING_NAME(trc, "js::AutoObjectRooter.obj");
            js_CallGCMarker(trc, obj, JSTRACE_OBJECT);
        }
        return;

      case ID:
        JS_SET_TRACING_NAME(trc, "js::AutoIdRooter.val");
        js_CallValueTracerIfGCThing(trc, static_cast<AutoIdRooter *>(this)->idval);
        return;

      case VECTOR: {
        js::Vector<jsval, 8> &vector = static_cast<js::AutoValueVector *>(this)->vector;
        js::TraceValues(trc, vector.length(), vector.begin(), "js::AutoValueVector.vector");
        return;
      }
    }

    JS_ASSERT(tag >= 0);
    TraceValues(trc, tag, static_cast<AutoArrayRooter *>(this)->array, "js::AutoArrayRooter.array");
}

void
js_TraceContext(JSTracer *trc, JSContext *acx)
{
    /* Stack frames and slots are traced by StackSpace::mark. */

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

    for (js::AutoGCRooter *gcr = acx->autoGCRooters; gcr; gcr = gcr->down)
        gcr->trace(trc);

    if (acx->sharpObjectMap.depth > 0)
        js_TraceSharpMap(trc, &acx->sharpObjectMap);

    js_TraceRegExpStatics(trc, acx);

    JS_CALL_VALUE_TRACER(trc, acx->iterValue, "iterValue");

    acx->compartment->marked = true;

#ifdef JS_TRACER
    TracerState* state = acx->tracerState;
    while (state) {
        if (state->nativeVp)
            TraceValues(trc, state->nativeVpLen, state->nativeVp, "nativeVp");
        state = state->prev;
    }
#endif
}

JS_REQUIRES_STACK void
js_TraceRuntime(JSTracer *trc)
{
    JSRuntime *rt = trc->context->runtime;

    for (GCRoots::Range r = rt->gcRootsHash.all(); !r.empty(); r.popFront())
        gc_root_traversal(r.front(), trc);

    for (GCLocks::Range r = rt->gcLocksHash.all(); !r.empty(); r.popFront())
        gc_lock_traversal(r.front(), trc);

    js_TraceAtomState(trc);
    js_TraceRuntimeNumberState(trc);
    js_MarkTraps(trc);

    JSContext *iter = NULL;
    while (JSContext *acx = js_ContextIterator(rt, JS_TRUE, &iter))
        js_TraceContext(trc, acx);

    for (ThreadDataIter i(rt); !i.empty(); i.popFront())
        i.threadData()->mark(trc);

    if (rt->gcExtraRootsTraceOp)
        rt->gcExtraRootsTraceOp(trc, rt->gcExtraRootsData);

    /*
     * For now we use the conservative stack scanner only as a sanity check,
     * so we mark conservatively after marking all other roots to detect
     * conservative leaks.
     */
    if (rt->state != JSRTS_LANDING)
        ConservativeGCStackMarker(trc).markRoots();
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

    /*
     * Trigger the GC when it is safe to call an operation callback on any
     * thread.
     */
    rt->gcIsNeeded = JS_TRUE;
    js_TriggerAllOperationCallbacks(rt, gcLocked);
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
    JS_ASSERT(thingKind == FINALIZE_OBJECT ||
              thingKind == FINALIZE_FUNCTION);

    /* Cope with stillborn objects that have no map. */
    if (!obj->map)
        return;

    /* Finalize obj first, in case it needs map and slots. */
    JSClass *clasp = obj->getClass();
    if (clasp->finalize)
        clasp->finalize(cx, obj);

    DTrace::finalizeObject(obj);

    if (JS_LIKELY(obj->isNative())) {
        JSScope *scope = obj->scope();
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
        JS_ASSERT(JSGCArenaInfo::fromGCThing(str)->list->thingKind ==
                  FINALIZE_STRING);
        JS_ASSERT(str->dependentBase());
        JS_RUNTIME_UNMETER(rt, liveDependentStrings);
    } else {
        unsigned thingKind = JSGCArenaInfo::fromGCThing(str)->list->thingKind;
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
FinalizeArenaList(JSContext *cx, unsigned thingKind)
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
        JSGCArenaInfo *ainfo = a->getInfo();
        JS_ASSERT(ainfo->list == arenaList);
        JS_ASSERT(!a->getMarkingDelay()->link);
        JS_ASSERT(a->getMarkingDelay()->unmarkedChildren == 0);

        JSGCThing *freeList = NULL;
        JSGCThing **tailp = &freeList;
        bool allClear = true;

        jsuword thing = a->toPageStart();
        jsuword thingsEnd = thing + GC_ARENA_SIZE / sizeof(T) * sizeof(T);

        jsuword nextFree = reinterpret_cast<jsuword>(ainfo->freeList);
        if (!nextFree) {
            nextFree = thingsEnd;
        } else {
            JS_ASSERT(thing <= nextFree);
            JS_ASSERT(nextFree < thingsEnd);
        }

        jsuword gcCellIndex = 0;
        jsbitmap *bitmap = a->getMarkBitmap();
        for (;; thing += sizeof(T), gcCellIndex += sizeof(T) >> GC_CELL_SHIFT) {
            if (thing == nextFree) {
                if (thing == thingsEnd)
                    break;
                nextFree = reinterpret_cast<jsuword>(
                    reinterpret_cast<JSGCThing *>(nextFree)->link);
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
                T *t = reinterpret_cast<T *>(thing);
                finalizer(cx, t, thingKind);
#ifdef DEBUG
                memset(t, JS_FREE_PATTERN, sizeof(T));
#endif
            }
            JSGCThing *t = reinterpret_cast<JSGCThing *>(thing);
            *tailp = t;
            tailp = &t->link;
        }

#ifdef DEBUG
        /* Check that the free list is consistent. */
        unsigned nfree = 0;
        if (freeList) {
            JS_ASSERT(tailp != &freeList);
            JSGCThing *t = freeList;
            for (;;) {
                ++nfree;
                if (&t->link == tailp)
                    break;
                JS_ASSERT(t < t->link);
                t = t->link;
            }
        }
#endif
        if (allClear) {
            /*
             * Forget just assembled free list head for the arena and
             * add the arena itself to the destroy list.
             */
            JS_ASSERT(nfree == ThingsPerArena(sizeof(T)));
            *ap = ainfo->prev;
            ReleaseGCArena(cx->runtime, a);
            METER(nkilledarenas++);
        } else {
            JS_ASSERT(nfree < ThingsPerArena(sizeof(T)));
            *tailp = NULL;
            ainfo->freeList = freeList;
            ap = &ainfo->prev;
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

const bool JS_WANT_GC_SUITE_PRINT = true;  //false for gnuplot output

struct GCTimer {
    uint64 enter;
    uint64 startMark;
    uint64 startSweep;
    uint64 sweepObjectEnd;
    uint64 sweepStringEnd;
    uint64 sweepDoubleEnd;
    uint64 sweepDestroyEnd;
    uint64 end;

    GCTimer() {
        getFirstEnter();
        memset(this, 0, sizeof(GCTimer));
        enter = rdtsc();
    }

    static uint64 getFirstEnter() {
        static uint64 firstEnter = rdtsc();
        return firstEnter;
    }

    void finish(bool lastGC) {
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

                    fprintf(gcFile, "     AppTime,  Total,   Mark,  Sweep,");
                    fprintf(gcFile, " FinObj, FinStr, FinDbl,");
                    fprintf(gcFile, " Destroy,  newChunks, destoyChunks\n");
                }
                JS_ASSERT(gcFile);
                fprintf(gcFile, "%12.1f, %6.1f, %6.1f, %6.1f, %6.1f, %6.1f,"\
                                 " %6.1f, %7.1f, ",
                        (double)(enter - getFirstEnter()) / 1e6,
                        (double)(end - enter) / 1e6,
                        (double)(startSweep - startMark) / 1e6,
                        (double)(sweepDestroyEnd - startSweep) / 1e6,
                        (double)(sweepObjectEnd - startSweep) / 1e6,
                        (double)(sweepStringEnd - sweepObjectEnd) / 1e6,
                        (double)(sweepDoubleEnd - sweepStringEnd) / 1e6,
                        (double)(sweepDestroyEnd - sweepDoubleEnd) / 1e6);
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

static inline bool
HasMarkedDoubles(JSGCArena *a)
{
    JS_STATIC_ASSERT(GC_MARK_BITMAP_SIZE == 8 * sizeof(uint64));
    uint64 *markBitmap = (uint64 *) a->getMarkBitmap();
    return !!(markBitmap[0] | markBitmap[1] | markBitmap[2] | markBitmap[3] |
              markBitmap[4] | markBitmap[5] | markBitmap[6] | markBitmap[7]);
}

static void
SweepDoubles(JSRuntime *rt)
{
#ifdef JS_GCMETER
    uint32 nlivearenas = 0, nkilledarenas = 0, nthings = 0;
#endif
    JSGCArena **ap = &rt->gcDoubleArenaList.head;
    while (JSGCArena *a = *ap) {
        JSGCArenaInfo *ainfo = a->getInfo();
        if (!HasMarkedDoubles(a)) {
            /* No marked double values in the arena. */
            *ap = ainfo->prev;
            ReleaseGCArena(rt, a);
            METER(nkilledarenas++);
        } else {
#ifdef JS_GCMETER
            jsdouble *thing = reinterpret_cast<jsdouble *>(a->toPageStart());
            jsdouble *end = thing + DOUBLES_PER_ARENA;
            for (; thing != end; ++thing) {
                if (IsMarkedGCThing(thing))
                    METER(nthings++);
            }
            METER(nlivearenas++);
#endif
            ap = &ainfo->prev;
        }
    }
    METER(UpdateArenaStats(&rt->gcStats.doubleArenaStats,
                           nlivearenas, nkilledarenas, nthings));
    rt->gcDoubleArenaList.cursor = rt->gcDoubleArenaList.head;
}

#ifdef JS_THREADSAFE

namespace js {

JS_FRIEND_API(void)
BackgroundSweepTask::replenishAndFreeLater(void *ptr)
{
    JS_ASSERT(freeCursor == freeCursorEnd);
    do {
        if (freeCursor && !freeVector.append(freeCursorEnd - FREE_ARRAY_LENGTH))
            break;
        freeCursor = (void **) js_malloc(FREE_ARRAY_SIZE);
        if (!freeCursor) {
            freeCursorEnd = NULL;
            break;
        }
        freeCursorEnd = freeCursor + FREE_ARRAY_LENGTH;
        *freeCursor++ = ptr;
        return;
    } while (false);
    js_free(ptr);
}

void
BackgroundSweepTask::run()
{
    if (freeCursor) {
        void **array = freeCursorEnd - FREE_ARRAY_LENGTH;
        freeElementsAndArray(array, freeCursor);
        freeCursor = freeCursorEnd = NULL;
    } else {
        JS_ASSERT(!freeCursorEnd);
    }
    for (void ***iter = freeVector.begin(); iter != freeVector.end(); ++iter) {
        void **array = *iter;
        freeElementsAndArray(array, array + FREE_ARRAY_LENGTH);
    }
}

}

#endif /* JS_THREADSAFE */

static void
SweepCompartments(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    JSCompartment **read = rt->compartments.begin();
    JSCompartment **end = rt->compartments.end();
    JSCompartment **write = read;
    while (read < end) {
        JSCompartment *compartment = (*read++);
        if (compartment->marked) {
            compartment->marked = false;
            *write++ = compartment;
            /* Remove dead wrappers from the compartment map. */
            compartment->sweep(cx);
        } else {
            if (compartment->principals)
                JSPRINCIPALS_DROP(cx, compartment->principals);
            delete compartment;
        }
    }
    rt->compartments.resize(write - rt->compartments.begin());
}

/*
 * Common cache invalidation and so forth that must be done before GC. Even if
 * GCUntilDone calls GC several times, this work only needs to be done once.
 */
static void
PreGCCleanup(JSContext *cx, JSGCInvocationKind gckind)
{
    JSRuntime *rt = cx->runtime;

    /* Clear gcIsNeeded now, when we are about to start a normal GC cycle. */
    rt->gcIsNeeded = JS_FALSE;

    /* Reset malloc counter. */
    rt->resetGCMallocBytes();

#ifdef JS_DUMP_SCOPE_METERS
    {
        extern void js_DumpScopeMeters(JSRuntime *rt);
        js_DumpScopeMeters(rt);
    }
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
        rt->shapeGen = JSScope::LAST_RESERVED_SHAPE;
        rt->protoHazardShape = 0;
    }

    js_PurgeThreads(cx);
    {
        JSContext *iter = NULL;
        while (JSContext *acx = js_ContextIterator(rt, JS_TRUE, &iter))
            acx->purge();
    }

    JS_CLEAR_WEAK_ROOTS(&cx->weakRoots);
}

/*
 * Perform mark-and-sweep GC.
 *
 * In a JS_THREADSAFE build, the calling thread must be rt->gcThread and each
 * other thread must be either outside all requests or blocked waiting for GC
 * to finish. Note that the caller does not hold rt->gcLock.
 */
static void
GC(JSContext *cx  GCTIMER_PARAM)
{
    JSRuntime *rt = cx->runtime;
    rt->gcNumber++;
    JS_ASSERT(!rt->gcUnmarkedArenaStackTop);
    JS_ASSERT(rt->gcMarkLaterCount == 0);

    /*
     * Mark phase.
     */
    JSTracer trc;
    JS_TRACER_INIT(&trc, cx, NULL);
    rt->gcMarkingTracer = &trc;
    JS_ASSERT(IS_GC_MARKING_TRACER(&trc));

    for (JSGCChunkInfo **i = rt->gcChunks.begin(); i != rt->gcChunks.end(); ++i)
        (*i)->clearMarkBitmap();

    js_TraceRuntime(&trc);
    js_MarkScriptFilenames(rt);

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
    JS_ASSERT(!cx->gcSweepTask);
    if (!rt->gcHelperThread.busy())
        cx->gcSweepTask = new js::BackgroundSweepTask();
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
    TIMESTAMP(startSweep);
    js_SweepAtomState(cx);

    /* Finalize watch points associated with unreachable objects. */
    js_SweepWatchPoints(cx);

#ifdef DEBUG
    /* Save the pre-sweep count of scope-mapped properties. */
    rt->liveScopePropsPreSweep = rt->liveScopeProps;
#endif

    /*
     * We finalize iterators before other objects so the iterator can use the
     * object which properties it enumerates over to finalize the enumeration
     * state. We finalize objects before string, double and other GC things
     * things to ensure that object's finalizer can access them even if they
     * will be freed.
     *
     * To minimize the number of checks per each to be freed object and
     * function we use separated list finalizers when a debug hook is
     * installed.
     */
    JS_ASSERT(!rt->gcEmptyArenaList);
    if (!cx->debugHooks->objectHook) {
        FinalizeArenaList<JSObject, FinalizeObject>(cx, FINALIZE_OBJECT);
        FinalizeArenaList<JSFunction, FinalizeFunction>(cx, FINALIZE_FUNCTION);
    } else {
        FinalizeArenaList<JSObject, FinalizeHookedObject>(cx, FINALIZE_OBJECT);
        FinalizeArenaList<JSFunction, FinalizeHookedFunction>(cx, FINALIZE_FUNCTION);
    }
#if JS_HAS_XML_SUPPORT
    FinalizeArenaList<JSXML, FinalizeXML>(cx, FINALIZE_XML);
#endif
    TIMESTAMP(sweepObjectEnd);

    /*
     * We sweep the deflated cache before we finalize the strings so the
     * cache can safely use js_IsAboutToBeFinalized..
     */
    rt->deflatedStringCache->sweep(cx);

    FinalizeArenaList<JSString, FinalizeString>(cx, FINALIZE_STRING);
    for (unsigned i = FINALIZE_EXTERNAL_STRING0;
         i <= FINALIZE_EXTERNAL_STRING_LAST;
         ++i) {
        FinalizeArenaList<JSString, FinalizeExternalString>(cx, i);
    }
    TIMESTAMP(sweepStringEnd);

    SweepDoubles(rt);
    TIMESTAMP(sweepDoubleEnd);

    SweepCompartments(cx);

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
    FreeGCChunks(rt);
    TIMESTAMP(sweepDestroyEnd);

#ifdef JS_THREADSAFE
    if (cx->gcSweepTask) {
        rt->gcHelperThread.schedule(cx->gcSweepTask);
        cx->gcSweepTask = NULL;
    }
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
}

#ifdef JS_THREADSAFE

/*
 * If the GC is running and we're called on another thread, wait for this GC
 * activation to finish. We can safely wait here without fear of deadlock (in
 * the case where we are called within a request on another thread's context)
 * because the GC doesn't set rt->gcRunning until after it has waited for all
 * active requests to end.
 *
 * We call here js_CurrentThreadId() after checking for rt->gcState to avoid
 * an expensive call when the GC is not running.
 */
void
js_WaitForGC(JSRuntime *rt)
{
    if (rt->gcRunning && rt->gcThread->id != js_CurrentThreadId()) {
        do {
            JS_AWAIT_GC_DONE(rt);
        } while (rt->gcRunning);
    }
}

/*
 * GC is running on another thread. Temporarily suspend all requests running
 * on the current thread and wait until the GC is done.
 */
static void
LetOtherGCFinish(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    JS_ASSERT(rt->gcThread);
    JS_ASSERT(cx->thread != rt->gcThread);

    size_t requestDebit = cx->thread->contextsInRequests;
    JS_ASSERT(requestDebit <= rt->requestCount);
#ifdef JS_TRACER
    JS_ASSERT_IF(requestDebit == 0, !JS_ON_TRACE(cx));
#endif
    if (requestDebit != 0) {
#ifdef JS_TRACER
        if (JS_ON_TRACE(cx)) {
            /*
             * Leave trace before we decrease rt->requestCount and notify the
             * GC. Otherwise the GC may start immediately after we unlock while
             * this thread is still on trace.
             */
            AutoUnlockGC unlock(rt);
            LeaveTrace(cx);
        }
#endif
        rt->requestCount -= requestDebit;
        if (rt->requestCount == 0)
            JS_NOTIFY_REQUEST_DONE(rt);
    }

    /* See comments before another call to js_ShareWaitingTitles below. */
    cx->thread->gcWaiting = true;
    js_ShareWaitingTitles(cx);

    /*
     * Check that we did not release the GC lock above and let the GC to
     * finish before we wait.
     */
    JS_ASSERT(rt->gcThread);
    JS_THREAD_DATA(cx)->conservativeGC.enable(true);

    /*
     * Wait for GC to finish on the other thread, even if requestDebit is 0
     * and even if GC has not started yet because the gcThread is waiting in
     * BeginGCSession. This ensures that js_GC never returns without a full GC
     * cycle happening.
     */
    do {
        JS_AWAIT_GC_DONE(rt);
    } while (rt->gcThread);

    JS_THREAD_DATA(cx)->conservativeGC.disable();
    cx->thread->gcWaiting = false;
    rt->requestCount += requestDebit;
}

#endif

/*
 * Start a new GC session assuming no GC is running on this or other threads.
 * Together with LetOtherGCFinish this function contains the rendezvous
 * algorithm by which we stop the world for GC.
 *
 * This thread becomes the GC thread. Wait for all other threads to quiesce.
 * Then set rt->gcRunning and return. The caller must call EndGCSession when
 * GC work is done.
 */
static void
BeginGCSession(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    JS_ASSERT(!rt->gcRunning);

#ifdef JS_THREADSAFE
    /* No other thread is in GC, so indicate that we're now in GC. */
    JS_ASSERT(!rt->gcThread);
    rt->gcThread = cx->thread;

    /*
     * Notify all operation callbacks, which will give them a chance to yield
     * their current request. Contexts that are not currently executing will
     * perform their callback at some later point, which then will be
     * unnecessary, but harmless.
     */
    js_NudgeOtherContexts(cx);

    /*
     * Discount all the requests on the current thread from contributing to
     * rt->requestCount before we wait for all other requests to finish.
     * JS_NOTIFY_REQUEST_DONE, which will wake us up, is only called on
     * rt->requestCount transitions to 0.
     */
    size_t requestDebit = cx->thread->contextsInRequests;
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

#endif /* JS_THREADSAFE */

    /*
     * Set rt->gcRunning here within the GC lock, and after waiting for any
     * active requests to end. This way js_WaitForGC called outside a request
     * would not block on the GC that is waiting for other requests to finish
     * with rt->gcThread set while JS_BeginRequest would do such wait.
     */
    rt->gcRunning = true;
}

/* End the current GC session and allow other threads to proceed. */
static void
EndGCSession(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;

    rt->gcRunning = false;
#ifdef JS_THREADSAFE
    JS_ASSERT(rt->gcThread == cx->thread);
    rt->gcThread = NULL;
    JS_NOTIFY_GC_DONE(rt);
#endif
}

/*
 * GC, repeatedly if necessary, until we think we have not created any new
 * garbage and no other threads are demanding more GC.
 */
static void
GCUntilDone(JSContext *cx, JSGCInvocationKind gckind  GCTIMER_PARAM)
{
    if (JS_ON_TRACE(cx))
        return;

    JSRuntime *rt = cx->runtime;

    /* Recursive GC or a call from another thread restarts the GC cycle. */
#ifndef JS_THREADSAFE
    if (rt->gcRunning) {
        rt->gcPoke = true;
        return;
    }
#else /* JS_THREADSAFE */
    if (rt->gcThread) {
        rt->gcPoke = true;
        if (cx->thread == rt->gcThread) {
            JS_ASSERT(rt->gcRunning);
            return;
        }
        LetOtherGCFinish(cx);

        /*
         * Check if the GC on another thread have collected the garbage and
         * it was not a set slot request.
         */
        if (!rt->gcPoke)
            return;
    }
#endif /* JS_THREADSAFE */

    BeginGCSession(cx);

    METER(rt->gcStats.poke++);

    /*
     * Do not scan the current thread on the shutdown or when the GC is called
     * outside a request.
     */
    bool scanGCThreadStack =
#ifdef JS_THREADSAFE
                             (cx->thread->contextsInRequests != 0) &&
#endif
                             (rt->state != JSRTS_LANDING);
    if (scanGCThreadStack)
        JS_THREAD_DATA(cx)->conservativeGC.enable(true);
    bool firstRun = true;
    do {
        rt->gcPoke = false;

        AutoUnlockGC unlock(rt);
        if (firstRun) {
            PreGCCleanup(cx, gckind);
            TIMESTAMP(startMark);
            firstRun = false;
        }
        GC(cx  GCTIMER_ARG);

        // GC again if:
        //   - another thread, not in a request, called js_GC
        //   - js_GC was called recursively
        //   - a finalizer called js_RemoveRoot or js_UnlockGCThingRT.
    } while (rt->gcPoke);

    if (scanGCThreadStack)
        JS_THREAD_DATA(cx)->conservativeGC.disable();

    rt->gcRegenShapes = false;
    rt->setGCLastBytes(rt->gcBytes);

    EndGCSession(cx);
}

/*
 * The gckind flag bit GC_LOCK_HELD indicates a call from js_NewGCThing with
 * rt->gcLock already held, so the lock should be kept on return.
 */
void
js_GC(JSContext *cx, JSGCInvocationKind gckind)
{
    JSRuntime *rt = cx->runtime;

    /*
     * Don't collect garbage if the runtime isn't up, and cx is not the last
     * context in the runtime.  The last context must force a GC, and nothing
     * should suppress that final collection or there may be shutdown leaks,
     * or runtime bloat until the next context is created.
     */
    if (rt->state != JSRTS_UP && gckind != GC_LAST_CONTEXT)
        return;

    GCTIMER_BEGIN();

    do {
        /*
         * Let the API user decide to defer a GC if it wants to (unless this
         * is the last context).  Invoke the callback regardless. Sample the
         * callback in case we are freely racing with a JS_SetGCCallback{,RT}
         * on another thread.
         */
        if (JSGCCallback callback = rt->gcCallback) {
            Conditionally<AutoUnlockGC> unlockIf(!!(gckind & GC_LOCK_HELD), rt);
            if (!callback(cx, JSGC_BEGIN) && gckind != GC_LAST_CONTEXT)
                return;
        }

        {
            /* Lock out other GC allocator and collector invocations. */
            Conditionally<AutoLockGC> lockIf(!(gckind & GC_LOCK_HELD), rt);

            GCUntilDone(cx, gckind  GCTIMER_ARG);
        }

        /* We re-sample the callback again as the finalizers can change it. */
        if (JSGCCallback callback = rt->gcCallback) {
            Conditionally<AutoUnlockGC> unlockIf(gckind & GC_LOCK_HELD, rt);

            (void) callback(cx, JSGC_END);
        }

        /*
         * On shutdown, iterate until the JSGC_END callback stops creating
         * garbage.
         */
    } while (gckind == GC_LAST_CONTEXT && rt->gcPoke);

    GCTIMER_END(gckind == GC_LAST_CONTEXT);
}

namespace js {

bool
SetProtoCheckingForCycles(JSContext *cx, JSObject *obj, JSObject *proto)
{
    JSRuntime *rt = cx->runtime;

    /*
     * This function cannot be called during the GC and always requires a
     * request.
     */
#ifdef JS_THREADSAFE
    JS_ASSERT(cx->requestDepth);
#endif

    AutoLockGC lock(rt);

    /*
     * The set slot request cannot be called recursively and must not be
     * called during a normal GC. So if at this point JSRuntime::gcThread is
     * set it must be a GC or a set slot request from another thread.
     */
#ifdef JS_THREADSAFE
    if (rt->gcThread) {
        JS_ASSERT(cx->thread != rt->gcThread);
        LetOtherGCFinish(cx);
    }
#endif

    BeginGCSession(cx);

    bool cycle;
    {
        AutoUnlockGC unlock(rt);

        cycle = false;
        for (JSObject *obj2 = proto; obj2;) {
            obj2 = obj2->wrappedObject(cx);
            if (obj2 == obj) {
                cycle = true;
                break;
            }
            obj2 = obj2->getProto();
        }
        if (!cycle)
            obj->setProto(proto);
    }

    EndGCSession(cx);

    return !cycle;
}

JSCompartment *
NewCompartment(JSContext *cx, JSPrincipals *principals)
{
    JSRuntime *rt = cx->runtime;
    JSCompartment *compartment = new JSCompartment(rt);
    if (!compartment || !compartment->init()) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    if (principals) {
        compartment->principals = principals;
        JSPRINCIPALS_HOLD(cx, principals);
    }

    AutoLockGC lock(rt);

    if (!rt->compartments.append(compartment)) {
        AutoUnlockGC unlock(rt);
        JS_ReportOutOfMemory(cx);
        return false;
    }

    return compartment;
}

}
