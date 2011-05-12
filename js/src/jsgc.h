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

#include "jstypes.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsdhash.h"
#include "jsbit.h"
#include "jsgcchunk.h"
#include "jsutil.h"
#include "jsvector.h"
#include "jsversion.h"
#include "jsobj.h"
#include "jsfun.h"
#include "jsgcstats.h"
#include "jscell.h"

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

/* The kind of GC thing with a finalizer. */
enum FinalizeKind {
    FINALIZE_OBJECT0,
    FINALIZE_OBJECT0_BACKGROUND,
    FINALIZE_OBJECT2,
    FINALIZE_OBJECT2_BACKGROUND,
    FINALIZE_OBJECT4,
    FINALIZE_OBJECT4_BACKGROUND,
    FINALIZE_OBJECT8,
    FINALIZE_OBJECT8_BACKGROUND,
    FINALIZE_OBJECT12,
    FINALIZE_OBJECT12_BACKGROUND,
    FINALIZE_OBJECT16,
    FINALIZE_OBJECT16_BACKGROUND,
    FINALIZE_OBJECT_LAST = FINALIZE_OBJECT16_BACKGROUND,
    FINALIZE_FUNCTION,
    FINALIZE_FUNCTION_AND_OBJECT_LAST = FINALIZE_FUNCTION,
    FINALIZE_SHAPE,
#if JS_HAS_XML_SUPPORT
    FINALIZE_XML,
#endif
    FINALIZE_SHORT_STRING,
    FINALIZE_STRING,
    FINALIZE_EXTERNAL_STRING,
    FINALIZE_LIMIT
};

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

template <typename T> struct Arena;

/* Every arena has a header. */
struct ArenaHeader {
    JSCompartment   *compartment;
    ArenaHeader     *next;
    FreeCell        *freeList;

  private:
    unsigned        thingKind;

  public:
    inline uintptr_t address() const;
    inline Chunk *chunk() const;

    template <typename T>
    Arena<T> *getArena() {
        return reinterpret_cast<Arena<T> *>(address());
    }

    unsigned getThingKind() const {
        return thingKind;
    }

    void setThingKind(unsigned kind) {
        thingKind = kind;
    }

    inline MarkingDelay *getMarkingDelay() const;

#ifdef DEBUG
    JS_FRIEND_API(size_t) getThingSize() const;
#endif

#if defined DEBUG || defined JS_GCMETER
    static size_t CountListLength(const ArenaHeader *aheader) {
        size_t n = 0;
        for (; aheader; aheader = aheader->next)
            ++n;
        return n;
    }
#endif
};

template <typename T, size_t N, size_t R1, size_t R2>
struct Things {
    char filler1[R1];
    T    things[N];
    char filler[R2];
};

template <typename T, size_t N, size_t R1>
struct Things<T, N, R1, 0> {
    char filler1[R1];
    T    things[N];
};

template <typename T, size_t N, size_t R2>
struct Things<T, N, 0, R2> {
    T    things[N];
    char filler2[R2];
};

template <typename T, size_t N>
struct Things<T, N, 0, 0> {
    T things[N];
};

template <typename T>
struct Arena {
    /*
     * Layout of an arena:
     * An arena is 4K. We want it to have a header followed by a list of T
     * objects. However, each object should be aligned to a sizeof(T)-boundary.
     * To achieve this, we pad before and after the object array.
     *
     * +-------------+-----+----+----+-----+----+-----+
     * | ArenaHeader | pad | T0 | T1 | ... | Tn | pad |
     * +-------------+-----+----+----+-----+----+-----+
     *
     * <----------------------------------------------> = 4096 bytes
     *               <-----> = Filler1Size
     * <-------------------> = HeaderSize
     *                     <--------------------------> = SpaceAfterHeader
     *                                          <-----> = Filler2Size
     */
    static const size_t Filler1Size =
        tl::If< sizeof(ArenaHeader) % sizeof(T) == 0, size_t,
                0,
                sizeof(T) - sizeof(ArenaHeader) % sizeof(T) >::result;
    static const size_t HeaderSize = sizeof(ArenaHeader) + Filler1Size;
    static const size_t SpaceAfterHeader = ArenaSize - HeaderSize;
    static const size_t Filler2Size = SpaceAfterHeader % sizeof(T);
    static const size_t ThingsPerArena = SpaceAfterHeader / sizeof(T);
    static const size_t FirstThingOffset = HeaderSize;
    static const size_t ThingsSpan = ThingsPerArena * sizeof(T);

    ArenaHeader aheader;
    Things<T, ThingsPerArena, Filler1Size, Filler2Size> t;

    static void staticAsserts() {
        /*
         * Everything we store in the heap must be a multiple of the cell
         * size.
         */
        JS_STATIC_ASSERT(sizeof(T) % Cell::CellSize == 0);
        JS_STATIC_ASSERT(offsetof(Arena<T>, t.things) % sizeof(T) == 0);
        JS_STATIC_ASSERT(sizeof(Arena<T>) == ArenaSize);
    }

    inline FreeCell *buildFreeList();

    bool finalize(JSContext *cx);
};

/*
 * When recursive marking uses too much stack the marking is delayed and
 * the corresponding arenas are put into a stack using a linked via the
 * following per arena structure.
 */
struct MarkingDelay {
    ArenaHeader *link;

    void init()
    {
        link = NULL;
    }

    /*
     * To separate arenas without things to mark later from the arena at the
     * marked delay stack bottom we use for the latter a special sentinel
     * value. We set it to the header for the second arena in the chunk
     * starting the 0 address.
     */
    static ArenaHeader *stackBottom() {
        return reinterpret_cast<ArenaHeader *>(ArenaSize);
    }
};

struct EmptyArenaLists {
    /* Arenas with no internal freelist prepared. */
    ArenaHeader *cellFreeList;

    /* Arenas with internal freelists prepared for a given finalize kind. */
    ArenaHeader *freeLists[FINALIZE_LIMIT];

    void init() {
        PodZero(this);
    }

    ArenaHeader *getOtherArena() {
        ArenaHeader *aheader = cellFreeList;
        if (aheader) {
            cellFreeList = aheader->next;
            return aheader;
        }
        for (int i = 0; i < FINALIZE_LIMIT; i++) {
            aheader = freeLists[i];
            if (aheader) {
                freeLists[i] = aheader->next;
                return aheader;
            }
        }
        JS_NOT_REACHED("No arena");
        return NULL;
    }

    ArenaHeader *getTypedFreeList(unsigned thingKind) {
        JS_ASSERT(thingKind < FINALIZE_LIMIT);
        ArenaHeader *aheader = freeLists[thingKind];
        if (aheader)
            freeLists[thingKind] = aheader->next;
        return aheader;
    }

    void insert(ArenaHeader *aheader) {
        unsigned thingKind = aheader->getThingKind();
        aheader->next = freeLists[thingKind];
        freeLists[thingKind] = aheader;
    }
};

/* The chunk header (located at the end of the chunk to preserve arena alignment). */
struct ChunkInfo {
    Chunk           *link;
    JSRuntime       *runtime;
    EmptyArenaLists emptyArenaLists;
    size_t          age;
    size_t          numFree;
};

const size_t BytesPerArena = ArenaSize + ArenaBitmapBytes + sizeof(MarkingDelay);
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
    Arena<FreeCell> arenas[ArenasPerChunk];
    ChunkBitmap     bitmap;
    MarkingDelay    markingDelay[ArenasPerChunk];
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

    void init(JSRuntime *rt);
    bool unused();
    bool hasAvailableArenas();
    bool withinArenasRange(Cell *cell);

    template <typename T>
    ArenaHeader *allocateArena(JSContext *cx, unsigned thingKind);

    void releaseArena(ArenaHeader *aheader);

    JSRuntime *getRuntime();
};
JS_STATIC_ASSERT(sizeof(Chunk) <= GC_CHUNK_SIZE);
JS_STATIC_ASSERT(sizeof(Chunk) + BytesPerArena > GC_CHUNK_SIZE);

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

#ifdef DEBUG
inline bool
Cell::isAligned() const
{
    uintptr_t offset = address() & ArenaMask;
    return offset % arenaHeader()->getThingSize() == 0;
}
#endif

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

inline MarkingDelay *
ArenaHeader::getMarkingDelay() const
{
    return &chunk()->markingDelay[Chunk::arenaIndex(address())];
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

#define JSTRACE_XML         3

/*
 * One past the maximum trace kind.
 */
#define JSTRACE_LIMIT       4

/*
 * Lower limit after which we limit the heap growth
 */
const size_t GC_ARENA_ALLOCATION_TRIGGER = 30 * js::GC_CHUNK_SIZE;

/*
 * A GC is triggered once the number of newly allocated arenas
 * is GC_HEAP_GROWTH_FACTOR times the number of live arenas after
 * the last GC starting after the lower limit of
 * GC_ARENA_ALLOCATION_TRIGGER.
 */
const float GC_HEAP_GROWTH_FACTOR = 3.0f;

static inline size_t
GetFinalizableTraceKind(size_t thingKind)
{
    JS_STATIC_ASSERT(JSExternalString::TYPE_LIMIT == 8);

    static const uint8 map[FINALIZE_LIMIT] = {
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
        JSTRACE_SHAPE,      /* FINALIZE_SHAPE */
#if JS_HAS_XML_SUPPORT      /* FINALIZE_XML */
        JSTRACE_XML,
#endif
        JSTRACE_STRING,     /* FINALIZE_SHORT_STRING */
        JSTRACE_STRING,     /* FINALIZE_STRING */
        JSTRACE_STRING,     /* FINALIZE_EXTERNAL_STRING */
    };

    JS_ASSERT(thingKind < FINALIZE_LIMIT);
    return map[thingKind];
}

inline uint32
GetGCThingTraceKind(const void *thing);

static inline JSRuntime *
GetGCThingRuntime(void *thing)
{
    return reinterpret_cast<FreeCell *>(thing)->chunk()->info.runtime;
}

/* The arenas in a list have uniform kind. */
class ArenaList {
  private:
    ArenaHeader     *head;      /* list start */
    ArenaHeader     **cursor;   /* arena with free things */

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

    volatile BackgroundFinalizeState backgroundFinalizeState;
#endif

  public:
#ifdef JS_GCMETER
    JSGCArenaStats  stats;
#endif

    void init() {
        head = NULL;
        cursor = &head;
#ifdef JS_THREADSAFE
        backgroundFinalizeState = BFS_DONE;
#endif
#ifdef JS_GCMETER
        PodZero(&stats);
#endif
    }

    inline ArenaHeader *searchForFreeArena();

    template <typename T>
    inline ArenaHeader *getArenaWithFreeList(JSContext *cx, unsigned thingKind);

    template<typename T>
    void finalizeNow(JSContext *cx);

#ifdef JS_THREADSAFE
    template<typename T>
    inline void finalizeLater(JSContext *cx);

    static void backgroundFinalize(JSContext *cx, ArenaHeader *listHead);

    bool willBeFinalizedLater() const {
        return backgroundFinalizeState == BFS_RUN;
    }
#endif

#ifdef DEBUG
    bool markedThingsInArenaList() {
# ifdef JS_THREADSAFE
        /* The background finalization must have stopped at this point. */
        JS_ASSERT(backgroundFinalizeState == BFS_DONE ||
                  backgroundFinalizeState == BFS_JUST_FINISHED);
# endif
        for (ArenaHeader *aheader = head; aheader; aheader = aheader->next) {
            if (!aheader->chunk()->bitmap.noBitsSet(aheader))
                return true;
        }
        return false;
    }
#endif /* DEBUG */

    void releaseAll(unsigned thingKind) {
# ifdef JS_THREADSAFE
        /*
         * We can only call this during the shutdown after the last GC when
         * the background finalization is disabled.
         */
        JS_ASSERT(backgroundFinalizeState == BFS_DONE);
# endif
        while (ArenaHeader *aheader = head) {
            head = aheader->next;
            aheader->chunk()->releaseArena(aheader);
        }
        cursor = &head;
    }

    bool isEmpty() const {
#ifdef JS_THREADSAFE
        /*
         * The arena cannot be empty if the background finalization is not yet
         * done.
         */
        if (backgroundFinalizeState != BFS_DONE)
            return false;
#endif
        return !head;
    }
};

struct FreeLists {
    FreeCell       **finalizables[FINALIZE_LIMIT];

    void purge();

    inline FreeCell *getNext(uint32 kind) {
        FreeCell *top = NULL;
        if (finalizables[kind]) {
            top = *finalizables[kind];
            if (top) {
                *finalizables[kind] = top->link;
            } else {
                finalizables[kind] = NULL;
            }
        }
        return top;
    }

    void populate(ArenaHeader *aheader, uint32 thingKind) {
        finalizables[thingKind] = &aheader->freeList;
    }

#ifdef DEBUG
    bool isEmpty() const {
        for (size_t i = 0; i != JS_ARRAY_LENGTH(finalizables); ++i) {
            if (finalizables[i])
                return false;
        }
        return true;
    }
#endif
};
}

typedef Vector<gc::Chunk *, 32, SystemAllocPolicy> GCChunks;

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
        uint64 bits = JSVAL_BITS(Jsvalify(key));
        return (uint32)bits ^ (uint32)(bits >> 32);
    }

    static bool match(const Value &l, const Value &k) { return l == k; }
};

typedef HashMap<Value, Value, WrapperHasher, SystemAllocPolicy> WrapperMap;

class AutoValueVector;
class AutoIdVector;
}

static inline void
CheckGCFreeListLink(js::gc::FreeCell *cell)
{
    /*
     * The GC things on the free lists come from one arena and the things on
     * the free list are linked in ascending address order.
     */
    JS_ASSERT_IF(cell->link, cell->arenaHeader() == cell->link->arenaHeader());
    JS_ASSERT_IF(cell->link, cell < cell->link);
}

extern bool
RefillFinalizableFreeList(JSContext *cx, unsigned thingKind);

#ifdef DEBUG
extern bool
CheckAllocation(JSContext *cx);
#endif

extern JS_FRIEND_API(uint32)
js_GetGCThingTraceKind(void *thing);

#if 1
/*
 * Since we're forcing a GC from JS_GC anyway, don't bother wasting cycles
 * loading oldval.  XXX remove implied force, fix jsinterp.c's "second arg
 * ignored", etc.
 */
#define GC_POKE(cx, oldval) ((cx)->runtime->gcPoke = JS_TRUE)
#else
#define GC_POKE(cx, oldval) ((cx)->runtime->gcPoke = JSVAL_IS_GCTHING(oldval))
#endif

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
TriggerGC(JSRuntime *rt);

/* Must be called with GC lock taken. */
extern void
TriggerCompartmentGC(JSCompartment *comp);

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
    GC_LAST_CONTEXT     = 1
} JSGCInvocationKind;

/* Pass NULL for |comp| to get a full GC. */
extern void
js_GC(JSContext *cx, JSCompartment *comp, JSGCInvocationKind gckind);

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

extern void
js_DestroyScriptsToGC(JSContext *cx, JSCompartment *comp);


namespace js {

#ifdef JS_THREADSAFE

/*
 * During the finalization we do not free immediately. Rather we add the
 * corresponding pointers to a buffer which we later release on a separated
 * thread.
 *
 * The buffer is implemented as a vector of 64K arrays of pointers, not as a
 * simple vector, to avoid realloc calls during the vector growth and to not
 * bloat the binary size of the inlined freeLater method. Any OOM during
 * buffer growth results in the pointer being freed immediately.
 */
class GCHelperThread {
    static const size_t FREE_ARRAY_SIZE = size_t(1) << 16;
    static const size_t FREE_ARRAY_LENGTH = FREE_ARRAY_SIZE / sizeof(void *);

    JSContext         *cx;
    PRThread*         thread;
    PRCondVar*        wakeup;
    PRCondVar*        sweepingDone;
    bool              shutdown;

    Vector<void **, 16, js::SystemAllocPolicy> freeVector;
    void            **freeCursor;
    void            **freeCursorEnd;

    Vector<js::gc::ArenaHeader *, 64, js::SystemAllocPolicy> finalizeVector;

    friend class js::gc::ArenaList;

    JS_FRIEND_API(void)
    replenishAndFreeLater(void *ptr);

    static void freeElementsAndArray(void **array, void **end) {
        JS_ASSERT(array <= end);
        for (void **p = array; p != end; ++p)
            js::Foreground::free_(*p);
        js::Foreground::free_(array);
    }

    static void threadMain(void* arg);

    void threadLoop(JSRuntime *rt);
    void doSweep();

  public:
    GCHelperThread()
      : thread(NULL),
        wakeup(NULL),
        sweepingDone(NULL),
        shutdown(false),
        freeCursor(NULL),
        freeCursorEnd(NULL),
        sweeping(false) { }

    volatile bool     sweeping;
    bool init(JSRuntime *rt);
    void finish(JSRuntime *rt);

    /* Must be called with GC lock taken. */
    void startBackgroundSweep(JSRuntime *rt);

    void waitBackgroundSweepEnd(JSRuntime *rt, bool gcUnlocked = true);

    void freeLater(void *ptr) {
        JS_ASSERT(!sweeping);
        if (freeCursor != freeCursorEnd)
            *freeCursor++ = ptr;
        else
            replenishAndFreeLater(ptr);
    }

    void setContext(JSContext *context) { cx = context; }
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
static const size_t XML_MARK_STACK_SIZE = 1024 * sizeof(JSXML *);
static const size_t LARGE_MARK_STACK_SIZE = 64 * sizeof(LargeMarkItem);

struct GCMarker : public JSTracer {
  private:
    /* The color is only applied to objects, functions and xml. */
    uint32 color;

  public:
    /* See comments before delayMarkingChildren is jsgc.cpp. */
    js::gc::ArenaHeader *unmarkedArenaStackTop;
#ifdef DEBUG
    size_t              markLaterArenas;
#endif

#if defined(JS_DUMP_CONSERVATIVE_GC_ROOTS) || defined(JS_GCMETER)
    js::gc::ConservativeGCStats conservativeStats;
#endif

#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    Vector<void *, 0, SystemAllocPolicy> conservativeRoots;
    const char *conservativeDumpFileName;

    void dumpConservativeRoots();
#endif

    MarkStack<JSObject *> objStack;
    MarkStack<JSXML *> xmlStack;
    MarkStack<LargeMarkItem> largeStack;

  public:
    explicit GCMarker(JSContext *cx);
    ~GCMarker();

    uint32 getMarkColor() const {
        return color;
    }

    void setMarkColor(uint32 newColor) {
        /* We must process the mark stack here, otherwise we confuse colors. */
        drainMarkStack();
        color = newColor;
    }

    void delayMarkingChildren(const void *thing);

    void markDelayedChildren();

    bool isMarkStackEmpty() {
        return objStack.isEmpty() && xmlStack.isEmpty() && largeStack.isEmpty();
    }

    JS_FRIEND_API(void) drainMarkStack();

    void pushObject(JSObject *obj) {
        if (!objStack.push(obj))
            delayMarkingChildren(obj);
    }

    void pushXML(JSXML *xml) {
        if (!xmlStack.push(xml))
            delayMarkingChildren(xml);
    }
};

void
MarkStackRangeConservatively(JSTracer *trc, Value *begin, Value *end);

} /* namespace js */

extern void
js_FinalizeStringRT(JSRuntime *rt, JSString *str);

/*
 * This function is defined in jsdbgapi.cpp but is declared here to avoid
 * polluting jsdbgapi.h, a public API header, with internal functions.
 */
extern void
js_MarkTraps(JSTracer *trc);

namespace js {
namespace gc {

/*
 * Macro to test if a traversal is the marking phase of GC to avoid exposing
 * ScriptFilenameEntry to traversal implementations.
 */
#define IS_GC_MARKING_TRACER(trc) ((trc)->callback == NULL)

#if JS_HAS_XML_SUPPORT
# define JS_IS_VALID_TRACE_KIND(kind) ((uint32)(kind) < JSTRACE_LIMIT)
#else
# define JS_IS_VALID_TRACE_KIND(kind) ((uint32)(kind) <= JSTRACE_SHAPE)
#endif

/*
 * Set object's prototype while checking that doing so would not create
 * a cycle in the proto chain. The cycle check and proto change are done
 * only when all other requests are finished or suspended to ensure exclusive
 * access to the chain. If there is a cycle, return false without reporting
 * an error. Otherwise, set the proto and return true.
 */
extern bool
SetProtoCheckingForCycles(JSContext *cx, JSObject *obj, JSObject *proto);

JSCompartment *
NewCompartment(JSContext *cx, JSPrincipals *principals);

} /* namespace js */
} /* namespace gc */

inline JSCompartment *
JSObject::getCompartment() const
{
    return compartment();
}

#endif /* jsgc_h___ */
