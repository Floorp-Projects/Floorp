/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This code implements an incremental mark-and-sweep garbage collector, with
 * most sweeping carried out in the background on a parallel thread.
 *
 * Full vs. zone GC
 * ----------------
 *
 * The collector can collect all zones at once, or a subset. These types of
 * collection are referred to as a full GC and a zone GC respectively.
 *
 * It is possible for an incremental collection that started out as a full GC to
 * become a zone GC if new zones are created during the course of the
 * collection.
 *
 * Incremental collection
 * ----------------------
 *
 * For a collection to be carried out incrementally the following conditions
 * must be met:
 *  - the collection must be run by calling js::GCSlice() rather than js::GC()
 *  - the GC mode must have been set to JSGC_MODE_INCREMENTAL with
 *    JS_SetGCParameter()
 *  - no thread may have an AutoKeepAtoms instance on the stack
 *
 * The last condition is an engine-internal mechanism to ensure that incremental
 * collection is not carried out without the correct barriers being implemented.
 * For more information see 'Incremental marking' below.
 *
 * If the collection is not incremental, all foreground activity happens inside
 * a single call to GC() or GCSlice(). However the collection is not complete
 * until the background sweeping activity has finished.
 *
 * An incremental collection proceeds as a series of slices, interleaved with
 * mutator activity, i.e. running JavaScript code. Slices are limited by a time
 * budget. The slice finishes as soon as possible after the requested time has
 * passed.
 *
 * Collector states
 * ----------------
 *
 * The collector proceeds through the following states, the current state being
 * held in JSRuntime::gcIncrementalState:
 *
 *  - MarkRoots  - marks the stack and other roots
 *  - Mark       - incrementally marks reachable things
 *  - Sweep      - sweeps zones in groups and continues marking unswept zones
 *  - Finalize   - performs background finalization, concurrent with mutator
 *  - Compact    - incrementally compacts by zone
 *  - Decommit   - performs background decommit and chunk removal
 *
 * The MarkRoots activity always takes place in the first slice. The next two
 * states can take place over one or more slices.
 *
 * In other words an incremental collection proceeds like this:
 *
 * Slice 1:   MarkRoots:  Roots pushed onto the mark stack.
 *            Mark:       The mark stack is processed by popping an element,
 *                        marking it, and pushing its children.
 *
 *          ... JS code runs ...
 *
 * Slice 2:   Mark:       More mark stack processing.
 *
 *          ... JS code runs ...
 *
 * Slice n-1: Mark:       More mark stack processing.
 *
 *          ... JS code runs ...
 *
 * Slice n:   Mark:       Mark stack is completely drained.
 *            Sweep:      Select first group of zones to sweep and sweep them.
 *
 *          ... JS code runs ...
 *
 * Slice n+1: Sweep:      Mark objects in unswept zones that were newly
 *                        identified as alive (see below). Then sweep more zone
 *                        groups.
 *
 *          ... JS code runs ...
 *
 * Slice n+2: Sweep:      Mark objects in unswept zones that were newly
 *                        identified as alive. Then sweep more zone groups.
 *
 *          ... JS code runs ...
 *
 * Slice m:   Sweep:      Sweeping is finished, and background sweeping
 *                        started on the helper thread.
 *
 *          ... JS code runs, remaining sweeping done on background thread ...
 *
 * When background sweeping finishes the GC is complete.
 *
 * Incremental marking
 * -------------------
 *
 * Incremental collection requires close collaboration with the mutator (i.e.,
 * JS code) to guarantee correctness.
 *
 *  - During an incremental GC, if a memory location (except a root) is written
 *    to, then the value it previously held must be marked. Write barriers
 *    ensure this.
 *
 *  - Any object that is allocated during incremental GC must start out marked.
 *
 *  - Roots are marked in the first slice and hence don't need write barriers.
 *    Roots are things like the C stack and the VM stack.
 *
 * The problem that write barriers solve is that between slices the mutator can
 * change the object graph. We must ensure that it cannot do this in such a way
 * that makes us fail to mark a reachable object (marking an unreachable object
 * is tolerable).
 *
 * We use a snapshot-at-the-beginning algorithm to do this. This means that we
 * promise to mark at least everything that is reachable at the beginning of
 * collection. To implement it we mark the old contents of every non-root memory
 * location written to by the mutator while the collection is in progress, using
 * write barriers. This is described in gc/Barrier.h.
 *
 * Incremental sweeping
 * --------------------
 *
 * Sweeping is difficult to do incrementally because object finalizers must be
 * run at the start of sweeping, before any mutator code runs. The reason is
 * that some objects use their finalizers to remove themselves from caches. If
 * mutator code was allowed to run after the start of sweeping, it could observe
 * the state of the cache and create a new reference to an object that was just
 * about to be destroyed.
 *
 * Sweeping all finalizable objects in one go would introduce long pauses, so
 * instead sweeping broken up into groups of zones. Zones which are not yet
 * being swept are still marked, so the issue above does not apply.
 *
 * The order of sweeping is restricted by cross compartment pointers - for
 * example say that object |a| from zone A points to object |b| in zone B and
 * neither object was marked when we transitioned to the Sweep phase. Imagine we
 * sweep B first and then return to the mutator. It's possible that the mutator
 * could cause |a| to become alive through a read barrier (perhaps it was a
 * shape that was accessed via a shape table). Then we would need to mark |b|,
 * which |a| points to, but |b| has already been swept.
 *
 * So if there is such a pointer then marking of zone B must not finish before
 * marking of zone A.  Pointers which form a cycle between zones therefore
 * restrict those zones to being swept at the same time, and these are found
 * using Tarjan's algorithm for finding the strongly connected components of a
 * graph.
 *
 * GC things without finalizers, and things with finalizers that are able to run
 * in the background, are swept on the background thread. This accounts for most
 * of the sweeping work.
 *
 * Reset
 * -----
 *
 * During incremental collection it is possible, although unlikely, for
 * conditions to change such that incremental collection is no longer safe. In
 * this case, the collection is 'reset' by ResetIncrementalGC(). If we are in
 * the mark state, this just stops marking, but if we have started sweeping
 * already, we continue until we have swept the current zone group. Following a
 * reset, a new non-incremental collection is started.
 *
 * Compacting GC
 * -------------
 *
 * Compacting GC happens at the end of a major GC as part of the last slice.
 * There are three parts:
 *
 *  - Arenas are selected for compaction.
 *  - The contents of those arenas are moved to new arenas.
 *  - All references to moved things are updated.
 *
 * Collecting Atoms
 * ----------------
 *
 * Atoms are collected differently from other GC things. They are contained in
 * a special zone and things in other zones may have pointers to them that are
 * not recorded in the cross compartment pointer map. Each zone holds a bitmap
 * with the atoms it might be keeping alive, and atoms are only collected if
 * they are not included in any zone's atom bitmap. See AtomMarking.cpp for how
 * this bitmap is managed.
 */

#include "jsgcinlines.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/TimeStamp.h"

#include <ctype.h>
#include <string.h>
#ifndef XP_WIN
# include <sys/mman.h>
# include <unistd.h>
#endif

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "jsfriendapi.h"
#include "jsobj.h"
#include "jsprf.h"
#include "jsscript.h"
#include "jstypes.h"
#include "jsutil.h"
#include "jswatchpoint.h"
#include "jsweakmap.h"
#ifdef XP_WIN
# include "jswin.h"
#endif

#include "gc/FindSCCs.h"
#include "gc/GCInternals.h"
#include "gc/GCTrace.h"
#include "gc/Marking.h"
#include "gc/Memory.h"
#include "gc/Policy.h"
#include "jit/BaselineJIT.h"
#include "jit/IonCode.h"
#include "jit/JitcodeMap.h"
#include "js/SliceBudget.h"
#include "proxy/DeadObjectProxy.h"
#include "vm/Debugger.h"
#include "vm/GeckoProfiler.h"
#include "vm/ProxyObject.h"
#include "vm/Shape.h"
#include "vm/String.h"
#include "vm/Symbol.h"
#include "vm/Time.h"
#include "vm/TraceLogging.h"
#include "vm/WrapperObject.h"

#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "gc/Heap-inl.h"
#include "gc/Nursery-inl.h"
#include "vm/GeckoProfiler-inl.h"
#include "vm/Stack-inl.h"
#include "vm/String-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::ArrayLength;
using mozilla::Get;
using mozilla::HashCodeScrambler;
using mozilla::Maybe;
using mozilla::Swap;

using JS::AutoGCRooter;

/* Increase the IGC marking slice time if we are in highFrequencyGC mode. */
static const int IGC_MARK_SLICE_MULTIPLIER = 2;

const AllocKind gc::slotsToThingKind[] = {
    /*  0 */ AllocKind::OBJECT0,  AllocKind::OBJECT2,  AllocKind::OBJECT2,  AllocKind::OBJECT4,
    /*  4 */ AllocKind::OBJECT4,  AllocKind::OBJECT8,  AllocKind::OBJECT8,  AllocKind::OBJECT8,
    /*  8 */ AllocKind::OBJECT8,  AllocKind::OBJECT12, AllocKind::OBJECT12, AllocKind::OBJECT12,
    /* 12 */ AllocKind::OBJECT12, AllocKind::OBJECT16, AllocKind::OBJECT16, AllocKind::OBJECT16,
    /* 16 */ AllocKind::OBJECT16
};

static_assert(JS_ARRAY_LENGTH(slotsToThingKind) == SLOTS_TO_THING_KIND_LIMIT,
              "We have defined a slot count for each kind.");

#define CHECK_THING_SIZE(allocKind, traceKind, type, sizedType) \
    static_assert(sizeof(sizedType) >= SortedArenaList::MinThingSize, \
                  #sizedType " is smaller than SortedArenaList::MinThingSize!"); \
    static_assert(sizeof(sizedType) >= sizeof(FreeSpan), \
                  #sizedType " is smaller than FreeSpan"); \
    static_assert(sizeof(sizedType) % CellSize == 0, \
                  "Size of " #sizedType " is not a multiple of CellSize");
FOR_EACH_ALLOCKIND(CHECK_THING_SIZE);
#undef CHECK_THING_SIZE

const uint32_t Arena::ThingSizes[] = {
#define EXPAND_THING_SIZE(allocKind, traceKind, type, sizedType) \
    sizeof(sizedType),
FOR_EACH_ALLOCKIND(EXPAND_THING_SIZE)
#undef EXPAND_THING_SIZE
};

FreeSpan ArenaLists::placeholder;

#undef CHECK_THING_SIZE_INNER
#undef CHECK_THING_SIZE

#define OFFSET(type) uint32_t(ArenaHeaderSize + (ArenaSize - ArenaHeaderSize) % sizeof(type))

const uint32_t Arena::FirstThingOffsets[] = {
#define EXPAND_FIRST_THING_OFFSET(allocKind, traceKind, type, sizedType) \
    OFFSET(sizedType),
FOR_EACH_ALLOCKIND(EXPAND_FIRST_THING_OFFSET)
#undef EXPAND_FIRST_THING_OFFSET
};

#undef OFFSET

#define COUNT(type) uint32_t((ArenaSize - ArenaHeaderSize) / sizeof(type))

const uint32_t Arena::ThingsPerArena[] = {
#define EXPAND_THINGS_PER_ARENA(allocKind, traceKind, type, sizedType) \
    COUNT(sizedType),
FOR_EACH_ALLOCKIND(EXPAND_THINGS_PER_ARENA)
#undef EXPAND_THINGS_PER_ARENA
};

#undef COUNT

struct js::gc::FinalizePhase
{
    gcstats::Phase statsPhase;
    AllocKinds kinds;
};

/*
 * Finalization order for GC things swept incrementally on the active thread.
 */
static const FinalizePhase IncrementalFinalizePhases[] = {
    {
        gcstats::PHASE_SWEEP_STRING, {
            AllocKind::EXTERNAL_STRING
        }
    },
    {
        gcstats::PHASE_SWEEP_SCRIPT, {
            AllocKind::SCRIPT
        }
    },
    {
        gcstats::PHASE_SWEEP_JITCODE, {
            AllocKind::JITCODE
        }
    }
};

/*
 * Finalization order for GC things swept on the background thread.
 */
static const FinalizePhase BackgroundFinalizePhases[] = {
    {
        gcstats::PHASE_SWEEP_SCRIPT, {
            AllocKind::LAZY_SCRIPT
        }
    },
    {
        gcstats::PHASE_SWEEP_OBJECT, {
            AllocKind::FUNCTION,
            AllocKind::FUNCTION_EXTENDED,
            AllocKind::OBJECT0_BACKGROUND,
            AllocKind::OBJECT2_BACKGROUND,
            AllocKind::OBJECT4_BACKGROUND,
            AllocKind::OBJECT8_BACKGROUND,
            AllocKind::OBJECT12_BACKGROUND,
            AllocKind::OBJECT16_BACKGROUND
        }
    },
    {
        gcstats::PHASE_SWEEP_SCOPE, {
            AllocKind::SCOPE
        }
    },
    {
        gcstats::PHASE_SWEEP_STRING, {
            AllocKind::FAT_INLINE_STRING,
            AllocKind::STRING,
            AllocKind::FAT_INLINE_ATOM,
            AllocKind::ATOM,
            AllocKind::SYMBOL
        }
    },
    {
        gcstats::PHASE_SWEEP_SHAPE, {
            AllocKind::SHAPE,
            AllocKind::ACCESSOR_SHAPE,
            AllocKind::BASE_SHAPE,
            AllocKind::OBJECT_GROUP
        }
    }
};

template<>
JSObject*
ArenaCellIterImpl::get<JSObject>() const
{
    MOZ_ASSERT(!done());
    return reinterpret_cast<JSObject*>(getCell());
}

void
Arena::unmarkAll()
{
    uintptr_t* word = chunk()->bitmap.arenaBits(this);
    memset(word, 0, ArenaBitmapWords * sizeof(uintptr_t));
}

/* static */ void
Arena::staticAsserts()
{
    static_assert(size_t(AllocKind::LIMIT) <= 255,
                  "We must be able to fit the allockind into uint8_t.");
    static_assert(JS_ARRAY_LENGTH(ThingSizes) == size_t(AllocKind::LIMIT),
                  "We haven't defined all thing sizes.");
    static_assert(JS_ARRAY_LENGTH(FirstThingOffsets) == size_t(AllocKind::LIMIT),
                  "We haven't defined all offsets.");
    static_assert(JS_ARRAY_LENGTH(ThingsPerArena) == size_t(AllocKind::LIMIT),
                  "We haven't defined all counts.");
}

template<typename T>
inline size_t
Arena::finalize(FreeOp* fop, AllocKind thingKind, size_t thingSize)
{
    /* Enforce requirements on size of T. */
    MOZ_ASSERT(thingSize % CellSize == 0);
    MOZ_ASSERT(thingSize <= 255);

    MOZ_ASSERT(allocated());
    MOZ_ASSERT(thingKind == getAllocKind());
    MOZ_ASSERT(thingSize == getThingSize());
    MOZ_ASSERT(!hasDelayedMarking);
    MOZ_ASSERT(!markOverflow);
    MOZ_ASSERT(!allocatedDuringIncremental);

    uint_fast16_t firstThing = firstThingOffset(thingKind);
    uint_fast16_t firstThingOrSuccessorOfLastMarkedThing = firstThing;
    uint_fast16_t lastThing = ArenaSize - thingSize;

    FreeSpan newListHead;
    FreeSpan* newListTail = &newListHead;
    size_t nmarked = 0;

    if (MOZ_UNLIKELY(MemProfiler::enabled())) {
        for (ArenaCellIterUnderFinalize i(this); !i.done(); i.next()) {
            T* t = i.get<T>();
            if (t->asTenured().isMarked())
                MemProfiler::MarkTenured(reinterpret_cast<void*>(t));
        }
    }

    for (ArenaCellIterUnderFinalize i(this); !i.done(); i.next()) {
        T* t = i.get<T>();
        if (t->asTenured().isMarked()) {
            uint_fast16_t thing = uintptr_t(t) & ArenaMask;
            if (thing != firstThingOrSuccessorOfLastMarkedThing) {
                // We just finished passing over one or more free things,
                // so record a new FreeSpan.
                newListTail->initBounds(firstThingOrSuccessorOfLastMarkedThing,
                                        thing - thingSize, this);
                newListTail = newListTail->nextSpanUnchecked(this);
            }
            firstThingOrSuccessorOfLastMarkedThing = thing + thingSize;
            nmarked++;
        } else {
            t->finalize(fop);
            JS_POISON(t, JS_SWEPT_TENURED_PATTERN, thingSize);
            TraceTenuredFinalize(t);
        }
    }

    if (nmarked == 0) {
        // Do nothing. The caller will update the arena appropriately.
        MOZ_ASSERT(newListTail == &newListHead);
        JS_EXTRA_POISON(data, JS_SWEPT_TENURED_PATTERN, sizeof(data));
        return nmarked;
    }

    MOZ_ASSERT(firstThingOrSuccessorOfLastMarkedThing != firstThing);
    uint_fast16_t lastMarkedThing = firstThingOrSuccessorOfLastMarkedThing - thingSize;
    if (lastThing == lastMarkedThing) {
        // If the last thing was marked, we will have already set the bounds of
        // the final span, and we just need to terminate the list.
        newListTail->initAsEmpty();
    } else {
        // Otherwise, end the list with a span that covers the final stretch of free things.
        newListTail->initFinal(firstThingOrSuccessorOfLastMarkedThing, lastThing, this);
    }

    firstFreeSpan = newListHead;
#ifdef DEBUG
    size_t nfree = numFreeThings(thingSize);
    MOZ_ASSERT(nfree + nmarked == thingsPerArena(thingKind));
#endif
    return nmarked;
}

// Finalize arenas from src list, releasing empty arenas if keepArenas wasn't
// specified and inserting the others into the appropriate destination size
// bins.
template<typename T>
static inline bool
FinalizeTypedArenas(FreeOp* fop,
                    Arena** src,
                    SortedArenaList& dest,
                    AllocKind thingKind,
                    SliceBudget& budget,
                    ArenaLists::KeepArenasEnum keepArenas)
{
    // When operating in the foreground, take the lock at the top.
    Maybe<AutoLockGC> maybeLock;
    if (fop->onActiveCooperatingThread())
        maybeLock.emplace(fop->runtime());

    // During background sweeping free arenas are released later on in
    // sweepBackgroundThings().
    MOZ_ASSERT_IF(!fop->onActiveCooperatingThread(), keepArenas == ArenaLists::KEEP_ARENAS);

    size_t thingSize = Arena::thingSize(thingKind);
    size_t thingsPerArena = Arena::thingsPerArena(thingKind);

    while (Arena* arena = *src) {
        *src = arena->next;
        size_t nmarked = arena->finalize<T>(fop, thingKind, thingSize);
        size_t nfree = thingsPerArena - nmarked;

        if (nmarked)
            dest.insertAt(arena, nfree);
        else if (keepArenas == ArenaLists::KEEP_ARENAS)
            arena->chunk()->recycleArena(arena, dest, thingsPerArena);
        else
            fop->runtime()->gc.releaseArena(arena, maybeLock.ref());

        budget.step(thingsPerArena);
        if (budget.isOverBudget())
            return false;
    }

    return true;
}

/*
 * Finalize the list. On return, |al|'s cursor points to the first non-empty
 * arena in the list (which may be null if all arenas are full).
 */
static bool
FinalizeArenas(FreeOp* fop,
               Arena** src,
               SortedArenaList& dest,
               AllocKind thingKind,
               SliceBudget& budget,
               ArenaLists::KeepArenasEnum keepArenas)
{
    switch (thingKind) {
#define EXPAND_CASE(allocKind, traceKind, type, sizedType) \
      case AllocKind::allocKind: \
        return FinalizeTypedArenas<type>(fop, src, dest, thingKind, budget, keepArenas);
FOR_EACH_ALLOCKIND(EXPAND_CASE)
#undef EXPAND_CASE

      default:
        MOZ_CRASH("Invalid alloc kind");
    }
}

Chunk*
ChunkPool::pop()
{
    MOZ_ASSERT(bool(head_) == bool(count_));
    if (!count_)
        return nullptr;
    return remove(head_);
}

void
ChunkPool::push(Chunk* chunk)
{
    MOZ_ASSERT(!chunk->info.next);
    MOZ_ASSERT(!chunk->info.prev);

    chunk->info.next = head_;
    if (head_)
        head_->info.prev = chunk;
    head_ = chunk;
    ++count_;

    MOZ_ASSERT(verify());
}

Chunk*
ChunkPool::remove(Chunk* chunk)
{
    MOZ_ASSERT(count_ > 0);
    MOZ_ASSERT(contains(chunk));

    if (head_ == chunk)
        head_ = chunk->info.next;
    if (chunk->info.prev)
        chunk->info.prev->info.next = chunk->info.next;
    if (chunk->info.next)
        chunk->info.next->info.prev = chunk->info.prev;
    chunk->info.next = chunk->info.prev = nullptr;
    --count_;

    MOZ_ASSERT(verify());
    return chunk;
}

#ifdef DEBUG
bool
ChunkPool::contains(Chunk* chunk) const
{
    verify();
    for (Chunk* cursor = head_; cursor; cursor = cursor->info.next) {
        if (cursor == chunk)
            return true;
    }
    return false;
}

bool
ChunkPool::verify() const
{
    MOZ_ASSERT(bool(head_) == bool(count_));
    uint32_t count = 0;
    for (Chunk* cursor = head_; cursor; cursor = cursor->info.next, ++count) {
        MOZ_ASSERT_IF(cursor->info.prev, cursor->info.prev->info.next == cursor);
        MOZ_ASSERT_IF(cursor->info.next, cursor->info.next->info.prev == cursor);
    }
    MOZ_ASSERT(count_ == count);
    return true;
}
#endif

void
ChunkPool::Iter::next()
{
    MOZ_ASSERT(!done());
    current_ = current_->info.next;
}

ChunkPool
GCRuntime::expireEmptyChunkPool(const AutoLockGC& lock)
{
    MOZ_ASSERT(emptyChunks(lock).verify());
    MOZ_ASSERT(tunables.minEmptyChunkCount(lock) <= tunables.maxEmptyChunkCount());

    ChunkPool expired;
    while (emptyChunks(lock).count() > tunables.minEmptyChunkCount(lock)) {
        Chunk* chunk = emptyChunks(lock).pop();
        prepareToFreeChunk(chunk->info);
        expired.push(chunk);
    }

    MOZ_ASSERT(expired.verify());
    MOZ_ASSERT(emptyChunks(lock).verify());
    MOZ_ASSERT(emptyChunks(lock).count() <= tunables.maxEmptyChunkCount());
    MOZ_ASSERT(emptyChunks(lock).count() <= tunables.minEmptyChunkCount(lock));
    return expired;
}

static void
FreeChunkPool(JSRuntime* rt, ChunkPool& pool)
{
    for (ChunkPool::Iter iter(pool); !iter.done();) {
        Chunk* chunk = iter.get();
        iter.next();
        pool.remove(chunk);
        MOZ_ASSERT(!chunk->info.numArenasFreeCommitted);
        UnmapPages(static_cast<void*>(chunk), ChunkSize);
    }
    MOZ_ASSERT(pool.count() == 0);
}

void
GCRuntime::freeEmptyChunks(JSRuntime* rt, const AutoLockGC& lock)
{
    FreeChunkPool(rt, emptyChunks(lock));
}

inline void
GCRuntime::prepareToFreeChunk(ChunkInfo& info)
{
    MOZ_ASSERT(numArenasFreeCommitted >= info.numArenasFreeCommitted);
    numArenasFreeCommitted -= info.numArenasFreeCommitted;
    stats().count(gcstats::STAT_DESTROY_CHUNK);
#ifdef DEBUG
    /*
     * Let FreeChunkPool detect a missing prepareToFreeChunk call before it
     * frees chunk.
     */
    info.numArenasFreeCommitted = 0;
#endif
}

inline void
GCRuntime::updateOnArenaFree(const ChunkInfo& info)
{
    ++numArenasFreeCommitted;
}

void
Chunk::addArenaToFreeList(JSRuntime* rt, Arena* arena)
{
    MOZ_ASSERT(!arena->allocated());
    arena->next = info.freeArenasHead;
    info.freeArenasHead = arena;
    ++info.numArenasFreeCommitted;
    ++info.numArenasFree;
    rt->gc.updateOnArenaFree(info);
}

void
Chunk::addArenaToDecommittedList(JSRuntime* rt, const Arena* arena)
{
    ++info.numArenasFree;
    decommittedArenas.set(Chunk::arenaIndex(arena->address()));
}

void
Chunk::recycleArena(Arena* arena, SortedArenaList& dest, size_t thingsPerArena)
{
    arena->setAsFullyUnused();
    dest.insertAt(arena, thingsPerArena);
}

void
Chunk::releaseArena(JSRuntime* rt, Arena* arena, const AutoLockGC& lock)
{
    MOZ_ASSERT(arena->allocated());
    MOZ_ASSERT(!arena->hasDelayedMarking);

    arena->release();
    addArenaToFreeList(rt, arena);
    updateChunkListAfterFree(rt, lock);
}

bool
Chunk::decommitOneFreeArena(JSRuntime* rt, AutoLockGC& lock)
{
    MOZ_ASSERT(info.numArenasFreeCommitted > 0);
    Arena* arena = fetchNextFreeArena(rt);
    updateChunkListAfterAlloc(rt, lock);

    bool ok;
    {
        AutoUnlockGC unlock(lock);
        ok = MarkPagesUnused(arena, ArenaSize);
    }

    if (ok)
        addArenaToDecommittedList(rt, arena);
    else
        addArenaToFreeList(rt, arena);
    updateChunkListAfterFree(rt, lock);

    return ok;
}

void
Chunk::decommitAllArenasWithoutUnlocking(const AutoLockGC& lock)
{
    for (size_t i = 0; i < ArenasPerChunk; ++i) {
        if (decommittedArenas.get(i) || arenas[i].allocated())
            continue;

        if (MarkPagesUnused(&arenas[i], ArenaSize)) {
            info.numArenasFreeCommitted--;
            decommittedArenas.set(i);
        }
    }
}

void
Chunk::updateChunkListAfterAlloc(JSRuntime* rt, const AutoLockGC& lock)
{
    if (MOZ_UNLIKELY(!hasAvailableArenas())) {
        rt->gc.availableChunks(lock).remove(this);
        rt->gc.fullChunks(lock).push(this);
    }
}

void
Chunk::updateChunkListAfterFree(JSRuntime* rt, const AutoLockGC& lock)
{
    if (info.numArenasFree == 1) {
        rt->gc.fullChunks(lock).remove(this);
        rt->gc.availableChunks(lock).push(this);
    } else if (!unused()) {
        MOZ_ASSERT(!rt->gc.fullChunks(lock).contains(this));
        MOZ_ASSERT(rt->gc.availableChunks(lock).contains(this));
        MOZ_ASSERT(!rt->gc.emptyChunks(lock).contains(this));
    } else {
        MOZ_ASSERT(unused());
        rt->gc.availableChunks(lock).remove(this);
        decommitAllArenas(rt);
        MOZ_ASSERT(info.numArenasFreeCommitted == 0);
        rt->gc.recycleChunk(this, lock);
    }
}

void
GCRuntime::releaseArena(Arena* arena, const AutoLockGC& lock)
{
    arena->zone->usage.removeGCArena();
    if (isBackgroundSweeping())
        arena->zone->threshold.updateForRemovedArena(tunables);
    return arena->chunk()->releaseArena(rt, arena, lock);
}

GCRuntime::GCRuntime(JSRuntime* rt) :
    rt(rt),
    systemZone(nullptr),
    systemZoneGroup(nullptr),
    atomsZone(nullptr),
    stats_(rt),
    marker(rt),
    usage(nullptr),
    mMemProfiler(rt),
    nextCellUniqueId_(LargestTaggedNullCellPointer + 1), // Ensure disjoint from null tagged pointers.
    numArenasFreeCommitted(0),
    verifyPreData(nullptr),
    chunkAllocationSinceLastGC(false),
    lastGCTime(PRMJ_Now()),
    mode(JSGC_MODE_INCREMENTAL),
    numActiveZoneIters(0),
    cleanUpEverything(false),
    grayBufferState(GCRuntime::GrayBufferState::Unused),
    grayBitsValid(false),
    majorGCTriggerReason(JS::gcreason::NO_REASON),
    fullGCForAtomsRequested_(false),
    minorGCNumber(0),
    majorGCNumber(0),
    jitReleaseNumber(0),
    number(0),
    startNumber(0),
    isFull(false),
    incrementalState(gc::State::NotActive),
    lastMarkSlice(false),
    sweepOnBackgroundThread(false),
    blocksToFreeAfterSweeping((size_t) JSContext::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    zoneGroupIndex(0),
    zoneGroups(nullptr),
    currentZoneGroup(nullptr),
    sweepZone(nullptr),
    sweepKind(AllocKind::FIRST),
    abortSweepAfterCurrentGroup(false),
    arenasAllocatedDuringSweep(nullptr),
    startedCompacting(false),
    relocatedArenasToRelease(nullptr),
#ifdef JS_GC_ZEAL
    markingValidator(nullptr),
#endif
    interFrameGC(false),
    defaultTimeBudget_((int64_t) SliceBudget::UnlimitedTimeBudget),
    incrementalAllowed(true),
    compactingEnabled(true),
    poked(false),
#ifdef JS_GC_ZEAL
    zealModeBits(0),
    zealFrequency(0),
    nextScheduled(0),
    deterministicOnly(false),
    incrementalLimit(0),
#endif
    fullCompartmentChecks(false),
    alwaysPreserveCode(false),
#ifdef DEBUG
    arenasEmptyAtShutdown(true),
#endif
    lock(mutexid::GCLock),
    allocTask(rt, emptyChunks_.ref()),
    decommitTask(rt),
    helperState(rt),
    nursery_(rt),
    storeBuffer_(rt, nursery()),
    blocksToFreeAfterMinorGC((size_t) JSContext::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE)
{
    setGCMode(JSGC_MODE_GLOBAL);
}

#ifdef JS_GC_ZEAL

void
GCRuntime::getZealBits(uint32_t* zealBits, uint32_t* frequency, uint32_t* scheduled)
{
    *zealBits = zealModeBits;
    *frequency = zealFrequency;
    *scheduled = nextScheduled;
}

const char* gc::ZealModeHelpText =
    "  Specifies how zealous the garbage collector should be. Some of these modes can\n"
    "  be set simultaneously, by passing multiple level options, e.g. \"2;4\" will activate\n"
    "  both modes 2 and 4. Modes can be specified by name or number.\n"
    "  \n"
    "  Values:\n"
    "    0: (None) Normal amount of collection (resets all modes)\n"
    "    1: (Poke) Collect when roots are added or removed\n"
    "    2: (Alloc) Collect when every N allocations (default: 100)\n"
    "    3: (FrameGC) Collect when the window paints (browser only)\n"
    "    4: (VerifierPre) Verify pre write barriers between instructions\n"
    "    5: (FrameVerifierPre) Verify pre write barriers between paints\n"
    "    6: (StackRooting) Verify stack rooting\n"
    "    7: (GenerationalGC) Collect the nursery every N nursery allocations\n"
    "    8: (IncrementalRootsThenFinish) Incremental GC in two slices: 1) mark roots 2) finish collection\n"
    "    9: (IncrementalMarkAllThenFinish) Incremental GC in two slices: 1) mark all 2) new marking and finish\n"
    "   10: (IncrementalMultipleSlices) Incremental GC in multiple slices\n"
    "   11: (IncrementalMarkingValidator) Verify incremental marking\n"
    "   12: (ElementsBarrier) Always use the individual element post-write barrier, regardless of elements size\n"
    "   13: (CheckHashTablesOnMinorGC) Check internal hashtables on minor GC\n"
    "   14: (Compact) Perform a shrinking collection every N allocations\n"
    "   15: (CheckHeapAfterGC) Walk the heap to check its integrity after every GC\n"
    "   16: (CheckNursery) Check nursery integrity on minor GC\n";

void
GCRuntime::setZeal(uint8_t zeal, uint32_t frequency)
{
    MOZ_ASSERT(zeal <= unsigned(ZealMode::Limit));

    if (verifyPreData)
        VerifyBarriers(rt, PreBarrierVerifier);

    if (zeal == 0 && hasZealMode(ZealMode::GenerationalGC)) {
        evictNursery(JS::gcreason::DEBUG_GC);
        nursery().leaveZealMode();
    }

    ZealMode zealMode = ZealMode(zeal);
    if (zealMode == ZealMode::GenerationalGC) {
        for (ZoneGroupsIter group(rt); !group.done(); group.next())
            group->nursery().enterZealMode();
    }

    // Zeal modes 8-10 are mutually exclusive. If we're setting one of those,
    // we first reset all of them.
    if (zealMode >= ZealMode::IncrementalRootsThenFinish &&
        zealMode <= ZealMode::IncrementalMultipleSlices)
    {
        clearZealMode(ZealMode::IncrementalRootsThenFinish);
        clearZealMode(ZealMode::IncrementalMarkAllThenFinish);
        clearZealMode(ZealMode::IncrementalMultipleSlices);
    }

    bool schedule = zealMode >= ZealMode::Alloc;
    if (zeal != 0)
        zealModeBits |= 1 << unsigned(zeal);
    else
        zealModeBits = 0;
    zealFrequency = frequency;
    nextScheduled = schedule ? frequency : 0;
}

void
GCRuntime::setNextScheduled(uint32_t count)
{
    nextScheduled = count;
}

bool
GCRuntime::parseAndSetZeal(const char* str)
{
    int frequency = -1;
    bool foundFrequency = false;
    mozilla::Vector<int, 0, SystemAllocPolicy> zeals;

    static const struct {
        const char* const zealMode;
        size_t length;
        uint32_t zeal;
    } zealModes[] = {
#define ZEAL_MODE(name, value) {#name, sizeof(#name) - 1, value},
        JS_FOR_EACH_ZEAL_MODE(ZEAL_MODE)
#undef ZEAL_MODE
        {"None", 4, 0}
    };

    do {
        int zeal = -1;

        const char* p = nullptr;
        if (isdigit(str[0])) {
            zeal = atoi(str);

            size_t offset = strspn(str, "0123456789");
            p = str + offset;
        } else {
            for (auto z : zealModes) {
                if (!strncmp(str, z.zealMode, z.length)) {
                    zeal = z.zeal;
                    p = str + z.length;
                    break;
                }
            }
        }
        if (p) {
            if (!*p || *p == ';') {
                frequency = JS_DEFAULT_ZEAL_FREQ;
            } else if (*p == ',') {
                frequency = atoi(p + 1);
                foundFrequency = true;
            }
        }

        if (zeal < 0 || zeal > int(ZealMode::Limit) || frequency <= 0) {
            fprintf(stderr, "Format: JS_GC_ZEAL=level(;level)*[,N]\n");
            fputs(ZealModeHelpText, stderr);
            return false;
        }

        if (!zeals.emplaceBack(zeal)) {
            return false;
        }
    } while (!foundFrequency &&
             (str = strchr(str, ';')) != nullptr &&
             str++);

    for (auto z : zeals)
        setZeal(z, frequency);
    return true;
}

#endif

/*
 * Lifetime in number of major GCs for type sets attached to scripts containing
 * observed types.
 */
static const uint64_t JIT_SCRIPT_RELEASE_TYPES_PERIOD = 20;

bool
GCRuntime::init(uint32_t maxbytes, uint32_t maxNurseryBytes)
{
    InitMemorySubsystem();

    if (!rootsHash.ref().init(256))
        return false;

    {
        AutoLockGC lock(rt);

        /*
         * Separate gcMaxMallocBytes from gcMaxBytes but initialize to maxbytes
         * for default backward API compatibility.
         */
        MOZ_ALWAYS_TRUE(tunables.setParameter(JSGC_MAX_BYTES, maxbytes, lock));
        MOZ_ALWAYS_TRUE(tunables.setParameter(JSGC_MAX_NURSERY_BYTES, maxNurseryBytes, lock));
        setMaxMallocBytes(maxbytes);

        const char* size = getenv("JSGC_MARK_STACK_LIMIT");
        if (size)
            setMarkStackLimit(atoi(size), lock);

        jitReleaseNumber = majorGCNumber + JIT_SCRIPT_RELEASE_TYPES_PERIOD;

        if (!nursery().init(maxNurseryBytes, lock))
            return false;
    }

#ifdef JS_GC_ZEAL
    const char* zealSpec = getenv("JS_GC_ZEAL");
    if (zealSpec && zealSpec[0] && !parseAndSetZeal(zealSpec))
        return false;
#endif

    if (!InitTrace(*this))
        return false;

    if (!marker.init(mode))
        return false;

    return true;
}

void
GCRuntime::finish()
{
    /* Wait for the nursery sweeping to end. */
    for (ZoneGroupsIter group(rt); !group.done(); group.next()) {
        if (group->nursery().isEnabled())
            group->nursery().waitBackgroundFreeEnd();
    }

    /*
     * Wait until the background finalization and allocation stops and the
     * helper thread shuts down before we forcefully release any remaining GC
     * memory.
     */
    helperState.finish();
    allocTask.cancel(GCParallelTask::CancelAndWait);
    decommitTask.cancel(GCParallelTask::CancelAndWait);

#ifdef JS_GC_ZEAL
    /* Free memory associated with GC verification. */
    finishVerifier();
#endif

    /* Delete all remaining zones. */
    if (rt->gcInitialized) {
        AutoSetThreadIsSweeping threadIsSweeping;
        for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
            for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next())
                js_delete(comp.get());
            js_delete(zone.get());
        }
    }

    groups.ref().clear();

    FreeChunkPool(rt, fullChunks_.ref());
    FreeChunkPool(rt, availableChunks_.ref());
    FreeChunkPool(rt, emptyChunks_.ref());

    FinishTrace();

    for (ZoneGroupsIter group(rt); !group.done(); group.next())
        group->nursery().printTotalProfileTimes();
    stats().printTotalProfileTimes();
}

bool
GCRuntime::setParameter(JSGCParamKey key, uint32_t value, AutoLockGC& lock)
{
    switch (key) {
      case JSGC_MAX_MALLOC_BYTES:
        setMaxMallocBytes(value);
        for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
            zone->setGCMaxMallocBytes(maxMallocBytesAllocated() * 0.9);
        break;
      case JSGC_SLICE_TIME_BUDGET:
        defaultTimeBudget_ = value ? value : SliceBudget::UnlimitedTimeBudget;
        break;
      case JSGC_MARK_STACK_LIMIT:
        if (value == 0)
            return false;
        setMarkStackLimit(value, lock);
        break;
      case JSGC_MODE:
        if (mode != JSGC_MODE_GLOBAL &&
            mode != JSGC_MODE_ZONE &&
            mode != JSGC_MODE_INCREMENTAL)
        {
            return false;
        }
        mode = JSGCMode(value);
        break;
      case JSGC_COMPACTING_ENABLED:
        compactingEnabled = value != 0;
        break;
      default:
        if (!tunables.setParameter(key, value, lock))
            return false;
        for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
            zone->threshold.updateAfterGC(zone->usage.gcBytes(), GC_NORMAL, tunables,
                                          schedulingState, lock);
        }
    }

    return true;
}

bool
GCSchedulingTunables::setParameter(JSGCParamKey key, uint32_t value, const AutoLockGC& lock)
{
    // Limit heap growth factor to one hundred times size of current heap.
    const double MaxHeapGrowthFactor = 100;

    switch(key) {
      case JSGC_MAX_BYTES:
        gcMaxBytes_ = value;
        break;
      case JSGC_MAX_NURSERY_BYTES:
        gcMaxNurseryBytes_ = value;
        break;
      case JSGC_HIGH_FREQUENCY_TIME_LIMIT:
        highFrequencyThresholdUsec_ = value * PRMJ_USEC_PER_MSEC;
        break;
      case JSGC_HIGH_FREQUENCY_LOW_LIMIT: {
        uint64_t newLimit = (uint64_t)value * 1024 * 1024;
        if (newLimit == UINT64_MAX)
            return false;
        highFrequencyLowLimitBytes_ = newLimit;
        if (highFrequencyLowLimitBytes_ >= highFrequencyHighLimitBytes_)
            highFrequencyHighLimitBytes_ = highFrequencyLowLimitBytes_ + 1;
        MOZ_ASSERT(highFrequencyHighLimitBytes_ > highFrequencyLowLimitBytes_);
        break;
      }
      case JSGC_HIGH_FREQUENCY_HIGH_LIMIT: {
        uint64_t newLimit = (uint64_t)value * 1024 * 1024;
        if (newLimit == 0)
            return false;
        highFrequencyHighLimitBytes_ = newLimit;
        if (highFrequencyHighLimitBytes_ <= highFrequencyLowLimitBytes_)
            highFrequencyLowLimitBytes_ = highFrequencyHighLimitBytes_ - 1;
        MOZ_ASSERT(highFrequencyHighLimitBytes_ > highFrequencyLowLimitBytes_);
        break;
      }
      case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX: {
        double newGrowth = value / 100.0;
        if (newGrowth <= 0.85 || newGrowth > MaxHeapGrowthFactor)
            return false;
        highFrequencyHeapGrowthMax_ = newGrowth;
        MOZ_ASSERT(highFrequencyHeapGrowthMax_ / 0.85 > 1.0);
        break;
      }
      case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN: {
        double newGrowth = value / 100.0;
        if (newGrowth <= 0.85 || newGrowth > MaxHeapGrowthFactor)
            return false;
        highFrequencyHeapGrowthMin_ = newGrowth;
        MOZ_ASSERT(highFrequencyHeapGrowthMin_ / 0.85 > 1.0);
        break;
      }
      case JSGC_LOW_FREQUENCY_HEAP_GROWTH: {
        double newGrowth = value / 100.0;
        if (newGrowth <= 0.9 || newGrowth > MaxHeapGrowthFactor)
            return false;
        lowFrequencyHeapGrowth_ = newGrowth;
        MOZ_ASSERT(lowFrequencyHeapGrowth_ / 0.9 > 1.0);
        break;
      }
      case JSGC_DYNAMIC_HEAP_GROWTH:
        dynamicHeapGrowthEnabled_ = value != 0;
        break;
      case JSGC_DYNAMIC_MARK_SLICE:
        dynamicMarkSliceEnabled_ = value != 0;
        break;
      case JSGC_ALLOCATION_THRESHOLD:
        gcZoneAllocThresholdBase_ = value * 1024 * 1024;
        break;
      case JSGC_MIN_EMPTY_CHUNK_COUNT:
        minEmptyChunkCount_ = value;
        if (minEmptyChunkCount_ > maxEmptyChunkCount_)
            maxEmptyChunkCount_ = minEmptyChunkCount_;
        MOZ_ASSERT(maxEmptyChunkCount_ >= minEmptyChunkCount_);
        break;
      case JSGC_MAX_EMPTY_CHUNK_COUNT:
        maxEmptyChunkCount_ = value;
        if (minEmptyChunkCount_ > maxEmptyChunkCount_)
            minEmptyChunkCount_ = maxEmptyChunkCount_;
        MOZ_ASSERT(maxEmptyChunkCount_ >= minEmptyChunkCount_);
        break;
      case JSGC_REFRESH_FRAME_SLICES_ENABLED:
        refreshFrameSlicesEnabled_ = value != 0;
        break;
      default:
        MOZ_CRASH("Unknown GC parameter.");
    }

    return true;
}

uint32_t
GCRuntime::getParameter(JSGCParamKey key, const AutoLockGC& lock)
{
    switch (key) {
      case JSGC_MAX_BYTES:
        return uint32_t(tunables.gcMaxBytes());
      case JSGC_MAX_MALLOC_BYTES:
        return mallocCounter.maxBytes();
      case JSGC_BYTES:
        return uint32_t(usage.gcBytes());
      case JSGC_MODE:
        return uint32_t(mode);
      case JSGC_UNUSED_CHUNKS:
        return uint32_t(emptyChunks(lock).count());
      case JSGC_TOTAL_CHUNKS:
        return uint32_t(fullChunks(lock).count() +
                        availableChunks(lock).count() +
                        emptyChunks(lock).count());
      case JSGC_SLICE_TIME_BUDGET:
        if (defaultTimeBudget_.ref() == SliceBudget::UnlimitedTimeBudget) {
            return 0;
        } else {
            MOZ_RELEASE_ASSERT(defaultTimeBudget_ >= 0);
            MOZ_RELEASE_ASSERT(defaultTimeBudget_ <= UINT32_MAX);
            return uint32_t(defaultTimeBudget_);
        }
      case JSGC_MARK_STACK_LIMIT:
        return marker.maxCapacity();
      case JSGC_HIGH_FREQUENCY_TIME_LIMIT:
        return tunables.highFrequencyThresholdUsec() / PRMJ_USEC_PER_MSEC;
      case JSGC_HIGH_FREQUENCY_LOW_LIMIT:
        return tunables.highFrequencyLowLimitBytes() / 1024 / 1024;
      case JSGC_HIGH_FREQUENCY_HIGH_LIMIT:
        return tunables.highFrequencyHighLimitBytes() / 1024 / 1024;
      case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX:
        return uint32_t(tunables.highFrequencyHeapGrowthMax() * 100);
      case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN:
        return uint32_t(tunables.highFrequencyHeapGrowthMin() * 100);
      case JSGC_LOW_FREQUENCY_HEAP_GROWTH:
        return uint32_t(tunables.lowFrequencyHeapGrowth() * 100);
      case JSGC_DYNAMIC_HEAP_GROWTH:
        return tunables.isDynamicHeapGrowthEnabled();
      case JSGC_DYNAMIC_MARK_SLICE:
        return tunables.isDynamicMarkSliceEnabled();
      case JSGC_ALLOCATION_THRESHOLD:
        return tunables.gcZoneAllocThresholdBase() / 1024 / 1024;
      case JSGC_MIN_EMPTY_CHUNK_COUNT:
        return tunables.minEmptyChunkCount(lock);
      case JSGC_MAX_EMPTY_CHUNK_COUNT:
        return tunables.maxEmptyChunkCount();
      case JSGC_COMPACTING_ENABLED:
        return compactingEnabled;
      case JSGC_REFRESH_FRAME_SLICES_ENABLED:
        return tunables.areRefreshFrameSlicesEnabled();
      default:
        MOZ_ASSERT(key == JSGC_NUMBER);
        return uint32_t(number);
    }
}

void
GCRuntime::setMarkStackLimit(size_t limit, AutoLockGC& lock)
{
    MOZ_ASSERT(!JS::CurrentThreadIsHeapBusy());
    AutoUnlockGC unlock(lock);
    AutoStopVerifyingBarriers pauseVerification(rt, false);
    marker.setMaxCapacity(limit);
}

bool
GCRuntime::addBlackRootsTracer(JSTraceDataOp traceOp, void* data)
{
    AssertHeapIsIdle();
    return !!blackRootTracers.ref().append(Callback<JSTraceDataOp>(traceOp, data));
}

void
GCRuntime::removeBlackRootsTracer(JSTraceDataOp traceOp, void* data)
{
    // Can be called from finalizers
    for (size_t i = 0; i < blackRootTracers.ref().length(); i++) {
        Callback<JSTraceDataOp>* e = &blackRootTracers.ref()[i];
        if (e->op == traceOp && e->data == data) {
            blackRootTracers.ref().erase(e);
        }
    }
}

void
GCRuntime::setGrayRootsTracer(JSTraceDataOp traceOp, void* data)
{
    AssertHeapIsIdle();
    grayRootTracer.op = traceOp;
    grayRootTracer.data = data;
}

void
GCRuntime::setGCCallback(JSGCCallback callback, void* data)
{
    gcCallback.op = callback;
    gcCallback.data = data;
}

void
GCRuntime::callGCCallback(JSGCStatus status) const
{
    if (gcCallback.op)
        gcCallback.op(TlsContext.get(), status, gcCallback.data);
}

void
GCRuntime::setObjectsTenuredCallback(JSObjectsTenuredCallback callback,
                                     void* data)
{
    tenuredCallback.op = callback;
    tenuredCallback.data = data;
}

void
GCRuntime::callObjectsTenuredCallback()
{
    if (tenuredCallback.op)
        tenuredCallback.op(TlsContext.get(), tenuredCallback.data);
}

namespace {

class AutoNotifyGCActivity {
  public:
    explicit AutoNotifyGCActivity(GCRuntime& gc) : gc_(gc) {
        if (!gc_.isIncrementalGCInProgress())
            gc_.callGCCallback(JSGC_BEGIN);
    }
    ~AutoNotifyGCActivity() {
        if (!gc_.isIncrementalGCInProgress())
            gc_.callGCCallback(JSGC_END);
    }

  private:
    GCRuntime& gc_;
};

} // (anon)

bool
GCRuntime::addFinalizeCallback(JSFinalizeCallback callback, void* data)
{
    return finalizeCallbacks.ref().append(Callback<JSFinalizeCallback>(callback, data));
}

void
GCRuntime::removeFinalizeCallback(JSFinalizeCallback callback)
{
    for (Callback<JSFinalizeCallback>* p = finalizeCallbacks.ref().begin();
         p < finalizeCallbacks.ref().end(); p++)
    {
        if (p->op == callback) {
            finalizeCallbacks.ref().erase(p);
            break;
        }
    }
}

void
GCRuntime::callFinalizeCallbacks(FreeOp* fop, JSFinalizeStatus status) const
{
    for (auto& p : finalizeCallbacks.ref())
        p.op(fop, status, !isFull, p.data);
}

bool
GCRuntime::addWeakPointerZoneGroupCallback(JSWeakPointerZoneGroupCallback callback, void* data)
{
    return updateWeakPointerZoneGroupCallbacks.ref().append(
            Callback<JSWeakPointerZoneGroupCallback>(callback, data));
}

void
GCRuntime::removeWeakPointerZoneGroupCallback(JSWeakPointerZoneGroupCallback callback)
{
    for (auto& p : updateWeakPointerZoneGroupCallbacks.ref()) {
        if (p.op == callback) {
            updateWeakPointerZoneGroupCallbacks.ref().erase(&p);
            break;
        }
    }
}

void
GCRuntime::callWeakPointerZoneGroupCallbacks() const
{
    for (auto const& p : updateWeakPointerZoneGroupCallbacks.ref())
        p.op(TlsContext.get(), p.data);
}

bool
GCRuntime::addWeakPointerCompartmentCallback(JSWeakPointerCompartmentCallback callback, void* data)
{
    return updateWeakPointerCompartmentCallbacks.ref().append(
            Callback<JSWeakPointerCompartmentCallback>(callback, data));
}

void
GCRuntime::removeWeakPointerCompartmentCallback(JSWeakPointerCompartmentCallback callback)
{
    for (auto& p : updateWeakPointerCompartmentCallbacks.ref()) {
        if (p.op == callback) {
            updateWeakPointerCompartmentCallbacks.ref().erase(&p);
            break;
        }
    }
}

void
GCRuntime::callWeakPointerCompartmentCallbacks(JSCompartment* comp) const
{
    for (auto const& p : updateWeakPointerCompartmentCallbacks.ref())
        p.op(TlsContext.get(), comp, p.data);
}

JS::GCSliceCallback
GCRuntime::setSliceCallback(JS::GCSliceCallback callback) {
    return stats().setSliceCallback(callback);
}

JS::GCNurseryCollectionCallback
GCRuntime::setNurseryCollectionCallback(JS::GCNurseryCollectionCallback callback) {
    return stats().setNurseryCollectionCallback(callback);
}

JS::DoCycleCollectionCallback
GCRuntime::setDoCycleCollectionCallback(JS::DoCycleCollectionCallback callback)
{
    auto prior = gcDoCycleCollectionCallback;
    gcDoCycleCollectionCallback = Callback<JS::DoCycleCollectionCallback>(callback, nullptr);
    return prior.op;
}

void
GCRuntime::callDoCycleCollectionCallback(JSContext* cx)
{
    if (gcDoCycleCollectionCallback.op)
        gcDoCycleCollectionCallback.op(cx);
}

bool
GCRuntime::addRoot(Value* vp, const char* name)
{
    /*
     * Sometimes Firefox will hold weak references to objects and then convert
     * them to strong references by calling AddRoot (e.g., via PreserveWrapper,
     * or ModifyBusyCount in workers). We need a read barrier to cover these
     * cases.
     */
    if (isIncrementalGCInProgress())
        GCPtrValue::writeBarrierPre(*vp);

    return rootsHash.ref().put(vp, name);
}

void
GCRuntime::removeRoot(Value* vp)
{
    rootsHash.ref().remove(vp);
    poke();
}

extern JS_FRIEND_API(bool)
js::AddRawValueRoot(JSContext* cx, Value* vp, const char* name)
{
    MOZ_ASSERT(vp);
    MOZ_ASSERT(name);
    bool ok = cx->runtime()->gc.addRoot(vp, name);
    if (!ok)
        JS_ReportOutOfMemory(cx);
    return ok;
}

extern JS_FRIEND_API(void)
js::RemoveRawValueRoot(JSContext* cx, Value* vp)
{
    cx->runtime()->gc.removeRoot(vp);
}

void
GCRuntime::setMaxMallocBytes(size_t value)
{
    mallocCounter.setMax(value);
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
        zone->setGCMaxMallocBytes(value);
}

void
GCRuntime::updateMallocCounter(JS::Zone* zone, size_t nbytes)
{
    bool triggered = mallocCounter.update(this, nbytes);
    if (!triggered && zone)
        zone->updateMallocCounter(nbytes);
}

double
ZoneHeapThreshold::allocTrigger(bool highFrequencyGC) const
{
    return (highFrequencyGC ? 0.85 : 0.9) * gcTriggerBytes();
}

/* static */ double
ZoneHeapThreshold::computeZoneHeapGrowthFactorForHeapSize(size_t lastBytes,
                                                          const GCSchedulingTunables& tunables,
                                                          const GCSchedulingState& state)
{
    if (!tunables.isDynamicHeapGrowthEnabled())
        return 3.0;

    // For small zones, our collection heuristics do not matter much: favor
    // something simple in this case.
    if (lastBytes < 1 * 1024 * 1024)
        return tunables.lowFrequencyHeapGrowth();

    // If GC's are not triggering in rapid succession, use a lower threshold so
    // that we will collect garbage sooner.
    if (!state.inHighFrequencyGCMode())
        return tunables.lowFrequencyHeapGrowth();

    // The heap growth factor depends on the heap size after a GC and the GC
    // frequency. For low frequency GCs (more than 1sec between GCs) we let
    // the heap grow to 150%. For high frequency GCs we let the heap grow
    // depending on the heap size:
    //   lastBytes < highFrequencyLowLimit: 300%
    //   lastBytes > highFrequencyHighLimit: 150%
    //   otherwise: linear interpolation between 300% and 150% based on lastBytes

    // Use shorter names to make the operation comprehensible.
    double minRatio = tunables.highFrequencyHeapGrowthMin();
    double maxRatio = tunables.highFrequencyHeapGrowthMax();
    double lowLimit = tunables.highFrequencyLowLimitBytes();
    double highLimit = tunables.highFrequencyHighLimitBytes();

    if (lastBytes <= lowLimit)
        return maxRatio;

    if (lastBytes >= highLimit)
        return minRatio;

    double factor = maxRatio - ((maxRatio - minRatio) * ((lastBytes - lowLimit) /
                                                         (highLimit - lowLimit)));
    MOZ_ASSERT(factor >= minRatio);
    MOZ_ASSERT(factor <= maxRatio);
    return factor;
}

/* static */ size_t
ZoneHeapThreshold::computeZoneTriggerBytes(double growthFactor, size_t lastBytes,
                                           JSGCInvocationKind gckind,
                                           const GCSchedulingTunables& tunables,
                                           const AutoLockGC& lock)
{
    size_t base = gckind == GC_SHRINK
                ? Max(lastBytes, tunables.minEmptyChunkCount(lock) * ChunkSize)
                : Max(lastBytes, tunables.gcZoneAllocThresholdBase());
    double trigger = double(base) * growthFactor;
    return size_t(Min(double(tunables.gcMaxBytes()), trigger));
}

void
ZoneHeapThreshold::updateAfterGC(size_t lastBytes, JSGCInvocationKind gckind,
                                 const GCSchedulingTunables& tunables,
                                 const GCSchedulingState& state, const AutoLockGC& lock)
{
    gcHeapGrowthFactor_ = computeZoneHeapGrowthFactorForHeapSize(lastBytes, tunables, state);
    gcTriggerBytes_ = computeZoneTriggerBytes(gcHeapGrowthFactor_, lastBytes, gckind, tunables,
                                              lock);
}

void
ZoneHeapThreshold::updateForRemovedArena(const GCSchedulingTunables& tunables)
{
    size_t amount = ArenaSize * gcHeapGrowthFactor_;
    MOZ_ASSERT(amount > 0);

    if ((gcTriggerBytes_ < amount) ||
        (gcTriggerBytes_ - amount < tunables.gcZoneAllocThresholdBase() * gcHeapGrowthFactor_))
    {
        return;
    }

    gcTriggerBytes_ -= amount;
}

void
GCMarker::delayMarkingArena(Arena* arena)
{
    if (arena->hasDelayedMarking) {
        /* Arena already scheduled to be marked later */
        return;
    }
    arena->setNextDelayedMarking(unmarkedArenaStackTop);
    unmarkedArenaStackTop = arena;
#ifdef DEBUG
    markLaterArenas++;
#endif
}

void
GCMarker::delayMarkingChildren(const void* thing)
{
    const TenuredCell* cell = TenuredCell::fromPointer(thing);
    cell->arena()->markOverflow = 1;
    delayMarkingArena(cell->arena());
}

inline void
ArenaLists::prepareForIncrementalGC()
{
    purge();
    for (auto i : AllAllocKinds())
        arenaLists(i).moveCursorToEnd();
}

/* Compacting GC */

bool
GCRuntime::shouldCompact()
{
    // Compact on shrinking GC if enabled, but skip compacting in incremental
    // GCs if we are currently animating.
    return invocationKind == GC_SHRINK && isCompactingGCEnabled() &&
        (!isIncremental || rt->lastAnimationTime + PRMJ_USEC_PER_SEC < PRMJ_Now());
}

bool
GCRuntime::isCompactingGCEnabled() const
{
    return compactingEnabled && TlsContext.get()->compactingDisabledCount == 0;
}

AutoDisableCompactingGC::AutoDisableCompactingGC(JSContext* cx)
  : cx(cx)
{
    ++cx->compactingDisabledCount;
    if (cx->runtime()->gc.isIncrementalGCInProgress() && cx->runtime()->gc.isCompactingGc())
        FinishGC(cx);
}

AutoDisableCompactingGC::~AutoDisableCompactingGC()
{
    MOZ_ASSERT(cx->compactingDisabledCount > 0);
    --cx->compactingDisabledCount;
}

static bool
CanRelocateZone(Zone* zone)
{
    return !zone->isAtomsZone() && !zone->isSelfHostingZone();
}

static const AllocKind AllocKindsToRelocate[] = {
    AllocKind::FUNCTION,
    AllocKind::FUNCTION_EXTENDED,
    AllocKind::OBJECT0,
    AllocKind::OBJECT0_BACKGROUND,
    AllocKind::OBJECT2,
    AllocKind::OBJECT2_BACKGROUND,
    AllocKind::OBJECT4,
    AllocKind::OBJECT4_BACKGROUND,
    AllocKind::OBJECT8,
    AllocKind::OBJECT8_BACKGROUND,
    AllocKind::OBJECT12,
    AllocKind::OBJECT12_BACKGROUND,
    AllocKind::OBJECT16,
    AllocKind::OBJECT16_BACKGROUND,
    AllocKind::SCRIPT,
    AllocKind::LAZY_SCRIPT,
    AllocKind::SCOPE,
    AllocKind::SHAPE,
    AllocKind::ACCESSOR_SHAPE,
    AllocKind::BASE_SHAPE,
    AllocKind::FAT_INLINE_STRING,
    AllocKind::STRING,
    AllocKind::EXTERNAL_STRING,
    AllocKind::FAT_INLINE_ATOM,
    AllocKind::ATOM
};

Arena*
ArenaList::removeRemainingArenas(Arena** arenap)
{
    // This is only ever called to remove arenas that are after the cursor, so
    // we don't need to update it.
#ifdef DEBUG
    for (Arena* arena = *arenap; arena; arena = arena->next)
        MOZ_ASSERT(cursorp_ != &arena->next);
#endif
    Arena* remainingArenas = *arenap;
    *arenap = nullptr;
    check();
    return remainingArenas;
}

static bool
ShouldRelocateAllArenas(JS::gcreason::Reason reason)
{
    return reason == JS::gcreason::DEBUG_GC;
}

/*
 * Choose which arenas to relocate all cells from. Return an arena cursor that
 * can be passed to removeRemainingArenas().
 */
Arena**
ArenaList::pickArenasToRelocate(size_t& arenaTotalOut, size_t& relocTotalOut)
{
    // Relocate the greatest number of arenas such that the number of used cells
    // in relocated arenas is less than or equal to the number of free cells in
    // unrelocated arenas. In other words we only relocate cells we can move
    // into existing arenas, and we choose the least full areans to relocate.
    //
    // This is made easier by the fact that the arena list has been sorted in
    // descending order of number of used cells, so we will always relocate a
    // tail of the arena list. All we need to do is find the point at which to
    // start relocating.

    check();

    if (isCursorAtEnd())
        return nullptr;

    Arena** arenap = cursorp_;     // Next arena to consider for relocation.
    size_t previousFreeCells = 0;  // Count of free cells before arenap.
    size_t followingUsedCells = 0; // Count of used cells after arenap.
    size_t fullArenaCount = 0;     // Number of full arenas (not relocated).
    size_t nonFullArenaCount = 0;  // Number of non-full arenas (considered for relocation).
    size_t arenaIndex = 0;         // Index of the next arena to consider.

    for (Arena* arena = head_; arena != *cursorp_; arena = arena->next)
        fullArenaCount++;

    for (Arena* arena = *cursorp_; arena; arena = arena->next) {
        followingUsedCells += arena->countUsedCells();
        nonFullArenaCount++;
    }

    mozilla::DebugOnly<size_t> lastFreeCells(0);
    size_t cellsPerArena = Arena::thingsPerArena((*arenap)->getAllocKind());

    while (*arenap) {
        Arena* arena = *arenap;
        if (followingUsedCells <= previousFreeCells)
            break;

        size_t freeCells = arena->countFreeCells();
        size_t usedCells = cellsPerArena - freeCells;
        followingUsedCells -= usedCells;
#ifdef DEBUG
        MOZ_ASSERT(freeCells >= lastFreeCells);
        lastFreeCells = freeCells;
#endif
        previousFreeCells += freeCells;
        arenap = &arena->next;
        arenaIndex++;
    }

    size_t relocCount = nonFullArenaCount - arenaIndex;
    MOZ_ASSERT(relocCount < nonFullArenaCount);
    MOZ_ASSERT((relocCount == 0) == (!*arenap));
    arenaTotalOut += fullArenaCount + nonFullArenaCount;
    relocTotalOut += relocCount;

    return arenap;
}

#ifdef DEBUG
inline bool
PtrIsInRange(const void* ptr, const void* start, size_t length)
{
    return uintptr_t(ptr) - uintptr_t(start) < length;
}
#endif

static TenuredCell*
AllocRelocatedCell(Zone* zone, AllocKind thingKind, size_t thingSize)
{
    AutoEnterOOMUnsafeRegion oomUnsafe;
    void* dstAlloc = zone->arenas.allocateFromFreeList(thingKind, thingSize);
    if (!dstAlloc)
        dstAlloc = GCRuntime::refillFreeListInGC(zone, thingKind);
    if (!dstAlloc) {
        // This can only happen in zeal mode or debug builds as we don't
        // otherwise relocate more cells than we have existing free space
        // for.
        oomUnsafe.crash("Could not allocate new arena while compacting");
    }
    return TenuredCell::fromPointer(dstAlloc);
}

static void
RelocateCell(Zone* zone, TenuredCell* src, AllocKind thingKind, size_t thingSize)
{
    JS::AutoSuppressGCAnalysis nogc(TlsContext.get());

    // Allocate a new cell.
    MOZ_ASSERT(zone == src->zone());
    TenuredCell* dst = AllocRelocatedCell(zone, thingKind, thingSize);

    // Copy source cell contents to destination.
    memcpy(dst, src, thingSize);

    // Move any uid attached to the object.
    src->zone()->transferUniqueId(dst, src);

    if (IsObjectAllocKind(thingKind)) {
        JSObject* srcObj = static_cast<JSObject*>(static_cast<Cell*>(src));
        JSObject* dstObj = static_cast<JSObject*>(static_cast<Cell*>(dst));

        if (srcObj->isNative()) {
            NativeObject* srcNative = &srcObj->as<NativeObject>();
            NativeObject* dstNative = &dstObj->as<NativeObject>();

            // Fixup the pointer to inline object elements if necessary.
            if (srcNative->hasFixedElements())
                dstNative->setFixedElements();

            // For copy-on-write objects that own their elements, fix up the
            // owner pointer to point to the relocated object.
            if (srcNative->denseElementsAreCopyOnWrite()) {
                GCPtrNativeObject& owner = dstNative->getElementsHeader()->ownerObject();
                if (owner == srcNative)
                    owner = dstNative;
            }
        }

        // Call object moved hook if present.
        if (JSObjectMovedOp op = srcObj->getClass()->extObjectMovedOp())
            op(dstObj, srcObj);

        MOZ_ASSERT_IF(dstObj->isNative(),
                      !PtrIsInRange((const Value*)dstObj->as<NativeObject>().getDenseElements(),
                                    src, thingSize));
    }

    // Copy the mark bits.
    dst->copyMarkBitsFrom(src);

    // Mark source cell as forwarded and leave a pointer to the destination.
    RelocationOverlay* overlay = RelocationOverlay::fromCell(src);
    overlay->forwardTo(dst);
}

static void
RelocateArena(Arena* arena, SliceBudget& sliceBudget)
{
    MOZ_ASSERT(arena->allocated());
    MOZ_ASSERT(!arena->hasDelayedMarking);
    MOZ_ASSERT(!arena->markOverflow);
    MOZ_ASSERT(!arena->allocatedDuringIncremental);
    MOZ_ASSERT(arena->bufferedCells()->isEmpty());

    Zone* zone = arena->zone;

    AllocKind thingKind = arena->getAllocKind();
    size_t thingSize = arena->getThingSize();

    for (ArenaCellIterUnderGC i(arena); !i.done(); i.next()) {
        RelocateCell(zone, i.getCell(), thingKind, thingSize);
        sliceBudget.step();
    }

#ifdef DEBUG
    for (ArenaCellIterUnderGC i(arena); !i.done(); i.next()) {
        TenuredCell* src = i.getCell();
        MOZ_ASSERT(RelocationOverlay::isCellForwarded(src));
        TenuredCell* dest = Forwarded(src);
        MOZ_ASSERT(src->isMarked(BLACK) == dest->isMarked(BLACK));
        MOZ_ASSERT(src->isMarked(GRAY) == dest->isMarked(GRAY));
    }
#endif
}

static inline bool
ShouldProtectRelocatedArenas(JS::gcreason::Reason reason)
{
    // For zeal mode collections we don't release the relocated arenas
    // immediately. Instead we protect them and keep them around until the next
    // collection so we can catch any stray accesses to them.
#ifdef DEBUG
    return reason == JS::gcreason::DEBUG_GC;
#else
    return false;
#endif
}

/*
 * Relocate all arenas identified by pickArenasToRelocate: for each arena,
 * relocate each cell within it, then add it to a list of relocated arenas.
 */
Arena*
ArenaList::relocateArenas(Arena* toRelocate, Arena* relocated, SliceBudget& sliceBudget,
                          gcstats::Statistics& stats)
{
    check();

    while (Arena* arena = toRelocate) {
        toRelocate = arena->next;
        RelocateArena(arena, sliceBudget);
        // Prepend to list of relocated arenas
        arena->next = relocated;
        relocated = arena;
        stats.count(gcstats::STAT_ARENA_RELOCATED);
    }

    check();

    return relocated;
}

// Skip compacting zones unless we can free a certain proportion of their GC
// heap memory.
static const double MIN_ZONE_RECLAIM_PERCENT = 2.0;

static bool
ShouldRelocateZone(size_t arenaCount, size_t relocCount, JS::gcreason::Reason reason)
{
    if (relocCount == 0)
        return false;

    if (IsOOMReason(reason))
        return true;

    return (relocCount * 100.0) / arenaCount >= MIN_ZONE_RECLAIM_PERCENT;
}

bool
ArenaLists::relocateArenas(Zone* zone, Arena*& relocatedListOut, JS::gcreason::Reason reason,
                           SliceBudget& sliceBudget, gcstats::Statistics& stats)
{
    // This is only called from the active thread while we are doing a GC, so
    // there is no need to lock.
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime_));
    MOZ_ASSERT(runtime_->gc.isHeapCompacting());
    MOZ_ASSERT(!runtime_->gc.isBackgroundSweeping());

    // Clear all the free lists.
    purge();

    if (ShouldRelocateAllArenas(reason)) {
        zone->prepareForCompacting();
        for (auto kind : AllocKindsToRelocate) {
            ArenaList& al = arenaLists(kind);
            Arena* allArenas = al.head();
            al.clear();
            relocatedListOut = al.relocateArenas(allArenas, relocatedListOut, sliceBudget, stats);
        }
    } else {
        size_t arenaCount = 0;
        size_t relocCount = 0;
        AllAllocKindArray<Arena**> toRelocate;

        for (auto kind : AllocKindsToRelocate)
            toRelocate[kind] = arenaLists(kind).pickArenasToRelocate(arenaCount, relocCount);

        if (!ShouldRelocateZone(arenaCount, relocCount, reason))
            return false;

        zone->prepareForCompacting();
        for (auto kind : AllocKindsToRelocate) {
            if (toRelocate[kind]) {
                ArenaList& al = arenaLists(kind);
                Arena* arenas = al.removeRemainingArenas(toRelocate[kind]);
                relocatedListOut = al.relocateArenas(arenas, relocatedListOut, sliceBudget, stats);
            }
        }
    }

    return true;
}

bool
GCRuntime::relocateArenas(Zone* zone, JS::gcreason::Reason reason, Arena*& relocatedListOut,
                          SliceBudget& sliceBudget)
{
    gcstats::AutoPhase ap(stats(), gcstats::PHASE_COMPACT_MOVE);

    MOZ_ASSERT(!zone->isPreservingCode());
    MOZ_ASSERT(CanRelocateZone(zone));

    js::CancelOffThreadIonCompile(rt, JS::Zone::Compact);

    if (!zone->arenas.relocateArenas(zone, relocatedListOut, reason, sliceBudget, stats()))
        return false;

#ifdef DEBUG
    // Check that we did as much compaction as we should have. There
    // should always be less than one arena's worth of free cells.
    for (auto i : AllocKindsToRelocate) {
        ArenaList& al = zone->arenas.arenaLists(i);
        size_t freeCells = 0;
        for (Arena* arena = al.arenaAfterCursor(); arena; arena = arena->next)
            freeCells += arena->countFreeCells();
        MOZ_ASSERT(freeCells < Arena::thingsPerArena(i));
    }
#endif

    return true;
}

void
MovingTracer::onObjectEdge(JSObject** objp)
{
    JSObject* obj = *objp;
    if (obj->runtimeFromAnyThread() == runtime() && IsForwarded(obj))
        *objp = Forwarded(obj);
}

void
MovingTracer::onShapeEdge(Shape** shapep)
{
    Shape* shape = *shapep;
    if (shape->runtimeFromAnyThread() == runtime() && IsForwarded(shape))
        *shapep = Forwarded(shape);
}

void
MovingTracer::onStringEdge(JSString** stringp)
{
    JSString* string = *stringp;
    if (string->runtimeFromAnyThread() == runtime() && IsForwarded(string))
        *stringp = Forwarded(string);
}

void
MovingTracer::onScriptEdge(JSScript** scriptp)
{
    JSScript* script = *scriptp;
    if (script->runtimeFromAnyThread() == runtime() && IsForwarded(script))
        *scriptp = Forwarded(script);
}

void
MovingTracer::onLazyScriptEdge(LazyScript** lazyp)
{
    LazyScript* lazy = *lazyp;
    if (lazy->runtimeFromAnyThread() == runtime() && IsForwarded(lazy))
        *lazyp = Forwarded(lazy);
}

void
MovingTracer::onBaseShapeEdge(BaseShape** basep)
{
    BaseShape* base = *basep;
    if (base->runtimeFromAnyThread() == runtime() && IsForwarded(base))
        *basep = Forwarded(base);
}

void
MovingTracer::onScopeEdge(Scope** scopep)
{
    Scope* scope = *scopep;
    if (scope->runtimeFromAnyThread() == runtime() && IsForwarded(scope))
        *scopep = Forwarded(scope);
}

void
Zone::prepareForCompacting()
{
    FreeOp* fop = runtimeFromActiveCooperatingThread()->defaultFreeOp();
    discardJitCode(fop);
}

void
GCRuntime::sweepTypesAfterCompacting(Zone* zone)
{
    FreeOp* fop = rt->defaultFreeOp();
    zone->beginSweepTypes(fop, rt->gc.releaseObservedTypes && !zone->isPreservingCode());

    AutoClearTypeInferenceStateOnOOM oom(zone);

    for (auto script = zone->cellIter<JSScript>(); !script.done(); script.next())
        script->maybeSweepTypes(&oom);
    for (auto group = zone->cellIter<ObjectGroup>(); !group.done(); group.next())
        group->maybeSweep(&oom);

    zone->types.endSweep(rt);
}

void
GCRuntime::sweepZoneAfterCompacting(Zone* zone)
{
    MOZ_ASSERT(zone->isCollecting());
    FreeOp* fop = rt->defaultFreeOp();
    sweepTypesAfterCompacting(zone);
    zone->sweepBreakpoints(fop);
    zone->sweepWeakMaps();
    for (auto* cache : zone->weakCaches())
        cache->sweep();

    for (CompartmentsInZoneIter c(zone); !c.done(); c.next()) {
        c->objectGroups.sweep(fop);
        c->sweepRegExps();
        c->sweepSavedStacks();
        c->sweepGlobalObject(fop);
        c->sweepSelfHostingScriptSource();
        c->sweepDebugEnvironments();
        c->sweepJitCompartment(fop);
        c->sweepNativeIterators();
        c->sweepTemplateObjects();
    }
}

template <typename T>
static inline void
UpdateCellPointers(MovingTracer* trc, T* cell)
{
    cell->fixupAfterMovingGC();
    cell->traceChildren(trc);
}

template <typename T>
static void
UpdateArenaPointersTyped(MovingTracer* trc, Arena* arena, JS::TraceKind traceKind)
{
    for (ArenaCellIterUnderGC i(arena); !i.done(); i.next())
        UpdateCellPointers(trc, reinterpret_cast<T*>(i.getCell()));
}

/*
 * Update the internal pointers for all cells in an arena.
 */
static void
UpdateArenaPointers(MovingTracer* trc, Arena* arena)
{
    AllocKind kind = arena->getAllocKind();

    switch (kind) {
#define EXPAND_CASE(allocKind, traceKind, type, sizedType) \
      case AllocKind::allocKind: \
        UpdateArenaPointersTyped<type>(trc, arena, JS::TraceKind::traceKind); \
        return;
FOR_EACH_ALLOCKIND(EXPAND_CASE)
#undef EXPAND_CASE

      default:
        MOZ_CRASH("Invalid alloc kind for UpdateArenaPointers");
    }
}

namespace js {
namespace gc {

struct ArenaListSegment
{
    Arena* begin;
    Arena* end;
};

struct ArenasToUpdate
{
    ArenasToUpdate(Zone* zone, AllocKinds kinds);
    bool done() { return kind == AllocKind::LIMIT; }
    ArenaListSegment getArenasToUpdate(AutoLockHelperThreadState& lock, unsigned maxLength);

  private:
    AllocKinds kinds;  // Selects which thing kinds to update
    Zone* zone;        // Zone to process
    AllocKind kind;    // Current alloc kind to process
    Arena* arena;      // Next arena to process

    AllocKind nextAllocKind(AllocKind i) { return AllocKind(uint8_t(i) + 1); }
    bool shouldProcessKind(AllocKind kind);
    Arena* next(AutoLockHelperThreadState& lock);
};

ArenasToUpdate::ArenasToUpdate(Zone* zone, AllocKinds kinds)
  : kinds(kinds), zone(zone), kind(AllocKind::FIRST), arena(nullptr)
{
    MOZ_ASSERT(zone->isGCCompacting());
}

Arena*
ArenasToUpdate::next(AutoLockHelperThreadState& lock)
{
    // Find the next arena to update.
    //
    // This iterates through the GC thing kinds filtered by shouldProcessKind(),
    // and then through thea arenas of that kind.  All state is held in the
    // object and we just return when we find an arena.

    for (; kind < AllocKind::LIMIT; kind = nextAllocKind(kind)) {
        if (kinds.contains(kind)) {
            if (!arena)
                arena = zone->arenas.getFirstArena(kind);
            else
                arena = arena->next;
            if (arena)
                return arena;
        }
    }

    MOZ_ASSERT(!arena);
    MOZ_ASSERT(done());
    return nullptr;
}

ArenaListSegment
ArenasToUpdate::getArenasToUpdate(AutoLockHelperThreadState& lock, unsigned maxLength)
{
    Arena* begin = next(lock);
    if (!begin)
        return { nullptr, nullptr };

    Arena* last = begin;
    unsigned count = 1;
    while (last->next && count < maxLength) {
        last = last->next;
        count++;
    }

    arena = last;
    return { begin, last->next };
}

struct UpdatePointersTask : public GCParallelTask
{
    // Maximum number of arenas to update in one block.
#ifdef DEBUG
    static const unsigned MaxArenasToProcess = 16;
#else
    static const unsigned MaxArenasToProcess = 256;
#endif

    UpdatePointersTask(JSRuntime* rt, ArenasToUpdate* source, AutoLockHelperThreadState& lock)
      : GCParallelTask(rt), source_(source)
    {
        arenas_.begin = nullptr;
        arenas_.end = nullptr;
    }

    ~UpdatePointersTask() override { join(); }

  private:
    ArenasToUpdate* source_;
    ArenaListSegment arenas_;

    virtual void run() override;
    bool getArenasToUpdate();
    void updateArenas();
};

bool
UpdatePointersTask::getArenasToUpdate()
{
    AutoLockHelperThreadState lock;
    arenas_ = source_->getArenasToUpdate(lock, MaxArenasToProcess);
    return arenas_.begin != nullptr;
}

void
UpdatePointersTask::updateArenas()
{
    MovingTracer trc(runtime());
    for (Arena* arena = arenas_.begin; arena != arenas_.end; arena = arena->next)
        UpdateArenaPointers(&trc, arena);
}

/* virtual */ void
UpdatePointersTask::run()
{
    // These checks assert when run in parallel.
    AutoDisableProxyCheck noProxyCheck;

    while (getArenasToUpdate())
        updateArenas();
}

} // namespace gc
} // namespace js

static const size_t MinCellUpdateBackgroundTasks = 2;
static const size_t MaxCellUpdateBackgroundTasks = 8;

static size_t
CellUpdateBackgroundTaskCount()
{
    if (!CanUseExtraThreads())
        return 0;

    size_t targetTaskCount = HelperThreadState().cpuCount / 2;
    return Min(Max(targetTaskCount, MinCellUpdateBackgroundTasks), MaxCellUpdateBackgroundTasks);
}

static bool
CanUpdateKindInBackground(AllocKind kind) {
    // We try to update as many GC things in parallel as we can, but there are
    // kinds for which this might not be safe:
    //  - we assume JSObjects that are foreground finalized are not safe to
    //    update in parallel
    //  - updating a shape touches child shapes in fixupShapeTreeAfterMovingGC()
    if (!js::gc::IsBackgroundFinalized(kind) || IsShapeAllocKind(kind))
        return false;

    return true;
}

static AllocKinds
ForegroundUpdateKinds(AllocKinds kinds)
{
    AllocKinds result;
    for (AllocKind kind : kinds) {
        if (!CanUpdateKindInBackground(kind))
            result += kind;
    }
    return result;
}

void
GCRuntime::updateTypeDescrObjects(MovingTracer* trc, Zone* zone)
{
    zone->typeDescrObjects().sweep();
    for (auto r = zone->typeDescrObjects().all(); !r.empty(); r.popFront())
        UpdateCellPointers(trc, r.front());
}

void
GCRuntime::updateCellPointers(MovingTracer* trc, Zone* zone, AllocKinds kinds, size_t bgTaskCount)
{
    AllocKinds fgKinds = bgTaskCount == 0 ? kinds : ForegroundUpdateKinds(kinds);
    AllocKinds bgKinds = kinds - fgKinds;

    ArenasToUpdate fgArenas(zone, fgKinds);
    ArenasToUpdate bgArenas(zone, bgKinds);
    Maybe<UpdatePointersTask> fgTask;
    Maybe<UpdatePointersTask> bgTasks[MaxCellUpdateBackgroundTasks];

    size_t tasksStarted = 0;

    {
        AutoLockHelperThreadState lock;

        fgTask.emplace(rt, &fgArenas, lock);

        for (size_t i = 0; i < bgTaskCount && !bgArenas.done(); i++) {
            bgTasks[i].emplace(rt, &bgArenas, lock);
            startTask(*bgTasks[i], gcstats::PHASE_COMPACT_UPDATE_CELLS, lock);
            tasksStarted = i;
        }
    }

    fgTask->runFromActiveCooperatingThread(rt);

    {
        AutoLockHelperThreadState lock;

        for (size_t i = 0; i < tasksStarted; i++)
            joinTask(*bgTasks[i], gcstats::PHASE_COMPACT_UPDATE_CELLS, lock);
    }
}

// After cells have been relocated any pointers to a cell's old locations must
// be updated to point to the new location.  This happens by iterating through
// all cells in heap and tracing their children (non-recursively) to update
// them.
//
// This is complicated by the fact that updating a GC thing sometimes depends on
// making use of other GC things.  After a moving GC these things may not be in
// a valid state since they may contain pointers which have not been updated
// yet.
//
// The main dependencies are:
//
//   - Updating a JSObject makes use of its shape
//   - Updating a typed object makes use of its type descriptor object
//
// This means we require at least three phases for update:
//
//  1) shapes
//  2) typed object type descriptor objects
//  3) all other objects
//
// Since we want to minimize the number of phases, we put everything else into
// the first phase and label it the 'misc' phase.

static const AllocKinds UpdatePhaseMisc {
    AllocKind::SCRIPT,
    AllocKind::LAZY_SCRIPT,
    AllocKind::BASE_SHAPE,
    AllocKind::SHAPE,
    AllocKind::ACCESSOR_SHAPE,
    AllocKind::OBJECT_GROUP,
    AllocKind::STRING,
    AllocKind::JITCODE,
    AllocKind::SCOPE
};

static const AllocKinds UpdatePhaseObjects {
    AllocKind::FUNCTION,
    AllocKind::FUNCTION_EXTENDED,
    AllocKind::OBJECT0,
    AllocKind::OBJECT0_BACKGROUND,
    AllocKind::OBJECT2,
    AllocKind::OBJECT2_BACKGROUND,
    AllocKind::OBJECT4,
    AllocKind::OBJECT4_BACKGROUND,
    AllocKind::OBJECT8,
    AllocKind::OBJECT8_BACKGROUND,
    AllocKind::OBJECT12,
    AllocKind::OBJECT12_BACKGROUND,
    AllocKind::OBJECT16,
    AllocKind::OBJECT16_BACKGROUND
};

void
GCRuntime::updateAllCellPointers(MovingTracer* trc, Zone* zone)
{
    size_t bgTaskCount = CellUpdateBackgroundTaskCount();

    updateCellPointers(trc, zone, UpdatePhaseMisc, bgTaskCount);

    // Update TypeDescrs before all other objects as typed objects access these
    // objects when we trace them.
    updateTypeDescrObjects(trc, zone);

    updateCellPointers(trc, zone, UpdatePhaseObjects, bgTaskCount);
}

/*
 * Update pointers to relocated cells in a single zone by doing a traversal of
 * that zone's arenas and calling per-zone sweep hooks.
 *
 * The latter is necessary to update weak references which are not marked as
 * part of the traversal.
 */
void
GCRuntime::updateZonePointersToRelocatedCells(Zone* zone, AutoLockForExclusiveAccess& lock)
{
    MOZ_ASSERT(!rt->isBeingDestroyed());
    MOZ_ASSERT(zone->isGCCompacting());

    gcstats::AutoPhase ap(stats(), gcstats::PHASE_COMPACT_UPDATE);
    MovingTracer trc(rt);

    zone->fixupAfterMovingGC();

    // Fixup compartment global pointers as these get accessed during marking.
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next())
        comp->fixupAfterMovingGC();

    // Iterate through all cells that can contain relocatable pointers to update
    // them. Since updating each cell is independent we try to parallelize this
    // as much as possible.
    updateAllCellPointers(&trc, zone);

    // Mark roots to update them.
    {
        gcstats::AutoPhase ap(stats(), gcstats::PHASE_MARK_ROOTS);

        WeakMapBase::traceZone(zone, &trc);
        for (CompartmentsInZoneIter c(zone); !c.done(); c.next()) {
            c->trace(&trc);
            if (c->watchpointMap)
                c->watchpointMap->trace(&trc);
        }
    }

    // Sweep everything to fix up weak pointers.
    rt->gc.sweepZoneAfterCompacting(zone);

    // Call callbacks to get the rest of the system to fixup other untraced pointers.
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next())
        callWeakPointerCompartmentCallbacks(comp);
    if (rt->sweepZoneCallback)
        rt->sweepZoneCallback(zone);
}

/*
 * Update runtime-wide pointers to relocated cells.
 */
void
GCRuntime::updateRuntimePointersToRelocatedCells(AutoLockForExclusiveAccess& lock)
{
    MOZ_ASSERT(!rt->isBeingDestroyed());

    gcstats::AutoPhase ap1(stats(), gcstats::PHASE_COMPACT_UPDATE);
    MovingTracer trc(rt);

    JSCompartment::fixupCrossCompartmentWrappersAfterMovingGC(&trc);

    rt->geckoProfiler().fixupStringsMapAfterMovingGC();

    traceRuntimeForMajorGC(&trc, lock);

    // Mark roots to update them.
    {
        gcstats::AutoPhase ap2(stats(), gcstats::PHASE_MARK_ROOTS);
        Debugger::traceAllForMovingGC(&trc);
        Debugger::traceIncomingCrossCompartmentEdges(&trc);

        // Mark all gray roots, making sure we call the trace callback to get the
        // current set.
        if (JSTraceDataOp op = grayRootTracer.op)
            (*op)(&trc, grayRootTracer.data);
    }

    // Sweep everything to fix up weak pointers.
    WatchpointMap::sweepAll(rt);
    Debugger::sweepAll(rt->defaultFreeOp());
    jit::JitRuntime::SweepJitcodeGlobalTable(rt);

    // Type inference may put more blocks here to free.
    blocksToFreeAfterSweeping.ref().freeAll();

    // Call callbacks to get the rest of the system to fixup other untraced pointers.
    callWeakPointerZoneGroupCallbacks();
}

void
GCRuntime::protectAndHoldArenas(Arena* arenaList)
{
    for (Arena* arena = arenaList; arena; ) {
        MOZ_ASSERT(arena->allocated());
        Arena* next = arena->next;
        if (!next) {
            // Prepend to hold list before we protect the memory.
            arena->next = relocatedArenasToRelease;
            relocatedArenasToRelease = arenaList;
        }
        ProtectPages(arena, ArenaSize);
        arena = next;
    }
}

void
GCRuntime::unprotectHeldRelocatedArenas()
{
    for (Arena* arena = relocatedArenasToRelease; arena; arena = arena->next) {
        UnprotectPages(arena, ArenaSize);
        MOZ_ASSERT(arena->allocated());
    }
}

void
GCRuntime::releaseRelocatedArenas(Arena* arenaList)
{
    AutoLockGC lock(rt);
    releaseRelocatedArenasWithoutUnlocking(arenaList, lock);
}

void
GCRuntime::releaseRelocatedArenasWithoutUnlocking(Arena* arenaList, const AutoLockGC& lock)
{
    // Release the relocated arenas, now containing only forwarding pointers
    unsigned count = 0;
    while (arenaList) {
        Arena* arena = arenaList;
        arenaList = arenaList->next;

        // Clear the mark bits
        arena->unmarkAll();

        // Mark arena as empty
        arena->setAsFullyUnused();

#if defined(JS_CRASH_DIAGNOSTICS) || defined(JS_GC_ZEAL)
        JS_POISON(reinterpret_cast<void*>(arena->thingsStart()),
                  JS_MOVED_TENURED_PATTERN, arena->getThingsSpan());
#endif

        releaseArena(arena, lock);
        ++count;
    }
}

// In debug mode we don't always release relocated arenas straight away.
// Sometimes protect them instead and hold onto them until the next GC sweep
// phase to catch any pointers to them that didn't get forwarded.

void
GCRuntime::releaseHeldRelocatedArenas()
{
#ifdef DEBUG
    unprotectHeldRelocatedArenas();
    Arena* arenas = relocatedArenasToRelease;
    relocatedArenasToRelease = nullptr;
    releaseRelocatedArenas(arenas);
#endif
}

void
GCRuntime::releaseHeldRelocatedArenasWithoutUnlocking(const AutoLockGC& lock)
{
#ifdef DEBUG
    unprotectHeldRelocatedArenas();
    releaseRelocatedArenasWithoutUnlocking(relocatedArenasToRelease, lock);
    relocatedArenasToRelease = nullptr;
#endif
}

ArenaLists::ArenaLists(JSRuntime* rt, ZoneGroup* group)
  : runtime_(rt),
    freeLists_(group),
    arenaLists_(group),
    backgroundFinalizeState_(),
    arenaListsToSweep_(),
    incrementalSweptArenaKind(group, AllocKind::LIMIT),
    incrementalSweptArenas(group),
    gcShapeArenasToUpdate(group, nullptr),
    gcAccessorShapeArenasToUpdate(group, nullptr),
    gcScriptArenasToUpdate(group, nullptr),
    gcObjectGroupArenasToUpdate(group, nullptr),
    savedObjectArenas_(group),
    savedEmptyObjectArenas(group, nullptr)
{
    for (auto i : AllAllocKinds())
        freeLists(i) = &placeholder;
    for (auto i : AllAllocKinds())
        backgroundFinalizeState(i) = BFS_DONE;
    for (auto i : AllAllocKinds())
        arenaListsToSweep(i) = nullptr;
}

void
ReleaseArenaList(JSRuntime* rt, Arena* arena, const AutoLockGC& lock)
{
    Arena* next;
    for (; arena; arena = next) {
        next = arena->next;
        rt->gc.releaseArena(arena, lock);
    }
}

ArenaLists::~ArenaLists()
{
    AutoLockGC lock(runtime_);

    for (auto i : AllAllocKinds()) {
        /*
         * We can only call this during the shutdown after the last GC when
         * the background finalization is disabled.
         */
        MOZ_ASSERT(backgroundFinalizeState(i) == BFS_DONE);
        ReleaseArenaList(runtime_, arenaLists(i).head(), lock);
    }
    ReleaseArenaList(runtime_, incrementalSweptArenas.ref().head(), lock);

    for (auto i : ObjectAllocKinds())
        ReleaseArenaList(runtime_, savedObjectArenas(i).head(), lock);
    ReleaseArenaList(runtime_, savedEmptyObjectArenas, lock);
}

void
ArenaLists::finalizeNow(FreeOp* fop, AllocKind thingKind, Arena** empty)
{
    MOZ_ASSERT(!IsBackgroundFinalized(thingKind));
    MOZ_ASSERT(backgroundFinalizeState(thingKind) == BFS_DONE);
    MOZ_ASSERT(empty);

    Arena* arenas = arenaLists(thingKind).head();
    if (!arenas)
        return;
    arenaLists(thingKind).clear();

    size_t thingsPerArena = Arena::thingsPerArena(thingKind);
    SortedArenaList finalizedSorted(thingsPerArena);

    auto unlimited = SliceBudget::unlimited();
    FinalizeArenas(fop, &arenas, finalizedSorted, thingKind, unlimited, KEEP_ARENAS);
    MOZ_ASSERT(!arenas);

    finalizedSorted.extractEmpty(empty);
    arenaLists(thingKind) = finalizedSorted.toArenaList();
}

void
ArenaLists::queueForForegroundSweep(FreeOp* fop, const FinalizePhase& phase)
{
    gcstats::AutoPhase ap(fop->runtime()->gc.stats(), phase.statsPhase);
    for (auto kind : phase.kinds)
        queueForForegroundSweep(fop, kind);
}

void
ArenaLists::queueForForegroundSweep(FreeOp* fop, AllocKind thingKind)
{
    MOZ_ASSERT(!IsBackgroundFinalized(thingKind));
    MOZ_ASSERT(backgroundFinalizeState(thingKind) == BFS_DONE);
    MOZ_ASSERT(!arenaListsToSweep(thingKind));

    arenaListsToSweep(thingKind) = arenaLists(thingKind).head();
    arenaLists(thingKind).clear();
}

void
ArenaLists::queueForBackgroundSweep(FreeOp* fop, const FinalizePhase& phase)
{
    gcstats::AutoPhase ap(fop->runtime()->gc.stats(), phase.statsPhase);
    for (auto kind : phase.kinds)
        queueForBackgroundSweep(fop, kind);
}

inline void
ArenaLists::queueForBackgroundSweep(FreeOp* fop, AllocKind thingKind)
{
    MOZ_ASSERT(IsBackgroundFinalized(thingKind));

    ArenaList* al = &arenaLists(thingKind);
    if (al->isEmpty()) {
        MOZ_ASSERT(backgroundFinalizeState(thingKind) == BFS_DONE);
        return;
    }

    MOZ_ASSERT(backgroundFinalizeState(thingKind) == BFS_DONE);

    arenaListsToSweep(thingKind) = al->head();
    al->clear();
    backgroundFinalizeState(thingKind) = BFS_RUN;
}

/*static*/ void
ArenaLists::backgroundFinalize(FreeOp* fop, Arena* listHead, Arena** empty)
{
    MOZ_ASSERT(listHead);
    MOZ_ASSERT(empty);

    AllocKind thingKind = listHead->getAllocKind();
    Zone* zone = listHead->zone;

    size_t thingsPerArena = Arena::thingsPerArena(thingKind);
    SortedArenaList finalizedSorted(thingsPerArena);

    auto unlimited = SliceBudget::unlimited();
    FinalizeArenas(fop, &listHead, finalizedSorted, thingKind, unlimited, KEEP_ARENAS);
    MOZ_ASSERT(!listHead);

    finalizedSorted.extractEmpty(empty);

    // When arenas are queued for background finalization, all arenas are moved
    // to arenaListsToSweep[], leaving the arenaLists[] empty. However, new
    // arenas may be allocated before background finalization finishes; now that
    // finalization is complete, we want to merge these lists back together.
    ArenaLists* lists = &zone->arenas;
    ArenaList* al = &lists->arenaLists(thingKind);

    // Flatten |finalizedSorted| into a regular ArenaList.
    ArenaList finalized = finalizedSorted.toArenaList();

    // We must take the GC lock to be able to safely modify the ArenaList;
    // however, this does not by itself make the changes visible to all threads,
    // as not all threads take the GC lock to read the ArenaLists.
    // That safety is provided by the ReleaseAcquire memory ordering of the
    // background finalize state, which we explicitly set as the final step.
    {
        AutoLockGC lock(lists->runtime_);
        MOZ_ASSERT(lists->backgroundFinalizeState(thingKind) == BFS_RUN);

        // Join |al| and |finalized| into a single list.
        *al = finalized.insertListWithCursorAtEnd(*al);

        lists->arenaListsToSweep(thingKind) = nullptr;
    }

    lists->backgroundFinalizeState(thingKind) = BFS_DONE;
}

void
ArenaLists::queueForegroundObjectsForSweep(FreeOp* fop)
{
    gcstats::AutoPhase ap(fop->runtime()->gc.stats(), gcstats::PHASE_SWEEP_OBJECT);

#ifdef DEBUG
    for (auto i : ObjectAllocKinds())
        MOZ_ASSERT(savedObjectArenas(i).isEmpty());
    MOZ_ASSERT(savedEmptyObjectArenas == nullptr);
#endif

    // Foreground finalized objects must be finalized at the beginning of the
    // sweep phase, before control can return to the mutator. Otherwise,
    // mutator behavior can resurrect certain objects whose references would
    // otherwise have been erased by the finalizer.
    finalizeNow(fop, AllocKind::OBJECT0, &savedEmptyObjectArenas.ref());
    finalizeNow(fop, AllocKind::OBJECT2, &savedEmptyObjectArenas.ref());
    finalizeNow(fop, AllocKind::OBJECT4, &savedEmptyObjectArenas.ref());
    finalizeNow(fop, AllocKind::OBJECT8, &savedEmptyObjectArenas.ref());
    finalizeNow(fop, AllocKind::OBJECT12, &savedEmptyObjectArenas.ref());
    finalizeNow(fop, AllocKind::OBJECT16, &savedEmptyObjectArenas.ref());

    // Prevent the arenas from having new objects allocated into them. We need
    // to know which objects are marked while we incrementally sweep dead
    // references from type information.
    savedObjectArenas(AllocKind::OBJECT0) = arenaLists(AllocKind::OBJECT0).copyAndClear();
    savedObjectArenas(AllocKind::OBJECT2) = arenaLists(AllocKind::OBJECT2).copyAndClear();
    savedObjectArenas(AllocKind::OBJECT4) = arenaLists(AllocKind::OBJECT4).copyAndClear();
    savedObjectArenas(AllocKind::OBJECT8) = arenaLists(AllocKind::OBJECT8).copyAndClear();
    savedObjectArenas(AllocKind::OBJECT12) = arenaLists(AllocKind::OBJECT12).copyAndClear();
    savedObjectArenas(AllocKind::OBJECT16) = arenaLists(AllocKind::OBJECT16).copyAndClear();
}

void
ArenaLists::mergeForegroundSweptObjectArenas()
{
    AutoLockGC lock(runtime_);
    ReleaseArenaList(runtime_, savedEmptyObjectArenas, lock);
    savedEmptyObjectArenas = nullptr;

    mergeSweptArenas(AllocKind::OBJECT0);
    mergeSweptArenas(AllocKind::OBJECT2);
    mergeSweptArenas(AllocKind::OBJECT4);
    mergeSweptArenas(AllocKind::OBJECT8);
    mergeSweptArenas(AllocKind::OBJECT12);
    mergeSweptArenas(AllocKind::OBJECT16);
}

inline void
ArenaLists::mergeSweptArenas(AllocKind thingKind)
{
    ArenaList* al = &arenaLists(thingKind);
    ArenaList* saved = &savedObjectArenas(thingKind);

    *al = saved->insertListWithCursorAtEnd(*al);
    saved->clear();
}

void
ArenaLists::queueForegroundThingsForSweep(FreeOp* fop)
{
    gcShapeArenasToUpdate = arenaListsToSweep(AllocKind::SHAPE);
    gcAccessorShapeArenasToUpdate = arenaListsToSweep(AllocKind::ACCESSOR_SHAPE);
    gcObjectGroupArenasToUpdate = arenaListsToSweep(AllocKind::OBJECT_GROUP);
    gcScriptArenasToUpdate = arenaListsToSweep(AllocKind::SCRIPT);
}

SliceBudget::SliceBudget()
  : timeBudget(UnlimitedTimeBudget), workBudget(UnlimitedWorkBudget)
{
    makeUnlimited();
}

SliceBudget::SliceBudget(TimeBudget time)
  : timeBudget(time), workBudget(UnlimitedWorkBudget)
{
    if (time.budget < 0) {
        makeUnlimited();
    } else {
        // Note: TimeBudget(0) is equivalent to WorkBudget(CounterReset).
        deadline = PRMJ_Now() + time.budget * PRMJ_USEC_PER_MSEC;
        counter = CounterReset;
    }
}

SliceBudget::SliceBudget(WorkBudget work)
  : timeBudget(UnlimitedTimeBudget), workBudget(work)
{
    if (work.budget < 0) {
        makeUnlimited();
    } else {
        deadline = 0;
        counter = work.budget;
    }
}

int
SliceBudget::describe(char* buffer, size_t maxlen) const
{
    if (isUnlimited())
        return snprintf(buffer, maxlen, "unlimited");
    else if (isWorkBudget())
        return snprintf(buffer, maxlen, "work(%" PRId64 ")", workBudget.budget);
    else
        return snprintf(buffer, maxlen, "%" PRId64 "ms", timeBudget.budget);
}

bool
SliceBudget::checkOverBudget()
{
    bool over = PRMJ_Now() >= deadline;
    if (!over)
        counter = CounterReset;
    return over;
}

void
GCRuntime::requestMajorGC(JS::gcreason::Reason reason)
{
    MOZ_ASSERT(!CurrentThreadIsPerformingGC());

    if (majorGCRequested())
        return;

    majorGCTriggerReason = reason;

    // There's no need to use RequestInterruptUrgent here. It's slower because
    // it has to interrupt (looping) Ion code, but loops in Ion code that
    // affect GC will have an explicit interrupt check.
    TlsContext.get()->requestInterrupt(JSContext::RequestInterruptCanWait);
}

void
Nursery::requestMinorGC(JS::gcreason::Reason reason) const
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime()));
    MOZ_ASSERT(!CurrentThreadIsPerformingGC());

    if (minorGCRequested())
        return;

    minorGCTriggerReason_ = reason;

    // See comment in requestMajorGC.
    TlsContext.get()->requestInterrupt(JSContext::RequestInterruptCanWait);
}

bool
GCRuntime::triggerGC(JS::gcreason::Reason reason)
{
    /*
     * Don't trigger GCs if this is being called off the active thread from
     * onTooMuchMalloc().
     */
    if (!CurrentThreadCanAccessRuntime(rt))
        return false;

    /* GC is already running. */
    if (JS::CurrentThreadIsHeapCollecting())
        return false;

    JS::PrepareForFullGC(rt->activeContextFromOwnThread());
    requestMajorGC(reason);
    return true;
}

void
GCRuntime::maybeAllocTriggerZoneGC(Zone* zone, const AutoLockGC& lock)
{
    size_t usedBytes = zone->usage.gcBytes();
    size_t thresholdBytes = zone->threshold.gcTriggerBytes();
    size_t igcThresholdBytes = thresholdBytes * tunables.zoneAllocThresholdFactor();

    if (usedBytes >= thresholdBytes) {
        // The threshold has been surpassed, immediately trigger a GC,
        // which will be done non-incrementally.
        triggerZoneGC(zone, JS::gcreason::ALLOC_TRIGGER);
    } else if (usedBytes >= igcThresholdBytes) {
        // Reduce the delay to the start of the next incremental slice.
        if (zone->gcDelayBytes < ArenaSize)
            zone->gcDelayBytes = 0;
        else
            zone->gcDelayBytes -= ArenaSize;

        if (!zone->gcDelayBytes) {
            // Start or continue an in progress incremental GC. We do this
            // to try to avoid performing non-incremental GCs on zones
            // which allocate a lot of data, even when incremental slices
            // can't be triggered via scheduling in the event loop.
            triggerZoneGC(zone, JS::gcreason::ALLOC_TRIGGER);

            // Delay the next slice until a certain amount of allocation
            // has been performed.
            zone->gcDelayBytes = tunables.zoneAllocDelayBytes();
        }
    }
}

bool
GCRuntime::triggerZoneGC(Zone* zone, JS::gcreason::Reason reason)
{
    /* Zones in use by a helper thread can't be collected. */
    if (!CurrentThreadCanAccessRuntime(rt)) {
        MOZ_ASSERT(zone->usedByHelperThread() || zone->isAtomsZone());
        return false;
    }

    /* GC is already running. */
    if (JS::CurrentThreadIsHeapCollecting())
        return false;

#ifdef JS_GC_ZEAL
    if (hasZealMode(ZealMode::Alloc)) {
        MOZ_RELEASE_ASSERT(triggerGC(reason));
        return true;
    }
#endif

    if (zone->isAtomsZone()) {
        /* We can't do a zone GC of the atoms compartment. */
        if (TlsContext.get()->keepAtoms || rt->hasHelperThreadZones()) {
            /* Skip GC and retrigger later, since atoms zone won't be collected
             * if keepAtoms is true. */
            fullGCForAtomsRequested_ = true;
            return false;
        }
        MOZ_RELEASE_ASSERT(triggerGC(reason));
        return true;
    }

    PrepareZoneForGC(zone);
    requestMajorGC(reason);
    return true;
}

void
GCRuntime::maybeGC(Zone* zone)
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));

#ifdef JS_GC_ZEAL
    if (hasZealMode(ZealMode::Alloc) || hasZealMode(ZealMode::Poke)) {
        JS::PrepareForFullGC(rt->activeContextFromOwnThread());
        gc(GC_NORMAL, JS::gcreason::DEBUG_GC);
        return;
    }
#endif

    if (gcIfRequested())
        return;

    if (zone->usage.gcBytes() > 1024 * 1024 &&
        zone->usage.gcBytes() >= zone->threshold.allocTrigger(schedulingState.inHighFrequencyGCMode()) &&
        !isIncrementalGCInProgress() &&
        !isBackgroundSweeping())
    {
        PrepareZoneForGC(zone);
        startGC(GC_NORMAL, JS::gcreason::EAGER_ALLOC_TRIGGER);
    }
}

// Do all possible decommit immediately from the current thread without
// releasing the GC lock or allocating any memory.
void
GCRuntime::decommitAllWithoutUnlocking(const AutoLockGC& lock)
{
    MOZ_ASSERT(emptyChunks(lock).count() == 0);
    for (ChunkPool::Iter chunk(availableChunks(lock)); !chunk.done(); chunk.next())
        chunk->decommitAllArenasWithoutUnlocking(lock);
    MOZ_ASSERT(availableChunks(lock).verify());
}

void
GCRuntime::startDecommit()
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
    MOZ_ASSERT(!decommitTask.isRunning());

    // If we are allocating heavily enough to trigger "high freqency" GC, then
    // skip decommit so that we do not compete with the mutator.
    if (schedulingState.inHighFrequencyGCMode())
        return;

    BackgroundDecommitTask::ChunkVector toDecommit;
    {
        AutoLockGC lock(rt);

        // Verify that all entries in the empty chunks pool are already decommitted.
        for (ChunkPool::Iter chunk(emptyChunks(lock)); !chunk.done(); chunk.next())
            MOZ_ASSERT(!chunk->info.numArenasFreeCommitted);

        // Since we release the GC lock while doing the decommit syscall below,
        // it is dangerous to iterate the available list directly, as the active
        // thread could modify it concurrently. Instead, we build and pass an
        // explicit Vector containing the Chunks we want to visit.
        MOZ_ASSERT(availableChunks(lock).verify());
        for (ChunkPool::Iter iter(availableChunks(lock)); !iter.done(); iter.next()) {
            if (!toDecommit.append(iter.get())) {
                // The OOM handler does a full, immediate decommit.
                return onOutOfMallocMemory(lock);
            }
        }
    }
    decommitTask.setChunksToScan(toDecommit);

    if (sweepOnBackgroundThread && decommitTask.start())
        return;

    decommitTask.runFromActiveCooperatingThread(rt);
}

void
js::gc::BackgroundDecommitTask::setChunksToScan(ChunkVector &chunks)
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime()));
    MOZ_ASSERT(!isRunning());
    MOZ_ASSERT(toDecommit.ref().empty());
    Swap(toDecommit.ref(), chunks);
}

/* virtual */ void
js::gc::BackgroundDecommitTask::run()
{
    AutoLockGC lock(runtime());

    for (Chunk* chunk : toDecommit.ref()) {

        // The arena list is not doubly-linked, so we have to work in the free
        // list order and not in the natural order.
        while (chunk->info.numArenasFreeCommitted) {
            bool ok = chunk->decommitOneFreeArena(runtime(), lock);

            // If we are low enough on memory that we can't update the page
            // tables, or if we need to return for any other reason, break out
            // of the loop.
            if (cancel_ || !ok)
                break;
        }
    }
    toDecommit.ref().clearAndFree();

    ChunkPool toFree = runtime()->gc.expireEmptyChunkPool(lock);
    if (toFree.count()) {
        AutoUnlockGC unlock(lock);
        FreeChunkPool(runtime(), toFree);
    }
}

void
GCRuntime::sweepBackgroundThings(ZoneList& zones, LifoAlloc& freeBlocks)
{
    freeBlocks.freeAll();

    if (zones.isEmpty())
        return;

    // We must finalize thing kinds in the order specified by BackgroundFinalizePhases.
    Arena* emptyArenas = nullptr;
    FreeOp fop(nullptr);
    for (unsigned phase = 0 ; phase < ArrayLength(BackgroundFinalizePhases) ; ++phase) {
        for (Zone* zone = zones.front(); zone; zone = zone->nextZone()) {
            for (auto kind : BackgroundFinalizePhases[phase].kinds) {
                Arena* arenas = zone->arenas.arenaListsToSweep(kind);
                MOZ_RELEASE_ASSERT(uintptr_t(arenas) != uintptr_t(-1));
                if (arenas)
                    ArenaLists::backgroundFinalize(&fop, arenas, &emptyArenas);
            }
        }
    }

    AutoLockGC lock(rt);

    // Release swept areans, dropping and reaquiring the lock every so often to
    // avoid blocking the active thread from allocating chunks.
    static const size_t LockReleasePeriod = 32;
    size_t releaseCount = 0;
    Arena* next;
    for (Arena* arena = emptyArenas; arena; arena = next) {
        next = arena->next;
        rt->gc.releaseArena(arena, lock);
        releaseCount++;
        if (releaseCount % LockReleasePeriod == 0) {
            lock.unlock();
            lock.lock();
        }
    }

    while (!zones.isEmpty())
        zones.removeFront();
}

void
GCRuntime::assertBackgroundSweepingFinished()
{
#ifdef DEBUG
    MOZ_ASSERT(backgroundSweepZones.ref().isEmpty());
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        for (auto i : AllAllocKinds()) {
            MOZ_ASSERT(!zone->arenas.arenaListsToSweep(i));
            MOZ_ASSERT(zone->arenas.doneBackgroundFinalize(i));
        }
    }
    MOZ_ASSERT(blocksToFreeAfterSweeping.ref().computedSizeOfExcludingThis() == 0);
#endif
}

void
GCHelperState::finish()
{
    // Wait for any lingering background sweeping to finish.
    waitBackgroundSweepEnd();
}

GCHelperState::State
GCHelperState::state(const AutoLockGC&)
{
    return state_;
}

void
GCHelperState::setState(State state, const AutoLockGC&)
{
    state_ = state;
}

void
GCHelperState::startBackgroundThread(State newState, const AutoLockGC& lock,
                                     const AutoLockHelperThreadState& helperLock)
{
    MOZ_ASSERT(!hasThread && state(lock) == IDLE && newState != IDLE);
    setState(newState, lock);

    {
        AutoEnterOOMUnsafeRegion noOOM;
        if (!HelperThreadState().gcHelperWorklist(helperLock).append(this))
            noOOM.crash("Could not add to pending GC helpers list");
    }

    HelperThreadState().notifyAll(GlobalHelperThreadState::PRODUCER, helperLock);
}

void
GCHelperState::waitForBackgroundThread(js::AutoLockGC& lock)
{
    while (isBackgroundSweeping())
        done.wait(lock.guard());
}

void
GCHelperState::work()
{
    MOZ_ASSERT(CanUseExtraThreads());

    AutoLockGC lock(rt);

    MOZ_ASSERT(!hasThread);
    hasThread = true;

#ifdef DEBUG
    MOZ_ASSERT(!TlsContext.get()->gcHelperStateThread);
    TlsContext.get()->gcHelperStateThread = true;
#endif

    TraceLoggerThread* logger = TraceLoggerForCurrentThread();

    switch (state(lock)) {

      case IDLE:
        MOZ_CRASH("GC helper triggered on idle state");
        break;

      case SWEEPING: {
        AutoTraceLog logSweeping(logger, TraceLogger_GCSweeping);
        doSweep(lock);
        MOZ_ASSERT(state(lock) == SWEEPING);
        break;
      }

    }

    setState(IDLE, lock);
    hasThread = false;

#ifdef DEBUG
    TlsContext.get()->gcHelperStateThread = false;
#endif

    done.notify_all();
}

void
GCRuntime::queueZonesForBackgroundSweep(ZoneList& zones)
{
    AutoLockHelperThreadState helperLock;
    AutoLockGC lock(rt);
    backgroundSweepZones.ref().transferFrom(zones);
    helperState.maybeStartBackgroundSweep(lock, helperLock);
}

void
GCRuntime::freeUnusedLifoBlocksAfterSweeping(LifoAlloc* lifo)
{
    MOZ_ASSERT(JS::CurrentThreadIsHeapBusy());
    AutoLockGC lock(rt);
    blocksToFreeAfterSweeping.ref().transferUnusedFrom(lifo);
}

void
GCRuntime::freeAllLifoBlocksAfterSweeping(LifoAlloc* lifo)
{
    MOZ_ASSERT(JS::CurrentThreadIsHeapBusy());
    AutoLockGC lock(rt);
    blocksToFreeAfterSweeping.ref().transferFrom(lifo);
}

void
GCRuntime::freeAllLifoBlocksAfterMinorGC(LifoAlloc* lifo)
{
    blocksToFreeAfterMinorGC.ref().transferFrom(lifo);
}

void
GCHelperState::maybeStartBackgroundSweep(const AutoLockGC& lock,
                                         const AutoLockHelperThreadState& helperLock)
{
    MOZ_ASSERT(CanUseExtraThreads());

    if (state(lock) == IDLE)
        startBackgroundThread(SWEEPING, lock, helperLock);
}

void
GCHelperState::waitBackgroundSweepEnd()
{
    AutoLockGC lock(rt);
    while (state(lock) == SWEEPING)
        waitForBackgroundThread(lock);
    if (!rt->gc.isIncrementalGCInProgress())
        rt->gc.assertBackgroundSweepingFinished();
}

void
GCHelperState::doSweep(AutoLockGC& lock)
{
    // The active thread may call queueZonesForBackgroundSweep() while this is
    // running so we must check there is no more work to do before exiting.

    do {
        while (!rt->gc.backgroundSweepZones.ref().isEmpty()) {
            AutoSetThreadIsSweeping threadIsSweeping;

            ZoneList zones;
            zones.transferFrom(rt->gc.backgroundSweepZones.ref());
            LifoAlloc freeLifoAlloc(JSContext::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
            freeLifoAlloc.transferFrom(&rt->gc.blocksToFreeAfterSweeping.ref());

            AutoUnlockGC unlock(lock);
            rt->gc.sweepBackgroundThings(zones, freeLifoAlloc);
        }
    } while (!rt->gc.backgroundSweepZones.ref().isEmpty());
}

#ifdef DEBUG

bool
GCHelperState::onBackgroundThread()
{
    return TlsContext.get()->gcHelperStateThread;
}

#endif // DEBUG

bool
GCRuntime::shouldReleaseObservedTypes()
{
    bool releaseTypes = false;

#ifdef JS_GC_ZEAL
    if (zealModeBits != 0)
        releaseTypes = true;
#endif

    /* We may miss the exact target GC due to resets. */
    if (majorGCNumber >= jitReleaseNumber)
        releaseTypes = true;

    if (releaseTypes)
        jitReleaseNumber = majorGCNumber + JIT_SCRIPT_RELEASE_TYPES_PERIOD;

    return releaseTypes;
}

struct IsAboutToBeFinalizedFunctor {
    template <typename T> bool operator()(Cell** t) {
        mozilla::DebugOnly<const Cell*> prior = *t;
        bool result = IsAboutToBeFinalizedUnbarriered(reinterpret_cast<T**>(t));
        // Sweep should not have to deal with moved pointers, since moving GC
        // handles updating the UID table manually.
        MOZ_ASSERT(*t == prior);
        return result;
    }
};

/* static */ bool
UniqueIdGCPolicy::needsSweep(Cell** cell, uint64_t*)
{
    return DispatchTraceKindTyped(IsAboutToBeFinalizedFunctor(), (*cell)->getTraceKind(), cell);
}

void
JS::Zone::sweepUniqueIds(js::FreeOp* fop)
{
    uniqueIds().sweep();
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
void
Zone::sweepCompartments(FreeOp* fop, bool keepAtleastOne, bool destroyingRuntime)
{
    JSRuntime* rt = runtimeFromActiveCooperatingThread();
    JSDestroyCompartmentCallback callback = rt->destroyCompartmentCallback;

    JSCompartment** read = compartments().begin();
    JSCompartment** end = compartments().end();
    JSCompartment** write = read;
    bool foundOne = false;
    while (read < end) {
        JSCompartment* comp = *read++;
        MOZ_ASSERT(!rt->isAtomsCompartment(comp));

        /*
         * Don't delete the last compartment if all the ones before it were
         * deleted and keepAtleastOne is true.
         */
        bool dontDelete = read == end && !foundOne && keepAtleastOne;
        if ((!comp->marked && !dontDelete) || destroyingRuntime) {
            if (callback)
                callback(fop, comp);
            if (comp->principals())
                JS_DropPrincipals(TlsContext.get(), comp->principals());
            js_delete(comp);
            rt->gc.stats().sweptCompartment();
        } else {
            *write++ = comp;
            foundOne = true;
        }
    }
    compartments().shrinkTo(write - compartments().begin());
    MOZ_ASSERT_IF(keepAtleastOne, !compartments().empty());
}

void
GCRuntime::sweepZones(FreeOp* fop, ZoneGroup* group, bool destroyingRuntime)
{
    JSZoneCallback callback = rt->destroyZoneCallback;

    Zone** read = group->zones().begin();
    Zone** end = group->zones().end();
    Zone** write = read;

    while (read < end) {
        Zone* zone = *read++;

        if (zone->wasGCStarted()) {
            MOZ_ASSERT(!zone->isQueuedForBackgroundSweep());
            const bool zoneIsDead = zone->arenas.arenaListsAreEmpty() &&
                                    !zone->hasMarkedCompartments();
            if (zoneIsDead || destroyingRuntime)
            {
                // We have just finished sweeping, so we should have freed any
                // empty arenas back to their Chunk for future allocation.
                zone->arenas.checkEmptyFreeLists();

                // We are about to delete the Zone; this will leave the Zone*
                // in the arena header dangling if there are any arenas
                // remaining at this point.
#ifdef DEBUG
                if (!zone->arenas.checkEmptyArenaLists())
                    arenasEmptyAtShutdown = false;
#endif

                if (callback)
                    callback(zone);

                zone->sweepCompartments(fop, false, destroyingRuntime);
                MOZ_ASSERT(zone->compartments().empty());
                MOZ_ASSERT_IF(arenasEmptyAtShutdown, zone->typeDescrObjects().empty());
                fop->delete_(zone);
                stats().sweptZone();
                continue;
            }
            zone->sweepCompartments(fop, true, destroyingRuntime);
        }
        *write++ = zone;
    }
    group->zones().shrinkTo(write - group->zones().begin());
}

void
GCRuntime::sweepZoneGroups(FreeOp* fop, bool destroyingRuntime)
{
    MOZ_ASSERT_IF(destroyingRuntime, numActiveZoneIters == 0);
    MOZ_ASSERT_IF(destroyingRuntime, arenasEmptyAtShutdown);

    if (rt->gc.numActiveZoneIters)
        return;

    assertBackgroundSweepingFinished();

    ZoneGroup** read = groups.ref().begin();
    ZoneGroup** end = groups.ref().end();
    ZoneGroup** write = read;

    while (read < end) {
        ZoneGroup* group = *read++;
        sweepZones(fop, group, destroyingRuntime);

        if (group->zones().empty()) {
            MOZ_ASSERT(numActiveZoneIters == 0);
            fop->delete_(group);
        } else {
            *write++ = group;
        }
    }
    groups.ref().shrinkTo(write - groups.ref().begin());
}

#ifdef DEBUG
static const char*
AllocKindToAscii(AllocKind kind)
{
    switch(kind) {
#define MAKE_CASE(allocKind, traceKind, type, sizedType) \
      case AllocKind:: allocKind: return #allocKind;
FOR_EACH_ALLOCKIND(MAKE_CASE)
#undef MAKE_CASE

      default:
        MOZ_CRASH("Unknown AllocKind in AllocKindToAscii");
    }
}
#endif // DEBUG

bool
ArenaLists::checkEmptyArenaList(AllocKind kind)
{
    size_t num_live = 0;
#ifdef DEBUG
    if (!arenaLists(kind).isEmpty()) {
        size_t max_cells = 20;
        char *env = getenv("JS_GC_MAX_LIVE_CELLS");
        if (env && *env)
            max_cells = atol(env);
        for (Arena* current = arenaLists(kind).head(); current; current = current->next) {
            for (ArenaCellIterUnderGC i(current); !i.done(); i.next()) {
                TenuredCell* t = i.getCell();
                MOZ_ASSERT(t->isMarked(), "unmarked cells should have been finalized");
                if (++num_live <= max_cells) {
                    fprintf(stderr, "ERROR: GC found live Cell %p of kind %s at shutdown\n",
                            t, AllocKindToAscii(kind));
                }
            }
        }
        fprintf(stderr, "ERROR: GC found %" PRIuSIZE " live Cells at shutdown\n", num_live);
    }
#endif // DEBUG
    return num_live == 0;
}

void
GCRuntime::purgeRuntime(AutoLockForExclusiveAccess& lock)
{
    for (GCCompartmentsIter comp(rt); !comp.done(); comp.next())
        comp->purge();

    for (const CooperatingContext& target : rt->cooperatingContexts()) {
        freeUnusedLifoBlocksAfterSweeping(&target.context()->tempLifoAlloc());
        target.context()->interpreterStack().purge(rt);
        target.context()->frontendCollectionPool().purge();
    }

    rt->caches().gsnCache.purge();
    rt->caches().envCoordinateNameCache.purge();
    rt->caches().newObjectCache.purge();
    rt->caches().nativeIterCache.purge();
    rt->caches().uncompressedSourceCache.purge();
    if (rt->caches().evalCache.initialized())
        rt->caches().evalCache.clear();

    if (auto cache = rt->maybeThisRuntimeSharedImmutableStrings())
        cache->purge();

    rt->promiseTasksToDestroy.lock()->clear();

    MOZ_ASSERT(unmarkGrayStack.empty());
    unmarkGrayStack.clearAndFree();
}

bool
GCRuntime::shouldPreserveJITCode(JSCompartment* comp, int64_t currentTime,
                                 JS::gcreason::Reason reason, bool canAllocateMoreCode)
{
    if (cleanUpEverything)
        return false;
    if (!canAllocateMoreCode)
        return false;

    if (alwaysPreserveCode)
        return true;
    if (comp->preserveJitCode())
        return true;
    if (comp->lastAnimationTime + PRMJ_USEC_PER_SEC >= currentTime)
        return true;
    if (reason == JS::gcreason::DEBUG_GC)
        return true;

    return false;
}

#ifdef DEBUG
class CompartmentCheckTracer : public JS::CallbackTracer
{
    void onChild(const JS::GCCellPtr& thing) override;

  public:
    explicit CompartmentCheckTracer(JSRuntime* rt)
      : JS::CallbackTracer(rt), src(nullptr), zone(nullptr), compartment(nullptr)
    {}

    Cell* src;
    JS::TraceKind srcKind;
    Zone* zone;
    JSCompartment* compartment;
};

namespace {
struct IsDestComparatorFunctor {
    JS::GCCellPtr dst_;
    explicit IsDestComparatorFunctor(JS::GCCellPtr dst) : dst_(dst) {}

    template <typename T> bool operator()(T* t) { return (*t) == dst_.asCell(); }
};
} // namespace (anonymous)

static bool
InCrossCompartmentMap(JSObject* src, JS::GCCellPtr dst)
{
    JSCompartment* srccomp = src->compartment();

    if (dst.is<JSObject>()) {
        Value key = ObjectValue(dst.as<JSObject>());
        if (WrapperMap::Ptr p = srccomp->lookupWrapper(key)) {
            if (*p->value().unsafeGet() == ObjectValue(*src))
                return true;
        }
    }

    /*
     * If the cross-compartment edge is caused by the debugger, then we don't
     * know the right hashtable key, so we have to iterate.
     */
    for (JSCompartment::WrapperEnum e(srccomp); !e.empty(); e.popFront()) {
        if (e.front().mutableKey().applyToWrapped(IsDestComparatorFunctor(dst)) &&
            ToMarkable(e.front().value().unbarrieredGet()) == src)
        {
            return true;
        }
    }

    return false;
}

struct MaybeCompartmentFunctor {
    template <typename T> JSCompartment* operator()(T* t) { return t->maybeCompartment(); }
};

void
CompartmentCheckTracer::onChild(const JS::GCCellPtr& thing)
{
    JSCompartment* comp = DispatchTyped(MaybeCompartmentFunctor(), thing);
    if (comp && compartment) {
        MOZ_ASSERT(comp == compartment || runtime()->isAtomsCompartment(comp) ||
                   (srcKind == JS::TraceKind::Object &&
                    InCrossCompartmentMap(static_cast<JSObject*>(src), thing)));
    } else {
        TenuredCell* tenured = TenuredCell::fromPointer(thing.asCell());
        Zone* thingZone = tenured->zoneFromAnyThread();
        MOZ_ASSERT(thingZone == zone || thingZone->isAtomsZone());
    }
}

void
GCRuntime::checkForCompartmentMismatches()
{
    if (TlsContext.get()->disableStrictProxyCheckingCount)
        return;

    CompartmentCheckTracer trc(rt);
    AutoAssertEmptyNursery empty(TlsContext.get());
    for (ZonesIter zone(rt, SkipAtoms); !zone.done(); zone.next()) {
        trc.zone = zone;
        for (auto thingKind : AllAllocKinds()) {
            for (auto i = zone->cellIter<TenuredCell>(thingKind, empty); !i.done(); i.next()) {
                trc.src = i.getCell();
                trc.srcKind = MapAllocToTraceKind(thingKind);
                trc.compartment = DispatchTraceKindTyped(MaybeCompartmentFunctor(),
                                                         trc.src, trc.srcKind);
                js::TraceChildren(&trc, trc.src, trc.srcKind);
            }
        }
    }
}
#endif

static void
RelazifyFunctions(Zone* zone, AllocKind kind)
{
    MOZ_ASSERT(kind == AllocKind::FUNCTION ||
               kind == AllocKind::FUNCTION_EXTENDED);

    AutoAssertEmptyNursery empty(TlsContext.get());

    JSRuntime* rt = zone->runtimeFromActiveCooperatingThread();
    for (auto i = zone->cellIter<JSObject>(kind, empty); !i.done(); i.next()) {
        JSFunction* fun = &i->as<JSFunction>();
        if (fun->hasScript())
            fun->maybeRelazify(rt);
    }
}

static bool
ShouldCollectZone(Zone* zone, JS::gcreason::Reason reason)
{
    // Normally we collect all scheduled zones.
    if (reason != JS::gcreason::COMPARTMENT_REVIVED)
        return zone->isGCScheduled();

    // If we are repeating a GC becuase we noticed dead compartments haven't
    // been collected, then only collect zones contianing those compartments.
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
        if (comp->scheduledForDestruction)
            return true;
    }

    return false;
}

bool
GCRuntime::beginMarkPhase(JS::gcreason::Reason reason, AutoLockForExclusiveAccess& lock)
{
    int64_t currentTime = PRMJ_Now();

#ifdef DEBUG
    if (fullCompartmentChecks)
        checkForCompartmentMismatches();
#endif

    isFull = true;
    bool any = false;

    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        /* Assert that zone state is as we expect */
        MOZ_ASSERT(!zone->isCollecting());
        MOZ_ASSERT(!zone->compartments().empty());
#ifdef DEBUG
        for (auto i : AllAllocKinds())
            MOZ_ASSERT(!zone->arenas.arenaListsToSweep(i));
#endif

        /* Set up which zones will be collected. */
        if (ShouldCollectZone(zone, reason)) {
            if (!zone->isAtomsZone()) {
                any = true;
                zone->setGCState(Zone::Mark);
            }
        } else {
            isFull = false;
        }

        zone->setPreservingCode(false);
    }

    // Discard JIT code more aggressively if the process is approaching its
    // executable code limit.
    bool canAllocateMoreCode = jit::CanLikelyAllocateMoreExecutableMemory();

    for (CompartmentsIter c(rt, WithAtoms); !c.done(); c.next()) {
        c->marked = false;
        c->scheduledForDestruction = false;
        c->maybeAlive = c->hasBeenEntered() || !c->zone()->isGCScheduled();
        if (shouldPreserveJITCode(c, currentTime, reason, canAllocateMoreCode))
            c->zone()->setPreservingCode(true);
    }

    if (!rt->gc.cleanUpEverything && canAllocateMoreCode) {
        if (JSCompartment* comp = jit::TopmostIonActivationCompartment(TlsContext.get()))
            comp->zone()->setPreservingCode(true);
    }

    /*
     * If keepAtoms() is true then either an instance of AutoKeepAtoms is
     * currently on the stack or parsing is currently happening on another
     * thread. In either case we don't have information about which atoms are
     * roots, so we must skip collecting atoms.
     *
     * Note that only affects the first slice of an incremental GC since root
     * marking is completed before we return to the mutator.
     *
     * Off-thread parsing is inhibited after the start of GC which prevents
     * races between creating atoms during parsing and sweeping atoms on the
     * active thread.
     *
     * Otherwise, we always schedule a GC in the atoms zone so that atoms which
     * the other collected zones are using are marked, and we can update the
     * set of atoms in use by the other collected zones at the end of the GC.
     */
    if (!TlsContext.get()->keepAtoms || rt->hasHelperThreadZones()) {
        Zone* atomsZone = rt->atomsCompartment(lock)->zone();
        if (atomsZone->isGCScheduled()) {
            MOZ_ASSERT(!atomsZone->isCollecting());
            atomsZone->setGCState(Zone::Mark);
            any = true;
        }
    }

    /* Check that at least one zone is scheduled for collection. */
    if (!any)
        return false;

    /*
     * Ensure that after the start of a collection we don't allocate into any
     * existing arenas, as this can cause unreachable things to be marked.
     */
    if (isIncremental) {
        for (GCZonesIter zone(rt); !zone.done(); zone.next())
            zone->arenas.prepareForIncrementalGC();
    }

    MemProfiler::MarkTenuredStart(rt);
    marker.start();
    GCMarker* gcmarker = &marker;

    /* For non-incremental GC the following sweep discards the jit code. */
    if (isIncremental) {
        js::CancelOffThreadIonCompile(rt, JS::Zone::Mark);
        for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
            gcstats::AutoPhase ap(stats(), gcstats::PHASE_MARK_DISCARD_CODE);
            zone->discardJitCode(rt->defaultFreeOp());
        }
    }

    /*
     * Relazify functions after discarding JIT code (we can't relazify
     * functions with JIT code) and before the actual mark phase, so that
     * the current GC can collect the JSScripts we're unlinking here.
     * We do this only when we're performing a shrinking GC, as too much
     * relazification can cause performance issues when we have to reparse
     * the same functions over and over.
     */
    if (invocationKind == GC_SHRINK) {
        {
            gcstats::AutoPhase ap(stats(), gcstats::PHASE_RELAZIFY_FUNCTIONS);
            for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
                if (zone->isSelfHostingZone())
                    continue;
                RelazifyFunctions(zone, AllocKind::FUNCTION);
                RelazifyFunctions(zone, AllocKind::FUNCTION_EXTENDED);
            }
        }

        /* Purge ShapeTables. */
        gcstats::AutoPhase ap(stats(), gcstats::PHASE_PURGE_SHAPE_TABLES);
        for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
            if (zone->keepShapeTables() || zone->isSelfHostingZone())
                continue;
            for (auto baseShape = zone->cellIter<BaseShape>(); !baseShape.done(); baseShape.next())
                baseShape->maybePurgeTable();
        }
    }

    startNumber = number;

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
        gcstats::AutoPhase ap(stats(), gcstats::PHASE_PURGE);
        purgeRuntime(lock);
    }

    /*
     * Mark phase.
     */
    gcstats::AutoPhase ap1(stats(), gcstats::PHASE_MARK);

    {
        gcstats::AutoPhase ap(stats(), gcstats::PHASE_UNMARK);

        for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
            /* Unmark everything in the zones being collected. */
            zone->arenas.unmarkAll();
        }

        for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
            /* Unmark all weak maps in the zones being collected. */
            WeakMapBase::unmarkZone(zone);
        }
    }

    traceRuntimeForMajorGC(gcmarker, lock);

    gcstats::AutoPhase ap2(stats(), gcstats::PHASE_MARK_ROOTS);

    if (isIncremental) {
        bufferGrayRoots();
        markCompartments();
    }

    return true;
}

void
GCRuntime::markCompartments()
{
    gcstats::AutoPhase ap(stats(), gcstats::PHASE_MARK_COMPARTMENTS);

    /*
     * This code ensures that if a compartment is "dead", then it will be
     * collected in this GC. A compartment is considered dead if its maybeAlive
     * flag is false. The maybeAlive flag is set if:
     *
     *   (1) the compartment has been entered (set in beginMarkPhase() above)
     *   (2) the compartment is not being collected (set in beginMarkPhase()
     *       above)
     *   (3) an object in the compartment was marked during root marking, either
     *       as a black root or a gray root (set in RootMarking.cpp), or
     *   (4) the compartment has incoming cross-compartment edges from another
     *       compartment that has maybeAlive set (set by this method).
     *
     * If the maybeAlive is false, then we set the scheduledForDestruction flag.
     * At the end of the GC, we look for compartments where
     * scheduledForDestruction is true. These are compartments that were somehow
     * "revived" during the incremental GC. If any are found, we do a special,
     * non-incremental GC of those compartments to try to collect them.
     *
     * Compartments can be revived for a variety of reasons. On reason is bug
     * 811587, where a reflector that was dead can be revived by DOM code that
     * still refers to the underlying DOM node.
     *
     * Read barriers and allocations can also cause revival. This might happen
     * during a function like JS_TransplantObject, which iterates over all
     * compartments, live or dead, and operates on their objects. See bug 803376
     * for details on this problem. To avoid the problem, we try to avoid
     * allocation and read barriers during JS_TransplantObject and the like.
     */

    /* Propagate the maybeAlive flag via cross-compartment edges. */

    Vector<JSCompartment*, 0, js::SystemAllocPolicy> workList;

    for (CompartmentsIter comp(rt, SkipAtoms); !comp.done(); comp.next()) {
        if (comp->maybeAlive) {
            if (!workList.append(comp))
                return;
        }
    }

    while (!workList.empty()) {
        JSCompartment* comp = workList.popCopy();
        for (JSCompartment::WrapperEnum e(comp); !e.empty(); e.popFront()) {
            if (e.front().key().is<JSString*>())
                continue;
            JSCompartment* dest = e.front().mutableKey().compartment();
            if (dest && !dest->maybeAlive) {
                dest->maybeAlive = true;
                if (!workList.append(dest))
                    return;
            }
        }
    }

    /* Set scheduleForDestruction based on maybeAlive. */

    for (GCCompartmentsIter comp(rt); !comp.done(); comp.next()) {
        MOZ_ASSERT(!comp->scheduledForDestruction);
        if (!comp->maybeAlive && !rt->isAtomsCompartment(comp))
            comp->scheduledForDestruction = true;
    }
}

template <class ZoneIterT>
void
GCRuntime::markWeakReferences(gcstats::Phase phase)
{
    MOZ_ASSERT(marker.isDrained());

    gcstats::AutoPhase ap1(stats(), phase);

    marker.enterWeakMarkingMode();

    // TODO bug 1167452: Make weak marking incremental
    auto unlimited = SliceBudget::unlimited();
    MOZ_RELEASE_ASSERT(marker.drainMarkStack(unlimited));

    for (;;) {
        bool markedAny = false;
        if (!marker.isWeakMarkingTracer()) {
            for (ZoneIterT zone(rt); !zone.done(); zone.next())
                markedAny |= WeakMapBase::markZoneIteratively(zone, &marker);
        }
        for (CompartmentsIterT<ZoneIterT> c(rt); !c.done(); c.next()) {
            if (c->watchpointMap)
                markedAny |= c->watchpointMap->markIteratively(&marker);
        }
        markedAny |= Debugger::markIteratively(&marker);
        markedAny |= jit::JitRuntime::MarkJitcodeGlobalTableIteratively(&marker);

        if (!markedAny)
            break;

        auto unlimited = SliceBudget::unlimited();
        MOZ_RELEASE_ASSERT(marker.drainMarkStack(unlimited));
    }
    MOZ_ASSERT(marker.isDrained());

    marker.leaveWeakMarkingMode();
}

void
GCRuntime::markWeakReferencesInCurrentGroup(gcstats::Phase phase)
{
    markWeakReferences<GCZoneGroupIter>(phase);
}

template <class ZoneIterT, class CompartmentIterT>
void
GCRuntime::markGrayReferences(gcstats::Phase phase)
{
    gcstats::AutoPhase ap(stats(), phase);
    if (hasBufferedGrayRoots()) {
        for (ZoneIterT zone(rt); !zone.done(); zone.next())
            markBufferedGrayRoots(zone);
    } else {
        MOZ_ASSERT(!isIncremental);
        if (JSTraceDataOp op = grayRootTracer.op)
            (*op)(&marker, grayRootTracer.data);
    }
    auto unlimited = SliceBudget::unlimited();
    MOZ_RELEASE_ASSERT(marker.drainMarkStack(unlimited));
}

void
GCRuntime::markGrayReferencesInCurrentGroup(gcstats::Phase phase)
{
    markGrayReferences<GCZoneGroupIter, GCCompartmentGroupIter>(phase);
}

void
GCRuntime::markAllWeakReferences(gcstats::Phase phase)
{
    markWeakReferences<GCZonesIter>(phase);
}

void
GCRuntime::markAllGrayReferences(gcstats::Phase phase)
{
    markGrayReferences<GCZonesIter, GCCompartmentsIter>(phase);
}

#ifdef JS_GC_ZEAL

struct GCChunkHasher {
    typedef gc::Chunk* Lookup;

    /*
     * Strip zeros for better distribution after multiplying by the golden
     * ratio.
     */
    static HashNumber hash(gc::Chunk* chunk) {
        MOZ_ASSERT(!(uintptr_t(chunk) & gc::ChunkMask));
        return HashNumber(uintptr_t(chunk) >> gc::ChunkShift);
    }

    static bool match(gc::Chunk* k, gc::Chunk* l) {
        MOZ_ASSERT(!(uintptr_t(k) & gc::ChunkMask));
        MOZ_ASSERT(!(uintptr_t(l) & gc::ChunkMask));
        return k == l;
    }
};

class js::gc::MarkingValidator
{
  public:
    explicit MarkingValidator(GCRuntime* gc);
    ~MarkingValidator();
    void nonIncrementalMark(AutoLockForExclusiveAccess& lock);
    void validate();

  private:
    GCRuntime* gc;
    bool initialized;

    typedef HashMap<Chunk*, ChunkBitmap*, GCChunkHasher, SystemAllocPolicy> BitmapMap;
    BitmapMap map;
};

js::gc::MarkingValidator::MarkingValidator(GCRuntime* gc)
  : gc(gc),
    initialized(false)
{}

js::gc::MarkingValidator::~MarkingValidator()
{
    if (!map.initialized())
        return;

    for (BitmapMap::Range r(map.all()); !r.empty(); r.popFront())
        js_delete(r.front().value());
}

void
js::gc::MarkingValidator::nonIncrementalMark(AutoLockForExclusiveAccess& lock)
{
    /*
     * Perform a non-incremental mark for all collecting zones and record
     * the results for later comparison.
     *
     * Currently this does not validate gray marking.
     */

    if (!map.init())
        return;

    JSRuntime* runtime = gc->rt;
    GCMarker* gcmarker = &gc->marker;

    gc->waitBackgroundSweepEnd();

    /* Save existing mark bits. */
    for (auto chunk = gc->allNonEmptyChunks(); !chunk.done(); chunk.next()) {
        ChunkBitmap* bitmap = &chunk->bitmap;
        ChunkBitmap* entry = js_new<ChunkBitmap>();
        if (!entry)
            return;

        memcpy((void*)entry->bitmap, (void*)bitmap->bitmap, sizeof(bitmap->bitmap));
        if (!map.putNew(chunk, entry))
            return;
    }

    /*
     * Temporarily clear the weakmaps' mark flags for the compartments we are
     * collecting.
     */

    WeakMapSet markedWeakMaps;
    if (!markedWeakMaps.init())
        return;

    /*
     * For saving, smush all of the keys into one big table and split them back
     * up into per-zone tables when restoring.
     */
    gc::WeakKeyTable savedWeakKeys(SystemAllocPolicy(), runtime->randomHashCodeScrambler());
    if (!savedWeakKeys.init())
        return;

    for (GCZonesIter zone(runtime); !zone.done(); zone.next()) {
        if (!WeakMapBase::saveZoneMarkedWeakMaps(zone, markedWeakMaps))
            return;

        AutoEnterOOMUnsafeRegion oomUnsafe;
        for (gc::WeakKeyTable::Range r = zone->gcWeakKeys().all(); !r.empty(); r.popFront()) {
            if (!savedWeakKeys.put(Move(r.front().key), Move(r.front().value)))
                oomUnsafe.crash("saving weak keys table for validator");
        }

        if (!zone->gcWeakKeys().clear())
            oomUnsafe.crash("clearing weak keys table for validator");
    }

    /*
     * After this point, the function should run to completion, so we shouldn't
     * do anything fallible.
     */
    initialized = true;

    /* Re-do all the marking, but non-incrementally. */
    js::gc::State state = gc->incrementalState;
    gc->incrementalState = State::MarkRoots;

    {
        gcstats::AutoPhase ap(gc->stats(), gcstats::PHASE_MARK);

        {
            gcstats::AutoPhase ap(gc->stats(), gcstats::PHASE_UNMARK);

            for (GCZonesIter zone(runtime); !zone.done(); zone.next())
                WeakMapBase::unmarkZone(zone);

            MOZ_ASSERT(gcmarker->isDrained());
            gcmarker->reset();

            for (auto chunk = gc->allNonEmptyChunks(); !chunk.done(); chunk.next())
                chunk->bitmap.clear();
        }

        gc->traceRuntimeForMajorGC(gcmarker, lock);

        gc->incrementalState = State::Mark;
        auto unlimited = SliceBudget::unlimited();
        MOZ_RELEASE_ASSERT(gc->marker.drainMarkStack(unlimited));
    }

    gc->incrementalState = State::Sweep;
    {
        gcstats::AutoPhase ap1(gc->stats(), gcstats::PHASE_SWEEP);
        gcstats::AutoPhase ap2(gc->stats(), gcstats::PHASE_SWEEP_MARK);

        gc->markAllWeakReferences(gcstats::PHASE_SWEEP_MARK_WEAK);

        /* Update zone state for gray marking. */
        for (GCZonesIter zone(runtime); !zone.done(); zone.next()) {
            MOZ_ASSERT(zone->isGCMarkingBlack());
            zone->setGCState(Zone::MarkGray);
        }
        gc->marker.setMarkColorGray();

        gc->markAllGrayReferences(gcstats::PHASE_SWEEP_MARK_GRAY);
        gc->markAllWeakReferences(gcstats::PHASE_SWEEP_MARK_GRAY_WEAK);

        /* Restore zone state. */
        for (GCZonesIter zone(runtime); !zone.done(); zone.next()) {
            MOZ_ASSERT(zone->isGCMarkingGray());
            zone->setGCState(Zone::Mark);
        }
        MOZ_ASSERT(gc->marker.isDrained());
        gc->marker.setMarkColorBlack();
    }

    /* Take a copy of the non-incremental mark state and restore the original. */
    for (auto chunk = gc->allNonEmptyChunks(); !chunk.done(); chunk.next()) {
        ChunkBitmap* bitmap = &chunk->bitmap;
        ChunkBitmap* entry = map.lookup(chunk)->value();
        Swap(*entry, *bitmap);
    }

    for (GCZonesIter zone(runtime); !zone.done(); zone.next()) {
        WeakMapBase::unmarkZone(zone);
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!zone->gcWeakKeys().clear())
            oomUnsafe.crash("clearing weak keys table for validator");
    }

    WeakMapBase::restoreMarkedWeakMaps(markedWeakMaps);

    for (gc::WeakKeyTable::Range r = savedWeakKeys.all(); !r.empty(); r.popFront()) {
        AutoEnterOOMUnsafeRegion oomUnsafe;
        Zone* zone = gc::TenuredCell::fromPointer(r.front().key.asCell())->zone();
        if (!zone->gcWeakKeys().put(Move(r.front().key), Move(r.front().value)))
            oomUnsafe.crash("restoring weak keys table for validator");
    }

    gc->incrementalState = state;
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

    gc->waitBackgroundSweepEnd();

    for (auto chunk = gc->allNonEmptyChunks(); !chunk.done(); chunk.next()) {
        BitmapMap::Ptr ptr = map.lookup(chunk);
        if (!ptr)
            continue;  /* Allocated after we did the non-incremental mark. */

        ChunkBitmap* bitmap = ptr->value();
        ChunkBitmap* incBitmap = &chunk->bitmap;

        for (size_t i = 0; i < ArenasPerChunk; i++) {
            if (chunk->decommittedArenas.get(i))
                continue;
            Arena* arena = &chunk->arenas[i];
            if (!arena->allocated())
                continue;
            if (!arena->zone->isGCSweeping())
                continue;
            if (arena->allocatedDuringIncremental)
                continue;

            AllocKind kind = arena->getAllocKind();
            uintptr_t thing = arena->thingsStart();
            uintptr_t end = arena->thingsEnd();
            while (thing < end) {
                Cell* cell = (Cell*)thing;

                /*
                 * If a non-incremental GC wouldn't have collected a cell, then
                 * an incremental GC won't collect it.
                 */
                MOZ_ASSERT_IF(bitmap->isMarked(cell, BLACK), incBitmap->isMarked(cell, BLACK));

                /*
                 * If the cycle collector isn't allowed to collect an object
                 * after a non-incremental GC has run, then it isn't allowed to
                 * collected it after an incremental GC.
                 */
                MOZ_ASSERT_IF(!bitmap->isMarked(cell, GRAY), !incBitmap->isMarked(cell, GRAY));

                thing += Arena::thingSize(kind);
            }
        }
    }
}

#endif // JS_GC_ZEAL

void
GCRuntime::computeNonIncrementalMarkingForValidation(AutoLockForExclusiveAccess& lock)
{
#ifdef JS_GC_ZEAL
    MOZ_ASSERT(!markingValidator);
    if (isIncremental && hasZealMode(ZealMode::IncrementalMarkingValidator))
        markingValidator = js_new<MarkingValidator>(this);
    if (markingValidator)
        markingValidator->nonIncrementalMark(lock);
#endif
}

void
GCRuntime::validateIncrementalMarking()
{
#ifdef JS_GC_ZEAL
    if (markingValidator)
        markingValidator->validate();
#endif
}

void
GCRuntime::finishMarkingValidation()
{
#ifdef JS_GC_ZEAL
    js_delete(markingValidator.ref());
    markingValidator = nullptr;
#endif
}

static void
DropStringWrappers(JSRuntime* rt)
{
    /*
     * String "wrappers" are dropped on GC because their presence would require
     * us to sweep the wrappers in all compartments every time we sweep a
     * compartment group.
     */
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
        for (JSCompartment::WrapperEnum e(c); !e.empty(); e.popFront()) {
            if (e.front().key().is<JSString*>())
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
namespace {
struct AddOutgoingEdgeFunctor {
    bool needsEdge_;
    ZoneComponentFinder& finder_;

    AddOutgoingEdgeFunctor(bool needsEdge, ZoneComponentFinder& finder)
      : needsEdge_(needsEdge), finder_(finder)
    {}

    template <typename T>
    void operator()(T tp) {
        TenuredCell& other = (*tp)->asTenured();

        /*
         * Add edge to wrapped object compartment if wrapped object is not
         * marked black to indicate that wrapper compartment not be swept
         * after wrapped compartment.
         */
        if (needsEdge_) {
            JS::Zone* zone = other.zone();
            if (zone->isGCMarking())
                finder_.addEdgeTo(zone);
        }
    }
};
} // namespace (anonymous)

void
JSCompartment::findOutgoingEdges(ZoneComponentFinder& finder)
{
    for (js::WrapperMap::Enum e(crossCompartmentWrappers); !e.empty(); e.popFront()) {
        CrossCompartmentKey& key = e.front().mutableKey();
        MOZ_ASSERT(!key.is<JSString*>());
        bool needsEdge = true;
        if (key.is<JSObject*>()) {
            TenuredCell& other = key.as<JSObject*>()->asTenured();
            needsEdge = !other.isMarked(BLACK) || other.isMarked(GRAY);
        }
        key.applyToWrapped(AddOutgoingEdgeFunctor(needsEdge, finder));
    }
}

void
Zone::findOutgoingEdges(ZoneComponentFinder& finder)
{
    /*
     * Any compartment may have a pointer to an atom in the atoms
     * compartment, and these aren't in the cross compartment map.
     */
    JSRuntime* rt = runtimeFromActiveCooperatingThread();
    Zone* atomsZone = rt->atomsCompartment(finder.lock)->zone();
    if (atomsZone->isGCMarking())
        finder.addEdgeTo(atomsZone);

    for (CompartmentsInZoneIter comp(this); !comp.done(); comp.next())
        comp->findOutgoingEdges(finder);

    for (ZoneSet::Range r = gcZoneGroupEdges().all(); !r.empty(); r.popFront()) {
        if (r.front()->isGCMarking())
            finder.addEdgeTo(r.front());
    }

    Debugger::findZoneEdges(this, finder);
}

bool
GCRuntime::findZoneEdgesForWeakMaps()
{
    /*
     * Weakmaps which have keys with delegates in a different zone introduce the
     * need for zone edges from the delegate's zone to the weakmap zone.
     *
     * Since the edges point into and not away from the zone the weakmap is in
     * we must find these edges in advance and store them in a set on the Zone.
     * If we run out of memory, we fall back to sweeping everything in one
     * group.
     */

    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        if (!WeakMapBase::findInterZoneEdges(zone))
            return false;
    }

    return true;
}

void
GCRuntime::findZoneGroups(AutoLockForExclusiveAccess& lock)
{
#ifdef DEBUG
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
        MOZ_ASSERT(zone->gcZoneGroupEdges().empty());
#endif

    JSContext* cx = TlsContext.get();
    ZoneComponentFinder finder(cx->nativeStackLimit[JS::StackForSystemCode], lock);
    if (!isIncremental || !findZoneEdgesForWeakMaps())
        finder.useOneComponent();

    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        MOZ_ASSERT(zone->isGCMarking());
        finder.addNode(zone);
    }
    zoneGroups = finder.getResultsList();
    currentZoneGroup = zoneGroups;
    zoneGroupIndex = 0;

    for (GCZonesIter zone(rt); !zone.done(); zone.next())
        zone->gcZoneGroupEdges().clear();

#ifdef DEBUG
    for (Zone* head = currentZoneGroup; head; head = head->nextGroup()) {
        for (Zone* zone = head; zone; zone = zone->nextNodeInGroup())
            MOZ_ASSERT(zone->isGCMarking());
    }

    MOZ_ASSERT_IF(!isIncremental, !currentZoneGroup->nextGroup());
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
        MOZ_ASSERT(zone->gcZoneGroupEdges().empty());
#endif
}

static void
ResetGrayList(JSCompartment* comp);

void
GCRuntime::getNextZoneGroup()
{
    currentZoneGroup = currentZoneGroup->nextGroup();
    ++zoneGroupIndex;
    if (!currentZoneGroup) {
        abortSweepAfterCurrentGroup = false;
        return;
    }

    for (Zone* zone = currentZoneGroup; zone; zone = zone->nextNodeInGroup()) {
        MOZ_ASSERT(zone->isGCMarking());
        MOZ_ASSERT(!zone->isQueuedForBackgroundSweep());
    }

    if (!isIncremental)
        ZoneComponentFinder::mergeGroups(currentZoneGroup);

    if (abortSweepAfterCurrentGroup) {
        MOZ_ASSERT(!isIncremental);
        for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
            MOZ_ASSERT(!zone->gcNextGraphComponent);
            MOZ_ASSERT(zone->isGCMarking());
            zone->setNeedsIncrementalBarrier(false, Zone::UpdateJit);
            zone->setGCState(Zone::NoGC);
            zone->gcGrayRoots().clearAndFree();
        }

        for (GCCompartmentGroupIter comp(rt); !comp.done(); comp.next())
            ResetGrayList(comp);

        abortSweepAfterCurrentGroup = false;
        currentZoneGroup = nullptr;
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
IsGrayListObject(JSObject* obj)
{
    MOZ_ASSERT(obj);
    return obj->is<CrossCompartmentWrapperObject>() && !IsDeadProxyObject(obj);
}

/* static */ unsigned
ProxyObject::grayLinkExtraSlot(JSObject* obj)
{
    MOZ_ASSERT(IsGrayListObject(obj));
    return 1;
}

#ifdef DEBUG
static void
AssertNotOnGrayList(JSObject* obj)
{
    MOZ_ASSERT_IF(IsGrayListObject(obj),
                  GetProxyExtra(obj, ProxyObject::grayLinkExtraSlot(obj)).isUndefined());
}
#endif

static void
AssertNoWrappersInGrayList(JSRuntime* rt)
{
#ifdef DEBUG
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
        MOZ_ASSERT(!c->gcIncomingGrayPointers);
        for (JSCompartment::WrapperEnum e(c); !e.empty(); e.popFront()) {
            if (!e.front().key().is<JSString*>())
                AssertNotOnGrayList(&e.front().value().unbarrieredGet().toObject());
        }
    }
#endif
}

static JSObject*
CrossCompartmentPointerReferent(JSObject* obj)
{
    MOZ_ASSERT(IsGrayListObject(obj));
    return &obj->as<ProxyObject>().private_().toObject();
}

static JSObject*
NextIncomingCrossCompartmentPointer(JSObject* prev, bool unlink)
{
    unsigned slot = ProxyObject::grayLinkExtraSlot(prev);
    JSObject* next = GetProxyExtra(prev, slot).toObjectOrNull();
    MOZ_ASSERT_IF(next, IsGrayListObject(next));

    if (unlink)
        SetProxyExtra(prev, slot, UndefinedValue());

    return next;
}

void
js::DelayCrossCompartmentGrayMarking(JSObject* src)
{
    MOZ_ASSERT(IsGrayListObject(src));

    /* Called from MarkCrossCompartmentXXX functions. */
    unsigned slot = ProxyObject::grayLinkExtraSlot(src);
    JSObject* dest = CrossCompartmentPointerReferent(src);
    JSCompartment* comp = dest->compartment();

    if (GetProxyExtra(src, slot).isUndefined()) {
        SetProxyExtra(src, slot, ObjectOrNullValue(comp->gcIncomingGrayPointers));
        comp->gcIncomingGrayPointers = src;
    } else {
        MOZ_ASSERT(GetProxyExtra(src, slot).isObjectOrNull());
    }

#ifdef DEBUG
    /*
     * Assert that the object is in our list, also walking the list to check its
     * integrity.
     */
    JSObject* obj = comp->gcIncomingGrayPointers;
    bool found = false;
    while (obj) {
        if (obj == src)
            found = true;
        obj = NextIncomingCrossCompartmentPointer(obj, false);
    }
    MOZ_ASSERT(found);
#endif
}

static void
MarkIncomingCrossCompartmentPointers(JSRuntime* rt, const uint32_t color)
{
    MOZ_ASSERT(color == BLACK || color == GRAY);

    static const gcstats::Phase statsPhases[] = {
        gcstats::PHASE_SWEEP_MARK_INCOMING_BLACK,
        gcstats::PHASE_SWEEP_MARK_INCOMING_GRAY
    };
    gcstats::AutoPhase ap1(rt->gc.stats(), statsPhases[color]);

    bool unlinkList = color == GRAY;

    for (GCCompartmentGroupIter c(rt); !c.done(); c.next()) {
        MOZ_ASSERT_IF(color == GRAY, c->zone()->isGCMarkingGray());
        MOZ_ASSERT_IF(color == BLACK, c->zone()->isGCMarkingBlack());
        MOZ_ASSERT_IF(c->gcIncomingGrayPointers, IsGrayListObject(c->gcIncomingGrayPointers));

        for (JSObject* src = c->gcIncomingGrayPointers;
             src;
             src = NextIncomingCrossCompartmentPointer(src, unlinkList))
        {
            JSObject* dst = CrossCompartmentPointerReferent(src);
            MOZ_ASSERT(dst->compartment() == c);

            if (color == GRAY) {
                if (IsMarkedUnbarriered(rt, &src) && src->asTenured().isMarked(GRAY))
                    TraceManuallyBarrieredEdge(&rt->gc.marker, &dst,
                                               "cross-compartment gray pointer");
            } else {
                if (IsMarkedUnbarriered(rt, &src) && !src->asTenured().isMarked(GRAY))
                    TraceManuallyBarrieredEdge(&rt->gc.marker, &dst,
                                               "cross-compartment black pointer");
            }
        }

        if (unlinkList)
            c->gcIncomingGrayPointers = nullptr;
    }

    auto unlimited = SliceBudget::unlimited();
    MOZ_RELEASE_ASSERT(rt->gc.marker.drainMarkStack(unlimited));
}

static bool
RemoveFromGrayList(JSObject* wrapper)
{
    if (!IsGrayListObject(wrapper))
        return false;

    unsigned slot = ProxyObject::grayLinkExtraSlot(wrapper);
    if (GetProxyExtra(wrapper, slot).isUndefined())
        return false;  /* Not on our list. */

    JSObject* tail = GetProxyExtra(wrapper, slot).toObjectOrNull();
    SetProxyExtra(wrapper, slot, UndefinedValue());

    JSCompartment* comp = CrossCompartmentPointerReferent(wrapper)->compartment();
    JSObject* obj = comp->gcIncomingGrayPointers;
    if (obj == wrapper) {
        comp->gcIncomingGrayPointers = tail;
        return true;
    }

    while (obj) {
        unsigned slot = ProxyObject::grayLinkExtraSlot(obj);
        JSObject* next = GetProxyExtra(obj, slot).toObjectOrNull();
        if (next == wrapper) {
            SetProxyExtra(obj, slot, ObjectOrNullValue(tail));
            return true;
        }
        obj = next;
    }

    MOZ_CRASH("object not found in gray link list");
}

static void
ResetGrayList(JSCompartment* comp)
{
    JSObject* src = comp->gcIncomingGrayPointers;
    while (src)
        src = NextIncomingCrossCompartmentPointer(src, true);
    comp->gcIncomingGrayPointers = nullptr;
}

void
js::NotifyGCNukeWrapper(JSObject* obj)
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
js::NotifyGCPreSwap(JSObject* a, JSObject* b)
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
js::NotifyGCPostSwap(JSObject* a, JSObject* b, unsigned removedFlags)
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

void
GCRuntime::endMarkingZoneGroup()
{
    gcstats::AutoPhase ap(stats(), gcstats::PHASE_SWEEP_MARK);

    /*
     * Mark any incoming black pointers from previously swept compartments
     * whose referents are not marked. This can occur when gray cells become
     * black by the action of UnmarkGray.
     */
    MarkIncomingCrossCompartmentPointers(rt, BLACK);
    markWeakReferencesInCurrentGroup(gcstats::PHASE_SWEEP_MARK_WEAK);

    /*
     * Change state of current group to MarkGray to restrict marking to this
     * group.  Note that there may be pointers to the atoms compartment, and
     * these will be marked through, as they are not marked with
     * MarkCrossCompartmentXXX.
     */
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        MOZ_ASSERT(zone->isGCMarkingBlack());
        zone->setGCState(Zone::MarkGray);
    }
    marker.setMarkColorGray();

    /* Mark incoming gray pointers from previously swept compartments. */
    MarkIncomingCrossCompartmentPointers(rt, GRAY);

    /* Mark gray roots and mark transitively inside the current compartment group. */
    markGrayReferencesInCurrentGroup(gcstats::PHASE_SWEEP_MARK_GRAY);
    markWeakReferencesInCurrentGroup(gcstats::PHASE_SWEEP_MARK_GRAY_WEAK);

    /* Restore marking state. */
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        MOZ_ASSERT(zone->isGCMarkingGray());
        zone->setGCState(Zone::Mark);
    }
    MOZ_ASSERT(marker.isDrained());
    marker.setMarkColorBlack();
}

class GCSweepTask : public GCParallelTask
{
    GCSweepTask(const GCSweepTask&) = delete;

  public:
    explicit GCSweepTask(JSRuntime* rt) : GCParallelTask(rt) {}
    GCSweepTask(GCSweepTask&& other)
      : GCParallelTask(mozilla::Move(other))
    {}
};

// Causes the given WeakCache to be swept when run.
class SweepWeakCacheTask : public GCSweepTask
{
    JS::WeakCache<void*>& cache;

    SweepWeakCacheTask(const SweepWeakCacheTask&) = delete;

  public:
    SweepWeakCacheTask(JSRuntime* rt, JS::WeakCache<void*>& wc) : GCSweepTask(rt), cache(wc) {}
    SweepWeakCacheTask(SweepWeakCacheTask&& other)
      : GCSweepTask(mozilla::Move(other)), cache(other.cache)
    {}

    void run() override {
        cache.sweep();
    }
};

#define MAKE_GC_SWEEP_TASK(name)                                              \
    class name : public GCSweepTask {                                         \
        void run() override;                                                  \
      public:                                                                 \
        explicit name (JSRuntime* rt) : GCSweepTask(rt) {}                    \
    }
MAKE_GC_SWEEP_TASK(SweepAtomsTask);
MAKE_GC_SWEEP_TASK(SweepCCWrappersTask);
MAKE_GC_SWEEP_TASK(SweepBaseShapesTask);
MAKE_GC_SWEEP_TASK(SweepInitialShapesTask);
MAKE_GC_SWEEP_TASK(SweepObjectGroupsTask);
MAKE_GC_SWEEP_TASK(SweepRegExpsTask);
MAKE_GC_SWEEP_TASK(SweepMiscTask);
#undef MAKE_GC_SWEEP_TASK

/* virtual */ void
SweepAtomsTask::run()
{
    DenseBitmap marked;
    if (runtime()->gc.atomMarking.computeBitmapFromChunkMarkBits(runtime(), marked)) {
        for (GCZonesIter zone(runtime()); !zone.done(); zone.next())
            runtime()->gc.atomMarking.updateZoneBitmap(zone, marked);
    } else {
        // Ignore OOM in computeBitmapFromChunkMarkBits. The updateZoneBitmap
        // call can only remove atoms from the zone bitmap, so it is
        // conservative to just not call it.
    }

    runtime()->gc.atomMarking.updateChunkMarkBits(runtime());
    runtime()->sweepAtoms();
    runtime()->unsafeSymbolRegistry().sweep();
    for (CompartmentsIter comp(runtime(), SkipAtoms); !comp.done(); comp.next())
        comp->sweepVarNames();
}

/* virtual */ void
SweepCCWrappersTask::run()
{
    for (GCCompartmentGroupIter c(runtime()); !c.done(); c.next())
        c->sweepCrossCompartmentWrappers();
}

/* virtual */ void
SweepObjectGroupsTask::run()
{
    for (GCCompartmentGroupIter c(runtime()); !c.done(); c.next())
        c->objectGroups.sweep(runtime()->defaultFreeOp());
}

/* virtual */ void
SweepRegExpsTask::run()
{
    for (GCCompartmentGroupIter c(runtime()); !c.done(); c.next())
        c->sweepRegExps();
}

/* virtual */ void
SweepMiscTask::run()
{
    for (GCCompartmentGroupIter c(runtime()); !c.done(); c.next()) {
        c->sweepSavedStacks();
        c->sweepSelfHostingScriptSource();
        c->sweepNativeIterators();
    }
}

void
GCRuntime::startTask(GCParallelTask& task, gcstats::Phase phase, AutoLockHelperThreadState& locked)
{
    if (!task.startWithLockHeld(locked)) {
        AutoUnlockHelperThreadState unlock(locked);
        gcstats::AutoPhase ap(stats(), phase);
        task.runFromActiveCooperatingThread(rt);
    }
}

void
GCRuntime::joinTask(GCParallelTask& task, gcstats::Phase phase, AutoLockHelperThreadState& locked)
{
    gcstats::AutoPhase ap(stats(), task, phase);
    task.joinWithLockHeld(locked);
}

using WeakCacheTaskVector = mozilla::Vector<SweepWeakCacheTask, 0, SystemAllocPolicy>;

static void
SweepWeakCachesFromActiveCooperatingThread(JSRuntime* rt)
{
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        for (JS::WeakCache<void*>* cache : zone->weakCaches()) {
            SweepWeakCacheTask task(rt, *cache);
            task.runFromActiveCooperatingThread(rt);
        }
    }
}

static WeakCacheTaskVector
PrepareWeakCacheTasks(JSRuntime* rt)
{
    WeakCacheTaskVector out;
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        for (JS::WeakCache<void*>* cache : zone->weakCaches()) {
            if (!out.append(SweepWeakCacheTask(rt, *cache))) {
                SweepWeakCachesFromActiveCooperatingThread(rt);
                return WeakCacheTaskVector();
            }
        }
    }
    return out;
}

void
GCRuntime::beginSweepingZoneGroup(AutoLockForExclusiveAccess& lock)
{
    /*
     * Begin sweeping the group of zones in gcCurrentZoneGroup,
     * performing actions that must be done before yielding to caller.
     */

    bool sweepingAtoms = false;
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        /* Set the GC state to sweeping. */
        MOZ_ASSERT(zone->isGCMarking());
        zone->setGCState(Zone::Sweep);

        /* Purge the ArenaLists before sweeping. */
        zone->arenas.purge();

        if (zone->isAtomsZone())
            sweepingAtoms = true;

        if (rt->sweepZoneCallback)
            rt->sweepZoneCallback(zone);

#ifdef DEBUG
        zone->gcLastZoneGroupIndex = zoneGroupIndex;
#endif
    }

    validateIncrementalMarking();

    FreeOp fop(rt);
    SweepAtomsTask sweepAtomsTask(rt);
    SweepCCWrappersTask sweepCCWrappersTask(rt);
    SweepObjectGroupsTask sweepObjectGroupsTask(rt);
    SweepRegExpsTask sweepRegExpsTask(rt);
    SweepMiscTask sweepMiscTask(rt);
    WeakCacheTaskVector sweepCacheTasks = PrepareWeakCacheTasks(rt);

    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        /* Clear all weakrefs that point to unmarked things. */
        for (auto edge : zone->gcWeakRefs()) {
            /* Edges may be present multiple times, so may already be nulled. */
            if (*edge && IsAboutToBeFinalizedDuringSweep(**edge))
                *edge = nullptr;
        }
        zone->gcWeakRefs().clear();

        /* No need to look up any more weakmap keys from this zone group. */
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!zone->gcWeakKeys().clear())
            oomUnsafe.crash("clearing weak keys in beginSweepingZoneGroup()");
    }

    {
        gcstats::AutoPhase ap(stats(), gcstats::PHASE_FINALIZE_START);
        callFinalizeCallbacks(&fop, JSFINALIZE_GROUP_START);
        {
            gcstats::AutoPhase ap2(stats(), gcstats::PHASE_WEAK_ZONEGROUP_CALLBACK);
            callWeakPointerZoneGroupCallbacks();
        }
        {
            gcstats::AutoPhase ap2(stats(), gcstats::PHASE_WEAK_COMPARTMENT_CALLBACK);
            for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
                for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next())
                    callWeakPointerCompartmentCallbacks(comp);
            }
        }
    }

    if (sweepingAtoms) {
        AutoLockHelperThreadState helperLock;
        startTask(sweepAtomsTask, gcstats::PHASE_SWEEP_ATOMS, helperLock);
    }

    {
        gcstats::AutoPhase ap(stats(), gcstats::PHASE_SWEEP_COMPARTMENTS);
        gcstats::AutoSCC scc(stats(), zoneGroupIndex);

        {
            AutoLockHelperThreadState helperLock;
            startTask(sweepCCWrappersTask, gcstats::PHASE_SWEEP_CC_WRAPPER, helperLock);
            startTask(sweepObjectGroupsTask, gcstats::PHASE_SWEEP_TYPE_OBJECT, helperLock);
            startTask(sweepRegExpsTask, gcstats::PHASE_SWEEP_REGEXP, helperLock);
            startTask(sweepMiscTask, gcstats::PHASE_SWEEP_MISC, helperLock);
            for (auto& task : sweepCacheTasks)
                startTask(task, gcstats::PHASE_SWEEP_MISC, helperLock);
        }

        // The remainder of the of the tasks run in parallel on the active
        // thread until we join, below.
        {
            gcstats::AutoPhase ap(stats(), gcstats::PHASE_SWEEP_MISC);

            // Cancel any active or pending off thread compilations.
            js::CancelOffThreadIonCompile(rt, JS::Zone::Sweep);

            for (GCCompartmentGroupIter c(rt); !c.done(); c.next()) {
                c->sweepGlobalObject(&fop);
                c->sweepDebugEnvironments();
                c->sweepJitCompartment(&fop);
                c->sweepTemplateObjects();
            }

            for (GCZoneGroupIter zone(rt); !zone.done(); zone.next())
                zone->sweepWeakMaps();

            // Bug 1071218: the following two methods have not yet been
            // refactored to work on a single zone-group at once.

            // Collect watch points associated with unreachable objects.
            WatchpointMap::sweepAll(rt);

            // Detach unreachable debuggers and global objects from each other.
            Debugger::sweepAll(&fop);

            // Sweep entries containing about-to-be-finalized JitCode and
            // update relocated TypeSet::Types inside the JitcodeGlobalTable.
            jit::JitRuntime::SweepJitcodeGlobalTable(rt);
        }

        {
            gcstats::AutoPhase apdc(stats(), gcstats::PHASE_SWEEP_DISCARD_CODE);
            for (GCZoneGroupIter zone(rt); !zone.done(); zone.next())
                zone->discardJitCode(&fop);
        }

        {
            gcstats::AutoPhase ap1(stats(), gcstats::PHASE_SWEEP_TYPES);
            gcstats::AutoPhase ap2(stats(), gcstats::PHASE_SWEEP_TYPES_BEGIN);
            for (GCZoneGroupIter zone(rt); !zone.done(); zone.next())
                zone->beginSweepTypes(&fop, releaseObservedTypes && !zone->isPreservingCode());
        }

        {
            gcstats::AutoPhase ap(stats(), gcstats::PHASE_SWEEP_BREAKPOINT);
            for (GCZoneGroupIter zone(rt); !zone.done(); zone.next())
                zone->sweepBreakpoints(&fop);
        }

        {
            gcstats::AutoPhase ap(stats(), gcstats::PHASE_SWEEP_BREAKPOINT);
            for (GCZoneGroupIter zone(rt); !zone.done(); zone.next())
                zone->sweepUniqueIds(&fop);
        }
    }

    // Rejoin our off-thread tasks.
    if (sweepingAtoms) {
        AutoLockHelperThreadState helperLock;
        joinTask(sweepAtomsTask, gcstats::PHASE_SWEEP_ATOMS, helperLock);
    }

    {
        gcstats::AutoPhase ap(stats(), gcstats::PHASE_SWEEP_COMPARTMENTS);
        gcstats::AutoSCC scc(stats(), zoneGroupIndex);

        AutoLockHelperThreadState helperLock;
        joinTask(sweepCCWrappersTask, gcstats::PHASE_SWEEP_CC_WRAPPER, helperLock);
        joinTask(sweepObjectGroupsTask, gcstats::PHASE_SWEEP_TYPE_OBJECT, helperLock);
        joinTask(sweepRegExpsTask, gcstats::PHASE_SWEEP_REGEXP, helperLock);
        joinTask(sweepMiscTask, gcstats::PHASE_SWEEP_MISC, helperLock);
        for (auto& task : sweepCacheTasks)
            joinTask(task, gcstats::PHASE_SWEEP_MISC, helperLock);
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
        gcstats::AutoSCC scc(stats(), zoneGroupIndex);
        zone->arenas.queueForegroundObjectsForSweep(&fop);
    }
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        gcstats::AutoSCC scc(stats(), zoneGroupIndex);
        for (unsigned i = 0; i < ArrayLength(IncrementalFinalizePhases); ++i)
            zone->arenas.queueForForegroundSweep(&fop, IncrementalFinalizePhases[i]);
    }
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        gcstats::AutoSCC scc(stats(), zoneGroupIndex);
        for (unsigned i = 0; i < ArrayLength(BackgroundFinalizePhases); ++i)
            zone->arenas.queueForBackgroundSweep(&fop, BackgroundFinalizePhases[i]);
    }
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        gcstats::AutoSCC scc(stats(), zoneGroupIndex);
        zone->arenas.queueForegroundThingsForSweep(&fop);
    }

    sweepingTypes = true;

    finalizePhase = 0;
    sweepZone = currentZoneGroup;
    sweepKind = AllocKind::FIRST;

    {
        gcstats::AutoPhase ap(stats(), gcstats::PHASE_FINALIZE_END);
        callFinalizeCallbacks(&fop, JSFINALIZE_GROUP_END);
    }
}

void
GCRuntime::endSweepingZoneGroup()
{
    /* Update the GC state for zones we have swept. */
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        MOZ_ASSERT(zone->isGCSweeping());
        AutoLockGC lock(rt);
        zone->setGCState(Zone::Finished);
        zone->threshold.updateAfterGC(zone->usage.gcBytes(), invocationKind, tunables,
                                      schedulingState, lock);
    }

    /* Start background thread to sweep zones if required. */
    ZoneList zones;
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next())
        zones.append(zone);
    if (sweepOnBackgroundThread)
        queueZonesForBackgroundSweep(zones);
    else
        sweepBackgroundThings(zones, blocksToFreeAfterSweeping.ref());

    /* Reset the list of arenas marked as being allocated during sweep phase. */
    while (Arena* arena = arenasAllocatedDuringSweep) {
        arenasAllocatedDuringSweep = arena->getNextAllocDuringSweep();
        arena->unsetAllocDuringSweep();
    }
}

void
GCRuntime::beginSweepPhase(bool destroyingRuntime, AutoLockForExclusiveAccess& lock)
{
    /*
     * Sweep phase.
     *
     * Finalize as we sweep, outside of lock but with CurrentThreadIsHeapBusy()
     * true so that any attempt to allocate a GC-thing from a finalizer will
     * fail, rather than nest badly and leave the unmarked newborn to be swept.
     */

    MOZ_ASSERT(!abortSweepAfterCurrentGroup);

    AutoSetThreadIsSweeping threadIsSweeping;

    releaseHeldRelocatedArenas();

    computeNonIncrementalMarkingForValidation(lock);

    gcstats::AutoPhase ap(stats(), gcstats::PHASE_SWEEP);

    sweepOnBackgroundThread =
        !destroyingRuntime && !TraceEnabled() && CanUseExtraThreads();

    releaseObservedTypes = shouldReleaseObservedTypes();

    AssertNoWrappersInGrayList(rt);
    DropStringWrappers(rt);

    findZoneGroups(lock);
    endMarkingZoneGroup();
    beginSweepingZoneGroup(lock);
}

bool
ArenaLists::foregroundFinalize(FreeOp* fop, AllocKind thingKind, SliceBudget& sliceBudget,
                               SortedArenaList& sweepList)
{
    if (!arenaListsToSweep(thingKind) && incrementalSweptArenas.ref().isEmpty())
        return true;

    if (!FinalizeArenas(fop, &arenaListsToSweep(thingKind), sweepList,
                        thingKind, sliceBudget, RELEASE_ARENAS))
    {
        incrementalSweptArenaKind = thingKind;
        incrementalSweptArenas = sweepList.toArenaList();
        return false;
    }

    // Clear any previous incremental sweep state we may have saved.
    incrementalSweptArenas.ref().clear();

    // Join |arenaLists[thingKind]| and |sweepList| into a single list.
    ArenaList finalized = sweepList.toArenaList();
    arenaLists(thingKind) =
        finalized.insertListWithCursorAtEnd(arenaLists(thingKind));

    return true;
}

GCRuntime::IncrementalProgress
GCRuntime::drainMarkStack(SliceBudget& sliceBudget, gcstats::Phase phase)
{
    /* Run a marking slice and return whether the stack is now empty. */
    gcstats::AutoPhase ap(stats(), phase);
    return marker.drainMarkStack(sliceBudget) ? Finished : NotFinished;
}

static void
SweepThing(Shape* shape)
{
    if (!shape->isMarked())
        shape->sweep();
}

static void
SweepThing(JSScript* script, AutoClearTypeInferenceStateOnOOM* oom)
{
    script->maybeSweepTypes(oom);
}

static void
SweepThing(ObjectGroup* group, AutoClearTypeInferenceStateOnOOM* oom)
{
    group->maybeSweep(oom);
}

template <typename T, typename... Args>
static bool
SweepArenaList(Arena** arenasToSweep, SliceBudget& sliceBudget, Args... args)
{
    while (Arena* arena = *arenasToSweep) {
        for (ArenaCellIterUnderGC i(arena); !i.done(); i.next())
            SweepThing(i.get<T>(), args...);

        *arenasToSweep = (*arenasToSweep)->next;
        AllocKind kind = MapTypeToFinalizeKind<T>::kind;
        sliceBudget.step(Arena::thingsPerArena(kind));
        if (sliceBudget.isOverBudget())
            return false;
    }

    return true;
}

GCRuntime::IncrementalProgress
GCRuntime::sweepPhase(SliceBudget& sliceBudget, AutoLockForExclusiveAccess& lock)
{
    AutoSetThreadIsSweeping threadIsSweeping;

    gcstats::AutoPhase ap(stats(), gcstats::PHASE_SWEEP);
    FreeOp fop(rt);

    if (drainMarkStack(sliceBudget, gcstats::PHASE_SWEEP_MARK) == NotFinished)
        return NotFinished;


    for (;;) {
        // Sweep dead type information stored in scripts and object groups, but
        // don't finalize them yet. We have to sweep dead information from both
        // live and dead scripts and object groups, so that no dead references
        // remain in them. Type inference can end up crawling these zones
        // again, such as for TypeCompartment::markSetsUnknown, and if this
        // happens after sweeping for the zone group finishes we won't be able
        // to determine which things in the zone are live.
        if (sweepingTypes) {
            gcstats::AutoPhase ap1(stats(), gcstats::PHASE_SWEEP_COMPARTMENTS);
            gcstats::AutoPhase ap2(stats(), gcstats::PHASE_SWEEP_TYPES);

            for (; sweepZone; sweepZone = sweepZone->nextNodeInGroup()) {
                ArenaLists& al = sweepZone->arenas;

                AutoClearTypeInferenceStateOnOOM oom(sweepZone);

                if (!SweepArenaList<JSScript>(&al.gcScriptArenasToUpdate.ref(), sliceBudget, &oom))
                    return NotFinished;

                if (!SweepArenaList<ObjectGroup>(
                        &al.gcObjectGroupArenasToUpdate.ref(), sliceBudget, &oom))
                {
                    return NotFinished;
                }

                // Finish sweeping type information in the zone.
                {
                    gcstats::AutoPhase ap(stats(), gcstats::PHASE_SWEEP_TYPES_END);
                    sweepZone->types.endSweep(rt);
                }

                // Foreground finalized objects have already been finalized,
                // and now their arenas can be reclaimed by freeing empty ones
                // and making non-empty ones available for allocation.
                al.mergeForegroundSweptObjectArenas();
            }

            sweepZone = currentZoneGroup;
            sweepingTypes = false;
        }

        /* Finalize foreground finalized things. */
        for (; finalizePhase < ArrayLength(IncrementalFinalizePhases) ; ++finalizePhase) {
            gcstats::AutoPhase ap(stats(), IncrementalFinalizePhases[finalizePhase].statsPhase);

            for (; sweepZone; sweepZone = sweepZone->nextNodeInGroup()) {
                Zone* zone = sweepZone;

                for (auto kind : SomeAllocKinds(sweepKind, AllocKind::LIMIT)) {
                    if (!IncrementalFinalizePhases[finalizePhase].kinds.contains(kind))
                        continue;

                    /* Set the number of things per arena for this AllocKind. */
                    size_t thingsPerArena = Arena::thingsPerArena(kind);
                    incrementalSweepList.ref().setThingsPerArena(thingsPerArena);

                    if (!zone->arenas.foregroundFinalize(&fop, kind, sliceBudget,
                                                         incrementalSweepList.ref()))
                    {
                        sweepKind = kind;
                        return NotFinished;
                    }

                    /* Reset the slots of the sweep list that we used. */
                    incrementalSweepList.ref().reset(thingsPerArena);
                }
                sweepKind = AllocKind::FIRST;
            }
            sweepZone = currentZoneGroup;
        }

        /* Remove dead shapes from the shape tree, but don't finalize them yet. */
        {
            gcstats::AutoPhase ap(stats(), gcstats::PHASE_SWEEP_SHAPE);

            for (; sweepZone; sweepZone = sweepZone->nextNodeInGroup()) {
                ArenaLists& al = sweepZone->arenas;

                if (!SweepArenaList<Shape>(&al.gcShapeArenasToUpdate.ref(), sliceBudget))
                    return NotFinished;

                if (!SweepArenaList<AccessorShape>(&al.gcAccessorShapeArenasToUpdate.ref(), sliceBudget))
                    return NotFinished;
            }
        }

        endSweepingZoneGroup();
        getNextZoneGroup();
        if (!currentZoneGroup)
            return Finished;

        endMarkingZoneGroup();
        beginSweepingZoneGroup(lock);
    }
}

void
GCRuntime::endSweepPhase(bool destroyingRuntime, AutoLockForExclusiveAccess& lock)
{
    AutoSetThreadIsSweeping threadIsSweeping;

    gcstats::AutoPhase ap(stats(), gcstats::PHASE_SWEEP);
    FreeOp fop(rt);

    MOZ_ASSERT_IF(destroyingRuntime, !sweepOnBackgroundThread);

    /*
     * Recalculate whether GC was full or not as this may have changed due to
     * newly created zones.  Can only change from full to not full.
     */
    if (isFull) {
        for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
            if (!zone->isCollecting()) {
                isFull = false;
                break;
            }
        }
    }

    {
        gcstats::AutoPhase ap(stats(), gcstats::PHASE_DESTROY);

        /*
         * Sweep script filenames after sweeping functions in the generic loop
         * above. In this way when a scripted function's finalizer destroys the
         * script and calls rt->destroyScriptHook, the hook can still access the
         * script's filename. See bug 323267.
         */
        SweepScriptData(rt, lock);

        /* Clear out any small pools that we're hanging on to. */
        if (rt->hasJitRuntime()) {
            rt->jitRuntime()->execAlloc().purge();
            rt->jitRuntime()->backedgeExecAlloc().purge();
        }
    }

    {
        gcstats::AutoPhase ap(stats(), gcstats::PHASE_FINALIZE_END);
        callFinalizeCallbacks(&fop, JSFINALIZE_COLLECTION_END);

        /* If we finished a full GC, then the gray bits are correct. */
        if (isFull)
            grayBitsValid = true;
    }

    finishMarkingValidation();

#ifdef DEBUG
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        for (auto i : AllAllocKinds()) {
            MOZ_ASSERT_IF(!IsBackgroundFinalized(i) ||
                          !sweepOnBackgroundThread,
                          !zone->arenas.arenaListsToSweep(i));
        }
    }
#endif

    AssertNoWrappersInGrayList(rt);
}

void
GCRuntime::beginCompactPhase()
{
    MOZ_ASSERT(!isBackgroundSweeping());

    gcstats::AutoPhase ap(stats(), gcstats::PHASE_COMPACT);

    MOZ_ASSERT(zonesToMaybeCompact.ref().isEmpty());
    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        if (CanRelocateZone(zone))
            zonesToMaybeCompact.ref().append(zone);
    }

    MOZ_ASSERT(!relocatedArenasToRelease);
    startedCompacting = true;
}

GCRuntime::IncrementalProgress
GCRuntime::compactPhase(JS::gcreason::Reason reason, SliceBudget& sliceBudget,
                        AutoLockForExclusiveAccess& lock)
{
    assertBackgroundSweepingFinished();
    MOZ_ASSERT(startedCompacting);

    gcstats::AutoPhase ap(stats(), gcstats::PHASE_COMPACT);

    // TODO: JSScripts can move. If the sampler interrupts the GC in the
    // middle of relocating an arena, invalid JSScript pointers may be
    // accessed. Suppress all sampling until a finer-grained solution can be
    // found. See bug 1295775.
    AutoSuppressProfilerSampling suppressSampling(TlsContext.get());

    ZoneList relocatedZones;
    Arena* relocatedArenas = nullptr;
    while (!zonesToMaybeCompact.ref().isEmpty()) {

        Zone* zone = zonesToMaybeCompact.ref().front();
        zonesToMaybeCompact.ref().removeFront();

        MOZ_ASSERT(zone->group()->nursery().isEmpty());
        MOZ_ASSERT(zone->isGCFinished());
        zone->setGCState(Zone::Compact);

        if (relocateArenas(zone, reason, relocatedArenas, sliceBudget)) {
            updateZonePointersToRelocatedCells(zone, lock);
            relocatedZones.append(zone);
        } else {
            zone->setGCState(Zone::Finished);
        }

        if (sliceBudget.isOverBudget())
            break;
    }

    if (!relocatedZones.isEmpty()) {
        updateRuntimePointersToRelocatedCells(lock);

        do {
            Zone* zone = relocatedZones.front();
            relocatedZones.removeFront();
            zone->setGCState(Zone::Finished);
        }
        while (!relocatedZones.isEmpty());
    }

    if (ShouldProtectRelocatedArenas(reason))
        protectAndHoldArenas(relocatedArenas);
    else
        releaseRelocatedArenas(relocatedArenas);

    // Clear caches that can contain cell pointers.
    rt->caches().newObjectCache.purge();
    rt->caches().nativeIterCache.purge();
    if (rt->caches().evalCache.initialized())
        rt->caches().evalCache.clear();

#ifdef DEBUG
    CheckHashTablesAfterMovingGC(rt);
#endif

    return zonesToMaybeCompact.ref().isEmpty() ? Finished : NotFinished;
}

void
GCRuntime::endCompactPhase(JS::gcreason::Reason reason)
{
    startedCompacting = false;
}

void
GCRuntime::finishCollection(JS::gcreason::Reason reason)
{
    assertBackgroundSweepingFinished();
    MOZ_ASSERT(marker.isDrained());
    marker.stop();
    clearBufferedGrayRoots();
    MemProfiler::SweepTenured(rt);

    uint64_t currentTime = PRMJ_Now();
    schedulingState.updateHighFrequencyMode(lastGCTime, currentTime, tunables);

    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        if (zone->isCollecting()) {
            MOZ_ASSERT(zone->isGCFinished());
            zone->setGCState(Zone::NoGC);
        }

        MOZ_ASSERT(!zone->isCollectingFromAnyThread());
        MOZ_ASSERT(!zone->wasGCStarted());
    }

    MOZ_ASSERT(zonesToMaybeCompact.ref().isEmpty());

    lastGCTime = currentTime;
}

static const char*
HeapStateToLabel(JS::HeapState heapState)
{
    switch (heapState) {
      case JS::HeapState::MinorCollecting:
        return "js::Nursery::collect";
      case JS::HeapState::MajorCollecting:
        return "js::GCRuntime::collect";
      case JS::HeapState::Tracing:
        return "JS_IterateCompartments";
      case JS::HeapState::Idle:
      case JS::HeapState::CycleCollecting:
        MOZ_CRASH("Should never have an Idle or CC heap state when pushing GC pseudo frames!");
    }
    MOZ_ASSERT_UNREACHABLE("Should have exhausted every JS::HeapState variant!");
    return nullptr;
}

#ifdef DEBUG
static bool
AllNurseriesAreEmpty(JSRuntime* rt)
{
    for (ZoneGroupsIter group(rt); !group.done(); group.next()) {
        if (!group->nursery().isEmpty())
            return false;
    }
    return true;
}
#endif

/* Start a new heap session. */
AutoTraceSession::AutoTraceSession(JSRuntime* rt, JS::HeapState heapState)
  : lock(rt),
    runtime(rt),
    prevState(TlsContext.get()->heapState),
    pseudoFrame(rt, HeapStateToLabel(heapState), ProfileEntry::Category::GC)
{
    MOZ_ASSERT(prevState == JS::HeapState::Idle);
    MOZ_ASSERT(heapState != JS::HeapState::Idle);
    MOZ_ASSERT_IF(heapState == JS::HeapState::MajorCollecting, AllNurseriesAreEmpty(rt));
    TlsContext.get()->heapState = heapState;
}

AutoTraceSession::~AutoTraceSession()
{
    MOZ_ASSERT(JS::CurrentThreadIsHeapBusy());
    TlsContext.get()->heapState = prevState;
}

JS_PUBLIC_API(JS::HeapState)
JS::CurrentThreadHeapState()
{
    return TlsContext.get()->heapState;
}

bool
GCRuntime::canChangeActiveContext(JSContext* cx)
{
    // Threads cannot be in the middle of any operation that affects GC
    // behavior when execution transfers to another thread for cooperative
    // scheduling.
    return cx->heapState == JS::HeapState::Idle
        && !cx->suppressGC
        && cx->allowGCBarriers
        && !cx->inUnsafeRegion
        && !cx->generationalDisabled
        && !cx->compactingDisabledCount
        && !cx->keepAtoms;
}

GCRuntime::IncrementalResult
GCRuntime::resetIncrementalGC(gc::AbortReason reason, AutoLockForExclusiveAccess& lock)
{
    MOZ_ASSERT(reason != gc::AbortReason::None);

    switch (incrementalState) {
      case State::NotActive:
          return IncrementalResult::Ok;

      case State::MarkRoots:
        MOZ_CRASH("resetIncrementalGC did not expect MarkRoots state");
        break;

      case State::Mark: {
        /* Cancel any ongoing marking. */
        marker.reset();
        marker.stop();
        clearBufferedGrayRoots();

        for (GCCompartmentsIter c(rt); !c.done(); c.next())
            ResetGrayList(c);

        for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
            MOZ_ASSERT(zone->isGCMarking());
            zone->setNeedsIncrementalBarrier(false, Zone::UpdateJit);
            zone->setGCState(Zone::NoGC);
        }

        blocksToFreeAfterSweeping.ref().freeAll();

        incrementalState = State::NotActive;

        MOZ_ASSERT(!marker.shouldCheckCompartments());

        break;
      }

      case State::Sweep: {
        marker.reset();

        for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next())
            c->scheduledForDestruction = false;

        /* Finish sweeping the current zone group, then abort. */
        abortSweepAfterCurrentGroup = true;

        /* Don't perform any compaction after sweeping. */
        bool wasCompacting = isCompacting;
        isCompacting = false;

        auto unlimited = SliceBudget::unlimited();
        incrementalCollectSlice(unlimited, JS::gcreason::RESET, lock);

        isCompacting = wasCompacting;

        {
            gcstats::AutoPhase ap(stats(), gcstats::PHASE_WAIT_BACKGROUND_THREAD);
            rt->gc.waitBackgroundSweepOrAllocEnd();
        }
        break;
      }

      case State::Finalize: {
        {
            gcstats::AutoPhase ap(stats(), gcstats::PHASE_WAIT_BACKGROUND_THREAD);
            rt->gc.waitBackgroundSweepOrAllocEnd();
        }

        bool wasCompacting = isCompacting;
        isCompacting = false;

        auto unlimited = SliceBudget::unlimited();
        incrementalCollectSlice(unlimited, JS::gcreason::RESET, lock);

        isCompacting = wasCompacting;

        break;
      }

      case State::Compact: {
        bool wasCompacting = isCompacting;

        isCompacting = true;
        startedCompacting = true;
        zonesToMaybeCompact.ref().clear();

        auto unlimited = SliceBudget::unlimited();
        incrementalCollectSlice(unlimited, JS::gcreason::RESET, lock);

        isCompacting = wasCompacting;
        break;
      }

      case State::Decommit: {
        auto unlimited = SliceBudget::unlimited();
        incrementalCollectSlice(unlimited, JS::gcreason::RESET, lock);
        break;
      }
    }

    stats().reset(reason);

#ifdef DEBUG
    assertBackgroundSweepingFinished();
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        MOZ_ASSERT(!zone->isCollectingFromAnyThread());
        MOZ_ASSERT(!zone->needsIncrementalBarrier());
        MOZ_ASSERT(!zone->isOnList());
    }
    MOZ_ASSERT(zonesToMaybeCompact.ref().isEmpty());
    MOZ_ASSERT(incrementalState == State::NotActive);
#endif

    return IncrementalResult::Reset;
}

namespace {

class AutoGCSlice {
  public:
    explicit AutoGCSlice(JSRuntime* rt);
    ~AutoGCSlice();

  private:
    JSRuntime* runtime;
    AutoSetThreadIsPerformingGC performingGC;
};

} /* anonymous namespace */

AutoGCSlice::AutoGCSlice(JSRuntime* rt)
  : runtime(rt)
{
    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        /*
         * Clear needsIncrementalBarrier early so we don't do any write
         * barriers during GC. We don't need to update the Ion barriers (which
         * is expensive) because Ion code doesn't run during GC. If need be,
         * we'll update the Ion barriers in ~AutoGCSlice.
         */
        if (zone->isGCMarking()) {
            MOZ_ASSERT(zone->needsIncrementalBarrier());
            zone->setNeedsIncrementalBarrier(false, Zone::DontUpdateJit);
        } else {
            MOZ_ASSERT(!zone->needsIncrementalBarrier());
        }
    }
}

AutoGCSlice::~AutoGCSlice()
{
    /* We can't use GCZonesIter if this is the end of the last slice. */
    for (ZonesIter zone(runtime, WithAtoms); !zone.done(); zone.next()) {
        if (zone->isGCMarking()) {
            zone->setNeedsIncrementalBarrier(true, Zone::UpdateJit);
            zone->arenas.purge();
        } else {
            zone->setNeedsIncrementalBarrier(false, Zone::UpdateJit);
        }
    }
}

void
GCRuntime::pushZealSelectedObjects()
{
#ifdef JS_GC_ZEAL
    /* Push selected objects onto the mark stack and clear the list. */
    for (JSObject** obj = selectedForMarking.ref().begin(); obj != selectedForMarking.ref().end(); obj++)
        TraceManuallyBarrieredEdge(&marker, obj, "selected obj");
#endif
}

static bool
IsShutdownGC(JS::gcreason::Reason reason)
{
    return reason == JS::gcreason::SHUTDOWN_CC || reason == JS::gcreason::DESTROY_RUNTIME;
}

static bool
ShouldCleanUpEverything(JS::gcreason::Reason reason, JSGCInvocationKind gckind)
{
    // During shutdown, we must clean everything up, for the sake of leak
    // detection. When a runtime has no contexts, or we're doing a GC before a
    // shutdown CC, those are strong indications that we're shutting down.
    return IsShutdownGC(reason) || gckind == GC_SHRINK;
}

void
GCRuntime::incrementalCollectSlice(SliceBudget& budget, JS::gcreason::Reason reason,
                                   AutoLockForExclusiveAccess& lock)
{
    AutoGCSlice slice(rt);

    bool destroyingRuntime = (reason == JS::gcreason::DESTROY_RUNTIME);

    gc::State initialState = incrementalState;

    bool useZeal = false;
#ifdef JS_GC_ZEAL
    if (reason == JS::gcreason::DEBUG_GC && !budget.isUnlimited()) {
        /*
         * Do the incremental collection type specified by zeal mode if the
         * collection was triggered by runDebugGC() and incremental GC has not
         * been cancelled by resetIncrementalGC().
         */
        useZeal = true;
    }
#endif

    MOZ_ASSERT_IF(isIncrementalGCInProgress(), isIncremental);
    isIncremental = !budget.isUnlimited();

    if (useZeal && (hasZealMode(ZealMode::IncrementalRootsThenFinish) ||
                    hasZealMode(ZealMode::IncrementalMarkAllThenFinish)))
    {
        /*
         * Yields between slices occurs at predetermined points in these modes;
         * the budget is not used.
         */
        budget.makeUnlimited();
    }

    switch (incrementalState) {
      case State::NotActive:
        initialReason = reason;
        cleanUpEverything = ShouldCleanUpEverything(reason, invocationKind);
        isCompacting = shouldCompact();
        lastMarkSlice = false;

        incrementalState = State::MarkRoots;

        MOZ_FALLTHROUGH;

      case State::MarkRoots:
        if (!beginMarkPhase(reason, lock)) {
            incrementalState = State::NotActive;
            return;
        }

        if (!destroyingRuntime)
            pushZealSelectedObjects();

        incrementalState = State::Mark;

        if (isIncremental && useZeal && hasZealMode(ZealMode::IncrementalRootsThenFinish))
            break;

        MOZ_FALLTHROUGH;

      case State::Mark:
        for (const CooperatingContext& target : rt->cooperatingContexts())
            AutoGCRooter::traceAllWrappers(target, &marker);

        /* If we needed delayed marking for gray roots, then collect until done. */
        if (!hasBufferedGrayRoots()) {
            budget.makeUnlimited();
            isIncremental = false;
        }

        if (drainMarkStack(budget, gcstats::PHASE_MARK) == NotFinished)
            break;

        MOZ_ASSERT(marker.isDrained());

        /*
         * In incremental GCs where we have already performed more than once
         * slice we yield after marking with the aim of starting the sweep in
         * the next slice, since the first slice of sweeping can be expensive.
         *
         * This is modified by the various zeal modes.  We don't yield in
         * IncrementalRootsThenFinish mode and we always yield in
         * IncrementalMarkAllThenFinish mode.
         *
         * We will need to mark anything new on the stack when we resume, so
         * we stay in Mark state.
         */
        if (!lastMarkSlice && isIncremental &&
            ((initialState == State::Mark &&
              !(useZeal && hasZealMode(ZealMode::IncrementalRootsThenFinish))) ||
             (useZeal && hasZealMode(ZealMode::IncrementalMarkAllThenFinish))))
        {
            lastMarkSlice = true;
            break;
        }

        incrementalState = State::Sweep;

        /*
         * This runs to completion, but we don't continue if the budget is
         * now exhasted.
         */
        beginSweepPhase(destroyingRuntime, lock);
        if (budget.isOverBudget())
            break;

        /*
         * Always yield here when running in incremental multi-slice zeal
         * mode, so RunDebugGC can reset the slice buget.
         */
        if (isIncremental && useZeal && hasZealMode(ZealMode::IncrementalMultipleSlices))
            break;

        MOZ_FALLTHROUGH;

      case State::Sweep:
        if (sweepPhase(budget, lock) == NotFinished)
            break;

        endSweepPhase(destroyingRuntime, lock);

        incrementalState = State::Finalize;

        MOZ_FALLTHROUGH;

      case State::Finalize:
        {
            gcstats::AutoPhase ap(stats(), gcstats::PHASE_WAIT_BACKGROUND_THREAD);

            // Yield until background finalization is done.
            if (isIncremental) {
                // Poll for end of background sweeping
                AutoLockGC lock(rt);
                if (isBackgroundSweeping())
                    break;
            } else {
                waitBackgroundSweepEnd();
            }
        }

        {
            // Re-sweep the zones list, now that background finalization is
            // finished to actually remove and free dead zones.
            gcstats::AutoPhase ap1(stats(), gcstats::PHASE_SWEEP);
            gcstats::AutoPhase ap2(stats(), gcstats::PHASE_DESTROY);
            AutoSetThreadIsSweeping threadIsSweeping;
            FreeOp fop(rt);
            sweepZoneGroups(&fop, destroyingRuntime);
        }

        MOZ_ASSERT(!startedCompacting);
        incrementalState = State::Compact;

        // Always yield before compacting since it is not incremental.
        if (isCompacting && isIncremental)
            break;

        MOZ_FALLTHROUGH;

      case State::Compact:
        if (isCompacting) {
            if (!startedCompacting)
                beginCompactPhase();

            if (compactPhase(reason, budget, lock) == NotFinished)
                break;

            endCompactPhase(reason);
        }

        startDecommit();
        incrementalState = State::Decommit;

        MOZ_FALLTHROUGH;

      case State::Decommit:
        {
            gcstats::AutoPhase ap(stats(), gcstats::PHASE_WAIT_BACKGROUND_THREAD);

            // Yield until background decommit is done.
            if (isIncremental && decommitTask.isRunning())
                break;

            decommitTask.join();
        }

        finishCollection(reason);
        incrementalState = State::NotActive;
        break;
    }
}

gc::AbortReason
gc::IsIncrementalGCUnsafe(JSRuntime* rt)
{
    MOZ_ASSERT(!TlsContext.get()->suppressGC);

    if (!rt->gc.isIncrementalGCAllowed())
        return gc::AbortReason::IncrementalDisabled;

    return gc::AbortReason::None;
}

GCRuntime::IncrementalResult
GCRuntime::budgetIncrementalGC(bool nonincrementalByAPI, JS::gcreason::Reason reason,
                               SliceBudget& budget, AutoLockForExclusiveAccess& lock)
{
    if (nonincrementalByAPI) {
        stats().nonincremental(gc::AbortReason::NonIncrementalRequested);
        budget.makeUnlimited();

        // Reset any in progress incremental GC if this was triggered via the
        // API. This isn't required for correctness, but sometimes during tests
        // the caller expects this GC to collect certain objects, and we need
        // to make sure to collect everything possible.
        if (reason != JS::gcreason::ALLOC_TRIGGER)
            return resetIncrementalGC(gc::AbortReason::NonIncrementalRequested, lock);

        return IncrementalResult::Ok;
    }

    if (reason == JS::gcreason::ABORT_GC) {
        budget.makeUnlimited();
        stats().nonincremental(gc::AbortReason::AbortRequested);
        return resetIncrementalGC(gc::AbortReason::AbortRequested, lock);
    }

    AbortReason unsafeReason = IsIncrementalGCUnsafe(rt);
    if (unsafeReason == AbortReason::None) {
        if (reason == JS::gcreason::COMPARTMENT_REVIVED)
            unsafeReason = gc::AbortReason::CompartmentRevived;
        else if (mode != JSGC_MODE_INCREMENTAL)
            unsafeReason = gc::AbortReason::ModeChange;
    }

    if (unsafeReason != AbortReason::None) {
        budget.makeUnlimited();
        stats().nonincremental(unsafeReason);
        return resetIncrementalGC(unsafeReason, lock);
    }

    if (isTooMuchMalloc()) {
        budget.makeUnlimited();
        stats().nonincremental(AbortReason::MallocBytesTrigger);
    }

    bool reset = false;
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        if (zone->usage.gcBytes() >= zone->threshold.gcTriggerBytes()) {
            budget.makeUnlimited();
            stats().nonincremental(AbortReason::GCBytesTrigger);
        }

        if (isIncrementalGCInProgress() && zone->isGCScheduled() != zone->wasGCStarted())
            reset = true;

        if (zone->isTooMuchMalloc()) {
            budget.makeUnlimited();
            stats().nonincremental(AbortReason::MallocBytesTrigger);
        }
    }

    if (reset)
        return resetIncrementalGC(AbortReason::ZoneChange, lock);

    return IncrementalResult::Ok;
}

namespace {

class AutoScheduleZonesForGC
{
    JSRuntime* rt_;

  public:
    explicit AutoScheduleZonesForGC(JSRuntime* rt) : rt_(rt) {
        for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
            if (rt->gc.gcMode() == JSGC_MODE_GLOBAL)
                zone->scheduleGC();

            /* This is a heuristic to avoid resets. */
            if (rt->gc.isIncrementalGCInProgress() && zone->needsIncrementalBarrier())
                zone->scheduleGC();

            /* This is a heuristic to reduce the total number of collections. */
            if (zone->usage.gcBytes() >=
                zone->threshold.allocTrigger(rt->gc.schedulingState.inHighFrequencyGCMode()))
            {
                zone->scheduleGC();
            }
        }
    }

    ~AutoScheduleZonesForGC() {
        for (ZonesIter zone(rt_, WithAtoms); !zone.done(); zone.next())
            zone->unscheduleGC();
    }
};

/*
 * An invariant of our GC/CC interaction is that there must not ever be any
 * black to gray edges in the system. It is possible to violate this with
 * simple compartmental GC. For example, in GC[n], we collect in both
 * compartmentA and compartmentB, and mark both sides of the cross-compartment
 * edge gray. Later in GC[n+1], we only collect compartmentA, but this time
 * mark it black. Now we are violating the invariants and must fix it somehow.
 *
 * To prevent this situation, we explicitly detect the black->gray state when
 * marking cross-compartment edges -- see ShouldMarkCrossCompartment -- adding
 * each violating edges to foundBlackGrayEdges. After we leave the trace
 * session for each GC slice, we "ExposeToActiveJS" on each of these edges
 * (which we cannot do safely from the guts of the GC).
 */
class AutoExposeLiveCrossZoneEdges
{
    BlackGrayEdgeVector* edges;

  public:
    explicit AutoExposeLiveCrossZoneEdges(BlackGrayEdgeVector* edgesPtr) : edges(edgesPtr) {
        MOZ_ASSERT(edges->empty());
    }
    ~AutoExposeLiveCrossZoneEdges() {
        for (auto& target : *edges) {
            MOZ_ASSERT(target);
            MOZ_ASSERT(!target->zone()->isCollecting());
            UnmarkGrayCellRecursively(target, target->getTraceKind());
        }
        edges->clear();
    }
};

} /* anonymous namespace */

/*
 * Run one GC "cycle" (either a slice of incremental GC or an entire
 * non-incremental GC. We disable inlining to ensure that the bottom of the
 * stack with possible GC roots recorded in MarkRuntime excludes any pointers we
 * use during the marking implementation.
 *
 * Returns true if we "reset" an existing incremental GC, which would force us
 * to run another cycle.
 */
MOZ_NEVER_INLINE GCRuntime::IncrementalResult
GCRuntime::gcCycle(bool nonincrementalByAPI, SliceBudget& budget, JS::gcreason::Reason reason)
{
    // Note that the following is allowed to re-enter GC in the finalizer.
    AutoNotifyGCActivity notify(*this);

    gcstats::AutoGCSlice agc(stats(), scanZonesBeforeGC(), invocationKind, budget, reason);

    AutoExposeLiveCrossZoneEdges aelcze(&foundBlackGrayEdges.ref());

    EvictAllNurseries(rt, reason);

    AutoTraceSession session(rt, JS::HeapState::MajorCollecting);

    majorGCTriggerReason = JS::gcreason::NO_REASON;
    interFrameGC = true;

    number++;
    if (!isIncrementalGCInProgress())
        incMajorGcNumber();

    // It's ok if threads other than the active thread have suppressGC set, as
    // they are operating on zones which will not be collected from here.
    MOZ_ASSERT(!TlsContext.get()->suppressGC);

    // Assert if this is a GC unsafe region.
    TlsContext.get()->verifyIsSafeToGC();

    {
        gcstats::AutoPhase ap(stats(), gcstats::PHASE_WAIT_BACKGROUND_THREAD);

        // Background finalization and decommit are finished by defininition
        // before we can start a new GC session.
        if (!isIncrementalGCInProgress()) {
            assertBackgroundSweepingFinished();
            MOZ_ASSERT(!decommitTask.isRunning());
        }

        // We must also wait for background allocation to finish so we can
        // avoid taking the GC lock when manipulating the chunks during the GC.
        // The background alloc task can run between slices, so we must wait
        // for it at the start of every slice.
        allocTask.cancel(GCParallelTask::CancelAndWait);
    }

    // We don't allow off-thread parsing to start while we're doing an
    // incremental GC.
    MOZ_ASSERT_IF(rt->activeGCInAtomsZone(), !rt->hasHelperThreadZones());

    auto result = budgetIncrementalGC(nonincrementalByAPI, reason, budget, session.lock);

    // If an ongoing incremental GC was reset, we may need to restart.
    if (result == IncrementalResult::Reset) {
        MOZ_ASSERT(!isIncrementalGCInProgress());
        return result;
    }

    TraceMajorGCStart();

    incrementalCollectSlice(budget, reason, session.lock);

    chunkAllocationSinceLastGC = false;

#ifdef JS_GC_ZEAL
    /* Keeping these around after a GC is dangerous. */
    clearSelectedForMarking();
#endif

    /* Clear gcMallocBytes for all zones. */
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
        zone->resetAllMallocBytes();

    resetMallocBytes();

    TraceMajorGCEnd();

    return IncrementalResult::Ok;
}

#ifdef JS_GC_ZEAL
static bool
IsDeterministicGCReason(JS::gcreason::Reason reason)
{
    switch (reason) {
      case JS::gcreason::API:
      case JS::gcreason::DESTROY_RUNTIME:
      case JS::gcreason::LAST_DITCH:
      case JS::gcreason::TOO_MUCH_MALLOC:
      case JS::gcreason::ALLOC_TRIGGER:
      case JS::gcreason::DEBUG_GC:
      case JS::gcreason::CC_FORCED:
      case JS::gcreason::SHUTDOWN_CC:
      case JS::gcreason::ABORT_GC:
        return true;

      default:
        return false;
    }
}
#endif

gcstats::ZoneGCStats
GCRuntime::scanZonesBeforeGC()
{
    gcstats::ZoneGCStats zoneStats;
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        zoneStats.zoneCount++;
        if (zone->isGCScheduled()) {
            zoneStats.collectedZoneCount++;
            zoneStats.collectedCompartmentCount += zone->compartments().length();
        }
    }

    for (CompartmentsIter c(rt, WithAtoms); !c.done(); c.next())
        zoneStats.compartmentCount++;

    return zoneStats;
}

// The GC can only clean up scheduledForDestruction compartments that were
// marked live by a barrier (e.g. by RemapWrappers from a navigation event).
// It is also common to have compartments held live because they are part of a
// cycle in gecko, e.g. involving the HTMLDocument wrapper. In this case, we
// need to run the CycleCollector in order to remove these edges before the
// compartment can be freed.
void
GCRuntime::maybeDoCycleCollection()
{
    const static double ExcessiveGrayCompartments = 0.8;
    const static size_t LimitGrayCompartments = 200;

    size_t compartmentsTotal = 0;
    size_t compartmentsGray = 0;
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
        ++compartmentsTotal;
        GlobalObject* global = c->unsafeUnbarrieredMaybeGlobal();
        if (global && global->asTenured().isMarked(GRAY))
            ++compartmentsGray;
    }
    double grayFraction = double(compartmentsGray) / double(compartmentsTotal);
    if (grayFraction > ExcessiveGrayCompartments || compartmentsGray > LimitGrayCompartments)
        callDoCycleCollectionCallback(rt->activeContextFromOwnThread());
}

void
GCRuntime::checkCanCallAPI()
{
    MOZ_RELEASE_ASSERT(CurrentThreadCanAccessRuntime(rt));

    /* If we attempt to invoke the GC while we are running in the GC, assert. */
    MOZ_RELEASE_ASSERT(!JS::CurrentThreadIsHeapBusy());

    MOZ_ASSERT(TlsContext.get()->isAllocAllowed());
}

bool
GCRuntime::checkIfGCAllowedInCurrentState(JS::gcreason::Reason reason)
{
    if (TlsContext.get()->suppressGC)
        return false;

    // Only allow shutdown GCs when we're destroying the runtime. This keeps
    // the GC callback from triggering a nested GC and resetting global state.
    if (rt->isBeingDestroyed() && !IsShutdownGC(reason))
        return false;

#ifdef JS_GC_ZEAL
    if (deterministicOnly && !IsDeterministicGCReason(reason))
        return false;
#endif

    return true;
}

bool
GCRuntime::shouldRepeatForDeadZone(JS::gcreason::Reason reason)
{
    MOZ_ASSERT_IF(reason == JS::gcreason::COMPARTMENT_REVIVED, !isIncremental);

    if (!isIncremental || isIncrementalGCInProgress())
        return false;

    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
        if (c->scheduledForDestruction)
            return true;
    }

    return false;
}

void
GCRuntime::collect(bool nonincrementalByAPI, SliceBudget budget, JS::gcreason::Reason reason)
{
    // Checks run for each request, even if we do not actually GC.
    checkCanCallAPI();

    // Check if we are allowed to GC at this time before proceeding.
    if (!checkIfGCAllowedInCurrentState(reason))
        return;

    AutoTraceLog logGC(TraceLoggerForCurrentThread(), TraceLogger_GC);
    AutoStopVerifyingBarriers av(rt, IsShutdownGC(reason));
    AutoEnqueuePendingParseTasksAfterGC aept(*this);
    AutoScheduleZonesForGC asz(rt);

    bool repeat = false;
    do {
        poked = false;
        bool wasReset = gcCycle(nonincrementalByAPI, budget, reason) == IncrementalResult::Reset;

        if (reason == JS::gcreason::ABORT_GC) {
            MOZ_ASSERT(!isIncrementalGCInProgress());
            break;
        }

        bool repeatForDeadZone = false;
        if (poked && cleanUpEverything) {
            /* Need to re-schedule all zones for GC. */
            JS::PrepareForFullGC(rt->activeContextFromOwnThread());
        } else if (shouldRepeatForDeadZone(reason) && !wasReset) {
            /*
             * This code makes an extra effort to collect compartments that we
             * thought were dead at the start of the GC. See the large comment
             * in beginMarkPhase.
             */
            repeatForDeadZone = true;
            reason = JS::gcreason::COMPARTMENT_REVIVED;
        }

        /*
         * If we reset an existing GC, we need to start a new one. Also, we
         * repeat GCs that happen during shutdown (the gcShouldCleanUpEverything
         * case) until we can be sure that no additional garbage is created
         * (which typically happens if roots are dropped during finalizers).
         */
        repeat = (poked && cleanUpEverything) || wasReset || repeatForDeadZone;
    } while (repeat);

    if (reason == JS::gcreason::COMPARTMENT_REVIVED)
        maybeDoCycleCollection();

#ifdef JS_GC_ZEAL
    if (rt->hasZealMode(ZealMode::CheckHeapAfterGC)) {
        gcstats::AutoPhase ap(rt->gc.stats(), gcstats::PHASE_TRACE_HEAP);
        CheckHeapAfterGC(rt);
    }
#endif
}

js::AutoEnqueuePendingParseTasksAfterGC::~AutoEnqueuePendingParseTasksAfterGC()
{
    if (!OffThreadParsingMustWaitForGC(gc_.rt))
        EnqueuePendingParseTasksAfterGC(gc_.rt);
}

SliceBudget
GCRuntime::defaultBudget(JS::gcreason::Reason reason, int64_t millis)
{
    if (millis == 0) {
        if (reason == JS::gcreason::ALLOC_TRIGGER)
            millis = defaultSliceBudget();
        else if (schedulingState.inHighFrequencyGCMode() && tunables.isDynamicMarkSliceEnabled())
            millis = defaultSliceBudget() * IGC_MARK_SLICE_MULTIPLIER;
        else
            millis = defaultSliceBudget();
    }

    return SliceBudget(TimeBudget(millis));
}

void
GCRuntime::gc(JSGCInvocationKind gckind, JS::gcreason::Reason reason)
{
    invocationKind = gckind;
    collect(true, SliceBudget::unlimited(), reason);
}

void
GCRuntime::startGC(JSGCInvocationKind gckind, JS::gcreason::Reason reason, int64_t millis)
{
    MOZ_ASSERT(!isIncrementalGCInProgress());
    if (!JS::IsIncrementalGCEnabled(TlsContext.get())) {
        gc(gckind, reason);
        return;
    }
    invocationKind = gckind;
    collect(false, defaultBudget(reason, millis), reason);
}

void
GCRuntime::gcSlice(JS::gcreason::Reason reason, int64_t millis)
{
    MOZ_ASSERT(isIncrementalGCInProgress());
    collect(false, defaultBudget(reason, millis), reason);
}

void
GCRuntime::finishGC(JS::gcreason::Reason reason)
{
    MOZ_ASSERT(isIncrementalGCInProgress());

    // If we're not collecting because we're out of memory then skip the
    // compacting phase if we need to finish an ongoing incremental GC
    // non-incrementally to avoid janking the browser.
    if (!IsOOMReason(initialReason)) {
        if (incrementalState == State::Compact) {
            abortGC();
            return;
        }

        isCompacting = false;
    }

    collect(false, SliceBudget::unlimited(), reason);
}

void
GCRuntime::abortGC()
{
    MOZ_ASSERT(isIncrementalGCInProgress());
    checkCanCallAPI();
    MOZ_ASSERT(!TlsContext.get()->suppressGC);

    collect(false, SliceBudget::unlimited(), JS::gcreason::ABORT_GC);
}

void
GCRuntime::notifyDidPaint()
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));

#ifdef JS_GC_ZEAL
    if (hasZealMode(ZealMode::FrameVerifierPre))
        verifyPreBarriers();

    if (hasZealMode(ZealMode::FrameGC)) {
        JS::PrepareForFullGC(rt->activeContextFromOwnThread());
        gc(GC_NORMAL, JS::gcreason::REFRESH_FRAME);
        return;
    }
#endif

    if (isIncrementalGCInProgress() && !interFrameGC && tunables.areRefreshFrameSlicesEnabled()) {
        JS::PrepareForIncrementalGC(rt->activeContextFromOwnThread());
        gcSlice(JS::gcreason::REFRESH_FRAME);
    }

    interFrameGC = false;
}

static bool
ZonesSelected(JSRuntime* rt)
{
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        if (zone->isGCScheduled())
            return true;
    }
    return false;
}

void
GCRuntime::startDebugGC(JSGCInvocationKind gckind, SliceBudget& budget)
{
    MOZ_ASSERT(!isIncrementalGCInProgress());
    if (!ZonesSelected(rt))
        JS::PrepareForFullGC(rt->activeContextFromOwnThread());
    invocationKind = gckind;
    collect(false, budget, JS::gcreason::DEBUG_GC);
}

void
GCRuntime::debugGCSlice(SliceBudget& budget)
{
    MOZ_ASSERT(isIncrementalGCInProgress());
    if (!ZonesSelected(rt))
        JS::PrepareForIncrementalGC(rt->activeContextFromOwnThread());
    collect(false, budget, JS::gcreason::DEBUG_GC);
}

/* Schedule a full GC unless a zone will already be collected. */
void
js::PrepareForDebugGC(JSRuntime* rt)
{
    if (!ZonesSelected(rt))
        JS::PrepareForFullGC(rt->activeContextFromOwnThread());
}

void
GCRuntime::onOutOfMallocMemory()
{
    // Stop allocating new chunks.
    allocTask.cancel(GCParallelTask::CancelAndWait);

    // Make sure we release anything queued for release.
    decommitTask.join();

    // Wait for background free of nursery huge slots to finish.
    for (ZoneGroupsIter group(rt); !group.done(); group.next())
        group->nursery().waitBackgroundFreeEnd();

    AutoLockGC lock(rt);
    onOutOfMallocMemory(lock);
}

void
GCRuntime::onOutOfMallocMemory(const AutoLockGC& lock)
{
    // Release any relocated arenas we may be holding on to, without releasing
    // the GC lock.
    releaseHeldRelocatedArenasWithoutUnlocking(lock);

    // Throw away any excess chunks we have lying around.
    freeEmptyChunks(rt, lock);

    // Immediately decommit as many arenas as possible in the hopes that this
    // might let the OS scrape together enough pages to satisfy the failing
    // malloc request.
    decommitAllWithoutUnlocking(lock);
}

void
GCRuntime::minorGC(JS::gcreason::Reason reason, gcstats::Phase phase)
{
    MOZ_ASSERT(!JS::CurrentThreadIsHeapBusy());

    if (TlsContext.get()->suppressGC)
        return;

    gcstats::AutoPhase ap(rt->gc.stats(), phase);

    nursery().clearMinorGCRequest();
    TraceLoggerThread* logger = TraceLoggerForCurrentThread();
    AutoTraceLog logMinorGC(logger, TraceLogger_MinorGC);
    nursery().collect(reason);
    MOZ_ASSERT(nursery().isEmpty());

    blocksToFreeAfterMinorGC.ref().freeAll();

#ifdef JS_GC_ZEAL
    if (rt->hasZealMode(ZealMode::CheckHeapAfterGC))
        CheckHeapAfterGC(rt);
#endif

    {
        AutoLockGC lock(rt);
        for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
            maybeAllocTriggerZoneGC(zone, lock);
    }
}

JS::AutoDisableGenerationalGC::AutoDisableGenerationalGC(JSContext* cx)
  : cx(cx)
{
    if (!cx->generationalDisabled) {
        cx->runtime()->gc.evictNursery(JS::gcreason::API);
        cx->nursery().disable();
    }
    ++cx->generationalDisabled;
}

JS::AutoDisableGenerationalGC::~AutoDisableGenerationalGC()
{
    if (--cx->generationalDisabled == 0) {
        for (ZoneGroupsIter group(cx->runtime()); !group.done(); group.next())
            group->nursery().enable();
    }
}

JS_PUBLIC_API(bool)
JS::IsGenerationalGCEnabled(JSRuntime* rt)
{
    return !TlsContext.get()->generationalDisabled;
}

bool
GCRuntime::gcIfRequested()
{
    // This method returns whether a major GC was performed.

    if (nursery().minorGCRequested())
        minorGC(nursery().minorGCTriggerReason());

    if (majorGCRequested()) {
        if (!isIncrementalGCInProgress())
            startGC(GC_NORMAL, majorGCTriggerReason);
        else
            gcSlice(majorGCTriggerReason);
        return true;
    }

    return false;
}

void
js::gc::FinishGC(JSContext* cx)
{
    if (JS::IsIncrementalGCInProgress(cx)) {
        JS::PrepareForIncrementalGC(cx);
        JS::FinishIncrementalGC(cx, JS::gcreason::API);
    }

    for (ZoneGroupsIter group(cx->runtime()); !group.done(); group.next())
        group->nursery().waitBackgroundFreeEnd();
}

AutoPrepareForTracing::AutoPrepareForTracing(JSContext* cx, ZoneSelector selector)
{
    js::gc::FinishGC(cx);
    session_.emplace(cx->runtime());
}

JSCompartment*
js::NewCompartment(JSContext* cx, JSPrincipals* principals,
                   const JS::CompartmentOptions& options)
{
    JSRuntime* rt = cx->runtime();
    JS_AbortIfWrongThread(cx);

    ScopedJSDeletePtr<ZoneGroup> groupHolder;
    ScopedJSDeletePtr<Zone> zoneHolder;

    Zone* zone = nullptr;
    ZoneGroup* group = nullptr;
    JS::ZoneSpecifier zoneSpec = options.creationOptions().zoneSpecifier();
    switch (zoneSpec) {
      case JS::SystemZone:
        // systemZone and possibly systemZoneGroup might be null here, in which
        // case we'll make a zone/group and set these fields below.
        zone = rt->gc.systemZone;
        group = rt->gc.systemZoneGroup;
        break;
      case JS::ExistingZone:
        zone = static_cast<Zone*>(options.creationOptions().zonePointer());
        MOZ_ASSERT(zone);
        group = zone->group();
        break;
      case JS::NewZoneInNewZoneGroup:
        break;
      case JS::NewZoneInSystemZoneGroup:
        // As above, systemZoneGroup might be null here.
        group = rt->gc.systemZoneGroup;
        break;
      case JS::NewZoneInExistingZoneGroup:
        group = static_cast<ZoneGroup*>(options.creationOptions().zonePointer());
        MOZ_ASSERT(group);
        break;
    }

    if (group) {
        // Take over ownership of the group while we create the compartment/zone.
        group->enter();
    } else {
        MOZ_ASSERT(!zone);
        group = cx->new_<ZoneGroup>(rt);
        if (!group)
            return nullptr;

        groupHolder.reset(group);

        if (!group->init()) {
            ReportOutOfMemory(cx);
            return nullptr;
        }

        if (cx->generationalDisabled)
            group->nursery().disable();
    }

    if (!zone) {
        zone = cx->new_<Zone>(cx->runtime(), group);
        if (!zone)
            return nullptr;

        zoneHolder.reset(zone);

        const JSPrincipals* trusted = rt->trustedPrincipals();
        bool isSystem = principals && principals == trusted;
        if (!zone->init(isSystem)) {
            ReportOutOfMemory(cx);
            return nullptr;
        }
    }

    ScopedJSDeletePtr<JSCompartment> compartment(cx->new_<JSCompartment>(zone, options));
    if (!compartment || !compartment->init(cx))
        return nullptr;

    // Set up the principals.
    JS_SetCompartmentPrincipals(compartment, principals);

    AutoLockGC lock(rt);

    if (!zone->compartments().append(compartment.get())) {
        ReportOutOfMemory(cx);
        return nullptr;
    }

    if (zoneHolder) {
        if (!group->zones().append(zone)) {
            ReportOutOfMemory(cx);
            return nullptr;
        }

        // Lazily set the runtime's sytem zone.
        if (zoneSpec == JS::SystemZone) {
            MOZ_RELEASE_ASSERT(!rt->gc.systemZone);
            rt->gc.systemZone = zone;
            zone->isSystem = true;
        }
    }

    if (groupHolder) {
        if (!rt->gc.groups.ref().append(group)) {
            ReportOutOfMemory(cx);
            return nullptr;
        }

        // Lazily set the runtime's system zone group.
        if (zoneSpec == JS::SystemZone || zoneSpec == JS::NewZoneInSystemZoneGroup) {
            MOZ_RELEASE_ASSERT(!rt->gc.systemZoneGroup);
            rt->gc.systemZoneGroup = group;
        }
    }

    zoneHolder.forget();
    groupHolder.forget();
    group->leave();
    return compartment.forget();
}

void
gc::MergeCompartments(JSCompartment* source, JSCompartment* target)
{
    // The source compartment must be specifically flagged as mergable.  This
    // also implies that the compartment is not visible to the debugger.
    MOZ_ASSERT(source->creationOptions_.mergeable());
    MOZ_ASSERT(source->creationOptions_.invisibleToDebugger());

    MOZ_ASSERT(source->creationOptions().addonIdOrNull() ==
               target->creationOptions().addonIdOrNull());

    JSContext* cx = source->runtimeFromActiveCooperatingThread()->activeContextFromOwnThread();

    AutoPrepareForTracing prepare(cx, SkipAtoms);

    // Cleanup tables and other state in the source compartment that will be
    // meaningless after merging into the target compartment.

    source->clearTables();
    source->zone()->clearTables();
    source->unsetIsDebuggee();

    // The delazification flag indicates the presence of LazyScripts in a
    // compartment for the Debugger API, so if the source compartment created
    // LazyScripts, the flag must be propagated to the target compartment.
    if (source->needsDelazificationForDebugger())
        target->scheduleDelazificationForDebugger();

    // Release any relocated arenas which we may be holding on to as they might
    // be in the source zone
    cx->runtime()->gc.releaseHeldRelocatedArenas();

    // Fixup compartment pointers in source to refer to target, and make sure
    // type information generations are in sync.

    for (auto script = source->zone()->cellIter<JSScript>(); !script.done(); script.next()) {
        MOZ_ASSERT(script->compartment() == source);
        script->compartment_ = target;
        script->setTypesGeneration(target->zone()->types.generation);
    }

    for (auto group = source->zone()->cellIter<ObjectGroup>(); !group.done(); group.next()) {
        group->setGeneration(target->zone()->types.generation);
        group->compartment_ = target;

        // Remove any unboxed layouts from the list in the off thread
        // compartment. These do not need to be reinserted in the target
        // compartment's list, as the list is not required to be complete.
        if (UnboxedLayout* layout = group->maybeUnboxedLayoutDontCheckGeneration())
            layout->detachFromCompartment();
    }

    // Fixup zone pointers in source's zone to refer to target's zone.

    for (auto thingKind : AllAllocKinds()) {
        for (ArenaIter aiter(source->zone(), thingKind); !aiter.done(); aiter.next()) {
            Arena* arena = aiter.get();
            arena->zone = target->zone();
        }
    }

    // The source should be the only compartment in its zone.
    for (CompartmentsInZoneIter c(source->zone()); !c.done(); c.next())
        MOZ_ASSERT(c.get() == source);

    // Merge the allocator, stats and UIDs in source's zone into target's zone.
    target->zone()->arenas.adoptArenas(cx->runtime(), &source->zone()->arenas);
    target->zone()->usage.adopt(source->zone()->usage);
    target->zone()->adoptUniqueIds(source->zone());

    // Merge other info in source's zone into target's zone.
    target->zone()->types.typeLifoAlloc().transferFrom(&source->zone()->types.typeLifoAlloc());

    // Atoms which are marked in source's zone are now marked in target's zone.
    cx->atomMarking().adoptMarkedAtoms(target->zone(), source->zone());
}

void
GCRuntime::runDebugGC()
{
#ifdef JS_GC_ZEAL
    if (TlsContext.get()->suppressGC)
        return;

    if (hasZealMode(ZealMode::GenerationalGC))
        return minorGC(JS::gcreason::DEBUG_GC);

    PrepareForDebugGC(rt);

    auto budget = SliceBudget::unlimited();
    if (hasZealMode(ZealMode::IncrementalRootsThenFinish) ||
        hasZealMode(ZealMode::IncrementalMarkAllThenFinish) ||
        hasZealMode(ZealMode::IncrementalMultipleSlices))
    {
        js::gc::State initialState = incrementalState;
        if (hasZealMode(ZealMode::IncrementalMultipleSlices)) {
            /*
             * Start with a small slice limit and double it every slice. This
             * ensure that we get multiple slices, and collection runs to
             * completion.
             */
            if (!isIncrementalGCInProgress())
                incrementalLimit = zealFrequency / 2;
            else
                incrementalLimit *= 2;
            budget = SliceBudget(WorkBudget(incrementalLimit));
        } else {
            // This triggers incremental GC but is actually ignored by IncrementalMarkSlice.
            budget = SliceBudget(WorkBudget(1));
        }

        if (!isIncrementalGCInProgress())
            invocationKind = GC_SHRINK;
        collect(false, budget, JS::gcreason::DEBUG_GC);

        /*
         * For multi-slice zeal, reset the slice size when we get to the sweep
         * or compact phases.
         */
        if (hasZealMode(ZealMode::IncrementalMultipleSlices)) {
            if ((initialState == State::Mark && incrementalState == State::Sweep) ||
                (initialState == State::Sweep && incrementalState == State::Compact))
            {
                incrementalLimit = zealFrequency / 2;
            }
        }
    } else if (hasZealMode(ZealMode::Compact)) {
        gc(GC_SHRINK, JS::gcreason::DEBUG_GC);
    } else {
        gc(GC_NORMAL, JS::gcreason::DEBUG_GC);
    }

#endif
}

void
GCRuntime::setFullCompartmentChecks(bool enabled)
{
    MOZ_ASSERT(!JS::CurrentThreadIsHeapMajorCollecting());
    fullCompartmentChecks = enabled;
}

#ifdef JS_GC_ZEAL
bool
GCRuntime::selectForMarking(JSObject* object)
{
    MOZ_ASSERT(!JS::CurrentThreadIsHeapMajorCollecting());
    return selectedForMarking.ref().append(object);
}

void
GCRuntime::clearSelectedForMarking()
{
    selectedForMarking.ref().clearAndFree();
}

void
GCRuntime::setDeterministic(bool enabled)
{
    MOZ_ASSERT(!JS::CurrentThreadIsHeapMajorCollecting());
    deterministicOnly = enabled;
}
#endif

#ifdef DEBUG

/* Should only be called manually under gdb */
void PreventGCDuringInteractiveDebug()
{
    TlsContext.get()->suppressGC++;
}

#endif

void
js::ReleaseAllJITCode(FreeOp* fop)
{
    js::CancelOffThreadIonCompile(fop->runtime());

    JSRuntime::AutoProhibitActiveContextChange apacc(fop->runtime());
    for (ZonesIter zone(fop->runtime(), SkipAtoms); !zone.done(); zone.next()) {
        zone->setPreservingCode(false);
        zone->discardJitCode(fop);
    }
}

void
js::PurgeJITCaches(Zone* zone)
{
    /* Discard Ion caches. */
    for (auto script = zone->cellIter<JSScript>(); !script.done(); script.next())
        jit::PurgeCaches(script);
}

void
ArenaLists::normalizeBackgroundFinalizeState(AllocKind thingKind)
{
    ArenaLists::BackgroundFinalizeState* bfs = &backgroundFinalizeState(thingKind);
    switch (*bfs) {
      case BFS_DONE:
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Background finalization in progress, but it should not be.");
        break;
    }
}

void
ArenaLists::adoptArenas(JSRuntime* rt, ArenaLists* fromArenaLists)
{
    // GC should be inactive, but still take the lock as a kind of read fence.
    AutoLockGC lock(rt);

    fromArenaLists->purge();

    for (auto thingKind : AllAllocKinds()) {
        // When we enter a parallel section, we join the background
        // thread, and we do not run GC while in the parallel section,
        // so no finalizer should be active!
        normalizeBackgroundFinalizeState(thingKind);
        fromArenaLists->normalizeBackgroundFinalizeState(thingKind);

        ArenaList* fromList = &fromArenaLists->arenaLists(thingKind);
        ArenaList* toList = &arenaLists(thingKind);
        fromList->check();
        toList->check();
        Arena* next;
        for (Arena* fromArena = fromList->head(); fromArena; fromArena = next) {
            // Copy fromArena->next before releasing/reinserting.
            next = fromArena->next;

            MOZ_ASSERT(!fromArena->isEmpty());
            toList->insertAtCursor(fromArena);
        }
        fromList->clear();
        toList->check();
    }
}

bool
ArenaLists::containsArena(JSRuntime* rt, Arena* needle)
{
    AutoLockGC lock(rt);
    ArenaList& list = arenaLists(needle->getAllocKind());
    for (Arena* arena = list.head(); arena; arena = arena->next) {
        if (arena == needle)
            return true;
    }
    return false;
}


AutoSuppressGC::AutoSuppressGC(JSContext* cx)
  : suppressGC_(cx->suppressGC.ref())
{
    suppressGC_++;
}

bool
js::UninlinedIsInsideNursery(const gc::Cell* cell)
{
    return IsInsideNursery(cell);
}

#ifdef DEBUG
AutoDisableProxyCheck::AutoDisableProxyCheck()
{
    TlsContext.get()->disableStrictProxyChecking();
}

AutoDisableProxyCheck::~AutoDisableProxyCheck()
{
    TlsContext.get()->enableStrictProxyChecking();
}

JS_FRIEND_API(void)
JS::AssertGCThingMustBeTenured(JSObject* obj)
{
    MOZ_ASSERT(obj->isTenured() &&
               (!IsNurseryAllocable(obj->asTenured().getAllocKind()) ||
                obj->getClass()->hasFinalize()));
}

JS_FRIEND_API(void)
JS::AssertGCThingIsNotAnObjectSubclass(Cell* cell)
{
    MOZ_ASSERT(cell);
    MOZ_ASSERT(cell->getTraceKind() != JS::TraceKind::Object);
}

JS_FRIEND_API(void)
js::gc::AssertGCThingHasType(js::gc::Cell* cell, JS::TraceKind kind)
{
    if (!cell)
        MOZ_ASSERT(kind == JS::TraceKind::Null);
    else if (IsInsideNursery(cell))
        MOZ_ASSERT(kind == JS::TraceKind::Object);
    else
        MOZ_ASSERT(MapAllocToTraceKind(cell->asTenured().getAllocKind()) == kind);
}
#endif

JS::AutoAssertNoGC::AutoAssertNoGC(JSContext* maybecx)
  : cx_(maybecx ? maybecx : TlsContext.get())
{
    cx_->inUnsafeRegion++;
}

JS::AutoAssertNoGC::~AutoAssertNoGC()
{
    MOZ_ASSERT(cx_->inUnsafeRegion > 0);
    cx_->inUnsafeRegion--;
}

JS::AutoAssertOnBarrier::AutoAssertOnBarrier(JSContext* cx)
  : context(cx),
    prev(cx->allowGCBarriers)
{
    context->allowGCBarriers = false;
}

JS::AutoAssertOnBarrier::~AutoAssertOnBarrier()
{
    MOZ_ASSERT(!context->allowGCBarriers);
    context->allowGCBarriers = prev;
}

JS_FRIEND_API(bool)
js::gc::BarriersAreAllowedOnCurrentThread()
{
    return TlsContext.get()->allowGCBarriers;
}

#ifdef DEBUG
JS::AutoAssertNoAlloc::AutoAssertNoAlloc(JSContext* cx)
  : gc(nullptr)
{
    disallowAlloc(cx->runtime());
}

void JS::AutoAssertNoAlloc::disallowAlloc(JSRuntime* rt)
{
    MOZ_ASSERT(!gc);
    gc = &rt->gc;
    TlsContext.get()->disallowAlloc();
}

JS::AutoAssertNoAlloc::~AutoAssertNoAlloc()
{
    if (gc)
        TlsContext.get()->allowAlloc();
}

AutoAssertNoNurseryAlloc::AutoAssertNoNurseryAlloc()
{
    TlsContext.get()->disallowNurseryAlloc();
}

AutoAssertNoNurseryAlloc::~AutoAssertNoNurseryAlloc()
{
    TlsContext.get()->allowNurseryAlloc();
}

JS::AutoEnterCycleCollection::AutoEnterCycleCollection(JSContext* cx)
  : runtime(cx->runtime())
{
    MOZ_ASSERT(!JS::CurrentThreadIsHeapBusy());
    TlsContext.get()->heapState = HeapState::CycleCollecting;
}

JS::AutoEnterCycleCollection::~AutoEnterCycleCollection()
{
    MOZ_ASSERT(JS::CurrentThreadIsHeapCycleCollecting());
    TlsContext.get()->heapState = HeapState::Idle;
}
#endif

JS::AutoAssertGCCallback::AutoAssertGCCallback(JSObject* obj)
  : AutoSuppressGCAnalysis()
{
    MOZ_ASSERT(JS::CurrentThreadIsHeapCollecting());
}

JS_FRIEND_API(const char*)
JS::GCTraceKindToAscii(JS::TraceKind kind)
{
    switch(kind) {
#define MAP_NAME(name, _0, _1) case JS::TraceKind::name: return #name;
JS_FOR_EACH_TRACEKIND(MAP_NAME);
#undef MAP_NAME
      default: return "Invalid";
    }
}

JS::GCCellPtr::GCCellPtr(const Value& v)
  : ptr(0)
{
    if (v.isString())
        ptr = checkedCast(v.toString(), JS::TraceKind::String);
    else if (v.isObject())
        ptr = checkedCast(&v.toObject(), JS::TraceKind::Object);
    else if (v.isSymbol())
        ptr = checkedCast(v.toSymbol(), JS::TraceKind::Symbol);
    else if (v.isPrivateGCThing())
        ptr = checkedCast(v.toGCThing(), v.toGCThing()->getTraceKind());
    else
        ptr = checkedCast(nullptr, JS::TraceKind::Null);
}

JS::TraceKind
JS::GCCellPtr::outOfLineKind() const
{
    MOZ_ASSERT((ptr & OutOfLineTraceKindMask) == OutOfLineTraceKindMask);
    MOZ_ASSERT(asCell()->isTenured());
    return MapAllocToTraceKind(asCell()->asTenured().getAllocKind());
}

bool
JS::GCCellPtr::mayBeOwnedByOtherRuntime() const
{
    return (is<JSString>() && as<JSString>().isPermanentAtom()) ||
           (is<Symbol>() && as<Symbol>().isWellKnownSymbol());
}

#ifdef JSGC_HASH_TABLE_CHECKS
void
js::gc::CheckHashTablesAfterMovingGC(JSRuntime* rt)
{
    /*
     * Check that internal hash tables no longer have any pointers to things
     * that have been moved.
     */
    rt->geckoProfiler().checkStringsMapAfterMovingGC();
    for (ZonesIter zone(rt, SkipAtoms); !zone.done(); zone.next()) {
        zone->checkUniqueIdTableAfterMovingGC();
        zone->checkInitialShapesTableAfterMovingGC();
        zone->checkBaseShapeTableAfterMovingGC();

        JS::AutoCheckCannotGC nogc;
        for (auto baseShape = zone->cellIter<BaseShape>(); !baseShape.done(); baseShape.next()) {
            if (ShapeTable* table = baseShape->maybeTable(nogc))
                table->checkAfterMovingGC();
        }
    }
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
        c->objectGroups.checkTablesAfterMovingGC();
        c->dtoaCache.checkCacheAfterMovingGC();
        c->checkWrapperMapAfterMovingGC();
        c->checkScriptMapsAfterMovingGC();
        if (c->debugEnvs)
            c->debugEnvs->checkHashTablesAfterMovingGC(rt);
    }
}
#endif

JS_PUBLIC_API(void)
JS::PrepareZoneForGC(Zone* zone)
{
    zone->scheduleGC();
}

JS_PUBLIC_API(void)
JS::PrepareForFullGC(JSContext* cx)
{
    for (ZonesIter zone(cx->runtime(), WithAtoms); !zone.done(); zone.next())
        zone->scheduleGC();
}

JS_PUBLIC_API(void)
JS::PrepareForIncrementalGC(JSContext* cx)
{
    if (!JS::IsIncrementalGCInProgress(cx))
        return;

    for (ZonesIter zone(cx->runtime(), WithAtoms); !zone.done(); zone.next()) {
        if (zone->wasGCStarted())
            PrepareZoneForGC(zone);
    }
}

JS_PUBLIC_API(bool)
JS::IsGCScheduled(JSContext* cx)
{
    for (ZonesIter zone(cx->runtime(), WithAtoms); !zone.done(); zone.next()) {
        if (zone->isGCScheduled())
            return true;
    }

    return false;
}

JS_PUBLIC_API(void)
JS::SkipZoneForGC(Zone* zone)
{
    zone->unscheduleGC();
}

JS_PUBLIC_API(void)
JS::GCForReason(JSContext* cx, JSGCInvocationKind gckind, gcreason::Reason reason)
{
    MOZ_ASSERT(gckind == GC_NORMAL || gckind == GC_SHRINK);
    cx->runtime()->gc.gc(gckind, reason);
}

JS_PUBLIC_API(void)
JS::StartIncrementalGC(JSContext* cx, JSGCInvocationKind gckind, gcreason::Reason reason, int64_t millis)
{
    MOZ_ASSERT(gckind == GC_NORMAL || gckind == GC_SHRINK);
    cx->runtime()->gc.startGC(gckind, reason, millis);
}

JS_PUBLIC_API(void)
JS::IncrementalGCSlice(JSContext* cx, gcreason::Reason reason, int64_t millis)
{
    cx->runtime()->gc.gcSlice(reason, millis);
}

JS_PUBLIC_API(void)
JS::FinishIncrementalGC(JSContext* cx, gcreason::Reason reason)
{
    cx->runtime()->gc.finishGC(reason);
}

JS_PUBLIC_API(void)
JS::AbortIncrementalGC(JSContext* cx)
{
    if (IsIncrementalGCInProgress(cx))
        cx->runtime()->gc.abortGC();
}

char16_t*
JS::GCDescription::formatSliceMessage(JSContext* cx) const
{
    UniqueChars cstr = cx->runtime()->gc.stats().formatCompactSliceMessage();

    size_t nchars = strlen(cstr.get());
    UniqueTwoByteChars out(js_pod_malloc<char16_t>(nchars + 1));
    if (!out)
        return nullptr;
    out.get()[nchars] = 0;

    CopyAndInflateChars(out.get(), cstr.get(), nchars);
    return out.release();
}

char16_t*
JS::GCDescription::formatSummaryMessage(JSContext* cx) const
{
    UniqueChars cstr = cx->runtime()->gc.stats().formatCompactSummaryMessage();

    size_t nchars = strlen(cstr.get());
    UniqueTwoByteChars out(js_pod_malloc<char16_t>(nchars + 1));
    if (!out)
        return nullptr;
    out.get()[nchars] = 0;

    CopyAndInflateChars(out.get(), cstr.get(), nchars);
    return out.release();
}

JS::dbg::GarbageCollectionEvent::Ptr
JS::GCDescription::toGCEvent(JSContext* cx) const
{
    return JS::dbg::GarbageCollectionEvent::Create(cx->runtime(), cx->runtime()->gc.stats(),
                                                   cx->runtime()->gc.majorGCCount());
}

char16_t*
JS::GCDescription::formatJSON(JSContext* cx, uint64_t timestamp) const
{
    UniqueChars cstr = cx->runtime()->gc.stats().formatJsonMessage(timestamp);

    size_t nchars = strlen(cstr.get());
    UniqueTwoByteChars out(js_pod_malloc<char16_t>(nchars + 1));
    if (!out)
        return nullptr;
    out.get()[nchars] = 0;

    CopyAndInflateChars(out.get(), cstr.get(), nchars);
    return out.release();
}

JS_PUBLIC_API(JS::GCSliceCallback)
JS::SetGCSliceCallback(JSContext* cx, GCSliceCallback callback)
{
    return cx->runtime()->gc.setSliceCallback(callback);
}

JS_PUBLIC_API(JS::DoCycleCollectionCallback)
JS::SetDoCycleCollectionCallback(JSContext* cx, JS::DoCycleCollectionCallback callback)
{
    return cx->runtime()->gc.setDoCycleCollectionCallback(callback);
}

JS_PUBLIC_API(JS::GCNurseryCollectionCallback)
JS::SetGCNurseryCollectionCallback(JSContext* cx, GCNurseryCollectionCallback callback)
{
    return cx->runtime()->gc.setNurseryCollectionCallback(callback);
}

JS_PUBLIC_API(void)
JS::DisableIncrementalGC(JSContext* cx)
{
    cx->runtime()->gc.disallowIncrementalGC();
}

JS_PUBLIC_API(bool)
JS::IsIncrementalGCEnabled(JSContext* cx)
{
    return cx->runtime()->gc.isIncrementalGCEnabled();
}

JS_PUBLIC_API(bool)
JS::IsIncrementalGCInProgress(JSContext* cx)
{
    return cx->runtime()->gc.isIncrementalGCInProgress() && !cx->runtime()->gc.isVerifyPreBarriersEnabled();
}

JS_PUBLIC_API(bool)
JS::IsIncrementalBarrierNeeded(JSContext* cx)
{
    return cx->runtime()->gc.state() == gc::State::Mark && !JS::CurrentThreadIsHeapBusy();
}

JS_PUBLIC_API(void)
JS::IncrementalPreWriteBarrier(JSObject* obj)
{
    if (!obj)
        return;

    MOZ_ASSERT(!JS::CurrentThreadIsHeapMajorCollecting());
    JSObject::writeBarrierPre(obj);
}

struct IncrementalReadBarrierFunctor {
    template <typename T> void operator()(T* t) { T::readBarrier(t); }
};

JS_PUBLIC_API(void)
JS::IncrementalReadBarrier(GCCellPtr thing)
{
    if (!thing)
        return;

    MOZ_ASSERT(!JS::CurrentThreadIsHeapMajorCollecting());
    DispatchTyped(IncrementalReadBarrierFunctor(), thing);
}

JS_PUBLIC_API(bool)
JS::WasIncrementalGC(JSContext* cx)
{
    return cx->runtime()->gc.isIncrementalGc();
}

uint64_t
js::gc::NextCellUniqueId(JSRuntime* rt)
{
    return rt->gc.nextCellUniqueId();
}

namespace js {
namespace gc {
namespace MemInfo {

static bool
GCBytesGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setNumber(double(cx->runtime()->gc.usage.gcBytes()));
    return true;
}

static bool
GCMaxBytesGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setNumber(double(cx->runtime()->gc.tunables.gcMaxBytes()));
    return true;
}

static bool
MallocBytesGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setNumber(double(cx->runtime()->gc.getMallocBytes()));
    return true;
}

static bool
MaxMallocGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setNumber(double(cx->runtime()->gc.maxMallocBytesAllocated()));
    return true;
}

static bool
GCHighFreqGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(cx->runtime()->gc.schedulingState.inHighFrequencyGCMode());
    return true;
}

static bool
GCNumberGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setNumber(double(cx->runtime()->gc.gcNumber()));
    return true;
}

static bool
MajorGCCountGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setNumber(double(cx->runtime()->gc.majorGCCount()));
    return true;
}

static bool
MinorGCCountGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setNumber(double(cx->runtime()->gc.minorGCCount()));
    return true;
}

static bool
ZoneGCBytesGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setNumber(double(cx->zone()->usage.gcBytes()));
    return true;
}

static bool
ZoneGCTriggerBytesGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setNumber(double(cx->zone()->threshold.gcTriggerBytes()));
    return true;
}

static bool
ZoneGCAllocTriggerGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    bool highFrequency = cx->runtime()->gc.schedulingState.inHighFrequencyGCMode();
    args.rval().setNumber(double(cx->zone()->threshold.allocTrigger(highFrequency)));
    return true;
}

static bool
ZoneMallocBytesGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setNumber(double(cx->zone()->GCMallocBytes()));
    return true;
}

static bool
ZoneMaxMallocGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setNumber(double(cx->zone()->GCMaxMallocBytes()));
    return true;
}

static bool
ZoneGCDelayBytesGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setNumber(double(cx->zone()->gcDelayBytes));
    return true;
}

static bool
ZoneGCHeapGrowthFactorGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    AutoLockGC lock(cx->runtime());
    args.rval().setNumber(cx->zone()->threshold.gcHeapGrowthFactor());
    return true;
}

static bool
ZoneGCNumberGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setNumber(double(cx->zone()->gcNumber()));
    return true;
}

#ifdef JS_MORE_DETERMINISTIC
static bool
DummyGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setUndefined();
    return true;
}
#endif

} /* namespace MemInfo */

JSObject*
NewMemoryInfoObject(JSContext* cx)
{
    RootedObject obj(cx, JS_NewObject(cx, nullptr));
    if (!obj)
        return nullptr;

    using namespace MemInfo;
    struct NamedGetter {
        const char* name;
        JSNative getter;
    } getters[] = {
        { "gcBytes", GCBytesGetter },
        { "gcMaxBytes", GCMaxBytesGetter },
        { "mallocBytesRemaining", MallocBytesGetter },
        { "maxMalloc", MaxMallocGetter },
        { "gcIsHighFrequencyMode", GCHighFreqGetter },
        { "gcNumber", GCNumberGetter },
        { "majorGCCount", MajorGCCountGetter },
        { "minorGCCount", MinorGCCountGetter }
    };

    for (auto pair : getters) {
#ifdef JS_MORE_DETERMINISTIC
        JSNative getter = DummyGetter;
#else
        JSNative getter = pair.getter;
#endif
        if (!JS_DefineProperty(cx, obj, pair.name, UndefinedHandleValue,
                               JSPROP_ENUMERATE | JSPROP_SHARED,
                               getter, nullptr))
        {
            return nullptr;
        }
    }

    RootedObject zoneObj(cx, JS_NewObject(cx, nullptr));
    if (!zoneObj)
        return nullptr;

    if (!JS_DefineProperty(cx, obj, "zone", zoneObj, JSPROP_ENUMERATE))
        return nullptr;

    struct NamedZoneGetter {
        const char* name;
        JSNative getter;
    } zoneGetters[] = {
        { "gcBytes", ZoneGCBytesGetter },
        { "gcTriggerBytes", ZoneGCTriggerBytesGetter },
        { "gcAllocTrigger", ZoneGCAllocTriggerGetter },
        { "mallocBytesRemaining", ZoneMallocBytesGetter },
        { "maxMalloc", ZoneMaxMallocGetter },
        { "delayBytes", ZoneGCDelayBytesGetter },
        { "heapGrowthFactor", ZoneGCHeapGrowthFactorGetter },
        { "gcNumber", ZoneGCNumberGetter }
    };

    for (auto pair : zoneGetters) {
 #ifdef JS_MORE_DETERMINISTIC
        JSNative getter = DummyGetter;
#else
        JSNative getter = pair.getter;
#endif
        if (!JS_DefineProperty(cx, zoneObj, pair.name, UndefinedHandleValue,
                               JSPROP_ENUMERATE | JSPROP_SHARED,
                               getter, nullptr))
        {
            return nullptr;
        }
    }

    return obj;
}

const char*
StateName(State state)
{
    switch(state) {
#define MAKE_CASE(name) case State::name: return #name;
      GCSTATES(MAKE_CASE)
#undef MAKE_CASE
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("invalide gc::State enum value");
}

void
AutoAssertHeapBusy::checkCondition(JSRuntime *rt)
{
    this->rt = rt;
    MOZ_ASSERT(JS::CurrentThreadIsHeapBusy());
}

void
AutoAssertEmptyNursery::checkCondition(JSContext* cx) {
    if (!noAlloc)
        noAlloc.emplace();
    this->cx = cx;
    MOZ_ASSERT(AllNurseriesAreEmpty(cx->runtime()));
}

AutoEmptyNursery::AutoEmptyNursery(JSContext* cx)
  : AutoAssertEmptyNursery()
{
    MOZ_ASSERT(!cx->suppressGC);
    cx->runtime()->gc.stats().suspendPhases();
    EvictAllNurseries(cx->runtime(), JS::gcreason::EVICT_NURSERY);
    cx->runtime()->gc.stats().resumePhases();
    checkCondition(cx);
}

} /* namespace gc */
} /* namespace js */

#ifdef DEBUG
void
js::gc::Cell::dump(FILE* fp) const
{
    switch (getTraceKind()) {
      case JS::TraceKind::Object:
        reinterpret_cast<const JSObject*>(this)->dump(fp);
        break;

      case JS::TraceKind::String:
          js::DumpString(reinterpret_cast<JSString*>(const_cast<Cell*>(this)), fp);
        break;

      case JS::TraceKind::Shape:
        reinterpret_cast<const Shape*>(this)->dump(fp);
        break;

      default:
        fprintf(fp, "%s(%p)\n", JS::GCTraceKindToAscii(getTraceKind()), (void*) this);
    }
}

// For use in a debugger.
void
js::gc::Cell::dump() const
{
    dump(stderr);
}
#endif

static inline bool
CanCheckGrayBits(const Cell* cell)
{
    MOZ_ASSERT(cell);
    if (!cell->isTenured())
        return false;

    auto tc = &cell->asTenured();
    auto rt = tc->runtimeFromAnyThread();
    return CurrentThreadCanAccessRuntime(rt) && rt->gc.areGrayBitsValid();
}

JS_PUBLIC_API(bool)
js::gc::detail::CellIsMarkedGrayIfKnown(const Cell* cell)
{
    // We ignore the gray marking state of cells and return false in the
    // following cases:
    //
    // 1) When OOM has caused us to clear the gcGrayBitsValid_ flag.
    //
    // 2) When we are in an incremental GC and examine a cell that is in a zone
    // that is not being collected. Gray targets of CCWs that are marked black
    // by a barrier will eventually be marked black in the next GC slice.
    //
    // 3) When we are not on the runtime's active thread. Helper threads might
    // call this while parsing, and they are not allowed to inspect the
    // runtime's incremental state. The objects being operated on are not able
    // to be collected and will not be marked any color.

    if (!CanCheckGrayBits(cell))
        return false;

    auto tc = &cell->asTenured();
    auto rt = tc->runtimeFromAnyThread();
    if (rt->gc.isIncrementalGCInProgress() && !tc->zone()->wasGCStarted())
        return false;

    return detail::CellIsMarkedGray(tc);
}

#ifdef DEBUG
JS_PUBLIC_API(bool)
js::gc::detail::CellIsNotGray(const Cell* cell)
{
    // Check that a cell is not marked gray.
    //
    // Since this is a debug-only check, take account of the eventual mark state
    // of cells that will be marked black by the next GC slice in an incremental
    // GC. For performance reasons we don't do this in CellIsMarkedGrayIfKnown.

    // TODO: I'd like to AssertHeapIsIdle() here, but this ends up getting
    // called while iterating the heap for memory reporting.
    MOZ_ASSERT(!JS::CurrentThreadIsHeapCollecting());
    MOZ_ASSERT(!JS::CurrentThreadIsHeapCycleCollecting());

    if (!CanCheckGrayBits(cell))
        return true;

    auto tc = &cell->asTenured();
    if (!detail::CellIsMarkedGray(tc))
        return true;

    auto rt = tc->runtimeFromAnyThread();
    Zone* sourceZone = nullptr;
    if (rt->gc.isIncrementalGCInProgress() &&
        !tc->zone()->wasGCStarted() &&
        (sourceZone = rt->gc.marker.stackContainsCrossZonePointerTo(tc)) &&
        sourceZone->wasGCStarted())
    {
        return true;
    }

    return false;
}
#endif
