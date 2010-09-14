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
#include "jstracer.h"
#include "methodjit/MethodJIT.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#include "jsprobes.h"
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
 * Check that JSTRACE_XML follows JSTRACE_OBJECT and JSTRACE_STRING.
 */
JS_STATIC_ASSERT(JSTRACE_OBJECT == 0);
JS_STATIC_ASSERT(JSTRACE_STRING == 1);
JS_STATIC_ASSERT(JSTRACE_XML    == 2);

/*
 * JS_IS_VALID_TRACE_KIND assumes that JSTRACE_STRING is the last non-xml
 * trace kind when JS_HAS_XML_SUPPORT is false.
 */
JS_STATIC_ASSERT(JSTRACE_STRING + 1 == JSTRACE_XML);

/*
 * Check consistency of external string constants from JSFinalizeGCThingKind.
 */
JS_STATIC_ASSERT(FINALIZE_EXTERNAL_STRING_LAST - FINALIZE_EXTERNAL_STRING0 ==
                 JS_EXTERNAL_STRING_LIMIT - 1);

/*
 * GC memory is allocated in chunks. The size of each chunk is GC_CHUNK_SIZE.
 * The chunk contains an array of GC arenas holding GC things, an array of
 * the mark bitmaps for each arena, an array of JSGCArenaInfo arena
 * descriptors, an array of JSGCMarkingDelay descriptors, the GCChunkInfo
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
 * list with the head stored in JSRuntime.gcChunkList. GCChunkInfo contains
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
 * word-aligned free arena bitmap and GCChunkInfo they should fit into the
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
 *       M is the number of words in the chunk without GCChunkInfo or
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
 *    M*B == (GC_CHUNK_SIZE - sizeof(GCChunkInfo)) / sizeof(jsuword) *
 *           JS_BITS_PER_WORD
 *        == (GC_CHUNK_SIZE - sizeof(GCChunkInfo)) * JS_BITS_PER_BYTE
 *
 * since GC_CHUNK_SIZE and sizeof(GCChunkInfo) are at least word-aligned.
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

#ifdef JS_GCMETER
# define METER(x)               ((void) (x))
# define METER_IF(condition, x) ((void) ((condition) && (x)))
#else
# define METER(x)               ((void) 0)
# define METER_IF(condition, x) ((void) 0)
#endif

struct JSGCArenaInfo {
    /*
     * Allocation list for the arena.
     */
    JSGCArenaList   *list;

    /*
     * Pointer to the previous arena in a linked list. The arena can either
     * belong to one of JSContext.gcArenaList lists or, when it does not have
     * any allocated GC things, to the list of free arenas in the chunk with
     * head stored in GCChunkInfo.lastFreeArena.
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

namespace js {

struct GCChunkInfo {
    JSRuntime       *runtime;
    size_t          numFreeArenas;
    size_t          gcChunkAge;

    inline void init(JSRuntime *rt);

    inline jsbitmap *getFreeArenaBitmap();

    inline jsuword getChunk();

    inline void clearMarkBitmap();

    static inline GCChunkInfo *fromChunk(jsuword chunk);
};

} /* namespace js */

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
    (GC_CHUNK_SIZE - sizeof(GCChunkInfo)) * JS_BITS_PER_BYTE /
    (JS_BITS_PER_WORD * GC_ARENA_ALL_WORDS + 1);

const size_t GC_FREE_ARENA_BITMAP_WORDS = (GC_ARENAS_PER_CHUNK +
                                           JS_BITS_PER_WORD - 1) /
                                          JS_BITS_PER_WORD;

const size_t GC_FREE_ARENA_BITMAP_SIZE = GC_FREE_ARENA_BITMAP_WORDS *
                                         sizeof(jsuword);

/* Check that GC_ARENAS_PER_CHUNK indeed maximises (1). */
JS_STATIC_ASSERT(GC_ARENAS_PER_CHUNK * GC_ARENA_ALL_WORDS +
                 GC_FREE_ARENA_BITMAP_WORDS <=
                 (GC_CHUNK_SIZE - sizeof(GCChunkInfo)) / sizeof(jsuword));

JS_STATIC_ASSERT((GC_ARENAS_PER_CHUNK + 1) * GC_ARENA_ALL_WORDS +
                 (GC_ARENAS_PER_CHUNK + 1 + JS_BITS_PER_WORD - 1) /
                 JS_BITS_PER_WORD >
                 (GC_CHUNK_SIZE - sizeof(GCChunkInfo)) / sizeof(jsuword));


const size_t GC_MARK_BITMAP_ARRAY_OFFSET = GC_ARENAS_PER_CHUNK
                                           << GC_ARENA_SHIFT;

const size_t GC_ARENA_INFO_ARRAY_OFFSET =
    GC_MARK_BITMAP_ARRAY_OFFSET + GC_MARK_BITMAP_SIZE * GC_ARENAS_PER_CHUNK;

const size_t GC_MARKING_DELAY_ARRAY_OFFSET =
    GC_ARENA_INFO_ARRAY_OFFSET + sizeof(JSGCArenaInfo) * GC_ARENAS_PER_CHUNK;

const size_t GC_CHUNK_INFO_OFFSET = GC_CHUNK_SIZE - GC_FREE_ARENA_BITMAP_SIZE -
                                    sizeof(GCChunkInfo);

inline jsuword
GCChunkInfo::getChunk() {
    jsuword addr = reinterpret_cast<jsuword>(this);
    JS_ASSERT((addr & GC_CHUNK_MASK) == GC_CHUNK_INFO_OFFSET);
    jsuword chunk = addr & ~GC_CHUNK_MASK;
    return chunk;
}

inline void
GCChunkInfo::clearMarkBitmap()
{
    PodZero(reinterpret_cast<jsbitmap *>(getChunk() + GC_MARK_BITMAP_ARRAY_OFFSET),
            GC_MARK_BITMAP_WORDS * GC_ARENAS_PER_CHUNK);
}

/* static */
inline GCChunkInfo *
GCChunkInfo::fromChunk(jsuword chunk) {
    JS_ASSERT(!(chunk & GC_CHUNK_MASK));
    jsuword addr = chunk | GC_CHUNK_INFO_OFFSET;
    return reinterpret_cast<GCChunkInfo *>(addr);
}

inline jsbitmap *
GCChunkInfo::getFreeArenaBitmap()
{
    jsuword addr = reinterpret_cast<jsuword>(this);
    return reinterpret_cast<jsbitmap *>(addr + sizeof(GCChunkInfo));
}

inline void
GCChunkInfo::init(JSRuntime *rt)
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

/*
 * Live objects are marked black. How many other additional colors are available
 * depends on the size of the GCThing.
 */
static const uint32 BLACK = 0;

static void
AssertValidColor(void *thing, uint32 color)
{
    JS_ASSERT_IF(color, color < JSGCArenaInfo::fromGCThing(thing)->list->thingSize / GC_CELL_SIZE);
}

inline bool
IsMarkedGCThing(void *thing, uint32 color = BLACK)
{
    AssertValidColor(thing, color);

    size_t index;
    jsbitmap *markBitmap = GetGCThingMarkBit(thing, index);
    return !!JS_TEST_BIT(markBitmap, index + color);
}

/*
 * The GC always marks live objects BLACK. If color is not BLACK, we also mark
 * the object with that additional color.
 */
inline bool
MarkIfUnmarkedGCThing(void *thing, uint32 color = BLACK)
{
    AssertValidColor(thing, color);

    size_t index;
    jsbitmap *markBitmap = GetGCThingMarkBit(thing, index);
    if (JS_TEST_BIT(markBitmap, index))
        return false;
    JS_SET_BIT(markBitmap, index);
    if (color != BLACK)
        JS_SET_BIT(markBitmap, index + color);
    return true;
}

size_t
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
struct JSGCThing {
    JSGCThing   *link;
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

inline jsuword
GetGCChunk(JSRuntime *rt)
{
    void *p = rt->gcChunkAllocator->alloc();
#ifdef MOZ_GCTIMER
    if (p)
        JS_ATOMIC_INCREMENT(&newChunkCount);
#endif
    METER_IF(p, rt->gcStats.nchunks++);
    METER_UPDATE_MAX(rt->gcStats.maxnchunks, rt->gcStats.nchunks);
    return reinterpret_cast<jsuword>(p);
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
    if (!JS_THREAD_DATA(cx)->waiveGCQuota &&
        (rt->gcBytes >= rt->gcMaxBytes ||
        rt->gcBytes > GC_HEAP_GROWTH_FACTOR * rt->gcNewArenaTriggerBytes)) {
        /*
         * FIXME bug 524051 We cannot run a last-ditch GC on trace for now, so
         * just pretend we are out of memory which will throw us off trace and
         * we will re-try this code path from the interpreter.
         */
        if (!JS_ON_TRACE(cx))
            return NULL;
        TriggerGC(rt);
    }

    if (rt->gcFreeArenaChunks.empty()) {
#ifdef DEBUG
        for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront())
            JS_ASSERT(GCChunkInfo::fromChunk(r.front())->numFreeArenas == 0);
#endif
        /*
         * Make sure that after the GC we can append all allocated chunks to
         * gcFreeArenaChunks.
         *
         * FIXME bug 583729 - use the same for the rt->gcChunkSet.
         */
        if (!rt->gcFreeArenaChunks.reserve(rt->gcChunkSet.count() + 1))
            return NULL;
        jsuword chunk = GetGCChunk(rt);
        if (!chunk)
            return NULL;
        GCChunkInfo *ci = GCChunkInfo::fromChunk(chunk);
        ci->init(rt);

        /*
         * FIXME bug 583732 - chunk is newly allocated and cannot present in
         * the table so using ordinary lookupForAdd is suboptimal here.
         */
        GCChunkSet::AddPtr p = rt->gcChunkSet.lookupForAdd(chunk);
        JS_ASSERT(!p);
        if (!rt->gcChunkSet.add(p, chunk)) {
            ReleaseGCChunk(rt, chunk);
            return NULL;
        }
        JS_ALWAYS_TRUE(rt->gcFreeArenaChunks.append(ci));
    }

    GCChunkInfo *ci = rt->gcFreeArenaChunks.back();
    JS_ASSERT(ci->numFreeArenas);

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
    if (ci->numFreeArenas == 0) {
        JS_ASSERT(ci == rt->gcFreeArenaChunks.back());
        rt->gcFreeArenaChunks.popBack();
    }

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
    GCChunkInfo *ci = GCChunkInfo::fromChunk(chunk);
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

    /* Remove unused chunks and rebuild gcFreeArenaChunks. */
    rt->gcFreeArenaChunks.clear();
    JS_ASSERT(rt->gcFreeArenaChunks.capacity() >= rt->gcChunkSet.count());
    for (GCChunkSet::Enum e(rt->gcChunkSet); !e.empty(); e.popFront()) {
        GCChunkInfo *ci = GCChunkInfo::fromChunk(e.front());
        JS_ASSERT(ci->runtime == rt);
        if (ci->numFreeArenas == GC_ARENAS_PER_CHUNK) {
            if (ci->gcChunkAge > GC_MAX_CHUNK_AGE) {
                e.removeFront();
                ReleaseGCChunk(rt, ci->getChunk());
                continue;
            }
            ci->gcChunkAge++;
        }

        if (ci->numFreeArenas)
            JS_ALWAYS_TRUE(rt->gcFreeArenaChunks.append(ci));
    }
}

static inline size_t
GetFinalizableThingSize(unsigned thingKind)
{
    JS_STATIC_ASSERT(JS_EXTERNAL_STRING_LIMIT == 8);

    static const uint8 map[FINALIZE_LIMIT] = {
        sizeof(JSObject),      /* FINALIZE_OBJECT */
        sizeof(JSFunction),    /* FINALIZE_FUNCTION */
#if JS_HAS_XML_SUPPORT
        sizeof(JSXML),         /* FINALIZE_XML */
#endif
        sizeof(JSShortString), /* FINALIZE_SHORT_STRING */
        sizeof(JSString),      /* FINALIZE_STRING */
        sizeof(JSString),      /* FINALIZE_EXTERNAL_STRING0 */
        sizeof(JSString),      /* FINALIZE_EXTERNAL_STRING1 */
        sizeof(JSString),      /* FINALIZE_EXTERNAL_STRING2 */
        sizeof(JSString),      /* FINALIZE_EXTERNAL_STRING3 */
        sizeof(JSString),      /* FINALIZE_EXTERNAL_STRING4 */
        sizeof(JSString),      /* FINALIZE_EXTERNAL_STRING5 */
        sizeof(JSString),      /* FINALIZE_EXTERNAL_STRING6 */
        sizeof(JSString),      /* FINALIZE_EXTERNAL_STRING7 */
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
#endif
        JSTRACE_STRING,     /* FINALIZE_SHORT_STRING */
        JSTRACE_STRING,     /* FINALIZE_STRING */
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
}

static void
FinishGCArenaLists(JSRuntime *rt)
{
    for (unsigned i = 0; i < FINALIZE_LIMIT; i++) {
        rt->gcArenaList[i].head = NULL;
        rt->gcArenaList[i].cursor = NULL;
    }

    rt->gcBytes = 0;

    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront())
        ReleaseGCChunk(rt, r.front());
    rt->gcChunkSet.clear();
    rt->gcFreeArenaChunks.clear();
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
    return GCChunkInfo::fromChunk(chunk)->runtime;
}

JS_FRIEND_API(bool)
js_IsAboutToBeFinalized(void *thing)
{
    if (JSString::isStatic(thing))
        return false;

    return !IsMarkedGCThing(thing);
}

JS_FRIEND_API(bool)
js_GCThingIsMarked(void *thing, uint32 color)
{
    return IsMarkedGCThing(thing, color);
}

JSBool
js_InitGC(JSRuntime *rt, uint32 maxbytes)
{
    InitGCArenaLists(rt);

    /*
     * Make room for at least 16 chunks so the table would not grow before
     * the browser starts up.
     */
    if (!rt->gcChunkSet.init(16))
        return false;

    if (!rt->gcRootsHash.init(256))
        return false;

    if (!rt->gcLocksHash.init(256))
        return false;

#ifdef JS_THREADSAFE
    rt->gcLock = JS_NEW_LOCK();
    if (!rt->gcLock)
        return false;
    rt->gcDone = JS_NEW_CONDVAR(rt->gcLock);
    if (!rt->gcDone)
        return false;
    rt->requestDone = JS_NEW_CONDVAR(rt->gcLock);
    if (!rt->requestDone)
        return false;
    if (!rt->gcHelperThread.init(rt))
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
    rt->gcNewArenaTriggerBytes = GC_ARENA_ALLOCATION_TRIGGER;

    METER(PodZero(&rt->gcStats));
    return true;
}

namespace js {

/*
 * Returns CGCT_VALID if the w can be a live GC thing and sets thing and traceKind
 * accordingly. Otherwise returns the reason for rejection.
 */
inline ConservativeGCTest
IsGCThingWord(JSRuntime *rt, jsuword w, void *&thing, uint32 &traceKind)
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

    /*
     * We assume that the compiler never uses sub-word alignment to store
     * pointers and does not tag pointers on its own. Additionally, the value
     * representation for all values and the jsid representation for GC-things
     * do not touch the low two bits. Thus any word with the low two bits set
     * is not a valid GC-thing.
     */
    JS_STATIC_ASSERT(JSID_TYPE_STRING == 0 && JSID_TYPE_OBJECT == 4);
    if (w & 0x3)
        return CGCT_LOWBITSET;

    /*
     * An object jsid has its low bits tagged. In the value representation on
     * 64-bit, the high bits are tagged.
     */
    const jsuword JSID_PAYLOAD_MASK = ~jsuword(JSID_TYPE_MASK);
#if JS_BITS_PER_WORD == 32
    jsuword payload = w & JSID_PAYLOAD_MASK;
#elif JS_BITS_PER_WORD == 64
    jsuword payload = w & JSID_PAYLOAD_MASK & JSVAL_PAYLOAD_MASK;
#endif

    jsuword chunk = payload & ~GC_CHUNK_MASK;
    if (!rt->gcChunkSet.has(chunk))
        return CGCT_NOTCHUNK;

    GCChunkInfo *ci = GCChunkInfo::fromChunk(chunk);

    if ((payload & GC_CHUNK_MASK) >= GC_MARK_BITMAP_ARRAY_OFFSET)
        return CGCT_NOTARENA;

    size_t arenaIndex = (payload & GC_CHUNK_MASK) >> GC_ARENA_SHIFT;
    if (JS_TEST_BIT(ci->getFreeArenaBitmap(), arenaIndex))
        return CGCT_FREEARENA;

    JSGCArena *a = JSGCArena::fromChunkAndIndex(chunk, arenaIndex);
    JSGCArenaInfo *ainfo = a->getInfo();

    traceKind = GetFinalizableArenaTraceKind(ainfo);

    /*
     * On 64-bit we might consider using the tag bits in w to disqualify
     * additional false roots, however, the condition would have to look
     * something like:
     *
     *   if ((traceKind == JSTRACE_STRING && tag > 0 && tag != JSVAL_TAG_SHIFT) ||
     *       (traceKind == JSTRACE_OBJECT && tag > 0 && tag != JSVAL_TAG_OBJECT))
     *     return CGCT_WRONGTAG;
     *
     * However, it seems like we should measure how often this actually avoids
     * false roots.
     */

    jsuword start = a->toPageStart();
    jsuword offset = payload - start;
    size_t thingSize = ainfo->list->thingSize;
    offset -= offset % thingSize;

    /*
     * If GC_ARENA_SIZE % thingSize != 0 or when thingSize is not a power
     * of two, thingSize-aligned pointer may point at the end of the last
     * thing yet be inside the arena.
     */
    if (offset + thingSize > GC_ARENA_SIZE) {
        JS_ASSERT(thingSize & (thingSize - 1));
        return CGCT_NOTARENA;
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
            return CGCT_NOTLIVE;
        JS_ASSERT_IF(cursor->link, cursor < cursor->link);
        cursor = cursor->link;
    }

    return CGCT_VALID;
}

inline ConservativeGCTest
IsGCThingWord(JSRuntime *rt, jsuword w)
{
    void *thing;
    uint32 traceKind;
    return IsGCThingWord(rt, w, thing, traceKind);
}

static void
MarkWordConservatively(JSTracer *trc, jsuword w)
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

    void *thing;
    uint32 traceKind;
    ConservativeGCTest test = IsGCThingWord(trc->context->runtime, w, thing, traceKind);
    if (test == CGCT_VALID) {
        Mark(trc, thing, traceKind, "machine stack");
#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
        if (IS_GC_MARKING_TRACER(trc) && static_cast<GCMarker *>(trc)->conservativeDumpFileName) {
            GCMarker::ConservativeRoot root = {thing, traceKind};
            static_cast<GCMarker *>(trc)->conservativeRoots.append(root);
        }
#endif
    }

#if defined JS_DUMP_CONSERVATIVE_GC_ROOTS || defined JS_GCMETER
    if (IS_GC_MARKING_TRACER(trc))
        static_cast<GCMarker *>(trc)->conservativeStats.counter[test]++;
#endif
}

static void
MarkRangeConservatively(JSTracer *trc, jsuword *begin, jsuword *end)
{
    JS_ASSERT(begin <= end);
    for (jsuword *i = begin; i != end; ++i)
        MarkWordConservatively(trc, *i);
}

static void
MarkThreadDataConservatively(JSTracer *trc, JSThreadData *td)
{
    ConservativeGCThreadData *ctd = &td->conservativeGC;
    JS_ASSERT(ctd->hasStackToScan());
    jsuword *stackMin, *stackEnd;
#if JS_STACK_GROWTH_DIRECTION > 0
    stackMin = td->nativeStackBase;
    stackEnd = ctd->nativeStackTop;
#else
    stackMin = ctd->nativeStackTop + 1;
    stackEnd = td->nativeStackBase;
#endif
    JS_ASSERT(stackMin <= stackEnd);
    MarkRangeConservatively(trc, stackMin, stackEnd);
    MarkRangeConservatively(trc, ctd->registerSnapshot.words,
                            JS_ARRAY_END(ctd->registerSnapshot.words));

}

void
MarkStackRangeConservatively(JSTracer *trc, Value *beginv, Value *endv)
{
    jsuword *begin = (jsuword *) beginv;
    jsuword *end = (jsuword *) endv;
#ifdef JS_NUNBOX32
    /*
     * With 64-bit jsvals on 32-bit systems, we can optimize a bit by
     * scanning only the payloads.
     */
    JS_ASSERT(begin <= end);
    for (jsuword *i = begin; i != end; i += 2)
        MarkWordConservatively(trc, *i);
#else
    MarkRangeConservatively(trc, begin, end);
#endif
}

void
MarkConservativeStackRoots(JSTracer *trc)
{
#ifdef JS_THREADSAFE
    for (JSThread::Map::Range r = trc->context->runtime->threads.all(); !r.empty(); r.popFront()) {
        JSThread *thread = r.front().value;
        ConservativeGCThreadData *ctd = &thread->data.conservativeGC;
        if (ctd->hasStackToScan()) {
            JS_ASSERT_IF(!thread->requestDepth, thread->suspendCount);
            MarkThreadDataConservatively(trc, &thread->data);
        } else {
            JS_ASSERT(!thread->suspendCount);
            JS_ASSERT(thread->requestDepth <= ctd->requestThreshold);
        }
    }
#else
    MarkThreadDataConservatively(trc, &trc->context->runtime->threadData);
#endif
}

JS_NEVER_INLINE void
ConservativeGCThreadData::recordStackTop()
{
    /* Update the native stack pointer if it points to a bigger stack. */
    jsuword dummy;
    nativeStackTop = &dummy;

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

static inline void
RecordNativeStackTopForGC(JSContext *cx)
{
    ConservativeGCThreadData *ctd = &JS_THREAD_DATA(cx)->conservativeGC;

#ifdef JS_THREADSAFE
    /* Record the stack top here only if we are called from a request. */
    JS_ASSERT(cx->thread->requestDepth >= ctd->requestThreshold);
    if (cx->thread->requestDepth == ctd->requestThreshold)
        return;
#endif
    ctd->recordStackTop();
}

} /* namespace js */

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
    rt->gcHelperThread.finish(rt);
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
js_AddRoot(JSContext *cx, Value *vp, const char *name)
{
    JSBool ok = js_AddRootRT(cx->runtime, Jsvalify(vp), name);
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

JS_FRIEND_API(JSBool)
js_AddRootRT(JSRuntime *rt, jsval *vp, const char *name)
{
    /*
     * Due to the long-standing, but now removed, use of rt->gcLock across the
     * bulk of js_GC, API users have come to depend on JS_AddRoot etc. locking
     * properly with a racing GC, without calling JS_AddRoot from a request.
     * We have to preserve API compatibility here, now that we avoid holding
     * rt->gcLock across the mark phase (including the root hashtable mark).
     */
    AutoLockGC lock(rt);
    js_WaitForGC(rt);

    return !!rt->gcRootsHash.put((void *)vp,
                                 RootInfo(name, JS_GC_ROOT_VALUE_PTR));
}

JS_FRIEND_API(JSBool)
js_AddGCThingRootRT(JSRuntime *rt, void **rp, const char *name)
{
    /*
     * Due to the long-standing, but now removed, use of rt->gcLock across the
     * bulk of js_GC, API users have come to depend on JS_AddRoot etc. locking
     * properly with a racing GC, without calling JS_AddRoot from a request.
     * We have to preserve API compatibility here, now that we avoid holding
     * rt->gcLock across the mark phase (including the root hashtable mark).
     */
    AutoLockGC lock(rt);
    js_WaitForGC(rt);

    return !!rt->gcRootsHash.put((void *)rp,
                                 RootInfo(name, JS_GC_ROOT_GCTHING_PTR));
}

JS_FRIEND_API(JSBool)
js_RemoveRoot(JSRuntime *rt, void *rp)
{
    /*
     * Due to the JS_RemoveRootRT API, we may be called outside of a request.
     * Same synchronization drill as above in js_AddRoot.
     */
    AutoLockGC lock(rt);
    js_WaitForGC(rt);
    rt->gcRootsHash.remove(rp);
    rt->gcPoke = JS_TRUE;
    return JS_TRUE;
}

typedef RootedValueMap::Range RootRange;
typedef RootedValueMap::Entry RootEntry;
typedef RootedValueMap::Enum RootEnum;

#ifdef DEBUG

static void
CheckLeakedRoots(JSRuntime *rt)
{
    uint32 leakedroots = 0;

    /* Warn (but don't assert) debug builds of any remaining roots. */
    for (RootRange r = rt->gcRootsHash.all(); !r.empty(); r.popFront()) {
        RootEntry &entry = r.front();
        leakedroots++;
        fprintf(stderr,
                "JS engine warning: leaking GC root \'%s\' at %p\n",
                entry.value.name ? entry.value.name : "", entry.key);
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
                  void (*dump)(const char *name, void *rp, JSGCRootType type, void *data),
                  void *data)
{
    for (RootRange r = rt->gcRootsHash.all(); !r.empty(); r.popFront()) {
        RootEntry &entry = r.front();
        if (const char *name = entry.value.name)
            dump(name, entry.key, entry.value.type, data);
    }
}

#endif /* DEBUG */

uint32
js_MapGCRoots(JSRuntime *rt, JSGCRootMapFun map, void *data)
{
    AutoLockGC lock(rt);
    int ct = 0;
    for (RootEnum e(rt->gcRootsHash); !e.empty(); e.popFront()) {
        RootEntry &entry = e.front();

        ct++;
        intN mapflags = map(entry.key, entry.value.type, entry.value.name, data);

        if (mapflags & JS_MAP_GCROOT_REMOVE)
            e.removeFront();
        if (mapflags & JS_MAP_GCROOT_STOP)
            break;
    }

    return ct;
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
}

void
JSGCFreeLists::moveTo(JSGCFreeLists *another)
{
    *another = *this;
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
        if (rt->gcRunning)
            return NULL;

        bool canGC = !JS_ON_TRACE(cx) && !JS_THREAD_DATA(cx)->waiveGCQuota;
        bool doGC = canGC && IsGCThresholdReached(rt);
        arenaList = &rt->gcArenaList[thingKind];
        for (;;) {
            if (doGC) {
                LastDitchGC(cx);
                METER(cx->runtime->gcArenaStats[thingKind].retry++);
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
                METER(cx->runtime->gcArenaStats[thingKind].fail++);
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
    METER(cx->runtime->gcArenaStats[thingKind].alloc++);

    JSGCThing **freeListp =
        JS_THREAD_DATA(cx)->gcFreeLists.finalizables + thingKind;
    JSGCThing *thing = *freeListp;
    if (thing) {
        *freeListp = thing->link;
        CheckGCFreeListLink(thing);
        METER(cx->runtime->gcArenaStats[thingKind].localalloc++);
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

    return thing;
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
        if (JSObject *proto = obj->getProto())
            JS_CALL_OBJECT_TRACER(trc, proto, "proto");
        if (JSObject *parent = obj->getParent())
            JS_CALL_OBJECT_TRACER(trc, parent, "parent");
        JSTraceOp op = obj->getOps()->trace;
        (op ? op : js_TraceObject)(trc, obj);
        break;
      }

      case JSTRACE_STRING: {
        JSString *str = (JSString *) thing;
        if (str->isDependent())
            JS_CALL_STRING_TRACER(trc, str->dependentBase(), "base");
        else if (str->isRope()) {
            if (str->isInteriorNode())
                JS_CALL_STRING_TRACER(trc, str->interiorNodeParent(), "parent");
            JS_CALL_STRING_TRACER(trc, str->ropeLeft(), "left child");
            JS_CALL_STRING_TRACER(trc, str->ropeRight(), "right child");
        }
        break;
      }

#if JS_HAS_XML_SUPPORT
      case JSTRACE_XML:
        js_TraceXML(trc, (JSXML *)thing);
        break;
#endif
    }
}

namespace js {

/*
 * When the native stack is low, the GC does not call JS_TraceChildren to mark
 * the reachable "children" of the thing. Rather the thing is put aside and
 * JS_TraceChildren is called later with more space on the C stack.
 *
 * To implement such delayed marking of the children with minimal overhead for
 * the normal case of sufficient native stack, the code uses two fields per
 * arena stored in JSGCMarkingDelay. The first field, JSGCMarkingDelay::link,
 * links all arenas with delayed things into a stack list with the pointer to
 * stack top in JSRuntime::gcUnmarkedArenaStackTop. delayMarkingChildren adds
 * arenas to the stack as necessary while markDelayedChildren pops the arenas
 * from the stack until it empties.
 *
 * The second field, JSGCMarkingDelay::unmarkedChildren, is a bitmap that
 * tells for which things the GC should call JS_TraceChildren later. The
 * bitmap is a single word. As such it does not pinpoint the delayed things
 * in the arena but rather tells the intervals containing
 * ThingsPerUnmarkedBit(thingSize) things. Later the code in
 * markDelayedChildren discovers such intervals and calls JS_TraceChildren on
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

GCMarker::GCMarker(JSContext *cx)
  : color(0), unmarkedArenaStackTop(NULL)
{
    JS_TRACER_INIT(this, cx, NULL);
#ifdef DEBUG
    markLaterCount = 0;
#endif
#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    conservativeDumpFileName = getenv("JS_DUMP_CONSERVATIVE_GC_ROOTS");
    memset(&conservativeStats, 0, sizeof(conservativeStats));
#endif
}

GCMarker::~GCMarker()
{
#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    dumpConservativeRoots();
#endif
#ifdef JS_GCMETER
    /* Update total stats. */
    context->runtime->gcStats.conservative.add(conservativeStats);
#endif
}

void
GCMarker::delayMarkingChildren(void *thing)
{
    JS_ASSERT(this == context->runtime->gcMarkingTracer);
    JS_ASSERT(IsMarkedGCThing(thing));
    METER(context->runtime->gcStats.unmarked++);

    JSGCArena *a = JSGCArena::fromGCThing(thing);
    JSGCArenaInfo *ainfo = a->getInfo();
    JSGCMarkingDelay *markingDelay = a->getMarkingDelay();

    size_t thingArenaIndex = GCThingToArenaIndex(thing);
    size_t unmarkedBitIndex = thingArenaIndex /
                              ThingsPerUnmarkedBit(ainfo->list->thingSize);
    JS_ASSERT(unmarkedBitIndex < JS_BITS_PER_WORD);

    jsuword bit = jsuword(1) << unmarkedBitIndex;
    if (markingDelay->unmarkedChildren != 0) {
        JS_ASSERT(unmarkedArenaStackTop);
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
         * See comments in markDelayedChildren.
         */
        markingDelay->unmarkedChildren = bit;
        if (!markingDelay->link) {
            if (!unmarkedArenaStackTop) {
                /* Stack was empty, mark the arena as the bottom element. */
                markingDelay->link = a;
            } else {
                JS_ASSERT(unmarkedArenaStackTop->getMarkingDelay()->link);
                markingDelay->link = unmarkedArenaStackTop;
            }
            unmarkedArenaStackTop = a;
        }
        JS_ASSERT(unmarkedArenaStackTop);
    }
#ifdef DEBUG
    markLaterCount += ThingsPerUnmarkedBit(ainfo->list->thingSize);
    METER_UPDATE_MAX(context->runtime->gcStats.maxunmarked, markLaterCount);
#endif
}

JS_FRIEND_API(void)
GCMarker::markDelayedChildren()
{
    JS_ASSERT(this == context->runtime->gcMarkingTracer);

    JSGCArena *a = unmarkedArenaStackTop;
    if (!a) {
        JS_ASSERT(markLaterCount == 0);
        return;
    }

    for (;;) {
        /*
         * The following assert verifies that the current arena belongs to the
         * unmarked stack, since delayMarkingChildren ensures that even for
         * the stack's bottom, prevUnmarked != 0 but rather points to
         * itself.
         */
        JSGCArenaInfo *ainfo = a->getInfo();
        JSGCMarkingDelay *markingDelay = a->getMarkingDelay();
        JS_ASSERT(markingDelay->link);
        JS_ASSERT(unmarkedArenaStackTop->getMarkingDelay()->link);
        unsigned thingSize = ainfo->list->thingSize;
        unsigned traceKind = GetFinalizableArenaTraceKind(ainfo);
        unsigned indexLimit = ThingsPerArena(thingSize);
        unsigned thingsPerUnmarkedBit = ThingsPerUnmarkedBit(thingSize);

        /*
         * We cannot use do-while loop here as a->unmarkedChildren can be zero
         * before the loop as a leftover from the previous iterations. See
         * comments after the loop.
         */
        while (markingDelay->unmarkedChildren != 0) {
            unsigned unmarkedBitIndex = JS_FLOOR_LOG2W(markingDelay->unmarkedChildren);
            markingDelay->unmarkedChildren &= ~(jsuword(1) << unmarkedBitIndex);
#ifdef DEBUG
            JS_ASSERT(markLaterCount >= thingsPerUnmarkedBit);
            markLaterCount -= thingsPerUnmarkedBit;
#endif
            unsigned thingIndex = unmarkedBitIndex * thingsPerUnmarkedBit;
            unsigned endIndex = thingIndex + thingsPerUnmarkedBit;

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
                    JS_TraceChildren(this, thing, traceKind);
                thing += thingSize;
            } while (thing != end);
        }

        /*
         * We finished tracing of all things in the the arena but we can only
         * pop it from the stack if the arena is the stack's top.
         *
         * When JS_TraceChildren from the above calls JS_CallTracer that in
         * turn on low C stack calls delayMarkingChildren and the latter
         * pushes new arenas to the unmarked stack, we have to skip popping
         * of this arena until it becomes the top of the stack again.
         */
        if (a == unmarkedArenaStackTop) {
            JSGCArena *aprev = markingDelay->link;
            markingDelay->link = NULL;
            if (a == aprev) {
                /*
                 * prevUnmarked points to itself and we reached the bottom of
                 * the stack.
                 */
                break;
            }
            unmarkedArenaStackTop = a = aprev;
        } else {
            a = unmarkedArenaStackTop;
        }
    }
    JS_ASSERT(unmarkedArenaStackTop);
    JS_ASSERT(!unmarkedArenaStackTop->getMarkingDelay()->link);
    unmarkedArenaStackTop = NULL;
    JS_ASSERT(markLaterCount == 0);
}

void
GCMarker::slowifyArrays()
{
    while (!arraysToSlowify.empty()) {
        JSObject *obj = arraysToSlowify.back();
        arraysToSlowify.popBack();
        if (IsMarkedGCThing(obj))
            obj->makeDenseArraySlow(context);
    }
}

void
Mark(JSTracer *trc, void *thing, uint32 kind)
{
    JS_ASSERT(thing);
    JS_ASSERT(JS_IS_VALID_TRACE_KIND(kind));
    JS_ASSERT(trc->debugPrinter || trc->debugPrintArg);
    JS_ASSERT_IF(!JSString::isStatic(thing), kind == GetFinalizableThingTraceKind(thing));
#ifdef DEBUG
    if (IS_GC_MARKING_TRACER(trc)) {
        JSRuntime *rt = trc->context->runtime;
        JS_ASSERT(rt->gcMarkingTracer == trc);
        JS_ASSERT(rt->gcRunning);
    }
#endif

    if (!IS_GC_MARKING_TRACER(trc)) {
        trc->callback(trc, thing, kind);
    } else {
        GCMarker *gcmarker = static_cast<GCMarker *>(trc);

        if (kind == JSTRACE_STRING) {
            /*
             * Optimize for string as their marking is not recursive.
             *
             * Iterate through all nodes and leaves in the rope if this is
             * part of a rope; otherwise, we only iterate once: on the string
             * itself.
             */
            JSRopeNodeIterator iter((JSString *) thing);
            JSString *str = iter.init();
            do {
                for (;;) {
                    if (JSString::isStatic(str))
                        break;
                    JS_ASSERT(kind == GetFinalizableThingTraceKind(str));
                    if (!MarkIfUnmarkedGCThing(str))
                        break;
                    if (!str->isDependent())
                        break;
                    str = str->dependentBase();
                }
                str = iter.next();
            } while (str);

        } else if (MarkIfUnmarkedGCThing(thing, gcmarker->getMarkColor())) {
            /*
             * With JS_GC_ASSUME_LOW_C_STACK defined the mark phase of GC
             * always uses the non-recursive code that otherwise would be
             * called only on a low C stack condition.
             */
#ifdef JS_GC_ASSUME_LOW_C_STACK
# define RECURSION_TOO_DEEP() true
#else
            int stackDummy;
# define RECURSION_TOO_DEEP() (!JS_CHECK_STACK_SIZE(trc->context, stackDummy))
#endif
            if (RECURSION_TOO_DEEP())
                gcmarker->delayMarkingChildren(thing);
            else
                JS_TraceChildren(trc, thing, kind);
        }
    }

#ifdef DEBUG
    trc->debugPrinter = NULL;
    trc->debugPrintArg = NULL;
#endif
}

void
MarkGCThing(JSTracer *trc, void *thing)
{
    JS_ASSERT(size_t(thing) % JS_GCTHING_ALIGN == 0);

    if (!thing)
        return;

    uint32 kind = js_GetGCThingTraceKind(thing);
    Mark(trc, thing, kind);
}

} /* namespace js */

static void
gc_root_traversal(JSTracer *trc, const RootEntry &entry)
{
#ifdef DEBUG
    void *ptr;
    if (entry.value.type == JS_GC_ROOT_GCTHING_PTR) {
        ptr = *reinterpret_cast<void **>(entry.key);
    } else {
        Value *vp = reinterpret_cast<Value *>(entry.key);
        ptr = vp->isGCThing() ? vp->asGCThing() : NULL;
    }

    if (ptr) {
        if (!JSString::isStatic(ptr)) {
            bool root_points_to_gcArenaList = false;
            jsuword thing = (jsuword) ptr;
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
            if (!root_points_to_gcArenaList && entry.value.name) {
                fprintf(stderr,
"JS API usage error: the address passed to JS_AddNamedRoot currently holds an\n"
"invalid gcthing.  This is usually caused by a missing call to JS_RemoveRoot.\n"
"The root's name is \"%s\".\n",
                        entry.value.name);
            }
            JS_ASSERT(root_points_to_gcArenaList);
        }
    }
#endif
    JS_SET_TRACING_NAME(trc, entry.value.name ? entry.value.name : "root");
    if (entry.value.type == JS_GC_ROOT_GCTHING_PTR)
        MarkGCThing(trc, *reinterpret_cast<void **>(entry.key));
    else
        MarkValueRaw(trc, *reinterpret_cast<Value *>(entry.key));
}

static void
gc_lock_traversal(const GCLocks::Entry &entry, JSTracer *trc)
{
    uint32 traceKind;

    JS_ASSERT(entry.value >= 1);
    traceKind = js_GetGCThingTraceKind(entry.key);
    JS_CALL_TRACER(trc, entry.key, traceKind, "locked object");
}

void
js_TraceStackFrame(JSTracer *trc, JSStackFrame *fp)
{
    if (fp->hasCallObj())
        JS_CALL_OBJECT_TRACER(trc, fp->getCallObj(), "call");
    if (fp->hasArgsObj())
        JS_CALL_OBJECT_TRACER(trc, fp->getArgsObj(), "arguments");
    if (fp->hasScript())
        js_TraceScript(trc, fp->getScript());

    /* Allow for primitive this parameter due to JSFUN_THISP_* flags. */
    MarkValue(trc, fp->getThisValue(), "this");
    MarkValue(trc, fp->getReturnValue(), "rval");
    if (fp->hasScopeChain())
        JS_CALL_OBJECT_TRACER(trc, fp->getScopeChain(), "scope chain");
}

inline void
AutoGCRooter::trace(JSTracer *trc)
{
    switch (tag) {
      case JSVAL:
        MarkValue(trc, static_cast<AutoValueRooter *>(this)->val, "js::AutoValueRooter.val");
        return;

      case SHAPE:
        static_cast<AutoShapeRooter *>(this)->shape->trace(trc);
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
        MarkIdRange(trc, ida->length, ida->vector, "js::AutoIdArray.idArray");
        return;
      }

      case DESCRIPTORS: {
        PropDescArray &descriptors =
            static_cast<AutoPropDescArrayRooter *>(this)->descriptors;
        for (size_t i = 0, len = descriptors.length(); i < len; i++) {
            PropDesc &desc = descriptors[i];
            MarkValue(trc, desc.pd, "PropDesc::pd");
            MarkValue(trc, desc.value, "PropDesc::value");
            MarkValue(trc, desc.get, "PropDesc::get");
            MarkValue(trc, desc.set, "PropDesc::set");
            MarkId(trc, desc.id, "PropDesc::id");
        }
        return;
      }

      case DESCRIPTOR : {
        PropertyDescriptor &desc = *static_cast<AutoPropertyDescriptorRooter *>(this);
        if (desc.obj)
            MarkObject(trc, desc.obj, "Descriptor::obj");
        MarkValue(trc, desc.value, "Descriptor::value");
        if ((desc.attrs & JSPROP_GETTER) && desc.getter)
            MarkObject(trc, CastAsObject(desc.getter), "Descriptor::get");
        if (desc.attrs & JSPROP_SETTER && desc.setter)
            MarkObject(trc, CastAsObject(desc.setter), "Descriptor::set");
        return;
      }

      case NAMESPACES: {
        JSXMLArray &array = static_cast<AutoNamespaceArray *>(this)->array;
        MarkObjectRange(trc, array.length, reinterpret_cast<JSObject **>(array.vector),
                        "JSXMLArray.vector");
        array.cursors->trace(trc);
        return;
      }

      case XML:
        js_TraceXML(trc, static_cast<AutoXMLRooter *>(this)->xml);
        return;

      case OBJECT:
        if (JSObject *obj = static_cast<AutoObjectRooter *>(this)->obj)
            MarkObject(trc, obj, "js::AutoObjectRooter.obj");
        return;

      case ID:
        MarkId(trc, static_cast<AutoIdRooter *>(this)->id_, "js::AutoIdRooter.val");
        return;

      case VALVECTOR: {
        Vector<Value, 8> &vector = static_cast<js::AutoValueVector *>(this)->vector;
        MarkValueRange(trc, vector.length(), vector.begin(), "js::AutoValueVector.vector");
        return;
      }

      case STRING:
        if (JSString *str = static_cast<js::AutoStringRooter *>(this)->str)
            MarkString(trc, str, "js::AutoStringRooter.str");
        return;

      case IDVECTOR: {
        Vector<jsid, 8> &vector = static_cast<js::AutoIdVector *>(this)->vector;
        MarkIdRange(trc, vector.length(), vector.begin(), "js::AutoIdVector.vector");
        return;
      }
    }

    JS_ASSERT(tag >= 0);
    MarkValueRange(trc, tag, static_cast<AutoArrayRooter *>(this)->array, "js::AutoArrayRooter.array");
}

namespace js {

void
MarkContext(JSTracer *trc, JSContext *acx)
{
    /* Stack frames and slots are traced by StackSpace::mark. */

    /* Mark other roots-by-definition in acx. */
    if (acx->globalObject && !JS_HAS_OPTION(acx, JSOPTION_UNROOTED_GLOBAL))
        JS_CALL_OBJECT_TRACER(trc, acx->globalObject, "global object");
    if (acx->throwing) {
        MarkValue(trc, acx->exception, "exception");
    } else {
        /* Avoid keeping GC-ed junk stored in JSContext.exception. */
        acx->exception.setNull();
    }

    for (js::AutoGCRooter *gcr = acx->autoGCRooters; gcr; gcr = gcr->down)
        gcr->trace(trc);

    if (acx->sharpObjectMap.depth > 0)
        js_TraceSharpMap(trc, &acx->sharpObjectMap);

    js_TraceRegExpStatics(trc, acx);

    MarkValue(trc, acx->iterValue, "iterValue");

    acx->compartment->marked = true;

#ifdef JS_TRACER
    TracerState* state = acx->tracerState;
    while (state) {
        if (state->nativeVp)
            MarkValueRange(trc, state->nativeVpLen, state->nativeVp, "nativeVp");
        state = state->prev;
    }
#endif
}

JS_REQUIRES_STACK void
MarkRuntime(JSTracer *trc)
{
    JSRuntime *rt = trc->context->runtime;

    if (rt->state != JSRTS_LANDING)
        MarkConservativeStackRoots(trc);

    /*
     * Verify that we do not have at this point unmarked GC things stored in
     * autorooters. To maximize test coverage we abort even in non-debug
     * builds for now, see bug 574313.
     */
    JSContext *iter;
#if 1
    iter = NULL;
    while (JSContext *acx = js_ContextIterator(rt, JS_TRUE, &iter)) {
        for (AutoGCRooter *gcr = acx->autoGCRooters; gcr; gcr = gcr->down) {
#ifdef JS_THREADSAFE
            JS_ASSERT_IF(!acx->thread->requestDepth, acx->thread->suspendCount);
#endif
            JS_ASSERT(JS_THREAD_DATA(acx)->conservativeGC.hasStackToScan());
            void *thing;
            switch (gcr->tag) {
              default:
                continue;
              case AutoGCRooter::JSVAL: {
                const Value &v = static_cast<AutoValueRooter *>(gcr)->val;
                if (!v.isMarkable())
                    continue;
                thing = v.asGCThing();
                break;
              }
              case AutoGCRooter::XML:
                thing = static_cast<AutoXMLRooter *>(gcr)->xml;
                break;
              case AutoGCRooter::OBJECT:
                thing = static_cast<AutoObjectRooter *>(gcr)->obj;
                if (!thing)
                    continue;
                break;
              case AutoGCRooter::ID: {
                jsid id = static_cast<AutoIdRooter *>(gcr)->id();
                if (!JSID_IS_GCTHING(id))
                    continue;
                thing = JSID_TO_GCTHING(id);
                break;
              }
            }

            if (JSString::isStatic(thing))
                continue;

            if (!IsMarkedGCThing(thing)) {
                ConservativeGCTest test = IsGCThingWord(rt, reinterpret_cast<jsuword>(thing));
                fprintf(stderr,
                        "Conservative GC scanner has missed the root 0x%p with tag %ld"
                        " on the stack due to %d. The root location 0x%p, distance from"
                        " the stack base %ld, conservative gc span %ld."
                        " Consevtaive GC status for the thread %d."
                        " Aborting.\n",
                        thing, (long) gcr->tag, int(test), (void *) gcr,
                        (long) ((jsword) JS_THREAD_DATA(acx)->nativeStackBase - (jsword) gcr),
                        (long) ((jsword) JS_THREAD_DATA(acx)->nativeStackBase -
                                (jsword) JS_THREAD_DATA(acx)->conservativeGC.nativeStackTop),
                        int(JS_THREAD_DATA(acx)->conservativeGC.hasStackToScan()));
                JS_ASSERT(false);
                abort();
            }
        }
    }
#endif

    for (RootRange r = rt->gcRootsHash.all(); !r.empty(); r.popFront())
        gc_root_traversal(trc, r.front());

    for (GCLocks::Range r = rt->gcLocksHash.all(); !r.empty(); r.popFront())
        gc_lock_traversal(r.front(), trc);

    js_TraceAtomState(trc);
    js_MarkTraps(trc);

    iter = NULL;
    while (JSContext *acx = js_ContextIterator(rt, JS_TRUE, &iter))
        MarkContext(trc, acx);

    for (ThreadDataIter i(rt); !i.empty(); i.popFront())
        i.threadData()->mark(trc);

    if (rt->emptyArgumentsShape)
        rt->emptyArgumentsShape->trace(trc);
    if (rt->emptyBlockShape)
        rt->emptyBlockShape->trace(trc);
    if (rt->emptyCallShape)
        rt->emptyCallShape->trace(trc);
    if (rt->emptyDeclEnvShape)
        rt->emptyDeclEnvShape->trace(trc);
    if (rt->emptyEnumeratorShape)
        rt->emptyEnumeratorShape->trace(trc);
    if (rt->emptyWithShape)
        rt->emptyWithShape->trace(trc);

    /*
     * We mark extra roots at the last thing so it can use use additional
     * colors to implement cycle collection.
     */
    if (rt->gcExtraRootsTraceOp)
        rt->gcExtraRootsTraceOp(trc, rt->gcExtraRootsData);

#ifdef DEBUG
    if (rt->functionMeterFilename) {
        for (int k = 0; k < 2; k++) {
            typedef JSRuntime::FunctionCountMap HM;
            HM &h = (k == 0) ? rt->methodReadBarrierCountMap : rt->unjoinedFunctionCountMap;
            for (HM::Range r = h.all(); !r.empty(); r.popFront()) {
                JSFunction *fun = r.front().key;
                JS_CALL_OBJECT_TRACER(trc, fun, "FunctionCountMap key");
            }
        }
    }
#endif
}

void
TriggerGC(JSRuntime *rt)
{
    JS_ASSERT(!rt->gcRunning);
    if (rt->gcIsNeeded)
        return;

    /*
     * Trigger the GC when it is safe to call an operation callback on any
     * thread.
     */
    rt->gcIsNeeded = true;
    TriggerAllOperationCallbacks(rt);
}

} /* namespace js */

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
    Class *clasp = obj->getClass();
    if (clasp->finalize)
        clasp->finalize(cx, obj);

    Probes::finalizeObject(obj);

    obj->finish(cx);
}

inline void
FinalizeFunction(JSContext *cx, JSFunction *fun, unsigned thingKind)
{
    FinalizeObject(cx, FUN_OBJECT(fun), thingKind);
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
FinalizeShortString(JSContext *cx, JSShortString *str, unsigned thingKind)
{
    JS_ASSERT(FINALIZE_SHORT_STRING == thingKind);
    JS_ASSERT(!JSString::isStatic(str->header()));
    JS_ASSERT(str->header()->isFlat());
    JS_RUNTIME_UNMETER(cx->runtime, liveStrings);
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
    } else if (str->isFlat()) {
        /*
         * flatChars for stillborn string is null, but cx->free checks
         * for a null pointer on its own.
         */
        cx->free(str->flatChars());
    } else if (str->isTopNode()) {
        cx->free(str->topNodeBuffer());
    }
    /* Nothing to be done for rope interior nodes. */
}

inline void
FinalizeExternalString(JSContext *cx, JSString *str, unsigned thingKind)
{
    unsigned type = thingKind - FINALIZE_EXTERNAL_STRING0;
    JS_ASSERT(type < JS_ARRAY_LENGTH(str_finalizers));
    JS_ASSERT(!JSString::isStatic(str));
    JS_ASSERT(str->isFlat());

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
    JS_ASSERT(!str->isRope());

    if (str->isDependent()) {
        /* A dependent string can not be external and must be valid. */
        JS_ASSERT(JSGCArenaInfo::fromGCThing(str)->list->thingKind == FINALIZE_STRING);
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
        } else if (thingKind != FINALIZE_SHORT_STRING) {
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

    METER(UpdateArenaStats(&cx->runtime->gcArenaStats[thingKind],
                           nlivearenas, nkilledarenas, nthings));
}

#ifdef JS_THREADSAFE

namespace js {

bool
GCHelperThread::init(JSRuntime *rt)
{
    if (!(wakeup = PR_NewCondVar(rt->gcLock)))
        return false;
    if (!(sweepingDone = PR_NewCondVar(rt->gcLock)))
        return false;

    thread = PR_CreateThread(PR_USER_THREAD, threadMain, rt, PR_PRIORITY_NORMAL,
                             PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    return !!thread;

}

void
GCHelperThread::finish(JSRuntime *rt)
{
    PRThread *join = NULL;
    {
        AutoLockGC lock(rt);
        if (thread && !shutdown) {
            shutdown = true;
            PR_NotifyCondVar(wakeup);
            join = thread;
        }
    }
    if (join) {
        /* PR_DestroyThread is not necessary. */
        PR_JoinThread(join);
    }
    if (wakeup)
        PR_DestroyCondVar(wakeup);
    if (sweepingDone)
        PR_DestroyCondVar(sweepingDone);
}

/* static */
void
GCHelperThread::threadMain(void *arg)
{
    JSRuntime *rt = static_cast<JSRuntime *>(arg);
    rt->gcHelperThread.threadLoop(rt);
}

void
GCHelperThread::threadLoop(JSRuntime *rt)
{
    AutoLockGC lock(rt);
    while (!shutdown) {
        /*
         * Sweeping can be true here on the first iteration if a GC and the
         * corresponding startBackgroundSweep call happen before this thread
         * has a chance to run.
         */
        if (!sweeping)
            PR_WaitCondVar(wakeup, PR_INTERVAL_NO_TIMEOUT);
        if (sweeping) {
            AutoUnlockGC unlock(rt);
            doSweep();
        }
        sweeping = false;
        PR_NotifyAllCondVar(sweepingDone);
    }
}

void
GCHelperThread::startBackgroundSweep(JSRuntime *rt)
{
    /* The caller takes the GC lock. */
    JS_ASSERT(!sweeping);
    sweeping = true;
    PR_NotifyCondVar(wakeup);
}

void
GCHelperThread::waitBackgroundSweepEnd(JSRuntime *rt)
{
    AutoLockGC lock(rt);
    while (sweeping)
        PR_WaitCondVar(sweepingDone, PR_INTERVAL_NO_TIMEOUT);
}

JS_FRIEND_API(void)
GCHelperThread::replenishAndFreeLater(void *ptr)
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
GCHelperThread::doSweep()
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
    freeVector.resize(0);
}

}

#endif /* JS_THREADSAFE */

static void
SweepCompartments(JSContext *cx)
{
    JSRuntime *rt = cx->runtime;
    JSCompartmentCallback callback = rt->compartmentCallback;
    JSCompartment **read = rt->compartments.begin();
    JSCompartment **end = rt->compartments.end();
    JSCompartment **write = read;

    /* Delete defaultCompartment only during runtime shutdown */
    rt->defaultCompartment->marked = true;

    while (read < end) {
        JSCompartment *compartment = (*read++);
        if (compartment->marked) {
            compartment->marked = false;
            *write++ = compartment;
            /* Remove dead wrappers from the compartment map. */
            compartment->sweep(cx);
        } else {
            if (callback)
                (void) callback(cx, compartment, JSCOMPARTMENT_DESTROY);
            if (compartment->principals)
                JSPRINCIPALS_DROP(cx, compartment->principals);
            delete compartment;
        }
    }
    rt->compartments.resize(write - rt->compartments.begin());
}

/*
 * Common cache invalidation and so forth that must be done before GC. Even if
 * GCUntilDone calls GC several times, this work needs to be done only once.
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
        rt->shapeGen = Shape::LAST_RESERVED_SHAPE;
        rt->protoHazardShape = 0;
    }

    js_PurgeThreads(cx);
    {
        JSContext *iter = NULL;
        while (JSContext *acx = js_ContextIterator(rt, JS_TRUE, &iter))
            acx->purge();
    }
}

/*
 * Perform mark-and-sweep GC.
 *
 * In a JS_THREADSAFE build, the calling thread must be rt->gcThread and each
 * other thread must be either outside all requests or blocked waiting for GC
 * to finish. Note that the caller does not hold rt->gcLock.
 */
static void
MarkAndSweep(JSContext *cx  GCTIMER_PARAM)
{
    JSRuntime *rt = cx->runtime;
    rt->gcNumber++;

    /*
     * Mark phase.
     */
    GCMarker gcmarker(cx);
    JS_ASSERT(IS_GC_MARKING_TRACER(&gcmarker));
    JS_ASSERT(gcmarker.getMarkColor() == BLACK);
    rt->gcMarkingTracer = &gcmarker;

    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront())
        GCChunkInfo::fromChunk(r.front())->clearMarkBitmap();

    MarkRuntime(&gcmarker);
    js_MarkScriptFilenames(rt);

    /*
     * Mark children of things that caused too deep recursion during the above
     * tracing.
     */
    gcmarker.markDelayedChildren();

    rt->gcMarkingTracer = NULL;

    if (rt->gcCallback)
        (void) rt->gcCallback(cx, JSGC_MARK_END);

#ifdef JS_THREADSAFE
    /*
     * cx->gcBackgroundFree is set if we need several mark-and-sweep loops to
     * finish the GC.
     */
    if(!cx->gcBackgroundFree) {
        /* Wait until the sweeping from the previois GC finishes. */
        rt->gcHelperThread.waitBackgroundSweepEnd(rt);
        cx->gcBackgroundFree = &rt->gcHelperThread;
    }
#endif

    /*
     * Sweep phase.
     *
     * Finalize as we sweep, outside of rt->gcLock but with rt->gcRunning set
     * so that any attempt to allocate a GC-thing from a finalizer will fail,
     * rather than nest badly and leave the unmarked newborn to be swept.
     *
     * We first sweep atom state so we can use js_IsAboutToBeFinalized on
     * JSString held in a hashtable to check if the hashtable entry can be
     * freed. Note that even after the entry is freed, JSObject finalizers can
     * continue to access the corresponding JSString* assuming that they are
     * unique. This works since the atomization API must not be called during
     * the GC.
     */
    TIMESTAMP(startSweep);
    js_SweepAtomState(cx);

    /* Finalize watch points associated with unreachable objects. */
    js_SweepWatchPoints(cx);

#ifdef DEBUG
    /* Save the pre-sweep count of scope-mapped properties. */
    rt->liveObjectPropsPreSweep = rt->liveObjectProps;
#endif

#ifdef JS_METHODJIT
    /* Fix-up call ICs guarding against unreachable objects. */
    mjit::SweepCallICs(cx);
#endif

    /*
     * We finalize iterators before other objects so the iterator can use the
     * object which properties it enumerates over to finalize the enumeration
     * state. We finalize objects before other GC things to ensure that
     * object's finalizer can access them even if they will be freed.
     */
    JS_ASSERT(!rt->gcEmptyArenaList);
    FinalizeArenaList<JSObject, FinalizeObject>(cx, FINALIZE_OBJECT);
    FinalizeArenaList<JSFunction, FinalizeFunction>(cx, FINALIZE_FUNCTION);
#if JS_HAS_XML_SUPPORT
    FinalizeArenaList<JSXML, FinalizeXML>(cx, FINALIZE_XML);
#endif
    TIMESTAMP(sweepObjectEnd);

    /*
     * We sweep the deflated cache before we finalize the strings so the
     * cache can safely use js_IsAboutToBeFinalized..
     */
    rt->deflatedStringCache->sweep(cx);

    FinalizeArenaList<JSShortString, FinalizeShortString>(cx, FINALIZE_SHORT_STRING);
    FinalizeArenaList<JSString, FinalizeString>(cx, FINALIZE_STRING);
    for (unsigned i = FINALIZE_EXTERNAL_STRING0;
         i <= FINALIZE_EXTERNAL_STRING_LAST;
         ++i) {
        FinalizeArenaList<JSString, FinalizeExternalString>(cx, i);
    }

    rt->gcNewArenaTriggerBytes = rt->gcBytes < GC_ARENA_ALLOCATION_TRIGGER ?
                                 GC_ARENA_ALLOCATION_TRIGGER :
                                 rt->gcBytes;

    TIMESTAMP(sweepStringEnd);

    SweepCompartments(cx);

    /*
     * Sweep the runtime's property trees after finalizing objects, in case any
     * had watchpoints referencing tree nodes.
     */
    js::PropertyTree::sweepShapes(cx);

    /*
     * Sweep script filenames after sweeping functions in the generic loop
     * above. In this way when a scripted function's finalizer destroys the
     * script and calls rt->destroyScriptHook, the hook can still access the
     * script's filename. See bug 323267.
     */
    js_SweepScriptFilenames(rt);

    /* Slowify arrays we have accumulated. */
    gcmarker.slowifyArrays();

    /*
     * Destroy arenas after we finished the sweeping so finalizers can safely
     * use js_IsAboutToBeFinalized().
     */
    FreeGCChunks(rt);
    TIMESTAMP(sweepDestroyEnd);

    if (rt->gcCallback)
        (void) rt->gcCallback(cx, JSGC_FINALIZE_END);
#ifdef DEBUG_srcnotesize
  { extern void DumpSrcNoteSizeHist();
    DumpSrcNoteSizeHist();
    printf("GC HEAP SIZE %lu\n", (unsigned long)rt->gcBytes);
  }
#endif

#ifdef JS_SCOPE_DEPTH_METER
    DumpScopeDepthMeter(rt);
#endif

#ifdef JS_DUMP_LOOP_STATS
    DumpLoopStats(rt);
#endif
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

    size_t requestDebit = cx->thread->requestDepth ? 1 : 0;
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

    /*
     * Wait for GC to finish on the other thread, even if requestDebit is 0
     * and even if GC has not started yet because the gcThread is waiting in
     * AutoGCSession. This ensures that js_GC never returns without a full GC
     * cycle happening.
     */
    do {
        JS_AWAIT_GC_DONE(rt);
    } while (rt->gcThread);

    cx->thread->gcWaiting = false;
    rt->requestCount += requestDebit;
}

#endif

class AutoGCSession {
  public:
    explicit AutoGCSession(JSContext *cx);
    ~AutoGCSession();

  private:
    JSContext   *context;

    /* Disable copy constructor or assignments */
    AutoGCSession(const AutoGCSession&);
    void operator=(const AutoGCSession&);
};

/*
 * Start a new GC session. Together with LetOtherGCFinish this function
 * contains the rendezvous algorithm by which we stop the world for GC.
 *
 * This thread becomes the GC thread. Wait for all other threads to quiesce.
 * Then set rt->gcRunning and return.
 */
AutoGCSession::AutoGCSession(JSContext *cx)
  : context(cx)
{
    JSRuntime *rt = cx->runtime;

#ifdef JS_THREADSAFE
    if (rt->gcThread && rt->gcThread != cx->thread)
        LetOtherGCFinish(cx);
#endif

    JS_ASSERT(!rt->gcRunning);

#ifdef JS_THREADSAFE
    /* No other thread is in GC, so indicate that we're now in GC. */
    JS_ASSERT(!rt->gcThread);
    rt->gcThread = cx->thread;

    /*
     * Notify operation callbacks on other threads, which will give them a
     * chance to yield their requests. Threads without requests perform their
     * callback at some later point, which then will be unnecessary, but
     * harmless.
     */
    for (JSThread::Map::Range r = rt->threads.all(); !r.empty(); r.popFront()) {
        JSThread *thread = r.front().value;
        if (thread != cx->thread)
            thread->data.triggerOperationCallback();
    }

    /*
     * Discount the request on the current thread from contributing to
     * rt->requestCount before we wait for all other requests to finish.
     * JS_NOTIFY_REQUEST_DONE, which will wake us up, is only called on
     * rt->requestCount transitions to 0.
     */
    size_t requestDebit = cx->thread->requestDepth ? 1 : 0;
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
AutoGCSession::~AutoGCSession()
{
    JSRuntime *rt = context->runtime;
    rt->gcRunning = false;
#ifdef JS_THREADSAFE
    JS_ASSERT(rt->gcThread == context->thread);
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
    if (rt->gcMarkAndSweep) {
        rt->gcPoke = true;
#ifdef JS_THREADSAFE
        JS_ASSERT(rt->gcThread);
        if (rt->gcThread != cx->thread) {
            /* We do not return until another GC finishes. */
            LetOtherGCFinish(cx);
        }
#endif
        return;
    }

    AutoGCSession gcsession(cx);

    METER(rt->gcStats.poke++);

    bool firstRun = true;
    rt->gcMarkAndSweep = true;
#ifdef JS_THREADSAFE
    JS_ASSERT(!cx->gcBackgroundFree);
#endif
    do {
        rt->gcPoke = false;

        AutoUnlockGC unlock(rt);
        if (firstRun) {
            PreGCCleanup(cx, gckind);
            TIMESTAMP(startMark);
            firstRun = false;
        }
        MarkAndSweep(cx  GCTIMER_ARG);

        // GC again if:
        //   - another thread, not in a request, called js_GC
        //   - js_GC was called recursively
        //   - a finalizer called js_RemoveRoot or js_UnlockGCThingRT.
    } while (rt->gcPoke);

#ifdef JS_THREADSAFE
    JS_ASSERT(cx->gcBackgroundFree == &rt->gcHelperThread);
    cx->gcBackgroundFree = NULL;
    rt->gcHelperThread.startBackgroundSweep(rt);
#endif

    rt->gcMarkAndSweep = false;
    rt->gcRegenShapes = false;
    rt->setGCLastBytes(rt->gcBytes);
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

    RecordNativeStackTopForGC(cx);

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
    /*
     * This function cannot be called during the GC and always requires a
     * request.
     */
#ifdef JS_THREADSAFE
    JS_ASSERT(cx->thread->requestDepth);

    /*
     * This is only necessary if AutoGCSession below would wait for GC to
     * finish on another thread, but to capture the minimal stack space and
     * for code simplicity we do it here unconditionally.
     */
    RecordNativeStackTopForGC(cx);
#endif

    JSRuntime *rt = cx->runtime;
    AutoLockGC lock(rt);
    AutoGCSession gcsession(cx);
    AutoUnlockGC unlock(rt);

    bool cycle = false;
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

    return !cycle;
}

void
TraceRuntime(JSTracer *trc)
{
    LeaveTrace(trc->context);

#ifdef JS_THREADSAFE
    {
        JSContext *cx = trc->context;
        JSRuntime *rt = cx->runtime;
        AutoLockGC lock(rt);

        if (rt->gcThread != cx->thread) {
            AutoGCSession gcsession(cx);
            AutoUnlockGC unlock(rt);
            RecordNativeStackTopForGC(trc->context);
            MarkRuntime(trc);
            return;
        }
    }
#else
    RecordNativeStackTopForGC(trc->context);
#endif

    /*
     * Calls from inside a normal GC or a recursive calls are OK and do not
     * require session setup.
     */
    MarkRuntime(trc);
}

JSCompartment *
NewCompartment(JSContext *cx, JSPrincipals *principals)
{
    JSRuntime *rt = cx->runtime;
    JSCompartment *compartment = new JSCompartment(rt);
    if (!compartment || !compartment->init()) {
        JS_ReportOutOfMemory(cx);
        return NULL;
    }

    if (principals) {
        compartment->principals = principals;
        JSPRINCIPALS_HOLD(cx, principals);
    }

    {
        AutoLockGC lock(rt);

        if (!rt->compartments.append(compartment)) {
            AutoUnlockGC unlock(rt);
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
    }

    JSCompartmentCallback callback = rt->compartmentCallback;
    if (callback && !callback(cx, compartment, JSCOMPARTMENT_NEW)) {
        AutoLockGC lock(rt);
        rt->compartments.popBack();
        return NULL;
    }

    return compartment;
}

}
