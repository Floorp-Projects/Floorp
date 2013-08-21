/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS Garbage Collector. */

#ifndef jsgc_h
#define jsgc_h

#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"

#include "jsapi.h"
#include "jslock.h"
#include "jsobj.h"

#include "js/GCAPI.h"
#include "js/Vector.h"

class JSAtom;
struct JSCompartment;
class JSFunction;
class JSFlatString;
class JSLinearString;

namespace js {

class ArgumentsObject;
class ArrayBufferObject;
class ArrayBufferViewObject;
class BaseShape;
class DebugScopeObject;
class GCHelperThread;
class GlobalObject;
class LazyScript;
class Nursery;
class PropertyName;
class ScopeObject;
class Shape;
class UnownedBaseShape;
struct SliceBudget;

unsigned GetCPUCount();

enum HeapState {
    Idle,             // doing nothing with the GC heap
    Tracing,          // tracing the GC heap without collecting, e.g. IterateCompartments()
    MajorCollecting,  // doing a GC of the major heap
    MinorCollecting   // doing a GC of the minor heap (nursery)
};

namespace ion {
    class IonCode;
}

namespace gc {

enum State {
    NO_INCREMENTAL,
    MARK_ROOTS,
    MARK,
    SWEEP,
    INVALID
};

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
    inline void put(Chunk *chunk);

    /*
     * Return the list of chunks that can be released outside the GC lock.
     * Must be called either during the GC or with the GC lock taken.
     */
    Chunk *expire(JSRuntime *rt, bool releaseAll);

    /* Must be called with the GC lock taken. */
    void expireAndFree(JSRuntime *rt, bool releaseAll);
};

static inline JSGCTraceKind
MapAllocToTraceKind(AllocKind kind)
{
    static const JSGCTraceKind map[] = {
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
        JSTRACE_SCRIPT,     /* FINALIZE_SCRIPT */
        JSTRACE_LAZY_SCRIPT,/* FINALIZE_LAZY_SCRIPT */
        JSTRACE_SHAPE,      /* FINALIZE_SHAPE */
        JSTRACE_BASE_SHAPE, /* FINALIZE_BASE_SHAPE */
        JSTRACE_TYPE_OBJECT,/* FINALIZE_TYPE_OBJECT */
        JSTRACE_STRING,     /* FINALIZE_SHORT_STRING */
        JSTRACE_STRING,     /* FINALIZE_STRING */
        JSTRACE_STRING,     /* FINALIZE_EXTERNAL_STRING */
        JSTRACE_IONCODE,    /* FINALIZE_IONCODE */
    };
    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(map) == FINALIZE_LIMIT);
    return map[kind];
}

template <typename T> struct MapTypeToTraceKind {};
template <> struct MapTypeToTraceKind<JSObject>         { const static JSGCTraceKind kind = JSTRACE_OBJECT; };
template <> struct MapTypeToTraceKind<JSFunction>       { const static JSGCTraceKind kind = JSTRACE_OBJECT; };
template <> struct MapTypeToTraceKind<ArgumentsObject>  { const static JSGCTraceKind kind = JSTRACE_OBJECT; };
template <> struct MapTypeToTraceKind<ArrayBufferObject>{ const static JSGCTraceKind kind = JSTRACE_OBJECT; };
template <> struct MapTypeToTraceKind<ArrayBufferViewObject>{ const static JSGCTraceKind kind = JSTRACE_OBJECT; };
template <> struct MapTypeToTraceKind<DebugScopeObject> { const static JSGCTraceKind kind = JSTRACE_OBJECT; };
template <> struct MapTypeToTraceKind<GlobalObject>     { const static JSGCTraceKind kind = JSTRACE_OBJECT; };
template <> struct MapTypeToTraceKind<ScopeObject>      { const static JSGCTraceKind kind = JSTRACE_OBJECT; };
template <> struct MapTypeToTraceKind<JSScript>         { const static JSGCTraceKind kind = JSTRACE_SCRIPT; };
template <> struct MapTypeToTraceKind<LazyScript>       { const static JSGCTraceKind kind = JSTRACE_LAZY_SCRIPT; };
template <> struct MapTypeToTraceKind<Shape>            { const static JSGCTraceKind kind = JSTRACE_SHAPE; };
template <> struct MapTypeToTraceKind<BaseShape>        { const static JSGCTraceKind kind = JSTRACE_BASE_SHAPE; };
template <> struct MapTypeToTraceKind<UnownedBaseShape> { const static JSGCTraceKind kind = JSTRACE_BASE_SHAPE; };
template <> struct MapTypeToTraceKind<types::TypeObject>{ const static JSGCTraceKind kind = JSTRACE_TYPE_OBJECT; };
template <> struct MapTypeToTraceKind<JSAtom>           { const static JSGCTraceKind kind = JSTRACE_STRING; };
template <> struct MapTypeToTraceKind<JSString>         { const static JSGCTraceKind kind = JSTRACE_STRING; };
template <> struct MapTypeToTraceKind<JSFlatString>     { const static JSGCTraceKind kind = JSTRACE_STRING; };
template <> struct MapTypeToTraceKind<JSLinearString>   { const static JSGCTraceKind kind = JSTRACE_STRING; };
template <> struct MapTypeToTraceKind<PropertyName>     { const static JSGCTraceKind kind = JSTRACE_STRING; };
template <> struct MapTypeToTraceKind<ion::IonCode>     { const static JSGCTraceKind kind = JSTRACE_IONCODE; };

#if defined(JSGC_GENERATIONAL) || defined(DEBUG)
static inline bool
IsNurseryAllocable(AllocKind kind)
{
    JS_ASSERT(kind >= 0 && unsigned(kind) < FINALIZE_LIMIT);
    static const bool map[] = {
        false,     /* FINALIZE_OBJECT0 */
        true,      /* FINALIZE_OBJECT0_BACKGROUND */
        false,     /* FINALIZE_OBJECT2 */
        true,      /* FINALIZE_OBJECT2_BACKGROUND */
        false,     /* FINALIZE_OBJECT4 */
        true,      /* FINALIZE_OBJECT4_BACKGROUND */
        false,     /* FINALIZE_OBJECT8 */
        true,      /* FINALIZE_OBJECT8_BACKGROUND */
        false,     /* FINALIZE_OBJECT12 */
        true,      /* FINALIZE_OBJECT12_BACKGROUND */
        false,     /* FINALIZE_OBJECT16 */
        true,      /* FINALIZE_OBJECT16_BACKGROUND */
        false,     /* FINALIZE_SCRIPT */
        false,     /* FINALIZE_LAZY_SCRIPT */
        false,     /* FINALIZE_SHAPE */
        false,     /* FINALIZE_BASE_SHAPE */
        false,     /* FINALIZE_TYPE_OBJECT */
        false,     /* FINALIZE_SHORT_STRING */
        false,     /* FINALIZE_STRING */
        false,     /* FINALIZE_EXTERNAL_STRING */
        false,     /* FINALIZE_IONCODE */
    };
    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(map) == FINALIZE_LIMIT);
    return map[kind];
}
#endif

static inline bool
IsBackgroundFinalized(AllocKind kind)
{
    JS_ASSERT(kind >= 0 && unsigned(kind) < FINALIZE_LIMIT);
    static const bool map[] = {
        false,     /* FINALIZE_OBJECT0 */
        true,      /* FINALIZE_OBJECT0_BACKGROUND */
        false,     /* FINALIZE_OBJECT2 */
        true,      /* FINALIZE_OBJECT2_BACKGROUND */
        false,     /* FINALIZE_OBJECT4 */
        true,      /* FINALIZE_OBJECT4_BACKGROUND */
        false,     /* FINALIZE_OBJECT8 */
        true,      /* FINALIZE_OBJECT8_BACKGROUND */
        false,     /* FINALIZE_OBJECT12 */
        true,      /* FINALIZE_OBJECT12_BACKGROUND */
        false,     /* FINALIZE_OBJECT16 */
        true,      /* FINALIZE_OBJECT16_BACKGROUND */
        false,     /* FINALIZE_SCRIPT */
        false,     /* FINALIZE_LAZY_SCRIPT */
        true,      /* FINALIZE_SHAPE */
        true,      /* FINALIZE_BASE_SHAPE */
        true,      /* FINALIZE_TYPE_OBJECT */
        true,      /* FINALIZE_SHORT_STRING */
        true,      /* FINALIZE_STRING */
        false,     /* FINALIZE_EXTERNAL_STRING */
        false,     /* FINALIZE_IONCODE */
    };
    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(map) == FINALIZE_LIMIT);
    return map[kind];
}

static inline bool
CanBeFinalizedInBackground(gc::AllocKind kind, Class *clasp)
{
    JS_ASSERT(kind <= gc::FINALIZE_OBJECT_LAST);
    /* If the class has no finalizer or a finalizer that is safe to call on
     * a different thread, we change the finalize kind. For example,
     * FINALIZE_OBJECT0 calls the finalizer on the main thread,
     * FINALIZE_OBJECT0_BACKGROUND calls the finalizer on the gcHelperThread.
     * IsBackgroundFinalized is called to prevent recursively incrementing
     * the finalize kind; kind may already be a background finalize kind.
     */
    return (!gc::IsBackgroundFinalized(kind) &&
            (!clasp->finalize || (clasp->flags & JSCLASS_BACKGROUND_FINALIZE)));
}

inline JSGCTraceKind
GetGCThingTraceKind(const void *thing);

/* Capacity for slotsToThingKind */
const size_t SLOTS_TO_THING_KIND_LIMIT = 17;

extern const AllocKind slotsToThingKind[];

/* Get the best kind to use when making an object with the given slot count. */
static inline AllocKind
GetGCObjectKind(size_t numSlots)
{
    if (numSlots >= SLOTS_TO_THING_KIND_LIMIT)
        return FINALIZE_OBJECT16;
    return slotsToThingKind[numSlots];
}

/* As for GetGCObjectKind, but for dense array allocation. */
static inline AllocKind
GetGCArrayKind(size_t numSlots)
{
    /*
     * Dense arrays can use their fixed slots to hold their elements array
     * (less two Values worth of ObjectElements header), but if more than the
     * maximum number of fixed slots is needed then the fixed slots will be
     * unused.
     */
    JS_STATIC_ASSERT(ObjectElements::VALUES_PER_HEADER == 2);
    if (numSlots > JSObject::NELEMENTS_LIMIT || numSlots + 2 >= SLOTS_TO_THING_KIND_LIMIT)
        return FINALIZE_OBJECT2;
    return slotsToThingKind[numSlots + 2];
}

static inline AllocKind
GetGCObjectFixedSlotsKind(size_t numFixedSlots)
{
    JS_ASSERT(numFixedSlots < SLOTS_TO_THING_KIND_LIMIT);
    return slotsToThingKind[numFixedSlots];
}

static inline AllocKind
GetBackgroundAllocKind(AllocKind kind)
{
    JS_ASSERT(!IsBackgroundFinalized(kind));
    JS_ASSERT(kind <= FINALIZE_OBJECT_LAST);
    return (AllocKind) (kind + 1);
}

/*
 * Try to get the next larger size for an object, keeping BACKGROUND
 * consistent.
 */
static inline bool
TryIncrementAllocKind(AllocKind *kindp)
{
    size_t next = size_t(*kindp) + 2;
    if (next >= size_t(FINALIZE_OBJECT_LIMIT))
        return false;
    *kindp = AllocKind(next);
    return true;
}

/* Get the number of fixed slots and initial capacity associated with a kind. */
static inline size_t
GetGCKindSlots(AllocKind thingKind)
{
    /* Using a switch in hopes that thingKind will usually be a compile-time constant. */
    switch (thingKind) {
      case FINALIZE_OBJECT0:
      case FINALIZE_OBJECT0_BACKGROUND:
        return 0;
      case FINALIZE_OBJECT2:
      case FINALIZE_OBJECT2_BACKGROUND:
        return 2;
      case FINALIZE_OBJECT4:
      case FINALIZE_OBJECT4_BACKGROUND:
        return 4;
      case FINALIZE_OBJECT8:
      case FINALIZE_OBJECT8_BACKGROUND:
        return 8;
      case FINALIZE_OBJECT12:
      case FINALIZE_OBJECT12_BACKGROUND:
        return 12;
      case FINALIZE_OBJECT16:
      case FINALIZE_OBJECT16_BACKGROUND:
        return 16;
      default:
        MOZ_ASSUME_UNREACHABLE("Bad object finalize kind");
    }
}

static inline size_t
GetGCKindSlots(AllocKind thingKind, Class *clasp)
{
    size_t nslots = GetGCKindSlots(thingKind);

    /* An object's private data uses the space taken by its last fixed slot. */
    if (clasp->flags & JSCLASS_HAS_PRIVATE) {
        JS_ASSERT(nslots > 0);
        nslots--;
    }

    /*
     * Functions have a larger finalize kind than FINALIZE_OBJECT to reserve
     * space for the extra fields in JSFunction, but have no fixed slots.
     */
    if (clasp == FunctionClassPtr)
        nslots = 0;

    return nslots;
}

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

    void insert(ArenaHeader *arena);
};

class ArenaLists
{
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

  public:
    /* For each arena kind, a list of arenas remaining to be swept. */
    ArenaHeader *arenaListsToSweep[FINALIZE_LIMIT];

    /* Shape arenas to be swept in the foreground. */
    ArenaHeader *gcShapeArenasToSweep;

  public:
    ArenaLists() {
        for (size_t i = 0; i != FINALIZE_LIMIT; ++i)
            freeLists[i].initAsEmpty();
        for (size_t i = 0; i != FINALIZE_LIMIT; ++i)
            backgroundFinalizeState[i] = BFS_DONE;
        for (size_t i = 0; i != FINALIZE_LIMIT; ++i)
            arenaListsToSweep[i] = NULL;
        gcShapeArenasToSweep = NULL;
    }

    ~ArenaLists() {
        for (size_t i = 0; i != FINALIZE_LIMIT; ++i) {
            /*
             * We can only call this during the shutdown after the last GC when
             * the background finalization is disabled.
             */
            JS_ASSERT(backgroundFinalizeState[i] == BFS_DONE);
            ArenaHeader **headp = &arenaLists[i].head;
            while (ArenaHeader *aheader = *headp) {
                *headp = aheader->next;
                aheader->chunk()->releaseArena(aheader);
            }
        }
    }

    static uintptr_t getFreeListOffset(AllocKind thingKind) {
        uintptr_t offset = offsetof(ArenaLists, freeLists);
        return offset + thingKind * sizeof(FreeSpan);
    }

    const FreeSpan *getFreeList(AllocKind thingKind) const {
        return &freeLists[thingKind];
    }

    ArenaHeader *getFirstArena(AllocKind thingKind) const {
        return arenaLists[thingKind].head;
    }

    ArenaHeader *getFirstArenaToSweep(AllocKind thingKind) const {
        return arenaListsToSweep[thingKind];
    }

    bool arenaListsAreEmpty() const {
        for (size_t i = 0; i != FINALIZE_LIMIT; ++i) {
            /*
             * The arena cannot be empty if the background finalization is not yet
             * done.
             */
            if (backgroundFinalizeState[i] != BFS_DONE)
                return false;
            if (arenaLists[i].head)
                return false;
        }
        return true;
    }

    bool arenasAreFull(AllocKind thingKind) const {
        return !*arenaLists[thingKind].cursor;
    }

    void unmarkAll() {
        for (size_t i = 0; i != FINALIZE_LIMIT; ++i) {
            /* The background finalization must have stopped at this point. */
            JS_ASSERT(backgroundFinalizeState[i] == BFS_DONE ||
                      backgroundFinalizeState[i] == BFS_JUST_FINISHED);
            for (ArenaHeader *aheader = arenaLists[i].head; aheader; aheader = aheader->next) {
                uintptr_t *word = aheader->chunk()->bitmap.arenaBits(aheader);
                memset(word, 0, ArenaBitmapWords * sizeof(uintptr_t));
            }
        }
    }

    bool doneBackgroundFinalize(AllocKind kind) const {
        return backgroundFinalizeState[kind] == BFS_DONE ||
               backgroundFinalizeState[kind] == BFS_JUST_FINISHED;
    }

    bool needBackgroundFinalizeWait(AllocKind kind) const {
        return backgroundFinalizeState[kind] != BFS_DONE;
    }

    /*
     * Return the free list back to the arena so the GC finalization will not
     * run the finalizers over unitialized bytes from free things.
     */
    void purge() {
        for (size_t i = 0; i != FINALIZE_LIMIT; ++i) {
            FreeSpan *headSpan = &freeLists[i];
            if (!headSpan->isEmpty()) {
                ArenaHeader *aheader = headSpan->arenaHeader();
                aheader->setFirstFreeSpan(headSpan);
                headSpan->initAsEmpty();
            }
        }
    }

    inline void prepareForIncrementalGC(JSRuntime *rt);

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

    template <AllowGC allowGC>
    static void *refillFreeList(ThreadSafeContext *cx, AllocKind thingKind);

    /*
     * Moves all arenas from |fromArenaLists| into |this|.  In
     * parallel blocks, we temporarily create one ArenaLists per
     * parallel thread.  When the parallel block ends, we move
     * whatever allocations may have been performed back into the
     * compartment's main arena list using this function.
     */
    void adoptArenas(JSRuntime *runtime, ArenaLists *fromArenaLists);

    /* True if the ArenaHeader in question is found in this ArenaLists */
    bool containsArena(JSRuntime *runtime, ArenaHeader *arenaHeader);

    void checkEmptyFreeLists() {
#ifdef DEBUG
        for (size_t i = 0; i < mozilla::ArrayLength(freeLists); ++i)
            JS_ASSERT(freeLists[i].isEmpty());
#endif
    }

    void checkEmptyFreeList(AllocKind kind) {
        JS_ASSERT(freeLists[kind].isEmpty());
    }

    void queueObjectsForSweep(FreeOp *fop);
    void queueStringsForSweep(FreeOp *fop);
    void queueShapesForSweep(FreeOp *fop);
    void queueScriptsForSweep(FreeOp *fop);
    void queueIonCodeForSweep(FreeOp *fop);

    bool foregroundFinalize(FreeOp *fop, AllocKind thingKind, SliceBudget &sliceBudget);
    static void backgroundFinalize(FreeOp *fop, ArenaHeader *listHead, bool onBackgroundThread);

  private:
    inline void finalizeNow(FreeOp *fop, AllocKind thingKind);
    inline void queueForForegroundSweep(FreeOp *fop, AllocKind thingKind);
    inline void queueForBackgroundSweep(FreeOp *fop, AllocKind thingKind);

    void *allocateFromArena(JS::Zone *zone, AllocKind thingKind);
    inline void *allocateFromArenaInline(JS::Zone *zone, AllocKind thingKind);

    friend class js::Nursery;
};

/*
 * Initial allocation size for data structures holding chunks is set to hold
 * chunks with total capacity of 16MB to avoid buffer resizes during browser
 * startup.
 */
const size_t INITIAL_CHUNK_CAPACITY = 16 * 1024 * 1024 / ChunkSize;

/* The number of GC cycles an empty chunk can survive before been released. */
const size_t MAX_EMPTY_CHUNK_AGE = 4;

} /* namespace gc */

typedef enum JSGCRootType {
    JS_GC_ROOT_VALUE_PTR,
    JS_GC_ROOT_STRING_PTR,
    JS_GC_ROOT_OBJECT_PTR,
    JS_GC_ROOT_SCRIPT_PTR
} JSGCRootType;

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

extern bool
AddValueRoot(JSContext *cx, js::Value *vp, const char *name);

extern bool
AddValueRootRT(JSRuntime *rt, js::Value *vp, const char *name);

extern bool
AddStringRoot(JSContext *cx, JSString **rp, const char *name);

extern bool
AddObjectRoot(JSContext *cx, JSObject **rp, const char *name);

extern bool
AddObjectRoot(JSRuntime *rt, JSObject **rp, const char *name);

extern bool
AddScriptRoot(JSContext *cx, JSScript **rp, const char *name);

} /* namespace js */

extern bool
js_InitGC(JSRuntime *rt, uint32_t maxbytes);

extern void
js_FinishGC(JSRuntime *rt);

namespace js {

class StackFrame;

extern void
MarkCompartmentActive(js::StackFrame *fp);

extern void
TraceRuntime(JSTracer *trc);

/* Must be called with GC lock taken. */
extern void
TriggerGC(JSRuntime *rt, JS::gcreason::Reason reason);

/* Must be called with GC lock taken. */
extern void
TriggerZoneGC(Zone *zone, JS::gcreason::Reason reason);

extern void
MaybeGC(JSContext *cx);

extern void
ReleaseAllJITCode(FreeOp *op);

/*
 * Kinds of js_GC invocation.
 */
typedef enum JSGCInvocationKind {
    /* Normal invocation. */
    GC_NORMAL           = 0,

    /* Minimize GC triggers and release empty GC chunks right away. */
    GC_SHRINK             = 1
} JSGCInvocationKind;

extern void
GC(JSRuntime *rt, JSGCInvocationKind gckind, JS::gcreason::Reason reason);

extern void
GCSlice(JSRuntime *rt, JSGCInvocationKind gckind, JS::gcreason::Reason reason, int64_t millis = 0);

extern void
GCFinalSlice(JSRuntime *rt, JSGCInvocationKind gckind, JS::gcreason::Reason reason);

extern void
GCDebugSlice(JSRuntime *rt, bool limit, int64_t objCount);

extern void
PrepareForDebugGC(JSRuntime *rt);

extern void
MinorGC(JSRuntime *rt, JS::gcreason::Reason reason);

#ifdef JS_GC_ZEAL
extern void
SetGCZeal(JSRuntime *rt, uint8_t zeal, uint32_t frequency);
#endif

/* Functions for managing cross compartment gray pointers. */

extern void
DelayCrossCompartmentGrayMarking(JSObject *src);

extern void
NotifyGCNukeWrapper(JSObject *o);

extern unsigned
NotifyGCPreSwap(JSObject *a, JSObject *b);

extern void
NotifyGCPostSwap(JSObject *a, JSObject *b, unsigned preResult);

void
InitTracer(JSTracer *trc, JSRuntime *rt, JSTraceCallback callback);

/*
 * Helper that implements sweeping and allocation for kinds that can be swept
 * and allocated off the main thread.
 *
 * In non-threadsafe builds, all actual sweeping and allocation is performed
 * on the main thread, but GCHelperThread encapsulates this from clients as
 * much as possible.
 */
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

    bool              sweepFlag;
    bool              shrinkFlag;

    Vector<void **, 16, js::SystemAllocPolicy> freeVector;
    void            **freeCursor;
    void            **freeCursorEnd;

    bool              backgroundAllocation;

    friend class js::gc::ArenaLists;

    void
    replenishAndFreeLater(void *ptr);

    static void freeElementsAndArray(void **array, void **end) {
        JS_ASSERT(array <= end);
        for (void **p = array; p != end; ++p)
            js_free(*p);
        js_free(array);
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
        sweepFlag(false),
        shrinkFlag(false),
        freeCursor(NULL),
        freeCursorEnd(NULL),
        backgroundAllocation(true)
    { }

    bool init();
    void finish();

    /* Must be called with the GC lock taken. */
    void startBackgroundSweep(bool shouldShrink);

    /* Must be called with the GC lock taken. */
    void startBackgroundShrink();

    /* Must be called without the GC lock taken. */
    void waitBackgroundSweepEnd();

    /* Must be called without the GC lock taken. */
    void waitBackgroundSweepOrAllocEnd();

    /* Must be called with the GC lock taken. */
    inline void startBackgroundAllocationIfIdle();

    bool canBackgroundAllocate() const {
        return backgroundAllocation;
    }

    void disableBackgroundAllocation() {
        backgroundAllocation = false;
    }

    PRThread *getThread() const {
        return thread;
    }

    bool onBackgroundThread();

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
};


struct GCChunkHasher {
    typedef gc::Chunk *Lookup;

    /*
     * Strip zeros for better distribution after multiplying by the golden
     * ratio.
     */
    static HashNumber hash(gc::Chunk *chunk) {
        JS_ASSERT(!(uintptr_t(chunk) & gc::ChunkMask));
        return HashNumber(uintptr_t(chunk) >> gc::ChunkShift);
    }

    static bool match(gc::Chunk *k, gc::Chunk *l) {
        JS_ASSERT(!(uintptr_t(k) & gc::ChunkMask));
        JS_ASSERT(!(uintptr_t(l) & gc::ChunkMask));
        return k == l;
    }
};

typedef HashSet<js::gc::Chunk *, GCChunkHasher, SystemAllocPolicy> GCChunkSet;

template<class T>
struct MarkStack {
    T *stack;
    T *tos;
    T *limit;

    T *ballast;
    T *ballastLimit;

    size_t sizeLimit;

    MarkStack(size_t sizeLimit)
      : stack(NULL),
        tos(NULL),
        limit(NULL),
        ballast(NULL),
        ballastLimit(NULL),
        sizeLimit(sizeLimit) { }

    ~MarkStack() {
        if (stack != ballast)
            js_free(stack);
        js_free(ballast);
    }

    bool init(size_t ballastcap) {
        JS_ASSERT(!stack);

        if (ballastcap == 0)
            return true;

        ballast = js_pod_malloc<T>(ballastcap);
        if (!ballast)
            return false;
        ballastLimit = ballast + ballastcap;
        initFromBallast();
        return true;
    }

    void initFromBallast() {
        stack = ballast;
        limit = ballastLimit;
        if (size_t(limit - stack) > sizeLimit)
            limit = stack + sizeLimit;
        tos = stack;
    }

    void setSizeLimit(size_t size) {
        JS_ASSERT(isEmpty());

        sizeLimit = size;
        reset();
    }

    bool push(T item) {
        if (tos == limit) {
            if (!enlarge())
                return false;
        }
        JS_ASSERT(tos < limit);
        *tos++ = item;
        return true;
    }

    bool push(T item1, T item2, T item3) {
        T *nextTos = tos + 3;
        if (nextTos > limit) {
            if (!enlarge())
                return false;
            nextTos = tos + 3;
        }
        JS_ASSERT(nextTos <= limit);
        tos[0] = item1;
        tos[1] = item2;
        tos[2] = item3;
        tos = nextTos;
        return true;
    }

    bool isEmpty() const {
        return tos == stack;
    }

    T pop() {
        JS_ASSERT(!isEmpty());
        return *--tos;
    }

    ptrdiff_t position() const {
        return tos - stack;
    }

    void reset() {
        if (stack != ballast)
            js_free(stack);
        initFromBallast();
        JS_ASSERT(stack == ballast);
    }

    bool enlarge() {
        size_t tosIndex = tos - stack;
        size_t cap = limit - stack;
        if (cap == sizeLimit)
            return false;
        size_t newcap = cap * 2;
        if (newcap == 0)
            newcap = 32;
        if (newcap > sizeLimit)
            newcap = sizeLimit;

        T *newStack;
        if (stack == ballast) {
            newStack = js_pod_malloc<T>(newcap);
            if (!newStack)
                return false;
            for (T *src = stack, *dst = newStack; src < tos; )
                *dst++ = *src++;
        } else {
            newStack = (T *)js_realloc(stack, sizeof(T) * newcap);
            if (!newStack)
                return false;
        }
        stack = newStack;
        tos = stack + tosIndex;
        limit = newStack + newcap;
        return true;
    }

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
        size_t n = 0;
        if (stack != ballast)
            n += mallocSizeOf(stack);
        n += mallocSizeOf(ballast);
        return n;
    }
};

/*
 * This class records how much work has been done in a given GC slice, so that
 * we can return before pausing for too long. Some slices are allowed to run for
 * unlimited time, and others are bounded. To reduce the number of gettimeofday
 * calls, we only check the time every 1000 operations.
 */
struct SliceBudget {
    int64_t deadline; /* in microseconds */
    intptr_t counter;

    static const intptr_t CounterReset = 1000;

    static const int64_t Unlimited = 0;
    static int64_t TimeBudget(int64_t millis);
    static int64_t WorkBudget(int64_t work);

    /* Equivalent to SliceBudget(UnlimitedBudget). */
    SliceBudget();

    /* Instantiate as SliceBudget(Time/WorkBudget(n)). */
    SliceBudget(int64_t budget);

    void reset() {
        deadline = INT64_MAX;
        counter = INTPTR_MAX;
    }

    void step(intptr_t amt = 1) {
        counter -= amt;
    }

    bool checkOverBudget();

    bool isOverBudget() {
        if (counter >= 0)
            return false;
        return checkOverBudget();
    }
};

static const size_t MARK_STACK_LENGTH = 32768;

struct GrayRoot {
    void *thing;
    JSGCTraceKind kind;
#ifdef DEBUG
    JSTraceNamePrinter debugPrinter;
    const void *debugPrintArg;
    size_t debugPrintIndex;
#endif

    GrayRoot(void *thing, JSGCTraceKind kind)
        : thing(thing), kind(kind) {}
};

struct GCMarker : public JSTracer {
  private:
    /*
     * We use a common mark stack to mark GC things of different types and use
     * the explicit tags to distinguish them when it cannot be deduced from
     * the context of push or pop operation.
     */
    enum StackTag {
        ValueArrayTag,
        ObjectTag,
        TypeTag,
        XmlTag,
        ArenaTag,
        SavedValueArrayTag,
        IonCodeTag,
        LastTag = IonCodeTag
    };

    static const uintptr_t StackTagMask = 7;

    static void staticAsserts() {
        JS_STATIC_ASSERT(StackTagMask >= uintptr_t(LastTag));
        JS_STATIC_ASSERT(StackTagMask <= gc::CellMask);
    }

  public:
    explicit GCMarker(JSRuntime *rt);
    bool init();

    void setSizeLimit(size_t size) { stack.setSizeLimit(size); }
    size_t sizeLimit() const { return stack.sizeLimit; }

    void start();
    void stop();
    void reset();

    void pushObject(JSObject *obj) {
        pushTaggedPtr(ObjectTag, obj);
    }

    void pushArenaList(gc::ArenaHeader *firstArena) {
        pushTaggedPtr(ArenaTag, firstArena);
    }

    void pushType(types::TypeObject *type) {
        pushTaggedPtr(TypeTag, type);
    }

    void pushIonCode(ion::IonCode *code) {
        pushTaggedPtr(IonCodeTag, code);
    }

    uint32_t getMarkColor() const {
        return color;
    }

    /*
     * Care must be taken changing the mark color from gray to black. The cycle
     * collector depends on the invariant that there are no black to gray edges
     * in the GC heap. This invariant lets the CC not trace through black
     * objects. If this invariant is violated, the cycle collector may free
     * objects that are still reachable.
     */
    void setMarkColorGray() {
        JS_ASSERT(isDrained());
        JS_ASSERT(color == gc::BLACK);
        color = gc::GRAY;
    }

    void setMarkColorBlack() {
        JS_ASSERT(isDrained());
        JS_ASSERT(color == gc::GRAY);
        color = gc::BLACK;
    }

    inline void delayMarkingArena(gc::ArenaHeader *aheader);
    void delayMarkingChildren(const void *thing);
    void markDelayedChildren(gc::ArenaHeader *aheader);
    bool markDelayedChildren(SliceBudget &budget);
    bool hasDelayedChildren() const {
        return !!unmarkedArenaStackTop;
    }

    bool isDrained() {
        return isMarkStackEmpty() && !unmarkedArenaStackTop;
    }

    bool drainMarkStack(SliceBudget &budget);

    /*
     * Gray marking must be done after all black marking is complete. However,
     * we do not have write barriers on XPConnect roots. Therefore, XPConnect
     * roots must be accumulated in the first slice of incremental GC. We
     * accumulate these roots in the each compartment's gcGrayRoots vector and
     * then mark them later, after black marking is complete for each
     * compartment. This accumulation can fail, but in that case we switch to
     * non-incremental GC.
     */
    bool hasBufferedGrayRoots() const;
    void startBufferingGrayRoots();
    void endBufferingGrayRoots();
    void resetBufferedGrayRoots();
    void markBufferedGrayRoots(JS::Zone *zone);

    static void GrayCallback(JSTracer *trc, void **thing, JSGCTraceKind kind);

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    MarkStack<uintptr_t> stack;

  private:
#ifdef DEBUG
    void checkZone(void *p);
#else
    void checkZone(void *p) {}
#endif

    void pushTaggedPtr(StackTag tag, void *ptr) {
        checkZone(ptr);
        uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
        JS_ASSERT(!(addr & StackTagMask));
        if (!stack.push(addr | uintptr_t(tag)))
            delayMarkingChildren(ptr);
    }

    void pushValueArray(JSObject *obj, void *start, void *end) {
        checkZone(obj);

        JS_ASSERT(start <= end);
        uintptr_t tagged = reinterpret_cast<uintptr_t>(obj) | GCMarker::ValueArrayTag;
        uintptr_t startAddr = reinterpret_cast<uintptr_t>(start);
        uintptr_t endAddr = reinterpret_cast<uintptr_t>(end);

        /*
         * Push in the reverse order so obj will be on top. If we cannot push
         * the array, we trigger delay marking for the whole object.
         */
        if (!stack.push(endAddr, startAddr, tagged))
            delayMarkingChildren(obj);
    }

    bool isMarkStackEmpty() {
        return stack.isEmpty();
    }

    bool restoreValueArray(JSObject *obj, void **vpp, void **endp);
    void saveValueRanges();
    inline void processMarkStackTop(SliceBudget &budget);
    void processMarkStackOther(SliceBudget &budget, uintptr_t tag, uintptr_t addr);

    void appendGrayRoot(void *thing, JSGCTraceKind kind);

    /* The color is only applied to objects and functions. */
    uint32_t color;

    mozilla::DebugOnly<bool> started;

    /* Pointer to the top of the stack of arenas we are delaying marking on. */
    js::gc::ArenaHeader *unmarkedArenaStackTop;
    /* Count of arenas that are currently in the stack. */
    mozilla::DebugOnly<size_t> markLaterArenas;

    bool grayFailed;
};

void
SetMarkStackLimit(JSRuntime *rt, size_t limit);

void
MarkStackRangeConservatively(JSTracer *trc, Value *begin, Value *end);

typedef void (*IterateChunkCallback)(JSRuntime *rt, void *data, gc::Chunk *chunk);
typedef void (*IterateZoneCallback)(JSRuntime *rt, void *data, JS::Zone *zone);
typedef void (*IterateArenaCallback)(JSRuntime *rt, void *data, gc::Arena *arena,
                                     JSGCTraceKind traceKind, size_t thingSize);
typedef void (*IterateCellCallback)(JSRuntime *rt, void *data, void *thing,
                                    JSGCTraceKind traceKind, size_t thingSize);

/*
 * This function calls |compartmentCallback| on every compartment,
 * |arenaCallback| on every in-use arena, and |cellCallback| on every in-use
 * cell in the GC heap.
 */
extern void
IterateZonesCompartmentsArenasCells(JSRuntime *rt, void *data,
                                    IterateZoneCallback zoneCallback,
                                    JSIterateCompartmentCallback compartmentCallback,
                                    IterateArenaCallback arenaCallback,
                                    IterateCellCallback cellCallback);

/*
 * Invoke chunkCallback on every in-use chunk.
 */
extern void
IterateChunks(JSRuntime *rt, void *data, IterateChunkCallback chunkCallback);

typedef void (*IterateScriptCallback)(JSRuntime *rt, void *data, JSScript *script);

/*
 * Invoke scriptCallback on every in-use script for
 * the given compartment or for all compartments if it is null.
 */
extern void
IterateScripts(JSRuntime *rt, JSCompartment *compartment,
               void *data, IterateScriptCallback scriptCallback);

} /* namespace js */

extern void
js_FinalizeStringRT(JSRuntime *rt, JSString *str);

/*
 * Macro to test if a traversal is the marking phase of the GC.
 */
#define IS_GC_MARKING_TRACER(trc) \
    ((trc)->callback == NULL || (trc)->callback == GCMarker::GrayCallback)

namespace js {

JSCompartment *
NewCompartment(JSContext *cx, JS::Zone *zone, JSPrincipals *principals,
               const JS::CompartmentOptions &options);

namespace gc {

/* Tries to run a GC no matter what (used for GC zeal). */
void
RunDebugGC(JSContext *cx);

void
SetDeterministicGC(JSContext *cx, bool enabled);

void
SetValidateGC(JSContext *cx, bool enabled);

void
SetFullCompartmentChecks(JSContext *cx, bool enabled);

/* Wait for the background thread to finish sweeping if it is running. */
void
FinishBackgroundFinalize(JSRuntime *rt);

/*
 * Merge all contents of source into target. This can only be used if source is
 * the only compartment in its zone.
 */
void
MergeCompartments(JSCompartment *source, JSCompartment *target);

const int ZealPokeValue = 1;
const int ZealAllocValue = 2;
const int ZealFrameGCValue = 3;
const int ZealVerifierPreValue = 4;
const int ZealFrameVerifierPreValue = 5;
// These two values used to be distinct.  They no longer are, but both were
// kept to avoid breaking fuzz tests.  Avoid using ZealStackRootingValue__2.
const int ZealStackRootingValue = 6;
const int ZealStackRootingValue__2 = 7;
const int ZealIncrementalRootsThenFinish = 8;
const int ZealIncrementalMarkAllThenFinish = 9;
const int ZealIncrementalMultipleSlices = 10;
const int ZealVerifierPostValue = 11;
const int ZealFrameVerifierPostValue = 12;
const int ZealLimit = 12;

enum VerifierType {
    PreBarrierVerifier,
    PostBarrierVerifier
};

#ifdef JS_GC_ZEAL

/* Check that write barriers have been used correctly. See jsgc.cpp. */
void
VerifyBarriers(JSRuntime *rt, VerifierType type);

void
MaybeVerifyBarriers(JSContext *cx, bool always = false);

#else

static inline void
VerifyBarriers(JSRuntime *rt, VerifierType type)
{
}

static inline void
MaybeVerifyBarriers(JSContext *cx, bool always = false)
{
}

#endif

/*
 * Instances of this class set the |JSRuntime::suppressGC| flag for the duration
 * that they are live. Use of this class is highly discouraged. Please carefully
 * read the comment in jscntxt.h above |suppressGC| and take all appropriate
 * precautions before instantiating this class.
 */
class AutoSuppressGC
{
    int32_t &suppressGC_;

  public:
    AutoSuppressGC(JSRuntime *rt);
    AutoSuppressGC(JSContext *cx);
    AutoSuppressGC(JSCompartment *comp);

    ~AutoSuppressGC()
    {
        suppressGC_--;
    }
};

} /* namespace gc */

#ifdef DEBUG
/* Use this to avoid assertions when manipulating the wrapper map. */
class AutoDisableProxyCheck
{
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER;
    uintptr_t &count;

  public:
    AutoDisableProxyCheck(JSRuntime *rt
                          MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

    ~AutoDisableProxyCheck() {
        count--;
    }
};
#else
struct AutoDisableProxyCheck
{
    AutoDisableProxyCheck(JSRuntime *rt) {}
};
#endif

void
PurgeJITCaches(JS::Zone *zone);

// This is the same as IsInsideNursery, but not inlined.
bool
UninlinedIsInsideNursery(JSRuntime *rt, const void *thing);

} /* namespace js */

#endif /* jsgc_h */
