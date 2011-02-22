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

/* Gross special case for Gecko, which defines malloc/calloc/free. */
#ifdef mozilla_mozalloc_macro_wrappers_h
#  define JS_GC_UNDEFD_MOZALLOC_WRAPPERS
/* The "anti-header" */
#  include "mozilla/mozalloc_undef_macro_wrappers.h"
#endif

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

struct Shape;

namespace gc {

/*
 * The kind of GC thing with a finalizer. The external strings follow the
 * ordinary string to simplify js_GetExternalStringGCType.
 */
enum FinalizeKind {
    FINALIZE_OBJECT0,
    FINALIZE_OBJECT2,
    FINALIZE_OBJECT4,
    FINALIZE_OBJECT8,
    FINALIZE_OBJECT12,
    FINALIZE_OBJECT16,
    FINALIZE_OBJECT_LAST = FINALIZE_OBJECT16,
    FINALIZE_FUNCTION,
#if JS_HAS_XML_SUPPORT
    FINALIZE_XML,
#endif
    FINALIZE_SHORT_STRING,
    FINALIZE_STRING,
    FINALIZE_EXTERNAL_STRING,
    FINALIZE_LIMIT
};

const uintN JS_FINALIZE_OBJECT_LIMIT = 6;

/* Every arena has a header. */
struct ArenaHeader {
    JSCompartment   *compartment;
    Arena<FreeCell> *next;
    FreeCell        *freeList;
    unsigned        thingKind;
    bool            isUsed;
    size_t          thingSize;
#ifdef DEBUG
    bool            hasFreeThings;
#endif
};

template <typename T>
union ThingOrCell {
    T               t;
    FreeCell        cell;
};

template <typename T, size_t N, size_t R>
struct Things {
    ThingOrCell<T>  things[N];
    char            filler[R];
};

template <typename T, size_t N>
struct Things<T, N, 0> {
    ThingOrCell<T>  things[N];
};

template <typename T>
struct Arena {
    static const size_t ArenaSize = 4096;

    struct AlignedArenaHeader {
        T align[(sizeof(ArenaHeader) + sizeof(T) - 1) / sizeof(T)];
    };

    /* We want things in the arena to be aligned, so align the header. */
    union {
        ArenaHeader aheader;
        AlignedArenaHeader align;
    };

    static const size_t ThingsPerArena = (ArenaSize - sizeof(AlignedArenaHeader)) / sizeof(T);
    static const size_t FillerSize = ArenaSize - sizeof(AlignedArenaHeader) - sizeof(T) * ThingsPerArena;
    Things<T, ThingsPerArena, FillerSize> t;

    inline Chunk *chunk() const;
    inline size_t arenaIndex() const;

    inline ArenaHeader *header() { return &aheader; };

    inline MarkingDelay *getMarkingDelay() const;
    inline ArenaBitmap *bitmap() const;

    inline ConservativeGCTest mark(T *thing, JSTracer *trc);
    void markDelayedChildren(JSTracer *trc);
    inline bool inFreeList(void *thing) const;
    inline T *getAlignedThing(void *thing);
#ifdef DEBUG
    inline bool assureThingIsAligned(void *thing);
#endif

    void init(JSCompartment *compartment, unsigned thingKind);
};
JS_STATIC_ASSERT(sizeof(Arena<FreeCell>) == 4096);

/*
 * Live objects are marked black. How many other additional colors are available
 * depends on the size of the GCThing.
 */
static const uint32 BLACK = 0;

/* An arena bitmap contains enough mark bits for all the cells in an arena. */
struct ArenaBitmap {
    static const size_t BitCount = Arena<FreeCell>::ArenaSize / Cell::CellSize;
    static const size_t BitWords = BitCount / JS_BITS_PER_WORD;

    uintptr_t bitmap[BitWords];

    JS_ALWAYS_INLINE bool isMarked(size_t bit, uint32 color) {
        bit += color;
        JS_ASSERT(bit < BitCount);
        uintptr_t *word = &bitmap[bit / JS_BITS_PER_WORD];
        return *word & (uintptr_t(1) << (bit % JS_BITS_PER_WORD));
    }

    JS_ALWAYS_INLINE bool markIfUnmarked(size_t bit, uint32 color) {
        JS_ASSERT(bit + color < BitCount);
        uintptr_t *word = &bitmap[bit / JS_BITS_PER_WORD];
        uintptr_t mask = (uintptr_t(1) << (bit % JS_BITS_PER_WORD));
        if (*word & mask)
            return false;
        *word |= mask;
        if (color != BLACK) {
            bit += color;
            word = &bitmap[bit / JS_BITS_PER_WORD];
            mask = (uintptr_t(1) << (bit % JS_BITS_PER_WORD));
            if (*word & mask)
                return false;
            *word |= mask;
        }
        return true;
    }

    JS_ALWAYS_INLINE void unmark(size_t bit, uint32 color) {
        bit += color;
        JS_ASSERT(bit < BitCount);
        uintptr_t *word = &bitmap[bit / JS_BITS_PER_WORD];
        *word &= ~(uintptr_t(1) << (bit % JS_BITS_PER_WORD));
    }

#ifdef DEBUG
    bool noBitsSet() {
        for (unsigned i = 0; i < BitWords; i++) {
            if (bitmap[i] != uintptr_t(0))
                return false;
        }
        return true;
    }
#endif
};

/* Ensure that bitmap covers the whole arena. */
JS_STATIC_ASSERT(Arena<FreeCell>::ArenaSize % Cell::CellSize == 0);
JS_STATIC_ASSERT(ArenaBitmap::BitCount % JS_BITS_PER_WORD == 0);

/* Marking delay is used to resume marking later when recursive marking uses too much stack. */
struct MarkingDelay {
    Arena<Cell> *link;
    uintptr_t   unmarkedChildren;
    jsuword     start;

    void init()
    {
        link = NULL;
        unmarkedChildren = 0;
    }
};

struct EmptyArenaLists {
    /* Arenas with no internal freelist prepared. */
    Arena<FreeCell> *cellFreeList;

    /* Arenas with internal freelists prepared for a given finalize kind. */
    Arena<FreeCell> *freeLists[FINALIZE_LIMIT];

    void init() {
        PodZero(this);
    }

    Arena<FreeCell> *getOtherArena() {
        Arena<FreeCell> *arena = cellFreeList;
        if (arena) {
            cellFreeList = arena->header()->next;
            return arena;
        }
        for (int i = 0; i < FINALIZE_LIMIT; i++) {
            if ((arena = (Arena<FreeCell> *) freeLists[i])) {
                freeLists[i] = freeLists[i]->header()->next;
                return arena;
            }
        }
        JS_NOT_REACHED("No arena");
        return NULL;
    }

    template <typename T>
    inline Arena<T> *getTypedFreeList(unsigned thingKind);

    template <typename T>
    inline Arena<T> *getNext(JSCompartment *comp, unsigned thingKind);

    template <typename T>
    inline void insert(Arena<T> *arena);
};

template <typename T>
inline Arena<T> *
EmptyArenaLists::getTypedFreeList(unsigned thingKind) {
    JS_ASSERT(thingKind < FINALIZE_LIMIT);
    Arena<T> *arena = (Arena<T>*) freeLists[thingKind];
    if (arena) {
        freeLists[thingKind] = freeLists[thingKind]->header()->next;
        return arena;
    }
    return NULL;
}

template<typename T>
inline Arena<T> *
EmptyArenaLists::getNext(JSCompartment *comp, unsigned thingKind) {
    Arena<T> *arena = getTypedFreeList<T>(thingKind);
    if (arena) {
        JS_ASSERT(arena->header()->isUsed == false);
        JS_ASSERT(arena->header()->thingSize == sizeof(T));
        arena->header()->isUsed = true;
        arena->header()->thingKind = thingKind;
        arena->header()->compartment = comp;
        return arena;
    }
    arena = (Arena<T> *)getOtherArena();
    JS_ASSERT(arena->header()->isUsed == false);
    arena->init(comp, thingKind);
    return arena;
}

template <typename T>
inline void
EmptyArenaLists::insert(Arena<T> *arena) {
    unsigned thingKind = arena->header()->thingKind;
    JS_ASSERT(thingKind < FINALIZE_LIMIT);
    arena->header()->next = freeLists[thingKind];
    freeLists[thingKind] = (Arena<FreeCell> *) arena;
}

/* The chunk header (located at the end of the chunk to preserve arena alignment). */
struct ChunkInfo {
    Chunk           *link;
    JSRuntime       *runtime;
    EmptyArenaLists emptyArenaLists;
    size_t          age;
    size_t          numFree;
};

/* Chunks contain arenas and associated data structures (mark bitmap, delayed marking state). */
struct Chunk {
    static const size_t BytesPerArena = sizeof(Arena<FreeCell>) +
                                        sizeof(ArenaBitmap) +
                                        sizeof(MarkingDelay);

    static const size_t ArenasPerChunk = (GC_CHUNK_SIZE - sizeof(ChunkInfo)) / BytesPerArena;

    Arena<FreeCell> arenas[ArenasPerChunk];
    ArenaBitmap     bitmaps[ArenasPerChunk];
    MarkingDelay    markingDelay[ArenasPerChunk];

    ChunkInfo       info;

    void clearMarkBitmap();
    void init(JSRuntime *rt);

    bool unused();
    bool hasAvailableArenas();
    bool withinArenasRange(Cell *cell);

    template <typename T>
    Arena<T> *allocateArena(JSCompartment *comp, unsigned thingKind);

    template <typename T>
    void releaseArena(Arena<T> *a);

    JSRuntime *getRuntime();
};
JS_STATIC_ASSERT(sizeof(Chunk) <= GC_CHUNK_SIZE);
JS_STATIC_ASSERT(sizeof(Chunk) + Chunk::BytesPerArena > GC_CHUNK_SIZE);

Arena<Cell> *
Cell::arena() const
{
    uintptr_t addr = uintptr_t(this);
    JS_ASSERT(addr % sizeof(FreeCell) == 0);
    addr &= ~(Arena<FreeCell>::ArenaSize - 1);
    return reinterpret_cast<Arena<Cell> *>(addr);
}

Chunk *
Cell::chunk() const
{
    uintptr_t addr = uintptr_t(this);
    JS_ASSERT(addr % sizeof(FreeCell) == 0);
    addr &= ~(GC_CHUNK_SIZE - 1);
    return reinterpret_cast<Chunk *>(addr);
}

ArenaBitmap *
Cell::bitmap() const
{
    return &chunk()->bitmaps[arena()->arenaIndex()];
}

STATIC_POSTCONDITION_ASSUME(return < ArenaBitmap::BitCount)
size_t
Cell::cellIndex() const
{
    return reinterpret_cast<const FreeCell *>(this) - reinterpret_cast<FreeCell *>(&arena()->t);
}

template <typename T>
Chunk *
Arena<T>::chunk() const
{
    uintptr_t addr = uintptr_t(this);
    JS_ASSERT(addr % sizeof(FreeCell) == 0);
    addr &= ~(GC_CHUNK_SIZE - 1);
    return reinterpret_cast<Chunk *>(addr);
}

template <typename T>
size_t
Arena<T>::arenaIndex() const
{
    return reinterpret_cast<const Arena<FreeCell> *>(this) - chunk()->arenas;
}

template <typename T>
MarkingDelay *
Arena<T>::getMarkingDelay() const
{
    return &chunk()->markingDelay[arenaIndex()];
}

template <typename T>
ArenaBitmap *
Arena<T>::bitmap() const
{
    return &chunk()->bitmaps[arenaIndex()];
}

template <typename T>
inline T *
Arena<T>::getAlignedThing(void *thing)
{
    jsuword start = reinterpret_cast<jsuword>(&t.things[0]);
    jsuword offset = reinterpret_cast<jsuword>(thing) - start;
    offset -= offset % aheader.thingSize;
    return reinterpret_cast<T *>(start + offset);
}

#ifdef DEBUG
template <typename T>
inline bool
Arena<T>::assureThingIsAligned(void *thing)
{
    return (getAlignedThing(thing) == thing);
}
#endif

static void
AssertValidColor(const void *thing, uint32 color)
{
    JS_ASSERT_IF(color, color < reinterpret_cast<const js::gc::FreeCell *>(thing)->arena()->header()->thingSize / sizeof(FreeCell));
}

inline bool
Cell::isMarked(uint32 color = BLACK) const
{
    AssertValidColor(this, color);
    return bitmap()->isMarked(cellIndex(), color);
}

bool
Cell::markIfUnmarked(uint32 color = BLACK) const
{
    AssertValidColor(this, color);
    return bitmap()->markIfUnmarked(cellIndex(), color);
}

void
Cell::unmark(uint32 color) const
{
    JS_ASSERT(color != BLACK);
    AssertValidColor(this, color);
    bitmap()->unmark(cellIndex(), color);
}

JSCompartment *
Cell::compartment() const
{
    return arena()->header()->compartment;
}

template <typename T>
static inline
Arena<T> *
GetArena(Cell *cell)
{
    return reinterpret_cast<Arena<T> *>(cell->arena());
}

#define JSTRACE_XML         2

/*
 * One past the maximum trace kind.
 */
#define JSTRACE_LIMIT       3

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
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT2 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT4 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT8 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT12 */
        JSTRACE_OBJECT,     /* FINALIZE_OBJECT16 */
        JSTRACE_OBJECT,     /* FINALIZE_FUNCTION */
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

static inline bool
IsFinalizableStringKind(unsigned thingKind)
{
    return unsigned(FINALIZE_SHORT_STRING) <= thingKind &&
           thingKind <= unsigned(FINALIZE_EXTERNAL_STRING);
}

/*
 * Get the type of the external string or -1 if the string was not created
 * with JS_NewExternalString.
 */
static inline intN
GetExternalStringGCType(JSExternalString *str)
{
    JS_STATIC_ASSERT(FINALIZE_STRING + 1 == FINALIZE_EXTERNAL_STRING);
    JS_ASSERT(!JSString::isStatic(str));

    unsigned thingKind = str->externalStringType;
    JS_ASSERT(IsFinalizableStringKind(thingKind));
    return intN(thingKind);
}

static inline uint32
GetGCThingTraceKind(void *thing)
{
    JS_ASSERT(thing);
    if (JSString::isStatic(thing))
        return JSTRACE_STRING;
    Cell *cell = reinterpret_cast<Cell *>(thing);
    return GetFinalizableTraceKind(cell->arena()->header()->thingKind);
}

static inline JSRuntime *
GetGCThingRuntime(void *thing)
{
    return reinterpret_cast<FreeCell *>(thing)->chunk()->info.runtime;
}

#ifdef DEBUG
extern bool
checkArenaListsForThing(JSCompartment *comp, jsuword thing);
#endif

/* The arenas in a list have uniform kind. */
struct ArenaList {
    Arena<FreeCell>       *head;          /* list start */
    Arena<FreeCell>       *cursor;        /* arena with free things */

    inline void init() {
        head = NULL;
        cursor = NULL;
    }

    inline Arena<FreeCell> *getNextWithFreeList() {
        Arena<FreeCell> *a;
        while (cursor != NULL) {
            ArenaHeader *aheader = cursor->header();
            a = cursor;
            cursor = aheader->next;
            if (aheader->freeList)
                return a;
        }
        return NULL;
    }

#ifdef DEBUG
    template <typename T>
    bool arenasContainThing(void *thing) {
        for (Arena<T> *a = (Arena<T> *) head; a; a = (Arena<T> *) a->header()->next) {
            JS_ASSERT(a->header()->isUsed);
            if (thing >= &a->t.things[0] && thing < &a->t.things[a->ThingsPerArena])
                return true;
        }
        return false;
    }

    bool markedThingsInArenaList() {
        for (Arena<FreeCell> *a = (Arena<FreeCell> *) head; a; a = (Arena<FreeCell> *) a->header()->next) {
            if (!a->bitmap()->noBitsSet())
                return true;
        }
        return false;
    }
#endif

    inline void insert(Arena<FreeCell> *a) {
        a->header()->next = head;
        head = a;
    }

    void releaseAll() {
        while (head) {
            Arena<FreeCell> *next = head->header()->next;
            head->chunk()->releaseArena(head);
            head = next;
        }
        head = NULL;
        cursor = NULL;
    }

    inline bool isEmpty() const {
        return (head == NULL);
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
#ifdef DEBUG
            if (top && !top->link)
                top->arena()->header()->hasFreeThings = false;
#endif
        }
        return top;
    }

    template <typename T>
    inline void populate(Arena<T> *a, uint32 thingKind) {
        finalizables[thingKind] = &a->header()->freeList;
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
    JS_ASSERT_IF(cell->link,
                 cell->arena() ==
                 cell->link->arena());
    JS_ASSERT_IF(cell->link, cell < cell->link);
}

extern bool
RefillFinalizableFreeList(JSContext *cx, unsigned thingKind);

#ifdef DEBUG
extern bool
CheckAllocation(JSContext *cx);
#endif

/*
 * Get the type of the external string or -1 if the string was not created
 * with JS_NewExternalString.
 */
extern intN
js_GetExternalStringGCType(JSString *str);

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
IsAboutToBeFinalized(JSContext *cx, void *thing);

extern JS_FRIEND_API(bool)
js_GCThingIsMarked(void *thing, uintN color);

extern void
js_TraceStackFrame(JSTracer *trc, JSStackFrame *fp);

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

    PRThread*         thread;
    PRCondVar*        wakeup;
    PRCondVar*        sweepingDone;
    bool              shutdown;
    bool              sweeping;

    Vector<void **, 16, js::SystemAllocPolicy> freeVector;
    void            **freeCursor;
    void            **freeCursorEnd;

    JS_FRIEND_API(void)
    replenishAndFreeLater(void *ptr);

    static void freeElementsAndArray(void **array, void **end) {
        JS_ASSERT(array <= end);
        for (void **p = array; p != end; ++p)
            js_free(*p);
        js_free(array);
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
        sweeping(false),
        freeCursor(NULL),
        freeCursorEnd(NULL) { }
    
    bool init(JSRuntime *rt);
    void finish(JSRuntime *rt);
    
    /* Must be called with GC lock taken. */
    void startBackgroundSweep(JSRuntime *rt);
    
    /* Must be called outside the GC lock. */
    void waitBackgroundSweepEnd(JSRuntime *rt);
    
    void freeLater(void *ptr) {
        JS_ASSERT(!sweeping);
        if (freeCursor != freeCursorEnd)
            *freeCursor++ = ptr;
        else
            replenishAndFreeLater(ptr);
    }
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
     * The GC scans conservatively between JSThreadData::nativeStackBase and
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

struct GCMarker : public JSTracer {
  private:
    /* The color is only applied to objects, functions and xml. */
    uint32 color;
  public:
    jsuword stackLimit;
    /* See comments before delayMarkingChildren is jsgc.cpp. */
    js::gc::Arena<js::gc::Cell> *unmarkedArenaStackTop;
#ifdef DEBUG
    size_t              markLaterCount;
#endif

#if defined(JS_DUMP_CONSERVATIVE_GC_ROOTS) || defined(JS_GCMETER)
    js::gc::ConservativeGCStats conservativeStats;
#endif

#ifdef JS_DUMP_CONSERVATIVE_GC_ROOTS
    struct ConservativeRoot { void *thing; uint32 thingKind; };
    Vector<ConservativeRoot, 0, SystemAllocPolicy> conservativeRoots;
    const char *conservativeDumpFileName;

    void dumpConservativeRoots();
#endif

  public:
    explicit GCMarker(JSContext *cx);
    ~GCMarker();

    uint32 getMarkColor() const {
        return color;
    }

    void setMarkColor(uint32 newColor) {
        /*
         * We must process any delayed marking here, otherwise we confuse
         * colors.
         */
        markDelayedChildren();
        color = newColor;
    }

    void delayMarkingChildren(void *thing);

    JS_FRIEND_API(void) markDelayedChildren();
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
# define JS_IS_VALID_TRACE_KIND(kind) ((uint32)(kind) <= JSTRACE_STRING)
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

#ifdef JS_GC_UNDEFD_MOZALLOC_WRAPPERS
#  include "mozilla/mozalloc_macro_wrappers.h"
#endif

#endif /* jsgc_h___ */
