/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS Mark-and-Sweep Garbage Collector. */

#include "jsgcinlines.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "mozilla/Util.h"

using mozilla::Swap;

/*
 * This code implements a mark-and-sweep garbage collector. The mark phase is
 * incremental. Most sweeping is done on a background thread. A GC is divided
 * into slices as follows:
 *
 * Slice 1: Roots pushed onto the mark stack. The mark stack is processed by
 * popping an element, marking it, and pushing its children.
 *   ... JS code runs ...
 * Slice 2: More mark stack processing.
 *   ... JS code runs ...
 * Slice n-1: More mark stack processing.
 *   ... JS code runs ...
 * Slice n: Mark stack is completely drained. Some sweeping is done.
 *   ... JS code runs, remaining sweeping done on background thread ...
 *
 * When background sweeping finishes the GC is complete.
 *
 * Incremental GC requires close collaboration with the mutator (i.e., JS code):
 *
 * 1. During an incremental GC, if a memory location (except a root) is written
 * to, then the value it previously held must be marked. Write barriers ensure
 * this.
 * 2. Any object that is allocated during incremental GC must start out marked.
 * 3. Roots are special memory locations that don't need write
 * barriers. However, they must be marked in the first slice. Roots are things
 * like the C stack and the VM stack, since it would be too expensive to put
 * barriers on them.
 */

#include <string.h>     /* for memset used when DEBUG */
#ifndef XP_WIN
# include <unistd.h>
#endif

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "jsobj.h"
#include "jsproxy.h"
#include "jsscript.h"
#include "jstypes.h"
#include "jsutil.h"
#include "jswatchpoint.h"
#include "jsweakmap.h"
#ifdef XP_WIN
# include "jswin.h"
#endif
#include "prmjtime.h"
#if JS_TRACE_LOGGING
#include "TraceLogging.h"
#endif

#include "gc/FindSCCs.h"
#include "gc/GCInternals.h"
#include "gc/Marking.h"
#include "gc/Memory.h"
#ifdef JS_ION
# include "ion/BaselineJIT.h"
#endif
#include "ion/IonCode.h"
#include "vm/Debugger.h"
#include "vm/ForkJoin.h"
#include "vm/ProxyObject.h"
#include "vm/Shape.h"
#include "vm/String.h"
#include "vm/WrapperObject.h"

#include "jsobjinlines.h"

#include "vm/Runtime-inl.h"
#include "vm/Stack-inl.h"
#include "vm/String-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::ArrayEnd;
using mozilla::DebugOnly;
using mozilla::Maybe;

/* Perform a Full GC every 20 seconds if MaybeGC is called */
static const uint64_t GC_IDLE_FULL_SPAN = 20 * 1000 * 1000;

/* Increase the IGC marking slice time if we are in highFrequencyGC mode. */
static const int IGC_MARK_SLICE_MULTIPLIER = 2;

#if defined(ANDROID) || defined(MOZ_B2G)
static const int MAX_EMPTY_CHUNK_COUNT = 2;
#else
static const int MAX_EMPTY_CHUNK_COUNT = 30;
#endif

/* This array should be const, but that doesn't link right under GCC. */
const AllocKind gc::slotsToThingKind[] = {
    /* 0 */  FINALIZE_OBJECT0,  FINALIZE_OBJECT2,  FINALIZE_OBJECT2,  FINALIZE_OBJECT4,
    /* 4 */  FINALIZE_OBJECT4,  FINALIZE_OBJECT8,  FINALIZE_OBJECT8,  FINALIZE_OBJECT8,
    /* 8 */  FINALIZE_OBJECT8,  FINALIZE_OBJECT12, FINALIZE_OBJECT12, FINALIZE_OBJECT12,
    /* 12 */ FINALIZE_OBJECT12, FINALIZE_OBJECT16, FINALIZE_OBJECT16, FINALIZE_OBJECT16,
    /* 16 */ FINALIZE_OBJECT16
};

JS_STATIC_ASSERT(JS_ARRAY_LENGTH(slotsToThingKind) == SLOTS_TO_THING_KIND_LIMIT);

const uint32_t Arena::ThingSizes[] = {
    sizeof(JSObject),           /* FINALIZE_OBJECT0             */
    sizeof(JSObject),           /* FINALIZE_OBJECT0_BACKGROUND  */
    sizeof(JSObject_Slots2),    /* FINALIZE_OBJECT2             */
    sizeof(JSObject_Slots2),    /* FINALIZE_OBJECT2_BACKGROUND  */
    sizeof(JSObject_Slots4),    /* FINALIZE_OBJECT4             */
    sizeof(JSObject_Slots4),    /* FINALIZE_OBJECT4_BACKGROUND  */
    sizeof(JSObject_Slots8),    /* FINALIZE_OBJECT8             */
    sizeof(JSObject_Slots8),    /* FINALIZE_OBJECT8_BACKGROUND  */
    sizeof(JSObject_Slots12),   /* FINALIZE_OBJECT12            */
    sizeof(JSObject_Slots12),   /* FINALIZE_OBJECT12_BACKGROUND */
    sizeof(JSObject_Slots16),   /* FINALIZE_OBJECT16            */
    sizeof(JSObject_Slots16),   /* FINALIZE_OBJECT16_BACKGROUND */
    sizeof(JSScript),           /* FINALIZE_SCRIPT              */
    sizeof(LazyScript),         /* FINALIZE_LAZY_SCRIPT         */
    sizeof(Shape),              /* FINALIZE_SHAPE               */
    sizeof(BaseShape),          /* FINALIZE_BASE_SHAPE          */
    sizeof(types::TypeObject),  /* FINALIZE_TYPE_OBJECT         */
    sizeof(JSShortString),      /* FINALIZE_SHORT_STRING        */
    sizeof(JSString),           /* FINALIZE_STRING              */
    sizeof(JSExternalString),   /* FINALIZE_EXTERNAL_STRING     */
    sizeof(ion::IonCode),       /* FINALIZE_IONCODE             */
};

#define OFFSET(type) uint32_t(sizeof(ArenaHeader) + (ArenaSize - sizeof(ArenaHeader)) % sizeof(type))

const uint32_t Arena::FirstThingOffsets[] = {
    OFFSET(JSObject),           /* FINALIZE_OBJECT0             */
    OFFSET(JSObject),           /* FINALIZE_OBJECT0_BACKGROUND  */
    OFFSET(JSObject_Slots2),    /* FINALIZE_OBJECT2             */
    OFFSET(JSObject_Slots2),    /* FINALIZE_OBJECT2_BACKGROUND  */
    OFFSET(JSObject_Slots4),    /* FINALIZE_OBJECT4             */
    OFFSET(JSObject_Slots4),    /* FINALIZE_OBJECT4_BACKGROUND  */
    OFFSET(JSObject_Slots8),    /* FINALIZE_OBJECT8             */
    OFFSET(JSObject_Slots8),    /* FINALIZE_OBJECT8_BACKGROUND  */
    OFFSET(JSObject_Slots12),   /* FINALIZE_OBJECT12            */
    OFFSET(JSObject_Slots12),   /* FINALIZE_OBJECT12_BACKGROUND */
    OFFSET(JSObject_Slots16),   /* FINALIZE_OBJECT16            */
    OFFSET(JSObject_Slots16),   /* FINALIZE_OBJECT16_BACKGROUND */
    OFFSET(JSScript),           /* FINALIZE_SCRIPT              */
    OFFSET(LazyScript),         /* FINALIZE_LAZY_SCRIPT         */
    OFFSET(Shape),              /* FINALIZE_SHAPE               */
    OFFSET(BaseShape),          /* FINALIZE_BASE_SHAPE          */
    OFFSET(types::TypeObject),  /* FINALIZE_TYPE_OBJECT         */
    OFFSET(JSShortString),      /* FINALIZE_SHORT_STRING        */
    OFFSET(JSString),           /* FINALIZE_STRING              */
    OFFSET(JSExternalString),   /* FINALIZE_EXTERNAL_STRING     */
    OFFSET(ion::IonCode),       /* FINALIZE_IONCODE             */
};

#undef OFFSET

/*
 * Finalization order for incrementally swept things.
 */

static const AllocKind FinalizePhaseStrings[] = {
    FINALIZE_EXTERNAL_STRING
};

static const AllocKind FinalizePhaseScripts[] = {
    FINALIZE_SCRIPT,
    FINALIZE_LAZY_SCRIPT
};

static const AllocKind FinalizePhaseIonCode[] = {
    FINALIZE_IONCODE
};

static const AllocKind * const FinalizePhases[] = {
    FinalizePhaseStrings,
    FinalizePhaseScripts,
    FinalizePhaseIonCode
};
static const int FinalizePhaseCount = sizeof(FinalizePhases) / sizeof(AllocKind*);

static const int FinalizePhaseLength[] = {
    sizeof(FinalizePhaseStrings) / sizeof(AllocKind),
    sizeof(FinalizePhaseScripts) / sizeof(AllocKind),
    sizeof(FinalizePhaseIonCode) / sizeof(AllocKind)
};

static const gcstats::Phase FinalizePhaseStatsPhase[] = {
    gcstats::PHASE_SWEEP_STRING,
    gcstats::PHASE_SWEEP_SCRIPT,
    gcstats::PHASE_SWEEP_IONCODE
};

/*
 * Finalization order for things swept in the background.
 */

static const AllocKind BackgroundPhaseObjects[] = {
    FINALIZE_OBJECT0_BACKGROUND,
    FINALIZE_OBJECT2_BACKGROUND,
    FINALIZE_OBJECT4_BACKGROUND,
    FINALIZE_OBJECT8_BACKGROUND,
    FINALIZE_OBJECT12_BACKGROUND,
    FINALIZE_OBJECT16_BACKGROUND
};

static const AllocKind BackgroundPhaseStrings[] = {
    FINALIZE_SHORT_STRING,
    FINALIZE_STRING
};

static const AllocKind BackgroundPhaseShapes[] = {
    FINALIZE_SHAPE,
    FINALIZE_BASE_SHAPE,
    FINALIZE_TYPE_OBJECT
};

static const AllocKind * const BackgroundPhases[] = {
    BackgroundPhaseObjects,
    BackgroundPhaseStrings,
    BackgroundPhaseShapes
};
static const int BackgroundPhaseCount = sizeof(BackgroundPhases) / sizeof(AllocKind*);

static const int BackgroundPhaseLength[] = {
    sizeof(BackgroundPhaseObjects) / sizeof(AllocKind),
    sizeof(BackgroundPhaseStrings) / sizeof(AllocKind),
    sizeof(BackgroundPhaseShapes) / sizeof(AllocKind)
};

#ifdef DEBUG
void
ArenaHeader::checkSynchronizedWithFreeList() const
{
    /*
     * Do not allow to access the free list when its real head is still stored
     * in FreeLists and is not synchronized with this one.
     */
    JS_ASSERT(allocated());

    /*
     * We can be called from the background finalization thread when the free
     * list in the zone can mutate at any moment. We cannot do any
     * checks in this case.
     */
    if (IsBackgroundFinalized(getAllocKind()) && zone->runtimeFromAnyThread()->gcHelperThread.onBackgroundThread())
        return;

    FreeSpan firstSpan = FreeSpan::decodeOffsets(arenaAddress(), firstFreeSpanOffsets);
    if (firstSpan.isEmpty())
        return;
    const FreeSpan *list = zone->allocator.arenas.getFreeList(getAllocKind());
    if (list->isEmpty() || firstSpan.arenaAddress() != list->arenaAddress())
        return;

    /*
     * Here this arena has free things, FreeList::lists[thingKind] is not
     * empty and also points to this arena. Thus they must the same.
     */
    JS_ASSERT(firstSpan.isSameNonEmptySpan(list));
}
#endif

/* static */ void
Arena::staticAsserts()
{
    JS_STATIC_ASSERT(sizeof(Arena) == ArenaSize);
    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(ThingSizes) == FINALIZE_LIMIT);
    JS_STATIC_ASSERT(JS_ARRAY_LENGTH(FirstThingOffsets) == FINALIZE_LIMIT);
}

template<typename T>
inline bool
Arena::finalize(FreeOp *fop, AllocKind thingKind, size_t thingSize)
{
    /* Enforce requirements on size of T. */
    JS_ASSERT(thingSize % CellSize == 0);
    JS_ASSERT(thingSize <= 255);

    JS_ASSERT(aheader.allocated());
    JS_ASSERT(thingKind == aheader.getAllocKind());
    JS_ASSERT(thingSize == aheader.getThingSize());
    JS_ASSERT(!aheader.hasDelayedMarking);
    JS_ASSERT(!aheader.markOverflow);
    JS_ASSERT(!aheader.allocatedDuringIncremental);

    uintptr_t thing = thingsStart(thingKind);
    uintptr_t lastByte = thingsEnd() - 1;

    FreeSpan nextFree(aheader.getFirstFreeSpan());
    nextFree.checkSpan();

    FreeSpan newListHead;
    FreeSpan *newListTail = &newListHead;
    uintptr_t newFreeSpanStart = 0;
    bool allClear = true;
    DebugOnly<size_t> nmarked = 0;
    for (;; thing += thingSize) {
        JS_ASSERT(thing <= lastByte + 1);
        if (thing == nextFree.first) {
            JS_ASSERT(nextFree.last <= lastByte);
            if (nextFree.last == lastByte)
                break;
            JS_ASSERT(Arena::isAligned(nextFree.last, thingSize));
            if (!newFreeSpanStart)
                newFreeSpanStart = thing;
            thing = nextFree.last;
            nextFree = *nextFree.nextSpan();
            nextFree.checkSpan();
        } else {
            T *t = reinterpret_cast<T *>(thing);
            if (t->isMarked()) {
                allClear = false;
                nmarked++;
                if (newFreeSpanStart) {
                    JS_ASSERT(thing >= thingsStart(thingKind) + thingSize);
                    newListTail->first = newFreeSpanStart;
                    newListTail->last = thing - thingSize;
                    newListTail = newListTail->nextSpanUnchecked(thingSize);
                    newFreeSpanStart = 0;
                }
            } else {
                if (!newFreeSpanStart)
                    newFreeSpanStart = thing;
                t->finalize(fop);
                JS_POISON(t, JS_FREE_PATTERN, thingSize);
            }
        }
    }

    if (allClear) {
        JS_ASSERT(newListTail == &newListHead);
        JS_ASSERT(newFreeSpanStart == thingsStart(thingKind));
        return true;
    }

    newListTail->first = newFreeSpanStart ? newFreeSpanStart : nextFree.first;
    JS_ASSERT(Arena::isAligned(newListTail->first, thingSize));
    newListTail->last = lastByte;

#ifdef DEBUG
    size_t nfree = 0;
    for (const FreeSpan *span = &newListHead; span != newListTail; span = span->nextSpan()) {
        span->checkSpan();
        JS_ASSERT(Arena::isAligned(span->first, thingSize));
        JS_ASSERT(Arena::isAligned(span->last, thingSize));
        nfree += (span->last - span->first) / thingSize + 1;
        JS_ASSERT(nfree + nmarked <= thingsPerArena(thingSize));
    }
    nfree += (newListTail->last + 1 - newListTail->first) / thingSize;
    JS_ASSERT(nfree + nmarked == thingsPerArena(thingSize));
#endif
    aheader.setFirstFreeSpan(&newListHead);

    return false;
}

/*
 * Insert an arena into the list in appropriate position and update the cursor
 * to ensure that any arena before the cursor is full.
 */
void ArenaList::insert(ArenaHeader *a)
{
    JS_ASSERT(a);
    JS_ASSERT_IF(!head, cursor == &head);
    a->next = *cursor;
    *cursor = a;
    if (!a->hasFreeThings())
        cursor = &a->next;
}

template<typename T>
static inline bool
FinalizeTypedArenas(FreeOp *fop,
                    ArenaHeader **src,
                    ArenaList &dest,
                    AllocKind thingKind,
                    SliceBudget &budget)
{
    /*
     * Finalize arenas from src list, releasing empty arenas and inserting the
     * others into dest in an appropriate position.
     */

    size_t thingSize = Arena::thingSize(thingKind);

    while (ArenaHeader *aheader = *src) {
        *src = aheader->next;
        bool allClear = aheader->getArena()->finalize<T>(fop, thingKind, thingSize);
        if (allClear)
            aheader->chunk()->releaseArena(aheader);
        else
            dest.insert(aheader);
        budget.step(Arena::thingsPerArena(thingSize));
        if (budget.isOverBudget())
            return false;
    }

    return true;
}

/*
 * Finalize the list. On return al->cursor points to the first non-empty arena
 * after the al->head.
 */
static bool
FinalizeArenas(FreeOp *fop,
               ArenaHeader **src,
               ArenaList &dest,
               AllocKind thingKind,
               SliceBudget &budget)
{
    switch(thingKind) {
      case FINALIZE_OBJECT0:
      case FINALIZE_OBJECT0_BACKGROUND:
      case FINALIZE_OBJECT2:
      case FINALIZE_OBJECT2_BACKGROUND:
      case FINALIZE_OBJECT4:
      case FINALIZE_OBJECT4_BACKGROUND:
      case FINALIZE_OBJECT8:
      case FINALIZE_OBJECT8_BACKGROUND:
      case FINALIZE_OBJECT12:
      case FINALIZE_OBJECT12_BACKGROUND:
      case FINALIZE_OBJECT16:
      case FINALIZE_OBJECT16_BACKGROUND:
        return FinalizeTypedArenas<JSObject>(fop, src, dest, thingKind, budget);
      case FINALIZE_SCRIPT:
        return FinalizeTypedArenas<JSScript>(fop, src, dest, thingKind, budget);
      case FINALIZE_LAZY_SCRIPT:
        return FinalizeTypedArenas<LazyScript>(fop, src, dest, thingKind, budget);
      case FINALIZE_SHAPE:
        return FinalizeTypedArenas<Shape>(fop, src, dest, thingKind, budget);
      case FINALIZE_BASE_SHAPE:
        return FinalizeTypedArenas<BaseShape>(fop, src, dest, thingKind, budget);
      case FINALIZE_TYPE_OBJECT:
        return FinalizeTypedArenas<types::TypeObject>(fop, src, dest, thingKind, budget);
      case FINALIZE_STRING:
        return FinalizeTypedArenas<JSString>(fop, src, dest, thingKind, budget);
      case FINALIZE_SHORT_STRING:
        return FinalizeTypedArenas<JSShortString>(fop, src, dest, thingKind, budget);
      case FINALIZE_EXTERNAL_STRING:
        return FinalizeTypedArenas<JSExternalString>(fop, src, dest, thingKind, budget);
      case FINALIZE_IONCODE:
#ifdef JS_ION
        return FinalizeTypedArenas<ion::IonCode>(fop, src, dest, thingKind, budget);
#endif
      default:
        MOZ_ASSUME_UNREACHABLE("Invalid alloc kind");
    }
}

static inline Chunk *
AllocChunk(JSRuntime *rt)
{
    return static_cast<Chunk *>(MapAlignedPages(rt, ChunkSize, ChunkSize));
}

static inline void
FreeChunk(JSRuntime *rt, Chunk *p)
{
    UnmapPages(rt, static_cast<void *>(p), ChunkSize);
}

inline bool
ChunkPool::wantBackgroundAllocation(JSRuntime *rt) const
{
    /*
     * To minimize memory waste we do not want to run the background chunk
     * allocation if we have empty chunks or when the runtime needs just few
     * of them.
     */
    return rt->gcHelperThread.canBackgroundAllocate() &&
           emptyCount == 0 &&
           rt->gcChunkSet.count() >= 4;
}

/* Must be called with the GC lock taken. */
inline Chunk *
ChunkPool::get(JSRuntime *rt)
{
    JS_ASSERT(this == &rt->gcChunkPool);

    Chunk *chunk = emptyChunkListHead;
    if (chunk) {
        JS_ASSERT(emptyCount);
        emptyChunkListHead = chunk->info.next;
        --emptyCount;
    } else {
        JS_ASSERT(!emptyCount);
        chunk = Chunk::allocate(rt);
        if (!chunk)
            return NULL;
        JS_ASSERT(chunk->info.numArenasFreeCommitted == ArenasPerChunk);
        rt->gcNumArenasFreeCommitted += ArenasPerChunk;
    }
    JS_ASSERT(chunk->unused());
    JS_ASSERT(!rt->gcChunkSet.has(chunk));

    if (wantBackgroundAllocation(rt))
        rt->gcHelperThread.startBackgroundAllocationIfIdle();

    return chunk;
}

/* Must be called either during the GC or with the GC lock taken. */
inline void
ChunkPool::put(Chunk *chunk)
{
    chunk->info.age = 0;
    chunk->info.next = emptyChunkListHead;
    emptyChunkListHead = chunk;
    emptyCount++;
}

/* Must be called either during the GC or with the GC lock taken. */
Chunk *
ChunkPool::expire(JSRuntime *rt, bool releaseAll)
{
    JS_ASSERT(this == &rt->gcChunkPool);

    /*
     * Return old empty chunks to the system while preserving the order of
     * other chunks in the list. This way, if the GC runs several times
     * without emptying the list, the older chunks will stay at the tail
     * and are more likely to reach the max age.
     */
    Chunk *freeList = NULL;
    int freeChunkCount = 0;
    for (Chunk **chunkp = &emptyChunkListHead; *chunkp; ) {
        JS_ASSERT(emptyCount);
        Chunk *chunk = *chunkp;
        JS_ASSERT(chunk->unused());
        JS_ASSERT(!rt->gcChunkSet.has(chunk));
        JS_ASSERT(chunk->info.age <= MAX_EMPTY_CHUNK_AGE);
        if (releaseAll || chunk->info.age == MAX_EMPTY_CHUNK_AGE ||
            freeChunkCount++ > MAX_EMPTY_CHUNK_COUNT)
        {
            *chunkp = chunk->info.next;
            --emptyCount;
            chunk->prepareToBeFreed(rt);
            chunk->info.next = freeList;
            freeList = chunk;
        } else {
            /* Keep the chunk but increase its age. */
            ++chunk->info.age;
            chunkp = &chunk->info.next;
        }
    }
    JS_ASSERT_IF(releaseAll, !emptyCount);
    return freeList;
}

static void
FreeChunkList(JSRuntime *rt, Chunk *chunkListHead)
{
    while (Chunk *chunk = chunkListHead) {
        JS_ASSERT(!chunk->info.numArenasFreeCommitted);
        chunkListHead = chunk->info.next;
        FreeChunk(rt, chunk);
    }
}

void
ChunkPool::expireAndFree(JSRuntime *rt, bool releaseAll)
{
    FreeChunkList(rt, expire(rt, releaseAll));
}

/* static */ Chunk *
Chunk::allocate(JSRuntime *rt)
{
    Chunk *chunk = AllocChunk(rt);

#ifdef JSGC_ROOT_ANALYSIS
    // Our poison pointers are not guaranteed to be invalid on 64-bit
    // architectures, and often are valid. We can't just reserve the full
    // poison range, because it might already have been taken up by something
    // else (shared library, previous allocation). So we'll just loop and
    // discard poison pointers until we get something valid.
    //
    // This leaks all of these poisoned pointers. It would be better if they
    // were marked as uncommitted, but it's a little complicated to avoid
    // clobbering pre-existing unrelated mappings.
    while (IsPoisonedPtr(chunk))
        chunk = AllocChunk(rt);
#endif

    if (!chunk)
        return NULL;
    chunk->init(rt);
    rt->gcStats.count(gcstats::STAT_NEW_CHUNK);
    return chunk;
}

/* Must be called with the GC lock taken. */
/* static */ inline void
Chunk::release(JSRuntime *rt, Chunk *chunk)
{
    JS_ASSERT(chunk);
    chunk->prepareToBeFreed(rt);
    FreeChunk(rt, chunk);
}

inline void
Chunk::prepareToBeFreed(JSRuntime *rt)
{
    JS_ASSERT(rt->gcNumArenasFreeCommitted >= info.numArenasFreeCommitted);
    rt->gcNumArenasFreeCommitted -= info.numArenasFreeCommitted;
    rt->gcStats.count(gcstats::STAT_DESTROY_CHUNK);

#ifdef DEBUG
    /*
     * Let FreeChunkList detect a missing prepareToBeFreed call before it
     * frees chunk.
     */
    info.numArenasFreeCommitted = 0;
#endif
}

void
Chunk::init(JSRuntime *rt)
{
    JS_POISON(this, JS_FREE_PATTERN, ChunkSize);

    /*
     * We clear the bitmap to guard against xpc_IsGrayGCThing being called on
     * uninitialized data, which would happen before the first GC cycle.
     */
    bitmap.clear();

    /* Initialize the arena tracking bitmap. */
    decommittedArenas.clear(false);

    /* Initialize the chunk info. */
    info.freeArenasHead = &arenas[0].aheader;
    info.lastDecommittedArenaOffset = 0;
    info.numArenasFree = ArenasPerChunk;
    info.numArenasFreeCommitted = ArenasPerChunk;
    info.age = 0;
    info.runtime = rt;

    /* Initialize the arena header state. */
    for (unsigned i = 0; i < ArenasPerChunk; i++) {
        arenas[i].aheader.setAsNotAllocated();
        arenas[i].aheader.next = (i + 1 < ArenasPerChunk)
                                 ? &arenas[i + 1].aheader
                                 : NULL;
    }

    /* The rest of info fields are initialized in PickChunk. */
}

static inline Chunk **
GetAvailableChunkList(Zone *zone)
{
    JSRuntime *rt = zone->runtimeFromAnyThread();
    return zone->isSystem
           ? &rt->gcSystemAvailableChunkListHead
           : &rt->gcUserAvailableChunkListHead;
}

inline void
Chunk::addToAvailableList(Zone *zone)
{
    insertToAvailableList(GetAvailableChunkList(zone));
}

inline void
Chunk::insertToAvailableList(Chunk **insertPoint)
{
    JS_ASSERT(hasAvailableArenas());
    JS_ASSERT(!info.prevp);
    JS_ASSERT(!info.next);
    info.prevp = insertPoint;
    Chunk *insertBefore = *insertPoint;
    if (insertBefore) {
        JS_ASSERT(insertBefore->info.prevp == insertPoint);
        insertBefore->info.prevp = &info.next;
    }
    info.next = insertBefore;
    *insertPoint = this;
}

inline void
Chunk::removeFromAvailableList()
{
    JS_ASSERT(info.prevp);
    *info.prevp = info.next;
    if (info.next) {
        JS_ASSERT(info.next->info.prevp == &info.next);
        info.next->info.prevp = info.prevp;
    }
    info.prevp = NULL;
    info.next = NULL;
}

/*
 * Search for and return the next decommitted Arena. Our goal is to keep
 * lastDecommittedArenaOffset "close" to a free arena. We do this by setting
 * it to the most recently freed arena when we free, and forcing it to
 * the last alloc + 1 when we allocate.
 */
uint32_t
Chunk::findDecommittedArenaOffset()
{
    /* Note: lastFreeArenaOffset can be past the end of the list. */
    for (unsigned i = info.lastDecommittedArenaOffset; i < ArenasPerChunk; i++)
        if (decommittedArenas.get(i))
            return i;
    for (unsigned i = 0; i < info.lastDecommittedArenaOffset; i++)
        if (decommittedArenas.get(i))
            return i;
    MOZ_ASSUME_UNREACHABLE("No decommitted arenas found.");
}

ArenaHeader *
Chunk::fetchNextDecommittedArena()
{
    JS_ASSERT(info.numArenasFreeCommitted == 0);
    JS_ASSERT(info.numArenasFree > 0);

    unsigned offset = findDecommittedArenaOffset();
    info.lastDecommittedArenaOffset = offset + 1;
    --info.numArenasFree;
    decommittedArenas.unset(offset);

    Arena *arena = &arenas[offset];
    MarkPagesInUse(info.runtime, arena, ArenaSize);
    arena->aheader.setAsNotAllocated();

    return &arena->aheader;
}

inline ArenaHeader *
Chunk::fetchNextFreeArena(JSRuntime *rt)
{
    JS_ASSERT(info.numArenasFreeCommitted > 0);
    JS_ASSERT(info.numArenasFreeCommitted <= info.numArenasFree);
    JS_ASSERT(info.numArenasFreeCommitted <= rt->gcNumArenasFreeCommitted);

    ArenaHeader *aheader = info.freeArenasHead;
    info.freeArenasHead = aheader->next;
    --info.numArenasFreeCommitted;
    --info.numArenasFree;
    --rt->gcNumArenasFreeCommitted;

    return aheader;
}

ArenaHeader *
Chunk::allocateArena(Zone *zone, AllocKind thingKind)
{
    JS_ASSERT(hasAvailableArenas());

    JSRuntime *rt = zone->runtimeFromAnyThread();
    if (!rt->isHeapMinorCollecting() && rt->gcBytes >= rt->gcMaxBytes)
        return NULL;

    ArenaHeader *aheader = JS_LIKELY(info.numArenasFreeCommitted > 0)
                           ? fetchNextFreeArena(rt)
                           : fetchNextDecommittedArena();
    aheader->init(zone, thingKind);
    if (JS_UNLIKELY(!hasAvailableArenas()))
        removeFromAvailableList();

    rt->gcBytes += ArenaSize;
    zone->gcBytes += ArenaSize;
    if (zone->gcBytes >= zone->gcTriggerBytes)
        TriggerZoneGC(zone, JS::gcreason::ALLOC_TRIGGER);

    return aheader;
}

inline void
Chunk::addArenaToFreeList(JSRuntime *rt, ArenaHeader *aheader)
{
    JS_ASSERT(!aheader->allocated());
    aheader->next = info.freeArenasHead;
    info.freeArenasHead = aheader;
    ++info.numArenasFreeCommitted;
    ++info.numArenasFree;
    ++rt->gcNumArenasFreeCommitted;
}

void
Chunk::releaseArena(ArenaHeader *aheader)
{
    JS_ASSERT(aheader->allocated());
    JS_ASSERT(!aheader->hasDelayedMarking);
    Zone *zone = aheader->zone;
    JSRuntime *rt = zone->runtimeFromAnyThread();
    AutoLockGC maybeLock;
    if (rt->gcHelperThread.sweeping())
        maybeLock.lock(rt);

    JS_ASSERT(rt->gcBytes >= ArenaSize);
    JS_ASSERT(zone->gcBytes >= ArenaSize);
    if (rt->gcHelperThread.sweeping())
        zone->reduceGCTriggerBytes(zone->gcHeapGrowthFactor * ArenaSize);
    rt->gcBytes -= ArenaSize;
    zone->gcBytes -= ArenaSize;

    aheader->setAsNotAllocated();
    addArenaToFreeList(rt, aheader);

    if (info.numArenasFree == 1) {
        JS_ASSERT(!info.prevp);
        JS_ASSERT(!info.next);
        addToAvailableList(zone);
    } else if (!unused()) {
        JS_ASSERT(info.prevp);
    } else {
        rt->gcChunkSet.remove(this);
        removeFromAvailableList();
        rt->gcChunkPool.put(this);
    }
}

/* The caller must hold the GC lock. */
static Chunk *
PickChunk(Zone *zone)
{
    JSRuntime *rt = zone->runtimeFromAnyThread();
    Chunk **listHeadp = GetAvailableChunkList(zone);
    Chunk *chunk = *listHeadp;
    if (chunk)
        return chunk;

    chunk = rt->gcChunkPool.get(rt);
    if (!chunk)
        return NULL;

    rt->gcChunkAllocationSinceLastGC = true;

    /*
     * FIXME bug 583732 - chunk is newly allocated and cannot be present in
     * the table so using ordinary lookupForAdd is suboptimal here.
     */
    GCChunkSet::AddPtr p = rt->gcChunkSet.lookupForAdd(chunk);
    JS_ASSERT(!p);
    if (!rt->gcChunkSet.add(p, chunk)) {
        Chunk::release(rt, chunk);
        return NULL;
    }

    chunk->info.prevp = NULL;
    chunk->info.next = NULL;
    chunk->addToAvailableList(zone);

    return chunk;
}

#ifdef JS_GC_ZEAL

extern void
js::SetGCZeal(JSRuntime *rt, uint8_t zeal, uint32_t frequency)
{
    if (zeal == 0) {
        if (rt->gcVerifyPreData)
            VerifyBarriers(rt, PreBarrierVerifier);
        if (rt->gcVerifyPostData)
            VerifyBarriers(rt, PostBarrierVerifier);
    }

    bool schedule = zeal >= js::gc::ZealAllocValue;
    rt->gcZeal_ = zeal;
    rt->gcZealFrequency = frequency;
    rt->gcNextScheduled = schedule ? frequency : 0;
}

static bool
InitGCZeal(JSRuntime *rt)
{
    const char *env = getenv("JS_GC_ZEAL");
    if (!env)
        return true;

    int zeal = -1;
    int frequency = JS_DEFAULT_ZEAL_FREQ;
    if (strcmp(env, "help") != 0) {
        zeal = atoi(env);
        const char *p = strchr(env, ',');
        if (p)
            frequency = atoi(p + 1);
    }

    if (zeal < 0 || zeal > ZealLimit || frequency < 0) {
        fprintf(stderr,
                "Format: JS_GC_ZEAL=N[,F]\n"
                "N indicates \"zealousness\":\n"
                "  0: no additional GCs\n"
                "  1: additional GCs at common danger points\n"
                "  2: GC every F allocations (default: 100)\n"
                "  3: GC when the window paints (browser only)\n"
                "  4: Verify pre write barriers between instructions\n"
                "  5: Verify pre write barriers between paints\n"
                "  6: Verify stack rooting\n"
                "  7: Verify stack rooting (yes, it's the same as 6)\n"
                "  8: Incremental GC in two slices: 1) mark roots 2) finish collection\n"
                "  9: Incremental GC in two slices: 1) mark all 2) new marking and finish\n"
                " 10: Incremental GC in multiple slices\n"
                " 11: Verify post write barriers between instructions\n"
                " 12: Verify post write barriers between paints\n"
                " 13: Purge analysis state every F allocations (default: 100)\n");
        return false;
    }

    SetGCZeal(rt, zeal, frequency);
    return true;
}

#endif

/* Lifetime for type sets attached to scripts containing observed types. */
static const int64_t JIT_SCRIPT_RELEASE_TYPES_INTERVAL = 60 * 1000 * 1000;

JSBool
js_InitGC(JSRuntime *rt, uint32_t maxbytes)
{
    InitMemorySubsystem(rt);

    if (!rt->gcChunkSet.init(INITIAL_CHUNK_CAPACITY))
        return false;

    if (!rt->gcRootsHash.init(256))
        return false;

#ifdef JS_THREADSAFE
    rt->gcLock = PR_NewLock();
    if (!rt->gcLock)
        return false;
#endif
    if (!rt->gcHelperThread.init())
        return false;

    /*
     * Separate gcMaxMallocBytes from gcMaxBytes but initialize to maxbytes
     * for default backward API compatibility.
     */
    rt->gcMaxBytes = maxbytes;
    rt->setGCMaxMallocBytes(maxbytes);

#ifndef JS_MORE_DETERMINISTIC
    rt->gcJitReleaseTime = PRMJ_Now() + JIT_SCRIPT_RELEASE_TYPES_INTERVAL;
#endif

#ifdef JSGC_GENERATIONAL
    if (!rt->gcNursery.init())
        return false;

    if (!rt->gcStoreBuffer.enable())
        return false;
#endif

#ifdef JS_GC_ZEAL
    if (!InitGCZeal(rt))
        return false;
#endif

    return true;
}

static void
RecordNativeStackTopForGC(JSRuntime *rt)
{
    ConservativeGCData *cgcd = &rt->conservativeGC;

#ifdef JS_THREADSAFE
    /* Record the stack top here only if we are called from a request. */
    if (!rt->requestDepth)
        return;
#endif
    cgcd->recordStackTop();
}

void
js_FinishGC(JSRuntime *rt)
{
    /*
     * Wait until the background finalization stops and the helper thread
     * shuts down before we forcefully release any remaining GC memory.
     */
    rt->gcHelperThread.finish();

#ifdef JS_GC_ZEAL
    /* Free memory associated with GC verification. */
    FinishVerifier(rt);
#endif

    /* Delete all remaining zones. */
    for (ZonesIter zone(rt); !zone.done(); zone.next()) {
        for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next())
            js_delete(comp.get());
        js_delete(zone.get());
    }

    rt->zones.clear();
    rt->atomsCompartment = NULL;

    rt->gcSystemAvailableChunkListHead = NULL;
    rt->gcUserAvailableChunkListHead = NULL;
    for (GCChunkSet::Range r(rt->gcChunkSet.all()); !r.empty(); r.popFront())
        Chunk::release(rt, r.front());
    rt->gcChunkSet.clear();

    rt->gcChunkPool.expireAndFree(rt, true);

    rt->gcRootsHash.clear();
}

template <typename T> struct BarrierOwner {};
template <typename T> struct BarrierOwner<T *> { typedef T result; };
template <> struct BarrierOwner<Value> { typedef HeapValue result; };

template <typename T>
static bool
AddRoot(JSRuntime *rt, T *rp, const char *name, JSGCRootType rootType)
{
    /*
     * Sometimes Firefox will hold weak references to objects and then convert
     * them to strong references by calling AddRoot (e.g., via PreserveWrapper,
     * or ModifyBusyCount in workers). We need a read barrier to cover these
     * cases.
     */
    if (rt->gcIncrementalState != NO_INCREMENTAL)
        BarrierOwner<T>::result::writeBarrierPre(*rp);

    return rt->gcRootsHash.put((void *)rp, RootInfo(name, rootType));
}

template <typename T>
static bool
AddRoot(JSContext *cx, T *rp, const char *name, JSGCRootType rootType)
{
    bool ok = AddRoot(cx->runtime(), rp, name, rootType);
    if (!ok)
        JS_ReportOutOfMemory(cx);
    return ok;
}

JSBool
js::AddValueRoot(JSContext *cx, Value *vp, const char *name)
{
    return AddRoot(cx, vp, name, JS_GC_ROOT_VALUE_PTR);
}

extern JSBool
js::AddValueRootRT(JSRuntime *rt, js::Value *vp, const char *name)
{
    return AddRoot(rt, vp, name, JS_GC_ROOT_VALUE_PTR);
}

extern JSBool
js::AddStringRoot(JSContext *cx, JSString **rp, const char *name)
{
    return AddRoot(cx, rp, name, JS_GC_ROOT_STRING_PTR);
}

extern JSBool
js::AddObjectRoot(JSContext *cx, JSObject **rp, const char *name)
{
    return AddRoot(cx, rp, name, JS_GC_ROOT_OBJECT_PTR);
}

extern JSBool
js::AddScriptRoot(JSContext *cx, JSScript **rp, const char *name)
{
    return AddRoot(cx, rp, name, JS_GC_ROOT_SCRIPT_PTR);
}

extern JS_FRIEND_API(bool)
js_AddObjectRoot(JSRuntime *rt, JSObject **objp)
{
    return AddRoot(rt, objp, NULL, JS_GC_ROOT_OBJECT_PTR);
}

extern JS_FRIEND_API(void)
js_RemoveObjectRoot(JSRuntime *rt, JSObject **objp)
{
    js_RemoveRoot(rt, objp);
}

JS_FRIEND_API(void)
js_RemoveRoot(JSRuntime *rt, void *rp)
{
    rt->gcRootsHash.remove(rp);
    rt->gcPoke = true;
}

typedef RootedValueMap::Range RootRange;
typedef RootedValueMap::Entry RootEntry;
typedef RootedValueMap::Enum RootEnum;

static size_t
ComputeTriggerBytes(Zone *zone, size_t lastBytes, size_t maxBytes, JSGCInvocationKind gckind)
{
    size_t base = gckind == GC_SHRINK ? lastBytes : Max(lastBytes, zone->runtimeFromMainThread()->gcAllocationThreshold);
    float trigger = float(base) * zone->gcHeapGrowthFactor;
    return size_t(Min(float(maxBytes), trigger));
}

void
Zone::setGCLastBytes(size_t lastBytes, JSGCInvocationKind gckind)
{
    /*
     * The heap growth factor depends on the heap size after a GC and the GC frequency.
     * For low frequency GCs (more than 1sec between GCs) we let the heap grow to 150%.
     * For high frequency GCs we let the heap grow depending on the heap size:
     *   lastBytes < highFrequencyLowLimit: 300%
     *   lastBytes > highFrequencyHighLimit: 150%
     *   otherwise: linear interpolation between 150% and 300% based on lastBytes
     */
    JSRuntime *rt = runtimeFromMainThread();

    if (!rt->gcDynamicHeapGrowth) {
        gcHeapGrowthFactor = 3.0;
    } else if (lastBytes < 1 * 1024 * 1024) {
        gcHeapGrowthFactor = rt->gcLowFrequencyHeapGrowth;
    } else {
        JS_ASSERT(rt->gcHighFrequencyHighLimitBytes > rt->gcHighFrequencyLowLimitBytes);
        uint64_t now = PRMJ_Now();
        if (rt->gcLastGCTime && rt->gcLastGCTime + rt->gcHighFrequencyTimeThreshold * PRMJ_USEC_PER_MSEC > now) {
            if (lastBytes <= rt->gcHighFrequencyLowLimitBytes) {
                gcHeapGrowthFactor = rt->gcHighFrequencyHeapGrowthMax;
            } else if (lastBytes >= rt->gcHighFrequencyHighLimitBytes) {
                gcHeapGrowthFactor = rt->gcHighFrequencyHeapGrowthMin;
            } else {
                double k = (rt->gcHighFrequencyHeapGrowthMin - rt->gcHighFrequencyHeapGrowthMax)
                           / (double)(rt->gcHighFrequencyHighLimitBytes - rt->gcHighFrequencyLowLimitBytes);
                gcHeapGrowthFactor = (k * (lastBytes - rt->gcHighFrequencyLowLimitBytes)
                                     + rt->gcHighFrequencyHeapGrowthMax);
                JS_ASSERT(gcHeapGrowthFactor <= rt->gcHighFrequencyHeapGrowthMax
                          && gcHeapGrowthFactor >= rt->gcHighFrequencyHeapGrowthMin);
            }
            rt->gcHighFrequencyGC = true;
        } else {
            gcHeapGrowthFactor = rt->gcLowFrequencyHeapGrowth;
            rt->gcHighFrequencyGC = false;
        }
    }
    gcTriggerBytes = ComputeTriggerBytes(this, lastBytes, rt->gcMaxBytes, gckind);
}

void
Zone::reduceGCTriggerBytes(size_t amount)
{
    JS_ASSERT(amount > 0);
    JS_ASSERT(gcTriggerBytes >= amount);
    if (gcTriggerBytes - amount < runtimeFromAnyThread()->gcAllocationThreshold * gcHeapGrowthFactor)
        return;
    gcTriggerBytes -= amount;
}

Allocator::Allocator(Zone *zone)
  : zone_(zone)
{}

inline void
ArenaLists::prepareForIncrementalGC(JSRuntime *rt)
{
    for (size_t i = 0; i != FINALIZE_LIMIT; ++i) {
        FreeSpan *headSpan = &freeLists[i];
        if (!headSpan->isEmpty()) {
            ArenaHeader *aheader = headSpan->arenaHeader();
            aheader->allocatedDuringIncremental = true;
            rt->gcMarker.delayMarkingArena(aheader);
        }
    }
}

static inline void
PushArenaAllocatedDuringSweep(JSRuntime *runtime, ArenaHeader *arena)
{
    arena->setNextAllocDuringSweep(runtime->gcArenasAllocatedDuringSweep);
    runtime->gcArenasAllocatedDuringSweep = arena;
}

inline void *
ArenaLists::allocateFromArenaInline(Zone *zone, AllocKind thingKind)
{
    /*
     * Parallel JS Note:
     *
     * This function can be called from parallel threads all of which
     * are associated with the same compartment. In that case, each
     * thread will have a distinct ArenaLists.  Therefore, whenever we
     * fall through to PickChunk() we must be sure that we are holding
     * a lock.
     */

    Chunk *chunk = NULL;

    ArenaList *al = &arenaLists[thingKind];
    AutoLockGC maybeLock;

#ifdef JS_THREADSAFE
    volatile uintptr_t *bfs = &backgroundFinalizeState[thingKind];
    if (*bfs != BFS_DONE) {
        /*
         * We cannot search the arena list for free things while the
         * background finalization runs and can modify head or cursor at any
         * moment. So we always allocate a new arena in that case.
         */
        maybeLock.lock(zone->runtimeFromMainThread());
        if (*bfs == BFS_RUN) {
            JS_ASSERT(!*al->cursor);
            chunk = PickChunk(zone);
            if (!chunk) {
                /*
                 * Let the caller to wait for the background allocation to
                 * finish and restart the allocation attempt.
                 */
                return NULL;
            }
        } else if (*bfs == BFS_JUST_FINISHED) {
            /* See comments before BackgroundFinalizeState definition. */
            *bfs = BFS_DONE;
        } else {
            JS_ASSERT(*bfs == BFS_DONE);
        }
    }
#endif /* JS_THREADSAFE */

    if (!chunk) {
        if (ArenaHeader *aheader = *al->cursor) {
            JS_ASSERT(aheader->hasFreeThings());

            /*
             * The empty arenas are returned to the chunk and should not present on
             * the list.
             */
            JS_ASSERT(!aheader->isEmpty());
            al->cursor = &aheader->next;

            /*
             * Move the free span stored in the arena to the free list and
             * allocate from it.
             */
            freeLists[thingKind] = aheader->getFirstFreeSpan();
            aheader->setAsFullyUsed();
            if (JS_UNLIKELY(zone->wasGCStarted())) {
                if (zone->needsBarrier()) {
                    aheader->allocatedDuringIncremental = true;
                    zone->runtimeFromMainThread()->gcMarker.delayMarkingArena(aheader);
                } else if (zone->isGCSweeping()) {
                    PushArenaAllocatedDuringSweep(zone->runtimeFromMainThread(), aheader);
                }
            }
            return freeLists[thingKind].infallibleAllocate(Arena::thingSize(thingKind));
        }

        /* Make sure we hold the GC lock before we call PickChunk. */
        if (!maybeLock.locked())
            maybeLock.lock(zone->runtimeFromAnyThread());
        chunk = PickChunk(zone);
        if (!chunk)
            return NULL;
    }

    /*
     * While we still hold the GC lock get an arena from some chunk, mark it
     * as full as its single free span is moved to the free lits, and insert
     * it to the list as a fully allocated arena.
     *
     * We add the arena before the the head, not after the tail pointed by the
     * cursor, so after the GC the most recently added arena will be used first
     * for allocations improving cache locality.
     */
    JS_ASSERT(!*al->cursor);
    ArenaHeader *aheader = chunk->allocateArena(zone, thingKind);
    if (!aheader)
        return NULL;

    if (JS_UNLIKELY(zone->wasGCStarted())) {
        if (zone->needsBarrier()) {
            aheader->allocatedDuringIncremental = true;
            zone->runtimeFromMainThread()->gcMarker.delayMarkingArena(aheader);
        } else if (zone->isGCSweeping()) {
            PushArenaAllocatedDuringSweep(zone->runtimeFromMainThread(), aheader);
        }
    }
    aheader->next = al->head;
    if (!al->head) {
        JS_ASSERT(al->cursor == &al->head);
        al->cursor = &aheader->next;
    }
    al->head = aheader;

    /* See comments before allocateFromNewArena about this assert. */
    JS_ASSERT(!aheader->hasFreeThings());
    uintptr_t arenaAddr = aheader->arenaAddress();
    return freeLists[thingKind].allocateFromNewArena(arenaAddr,
                                                     Arena::firstThingOffset(thingKind),
                                                     Arena::thingSize(thingKind));
}

void *
ArenaLists::allocateFromArena(JS::Zone *zone, AllocKind thingKind)
{
    return allocateFromArenaInline(zone, thingKind);
}

void
ArenaLists::finalizeNow(FreeOp *fop, AllocKind thingKind)
{
    JS_ASSERT(!IsBackgroundFinalized(thingKind));
    JS_ASSERT(backgroundFinalizeState[thingKind] == BFS_DONE ||
              backgroundFinalizeState[thingKind] == BFS_JUST_FINISHED);

    ArenaHeader *arenas = arenaLists[thingKind].head;
    arenaLists[thingKind].clear();

    SliceBudget budget;
    FinalizeArenas(fop, &arenas, arenaLists[thingKind], thingKind, budget);
    JS_ASSERT(!arenas);
}

void
ArenaLists::queueForForegroundSweep(FreeOp *fop, AllocKind thingKind)
{
    JS_ASSERT(!IsBackgroundFinalized(thingKind));
    JS_ASSERT(backgroundFinalizeState[thingKind] == BFS_DONE);
    JS_ASSERT(!arenaListsToSweep[thingKind]);

    arenaListsToSweep[thingKind] = arenaLists[thingKind].head;
    arenaLists[thingKind].clear();
}

inline void
ArenaLists::queueForBackgroundSweep(FreeOp *fop, AllocKind thingKind)
{
    JS_ASSERT(IsBackgroundFinalized(thingKind));

#ifdef JS_THREADSAFE
    JS_ASSERT(!fop->runtime()->gcHelperThread.sweeping());
#endif

    ArenaList *al = &arenaLists[thingKind];
    if (!al->head) {
        JS_ASSERT(backgroundFinalizeState[thingKind] == BFS_DONE);
        JS_ASSERT(al->cursor == &al->head);
        return;
    }

    /*
     * The state can be done, or just-finished if we have not allocated any GC
     * things from the arena list after the previous background finalization.
     */
    JS_ASSERT(backgroundFinalizeState[thingKind] == BFS_DONE ||
              backgroundFinalizeState[thingKind] == BFS_JUST_FINISHED);

    arenaListsToSweep[thingKind] = al->head;
    al->clear();
    backgroundFinalizeState[thingKind] = BFS_RUN;
}

/*static*/ void
ArenaLists::backgroundFinalize(FreeOp *fop, ArenaHeader *listHead, bool onBackgroundThread)
{
    JS_ASSERT(listHead);
    AllocKind thingKind = listHead->getAllocKind();
    Zone *zone = listHead->zone;

    ArenaList finalized;
    SliceBudget budget;
    FinalizeArenas(fop, &listHead, finalized, thingKind, budget);
    JS_ASSERT(!listHead);

    /*
     * After we finish the finalization al->cursor must point to the end of
     * the head list as we emptied the list before the background finalization
     * and the allocation adds new arenas before the cursor.
     */
    ArenaLists *lists = &zone->allocator.arenas;
    ArenaList *al = &lists->arenaLists[thingKind];

    AutoLockGC lock(fop->runtime());
    JS_ASSERT(lists->backgroundFinalizeState[thingKind] == BFS_RUN);
    JS_ASSERT(!*al->cursor);

    if (finalized.head) {
        *al->cursor = finalized.head;
        if (finalized.cursor != &finalized.head)
            al->cursor = finalized.cursor;
    }

    /*
     * We must set the state to BFS_JUST_FINISHED if we are running on the
     * background thread and we have touched arenaList list, even if we add to
     * the list only fully allocated arenas without any free things. It ensures
     * that the allocation thread takes the GC lock and all writes to the free
     * list elements are propagated. As we always take the GC lock when
     * allocating new arenas from the chunks we can set the state to BFS_DONE if
     * we have released all finalized arenas back to their chunks.
     */
    if (onBackgroundThread && finalized.head)
        lists->backgroundFinalizeState[thingKind] = BFS_JUST_FINISHED;
    else
        lists->backgroundFinalizeState[thingKind] = BFS_DONE;

    lists->arenaListsToSweep[thingKind] = NULL;
}

void
ArenaLists::queueObjectsForSweep(FreeOp *fop)
{
    gcstats::AutoPhase ap(fop->runtime()->gcStats, gcstats::PHASE_SWEEP_OBJECT);

    finalizeNow(fop, FINALIZE_OBJECT0);
    finalizeNow(fop, FINALIZE_OBJECT2);
    finalizeNow(fop, FINALIZE_OBJECT4);
    finalizeNow(fop, FINALIZE_OBJECT8);
    finalizeNow(fop, FINALIZE_OBJECT12);
    finalizeNow(fop, FINALIZE_OBJECT16);

    queueForBackgroundSweep(fop, FINALIZE_OBJECT0_BACKGROUND);
    queueForBackgroundSweep(fop, FINALIZE_OBJECT2_BACKGROUND);
    queueForBackgroundSweep(fop, FINALIZE_OBJECT4_BACKGROUND);
    queueForBackgroundSweep(fop, FINALIZE_OBJECT8_BACKGROUND);
    queueForBackgroundSweep(fop, FINALIZE_OBJECT12_BACKGROUND);
    queueForBackgroundSweep(fop, FINALIZE_OBJECT16_BACKGROUND);
}

void
ArenaLists::queueStringsForSweep(FreeOp *fop)
{
    gcstats::AutoPhase ap(fop->runtime()->gcStats, gcstats::PHASE_SWEEP_STRING);

    queueForBackgroundSweep(fop, FINALIZE_SHORT_STRING);
    queueForBackgroundSweep(fop, FINALIZE_STRING);

    queueForForegroundSweep(fop, FINALIZE_EXTERNAL_STRING);
}

void
ArenaLists::queueScriptsForSweep(FreeOp *fop)
{
    gcstats::AutoPhase ap(fop->runtime()->gcStats, gcstats::PHASE_SWEEP_SCRIPT);
    queueForForegroundSweep(fop, FINALIZE_SCRIPT);
    queueForForegroundSweep(fop, FINALIZE_LAZY_SCRIPT);
}

void
ArenaLists::queueIonCodeForSweep(FreeOp *fop)
{
    gcstats::AutoPhase ap(fop->runtime()->gcStats, gcstats::PHASE_SWEEP_IONCODE);
    queueForForegroundSweep(fop, FINALIZE_IONCODE);
}

void
ArenaLists::queueShapesForSweep(FreeOp *fop)
{
    gcstats::AutoPhase ap(fop->runtime()->gcStats, gcstats::PHASE_SWEEP_SHAPE);

    queueForBackgroundSweep(fop, FINALIZE_SHAPE);
    queueForBackgroundSweep(fop, FINALIZE_BASE_SHAPE);
    queueForBackgroundSweep(fop, FINALIZE_TYPE_OBJECT);
}

static void *
RunLastDitchGC(JSContext *cx, JS::Zone *zone, AllocKind thingKind)
{
    /*
     * In parallel sections, we do not attempt to refill the free list
     * and hence do not encounter last ditch GC.
     */
    JS_ASSERT(!InParallelSection());

    PrepareZoneForGC(zone);

    JSRuntime *rt = cx->runtime();

    /* The last ditch GC preserves all atoms. */
    AutoKeepAtoms keepAtoms(cx->perThreadData);
    GC(rt, GC_NORMAL, JS::gcreason::LAST_DITCH);

    /*
     * The JSGC_END callback can legitimately allocate new GC
     * things and populate the free list. If that happens, just
     * return that list head.
     */
    size_t thingSize = Arena::thingSize(thingKind);
    if (void *thing = zone->allocator.arenas.allocateFromFreeList(thingKind, thingSize))
        return thing;

    return NULL;
}

template <AllowGC allowGC>
/* static */ void *
ArenaLists::refillFreeList(ThreadSafeContext *cx, AllocKind thingKind)
{
    JS_ASSERT(cx->allocator()->arenas.freeLists[thingKind].isEmpty());
    JS_ASSERT(!cx->isHeapBusy());

    Zone *zone = cx->allocator()->zone_;

    bool runGC = cx->allowGC() && allowGC &&
                 cx->asJSContext()->runtime()->gcIncrementalState != NO_INCREMENTAL &&
                 zone->gcBytes > zone->gcTriggerBytes;

    for (;;) {
        if (JS_UNLIKELY(runGC)) {
            if (void *thing = RunLastDitchGC(cx->asJSContext(), zone, thingKind))
                return thing;
        }

        /*
         * allocateFromArena may fail while the background finalization still
         * run. If we aren't in a fork join, we want to wait for it to finish
         * and restart. However, checking for that is racy as the background
         * finalization could free some things after allocateFromArena decided
         * to fail but at this point it may have already stopped. To avoid
         * this race we always try to allocate twice.
         *
         * If we're in a fork join, we simply try it once and return whatever
         * value we get.
         */
        for (bool secondAttempt = false; ; secondAttempt = true) {
            void *thing = cx->allocator()->arenas.allocateFromArenaInline(zone, thingKind);
            if (JS_LIKELY(!!thing) || !cx->isJSContext())
                return thing;
            if (secondAttempt)
                break;

            cx->asJSContext()->runtime()->gcHelperThread.waitBackgroundSweepEnd();
        }

        if (!cx->allowGC() || !allowGC)
            return NULL;

        /*
         * We failed to allocate. Run the GC if we haven't done it already.
         * Otherwise report OOM.
         */
        if (runGC)
            break;
        runGC = true;
    }

    JS_ASSERT(allowGC);
    js_ReportOutOfMemory(cx);
    return NULL;
}

template void *
ArenaLists::refillFreeList<NoGC>(ThreadSafeContext *cx, AllocKind thingKind);

template void *
ArenaLists::refillFreeList<CanGC>(ThreadSafeContext *cx, AllocKind thingKind);

JSGCTraceKind
js_GetGCThingTraceKind(void *thing)
{
    return GetGCThingTraceKind(thing);
}

void
js::InitTracer(JSTracer *trc, JSRuntime *rt, JSTraceCallback callback)
{
    trc->runtime = rt;
    trc->callback = callback;
    trc->debugPrinter = NULL;
    trc->debugPrintArg = NULL;
    trc->debugPrintIndex = size_t(-1);
    trc->eagerlyTraceWeakMaps = TraceWeakMapValues;
#ifdef JS_GC_ZEAL
    trc->realLocation = NULL;
#endif
}

/* static */ int64_t
SliceBudget::TimeBudget(int64_t millis)
{
    return millis * PRMJ_USEC_PER_MSEC;
}

/* static */ int64_t
SliceBudget::WorkBudget(int64_t work)
{
    /* For work = 0 not to mean Unlimited, we subtract 1. */
    return -work - 1;
}

SliceBudget::SliceBudget()
  : deadline(INT64_MAX),
    counter(INTPTR_MAX)
{
}

SliceBudget::SliceBudget(int64_t budget)
{
    if (budget == Unlimited) {
        deadline = INT64_MAX;
        counter = INTPTR_MAX;
    } else if (budget > 0) {
        deadline = PRMJ_Now() + budget;
        counter = CounterReset;
    } else {
        deadline = 0;
        counter = -budget - 1;
    }
}

bool
SliceBudget::checkOverBudget()
{
    bool over = PRMJ_Now() > deadline;
    if (!over)
        counter = CounterReset;
    return over;
}

GCMarker::GCMarker(JSRuntime *rt)
  : stack(size_t(-1)),
    color(BLACK),
    started(false),
    unmarkedArenaStackTop(NULL),
    markLaterArenas(0),
    grayFailed(false)
{
    InitTracer(this, rt, NULL);
}

bool
GCMarker::init()
{
    return stack.init(MARK_STACK_LENGTH);
}

void
GCMarker::start()
{
    JS_ASSERT(!started);
    started = true;
    color = BLACK;

    JS_ASSERT(!unmarkedArenaStackTop);
    JS_ASSERT(markLaterArenas == 0);

    /*
     * The GC is recomputing the liveness of WeakMap entries, so we delay
     * visting entries.
     */
    eagerlyTraceWeakMaps = DoNotTraceWeakMaps;
}

void
GCMarker::stop()
{
    JS_ASSERT(isDrained());

    JS_ASSERT(started);
    started = false;

    JS_ASSERT(!unmarkedArenaStackTop);
    JS_ASSERT(markLaterArenas == 0);

    /* Free non-ballast stack memory. */
    stack.reset();

    resetBufferedGrayRoots();
}

void
GCMarker::reset()
{
    color = BLACK;

    stack.reset();
    JS_ASSERT(isMarkStackEmpty());

    while (unmarkedArenaStackTop) {
        ArenaHeader *aheader = unmarkedArenaStackTop;
        JS_ASSERT(aheader->hasDelayedMarking);
        JS_ASSERT(markLaterArenas);
        unmarkedArenaStackTop = aheader->getNextDelayedMarking();
        aheader->unsetDelayedMarking();
        aheader->markOverflow = 0;
        aheader->allocatedDuringIncremental = 0;
        markLaterArenas--;
    }
    JS_ASSERT(isDrained());
    JS_ASSERT(!markLaterArenas);
}

/*
 * When the native stack is low, the GC does not call JS_TraceChildren to mark
 * the reachable "children" of the thing. Rather the thing is put aside and
 * JS_TraceChildren is called later with more space on the C stack.
 *
 * To implement such delayed marking of the children with minimal overhead for
 * the normal case of sufficient native stack, the code adds a field per
 * arena. The field markingDelay->link links all arenas with delayed things
 * into a stack list with the pointer to stack top in
 * GCMarker::unmarkedArenaStackTop. delayMarkingChildren adds
 * arenas to the stack as necessary while markDelayedChildren pops the arenas
 * from the stack until it empties.
 */

inline void
GCMarker::delayMarkingArena(ArenaHeader *aheader)
{
    if (aheader->hasDelayedMarking) {
        /* Arena already scheduled to be marked later */
        return;
    }
    aheader->setNextDelayedMarking(unmarkedArenaStackTop);
    unmarkedArenaStackTop = aheader;
    markLaterArenas++;
}

void
GCMarker::delayMarkingChildren(const void *thing)
{
    const Cell *cell = reinterpret_cast<const Cell *>(thing);
    cell->arenaHeader()->markOverflow = 1;
    delayMarkingArena(cell->arenaHeader());
}

void
GCMarker::markDelayedChildren(ArenaHeader *aheader)
{
    if (aheader->markOverflow) {
        bool always = aheader->allocatedDuringIncremental;
        aheader->markOverflow = 0;

        for (CellIterUnderGC i(aheader); !i.done(); i.next()) {
            Cell *t = i.getCell();
            if (always || t->isMarked()) {
                t->markIfUnmarked();
                JS_TraceChildren(this, t, MapAllocToTraceKind(aheader->getAllocKind()));
            }
        }
    } else {
        JS_ASSERT(aheader->allocatedDuringIncremental);
        PushArena(this, aheader);
    }
    aheader->allocatedDuringIncremental = 0;
    /*
     * Note that during an incremental GC we may still be allocating into
     * aheader. However, prepareForIncrementalGC sets the
     * allocatedDuringIncremental flag if we continue marking.
     */
}

bool
GCMarker::markDelayedChildren(SliceBudget &budget)
{
    gcstats::Phase phase = runtime->gcIncrementalState == MARK
                         ? gcstats::PHASE_MARK_DELAYED
                         : gcstats::PHASE_SWEEP_MARK_DELAYED;
    gcstats::AutoPhase ap(runtime->gcStats, phase);

    JS_ASSERT(unmarkedArenaStackTop);
    do {
        /*
         * If marking gets delayed at the same arena again, we must repeat
         * marking of its things. For that we pop arena from the stack and
         * clear its hasDelayedMarking flag before we begin the marking.
         */
        ArenaHeader *aheader = unmarkedArenaStackTop;
        JS_ASSERT(aheader->hasDelayedMarking);
        JS_ASSERT(markLaterArenas);
        unmarkedArenaStackTop = aheader->getNextDelayedMarking();
        aheader->unsetDelayedMarking();
        markLaterArenas--;
        markDelayedChildren(aheader);

        budget.step(150);
        if (budget.isOverBudget())
            return false;
    } while (unmarkedArenaStackTop);
    JS_ASSERT(!markLaterArenas);

    return true;
}

#ifdef DEBUG
void
GCMarker::checkZone(void *p)
{
    JS_ASSERT(started);
    DebugOnly<Cell *> cell = static_cast<Cell *>(p);
    JS_ASSERT_IF(cell->isTenured(), cell->tenuredZone()->isCollecting());
}
#endif

bool
GCMarker::hasBufferedGrayRoots() const
{
    return !grayFailed;
}

void
GCMarker::startBufferingGrayRoots()
{
    JS_ASSERT(!grayFailed);
    for (GCZonesIter zone(runtime); !zone.done(); zone.next())
        JS_ASSERT(zone->gcGrayRoots.empty());

    JS_ASSERT(!callback);
    callback = GrayCallback;
    JS_ASSERT(IS_GC_MARKING_TRACER(this));
}

void
GCMarker::endBufferingGrayRoots()
{
    JS_ASSERT(callback == GrayCallback);
    callback = NULL;
    JS_ASSERT(IS_GC_MARKING_TRACER(this));
}

void
GCMarker::resetBufferedGrayRoots()
{
    for (GCZonesIter zone(runtime); !zone.done(); zone.next())
        zone->gcGrayRoots.clearAndFree();
    grayFailed = false;
}

void
GCMarker::markBufferedGrayRoots(JS::Zone *zone)
{
    JS_ASSERT(!grayFailed);
    JS_ASSERT(zone->isGCMarkingGray());

    for (GrayRoot *elem = zone->gcGrayRoots.begin(); elem != zone->gcGrayRoots.end(); elem++) {
#ifdef DEBUG
        debugPrinter = elem->debugPrinter;
        debugPrintArg = elem->debugPrintArg;
        debugPrintIndex = elem->debugPrintIndex;
#endif
        void *tmp = elem->thing;
        JS_SET_TRACING_LOCATION(this, (void *)&elem->thing);
        MarkKind(this, &tmp, elem->kind);
        JS_ASSERT(tmp == elem->thing);
    }
}

void
GCMarker::appendGrayRoot(void *thing, JSGCTraceKind kind)
{
    JS_ASSERT(started);

    if (grayFailed)
        return;

    GrayRoot root(thing, kind);
#ifdef DEBUG
    root.debugPrinter = debugPrinter;
    root.debugPrintArg = debugPrintArg;
    root.debugPrintIndex = debugPrintIndex;
#endif

    Zone *zone = static_cast<Cell *>(thing)->tenuredZone();
    if (zone->isCollecting()) {
        zone->maybeAlive = true;
        if (!zone->gcGrayRoots.append(root)) {
            grayFailed = true;
            resetBufferedGrayRoots();
        }
    }
}

void
GCMarker::GrayCallback(JSTracer *trc, void **thingp, JSGCTraceKind kind)
{
    GCMarker *gcmarker = static_cast<GCMarker *>(trc);
    gcmarker->appendGrayRoot(*thingp, kind);
}

size_t
GCMarker::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    size_t size = stack.sizeOfExcludingThis(mallocSizeOf);
    for (ZonesIter zone(runtime); !zone.done(); zone.next())
        size += zone->gcGrayRoots.sizeOfExcludingThis(mallocSizeOf);
    return size;
}

void
js::SetMarkStackLimit(JSRuntime *rt, size_t limit)
{
    JS_ASSERT(!rt->isHeapBusy());
    rt->gcMarker.setSizeLimit(limit);
}

void
js::MarkCompartmentActive(StackFrame *fp)
{
    fp->script()->compartment()->zone()->active = true;
}

static void
TriggerOperationCallback(JSRuntime *rt, JS::gcreason::Reason reason)
{
    if (rt->gcIsNeeded)
        return;

    rt->gcIsNeeded = true;
    rt->gcTriggerReason = reason;
    rt->triggerOperationCallback();
}

void
js::TriggerGC(JSRuntime *rt, JS::gcreason::Reason reason)
{
    /* Wait till end of parallel section to trigger GC. */
    if (InParallelSection()) {
        ForkJoinSlice::Current()->requestGC(reason);
        return;
    }

    JS_ASSERT(CurrentThreadCanAccessRuntime(rt));

    if (rt->isHeapBusy())
        return;

    JS::PrepareForFullGC(rt);
    TriggerOperationCallback(rt, reason);
}

void
js::TriggerZoneGC(Zone *zone, JS::gcreason::Reason reason)
{
    /*
     * If parallel threads are running, wait till they
     * are stopped to trigger GC.
     */
    if (InParallelSection()) {
        ForkJoinSlice::Current()->requestZoneGC(zone, reason);
        return;
    }

    JSRuntime *rt = zone->runtimeFromMainThread();

    if (rt->isHeapBusy())
        return;

    if (rt->gcZeal() == ZealAllocValue) {
        TriggerGC(rt, reason);
        return;
    }

    if (zone == rt->atomsCompartment->zone()) {
        /* We can't do a zone GC of the atoms compartment. */
        TriggerGC(rt, reason);
        return;
    }

    PrepareZoneForGC(zone);
    TriggerOperationCallback(rt, reason);
}

void
js::MaybeGC(JSContext *cx)
{
    JSRuntime *rt = cx->runtime();
    JS_ASSERT(CurrentThreadCanAccessRuntime(rt));

    if (rt->gcZeal() == ZealAllocValue || rt->gcZeal() == ZealPokeValue) {
        JS::PrepareForFullGC(rt);
        GC(rt, GC_NORMAL, JS::gcreason::MAYBEGC);
        return;
    }

    if (rt->gcIsNeeded) {
        GCSlice(rt, GC_NORMAL, JS::gcreason::MAYBEGC);
        return;
    }

    double factor = rt->gcHighFrequencyGC ? 0.85 : 0.9;
    Zone *zone = cx->zone();
    if (zone->gcBytes > 1024 * 1024 &&
        zone->gcBytes >= factor * zone->gcTriggerBytes &&
        rt->gcIncrementalState == NO_INCREMENTAL &&
        !rt->gcHelperThread.sweeping())
    {
        PrepareZoneForGC(zone);
        GCSlice(rt, GC_NORMAL, JS::gcreason::MAYBEGC);
        return;
    }

#ifndef JS_MORE_DETERMINISTIC
    /*
     * Access to the counters and, on 32 bit, setting gcNextFullGCTime below
     * is not atomic and a race condition could trigger or suppress the GC. We
     * tolerate this.
     */
    int64_t now = PRMJ_Now();
    if (rt->gcNextFullGCTime && rt->gcNextFullGCTime <= now) {
        if (rt->gcChunkAllocationSinceLastGC ||
            rt->gcNumArenasFreeCommitted > rt->gcDecommitThreshold)
        {
            JS::PrepareForFullGC(rt);
            GCSlice(rt, GC_SHRINK, JS::gcreason::MAYBEGC);
        } else {
            rt->gcNextFullGCTime = now + GC_IDLE_FULL_SPAN;
        }
    }
#endif
}

static void
DecommitArenasFromAvailableList(JSRuntime *rt, Chunk **availableListHeadp)
{
    Chunk *chunk = *availableListHeadp;
    if (!chunk)
        return;

    /*
     * Decommit is expensive so we avoid holding the GC lock while calling it.
     *
     * We decommit from the tail of the list to minimize interference with the
     * main thread that may start to allocate things at this point.
     *
     * The arena that is been decommitted outside the GC lock must not be
     * available for allocations either via the free list or via the
     * decommittedArenas bitmap. For that we just fetch the arena from the
     * free list before the decommit pretending as it was allocated. If this
     * arena also is the single free arena in the chunk, then we must remove
     * from the available list before we release the lock so the allocation
     * thread would not see chunks with no free arenas on the available list.
     *
     * After we retake the lock, we mark the arena as free and decommitted if
     * the decommit was successful. We must also add the chunk back to the
     * available list if we removed it previously or when the main thread
     * have allocated all remaining free arenas in the chunk.
     *
     * We also must make sure that the aheader is not accessed again after we
     * decommit the arena.
     */
    JS_ASSERT(chunk->info.prevp == availableListHeadp);
    while (Chunk *next = chunk->info.next) {
        JS_ASSERT(next->info.prevp == &chunk->info.next);
        chunk = next;
    }

    for (;;) {
        while (chunk->info.numArenasFreeCommitted != 0) {
            ArenaHeader *aheader = chunk->fetchNextFreeArena(rt);

            Chunk **savedPrevp = chunk->info.prevp;
            if (!chunk->hasAvailableArenas())
                chunk->removeFromAvailableList();

            size_t arenaIndex = Chunk::arenaIndex(aheader->arenaAddress());
            bool ok;
            {
                /*
                 * If the main thread waits for the decommit to finish, skip
                 * potentially expensive unlock/lock pair on the contested
                 * lock.
                 */
                Maybe<AutoUnlockGC> maybeUnlock;
                if (!rt->isHeapBusy())
                    maybeUnlock.construct(rt);
                ok = MarkPagesUnused(rt, aheader->getArena(), ArenaSize);
            }

            if (ok) {
                ++chunk->info.numArenasFree;
                chunk->decommittedArenas.set(arenaIndex);
            } else {
                chunk->addArenaToFreeList(rt, aheader);
            }
            JS_ASSERT(chunk->hasAvailableArenas());
            JS_ASSERT(!chunk->unused());
            if (chunk->info.numArenasFree == 1) {
                /*
                 * Put the chunk back to the available list either at the
                 * point where it was before to preserve the available list
                 * that we enumerate, or, when the allocation thread has fully
                 * used all the previous chunks, at the beginning of the
                 * available list.
                 */
                Chunk **insertPoint = savedPrevp;
                if (savedPrevp != availableListHeadp) {
                    Chunk *prev = Chunk::fromPointerToNext(savedPrevp);
                    if (!prev->hasAvailableArenas())
                        insertPoint = availableListHeadp;
                }
                chunk->insertToAvailableList(insertPoint);
            } else {
                JS_ASSERT(chunk->info.prevp);
            }

            if (rt->gcChunkAllocationSinceLastGC || !ok) {
                /*
                 * The allocator thread has started to get new chunks. We should stop
                 * to avoid decommitting arenas in just allocated chunks.
                 */
                return;
            }
        }

        /*
         * chunk->info.prevp becomes null when the allocator thread consumed
         * all chunks from the available list.
         */
        JS_ASSERT_IF(chunk->info.prevp, *chunk->info.prevp == chunk);
        if (chunk->info.prevp == availableListHeadp || !chunk->info.prevp)
            break;

        /*
         * prevp exists and is not the list head. It must point to the next
         * field of the previous chunk.
         */
        chunk = chunk->getPrevious();
    }
}

static void
DecommitArenas(JSRuntime *rt)
{
    DecommitArenasFromAvailableList(rt, &rt->gcSystemAvailableChunkListHead);
    DecommitArenasFromAvailableList(rt, &rt->gcUserAvailableChunkListHead);
}

/* Must be called with the GC lock taken. */
static void
ExpireChunksAndArenas(JSRuntime *rt, bool shouldShrink)
{
    if (Chunk *toFree = rt->gcChunkPool.expire(rt, shouldShrink)) {
        AutoUnlockGC unlock(rt);
        FreeChunkList(rt, toFree);
    }

    if (shouldShrink)
        DecommitArenas(rt);
}

static void
SweepBackgroundThings(JSRuntime* rt, bool onBackgroundThread)
{
    /*
     * We must finalize in the correct order, see comments in
     * finalizeObjects.
     */
    FreeOp fop(rt, false);
    for (int phase = 0 ; phase < BackgroundPhaseCount ; ++phase) {
        for (Zone *zone = rt->gcSweepingZones; zone; zone = zone->gcNextGraphNode) {
            for (int index = 0 ; index < BackgroundPhaseLength[phase] ; ++index) {
                AllocKind kind = BackgroundPhases[phase][index];
                ArenaHeader *arenas = zone->allocator.arenas.arenaListsToSweep[kind];
                if (arenas)
                    ArenaLists::backgroundFinalize(&fop, arenas, onBackgroundThread);
            }
        }
    }

    rt->gcSweepingZones = NULL;
}

#ifdef JS_THREADSAFE
static void
AssertBackgroundSweepingFinished(JSRuntime *rt)
{
    JS_ASSERT(!rt->gcSweepingZones);
    for (ZonesIter zone(rt); !zone.done(); zone.next()) {
        for (unsigned i = 0; i < FINALIZE_LIMIT; ++i) {
            JS_ASSERT(!zone->allocator.arenas.arenaListsToSweep[i]);
            JS_ASSERT(zone->allocator.arenas.doneBackgroundFinalize(AllocKind(i)));
        }
    }
}

unsigned
js::GetCPUCount()
{
    static unsigned ncpus = 0;
    if (ncpus == 0) {
# ifdef XP_WIN
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        ncpus = unsigned(sysinfo.dwNumberOfProcessors);
# else
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        ncpus = (n > 0) ? unsigned(n) : 1;
# endif
    }
    return ncpus;
}
#endif /* JS_THREADSAFE */

bool
GCHelperThread::init()
{
    if (!rt->useHelperThreads()) {
        backgroundAllocation = false;
        return true;
    }

#ifdef JS_THREADSAFE
    if (!(wakeup = PR_NewCondVar(rt->gcLock)))
        return false;
    if (!(done = PR_NewCondVar(rt->gcLock)))
        return false;

    thread = PR_CreateThread(PR_USER_THREAD, threadMain, this, PR_PRIORITY_NORMAL,
                             PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (!thread)
        return false;

    backgroundAllocation = (GetCPUCount() >= 2);
#endif /* JS_THREADSAFE */
    return true;
}

void
GCHelperThread::finish()
{
    if (!rt->useHelperThreads()) {
        JS_ASSERT(state == IDLE);
        return;
    }


#ifdef JS_THREADSAFE
    PRThread *join = NULL;
    {
        AutoLockGC lock(rt);
        if (thread && state != SHUTDOWN) {
            /*
             * We cannot be in the ALLOCATING or CANCEL_ALLOCATION states as
             * the allocations should have been stopped during the last GC.
             */
            JS_ASSERT(state == IDLE || state == SWEEPING);
            if (state == IDLE)
                PR_NotifyCondVar(wakeup);
            state = SHUTDOWN;
            join = thread;
        }
    }
    if (join) {
        /* PR_DestroyThread is not necessary. */
        PR_JoinThread(join);
    }
    if (wakeup)
        PR_DestroyCondVar(wakeup);
    if (done)
        PR_DestroyCondVar(done);
#endif /* JS_THREADSAFE */
}

#ifdef JS_THREADSAFE
/* static */
void
GCHelperThread::threadMain(void *arg)
{
    PR_SetCurrentThreadName("JS GC Helper");
    static_cast<GCHelperThread *>(arg)->threadLoop();
}

void
GCHelperThread::threadLoop()
{
    AutoLockGC lock(rt);

    /*
     * Even on the first iteration the state can be SHUTDOWN or SWEEPING if
     * the stop request or the GC and the corresponding startBackgroundSweep call
     * happen before this thread has a chance to run.
     */
    for (;;) {
        switch (state) {
          case SHUTDOWN:
            return;
          case IDLE:
            PR_WaitCondVar(wakeup, PR_INTERVAL_NO_TIMEOUT);
            break;
          case SWEEPING:
            doSweep();
            if (state == SWEEPING)
                state = IDLE;
            PR_NotifyAllCondVar(done);
            break;
          case ALLOCATING:
            do {
                Chunk *chunk;
                {
                    AutoUnlockGC unlock(rt);
                    chunk = Chunk::allocate(rt);
                }

                /* OOM stops the background allocation. */
                if (!chunk)
                    break;
                JS_ASSERT(chunk->info.numArenasFreeCommitted == ArenasPerChunk);
                rt->gcNumArenasFreeCommitted += ArenasPerChunk;
                rt->gcChunkPool.put(chunk);
            } while (state == ALLOCATING && rt->gcChunkPool.wantBackgroundAllocation(rt));
            if (state == ALLOCATING)
                state = IDLE;
            break;
          case CANCEL_ALLOCATION:
            state = IDLE;
            PR_NotifyAllCondVar(done);
            break;
        }
    }
}
#endif /* JS_THREADSAFE */

void
GCHelperThread::startBackgroundSweep(bool shouldShrink)
{
    JS_ASSERT(rt->useHelperThreads());

#ifdef JS_THREADSAFE
    AutoLockGC lock(rt);
    JS_ASSERT(state == IDLE);
    JS_ASSERT(!sweepFlag);
    sweepFlag = true;
    shrinkFlag = shouldShrink;
    state = SWEEPING;
    PR_NotifyCondVar(wakeup);
#endif /* JS_THREADSAFE */
}

/* Must be called with the GC lock taken. */
void
GCHelperThread::startBackgroundShrink()
{
    JS_ASSERT(rt->useHelperThreads());

#ifdef JS_THREADSAFE
    switch (state) {
      case IDLE:
        JS_ASSERT(!sweepFlag);
        shrinkFlag = true;
        state = SWEEPING;
        PR_NotifyCondVar(wakeup);
        break;
      case SWEEPING:
        shrinkFlag = true;
        break;
      case ALLOCATING:
      case CANCEL_ALLOCATION:
        /*
         * If we have started background allocation there is nothing to
         * shrink.
         */
        break;
      case SHUTDOWN:
        MOZ_ASSUME_UNREACHABLE("No shrink on shutdown");
    }
#endif /* JS_THREADSAFE */
}

void
GCHelperThread::waitBackgroundSweepEnd()
{
    if (!rt->useHelperThreads()) {
        JS_ASSERT(state == IDLE);
        return;
    }

#ifdef JS_THREADSAFE
    AutoLockGC lock(rt);
    while (state == SWEEPING)
        PR_WaitCondVar(done, PR_INTERVAL_NO_TIMEOUT);
    if (rt->gcIncrementalState == NO_INCREMENTAL)
        AssertBackgroundSweepingFinished(rt);
#endif /* JS_THREADSAFE */
}

void
GCHelperThread::waitBackgroundSweepOrAllocEnd()
{
    if (!rt->useHelperThreads()) {
        JS_ASSERT(state == IDLE);
        return;
    }

#ifdef JS_THREADSAFE
    AutoLockGC lock(rt);
    if (state == ALLOCATING)
        state = CANCEL_ALLOCATION;
    while (state == SWEEPING || state == CANCEL_ALLOCATION)
        PR_WaitCondVar(done, PR_INTERVAL_NO_TIMEOUT);
    if (rt->gcIncrementalState == NO_INCREMENTAL)
        AssertBackgroundSweepingFinished(rt);
#endif /* JS_THREADSAFE */
}

/* Must be called with the GC lock taken. */
inline void
GCHelperThread::startBackgroundAllocationIfIdle()
{
    JS_ASSERT(rt->useHelperThreads());

#ifdef JS_THREADSAFE
    if (state == IDLE) {
        state = ALLOCATING;
        PR_NotifyCondVar(wakeup);
    }
#endif /* JS_THREADSAFE */
}

void
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

#ifdef JS_THREADSAFE
/* Must be called with the GC lock taken. */
void
GCHelperThread::doSweep()
{
    if (sweepFlag) {
        sweepFlag = false;
        AutoUnlockGC unlock(rt);

        SweepBackgroundThings(rt, true);

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

        rt->freeLifoAlloc.freeAll();
    }

    bool shrinking = shrinkFlag;
    ExpireChunksAndArenas(rt, shrinking);

    /*
     * The main thread may have called ShrinkGCBuffers while
     * ExpireChunksAndArenas(rt, false) was running, so we recheck the flag
     * afterwards.
     */
    if (!shrinking && shrinkFlag) {
        shrinkFlag = false;
        ExpireChunksAndArenas(rt, true);
    }
}
#endif /* JS_THREADSAFE */

bool
GCHelperThread::onBackgroundThread()
{
#ifdef JS_THREADSAFE
    return PR_GetCurrentThread() == getThread();
#else
    return false;
#endif
}

static bool
ReleaseObservedTypes(JSRuntime *rt)
{
    bool releaseTypes = rt->gcZeal() != 0;

#ifndef JS_MORE_DETERMINISTIC
    int64_t now = PRMJ_Now();
    if (now >= rt->gcJitReleaseTime)
        releaseTypes = true;
    if (releaseTypes)
        rt->gcJitReleaseTime = now + JIT_SCRIPT_RELEASE_TYPES_INTERVAL;
#endif

    return releaseTypes;
}

/*
 * It's simpler if we preserve the invariant that every zone has at least one
 * compartment. If we know we're deleting the entire zone, then
 * SweepCompartments is allowed to delete all compartments. In this case,
 * |keepAtleastOne| is false. If some objects remain in the zone so that it
 * cannot be deleted, then we set |keepAtleastOne| to true, which prohibits
 * SweepCompartments from deleting every compartment. Instead, it preserves an
 * arbitrary compartment in the zone.
 */
static void
SweepCompartments(FreeOp *fop, Zone *zone, bool keepAtleastOne, bool lastGC)
{
    JSRuntime *rt = zone->runtimeFromMainThread();
    JSDestroyCompartmentCallback callback = rt->destroyCompartmentCallback;

    JSCompartment **read = zone->compartments.begin();
    JSCompartment **end = zone->compartments.end();
    JSCompartment **write = read;
    bool foundOne = false;
    while (read < end) {
        JSCompartment *comp = *read++;
        JS_ASSERT(comp != rt->atomsCompartment);

        /*
         * Don't delete the last compartment if all the ones before it were
         * deleted and keepAtleastOne is true.
         */
        bool dontDelete = read == end && !foundOne && keepAtleastOne;
        if ((!comp->marked && !dontDelete) || lastGC) {
            if (callback)
                callback(fop, comp);
            if (comp->principals)
                JS_DropPrincipals(rt, comp->principals);
            js_delete(comp);
        } else {
            *write++ = comp;
            foundOne = true;
        }
    }
    zone->compartments.resize(write - zone->compartments.begin());
    JS_ASSERT_IF(keepAtleastOne, !zone->compartments.empty());
}

static void
SweepZones(FreeOp *fop, bool lastGC)
{
    JSRuntime *rt = fop->runtime();
    JS_ASSERT_IF(lastGC, !rt->hasContexts());

    /* Skip the atomsCompartment zone. */
    Zone **read = rt->zones.begin() + 1;
    Zone **end = rt->zones.end();
    Zone **write = read;
    JS_ASSERT(rt->zones.length() >= 1);
    JS_ASSERT(rt->zones[0] == rt->atomsCompartment->zone());

    while (read < end) {
        Zone *zone = *read++;

        if (!zone->hold && zone->wasGCStarted()) {
            if (zone->allocator.arenas.arenaListsAreEmpty() || lastGC) {
                zone->allocator.arenas.checkEmptyFreeLists();
                SweepCompartments(fop, zone, false, lastGC);
                JS_ASSERT(zone->compartments.empty());
                fop->delete_(zone);
                continue;
            }
            SweepCompartments(fop, zone, true, lastGC);
        }
        *write++ = zone;
    }
    rt->zones.resize(write - rt->zones.begin());
}

static void
PurgeRuntime(JSRuntime *rt)
{
    for (GCCompartmentsIter comp(rt); !comp.done(); comp.next())
        comp->purge();

    rt->freeLifoAlloc.transferUnusedFrom(&rt->tempLifoAlloc);
    rt->interpreterStack().purge(rt);

    rt->gsnCache.purge();
    rt->newObjectCache.purge();
    rt->nativeIterCache.purge();
    rt->sourceDataCache.purge();
    rt->evalCache.clear();

    bool activeCompilations = false;
    for (ThreadDataIter iter(rt); !iter.done(); iter.next())
        activeCompilations |= iter->activeCompilations;
    if (!activeCompilations)
        rt->parseMapPool.purgeAll();
}

static bool
ShouldPreserveJITCode(JSCompartment *comp, int64_t currentTime)
{
    JSRuntime *rt = comp->runtimeFromMainThread();
    if (rt->gcShouldCleanUpEverything || !comp->zone()->types.inferenceEnabled)
        return false;

    if (rt->alwaysPreserveCode)
        return true;
    if (comp->lastAnimationTime + PRMJ_USEC_PER_SEC >= currentTime &&
        comp->lastCodeRelease + (PRMJ_USEC_PER_SEC * 300) >= currentTime)
    {
        return true;
    }

    comp->lastCodeRelease = currentTime;
    return false;
}

#ifdef DEBUG
struct CompartmentCheckTracer : public JSTracer
{
    Cell *src;
    JSGCTraceKind srcKind;
    Zone *zone;
    JSCompartment *compartment;
};

static bool
InCrossCompartmentMap(JSObject *src, Cell *dst, JSGCTraceKind dstKind)
{
    JSCompartment *srccomp = src->compartment();

    if (dstKind == JSTRACE_OBJECT) {
        Value key = ObjectValue(*static_cast<JSObject *>(dst));
        if (WrapperMap::Ptr p = srccomp->lookupWrapper(key)) {
            if (*p->value.unsafeGet() == ObjectValue(*src))
                return true;
        }
    }

    /*
     * If the cross-compartment edge is caused by the debugger, then we don't
     * know the right hashtable key, so we have to iterate.
     */
    for (JSCompartment::WrapperEnum e(srccomp); !e.empty(); e.popFront()) {
        if (e.front().key.wrapped == dst && ToMarkable(e.front().value) == src)
            return true;
    }

    return false;
}

static void
CheckCompartment(CompartmentCheckTracer *trc, JSCompartment *thingCompartment,
                 Cell *thing, JSGCTraceKind kind)
{
    JS_ASSERT(thingCompartment == trc->compartment ||
              thingCompartment == trc->runtime->atomsCompartment ||
              (trc->srcKind == JSTRACE_OBJECT &&
               InCrossCompartmentMap((JSObject *)trc->src, thing, kind)));
}

static JSCompartment *
CompartmentOfCell(Cell *thing, JSGCTraceKind kind)
{
    if (kind == JSTRACE_OBJECT)
        return static_cast<JSObject *>(thing)->compartment();
    else if (kind == JSTRACE_SHAPE)
        return static_cast<Shape *>(thing)->compartment();
    else if (kind == JSTRACE_BASE_SHAPE)
        return static_cast<BaseShape *>(thing)->compartment();
    else if (kind == JSTRACE_SCRIPT)
        return static_cast<JSScript *>(thing)->compartment();
    else
        return NULL;
}

static void
CheckCompartmentCallback(JSTracer *trcArg, void **thingp, JSGCTraceKind kind)
{
    CompartmentCheckTracer *trc = static_cast<CompartmentCheckTracer *>(trcArg);
    Cell *thing = (Cell *)*thingp;

    JSCompartment *comp = CompartmentOfCell(thing, kind);
    if (comp && trc->compartment) {
        CheckCompartment(trc, comp, thing, kind);
    } else {
        JS_ASSERT(thing->tenuredZone() == trc->zone ||
                  thing->tenuredZone() == trc->runtime->atomsCompartment->zone());
    }
}

static void
CheckForCompartmentMismatches(JSRuntime *rt)
{
    if (rt->gcDisableStrictProxyCheckingCount)
        return;

    CompartmentCheckTracer trc;
    JS_TracerInit(&trc, rt, CheckCompartmentCallback);

    for (ZonesIter zone(rt); !zone.done(); zone.next()) {
        trc.zone = zone;
        for (size_t thingKind = 0; thingKind < FINALIZE_LAST; thingKind++) {
            for (CellIterUnderGC i(zone, AllocKind(thingKind)); !i.done(); i.next()) {
                trc.src = i.getCell();
                trc.srcKind = MapAllocToTraceKind(AllocKind(thingKind));
                trc.compartment = CompartmentOfCell(trc.src, trc.srcKind);
                JS_TraceChildren(&trc, trc.src, trc.srcKind);
            }
        }
    }
}
#endif

static bool
BeginMarkPhase(JSRuntime *rt)
{
    int64_t currentTime = PRMJ_Now();

#ifdef DEBUG
    if (rt->gcFullCompartmentChecks)
        CheckForCompartmentMismatches(rt);
#endif

    rt->gcIsFull = true;
    bool any = false;

    for (ZonesIter zone(rt); !zone.done(); zone.next()) {
        /* Assert that zone state is as we expect */
        JS_ASSERT(!zone->isCollecting());
        JS_ASSERT(!zone->compartments.empty());
        for (unsigned i = 0; i < FINALIZE_LIMIT; ++i)
            JS_ASSERT(!zone->allocator.arenas.arenaListsToSweep[i]);

        /* Set up which zones will be collected. */
        if (zone->isGCScheduled()) {
            if (zone != rt->atomsCompartment->zone()) {
                any = true;
                zone->setGCState(Zone::Mark);
            }
        } else {
            rt->gcIsFull = false;
        }

        zone->scheduledForDestruction = false;
        zone->maybeAlive = zone->hold;
        zone->setPreservingCode(false);
    }

    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        JS_ASSERT(!c->gcLiveArrayBuffers);
        c->marked = false;
        if (ShouldPreserveJITCode(c, currentTime))
            c->zone()->setPreservingCode(true);
    }

    /* Check that at least one zone is scheduled for collection. */
    if (!any)
        return false;

    /*
     * Atoms are not in the cross-compartment map. So if there are any
     * zones that are not being collected, we are not allowed to collect
     * atoms. Otherwise, the non-collected zones could contain pointers
     * to atoms that we would miss.
     */
    Zone *atomsZone = rt->atomsCompartment->zone();

    bool keepAtoms = false;
    for (ThreadDataIter iter(rt); !iter.done(); iter.next())
        keepAtoms |= iter->gcKeepAtoms;

    /*
     * We don't scan the stacks of exclusive threads, so we need to avoid
     * collecting their objects in another way. The only GC thing pointers they
     * have are to their exclusive compartment (which is not collected) or to
     * the atoms compartment. Therefore, we avoid collecting the atoms
     * compartment when exclusive threads are running.
     */
    keepAtoms |= rt->exclusiveThreadsPresent();

    if (atomsZone->isGCScheduled() && rt->gcIsFull && !keepAtoms) {
        JS_ASSERT(!atomsZone->isCollecting());
        atomsZone->setGCState(Zone::Mark);
    }

    /*
     * At the end of each incremental slice, we call prepareForIncrementalGC,
     * which marks objects in all arenas that we're currently allocating
     * into. This can cause leaks if unreachable objects are in these
     * arenas. This purge call ensures that we only mark arenas that have had
     * allocations after the incremental GC started.
     */
    if (rt->gcIsIncremental) {
        for (GCZonesIter zone(rt); !zone.done(); zone.next())
            zone->allocator.arenas.purge();
    }

    rt->gcMarker.start();
    JS_ASSERT(!rt->gcMarker.callback);
    JS_ASSERT(IS_GC_MARKING_TRACER(&rt->gcMarker));

    /* For non-incremental GC the following sweep discards the jit code. */
    if (rt->gcIsIncremental) {
        for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
            gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_MARK_DISCARD_CODE);
            zone->discardJitCode(rt->defaultFreeOp(), false);
        }
    }

    GCMarker *gcmarker = &rt->gcMarker;

    rt->gcStartNumber = rt->gcNumber;

    /*
     * We must purge the runtime at the beginning of an incremental GC. The
     * danger if we purge later is that the snapshot invariant of incremental
     * GC will be broken, as follows. If some object is reachable only through
     * some cache (say the dtoaCache) then it will not be part of the snapshot.
     * If we purge after root marking, then the mutator could obtain a pointer
     * to the object and start using it. This object might never be marked, so
     * a GC hazard would exist.
     */
    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_PURGE);
        PurgeRuntime(rt);
    }

    /*
     * Mark phase.
     */
    gcstats::AutoPhase ap1(rt->gcStats, gcstats::PHASE_MARK);
    gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_MARK_ROOTS);

    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        /* Unmark everything in the zones being collected. */
        zone->allocator.arenas.unmarkAll();
    }

    for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
        /* Reset weak map list for the compartments being collected. */
        WeakMapBase::resetCompartmentWeakMapList(c);
    }

    MarkRuntime(gcmarker);
    BufferGrayRoots(gcmarker);

    /*
     * This code ensures that if a zone is "dead", then it will be
     * collected in this GC. A zone is considered dead if its maybeAlive
     * flag is false. The maybeAlive flag is set if:
     *   (1) the zone has incoming cross-compartment edges, or
     *   (2) an object in the zone was marked during root marking, either
     *       as a black root or a gray root.
     * If the maybeAlive is false, then we set the scheduledForDestruction flag.
     * At any time later in the GC, if we try to mark an object whose
     * zone is scheduled for destruction, we will assert.
     * NOTE: Due to bug 811587, we only assert if gcManipulatingDeadCompartments
     * is true (e.g., if we're doing a brain transplant).
     *
     * The purpose of this check is to ensure that a zone that we would
     * normally destroy is not resurrected by a read barrier or an
     * allocation. This might happen during a function like JS_TransplantObject,
     * which iterates over all compartments, live or dead, and operates on their
     * objects. See bug 803376 for details on this problem. To avoid the
     * problem, we are very careful to avoid allocation and read barriers during
     * JS_TransplantObject and the like. The code here ensures that we don't
     * regress.
     *
     * Note that there are certain cases where allocations or read barriers in
     * dead zone are difficult to avoid. We detect such cases (via the
     * gcObjectsMarkedInDeadCompartment counter) and redo any ongoing GCs after
     * the JS_TransplantObject function has finished. This ensures that the dead
     * zones will be cleaned up. See AutoMarkInDeadZone and
     * AutoMaybeTouchDeadZones for details.
     */

    /* Set the maybeAlive flag based on cross-compartment edges. */
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        for (JSCompartment::WrapperEnum e(c); !e.empty(); e.popFront()) {
            Cell *dst = e.front().key.wrapped;
            dst->tenuredZone()->maybeAlive = true;
        }
    }

    /*
     * For black roots, code in gc/Marking.cpp will already have set maybeAlive
     * during MarkRuntime.
     */

    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        if (!zone->maybeAlive)
            zone->scheduledForDestruction = true;
    }
    rt->gcFoundBlackGrayEdges = false;

    return true;
}

template <class CompartmentIterT>
static void
MarkWeakReferences(JSRuntime *rt, gcstats::Phase phase)
{
    GCMarker *gcmarker = &rt->gcMarker;
    JS_ASSERT(gcmarker->isDrained());

    gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_MARK);
    gcstats::AutoPhase ap1(rt->gcStats, phase);

    for (;;) {
        bool markedAny = false;
        for (CompartmentIterT c(rt); !c.done(); c.next()) {
            markedAny |= WatchpointMap::markCompartmentIteratively(c, gcmarker);
            markedAny |= WeakMapBase::markCompartmentIteratively(c, gcmarker);
        }
        markedAny |= Debugger::markAllIteratively(gcmarker);

        if (!markedAny)
            break;

        SliceBudget budget;
        gcmarker->drainMarkStack(budget);
    }
    JS_ASSERT(gcmarker->isDrained());
}

static void
MarkWeakReferencesInCurrentGroup(JSRuntime *rt, gcstats::Phase phase)
{
    MarkWeakReferences<GCCompartmentGroupIter>(rt, phase);
}

template <class ZoneIterT, class CompartmentIterT>
static void
MarkGrayReferences(JSRuntime *rt)
{
    GCMarker *gcmarker = &rt->gcMarker;

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_MARK);
        gcstats::AutoPhase ap1(rt->gcStats, gcstats::PHASE_SWEEP_MARK_GRAY);
        gcmarker->setMarkColorGray();
        if (gcmarker->hasBufferedGrayRoots()) {
            for (ZoneIterT zone(rt); !zone.done(); zone.next())
                gcmarker->markBufferedGrayRoots(zone);
        } else {
            JS_ASSERT(!rt->gcIsIncremental);
            if (JSTraceDataOp op = rt->gcGrayRootTracer.op)
                (*op)(gcmarker, rt->gcGrayRootTracer.data);
        }
        SliceBudget budget;
        gcmarker->drainMarkStack(budget);
    }

    MarkWeakReferences<CompartmentIterT>(rt, gcstats::PHASE_SWEEP_MARK_GRAY_WEAK);

    JS_ASSERT(gcmarker->isDrained());

    gcmarker->setMarkColorBlack();
}

static void
MarkGrayReferencesInCurrentGroup(JSRuntime *rt)
{
    MarkGrayReferences<GCZoneGroupIter, GCCompartmentGroupIter>(rt);
}

#ifdef DEBUG

static void
MarkAllWeakReferences(JSRuntime *rt, gcstats::Phase phase)
{
    MarkWeakReferences<GCCompartmentsIter>(rt, phase);
}

static void
MarkAllGrayReferences(JSRuntime *rt)
{
    MarkGrayReferences<GCZonesIter, GCCompartmentsIter>(rt);
}

class js::gc::MarkingValidator
{
  public:
    MarkingValidator(JSRuntime *rt);
    ~MarkingValidator();
    void nonIncrementalMark();
    void validate();

  private:
    JSRuntime *runtime;
    bool initialized;

    typedef HashMap<Chunk *, ChunkBitmap *, GCChunkHasher, SystemAllocPolicy> BitmapMap;
    BitmapMap map;
};

js::gc::MarkingValidator::MarkingValidator(JSRuntime *rt)
  : runtime(rt),
    initialized(false)
{}

js::gc::MarkingValidator::~MarkingValidator()
{
    if (!map.initialized())
        return;

    for (BitmapMap::Range r(map.all()); !r.empty(); r.popFront())
        js_delete(r.front().value);
}

void
js::gc::MarkingValidator::nonIncrementalMark()
{
    /*
     * Perform a non-incremental mark for all collecting zones and record
     * the results for later comparison.
     *
     * Currently this does not validate gray marking.
     */

    if (!map.init())
        return;

    GCMarker *gcmarker = &runtime->gcMarker;

    /* Save existing mark bits. */
    for (GCChunkSet::Range r(runtime->gcChunkSet.all()); !r.empty(); r.popFront()) {
        ChunkBitmap *bitmap = &r.front()->bitmap;
	ChunkBitmap *entry = js_new<ChunkBitmap>();
        if (!entry)
            return;

        memcpy((void *)entry->bitmap, (void *)bitmap->bitmap, sizeof(bitmap->bitmap));
        if (!map.putNew(r.front(), entry))
            return;
    }

    /*
     * Save the lists of live weakmaps and array buffers for the compartments we
     * are collecting.
     */
    WeakMapVector weakmaps;
    ArrayBufferVector arrayBuffers;
    for (GCCompartmentsIter c(runtime); !c.done(); c.next()) {
        if (!WeakMapBase::saveCompartmentWeakMapList(c, weakmaps) ||
            !ArrayBufferObject::saveArrayBufferList(c, arrayBuffers))
        {
            return;
        }
    }

    /*
     * After this point, the function should run to completion, so we shouldn't
     * do anything fallible.
     */
    initialized = true;

    /*
     * Reset the lists of live weakmaps and array buffers for the compartments we
     * are collecting.
     */
    for (GCCompartmentsIter c(runtime); !c.done(); c.next()) {
        WeakMapBase::resetCompartmentWeakMapList(c);
        ArrayBufferObject::resetArrayBufferList(c);
    }

    /* Re-do all the marking, but non-incrementally. */
    js::gc::State state = runtime->gcIncrementalState;
    runtime->gcIncrementalState = MARK_ROOTS;

    JS_ASSERT(gcmarker->isDrained());
    gcmarker->reset();

    for (GCChunkSet::Range r(runtime->gcChunkSet.all()); !r.empty(); r.popFront())
        r.front()->bitmap.clear();

    {
        gcstats::AutoPhase ap1(runtime->gcStats, gcstats::PHASE_MARK);
        gcstats::AutoPhase ap2(runtime->gcStats, gcstats::PHASE_MARK_ROOTS);
        MarkRuntime(gcmarker, true);
    }

    SliceBudget budget;
    runtime->gcIncrementalState = MARK;
    runtime->gcMarker.drainMarkStack(budget);

    {
        gcstats::AutoPhase ap(runtime->gcStats, gcstats::PHASE_SWEEP);
        MarkAllWeakReferences(runtime, gcstats::PHASE_SWEEP_MARK_WEAK);

        /* Update zone state for gray marking. */
        for (GCZonesIter zone(runtime); !zone.done(); zone.next()) {
            JS_ASSERT(zone->isGCMarkingBlack());
            zone->setGCState(Zone::MarkGray);
        }

        MarkAllGrayReferences(runtime);

        /* Restore zone state. */
        for (GCZonesIter zone(runtime); !zone.done(); zone.next()) {
            JS_ASSERT(zone->isGCMarkingGray());
            zone->setGCState(Zone::Mark);
        }
    }

    /* Take a copy of the non-incremental mark state and restore the original. */
    for (GCChunkSet::Range r(runtime->gcChunkSet.all()); !r.empty(); r.popFront()) {
        Chunk *chunk = r.front();
        ChunkBitmap *bitmap = &chunk->bitmap;
        ChunkBitmap *entry = map.lookup(chunk)->value;
        Swap(*entry, *bitmap);
    }

    /* Restore the weak map and array buffer lists. */
    for (GCCompartmentsIter c(runtime); !c.done(); c.next()) {
        WeakMapBase::resetCompartmentWeakMapList(c);
        ArrayBufferObject::resetArrayBufferList(c);
    }
    WeakMapBase::restoreCompartmentWeakMapLists(weakmaps);
    ArrayBufferObject::restoreArrayBufferLists(arrayBuffers);

    runtime->gcIncrementalState = state;
}

void
js::gc::MarkingValidator::validate()
{
    /*
     * Validates the incremental marking for a single compartment by comparing
     * the mark bits to those previously recorded for a non-incremental mark.
     */

    if (!initialized)
        return;

    for (GCChunkSet::Range r(runtime->gcChunkSet.all()); !r.empty(); r.popFront()) {
        Chunk *chunk = r.front();
        BitmapMap::Ptr ptr = map.lookup(chunk);
        if (!ptr)
            continue;  /* Allocated after we did the non-incremental mark. */

        ChunkBitmap *bitmap = ptr->value;
        ChunkBitmap *incBitmap = &chunk->bitmap;

        for (size_t i = 0; i < ArenasPerChunk; i++) {
            if (chunk->decommittedArenas.get(i))
                continue;
            Arena *arena = &chunk->arenas[i];
            if (!arena->aheader.allocated())
                continue;
            if (!arena->aheader.zone->isGCSweeping())
                continue;
            if (arena->aheader.allocatedDuringIncremental)
                continue;

            AllocKind kind = arena->aheader.getAllocKind();
            uintptr_t thing = arena->thingsStart(kind);
            uintptr_t end = arena->thingsEnd();
            while (thing < end) {
                Cell *cell = (Cell *)thing;

                /*
                 * If a non-incremental GC wouldn't have collected a cell, then
                 * an incremental GC won't collect it.
                 */
                JS_ASSERT_IF(bitmap->isMarked(cell, BLACK), incBitmap->isMarked(cell, BLACK));

                /*
                 * If the cycle collector isn't allowed to collect an object
                 * after a non-incremental GC has run, then it isn't allowed to
                 * collected it after an incremental GC.
                 */
                JS_ASSERT_IF(!bitmap->isMarked(cell, GRAY), !incBitmap->isMarked(cell, GRAY));

                thing += Arena::thingSize(kind);
            }
        }
    }
}

#endif

static void
ComputeNonIncrementalMarkingForValidation(JSRuntime *rt)
{
#ifdef DEBUG
    JS_ASSERT(!rt->gcMarkingValidator);
    if (rt->gcIsIncremental && rt->gcValidate)
        rt->gcMarkingValidator = js_new<MarkingValidator>(rt);
    if (rt->gcMarkingValidator)
        rt->gcMarkingValidator->nonIncrementalMark();
#endif
}

static void
ValidateIncrementalMarking(JSRuntime *rt)
{
#ifdef DEBUG
    if (rt->gcMarkingValidator)
        rt->gcMarkingValidator->validate();
#endif
}

static void
FinishMarkingValidation(JSRuntime *rt)
{
#ifdef DEBUG
    js_delete(rt->gcMarkingValidator);
    rt->gcMarkingValidator = NULL;
#endif
}

static void
AssertNeedsBarrierFlagsConsistent(JSRuntime *rt)
{
#ifdef DEBUG
    bool anyNeedsBarrier = false;
    for (ZonesIter zone(rt); !zone.done(); zone.next())
        anyNeedsBarrier |= zone->needsBarrier();
    JS_ASSERT(rt->needsBarrier() == anyNeedsBarrier);
#endif
}

static void
DropStringWrappers(JSRuntime *rt)
{
    /*
     * String "wrappers" are dropped on GC because their presence would require
     * us to sweep the wrappers in all compartments every time we sweep a
     * compartment group.
     */
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        for (JSCompartment::WrapperEnum e(c); !e.empty(); e.popFront()) {
            if (e.front().key.kind == CrossCompartmentKey::StringWrapper)
                e.removeFront();
        }
    }
}

/*
 * Group zones that must be swept at the same time.
 *
 * If compartment A has an edge to an unmarked object in compartment B, then we
 * must not sweep A in a later slice than we sweep B. That's because a write
 * barrier in A that could lead to the unmarked object in B becoming
 * marked. However, if we had already swept that object, we would be in trouble.
 *
 * If we consider these dependencies as a graph, then all the compartments in
 * any strongly-connected component of this graph must be swept in the same
 * slice.
 *
 * Tarjan's algorithm is used to calculate the components.
 */

void
JSCompartment::findOutgoingEdges(ComponentFinder<JS::Zone> &finder)
{
    for (js::WrapperMap::Enum e(crossCompartmentWrappers); !e.empty(); e.popFront()) {
        CrossCompartmentKey::Kind kind = e.front().key.kind;
        JS_ASSERT(kind != CrossCompartmentKey::StringWrapper);
        Cell *other = e.front().key.wrapped;
        if (kind == CrossCompartmentKey::ObjectWrapper) {
            /*
             * Add edge to wrapped object compartment if wrapped object is not
             * marked black to indicate that wrapper compartment not be swept
             * after wrapped compartment.
             */
            if (!other->isMarked(BLACK) || other->isMarked(GRAY)) {
                JS::Zone *w = other->tenuredZone();
                if (w->isGCMarking())
                    finder.addEdgeTo(w);
            }
        } else {
            JS_ASSERT(kind == CrossCompartmentKey::DebuggerScript ||
                      kind == CrossCompartmentKey::DebuggerSource ||
                      kind == CrossCompartmentKey::DebuggerObject ||
                      kind == CrossCompartmentKey::DebuggerEnvironment);
            /*
             * Add edge for debugger object wrappers, to ensure (in conjuction
             * with call to Debugger::findCompartmentEdges below) that debugger
             * and debuggee objects are always swept in the same group.
             */
            JS::Zone *w = other->tenuredZone();
            if (w->isGCMarking())
                finder.addEdgeTo(w);
        }
    }

    Debugger::findCompartmentEdges(zone(), finder);
}

void
Zone::findOutgoingEdges(ComponentFinder<JS::Zone> &finder)
{
    /*
     * Any compartment may have a pointer to an atom in the atoms
     * compartment, and these aren't in the cross compartment map.
     */
    JSRuntime *rt = runtimeFromMainThread();
    if (rt->atomsCompartment->zone()->isGCMarking())
        finder.addEdgeTo(rt->atomsCompartment->zone());

    for (CompartmentsInZoneIter comp(this); !comp.done(); comp.next())
        comp->findOutgoingEdges(finder);
}

static void
FindZoneGroups(JSRuntime *rt)
{
    ComponentFinder<Zone> finder(rt->mainThread.nativeStackLimit);
    if (!rt->gcIsIncremental)
        finder.useOneComponent();

    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        JS_ASSERT(zone->isGCMarking());
        finder.addNode(zone);
    }
    rt->gcZoneGroups = finder.getResultsList();
    rt->gcCurrentZoneGroup = rt->gcZoneGroups;
    rt->gcZoneGroupIndex = 0;
}

static void
ResetGrayList(JSCompartment* comp);

static void
GetNextZoneGroup(JSRuntime *rt)
{
    rt->gcCurrentZoneGroup = rt->gcCurrentZoneGroup->nextGroup();
    ++rt->gcZoneGroupIndex;
    if (!rt->gcCurrentZoneGroup) {
        rt->gcAbortSweepAfterCurrentGroup = false;
        return;
    }

    if (!rt->gcIsIncremental)
        ComponentFinder<Zone>::mergeGroups(rt->gcCurrentZoneGroup);

    if (rt->gcAbortSweepAfterCurrentGroup) {
        JS_ASSERT(!rt->gcIsIncremental);
        for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
            JS_ASSERT(!zone->gcNextGraphComponent);
            JS_ASSERT(zone->isGCMarking());
            zone->setNeedsBarrier(false, Zone::UpdateIon);
            zone->setGCState(Zone::NoGC);
            zone->gcGrayRoots.clearAndFree();
        }
        rt->setNeedsBarrier(false);
        AssertNeedsBarrierFlagsConsistent(rt);

        for (GCCompartmentGroupIter comp(rt); !comp.done(); comp.next()) {
            ArrayBufferObject::resetArrayBufferList(comp);
            ResetGrayList(comp);
        }

        rt->gcAbortSweepAfterCurrentGroup = false;
        rt->gcCurrentZoneGroup = NULL;
    }
}

/*
 * Gray marking:
 *
 * At the end of collection, anything reachable from a gray root that has not
 * otherwise been marked black must be marked gray.
 *
 * This means that when marking things gray we must not allow marking to leave
 * the current compartment group, as that could result in things being marked
 * grey when they might subsequently be marked black.  To achieve this, when we
 * find a cross compartment pointer we don't mark the referent but add it to a
 * singly-linked list of incoming gray pointers that is stored with each
 * compartment.
 *
 * The list head is stored in JSCompartment::gcIncomingGrayPointers and contains
 * cross compartment wrapper objects. The next pointer is stored in the second
 * extra slot of the cross compartment wrapper.
 *
 * The list is created during gray marking when one of the
 * MarkCrossCompartmentXXX functions is called for a pointer that leaves the
 * current compartent group.  This calls DelayCrossCompartmentGrayMarking to
 * push the referring object onto the list.
 *
 * The list is traversed and then unlinked in
 * MarkIncomingCrossCompartmentPointers.
 */

static bool
IsGrayListObject(JSObject *obj)
{
    JS_ASSERT(obj);
    return obj->is<CrossCompartmentWrapperObject>() && !IsDeadProxyObject(obj);
}

/* static */ unsigned
ProxyObject::grayLinkSlot(JSObject *obj)
{
    JS_ASSERT(IsGrayListObject(obj));
    return ProxyObject::EXTRA_SLOT + 1;
}

#ifdef DEBUG
static void
AssertNotOnGrayList(JSObject *obj)
{
    JS_ASSERT_IF(IsGrayListObject(obj),
                 obj->getReservedSlot(ProxyObject::grayLinkSlot(obj)).isUndefined());
}
#endif

static JSObject *
CrossCompartmentPointerReferent(JSObject *obj)
{
    JS_ASSERT(IsGrayListObject(obj));
    return &obj->as<ProxyObject>().private_().toObject();
}

static JSObject *
NextIncomingCrossCompartmentPointer(JSObject *prev, bool unlink)
{
    unsigned slot = ProxyObject::grayLinkSlot(prev);
    JSObject *next = prev->getReservedSlot(slot).toObjectOrNull();
    JS_ASSERT_IF(next, IsGrayListObject(next));

    if (unlink)
        prev->setSlot(slot, UndefinedValue());

    return next;
}

void
js::DelayCrossCompartmentGrayMarking(JSObject *src)
{
    JS_ASSERT(IsGrayListObject(src));

    /* Called from MarkCrossCompartmentXXX functions. */
    unsigned slot = ProxyObject::grayLinkSlot(src);
    JSObject *dest = CrossCompartmentPointerReferent(src);
    JSCompartment *comp = dest->compartment();

    if (src->getReservedSlot(slot).isUndefined()) {
        src->setCrossCompartmentSlot(slot, ObjectOrNullValue(comp->gcIncomingGrayPointers));
        comp->gcIncomingGrayPointers = src;
    } else {
        JS_ASSERT(src->getReservedSlot(slot).isObjectOrNull());
    }

#ifdef DEBUG
    /*
     * Assert that the object is in our list, also walking the list to check its
     * integrity.
     */
    JSObject *obj = comp->gcIncomingGrayPointers;
    bool found = false;
    while (obj) {
        if (obj == src)
            found = true;
        obj = NextIncomingCrossCompartmentPointer(obj, false);
    }
    JS_ASSERT(found);
#endif
}

static void
MarkIncomingCrossCompartmentPointers(JSRuntime *rt, const uint32_t color)
{
    JS_ASSERT(color == BLACK || color == GRAY);

    gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_MARK);
    static const gcstats::Phase statsPhases[] = {
        gcstats::PHASE_SWEEP_MARK_INCOMING_BLACK,
        gcstats::PHASE_SWEEP_MARK_INCOMING_GRAY
    };
    gcstats::AutoPhase ap1(rt->gcStats, statsPhases[color]);

    bool unlinkList = color == GRAY;

    for (GCCompartmentGroupIter c(rt); !c.done(); c.next()) {
        JS_ASSERT_IF(color == GRAY, c->zone()->isGCMarkingGray());
        JS_ASSERT_IF(color == BLACK, c->zone()->isGCMarkingBlack());
        JS_ASSERT_IF(c->gcIncomingGrayPointers, IsGrayListObject(c->gcIncomingGrayPointers));

        for (JSObject *src = c->gcIncomingGrayPointers;
             src;
             src = NextIncomingCrossCompartmentPointer(src, unlinkList))
        {
            JSObject *dst = CrossCompartmentPointerReferent(src);
            JS_ASSERT(dst->compartment() == c);

            if (color == GRAY) {
                if (IsObjectMarked(&src) && src->isMarked(GRAY))
                    MarkGCThingUnbarriered(&rt->gcMarker, (void**)&dst,
                                           "cross-compartment gray pointer");
            } else {
                if (IsObjectMarked(&src) && !src->isMarked(GRAY))
                    MarkGCThingUnbarriered(&rt->gcMarker, (void**)&dst,
                                           "cross-compartment black pointer");
            }
        }

        if (unlinkList)
            c->gcIncomingGrayPointers = NULL;
    }

    SliceBudget budget;
    rt->gcMarker.drainMarkStack(budget);
}

static bool
RemoveFromGrayList(JSObject *wrapper)
{
    if (!IsGrayListObject(wrapper))
        return false;

    unsigned slot = ProxyObject::grayLinkSlot(wrapper);
    if (wrapper->getReservedSlot(slot).isUndefined())
        return false;  /* Not on our list. */

    JSObject *tail = wrapper->getReservedSlot(slot).toObjectOrNull();
    wrapper->setReservedSlot(slot, UndefinedValue());

    JSCompartment *comp = CrossCompartmentPointerReferent(wrapper)->compartment();
    JSObject *obj = comp->gcIncomingGrayPointers;
    if (obj == wrapper) {
        comp->gcIncomingGrayPointers = tail;
        return true;
    }

    while (obj) {
        unsigned slot = ProxyObject::grayLinkSlot(obj);
        JSObject *next = obj->getReservedSlot(slot).toObjectOrNull();
        if (next == wrapper) {
            obj->setCrossCompartmentSlot(slot, ObjectOrNullValue(tail));
            return true;
        }
        obj = next;
    }

    MOZ_ASSUME_UNREACHABLE("object not found in gray link list");
}

static void
ResetGrayList(JSCompartment *comp)
{
    JSObject *src = comp->gcIncomingGrayPointers;
    while (src)
        src = NextIncomingCrossCompartmentPointer(src, true);
    comp->gcIncomingGrayPointers = NULL;
}

void
js::NotifyGCNukeWrapper(JSObject *obj)
{
    /*
     * References to target of wrapper are being removed, we no longer have to
     * remember to mark it.
     */
    RemoveFromGrayList(obj);
}

enum {
    JS_GC_SWAP_OBJECT_A_REMOVED = 1 << 0,
    JS_GC_SWAP_OBJECT_B_REMOVED = 1 << 1
};

unsigned
js::NotifyGCPreSwap(JSObject *a, JSObject *b)
{
    /*
     * Two objects in the same compartment are about to have had their contents
     * swapped.  If either of them are in our gray pointer list, then we remove
     * them from the lists, returning a bitset indicating what happened.
     */
    return (RemoveFromGrayList(a) ? JS_GC_SWAP_OBJECT_A_REMOVED : 0) |
           (RemoveFromGrayList(b) ? JS_GC_SWAP_OBJECT_B_REMOVED : 0);
}

void
js::NotifyGCPostSwap(JSObject *a, JSObject *b, unsigned removedFlags)
{
    /*
     * Two objects in the same compartment have had their contents swapped.  If
     * either of them were in our gray pointer list, we re-add them again.
     */
    if (removedFlags & JS_GC_SWAP_OBJECT_A_REMOVED)
        DelayCrossCompartmentGrayMarking(b);
    if (removedFlags & JS_GC_SWAP_OBJECT_B_REMOVED)
        DelayCrossCompartmentGrayMarking(a);
}

static void
EndMarkingZoneGroup(JSRuntime *rt)
{
    /*
     * Mark any incoming black pointers from previously swept compartments
     * whose referents are not marked. This can occur when gray cells become
     * black by the action of UnmarkGray.
     */
    MarkIncomingCrossCompartmentPointers(rt, BLACK);

    MarkWeakReferencesInCurrentGroup(rt, gcstats::PHASE_SWEEP_MARK_WEAK);

    /*
     * Change state of current group to MarkGray to restrict marking to this
     * group.  Note that there may be pointers to the atoms compartment, and
     * these will be marked through, as they are not marked with
     * MarkCrossCompartmentXXX.
     */
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        JS_ASSERT(zone->isGCMarkingBlack());
        zone->setGCState(Zone::MarkGray);
    }

    /* Mark incoming gray pointers from previously swept compartments. */
    rt->gcMarker.setMarkColorGray();
    MarkIncomingCrossCompartmentPointers(rt, GRAY);
    rt->gcMarker.setMarkColorBlack();

    /* Mark gray roots and mark transitively inside the current compartment group. */
    MarkGrayReferencesInCurrentGroup(rt);

    /* Restore marking state. */
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        JS_ASSERT(zone->isGCMarkingGray());
        zone->setGCState(Zone::Mark);
    }

    JS_ASSERT(rt->gcMarker.isDrained());
}

static void
BeginSweepingZoneGroup(JSRuntime *rt)
{
    /*
     * Begin sweeping the group of zones in gcCurrentZoneGroup,
     * performing actions that must be done before yielding to caller.
     */

    bool sweepingAtoms = false;
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        /* Set the GC state to sweeping. */
        JS_ASSERT(zone->isGCMarking());
        zone->setGCState(Zone::Sweep);

        /* Purge the ArenaLists before sweeping. */
        zone->allocator.arenas.purge();

        if (zone == rt->atomsCompartment->zone())
            sweepingAtoms = true;
    }

    ValidateIncrementalMarking(rt);

    FreeOp fop(rt, rt->gcSweepOnBackgroundThread);

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_FINALIZE_START);
        if (rt->gcFinalizeCallback)
            rt->gcFinalizeCallback(&fop, JSFINALIZE_GROUP_START, !rt->gcIsFull /* unused */);
    }

    if (sweepingAtoms) {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_ATOMS);
        SweepAtoms(rt);
    }

    /* Prune out dead views from ArrayBuffer's view lists. */
    for (GCCompartmentGroupIter c(rt); !c.done(); c.next())
        ArrayBufferObject::sweep(c);

    /* Collect watch points associated with unreachable objects. */
    WatchpointMap::sweepAll(rt);

    /* Detach unreachable debuggers and global objects from each other. */
    Debugger::sweepAll(&fop);

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_COMPARTMENTS);

        for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
            gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_DISCARD_CODE);
            zone->discardJitCode(&fop, !zone->isPreservingCode());
        }

        bool releaseTypes = ReleaseObservedTypes(rt);
        for (GCCompartmentGroupIter c(rt); !c.done(); c.next()) {
            gcstats::AutoSCC scc(rt->gcStats, rt->gcZoneGroupIndex);
            c->sweep(&fop, releaseTypes);
        }

        for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
            gcstats::AutoSCC scc(rt->gcStats, rt->gcZoneGroupIndex);
            zone->sweep(&fop, releaseTypes);
        }
    }

    /*
     * Queue all GC things in all zones for sweeping, either in the
     * foreground or on the background thread.
     *
     * Note that order is important here for the background case.
     *
     * Objects are finalized immediately but this may change in the future.
     */
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        gcstats::AutoSCC scc(rt->gcStats, rt->gcZoneGroupIndex);
        zone->allocator.arenas.queueObjectsForSweep(&fop);
    }
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        gcstats::AutoSCC scc(rt->gcStats, rt->gcZoneGroupIndex);
        zone->allocator.arenas.queueStringsForSweep(&fop);
    }
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        gcstats::AutoSCC scc(rt->gcStats, rt->gcZoneGroupIndex);
        zone->allocator.arenas.queueScriptsForSweep(&fop);
    }
#ifdef JS_ION
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        gcstats::AutoSCC scc(rt->gcStats, rt->gcZoneGroupIndex);
        zone->allocator.arenas.queueIonCodeForSweep(&fop);
    }
#endif
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        gcstats::AutoSCC scc(rt->gcStats, rt->gcZoneGroupIndex);
        zone->allocator.arenas.queueShapesForSweep(&fop);
        zone->allocator.arenas.gcShapeArenasToSweep =
            zone->allocator.arenas.arenaListsToSweep[FINALIZE_SHAPE];
    }

    rt->gcSweepPhase = 0;
    rt->gcSweepZone = rt->gcCurrentZoneGroup;
    rt->gcSweepKindIndex = 0;

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_FINALIZE_END);
        if (rt->gcFinalizeCallback)
            rt->gcFinalizeCallback(&fop, JSFINALIZE_GROUP_END, !rt->gcIsFull /* unused */);
    }
}

static void
EndSweepingZoneGroup(JSRuntime *rt)
{
    /* Update the GC state for zones we have swept and unlink the list. */
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        JS_ASSERT(zone->isGCSweeping());
        zone->setGCState(Zone::Finished);
    }

    /* Reset the list of arenas marked as being allocated during sweep phase. */
    while (ArenaHeader *arena = rt->gcArenasAllocatedDuringSweep) {
        rt->gcArenasAllocatedDuringSweep = arena->getNextAllocDuringSweep();
        arena->unsetAllocDuringSweep();
    }
}

static void
BeginSweepPhase(JSRuntime *rt)
{
    /*
     * Sweep phase.
     *
     * Finalize as we sweep, outside of rt->gcLock but with rt->isHeapBusy()
     * true so that any attempt to allocate a GC-thing from a finalizer will
     * fail, rather than nest badly and leave the unmarked newborn to be swept.
     */

    JS_ASSERT(!rt->gcAbortSweepAfterCurrentGroup);

    ComputeNonIncrementalMarkingForValidation(rt);

    gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP);

#ifdef JS_THREADSAFE
    rt->gcSweepOnBackgroundThread = rt->hasContexts() && rt->useHelperThreads();
#endif

#ifdef DEBUG
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        JS_ASSERT(!c->gcIncomingGrayPointers);
        for (JSCompartment::WrapperEnum e(c); !e.empty(); e.popFront()) {
            if (e.front().key.kind != CrossCompartmentKey::StringWrapper)
                AssertNotOnGrayList(&e.front().value.get().toObject());
        }
    }
#endif

    DropStringWrappers(rt);
    FindZoneGroups(rt);
    EndMarkingZoneGroup(rt);
    BeginSweepingZoneGroup(rt);
}

bool
ArenaLists::foregroundFinalize(FreeOp *fop, AllocKind thingKind, SliceBudget &sliceBudget)
{
    if (!arenaListsToSweep[thingKind])
        return true;

    ArenaList &dest = arenaLists[thingKind];
    return FinalizeArenas(fop, &arenaListsToSweep[thingKind], dest, thingKind, sliceBudget);
}

static bool
DrainMarkStack(JSRuntime *rt, SliceBudget &sliceBudget, gcstats::Phase phase)
{
    /* Run a marking slice and return whether the stack is now empty. */
    gcstats::AutoPhase ap(rt->gcStats, phase);
    return rt->gcMarker.drainMarkStack(sliceBudget);
}

static bool
SweepPhase(JSRuntime *rt, SliceBudget &sliceBudget)
{
    gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP);
    FreeOp fop(rt, rt->gcSweepOnBackgroundThread);

    bool finished = DrainMarkStack(rt, sliceBudget, gcstats::PHASE_SWEEP_MARK);
    if (!finished)
        return false;

    for (;;) {
        /* Finalize foreground finalized things. */
        for (; rt->gcSweepPhase < FinalizePhaseCount ; ++rt->gcSweepPhase) {
            gcstats::AutoPhase ap(rt->gcStats, FinalizePhaseStatsPhase[rt->gcSweepPhase]);

            for (; rt->gcSweepZone; rt->gcSweepZone = rt->gcSweepZone->nextNodeInGroup()) {
                Zone *zone = rt->gcSweepZone;

                while (rt->gcSweepKindIndex < FinalizePhaseLength[rt->gcSweepPhase]) {
                    AllocKind kind = FinalizePhases[rt->gcSweepPhase][rt->gcSweepKindIndex];

                    if (!zone->allocator.arenas.foregroundFinalize(&fop, kind, sliceBudget))
                        return false;  /* Yield to the mutator. */

                    ++rt->gcSweepKindIndex;
                }
                rt->gcSweepKindIndex = 0;
            }
            rt->gcSweepZone = rt->gcCurrentZoneGroup;
        }

        /* Remove dead shapes from the shape tree, but don't finalize them yet. */
        {
            gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_SHAPE);

            for (; rt->gcSweepZone; rt->gcSweepZone = rt->gcSweepZone->nextNodeInGroup()) {
                Zone *zone = rt->gcSweepZone;
                while (ArenaHeader *arena = zone->allocator.arenas.gcShapeArenasToSweep) {
                    for (CellIterUnderGC i(arena); !i.done(); i.next()) {
                        Shape *shape = i.get<Shape>();
                        if (!shape->isMarked())
                            shape->sweep();
                    }

                    zone->allocator.arenas.gcShapeArenasToSweep = arena->next;
                    sliceBudget.step(Arena::thingsPerArena(Arena::thingSize(FINALIZE_SHAPE)));
                    if (sliceBudget.isOverBudget())
                        return false;  /* Yield to the mutator. */
                }
            }
        }

        EndSweepingZoneGroup(rt);
        GetNextZoneGroup(rt);
        if (!rt->gcCurrentZoneGroup)
            return true;  /* We're finished. */
        EndMarkingZoneGroup(rt);
        BeginSweepingZoneGroup(rt);
    }
}

static void
EndSweepPhase(JSRuntime *rt, JSGCInvocationKind gckind, bool lastGC)
{
    gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP);
    FreeOp fop(rt, rt->gcSweepOnBackgroundThread);

    JS_ASSERT_IF(lastGC, !rt->gcSweepOnBackgroundThread);

    JS_ASSERT(rt->gcMarker.isDrained());
    rt->gcMarker.stop();

    /*
     * Recalculate whether GC was full or not as this may have changed due to
     * newly created zones.  Can only change from full to not full.
     */
    if (rt->gcIsFull) {
        for (ZonesIter zone(rt); !zone.done(); zone.next()) {
            if (!zone->isCollecting()) {
                rt->gcIsFull = false;
                break;
            }
        }
    }

    /*
     * If we found any black->gray edges during marking, we completely clear the
     * mark bits of all uncollected zones, or if a reset has occured, zones that
     * will no longer be collected. This is safe, although it may
     * prevent the cycle collector from collecting some dead objects.
     */
    if (rt->gcFoundBlackGrayEdges) {
        for (ZonesIter zone(rt); !zone.done(); zone.next()) {
            if (!zone->isCollecting())
                zone->allocator.arenas.unmarkAll();
        }
    }

#ifdef DEBUG
    PropertyTree::dumpShapes(rt);
#endif

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_DESTROY);

        /*
         * Sweep script filenames after sweeping functions in the generic loop
         * above. In this way when a scripted function's finalizer destroys the
         * script and calls rt->destroyScriptHook, the hook can still access the
         * script's filename. See bug 323267.
         */
        if (rt->gcIsFull)
            SweepScriptData(rt);

        /* Clear out any small pools that we're hanging on to. */
        if (JSC::ExecutableAllocator *execAlloc = rt->maybeExecAlloc())
            execAlloc->purge();

        /*
         * This removes compartments from rt->compartment, so we do it last to make
         * sure we don't miss sweeping any compartments.
         */
        if (!lastGC)
            SweepZones(&fop, lastGC);

        if (!rt->gcSweepOnBackgroundThread) {
            /*
             * Destroy arenas after we finished the sweeping so finalizers can
             * safely use IsAboutToBeFinalized(). This is done on the
             * GCHelperThread if possible. We acquire the lock only because
             * Expire needs to unlock it for other callers.
             */
            AutoLockGC lock(rt);
            ExpireChunksAndArenas(rt, gckind == GC_SHRINK);
        }
    }

    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_FINALIZE_END);

        if (rt->gcFinalizeCallback)
            rt->gcFinalizeCallback(&fop, JSFINALIZE_COLLECTION_END, !rt->gcIsFull);

        /* If we finished a full GC, then the gray bits are correct. */
        if (rt->gcIsFull)
            rt->gcGrayBitsValid = true;
    }

    /* Set up list of zones for sweeping of background things. */
    JS_ASSERT(!rt->gcSweepingZones);
    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        zone->gcNextGraphNode = rt->gcSweepingZones;
        rt->gcSweepingZones = zone;
    }

    /* If not sweeping on background thread then we must do it here. */
    if (!rt->gcSweepOnBackgroundThread) {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_DESTROY);

        SweepBackgroundThings(rt, false);

        rt->freeLifoAlloc.freeAll();

        /* Ensure the compartments get swept if it's the last GC. */
        if (lastGC)
            SweepZones(&fop, lastGC);
    }

    for (ZonesIter zone(rt); !zone.done(); zone.next()) {
        zone->setGCLastBytes(zone->gcBytes, gckind);
        if (zone->isCollecting()) {
            JS_ASSERT(zone->isGCFinished());
            zone->setGCState(Zone::NoGC);
        }

#ifdef DEBUG
        JS_ASSERT(!zone->isCollecting());
        JS_ASSERT(!zone->wasGCStarted());

        for (unsigned i = 0 ; i < FINALIZE_LIMIT ; ++i) {
            JS_ASSERT_IF(!IsBackgroundFinalized(AllocKind(i)) ||
                         !rt->gcSweepOnBackgroundThread,
                         !zone->allocator.arenas.arenaListsToSweep[i]);
        }
#endif
    }

#ifdef DEBUG
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        JS_ASSERT(!c->gcIncomingGrayPointers);
        JS_ASSERT(!c->gcLiveArrayBuffers);

        for (JSCompartment::WrapperEnum e(c); !e.empty(); e.popFront()) {
            if (e.front().key.kind != CrossCompartmentKey::StringWrapper)
                AssertNotOnGrayList(&e.front().value.get().toObject());
        }
    }
#endif

    FinishMarkingValidation(rt);

    rt->gcLastGCTime = PRMJ_Now();
}

/* ...while this class is to be used only for garbage collection. */
class AutoGCSession : AutoTraceSession {
  public:
    explicit AutoGCSession(JSRuntime *rt);
    ~AutoGCSession();
};

/* Start a new heap session. */
AutoTraceSession::AutoTraceSession(JSRuntime *rt, js::HeapState heapState)
  : runtime(rt),
    prevState(rt->heapState),
    pause(rt)
{
    JS_ASSERT(!rt->noGCOrAllocationCheck);
    JS_ASSERT(!rt->isHeapBusy());
    JS_ASSERT(heapState != Idle);
    rt->heapState = heapState;
}

AutoTraceSession::~AutoTraceSession()
{
    JS_ASSERT(runtime->isHeapBusy());
    runtime->heapState = prevState;
}

AutoGCSession::AutoGCSession(JSRuntime *rt)
  : AutoTraceSession(rt, MajorCollecting)
{
    runtime->gcIsNeeded = false;
    runtime->gcInterFrameGC = true;

    runtime->gcNumber++;
}

AutoGCSession::~AutoGCSession()
{
#ifndef JS_MORE_DETERMINISTIC
    runtime->gcNextFullGCTime = PRMJ_Now() + GC_IDLE_FULL_SPAN;
#endif

    runtime->gcChunkAllocationSinceLastGC = false;

#ifdef JS_GC_ZEAL
    /* Keeping these around after a GC is dangerous. */
    runtime->gcSelectedForMarking.clearAndFree();
#endif

    /* Clear gcMallocBytes for all compartments */
    for (ZonesIter zone(runtime); !zone.done(); zone.next()) {
        zone->resetGCMallocBytes();
        zone->unscheduleGC();
    }

    runtime->resetGCMallocBytes();
}

AutoCopyFreeListToArenas::AutoCopyFreeListToArenas(JSRuntime *rt)
  : runtime(rt)
{
    for (ZonesIter zone(rt); !zone.done(); zone.next())
        zone->allocator.arenas.copyFreeListsToArenas();
}

AutoCopyFreeListToArenas::~AutoCopyFreeListToArenas()
{
    for (ZonesIter zone(runtime); !zone.done(); zone.next())
        zone->allocator.arenas.clearFreeListsInArenas();
}

static void
IncrementalCollectSlice(JSRuntime *rt,
                        int64_t budget,
                        JS::gcreason::Reason gcReason,
                        JSGCInvocationKind gcKind);

static void
ResetIncrementalGC(JSRuntime *rt, const char *reason)
{
    switch (rt->gcIncrementalState) {
      case NO_INCREMENTAL:
        return;

      case MARK: {
        /* Cancel any ongoing marking. */
        AutoCopyFreeListToArenas copy(rt);

        rt->gcMarker.reset();
        rt->gcMarker.stop();

        for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
            ArrayBufferObject::resetArrayBufferList(c);
            ResetGrayList(c);
        }

        for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
            JS_ASSERT(zone->isGCMarking());
            zone->setNeedsBarrier(false, Zone::UpdateIon);
            zone->setGCState(Zone::NoGC);
        }
        rt->setNeedsBarrier(false);
        AssertNeedsBarrierFlagsConsistent(rt);

        rt->gcIncrementalState = NO_INCREMENTAL;

        JS_ASSERT(!rt->gcStrictCompartmentChecking);

        break;
      }

      case SWEEP:
        rt->gcMarker.reset();

        for (ZonesIter zone(rt); !zone.done(); zone.next())
            zone->scheduledForDestruction = false;

        /* Finish sweeping the current zone group, then abort. */
        rt->gcAbortSweepAfterCurrentGroup = true;
        IncrementalCollectSlice(rt, SliceBudget::Unlimited, JS::gcreason::RESET, GC_NORMAL);

        {
            gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_WAIT_BACKGROUND_THREAD);
            rt->gcHelperThread.waitBackgroundSweepOrAllocEnd();
        }
        break;

      default:
        MOZ_ASSUME_UNREACHABLE("Invalid incremental GC state");
    }

    rt->gcStats.reset(reason);

#ifdef DEBUG
    for (CompartmentsIter c(rt); !c.done(); c.next())
        JS_ASSERT(!c->gcLiveArrayBuffers);

    for (ZonesIter zone(rt); !zone.done(); zone.next()) {
        JS_ASSERT(!zone->needsBarrier());
        for (unsigned i = 0; i < FINALIZE_LIMIT; ++i)
            JS_ASSERT(!zone->allocator.arenas.arenaListsToSweep[i]);
    }
#endif
}

class AutoGCSlice {
  public:
    AutoGCSlice(JSRuntime *rt);
    ~AutoGCSlice();

  private:
    JSRuntime *runtime;
};

AutoGCSlice::AutoGCSlice(JSRuntime *rt)
  : runtime(rt)
{
    /*
     * During incremental GC, the compartment's active flag determines whether
     * there are stack frames active for any of its scripts. Normally this flag
     * is set at the beginning of the mark phase. During incremental GC, we also
     * set it at the start of every phase.
     */
    for (ActivationIterator iter(rt); !iter.done(); ++iter)
        iter.activation()->compartment()->zone()->active = true;

    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        /*
         * Clear needsBarrier early so we don't do any write barriers during
         * GC. We don't need to update the Ion barriers (which is expensive)
         * because Ion code doesn't run during GC. If need be, we'll update the
         * Ion barriers in ~AutoGCSlice.
         */
        if (zone->isGCMarking()) {
            JS_ASSERT(zone->needsBarrier());
            zone->setNeedsBarrier(false, Zone::DontUpdateIon);
        } else {
            JS_ASSERT(!zone->needsBarrier());
        }
    }
    rt->setNeedsBarrier(false);
    AssertNeedsBarrierFlagsConsistent(rt);
}

AutoGCSlice::~AutoGCSlice()
{
    /* We can't use GCZonesIter if this is the end of the last slice. */
    bool haveBarriers = false;
    for (ZonesIter zone(runtime); !zone.done(); zone.next()) {
        if (zone->isGCMarking()) {
            zone->setNeedsBarrier(true, Zone::UpdateIon);
            zone->allocator.arenas.prepareForIncrementalGC(runtime);
            haveBarriers = true;
        } else {
            zone->setNeedsBarrier(false, Zone::UpdateIon);
        }
    }
    runtime->setNeedsBarrier(haveBarriers);
    AssertNeedsBarrierFlagsConsistent(runtime);
}

static void
PushZealSelectedObjects(JSRuntime *rt)
{
#ifdef JS_GC_ZEAL
    /* Push selected objects onto the mark stack and clear the list. */
    for (JSObject **obj = rt->gcSelectedForMarking.begin();
         obj != rt->gcSelectedForMarking.end(); obj++)
    {
        MarkObjectUnbarriered(&rt->gcMarker, obj, "selected obj");
    }
#endif
}

static void
IncrementalCollectSlice(JSRuntime *rt,
                        int64_t budget,
                        JS::gcreason::Reason reason,
                        JSGCInvocationKind gckind)
{
    AutoCopyFreeListToArenas copy(rt);
    AutoGCSlice slice(rt);

    gc::State initialState = rt->gcIncrementalState;

    int zeal = 0;
#ifdef JS_GC_ZEAL
    if (reason == JS::gcreason::DEBUG_GC && budget != SliceBudget::Unlimited) {
        /*
         * Do the incremental collection type specified by zeal mode if the
         * collection was triggered by RunDebugGC() and incremental GC has not
         * been cancelled by ResetIncrementalGC.
         */
        zeal = rt->gcZeal();
    }
#endif

    JS_ASSERT_IF(rt->gcIncrementalState != NO_INCREMENTAL, rt->gcIsIncremental);
    rt->gcIsIncremental = budget != SliceBudget::Unlimited;

    if (zeal == ZealIncrementalRootsThenFinish || zeal == ZealIncrementalMarkAllThenFinish) {
        /*
         * Yields between slices occurs at predetermined points in these modes;
         * the budget is not used.
         */
        budget = SliceBudget::Unlimited;
    }

    SliceBudget sliceBudget(budget);

    if (rt->gcIncrementalState == NO_INCREMENTAL) {
        rt->gcIncrementalState = MARK_ROOTS;
        rt->gcLastMarkSlice = false;
    }

    if (rt->gcIncrementalState == MARK)
        AutoGCRooter::traceAllWrappers(&rt->gcMarker);

    switch (rt->gcIncrementalState) {

      case MARK_ROOTS:
        if (!BeginMarkPhase(rt)) {
            rt->gcIncrementalState = NO_INCREMENTAL;
            return;
        }

        if (rt->hasContexts())
            PushZealSelectedObjects(rt);

        rt->gcIncrementalState = MARK;

        if (zeal == ZealIncrementalRootsThenFinish)
            break;

        /* fall through */

      case MARK: {
        /* If we needed delayed marking for gray roots, then collect until done. */
        if (!rt->gcMarker.hasBufferedGrayRoots())
            sliceBudget.reset();

        bool finished = DrainMarkStack(rt, sliceBudget, gcstats::PHASE_MARK);
        if (!finished)
            break;

        JS_ASSERT(rt->gcMarker.isDrained());

        if (!rt->gcLastMarkSlice &&
            ((initialState == MARK && budget != SliceBudget::Unlimited) ||
             zeal == ZealIncrementalMarkAllThenFinish))
        {
            /*
             * Yield with the aim of starting the sweep in the next
             * slice.  We will need to mark anything new on the stack
             * when we resume, so we stay in MARK state.
             */
            rt->gcLastMarkSlice = true;
            break;
        }

        rt->gcIncrementalState = SWEEP;

        /*
         * This runs to completion, but we don't continue if the budget is
         * now exhasted.
         */
        BeginSweepPhase(rt);
        if (sliceBudget.isOverBudget())
            break;

        /*
         * Always yield here when running in incremental multi-slice zeal
         * mode, so RunDebugGC can reset the slice buget.
         */
        if (zeal == ZealIncrementalMultipleSlices)
            break;

        /* fall through */
      }

      case SWEEP: {
        bool finished = SweepPhase(rt, sliceBudget);
        if (!finished)
            break;

        EndSweepPhase(rt, gckind, reason == JS::gcreason::LAST_CONTEXT);

        if (rt->gcSweepOnBackgroundThread)
            rt->gcHelperThread.startBackgroundSweep(gckind == GC_SHRINK);

        rt->gcIncrementalState = NO_INCREMENTAL;
        break;
      }

      default:
        JS_ASSERT(false);
    }
}

IncrementalSafety
gc::IsIncrementalGCSafe(JSRuntime *rt)
{
    JS_ASSERT(!rt->mainThread.suppressGC);

    bool keepAtoms = false;
    for (ThreadDataIter iter(rt); !iter.done(); iter.next())
        keepAtoms |= iter->gcKeepAtoms;

    keepAtoms |= rt->exclusiveThreadsPresent();

    if (keepAtoms)
        return IncrementalSafety::Unsafe("gcKeepAtoms set");

    if (!rt->gcIncrementalEnabled)
        return IncrementalSafety::Unsafe("incremental permanently disabled");

    return IncrementalSafety::Safe();
}

static void
BudgetIncrementalGC(JSRuntime *rt, int64_t *budget)
{
    IncrementalSafety safe = IsIncrementalGCSafe(rt);
    if (!safe) {
        ResetIncrementalGC(rt, safe.reason());
        *budget = SliceBudget::Unlimited;
        rt->gcStats.nonincremental(safe.reason());
        return;
    }

    if (rt->gcMode != JSGC_MODE_INCREMENTAL) {
        ResetIncrementalGC(rt, "GC mode change");
        *budget = SliceBudget::Unlimited;
        rt->gcStats.nonincremental("GC mode");
        return;
    }

    if (rt->isTooMuchMalloc()) {
        *budget = SliceBudget::Unlimited;
        rt->gcStats.nonincremental("malloc bytes trigger");
    }

    bool reset = false;
    for (ZonesIter zone(rt); !zone.done(); zone.next()) {
        if (zone->gcBytes >= zone->gcTriggerBytes) {
            *budget = SliceBudget::Unlimited;
            rt->gcStats.nonincremental("allocation trigger");
        }

        if (rt->gcIncrementalState != NO_INCREMENTAL &&
            zone->isGCScheduled() != zone->wasGCStarted())
        {
            reset = true;
        }

        if (zone->isTooMuchMalloc()) {
            *budget = SliceBudget::Unlimited;
            rt->gcStats.nonincremental("malloc bytes trigger");
        }
    }

    if (reset)
        ResetIncrementalGC(rt, "zone change");
}

/*
 * GC, repeatedly if necessary, until we think we have not created any new
 * garbage. We disable inlining to ensure that the bottom of the stack with
 * possible GC roots recorded in MarkRuntime excludes any pointers we use during
 * the marking implementation.
 */
static JS_NEVER_INLINE void
GCCycle(JSRuntime *rt, bool incremental, int64_t budget, JSGCInvocationKind gckind, JS::gcreason::Reason reason)
{
    /* If we attempt to invoke the GC while we are running in the GC, assert. */
    JS_ASSERT(!rt->isHeapBusy());

#ifdef DEBUG
    for (ZonesIter zone(rt); !zone.done(); zone.next())
        JS_ASSERT_IF(rt->gcMode == JSGC_MODE_GLOBAL, zone->isGCScheduled());
#endif

    AutoGCSession gcsession(rt);

    /*
     * As we about to purge caches and clear the mark bits we must wait for
     * any background finalization to finish. We must also wait for the
     * background allocation to finish so we can avoid taking the GC lock
     * when manipulating the chunks during the GC.
     */
    {
        gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_WAIT_BACKGROUND_THREAD);
        rt->gcHelperThread.waitBackgroundSweepOrAllocEnd();
    }

    {
        if (!incremental) {
            /* If non-incremental GC was requested, reset incremental GC. */
            ResetIncrementalGC(rt, "requested");
            rt->gcStats.nonincremental("requested");
            budget = SliceBudget::Unlimited;
        } else {
            BudgetIncrementalGC(rt, &budget);
        }

        IncrementalCollectSlice(rt, budget, reason, gckind);
    }
}

#ifdef JS_GC_ZEAL
static bool
IsDeterministicGCReason(JS::gcreason::Reason reason)
{
    if (reason > JS::gcreason::DEBUG_GC &&
        reason != JS::gcreason::CC_FORCED && reason != JS::gcreason::SHUTDOWN_CC)
    {
        return false;
    }

    if (reason == JS::gcreason::MAYBEGC)
        return false;

    return true;
}
#endif

static bool
ShouldCleanUpEverything(JSRuntime *rt, JS::gcreason::Reason reason, JSGCInvocationKind gckind)
{
    // During shutdown, we must clean everything up, for the sake of leak
    // detection. When a runtime has no contexts, or we're doing a GC before a
    // shutdown CC, those are strong indications that we're shutting down.
    //
    // DEBUG_MODE_GC indicates we're discarding code because the debug mode
    // has changed; debug mode affects the results of bytecode analysis, so
    // we need to clear everything away.
    return !rt->hasContexts() ||
           reason == JS::gcreason::SHUTDOWN_CC ||
           reason == JS::gcreason::DEBUG_MODE_GC ||
           gckind == GC_SHRINK;
}

#ifdef JSGC_GENERATIONAL
class AutoDisableStoreBuffer
{
    JSRuntime *runtime;
    bool prior;

  public:
    AutoDisableStoreBuffer(JSRuntime *rt) : runtime(rt) {
        prior = rt->gcStoreBuffer.isEnabled();
        rt->gcStoreBuffer.disable();
    }
    ~AutoDisableStoreBuffer() {
        if (prior)
            runtime->gcStoreBuffer.enable();
    }
};
#else
struct AutoDisableStoreBuffer
{
    AutoDisableStoreBuffer(JSRuntime *) {}
};
#endif

static void
Collect(JSRuntime *rt, bool incremental, int64_t budget,
        JSGCInvocationKind gckind, JS::gcreason::Reason reason)
{
    /* GC shouldn't be running in parallel execution mode */
    JS_ASSERT(!InParallelSection());

    JS_AbortIfWrongThread(rt);

    if (rt->mainThread.suppressGC)
        return;

#if JS_TRACE_LOGGING
    AutoTraceLog logger(TraceLogging::defaultLogger(),
                        TraceLogging::GC_START,
                        TraceLogging::GC_STOP);
#endif

    ContextIter cx(rt);
    if (!cx.done())
        MaybeCheckStackRoots(cx);

#ifdef JS_GC_ZEAL
    if (rt->gcDeterministicOnly && !IsDeterministicGCReason(reason))
        return;
#endif

    JS_ASSERT_IF(!incremental || budget != SliceBudget::Unlimited, JSGC_INCREMENTAL);

    AutoStopVerifyingBarriers av(rt, reason == JS::gcreason::SHUTDOWN_CC || !rt->hasContexts());

    MinorGC(rt, reason);

    /*
     * Marking can trigger many incidental post barriers, some of them for
     * objects which are not going to be live after the GC.
     */
    AutoDisableStoreBuffer adsb(rt);

    RecordNativeStackTopForGC(rt);

    int zoneCount = 0;
    int compartmentCount = 0;
    int collectedCount = 0;
    for (ZonesIter zone(rt); !zone.done(); zone.next()) {
        if (rt->gcMode == JSGC_MODE_GLOBAL)
            zone->scheduleGC();

        /* This is a heuristic to avoid resets. */
        if (rt->gcIncrementalState != NO_INCREMENTAL && zone->needsBarrier())
            zone->scheduleGC();

        zoneCount++;
        if (zone->isGCScheduled())
            collectedCount++;
    }

    for (CompartmentsIter c(rt); !c.done(); c.next())
        compartmentCount++;

    rt->gcShouldCleanUpEverything = ShouldCleanUpEverything(rt, reason, gckind);

    gcstats::AutoGCSlice agc(rt->gcStats, collectedCount, zoneCount, compartmentCount, reason);

    do {
        /*
         * Let the API user decide to defer a GC if it wants to (unless this
         * is the last context). Invoke the callback regardless.
         */
        if (rt->gcIncrementalState == NO_INCREMENTAL) {
            gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_GC_BEGIN);
            if (JSGCCallback callback = rt->gcCallback)
                callback(rt, JSGC_BEGIN, rt->gcCallbackData);
        }

        rt->gcPoke = false;
        GCCycle(rt, incremental, budget, gckind, reason);

        if (rt->gcIncrementalState == NO_INCREMENTAL) {
            gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_GC_END);
            if (JSGCCallback callback = rt->gcCallback)
                callback(rt, JSGC_END, rt->gcCallbackData);
        }

        /* Need to re-schedule all zones for GC. */
        if (rt->gcPoke && rt->gcShouldCleanUpEverything)
            JS::PrepareForFullGC(rt);

        /*
         * On shutdown, iterate until finalizers or the JSGC_END callback
         * stop creating garbage.
         */
    } while (rt->gcPoke && rt->gcShouldCleanUpEverything);
}

void
js::GC(JSRuntime *rt, JSGCInvocationKind gckind, JS::gcreason::Reason reason)
{
    Collect(rt, false, SliceBudget::Unlimited, gckind, reason);
}

void
js::GCSlice(JSRuntime *rt, JSGCInvocationKind gckind, JS::gcreason::Reason reason, int64_t millis)
{
    int64_t sliceBudget;
    if (millis)
        sliceBudget = SliceBudget::TimeBudget(millis);
    else if (rt->gcHighFrequencyGC && rt->gcDynamicMarkSlice)
        sliceBudget = rt->gcSliceBudget * IGC_MARK_SLICE_MULTIPLIER;
    else
        sliceBudget = rt->gcSliceBudget;

    Collect(rt, true, sliceBudget, gckind, reason);
}

void
js::GCFinalSlice(JSRuntime *rt, JSGCInvocationKind gckind, JS::gcreason::Reason reason)
{
    Collect(rt, true, SliceBudget::Unlimited, gckind, reason);
}

static bool
ZonesSelected(JSRuntime *rt)
{
    for (ZonesIter zone(rt); !zone.done(); zone.next()) {
        if (zone->isGCScheduled())
            return true;
    }
    return false;
}

void
js::GCDebugSlice(JSRuntime *rt, bool limit, int64_t objCount)
{
    int64_t budget = limit ? SliceBudget::WorkBudget(objCount) : SliceBudget::Unlimited;
    if (!ZonesSelected(rt)) {
        if (JS::IsIncrementalGCInProgress(rt))
            JS::PrepareForIncrementalGC(rt);
        else
            JS::PrepareForFullGC(rt);
    }
    Collect(rt, true, budget, GC_NORMAL, JS::gcreason::DEBUG_GC);
}

/* Schedule a full GC unless a zone will already be collected. */
void
js::PrepareForDebugGC(JSRuntime *rt)
{
    if (!ZonesSelected(rt))
        JS::PrepareForFullGC(rt);
}

JS_FRIEND_API(void)
JS::ShrinkGCBuffers(JSRuntime *rt)
{
    AutoLockGC lock(rt);
    JS_ASSERT(!rt->isHeapBusy());

    if (!rt->useHelperThreads())
        ExpireChunksAndArenas(rt, true);
    else
        rt->gcHelperThread.startBackgroundShrink();
}

void
js::MinorGC(JSRuntime *rt, JS::gcreason::Reason reason)
{
#ifdef JSGC_GENERATIONAL
#if JS_TRACE_LOGGING
    AutoTraceLog logger(TraceLogging::defaultLogger(),
                        TraceLogging::MINOR_GC_START,
                        TraceLogging::MINOR_GC_STOP);
#endif
    rt->gcNursery.collect(rt, reason);
#endif
}

void
js::gc::FinishBackgroundFinalize(JSRuntime *rt)
{
    rt->gcHelperThread.waitBackgroundSweepEnd();
}

AutoFinishGC::AutoFinishGC(JSRuntime *rt)
{
    if (JS::IsIncrementalGCInProgress(rt)) {
        JS::PrepareForIncrementalGC(rt);
        JS::FinishIncrementalGC(rt, JS::gcreason::API);
    }

    gc::FinishBackgroundFinalize(rt);
}

AutoPrepareForTracing::AutoPrepareForTracing(JSRuntime *rt)
  : finish(rt),
    session(rt),
    copy(rt)
{
    RecordNativeStackTopForGC(rt);
}

JSCompartment *
js::NewCompartment(JSContext *cx, Zone *zone, JSPrincipals *principals,
                   const JS::CompartmentOptions &options)
{
    JSRuntime *rt = cx->runtime();
    JS_AbortIfWrongThread(rt);

    ScopedJSDeletePtr<Zone> zoneHolder;
    if (!zone) {
        zone = cx->new_<Zone>(rt);
        if (!zone)
            return NULL;

        zoneHolder.reset(zone);

        if (!zone->init(cx))
            return NULL;

        zone->setGCLastBytes(8192, GC_NORMAL);

        JSPrincipals *trusted = rt->trustedPrincipals();
        zone->isSystem = principals && principals == trusted;
    }

    ScopedJSDeletePtr<JSCompartment> compartment(cx->new_<JSCompartment>(zone, options));
    if (!compartment || !compartment->init(cx))
        return NULL;

    // Set up the principals.
    JS_SetCompartmentPrincipals(compartment, principals);

    AutoLockGC lock(rt);

    if (!zone->compartments.append(compartment.get())) {
        js_ReportOutOfMemory(cx);
        return NULL;
    }

    if (zoneHolder && !rt->zones.append(zone)) {
        js_ReportOutOfMemory(cx);
        return NULL;
    }

    zoneHolder.forget();
    return compartment.forget();
}

void
gc::RunDebugGC(JSContext *cx)
{
#ifdef JS_GC_ZEAL
    JSRuntime *rt = cx->runtime();

    if (rt->mainThread.suppressGC)
        return;

    PrepareForDebugGC(cx->runtime());

    int type = rt->gcZeal();
    if (type == ZealIncrementalRootsThenFinish ||
        type == ZealIncrementalMarkAllThenFinish ||
        type == ZealIncrementalMultipleSlices)
    {
        js::gc::State initialState = rt->gcIncrementalState;
        int64_t budget;
        if (type == ZealIncrementalMultipleSlices) {
            /*
             * Start with a small slice limit and double it every slice. This
             * ensure that we get multiple slices, and collection runs to
             * completion.
             */
            if (initialState == NO_INCREMENTAL)
                rt->gcIncrementalLimit = rt->gcZealFrequency / 2;
            else
                rt->gcIncrementalLimit *= 2;
            budget = SliceBudget::WorkBudget(rt->gcIncrementalLimit);
        } else {
            // This triggers incremental GC but is actually ignored by IncrementalMarkSlice.
            budget = SliceBudget::WorkBudget(1);
        }

        Collect(rt, true, budget, GC_NORMAL, JS::gcreason::DEBUG_GC);

        /*
         * For multi-slice zeal, reset the slice size when we get to the sweep
         * phase.
         */
        if (type == ZealIncrementalMultipleSlices &&
            initialState == MARK && rt->gcIncrementalState == SWEEP)
        {
            rt->gcIncrementalLimit = rt->gcZealFrequency / 2;
        }
    } else if (type == ZealPurgeAnalysisValue) {
        cx->compartment()->types.maybePurgeAnalysis(cx, /* force = */ true);
    } else {
        Collect(rt, false, SliceBudget::Unlimited, GC_NORMAL, JS::gcreason::DEBUG_GC);
    }

#endif
}

void
gc::SetDeterministicGC(JSContext *cx, bool enabled)
{
#ifdef JS_GC_ZEAL
    JSRuntime *rt = cx->runtime();
    rt->gcDeterministicOnly = enabled;
#endif
}

void
gc::SetValidateGC(JSContext *cx, bool enabled)
{
    JSRuntime *rt = cx->runtime();
    rt->gcValidate = enabled;
}

void
gc::SetFullCompartmentChecks(JSContext *cx, bool enabled)
{
    JSRuntime *rt = cx->runtime();
    rt->gcFullCompartmentChecks = enabled;
}

#ifdef DEBUG

/* Should only be called manually under gdb */
void PreventGCDuringInteractiveDebug()
{
    TlsPerThreadData.get()->suppressGC++;
}

#endif

void
js::ReleaseAllJITCode(FreeOp *fop)
{
#ifdef JS_ION
    for (ZonesIter zone(fop->runtime()); !zone.done(); zone.next()) {

# ifdef DEBUG
        /* Assert no baseline scripts are marked as active. */
        for (CellIter i(zone, FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            JS_ASSERT_IF(script->hasBaselineScript(), !script->baselineScript()->active());
        }
# endif

        /* Mark baseline scripts on the stack as active. */
        ion::MarkActiveBaselineScripts(zone);

        ion::InvalidateAll(fop, zone);

        for (CellIter i(zone, FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            ion::FinishInvalidation(fop, script);

            /*
             * Discard baseline script if it's not marked as active. Note that
             * this also resets the active flag.
             */
            ion::FinishDiscardBaselineScript(fop, script);
        }
    }

    /* Sweep now invalidated compiler outputs from each compartment. */
    for (CompartmentsIter comp(fop->runtime()); !comp.done(); comp.next())
        comp->types.sweepCompilerOutputs(fop, false);
#endif
}

/*
 * There are three possible PCCount profiling states:
 *
 * 1. None: Neither scripts nor the runtime have count information.
 * 2. Profile: Active scripts have count information, the runtime does not.
 * 3. Query: Scripts do not have count information, the runtime does.
 *
 * When starting to profile scripts, counting begins immediately, with all JIT
 * code discarded and recompiled with counts as necessary. Active interpreter
 * frames will not begin profiling until they begin executing another script
 * (via a call or return).
 *
 * The below API functions manage transitions to new states, according
 * to the table below.
 *
 *                                  Old State
 *                          -------------------------
 * Function                 None      Profile   Query
 * --------
 * StartPCCountProfiling    Profile   Profile   Profile
 * StopPCCountProfiling     None      Query     Query
 * PurgePCCounts            None      None      None
 */

static void
ReleaseScriptCounts(FreeOp *fop)
{
    JSRuntime *rt = fop->runtime();
    JS_ASSERT(rt->scriptAndCountsVector);

    ScriptAndCountsVector &vec = *rt->scriptAndCountsVector;

    for (size_t i = 0; i < vec.length(); i++)
        vec[i].scriptCounts.destroy(fop);

    fop->delete_(rt->scriptAndCountsVector);
    rt->scriptAndCountsVector = NULL;
}

JS_FRIEND_API(void)
js::StartPCCountProfiling(JSContext *cx)
{
    JSRuntime *rt = cx->runtime();

    if (rt->profilingScripts)
        return;

    if (rt->scriptAndCountsVector)
        ReleaseScriptCounts(rt->defaultFreeOp());

    ReleaseAllJITCode(rt->defaultFreeOp());

    rt->profilingScripts = true;
}

JS_FRIEND_API(void)
js::StopPCCountProfiling(JSContext *cx)
{
    JSRuntime *rt = cx->runtime();

    if (!rt->profilingScripts)
        return;
    JS_ASSERT(!rt->scriptAndCountsVector);

    ReleaseAllJITCode(rt->defaultFreeOp());

    ScriptAndCountsVector *vec = cx->new_<ScriptAndCountsVector>(SystemAllocPolicy());
    if (!vec)
        return;

    for (ZonesIter zone(rt); !zone.done(); zone.next()) {
        for (CellIter i(zone, FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            if (script->hasScriptCounts && script->types) {
                ScriptAndCounts sac;
                sac.script = script;
                sac.scriptCounts.set(script->releaseScriptCounts());
                if (!vec->append(sac))
                    sac.scriptCounts.destroy(rt->defaultFreeOp());
            }
        }
    }

    rt->profilingScripts = false;
    rt->scriptAndCountsVector = vec;
}

JS_FRIEND_API(void)
js::PurgePCCounts(JSContext *cx)
{
    JSRuntime *rt = cx->runtime();

    if (!rt->scriptAndCountsVector)
        return;
    JS_ASSERT(!rt->profilingScripts);

    ReleaseScriptCounts(rt->defaultFreeOp());
}

void
js::PurgeJITCaches(Zone *zone)
{
#ifdef JS_ION
    for (CellIterUnderGC i(zone, FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();

        /* Discard Ion caches. */
        ion::PurgeCaches(script, zone);
    }
#endif
}


void
ArenaLists::adoptArenas(JSRuntime *rt, ArenaLists *fromArenaLists)
{
    // The other parallel threads have all completed now, and GC
    // should be inactive, but still take the lock as a kind of read
    // fence.
    AutoLockGC lock(rt);

    fromArenaLists->purge();

    for (size_t thingKind = 0; thingKind != FINALIZE_LIMIT; thingKind++) {
#ifdef JS_THREADSAFE
        // When we enter a parallel section, we join the background
        // thread, and we do not run GC while in the parallel section,
        // so no finalizer should be active!
        volatile uintptr_t *bfs = &backgroundFinalizeState[thingKind];
        switch (*bfs) {
          case BFS_DONE:
            break;
          case BFS_JUST_FINISHED:
            // No allocations between end of last sweep and now.
            // Transfering over arenas is a kind of allocation.
            *bfs = BFS_DONE;
            break;
          default:
            JS_ASSERT(!"Background finalization in progress, but it should not be.");
            break;
        }
#endif /* JS_THREADSAFE */

        ArenaList *fromList = &fromArenaLists->arenaLists[thingKind];
        ArenaList *toList = &arenaLists[thingKind];
        while (fromList->head != NULL) {
            ArenaHeader *fromHeader = fromList->head;
            fromList->head = fromHeader->next;
            fromHeader->next = NULL;

            toList->insert(fromHeader);
        }
    }
}

bool
ArenaLists::containsArena(JSRuntime *rt, ArenaHeader *needle)
{
    AutoLockGC lock(rt);
    size_t allocKind = needle->getAllocKind();
    for (ArenaHeader *aheader = arenaLists[allocKind].head;
         aheader != NULL;
         aheader = aheader->next)
    {
        if (aheader == needle)
            return true;
    }
    return false;
}


AutoMaybeTouchDeadZones::AutoMaybeTouchDeadZones(JSContext *cx)
  : runtime(cx->runtime()),
    markCount(runtime->gcObjectsMarkedInDeadZones),
    inIncremental(JS::IsIncrementalGCInProgress(runtime)),
    manipulatingDeadZones(runtime->gcManipulatingDeadZones)
{
    runtime->gcManipulatingDeadZones = true;
}

AutoMaybeTouchDeadZones::AutoMaybeTouchDeadZones(JSObject *obj)
  : runtime(obj->compartment()->runtimeFromMainThread()),
    markCount(runtime->gcObjectsMarkedInDeadZones),
    inIncremental(JS::IsIncrementalGCInProgress(runtime)),
    manipulatingDeadZones(runtime->gcManipulatingDeadZones)
{
    runtime->gcManipulatingDeadZones = true;
}

AutoMaybeTouchDeadZones::~AutoMaybeTouchDeadZones()
{
    if (inIncremental && runtime->gcObjectsMarkedInDeadZones != markCount) {
        JS::PrepareForFullGC(runtime);
        js::GC(runtime, GC_NORMAL, JS::gcreason::TRANSPLANT);
    }

    runtime->gcManipulatingDeadZones = manipulatingDeadZones;
}

AutoSuppressGC::AutoSuppressGC(JSContext *cx)
  : suppressGC_(cx->runtime()->mainThread.suppressGC)
{
    suppressGC_++;
}

AutoSuppressGC::AutoSuppressGC(JSCompartment *comp)
  : suppressGC_(comp->runtimeFromMainThread()->mainThread.suppressGC)
{
    suppressGC_++;
}

bool
js::UninlinedIsInsideNursery(JSRuntime *rt, const void *thing)
{
    return IsInsideNursery(rt, thing);
}

#ifdef DEBUG
AutoDisableProxyCheck::AutoDisableProxyCheck(JSRuntime *rt
                                             MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : count(rt->gcDisableStrictProxyCheckingCount)
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    count++;
}

JS_FRIEND_API(void)
JS::AssertGCThingMustBeTenured(JSObject *obj)
{
    JS_ASSERT((!IsNurseryAllocable(obj->tenuredGetAllocKind()) || obj->getClass()->finalize) &&
              obj->isTenured());
}
#endif
