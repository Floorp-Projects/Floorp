/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef jsgc_h___
#define jsgc_h___

/*
 * JS Garbage Collector.
 */
#include <setjmp.h>

#include "mozilla/Util.h"

#include "jsalloc.h"
#include "jstypes.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsdhash.h"
#include "jsgcchunk.h"
#include "jslock.h"
#include "jsutil.h"
#include "jsversion.h"
#include "jsgcstats.h"
#include "jscell.h"

#include "gc/Statistics.h"
#include "js/HashTable.h"
#include "js/Vector.h"

struct JSCompartment;

extern "C" void
js_TraceXML(JSTracer *trc, JSXML* thing);

#if JS_STACK_GROWTH_DIRECTION > 0
# define JS_CHECK_STACK_SIZE(limit, lval)  ((jsuword)(lval) < limit)
#else
# define JS_CHECK_STACK_SIZE(limit, lval)  ((jsuword)(lval) > limit)
#endif

namespace js {

class GCHelperThread;
struct Shape;

namespace gc {

struct Arena;

/*
 * This must be an upper bound, but we do not need the least upper bound, so
 * we just exclude non-background objects.
 */
const size_t MAX_BACKGROUND_FINALIZE_KINDS = FINALIZE_LIMIT - (FINALIZE_OBJECT_LAST + 1) / 2;

const size_t ArenaShift = 12;
const size_t ArenaSize = size_t(1) << ArenaShift;
const size_t ArenaMask = ArenaSize - 1;

/*
 * The mark bitmap has one bit per each GC cell. For multi-cell GC things this
 * wastes space but allows to avoid expensive devisions by thing's size when
 * accessing the bitmap. In addition this allows to use some bits for colored
 * marking during the cycle GC.
 */
const size_t ArenaCellCount = size_t(1) << (ArenaShift - Cell::CellShift);
const size_t ArenaBitmapBits = ArenaCellCount;
const size_t ArenaBitmapBytes = ArenaBitmapBits / 8;
const size_t ArenaBitmapWords = ArenaBitmapBits / JS_BITS_PER_WORD;

/*
 * A FreeSpan represents a contiguous sequence of free cells in an Arena.
 * |first| is the address of the first free cell in the span. |last| is the
 * address of the last free cell in the span. This last cell holds a FreeSpan
 * data structure for the next span unless this is the last span on the list
 * of spans in the arena. For this last span |last| points to the last byte of
 * the last thing in the arena and no linkage is stored there, so
 * |last| == arenaStart + ArenaSize - 1. If the space at the arena end is
 * fully used this last span is empty and |first| == |last + 1|.
 *
 * Thus |first| < |last| implies that we have either the last span with at least
 * one element or that the span is not the last and contains at least 2
 * elements. In both cases to allocate a thing from this span we need simply
 * to increment |first| by the allocation size.
 *
 * |first| == |last| implies that we have a one element span that records the
 * next span. So to allocate from it we need to update the span list head
 * with a copy of the span stored at |last| address so the following
 * allocations will use that span.
 *
 * |first| > |last| implies that we have an empty last span and the arena is
 * fully used.
 *
 * Also only for the last span (|last| & 1)! = 0 as all allocation sizes are
 * multiples of Cell::CellSize.
 */
struct FreeSpan {
    uintptr_t   first;
    uintptr_t   last;

  public:
    FreeSpan() {}

    FreeSpan(uintptr_t first, uintptr_t last)
      : first(first), last(last) {
        checkSpan();
    }

    /*
     * To minimize the size of the arena header the first span is encoded
     * there as offsets from the arena start.
     */
    static size_t encodeOffsets(size_t firstOffset, size_t lastOffset) {
        /* Check that we can pack the offsets into uint16. */
        JS_STATIC_ASSERT(ArenaShift < 16);
        JS_ASSERT(firstOffset <= ArenaSize);
        JS_ASSERT(lastOffset < ArenaSize);
        JS_ASSERT(firstOffset <= ((lastOffset + 1) & ~size_t(1)));
        return firstOffset | (lastOffset << 16);
    }

    /*
     * Encoded offsets for a full arena when its first span is the last one
     * and empty.
     */
    static const size_t FullArenaOffsets = ArenaSize | ((ArenaSize - 1) << 16);

    static FreeSpan decodeOffsets(uintptr_t arenaAddr, size_t offsets) {
        JS_ASSERT(!(arenaAddr & ArenaMask));

        size_t firstOffset = offsets & 0xFFFF;
        size_t lastOffset = offsets >> 16;
        JS_ASSERT(firstOffset <= ArenaSize);
        JS_ASSERT(lastOffset < ArenaSize);

        /*
         * We must not use | when calculating first as firstOffset is
         * ArenaMask + 1 for the empty span.
         */
        return FreeSpan(arenaAddr + firstOffset, arenaAddr | lastOffset);
    }

    void initAsEmpty(uintptr_t arenaAddr = 0) {
        JS_ASSERT(!(arenaAddr & ArenaMask));
        first = arenaAddr + ArenaSize;
        last = arenaAddr | (ArenaSize  - 1);
        JS_ASSERT(isEmpty());
    }

    bool isEmpty() const {
        checkSpan();
        return first > last;
    }

    bool hasNext() const {
        checkSpan();
        return !(last & uintptr_t(1));
    }

    const FreeSpan *nextSpan() const {
        JS_ASSERT(hasNext());
        return reinterpret_cast<FreeSpan *>(last);
    }

    FreeSpan *nextSpanUnchecked(size_t thingSize) const {
#ifdef DEBUG
        uintptr_t lastOffset = last & ArenaMask;
        JS_ASSERT(!(lastOffset & 1));
        JS_ASSERT((ArenaSize - lastOffset) % thingSize == 0);
#endif
        return reinterpret_cast<FreeSpan *>(last);
    }

    uintptr_t arenaAddressUnchecked() const {
        return last & ~ArenaMask;
    }

    uintptr_t arenaAddress() const {
        checkSpan();
        return arenaAddressUnchecked();
    }

    ArenaHeader *arenaHeader() const {
        return reinterpret_cast<ArenaHeader *>(arenaAddress());
    }

    bool isSameNonEmptySpan(const FreeSpan *another) const {
        JS_ASSERT(!isEmpty());
        JS_ASSERT(!another->isEmpty());
        return first == another->first && last == another->last;
    }

    bool isWithinArena(uintptr_t arenaAddr) const {
        JS_ASSERT(!(arenaAddr & ArenaMask));

        /* Return true for the last empty span as well. */
        return arenaAddress() == arenaAddr;
    }

    size_t encodeAsOffsets() const {
        /*
         * We must use first - arenaAddress(), not first & ArenaMask as
         * first == ArenaMask + 1 for an empty span.
         */
        uintptr_t arenaAddr = arenaAddress();
        return encodeOffsets(first - arenaAddr, last & ArenaMask);
    }

    /* See comments before FreeSpan for details. */
    JS_ALWAYS_INLINE void *allocate(size_t thingSize) {
        JS_ASSERT(thingSize % Cell::CellSize == 0);
        checkSpan();
        uintptr_t thing = first;
        if (thing < last) {
            /* Bump-allocate from the current span. */
            first = thing + thingSize;
        } else if (JS_LIKELY(thing == last)) {
            /*
             * Move to the next span. We use JS_LIKELY as without PGO
             * compilers mis-predict == here as unlikely to succeed.
             */
            *this = *reinterpret_cast<FreeSpan *>(thing);
        } else {
            return NULL;
        }
        checkSpan();
        return reinterpret_cast<void *>(thing);
    }

    /* A version of allocate when we know that the span is not empty. */
    JS_ALWAYS_INLINE void *infallibleAllocate(size_t thingSize) {
        JS_ASSERT(thingSize % Cell::CellSize == 0);
        checkSpan();
        uintptr_t thing = first;
        if (thing < last) {
            first = thing + thingSize;
        } else {
            JS_ASSERT(thing == last);
            *this = *reinterpret_cast<FreeSpan *>(thing);
        }
        checkSpan();
        return reinterpret_cast<void *>(thing);
    }

    /*
     * Allocate from a newly allocated arena. We do not move the free list
     * from the arena. Rather we set the arena up as fully used during the
     * initialization so to allocate we simply return the first thing in the
     * arena and set the free list to point to the second.
     */
    JS_ALWAYS_INLINE void *allocateFromNewArena(uintptr_t arenaAddr, size_t firstThingOffset,
                                                size_t thingSize) {
        JS_ASSERT(!(arenaAddr & ArenaMask));
        uintptr_t thing = arenaAddr | firstThingOffset;
        first = thing + thingSize;
        last = arenaAddr | ArenaMask;
        checkSpan();
        return reinterpret_cast<void *>(thing);
    }

    void checkSpan() const {
#ifdef DEBUG
        /* We do not allow spans at the end of the address space. */
        JS_ASSERT(last != uintptr_t(-1));
        JS_ASSERT(first);
        JS_ASSERT(last);
        JS_ASSERT(first - 1 <= last);
        uintptr_t arenaAddr = arenaAddressUnchecked();
        if (last & 1) {
            /* The span is the last. */
            JS_ASSERT((last & ArenaMask) == ArenaMask);

            if (first - 1 == last) {
                /* The span is last and empty. The above start != 0 check
                 * implies that we are not at the end of the address space.
                 */
                return;
            }
            size_t spanLength = last - first + 1;
            JS_ASSERT(spanLength % Cell::CellSize == 0);

            /* Start and end must belong to the same arena. */
            JS_ASSERT((first & ~ArenaMask) == arenaAddr);
            return;
        }

        /* The span is not the last and we have more spans to follow. */
        JS_ASSERT(first <= last);
        size_t spanLengthWithoutOneThing = last - first;
        JS_ASSERT(spanLengthWithoutOneThing % Cell::CellSize == 0);

        JS_ASSERT((first & ~ArenaMask) == arenaAddr);

        /*
         * If there is not enough space before the arena end to allocate one
         * more thing, then the span must be marked as the last one to avoid
         * storing useless empty span reference.
         */
        size_t beforeTail = ArenaSize - (last & ArenaMask);
        JS_ASSERT(beforeTail >= sizeof(FreeSpan) + Cell::CellSize);

        FreeSpan *next = reinterpret_cast<FreeSpan *>(last);

        /*
         * The GC things on the list of free spans come from one arena
         * and the spans are linked in ascending address order with
         * at least one non-free thing between spans.
         */
        JS_ASSERT(last < next->first);
        JS_ASSERT(arenaAddr == next->arenaAddressUnchecked());

        if (next->first > next->last) {
            /*
             * The next span is the empty span that terminates the list for
             * arenas that do not have any free things at the end.
             */
            JS_ASSERT(next->first - 1 == next->last);
            JS_ASSERT(arenaAddr + ArenaSize == next->first);
        }
#endif
    }

};

/* Every arena has a header. */
struct ArenaHeader {
    friend struct FreeLists;

    JSCompartment   *compartment;
    ArenaHeader     *next;

  private:
    /*
     * The first span of free things in the arena. We encode it as the start
     * and end offsets within the arena, not as FreeSpan structure, to
     * minimize the header size.
     */
    size_t          firstFreeSpanOffsets;

    /*
     * One of AllocKind constants or FINALIZE_LIMIT when the arena does not
     * contain any GC things and is on the list of empty arenas in the GC
     * chunk. The latter allows to quickly check if the arena is allocated
     * during the conservative GC scanning without searching the arena in the
     * list.
     */
    size_t       allocKind          : 8;

    /*
     * When recursive marking uses too much stack the marking is delayed and
     * the corresponding arenas are put into a stack using the following field
     * as a linkage. To distinguish the bottom of the stack from the arenas
     * not present in the stack we use an extra flag to tag arenas on the
     * stack.
     *
     * To minimize the ArenaHeader size we record the next delayed marking
     * linkage as arenaAddress() >> ArenaShift and pack it with the allocKind
     * field and hasDelayedMarking flag. We use 8 bits for the allocKind, not
     * ArenaShift - 1, so the compiler can use byte-level memory instructions
     * to access it.
     */
  public:
    size_t       hasDelayedMarking  : 1;
    size_t       nextDelayedMarking : JS_BITS_PER_WORD - 8 - 1;

    static void staticAsserts() {
        /* We must be able to fit the allockind into uint8. */
        JS_STATIC_ASSERT(FINALIZE_LIMIT <= 255);

        /*
         * nextDelayedMarkingpacking assumes that ArenaShift has enough bits
         * to cover allocKind and hasDelayedMarking.
         */
        JS_STATIC_ASSERT(ArenaShift >= 8 + 1);
    }

    inline uintptr_t address() const;
    inline Chunk *chunk() const;

    void setAsNotAllocated() {
        allocKind = size_t(FINALIZE_LIMIT);
        hasDelayedMarking = 0;
        nextDelayedMarking = 0;
    }

    bool allocated() const {
        JS_ASSERT(allocKind <= size_t(FINALIZE_LIMIT));
        return allocKind < size_t(FINALIZE_LIMIT);
    }

    inline void init(JSCompartment *comp, AllocKind kind);

    uintptr_t arenaAddress() const {
        return address();
    }

    Arena *getArena() {
        return reinterpret_cast<Arena *>(arenaAddress());
    }

    AllocKind getAllocKind() const {
        JS_ASSERT(allocated());
        return AllocKind(allocKind);
    }

    inline size_t getThingSize() const;

    bool hasFreeThings() const {
        return firstFreeSpanOffsets != FreeSpan::FullArenaOffsets;
    }

    inline bool isEmpty() const;

    void setAsFullyUsed() {
        firstFreeSpanOffsets = FreeSpan::FullArenaOffsets;
    }

    FreeSpan getFirstFreeSpan() const {
#ifdef DEBUG
        checkSynchronizedWithFreeList();
#endif
        return FreeSpan::decodeOffsets(arenaAddress(), firstFreeSpanOffsets);
    }

    void setFirstFreeSpan(const FreeSpan *span) {
        JS_ASSERT(span->isWithinArena(arenaAddress()));
        firstFreeSpanOffsets = span->encodeAsOffsets();
    }

#ifdef DEBUG
    void checkSynchronizedWithFreeList() const;
#endif

    inline Arena *getNextDelayedMarking() const;
    inline void setNextDelayedMarking(Arena *arena);
};

struct Arena {
    /*
     * Layout of an arena:
     * An arena is 4K in size and 4K-aligned. It starts with the ArenaHeader
     * descriptor followed by some pad bytes. The remainder of the arena is
     * filled with the array of T things. The pad bytes ensure that the thing
     * array ends exactly at the end of the arena.
     *
     * +-------------+-----+----+----+-----+----+
     * | ArenaHeader | pad | T0 | T1 | ... | Tn |
     * +-------------+-----+----+----+-----+----+
     *
     * <----------------------------------------> = ArenaSize bytes
     * <-------------------> = first thing offset
     */
    ArenaHeader aheader;
    uint8_t     data[ArenaSize - sizeof(ArenaHeader)];

  private:
    static JS_FRIEND_DATA(const uint32) ThingSizes[];
    static JS_FRIEND_DATA(const uint32) FirstThingOffsets[];

  public:
    static void staticAsserts();

    static size_t thingSize(AllocKind kind) {
        return ThingSizes[kind];
    }

    static size_t firstThingOffset(AllocKind kind) {
        return FirstThingOffsets[kind];
    }

    static size_t thingsPerArena(size_t thingSize) {
        JS_ASSERT(thingSize % Cell::CellSize == 0);

        /* We should be able to fit FreeSpan in any GC thing. */
        JS_ASSERT(thingSize >= sizeof(FreeSpan));

        return (ArenaSize - sizeof(ArenaHeader)) / thingSize;
    }

    static size_t thingsSpan(size_t thingSize) {
        return thingsPerArena(thingSize) * thingSize;
    }

    static bool isAligned(uintptr_t thing, size_t thingSize) {
        /* Things ends at the arena end. */
        uintptr_t tailOffset = (ArenaSize - thing) & ArenaMask;
        return tailOffset % thingSize == 0;
    }

    uintptr_t address() const {
        return aheader.address();
    }

    uintptr_t thingsStart(AllocKind thingKind) {
        return address() | firstThingOffset(thingKind);
    }

    uintptr_t thingsEnd() {
        return address() + ArenaSize;
    }

    template <typename T>
    bool finalize(JSContext *cx, AllocKind thingKind, size_t thingSize);
};

/* The chunk header (located at the end of the chunk to preserve arena alignment). */
struct ChunkInfo {
    Chunk           *next;
    Chunk           **prevp;
    ArenaHeader     *emptyArenaListHead;
    size_t          age;
    size_t          numFree;
};

const size_t BytesPerArena = ArenaSize + ArenaBitmapBytes;
const size_t ArenasPerChunk = (GC_CHUNK_SIZE - sizeof(ChunkInfo)) / BytesPerArena;

/* A chunk bitmap contains enough mark bits for all the cells in a chunk. */
struct ChunkBitmap {
    uintptr_t bitmap[ArenaBitmapWords * ArenasPerChunk];

    JS_ALWAYS_INLINE void getMarkWordAndMask(const Cell *cell, uint32 color,
                                             uintptr_t **wordp, uintptr_t *maskp);

    JS_ALWAYS_INLINE bool isMarked(const Cell *cell, uint32 color) {
        uintptr_t *word, mask;
        getMarkWordAndMask(cell, color, &word, &mask);
        return *word & mask;
    }

    JS_ALWAYS_INLINE bool markIfUnmarked(const Cell *cell, uint32 color) {
        uintptr_t *word, mask;
        getMarkWordAndMask(cell, BLACK, &word, &mask);
        if (*word & mask)
            return false;
        *word |= mask;
        if (color != BLACK) {
            /*
             * We use getMarkWordAndMask to recalculate both mask and word as
             * doing just mask << color may overflow the mask.
             */
            getMarkWordAndMask(cell, color, &word, &mask);
            if (*word & mask)
                return false;
            *word |= mask;
        }
        return true;
    }

    JS_ALWAYS_INLINE void unmark(const Cell *cell, uint32 color) {
        uintptr_t *word, mask;
        getMarkWordAndMask(cell, color, &word, &mask);
        *word &= ~mask;
    }

    void clear() {
        PodArrayZero(bitmap);
    }

#ifdef DEBUG
    bool noBitsSet(ArenaHeader *aheader) {
        /*
         * We assume that the part of the bitmap corresponding to the arena
         * has the exact number of words so we do not need to deal with a word
         * that covers bits from two arenas.
         */
        JS_STATIC_ASSERT(ArenaBitmapBits == ArenaBitmapWords * JS_BITS_PER_WORD);

        uintptr_t *word, unused;
        getMarkWordAndMask(reinterpret_cast<Cell *>(aheader->address()), BLACK, &word, &unused);
        for (size_t i = 0; i != ArenaBitmapWords; i++) {
            if (word[i])
                return false;
        }
        return true;
    }
#endif
};

JS_STATIC_ASSERT(ArenaBitmapBytes * ArenasPerChunk == sizeof(ChunkBitmap));

/*
 * Chunks contain arenas and associated data structures (mark bitmap, delayed
 * marking state).
 */
struct Chunk {
    Arena           arenas[ArenasPerChunk];
    ChunkBitmap     bitmap;
    ChunkInfo       info;

    static Chunk *fromAddress(uintptr_t addr) {
        addr &= ~GC_CHUNK_MASK;
        return reinterpret_cast<Chunk *>(addr);
    }

    static bool withinArenasRange(uintptr_t addr) {
        uintptr_t offset = addr & GC_CHUNK_MASK;
        return offset < ArenasPerChunk * ArenaSize;
    }

    static size_t arenaIndex(uintptr_t addr) {
        JS_ASSERT(withinArenasRange(addr));
        return (addr & GC_CHUNK_MASK) >> ArenaShift;
    }

    uintptr_t address() const {
        uintptr_t addr = reinterpret_cast<uintptr_t>(this);
        JS_ASSERT(!(addr & GC_CHUNK_MASK));
        return addr;
    }

    bool unused() const {
        return info.numFree == ArenasPerChunk;
    }

    bool hasAvailableArenas() const {
        return info.numFree > 0;
    }

    inline void addToAvailableList(JSCompartment *compartment);
    inline void removeFromAvailableList();

    ArenaHeader *allocateArena(JSCompartment *comp, AllocKind kind);

    void releaseArena(ArenaHeader *aheader);

    static Chunk *allocate(JSRuntime *rt);
    static inline void release(JSRuntime *rt, Chunk *chunk);

  private:
    inline void init();
};

JS_STATIC_ASSERT(sizeof(Chunk) <= GC_CHUNK_SIZE);
JS_STATIC_ASSERT(sizeof(Chunk) + BytesPerArena > GC_CHUNK_SIZE);

class ChunkPool {
    Chunk   *emptyChunkListHead;
    size_t  emptyCount;

  public:
    ChunkPool()
      : emptyChunkListHead(NULL),
        emptyCount(0) { }

    size_t getEmptyCount() const {
        return emptyCount;
    }

    inline bool wantBackgroundAllocation(JSRuntime *rt) const;

    /* Must be called with the GC lock taken. */
    inline Chunk *get(JSRuntime *rt);

    /* Must be called either during the GC or with the GC lock taken. */
    inline void put(JSRuntime *rt, Chunk *chunk);

    /* Must be called either during the GC or with the GC lock taken. */
    void expire(JSRuntime *rt, bool releaseAll);
};

inline uintptr_t
Cell::address() const
{
    uintptr_t addr = uintptr_t(this);
    JS_ASSERT(addr % Cell::CellSize == 0);
    JS_ASSERT(Chunk::withinArenasRange(addr));
    return addr;
}

inline ArenaHeader *
Cell::arenaHeader() const
{
    uintptr_t addr = address();
    addr &= ~ArenaMask;
    return reinterpret_cast<ArenaHeader *>(addr);
}

Chunk *
Cell::chunk() const
{
    uintptr_t addr = uintptr_t(this);
    JS_ASSERT(addr % Cell::CellSize == 0);
    addr &= ~(GC_CHUNK_SIZE - 1);
    return reinterpret_cast<Chunk *>(addr);
}

AllocKind
Cell::getAllocKind() const
{
    return arenaHeader()->getAllocKind();
}

#ifdef DEBUG
inline bool
Cell::isAligned() const
{
    return Arena::isAligned(address(), arenaHeader()->getThingSize());
}
#endif

inline void
ArenaHeader::init(JSCompartment *comp, AllocKind kind)
{
    JS_ASSERT(!allocated());
    JS_ASSERT(!hasDelayedMarking);
    compartment = comp;

    JS_STATIC_ASSERT(FINALIZE_LIMIT <= 255);
    allocKind = size_t(kind);

    /* See comments in FreeSpan::allocateFromNewArena. */
    firstFreeSpanOffsets = FreeSpan::FullArenaOffsets;
}

inline uintptr_t
ArenaHeader::address() const
{
    uintptr_t addr = reinterpret_cast<uintptr_t>(this);
    JS_ASSERT(!(addr & ArenaMask));
    JS_ASSERT(Chunk::withinArenasRange(addr));
    return addr;
}

inline Chunk *
ArenaHeader::chunk() const
{
    return Chunk::fromAddress(address());
}

inline bool
ArenaHeader::isEmpty() const
{
    /* Arena is empty if its first span covers the whole arena. */
    JS_ASSERT(allocated());
    size_t firstThingOffset = Arena::firstThingOffset(getAllocKind());
    return firstFreeSpanOffsets == FreeSpan::encodeOffsets(firstThingOffset, ArenaMask);
}

inline size_t
ArenaHeader::getThingSize() const
{
    JS_ASSERT(allocated());
    return Arena::thingSize(getAllocKind());
}

inline Arena *
ArenaHeader::getNextDelayedMarking() const
{
    return reinterpret_cast<Arena *>(nextDelayedMarking << ArenaShift);
}

inline void
ArenaHeader::setNextDelayedMarking(Arena *arena)
{
    JS_ASSERT(!hasDelayedMarking);
    hasDelayedMarking = 1;
    nextDelayedMarking = arena->address() >> ArenaShift;
}

JS_ALWAYS_INLINE void
ChunkBitmap::getMarkWordAndMask(const Cell *cell, uint32 color,
                                uintptr_t **wordp, uintptr_t *maskp)
{
    JS_ASSERT(cell->chunk() == Chunk::fromAddress(reinterpret_cast<uintptr_t>(this)));
    size_t bit = (cell->address() & GC_CHUNK_MASK) / Cell::CellSize + color;
    JS_ASSERT(bit < ArenaBitmapBits * ArenasPerChunk);
    *maskp = uintptr_t(1) << (bit % JS_BITS_PER_WORD);
    *wordp = &bitmap[bit / JS_BITS_PER_WORD];
}

static void
AssertValidColor(const void *thing, uint32 color)
{
#ifdef DEBUG
    ArenaHeader *aheader = reinterpret_cast<const js::gc::Cell *>(thing)->arenaHeader();
    JS_ASSERT_IF(color, color < aheader->getThingSize() / Cell::CellSize);
#endif
}

inline bool
Cell::isMarked(uint32 color) const
{
    AssertValidColor(this, color);
    return chunk()->bitmap.isMarked(this, color);
}

bool
Cell::markIfUnmarked(uint32 color) const
{
    AssertValidColor(this, color);
    return chunk()->bitmap.markIfUnmarked(this, color);
}

void
Cell::unmark(uint32 color) const
{
    JS_ASSERT(color != BLACK);
    AssertValidColor(this, color);
    chunk()->bitmap.unmark(this, color);
}

JSCompartment *
Cell::compartment() const
{
    return arenaHeader()->compartment;
}

/*
 * Lower limit after which we limit the heap growth
 */
const size_t GC_ALLOCATION_THRESHOLD = 30 * 1024 * 1024;

/*
 * A GC is triggered once the number of newly allocated arenas is
 * GC_HEAP_GROWTH_FACTOR times the number of live arenas after the last GC
 * starting after the lower limit of GC_ALLOCATION_THRESHOLD.
 */
const float GC_HEAP_GROWTH_FACTOR = 3.0f;

/* Perform a Full GC every 20 seconds if MaybeGC is called */
static const int64 GC_IDLE_FULL_SPAN = 20 * 1000 * 1000;

static inline JSGCTraceKind
MapAllocToTraceKind(AllocKind thingKind)
{
    static const JSGCTraceKind map[FINALIZE_LIMIT] = {
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT0 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT0_BACKGROUND */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT2 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT2_BACKGROUND */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT4 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT4_BACKGROUND */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT8 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT8_BACKGROUND */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT12 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT12_BACKGROUND */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT16 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT16_BACKGROUND */
        JSTRACE_OBJECT,     /* FINALIZE_FUNCTION */
        JSTRACE_SCRIPT,     /* FINALIZE_SCRIPT */
        JSTRACE_SHAPE,      /* FINALIZE_SHAPE */
        JSTRACE_TYPE_OBJECT,/* FINALIZE_TYPE_OBJECT */
#if JS_HAS_XML_SUPPORT      /* FINALIZE_XML */
        JSTRACE_XML,
#endif
        JSTRACE_STRING,     /* FINALIZE_SHORT_STRING */
        JSTRACE_STRING,     /* FINALIZE_STRING */
        JSTRACE_STRING,     /* FINALIZE_EXTERNAL_STRING */
    };
    return map[thingKind];
}

inline JSGCTraceKind
GetGCThingTraceKind(const void *thing);

struct ArenaLists {

    /*
     * ArenaList::head points to the start of the list. Normally cursor points
     * to the first arena in the list with some free things and all arenas
     * before cursor are fully allocated. However, as the arena currently being
     * allocated from is considered full while its list of free spans is moved
     * into the freeList, during the GC or cell enumeration, when an
     * unallocated freeList is moved back to the arena, we can see an arena
     * with some free cells before the cursor. The cursor is an indirect
     * pointer to allow for efficient list insertion at the cursor point and
     * other list manipulations.
     */
    struct ArenaList {
        ArenaHeader     *head;
        ArenaHeader     **cursor;

        ArenaList() {
            clear();
        }

        void clear() {
            head = NULL;
            cursor = &head;
        }
    };

  private:
    /*
     * For each arena kind its free list is represented as the first span with
     * free things. Initially all the spans are initialized as empty. After we
     * find a new arena with available things we move its first free span into
     * the list and set the arena as fully allocated. way we do not need to
     * update the arena header after the initial allocation. When starting the
     * GC we only move the head of the of the list of spans back to the arena
     * only for the arena that was not fully allocated.
     */
    FreeSpan       freeLists[FINALIZE_LIMIT];

    ArenaList      arenaLists[FINALIZE_LIMIT];

#ifdef JS_THREADSAFE
    /*
     * The background finalization adds the finalized arenas to the list at
     * the *cursor position. backgroundFinalizeState controls the interaction
     * between the GC lock and the access to the list from the allocation
     * thread.
     *
     * BFS_DONE indicates that the finalizations is not running or cannot
     * affect this arena list. The allocation thread can access the list
     * outside the GC lock.
     *
     * In BFS_RUN and BFS_JUST_FINISHED the allocation thread must take the
     * lock. The former indicates that the finalization still runs. The latter
     * signals that finalization just added to the list finalized arenas. In
     * that case the lock effectively serves as a read barrier to ensure that
     * the allocation thread see all the writes done during finalization.
     */
    enum BackgroundFinalizeState {
        BFS_DONE,
        BFS_RUN,
        BFS_JUST_FINISHED
    };

    volatile uintptr_t backgroundFinalizeState[FINALIZE_LIMIT];
#endif

  public:
    ArenaLists() {
        for (size_t i = 0; i != FINALIZE_LIMIT; ++i)
            freeLists[i].initAsEmpty();
#ifdef JS_THREADSAFE
        for (size_t i = 0; i != FINALIZE_LIMIT; ++i)
            backgroundFinalizeState[i] = BFS_DONE;
#endif
    }

    ~ArenaLists() {
        for (size_t i = 0; i != FINALIZE_LIMIT; ++i) {
#ifdef JS_THREADSAFE
            /*
             * We can only call this during the shutdown after the last GC when
             * the background finalization is disabled.
             */
            JS_ASSERT(backgroundFinalizeState[i] == BFS_DONE);
#endif
            ArenaHeader **headp = &arenaLists[i].head;
            while (ArenaHeader *aheader = *headp) {
                *headp = aheader->next;
                aheader->chunk()->releaseArena(aheader);
            }
        }
    }

    const FreeSpan *getFreeList(AllocKind thingKind) const {
        return &freeLists[thingKind];
    }

    ArenaHeader *getFirstArena(AllocKind thingKind) const {
        return arenaLists[thingKind].head;
    }

    bool arenaListsAreEmpty() const {
        for (size_t i = 0; i != FINALIZE_LIMIT; ++i) {
#ifdef JS_THREADSAFE
            /*
             * The arena cannot be empty if the background finalization is not yet
             * done.
             */
            if (backgroundFinalizeState[i] != BFS_DONE)
                return false;
#endif
            if (arenaLists[i].head)
                return false;
        }
        return true;
    }

#ifdef DEBUG
    bool checkArenaListAllUnmarked() const {
        for (size_t i = 0; i != FINALIZE_LIMIT; ++i) {
# ifdef JS_THREADSAFE
            /* The background finalization must have stopped at this point. */
            JS_ASSERT(backgroundFinalizeState[i] == BFS_DONE ||
                      backgroundFinalizeState[i] == BFS_JUST_FINISHED);
# endif
            for (ArenaHeader *aheader = arenaLists[i].head; aheader; aheader = aheader->next) {
                if (!aheader->chunk()->bitmap.noBitsSet(aheader))
                    return false;
            }
        }
        return true;
    }
#endif

#ifdef JS_THREADSAFE
    bool doneBackgroundFinalize(AllocKind kind) const {
        return backgroundFinalizeState[kind] == BFS_DONE;
    }
#endif

    /*
     * Return the free list back to the arena so the GC finalization will not
     * run the finalizers over unitialized bytes from free things.
     */
    void purge() {
        for (size_t i = 0; i != FINALIZE_LIMIT; ++i) {
            FreeSpan *headSpan = &freeLists[i];
            if (!headSpan->isEmpty()) {
                ArenaHeader *aheader = headSpan->arenaHeader();
                JS_ASSERT(!aheader->hasFreeThings());
                aheader->setFirstFreeSpan(headSpan);
                headSpan->initAsEmpty();
            }
        }
    }

    /*
     * Temporarily copy the free list heads to the arenas so the code can see
     * the proper value in ArenaHeader::freeList when accessing the latter
     * outside the GC.
     */
    void copyFreeListsToArenas() {
        for (size_t i = 0; i != FINALIZE_LIMIT; ++i)
            copyFreeListToArena(AllocKind(i));
    }

    void copyFreeListToArena(AllocKind thingKind) {
        FreeSpan *headSpan = &freeLists[thingKind];
        if (!headSpan->isEmpty()) {
            ArenaHeader *aheader = headSpan->arenaHeader();
            JS_ASSERT(!aheader->hasFreeThings());
            aheader->setFirstFreeSpan(headSpan);
        }
    }

    /*
     * Clear the free lists in arenas that were temporarily set there using
     * copyToArenas.
     */
    void clearFreeListsInArenas() {
        for (size_t i = 0; i != FINALIZE_LIMIT; ++i)
            clearFreeListInArena(AllocKind(i));
    }


    void clearFreeListInArena(AllocKind kind) {
        FreeSpan *headSpan = &freeLists[kind];
        if (!headSpan->isEmpty()) {
            ArenaHeader *aheader = headSpan->arenaHeader();
            JS_ASSERT(aheader->getFirstFreeSpan().isSameNonEmptySpan(headSpan));
            aheader->setAsFullyUsed();
        }
    }

    /*
     * Check that the free list is either empty or were synchronized with the
     * arena using copyToArena().
     */
    bool isSynchronizedFreeList(AllocKind kind) {
        FreeSpan *headSpan = &freeLists[kind];
        if (headSpan->isEmpty())
            return true;
        ArenaHeader *aheader = headSpan->arenaHeader();
        if (aheader->hasFreeThings()) {
            /*
             * If the arena has a free list, it must be the same as one in
             * lists.
             */
            JS_ASSERT(aheader->getFirstFreeSpan().isSameNonEmptySpan(headSpan));
            return true;
        }
        return false;
    }

    JS_ALWAYS_INLINE void *allocateFromFreeList(AllocKind thingKind, size_t thingSize) {
        return freeLists[thingKind].allocate(thingSize);
    }

    static void *refillFreeList(JSContext *cx, AllocKind thingKind);

    void checkEmptyFreeLists() {
#ifdef DEBUG
        for (size_t i = 0; i < mozilla::ArrayLength(freeLists); ++i)
            JS_ASSERT(freeLists[i].isEmpty());
#endif
    }

    void checkEmptyFreeList(AllocKind kind) {
        JS_ASSERT(freeLists[kind].isEmpty());
    }

    void finalizeObjects(JSContext *cx);
    void finalizeStrings(JSContext *cx);
    void finalizeShapes(JSContext *cx);
    void finalizeScripts(JSContext *cx);

#ifdef JS_THREADSAFE
    static void backgroundFinalize(JSContext *cx, ArenaHeader *listHead);
#endif

  private:
    inline void finalizeNow(JSContext *cx, AllocKind thingKind);
    inline void finalizeLater(JSContext *cx, AllocKind thingKind);

    inline void *allocateFromArena(JSCompartment *comp, AllocKind thingKind);
};

/*
 * Initial allocation size for data structures holding chunks is set to hold
 * chunks with total capacity of 16MB to avoid buffer resizes during browser
 * startup.
 */
const size_t INITIAL_CHUNK_CAPACITY = 16 * 1024 * 1024 / GC_CHUNK_SIZE;

/* The number of GC cycles an empty chunk can survive before been released. */
const size_t MAX_EMPTY_CHUNK_AGE = 4;

} /* namespace gc */

struct GCPtrHasher
{
    typedef void *Lookup;

    static HashNumber hash(void *key) {
        return HashNumber(uintptr_t(key) >> JS_GCTHING_ZEROBITS);
    }

    static bool match(void *l, void *k) { return l == k; }
};

typedef HashMap<void *, uint32, GCPtrHasher, SystemAllocPolicy> GCLocks;

struct RootInfo {
    RootInfo() {}
    RootInfo(const char *name, JSGCRootType type) : name(name), type(type) {}
    const char *name;
    JSGCRootType type;
};

typedef js::HashMap<void *,
                    RootInfo,
                    js::DefaultHasher<void *>,
                    js::SystemAllocPolicy> RootedValueMap;

/* If HashNumber grows, need to change WrapperHasher. */
JS_STATIC_ASSERT(sizeof(HashNumber) == 4);

struct WrapperHasher
{
    typedef Value Lookup;

    static HashNumber hash(Value key) {
        uint64 bits = key.asRawBits();
        return (uint32)bits ^ (uint32)(bits >> 32);
    }

    static bool match(const Value &l, const Value &k) { return l == k; }
};

typedef HashMap<Value, Value, WrapperHasher, SystemAllocPolicy> WrapperMap;

class AutoValueVector;
class AutoIdVector;

} /* namespace js */

#ifdef DEBUG
extern bool
CheckAllocation(JSContext *cx);
#endif

extern JS_FRIEND_API(JSGCTraceKind)
js_GetGCThingTraceKind(void *thing);

extern JSBool
js_InitGC(JSRuntime *rt, uint32 maxbytes);

extern void
js_FinishGC(JSRuntime *rt);

extern JSBool
js_AddRoot(JSContext *cx, js::Value *vp, const char *name);

extern JSBool
js_AddGCThingRoot(JSContext *cx, void **rp, const char *name);

#ifdef DEBUG
extern void
js_DumpNamedRoots(JSRuntime *rt,
                  void (*dump)(const char *name, void *rp, JSGCRootType type, void *data),
                  void *data);
#endif

extern uint32
js_MapGCRoots(JSRuntime *rt, JSGCRootMapFun map, void *data);

/* Table of pointers with count valid members. */
typedef struct JSPtrTable {
    size_t      count;
    void        **array;
} JSPtrTable;

extern JSBool
js_RegisterCloseableIterator(JSContext *cx, JSObject *obj);

#ifdef JS_TRACER
extern JSBool
js_ReserveObjects(JSContext *cx, size_t nobjects);
#endif

extern JSBool
js_LockGCThingRT(JSRuntime *rt, void *thing);

extern void
js_UnlockGCThingRT(JSRuntime *rt, void *thing);

extern JS_FRIEND_API(bool)
IsAboutToBeFinalized(JSContext *cx, const void *thing);

extern JS_FRIEND_API(bool)
js_GCThingIsMarked(void *thing, uintN color);

extern void
js_TraceStackFrame(JSTracer *trc, js::StackFrame *fp);

namespace js {

extern JS_REQUIRES_STACK void
MarkRuntime(JSTracer *trc);

extern void
TraceRuntime(JSTracer *trc);

extern JS_REQUIRES_STACK JS_FRIEND_API(void)
MarkContext(JSTracer *trc, JSContext *acx);

/* Must be called with GC lock taken. */
extern void
TriggerGC(JSRuntime *rt, js::gcstats::Reason reason);

/* Must be called with GC lock taken. */
extern void
TriggerCompartmentGC(JSCompartment *comp, js::gcstats::Reason reason);

extern void
MaybeGC(JSContext *cx);

} /* namespace js */

/*
 * Kinds of js_GC invocation.
 */
typedef enum JSGCInvocationKind {
    /* Normal invocation. */
    GC_NORMAL           = 0,

    /*
     * Called from js_DestroyContext for last JSContext in a JSRuntime, when
     * it is imperative that rt->gcPoke gets cleared early in js_GC.
     */
    GC_LAST_CONTEXT     = 1,

    /* Minimize GC triggers and release empty GC chunks right away. */
    GC_SHRINK             = 2
} JSGCInvocationKind;

/* Pass NULL for |comp| to get a full GC. */
extern void
js_GC(JSContext *cx, JSCompartment *comp, JSGCInvocationKind gckind, js::gcstats::Reason r);

#ifdef JS_THREADSAFE
/*
 * This is a helper for code at can potentially run outside JS request to
 * ensure that the GC is not running when the function returns.
 *
 * This function must be called with the GC lock held.
 */
extern void
js_WaitForGC(JSRuntime *rt);

#else /* !JS_THREADSAFE */

# define js_WaitForGC(rt)    ((void) 0)

#endif

namespace js {

#ifdef JS_THREADSAFE

class GCHelperThread {
    enum State {
        IDLE,
        SWEEPING,
        ALLOCATING,
        CANCEL_ALLOCATION,
        SHUTDOWN
    };

    /*
     * During the finalization we do not free immediately. Rather we add the
     * corresponding pointers to a buffer which we later release on a
     * separated thread.
     *
     * The buffer is implemented as a vector of 64K arrays of pointers, not as
     * a simple vector, to avoid realloc calls during the vector growth and to
     * not bloat the binary size of the inlined freeLater method. Any OOM
     * during buffer growth results in the pointer being freed immediately.
     */
    static const size_t FREE_ARRAY_SIZE = size_t(1) << 16;
    static const size_t FREE_ARRAY_LENGTH = FREE_ARRAY_SIZE / sizeof(void *);

    JSRuntime         *const rt;
    PRThread          *thread;
    PRCondVar         *wakeup;
    PRCondVar         *done;
    volatile State    state;

    JSContext         *context;
    bool              shrinkFlag;

    Vector<void **, 16, js::SystemAllocPolicy> freeVector;
    void            **freeCursor;
    void            **freeCursorEnd;

    Vector<js::gc::ArenaHeader *, 64, js::SystemAllocPolicy> finalizeVector;

    bool    backgroundAllocation;

    friend struct js::gc::ArenaLists;

    JS_FRIEND_API(void)
    replenishAndFreeLater(void *ptr);

    static void freeElementsAndArray(void **array, void **end) {
        JS_ASSERT(array <= end);
        for (void **p = array; p != end; ++p)
            js::Foreground::free_(*p);
        js::Foreground::free_(array);
    }

    static void threadMain(void* arg);
    void threadLoop();

    /* Must be called with the GC lock taken. */
    void doSweep();

  public:
    GCHelperThread(JSRuntime *rt)
      : rt(rt),
        thread(NULL),
        wakeup(NULL),
        done(NULL),
        state(IDLE),
        freeCursor(NULL),
        freeCursorEnd(NULL),
        backgroundAllocation(true)
    { }

    bool init();
    void finish();

    /* Must be called with the GC lock taken. */
    inline void startBackgroundSweep(bool shouldShrink);

    /* Must be called with the GC lock taken. */
    void waitBackgroundSweepEnd();

    /* Must be called with the GC lock taken. */
    void waitBackgroundSweepOrAllocEnd();

    /* Must be called with the GC lock taken. */
    inline void startBackgroundAllocationIfIdle();

    bool canBackgroundAllocate() const {
        return backgroundAllocation;
    }

    void disableBackgroundAllocation() {
        backgroundAllocation = false;
    }

    /*
     * Outside the GC lock may give true answer when in fact the sweeping has
     * been done.
     */
    bool sweeping() const {
        return state == SWEEPING;
    }

    bool shouldShrink() const {
        JS_ASSERT(sweeping());
        return shrinkFlag;
    }

    void freeLater(void *ptr) {
        JS_ASSERT(!sweeping());
        if (freeCursor != freeCursorEnd)
            *freeCursor++ = ptr;
        else
            replenishAndFreeLater(ptr);
    }

    /* Must be called with the GC lock taken. */
    bool prepareForBackgroundSweep(JSContext *cx);
};

#endif /* JS_THREADSAFE */

struct GCChunkHasher {
    typedef gc::Chunk *Lookup;

    /*
     * Strip zeros for better distribution after multiplying by the golden
     * ratio.
     */
    static HashNumber hash(gc::Chunk *chunk) {
        JS_ASSERT(!(jsuword(chunk) & GC_CHUNK_MASK));
        return HashNumber(jsuword(chunk) >> GC_CHUNK_SHIFT);
    }

    static bool match(gc::Chunk *k, gc::Chunk *l) {
        JS_ASSERT(!(jsuword(k) & GC_CHUNK_MASK));
        JS_ASSERT(!(jsuword(l) & GC_CHUNK_MASK));
        return k == l;
    }
};

typedef HashSet<js::gc::Chunk *, GCChunkHasher, SystemAllocPolicy> GCChunkSet;

struct ConservativeGCThreadData {

    /*
     * The GC scans conservatively between ThreadData::nativeStackBase and
     * nativeStackTop unless the latter is NULL.
     */
    jsuword             *nativeStackTop;

    union {
        jmp_buf         jmpbuf;
        jsuword         words[JS_HOWMANY(sizeof(jmp_buf), sizeof(jsuword))];
    } registerSnapshot;

    /*
     * Cycle collector uses this to communicate that the native stack of the
     * GC thread should be scanned only if the thread have more than the given
     * threshold of requests.
     */
    unsigned requestThreshold;

    ConservativeGCThreadData()
      : nativeStackTop(NULL), requestThreshold(0)
    {
    }

    ~ConservativeGCThreadData() {
#ifdef JS_THREADSAFE
        /*
         * The conservative GC scanner should be disabled when the thread leaves
         * the last request.
         */
        JS_ASSERT(!hasStackToScan());
#endif
    }

    JS_NEVER_INLINE void recordStackTop();

#ifdef JS_THREADSAFE
    void updateForRequestEnd(unsigned suspendCount) {
        if (suspendCount)
            recordStackTop();
        else
            nativeStackTop = NULL;
    }
#endif

    bool hasStackToScan() const {
        return !!nativeStackTop;
    }
};

template<class T>
struct MarkStack {
    T *stack;
    uintN tos, limit;

    bool push(T item) {
        if (tos == limit)
            return false;
        stack[tos++] = item;
        return true;
    }

    bool isEmpty() { return tos == 0; }

    T pop() {
        JS_ASSERT(!isEmpty());
        return stack[--tos];
    }

    T &peek() {
        JS_ASSERT(!isEmpty());
        return stack[tos-1];
    }

    MarkStack(void **buffer, size_t size)
    {
        tos = 0;
        limit = size / sizeof(T) - 1;
        stack = (T *)buffer;
    }
};

struct LargeMarkItem
{
    JSObject *obj;
    uintN markpos;

    LargeMarkItem(JSObject *obj) : obj(obj), markpos(0) {}
};

static const size_t OBJECT_MARK_STACK_SIZE = 32768 * sizeof(JSObject *);
static const size_t ROPES_MARK_STACK_SIZE = 1024 * sizeof(JSString *);
static const size_t XML_MARK_STACK_SIZE = 1024 * sizeof(JSXML *);
static const size_t TYPE_MARK_STACK_SIZE = 1024 * sizeof(types::TypeObject *);
static const size_t LARGE_MARK_STACK_SIZE = 64 * sizeof(LargeMarkItem);

struct GCMarker : public JSTracer {
  private:
    /* The color is only applied to objects, functions and xml. */
    uint32 color;

  public:
    /* Pointer to the top of the stack of arenas we are delaying marking on. */
    js::gc::Arena *unmarkedArenaStackTop;
    /* Count of arenas that are currently in the stack. */
    DebugOnly<size_t> markLaterArenas;

#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    js::gc::ConservativeGCStats conservativeStats;
    Vector<void *, 0, SystemAllocPolicy> conservativeRoots;
    const char *conservativeDumpFileName;
    void dumpConservativeRoots();
#endif

    MarkStack<JSObject *> objStack;
    MarkStack<JSRope *> ropeStack;
    MarkStack<types::TypeObject *> typeStack;
    MarkStack<JSXML *> xmlStack;
    MarkStack<LargeMarkItem> largeStack;

  public:
    explicit GCMarker(JSContext *cx);
    ~GCMarker();

    uint32 getMarkColor() const {
        return color;
    }

    /*
     * The only valid color transition during a GC is from black to gray. It is
     * wrong to switch the mark color from gray to black. The reason is that the
     * cycle collector depends on the invariant that there are no black to gray
     * edges in the GC heap. This invariant lets the CC not trace through black
     * objects. If this invariant is violated, the cycle collector may free
     * objects that are still reachable.
     *
     * We don't assert this yet, but we should.
     */
    void setMarkColor(uint32 newColor) {
        //JS_ASSERT(color == BLACK && newColor == GRAY);
        color = newColor;
    }

    void delayMarkingChildren(const void *thing);

    void markDelayedChildren();

    bool isMarkStackEmpty() {
        return objStack.isEmpty() &&
               ropeStack.isEmpty() &&
               typeStack.isEmpty() &&
               xmlStack.isEmpty() &&
               largeStack.isEmpty();
    }

    void drainMarkStack();

    void pushObject(JSObject *obj) {
        if (!objStack.push(obj))
            delayMarkingChildren(obj);
    }

    void pushRope(JSRope *rope) {
        if (!ropeStack.push(rope))
            delayMarkingChildren(rope);
    }

    void pushType(types::TypeObject *type) {
        if (!typeStack.push(type))
            delayMarkingChildren(type);
    }

    void pushXML(JSXML *xml) {
        if (!xmlStack.push(xml))
            delayMarkingChildren(xml);
    }
};

void
MarkStackRangeConservatively(JSTracer *trc, Value *begin, Value *end);

typedef void (*IterateCompartmentCallback)(JSContext *cx, void *data, JSCompartment *compartment);
typedef void (*IterateArenaCallback)(JSContext *cx, void *data, gc::Arena *arena,
                                     JSGCTraceKind traceKind, size_t thingSize);
typedef void (*IterateCellCallback)(JSContext *cx, void *data, void *thing,
                                    JSGCTraceKind traceKind, size_t thingSize);

/*
 * This function calls |compartmentCallback| on every compartment,
 * |arenaCallback| on every in-use arena, and |cellCallback| on every in-use
 * cell in the GC heap.
 */
extern JS_FRIEND_API(void)
IterateCompartmentsArenasCells(JSContext *cx, void *data,
                               IterateCompartmentCallback compartmentCallback,
                               IterateArenaCallback arenaCallback,
                               IterateCellCallback cellCallback);

/*
 * Invoke cellCallback on every in-use object of the specified thing kind for
 * the given compartment or for all compartments if it is null.
 */
extern JS_FRIEND_API(void)
IterateCells(JSContext *cx, JSCompartment *compartment, gc::AllocKind thingKind,
             void *data, IterateCellCallback cellCallback);

} /* namespace js */

extern void
js_FinalizeStringRT(JSRuntime *rt, JSString *str);

/*
 * Macro to test if a traversal is the marking phase of the GC.
 */
#define IS_GC_MARKING_TRACER(trc) ((trc)->callback == NULL)

namespace js {
namespace gc {

JSCompartment *
NewCompartment(JSContext *cx, JSPrincipals *principals);

/* Tries to run a GC no matter what (used for GC zeal). */
void
RunDebugGC(JSContext *cx);

} /* namespace gc */

static inline JSCompartment *
GetObjectCompartment(JSObject *obj) { return reinterpret_cast<js::gc::Cell *>(obj)->compartment(); }

} /* namespace js */

#endif /* jsgc_h___ */
