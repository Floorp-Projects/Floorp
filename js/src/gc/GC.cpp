/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * [SMDOC] Garbage Collector
 *
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
 *                        sweep groups.
 *
 *          ... JS code runs ...
 *
 * Slice n+2: Sweep:      Mark objects in unswept zones that were newly
 *                        identified as alive. Then sweep more zones.
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
 * already, we continue until we have swept the current sweep group. Following a
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

#include "gc/GC-inl.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "mozilla/Range.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/Unused.h"

#include <ctype.h>
#include <initializer_list>
#include <string.h>
#ifndef XP_WIN
# include <sys/mman.h>
# include <unistd.h>
#endif

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jstypes.h"
#include "jsutil.h"

#include "gc/FindSCCs.h"
#include "gc/FreeOp.h"
#include "gc/GCInternals.h"
#include "gc/GCTrace.h"
#include "gc/Memory.h"
#include "gc/Policy.h"
#include "gc/WeakMap.h"
#include "jit/BaselineJIT.h"
#include "jit/IonCode.h"
#include "jit/JitcodeMap.h"
#include "jit/JitRealm.h"
#include "js/SliceBudget.h"
#include "proxy/DeadObjectProxy.h"
#include "util/Windows.h"
#ifdef ENABLE_BIGINT
#include "vm/BigIntType.h"
#endif
#include "vm/Debugger.h"
#include "vm/GeckoProfiler.h"
#include "vm/JSAtom.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/JSScript.h"
#include "vm/Printer.h"
#include "vm/ProxyObject.h"
#include "vm/Realm.h"
#include "vm/Shape.h"
#include "vm/StringType.h"
#include "vm/SymbolType.h"
#include "vm/Time.h"
#include "vm/TraceLogging.h"
#include "vm/WrapperObject.h"

#include "gc/Heap-inl.h"
#include "gc/Marking-inl.h"
#include "gc/Nursery-inl.h"
#include "gc/PrivateIterators-inl.h"
#include "vm/GeckoProfiler-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/Stack-inl.h"
#include "vm/StringType-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::ArrayLength;
using mozilla::Maybe;
using mozilla::Swap;
using mozilla::TimeStamp;

using JS::AutoGCRooter;

/*
 * Default settings for tuning the GC.  Some of these can be set at runtime,
 * This list is not complete, some tuning parameters are not listed here.
 *
 * If you change the values here, please also consider changing them in
 * modules/libpref/init/all.js where they are duplicated for the Firefox
 * preferences.
 */
namespace js {
namespace gc {
namespace TuningDefaults {

    /* JSGC_ALLOCATION_THRESHOLD */
    static const size_t GCZoneAllocThresholdBase = 30 * 1024 * 1024;

    /* JSGC_MAX_MALLOC_BYTES */
    static const size_t MaxMallocBytes = 128 * 1024 * 1024;

    /* JSGC_ALLOCATION_THRESHOLD_FACTOR */
    static const double AllocThresholdFactor = 0.9;

    /* JSGC_ALLOCATION_THRESHOLD_FACTOR_AVOID_INTERRUPT */
    static const double AllocThresholdFactorAvoidInterrupt = 0.9;

    /* no parameter */
    static const double MallocThresholdGrowFactor = 1.5;

    /* no parameter */
    static const double MallocThresholdShrinkFactor = 0.9;

    /* no parameter */
    static const size_t MallocThresholdLimit = 1024 * 1024 * 1024;

    /* no parameter */
    static const size_t ZoneAllocDelayBytes = 1024 * 1024;

    /* JSGC_DYNAMIC_HEAP_GROWTH */
    static const bool DynamicHeapGrowthEnabled = false;

    /* JSGC_HIGH_FREQUENCY_TIME_LIMIT */
    static const auto HighFrequencyThreshold = 1; // in seconds

    /* JSGC_HIGH_FREQUENCY_LOW_LIMIT */
    static const uint64_t HighFrequencyLowLimitBytes = 100 * 1024 * 1024;

    /* JSGC_HIGH_FREQUENCY_HIGH_LIMIT */
    static const uint64_t HighFrequencyHighLimitBytes = 500 * 1024 * 1024;

    /* JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX */
    static const double HighFrequencyHeapGrowthMax = 3.0;

    /* JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN */
    static const double HighFrequencyHeapGrowthMin = 1.5;

    /* JSGC_LOW_FREQUENCY_HEAP_GROWTH */
    static const double LowFrequencyHeapGrowth = 1.5;

    /* JSGC_DYNAMIC_MARK_SLICE */
    static const bool DynamicMarkSliceEnabled = false;

    /* JSGC_MIN_EMPTY_CHUNK_COUNT */
    static const uint32_t MinEmptyChunkCount = 1;

    /* JSGC_MAX_EMPTY_CHUNK_COUNT */
    static const uint32_t MaxEmptyChunkCount = 30;

    /* JSGC_SLICE_TIME_BUDGET */
    static const int64_t DefaultTimeBudget = SliceBudget::UnlimitedTimeBudget;

    /* JSGC_MODE */
    static const JSGCMode Mode = JSGC_MODE_INCREMENTAL;

    /* JSGC_COMPACTING_ENABLED */
    static const bool CompactingEnabled = true;

    /* JSGC_NURSERY_FREE_THRESHOLD_FOR_IDLE_COLLECTION */
    static const uint32_t NurseryFreeThresholdForIdleCollection =
        Nursery::NurseryChunkUsableSize / 4;

}}} // namespace js::gc::TuningDefaults

/*
 * We start to incremental collection for a zone when a proportion of its
 * threshold is reached. This is configured by the
 * JSGC_ALLOCATION_THRESHOLD_FACTOR and
 * JSGC_ALLOCATION_THRESHOLD_FACTOR_AVOID_INTERRUPT parameters.
 */
static const double MinAllocationThresholdFactor = 0.9;

/*
 * We may start to collect a zone before its trigger threshold is reached if
 * GCRuntime::maybeGC() is called for that zone or we start collecting other
 * zones. These eager threshold factors are not configurable.
 */
static const double HighFrequencyEagerAllocTriggerFactor = 0.85;
static const double LowFrequencyEagerAllocTriggerFactor = 0.9;

/*
 * Don't allow heap growth factors to be set so low that collections could
 * reduce the trigger threshold.
 */
static const double MinHighFrequencyHeapGrowthFactor =
    1.0 / Min(HighFrequencyEagerAllocTriggerFactor, MinAllocationThresholdFactor);
static const double MinLowFrequencyHeapGrowthFactor =
    1.0 / Min(LowFrequencyEagerAllocTriggerFactor, MinAllocationThresholdFactor);

/* Increase the IGC marking slice time if we are in highFrequencyGC mode. */
static const int IGC_MARK_SLICE_MULTIPLIER = 2;

const AllocKind gc::slotsToThingKind[] = {
    /*  0 */ AllocKind::OBJECT0,  AllocKind::OBJECT2,  AllocKind::OBJECT2,  AllocKind::OBJECT4,
    /*  4 */ AllocKind::OBJECT4,  AllocKind::OBJECT8,  AllocKind::OBJECT8,  AllocKind::OBJECT8,
    /*  8 */ AllocKind::OBJECT8,  AllocKind::OBJECT12, AllocKind::OBJECT12, AllocKind::OBJECT12,
    /* 12 */ AllocKind::OBJECT12, AllocKind::OBJECT16, AllocKind::OBJECT16, AllocKind::OBJECT16,
    /* 16 */ AllocKind::OBJECT16
};

static_assert(mozilla::ArrayLength(slotsToThingKind) == SLOTS_TO_THING_KIND_LIMIT,
              "We have defined a slot count for each kind.");

#define CHECK_THING_SIZE(allocKind, traceKind, type, sizedType, bgFinal, nursery) \
    static_assert(sizeof(sizedType) >= SortedArenaList::MinThingSize, \
                  #sizedType " is smaller than SortedArenaList::MinThingSize!"); \
    static_assert(sizeof(sizedType) >= sizeof(FreeSpan), \
                  #sizedType " is smaller than FreeSpan"); \
    static_assert(sizeof(sizedType) % CellAlignBytes == 0, \
                  "Size of " #sizedType " is not a multiple of CellAlignBytes"); \
    static_assert(sizeof(sizedType) >= MinCellSize, \
                  "Size of " #sizedType " is smaller than the minimum size");
FOR_EACH_ALLOCKIND(CHECK_THING_SIZE);
#undef CHECK_THING_SIZE

const uint32_t Arena::ThingSizes[] = {
#define EXPAND_THING_SIZE(allocKind, traceKind, type, sizedType, bgFinal, nursery) \
    sizeof(sizedType),
FOR_EACH_ALLOCKIND(EXPAND_THING_SIZE)
#undef EXPAND_THING_SIZE
};

FreeSpan FreeLists::emptySentinel;

#undef CHECK_THING_SIZE_INNER
#undef CHECK_THING_SIZE

#define OFFSET(type) uint32_t(ArenaHeaderSize + (ArenaSize - ArenaHeaderSize) % sizeof(type))

const uint32_t Arena::FirstThingOffsets[] = {
#define EXPAND_FIRST_THING_OFFSET(allocKind, traceKind, type, sizedType, bgFinal, nursery) \
    OFFSET(sizedType),
FOR_EACH_ALLOCKIND(EXPAND_FIRST_THING_OFFSET)
#undef EXPAND_FIRST_THING_OFFSET
};

#undef OFFSET

#define COUNT(type) uint32_t((ArenaSize - ArenaHeaderSize) / sizeof(type))

const uint32_t Arena::ThingsPerArena[] = {
#define EXPAND_THINGS_PER_ARENA(allocKind, traceKind, type, sizedType, bgFinal, nursery) \
    COUNT(sizedType),
FOR_EACH_ALLOCKIND(EXPAND_THINGS_PER_ARENA)
#undef EXPAND_THINGS_PER_ARENA
};

#undef COUNT

struct js::gc::FinalizePhase
{
    gcstats::PhaseKind statsPhase;
    AllocKinds kinds;
};

/*
 * Finalization order for objects swept incrementally on the main thread.
 */
static const FinalizePhase ForegroundObjectFinalizePhase = {
    gcstats::PhaseKind::SWEEP_OBJECT, {
        AllocKind::OBJECT0,
        AllocKind::OBJECT2,
        AllocKind::OBJECT4,
        AllocKind::OBJECT8,
        AllocKind::OBJECT12,
        AllocKind::OBJECT16
    }
};

/*
 * Finalization order for GC things swept incrementally on the main thread.
 */
static const FinalizePhase ForegroundNonObjectFinalizePhase = {
    gcstats::PhaseKind::SWEEP_SCRIPT, {
        AllocKind::SCRIPT,
        AllocKind::JITCODE
    }
};

/*
 * Finalization order for GC things swept on the background thread.
 */
static const FinalizePhase BackgroundFinalizePhases[] = {
    {
        gcstats::PhaseKind::SWEEP_SCRIPT, {
            AllocKind::LAZY_SCRIPT
        }
    },
    {
        gcstats::PhaseKind::SWEEP_OBJECT, {
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
        gcstats::PhaseKind::SWEEP_SCOPE, {
            AllocKind::SCOPE,
        }
    },
    {
        gcstats::PhaseKind::SWEEP_REGEXP_SHARED, {
            AllocKind::REGEXP_SHARED,
        }
    },
    {
        gcstats::PhaseKind::SWEEP_STRING, {
            AllocKind::FAT_INLINE_STRING,
            AllocKind::STRING,
            AllocKind::EXTERNAL_STRING,
            AllocKind::FAT_INLINE_ATOM,
            AllocKind::ATOM,
            AllocKind::SYMBOL,
#ifdef ENABLE_BIGINT
            AllocKind::BIGINT
#endif
        }
    },
    {
        gcstats::PhaseKind::SWEEP_SHAPE, {
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

void
Arena::unmarkPreMarkedFreeCells()
{
    for (ArenaFreeCellIter iter(this); !iter.done(); iter.next()) {
        TenuredCell* cell = iter.getCell();
        MOZ_ASSERT(cell->isMarkedBlack());
        cell->unmark();
    }
}

#ifdef DEBUG
void
Arena::checkNoMarkedFreeCells()
{
    for (ArenaFreeCellIter iter(this); !iter.done(); iter.next())
        MOZ_ASSERT(!iter.getCell()->isMarkedAny());
}
#endif

/* static */ void
Arena::staticAsserts()
{
    static_assert(size_t(AllocKind::LIMIT) <= 255,
                  "We must be able to fit the allockind into uint8_t.");
    static_assert(mozilla::ArrayLength(ThingSizes) == size_t(AllocKind::LIMIT),
                  "We haven't defined all thing sizes.");
    static_assert(mozilla::ArrayLength(FirstThingOffsets) == size_t(AllocKind::LIMIT),
                  "We haven't defined all offsets.");
    static_assert(mozilla::ArrayLength(ThingsPerArena) == size_t(AllocKind::LIMIT),
                  "We haven't defined all counts.");
}

template<typename T>
inline size_t
Arena::finalize(FreeOp* fop, AllocKind thingKind, size_t thingSize)
{
    /* Enforce requirements on size of T. */
    MOZ_ASSERT(thingSize % CellAlignBytes == 0);
    MOZ_ASSERT(thingSize >= MinCellSize);
    MOZ_ASSERT(thingSize <= 255);

    MOZ_ASSERT(allocated());
    MOZ_ASSERT(thingKind == getAllocKind());
    MOZ_ASSERT(thingSize == getThingSize());
    MOZ_ASSERT(!hasDelayedMarking);
    MOZ_ASSERT(!markOverflow);

    uint_fast16_t firstThing = firstThingOffset(thingKind);
    uint_fast16_t firstThingOrSuccessorOfLastMarkedThing = firstThing;
    uint_fast16_t lastThing = ArenaSize - thingSize;

    FreeSpan newListHead;
    FreeSpan* newListTail = &newListHead;
    size_t nmarked = 0;

    for (ArenaCellIterUnderFinalize i(this); !i.done(); i.next()) {
        T* t = i.get<T>();
        if (t->asTenured().isMarkedAny()) {
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
            JS_POISON(t, JS_SWEPT_TENURED_PATTERN, thingSize, MemCheckKind::MakeUndefined);
            gcTracer.traceTenuredFinalize(t);
        }
    }

    if (nmarked == 0) {
        // Do nothing. The caller will update the arena appropriately.
        MOZ_ASSERT(newListTail == &newListHead);
        JS_EXTRA_POISON(data, JS_SWEPT_TENURED_PATTERN, sizeof(data), MemCheckKind::MakeUndefined);
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
    if (fop->onMainThread())
        maybeLock.emplace(fop->runtime());

    // During background sweeping free arenas are released later on in
    // sweepBackgroundThings().
    MOZ_ASSERT_IF(!fop->onMainThread(), keepArenas == ArenaLists::KEEP_ARENAS);

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
#define EXPAND_CASE(allocKind, traceKind, type, sizedType, bgFinal, nursery) \
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
FreeChunkPool(ChunkPool& pool)
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
GCRuntime::freeEmptyChunks(const AutoLockGC& lock)
{
    FreeChunkPool(emptyChunks(lock));
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
GCRuntime::updateOnArenaFree()
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
    rt->gc.updateOnArenaFree();
}

void
Chunk::addArenaToDecommittedList(const Arena* arena)
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

    arena->release(lock);
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
        addArenaToDecommittedList(arena);
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
        decommitAllArenas();
        MOZ_ASSERT(info.numArenasFreeCommitted == 0);
        rt->gc.recycleChunk(this, lock);
    }
}

void
GCRuntime::releaseArena(Arena* arena, const AutoLockGC& lock)
{
    arena->zone->usage.removeGCArena();
    arena->chunk()->releaseArena(rt, arena, lock);
}

GCRuntime::GCRuntime(JSRuntime* rt) :
    rt(rt),
    systemZone(nullptr),
    atomsZone(nullptr),
    stats_(rt),
    marker(rt),
    usage(nullptr),
    nextCellUniqueId_(LargestTaggedNullCellPointer + 1), // Ensure disjoint from null tagged pointers.
    numArenasFreeCommitted(0),
    verifyPreData(nullptr),
    chunkAllocationSinceLastGC(false),
    lastGCTime(mozilla::TimeStamp::Now()),
    mode(TuningDefaults::Mode),
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
    isFull(false),
    incrementalState(gc::State::NotActive),
    initialState(gc::State::NotActive),
#ifdef JS_GC_ZEAL
    useZeal(false),
#endif
    lastMarkSlice(false),
    safeToYield(true),
    sweepOnBackgroundThread(false),
    blocksToFreeAfterSweeping((size_t) JSContext::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    sweepGroupIndex(0),
    sweepGroups(nullptr),
    currentSweepGroup(nullptr),
    sweepZone(nullptr),
    abortSweepAfterCurrentGroup(false),
    startedCompacting(false),
    relocatedArenasToRelease(nullptr),
#ifdef JS_GC_ZEAL
    markingValidator(nullptr),
#endif
    defaultTimeBudget_(TuningDefaults::DefaultTimeBudget),
    incrementalAllowed(true),
    compactingEnabled(TuningDefaults::CompactingEnabled),
    rootsRemoved(false),
#ifdef JS_GC_ZEAL
    zealModeBits(0),
    zealFrequency(0),
    nextScheduled(0),
    deterministicOnly(false),
    incrementalLimit(0),
#endif
    fullCompartmentChecks(false),
    gcCallbackDepth(0),
    alwaysPreserveCode(false),
#ifdef DEBUG
    arenasEmptyAtShutdown(true),
#endif
    lock(mutexid::GCLock),
    allocTask(rt, emptyChunks_.ref()),
    sweepTask(rt),
    decommitTask(rt),
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
    "    0:  (None) Normal amount of collection (resets all modes)\n"
    "    1:  (RootsChange) Collect when roots are added or removed\n"
    "    2:  (Alloc) Collect when every N allocations (default: 100)\n"
    "    4:  (VerifierPre) Verify pre write barriers between instructions\n"
    "    7:  (GenerationalGC) Collect the nursery every N nursery allocations\n"
    "    8:  (YieldBeforeMarking) Incremental GC in two slices that yields between\n"
    "        the root marking and marking phases\n"
    "    9:  (YieldBeforeSweeping) Incremental GC in two slices that yields between\n"
    "        the marking and sweeping phases\n"
    "    10: (IncrementalMultipleSlices) Incremental GC in many slices\n"
    "    11: (IncrementalMarkingValidator) Verify incremental marking\n"
    "    12: (ElementsBarrier) Use the individual element post-write barrier\n"
    "        regardless of elements size\n"
    "    13: (CheckHashTablesOnMinorGC) Check internal hashtables on minor GC\n"
    "    14: (Compact) Perform a shrinking collection every N allocations\n"
    "    15: (CheckHeapAfterGC) Walk the heap to check its integrity after every GC\n"
    "    16: (CheckNursery) Check nursery integrity on minor GC\n"
    "    17: (YieldBeforeSweepingAtoms) Incremental GC in two slices that yields\n"
    "        before sweeping the atoms table\n"
    "    18: (CheckGrayMarking) Check gray marking invariants after every GC\n"
    "    19: (YieldBeforeSweepingCaches) Incremental GC in two slices that yields\n"
    "        before sweeping weak caches\n"
    "    20: (YieldBeforeSweepingTypes) Incremental GC in two slices that yields\n"
    "        before sweeping type information\n"
    "    21: (YieldBeforeSweepingObjects) Incremental GC in two slices that yields\n"
    "        before sweeping foreground finalized objects\n"
    "    22: (YieldBeforeSweepingNonObjects) Incremental GC in two slices that yields\n"
    "        before sweeping non-object GC things\n"
    "    23: (YieldBeforeSweepingShapeTrees) Incremental GC in two slices that yields\n"
    "        before sweeping shape trees\n";

// The set of zeal modes that control incremental slices. These modes are
// mutually exclusive.
static const mozilla::EnumSet<ZealMode> IncrementalSliceZealModes = {
    ZealMode::YieldBeforeMarking,
    ZealMode::YieldBeforeSweeping,
    ZealMode::IncrementalMultipleSlices,
    ZealMode::YieldBeforeSweepingAtoms,
    ZealMode::YieldBeforeSweepingCaches,
    ZealMode::YieldBeforeSweepingTypes,
    ZealMode::YieldBeforeSweepingObjects,
    ZealMode::YieldBeforeSweepingNonObjects,
    ZealMode::YieldBeforeSweepingShapeTrees
};

void
GCRuntime::setZeal(uint8_t zeal, uint32_t frequency)
{
    MOZ_ASSERT(zeal <= unsigned(ZealMode::Limit));

    if (temporaryAbortIfWasmGc(rt->mainContextFromOwnThread()))
        return;

    if (verifyPreData)
        VerifyBarriers(rt, PreBarrierVerifier);

    if (zeal == 0) {
        if (hasZealMode(ZealMode::GenerationalGC)) {
            evictNursery(JS::gcreason::DEBUG_GC);
            nursery().leaveZealMode();
        }

        if (isIncrementalGCInProgress())
            finishGC(JS::gcreason::DEBUG_GC);
    }

    ZealMode zealMode = ZealMode(zeal);
    if (zealMode == ZealMode::GenerationalGC)
        nursery().enterZealMode();

    // Some modes are mutually exclusive. If we're setting one of those, we
    // first reset all of them.
    if (IncrementalSliceZealModes.contains(zealMode)) {
        for (auto mode : IncrementalSliceZealModes)
            clearZealMode(mode);
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

using CharRange = mozilla::Range<const char>;
using CharRangeVector = Vector<CharRange, 0, SystemAllocPolicy>;

static bool
ParseZealModeName(CharRange text, uint32_t* modeOut)
{
    struct ModeInfo
    {
        const char* name;
        size_t length;
        uint32_t value;
    };

    static const ModeInfo zealModes[] = {
        {"None", 0},
#define ZEAL_MODE(name, value) {#name, strlen(#name), value},
        JS_FOR_EACH_ZEAL_MODE(ZEAL_MODE)
#undef ZEAL_MODE
    };

    for (auto mode : zealModes) {
        if (text.length() == mode.length &&
            memcmp(text.begin().get(), mode.name, mode.length) == 0)
        {
            *modeOut = mode.value;
            return true;
        }
    }

    return false;
}

static bool
ParseZealModeNumericParam(CharRange text, uint32_t* paramOut)
{
    if (text.length() == 0)
        return false;

    for (auto c : text) {
        if (!isdigit(c))
            return false;
    }

    *paramOut = atoi(text.begin().get());
    return true;
}

static bool
SplitStringBy(CharRange text, char delimiter, CharRangeVector* result)
{
    auto start = text.begin();
    for (auto ptr = start; ptr != text.end(); ptr++) {
        if (*ptr == delimiter) {
            if (!result->emplaceBack(start, ptr))
                return false;
            start = ptr + 1;
        }
    }

    return result->emplaceBack(start, text.end());
}

static bool
PrintZealHelpAndFail()
{
    fprintf(stderr, "Format: JS_GC_ZEAL=level(;level)*[,N]\n");
    fputs(ZealModeHelpText, stderr);
    return false;
}

bool
GCRuntime::parseAndSetZeal(const char* str)
{
    // Set the zeal mode from a string consisting of one or more mode specifiers
    // separated by ';', optionally followed by a ',' and the trigger frequency.
    // The mode specifiers can by a mode name or its number.

    auto text = CharRange(str, strlen(str));

    CharRangeVector parts;
    if (!SplitStringBy(text, ',', &parts))
        return false;

    if (parts.length() == 0 || parts.length() > 2)
        return PrintZealHelpAndFail();

    uint32_t frequency = JS_DEFAULT_ZEAL_FREQ;
    if (parts.length() == 2 && !ParseZealModeNumericParam(parts[1], &frequency))
        return PrintZealHelpAndFail();

    CharRangeVector modes;
    if (!SplitStringBy(parts[0], ';', &modes))
        return false;

    for (const auto& descr : modes) {
        uint32_t mode;
        if (!ParseZealModeName(descr, &mode) && !ParseZealModeNumericParam(descr, &mode))
            return PrintZealHelpAndFail();

        setZeal(mode, frequency);
    }

    return true;
}

static const char*
AllocKindName(AllocKind kind)
{
    static const char* names[] = {
#define EXPAND_THING_NAME(allocKind, _1, _2, _3, _4, _5) \
        #allocKind,
FOR_EACH_ALLOCKIND(EXPAND_THING_NAME)
#undef EXPAND_THING_NAME
    };
    static_assert(ArrayLength(names) == size_t(AllocKind::LIMIT),
                  "names array should have an entry for every AllocKind");

    size_t i = size_t(kind);
    MOZ_ASSERT(i < ArrayLength(names));
    return names[i];
}

void
js::gc::DumpArenaInfo()
{
    fprintf(stderr, "Arena header size: %zu\n\n", ArenaHeaderSize);

    fprintf(stderr, "GC thing kinds:\n");
    fprintf(stderr, "%25s %8s %8s %8s\n", "AllocKind:", "Size:", "Count:", "Padding:");
    for (auto kind : AllAllocKinds()) {
        fprintf(stderr,
                "%25s %8zu %8zu %8zu\n",
                AllocKindName(kind),
                Arena::thingSize(kind),
                Arena::thingsPerArena(kind),
                Arena::firstThingOffset(kind) - ArenaHeaderSize);
    }
}

#endif // JS_GC_ZEAL

/*
 * Lifetime in number of major GCs for type sets attached to scripts containing
 * observed types.
 */
static const uint64_t JIT_SCRIPT_RELEASE_TYPES_PERIOD = 20;

bool
GCRuntime::init(uint32_t maxbytes, uint32_t maxNurseryBytes)
{
    MOZ_ASSERT(SystemPageSize());

    if (!rootsHash.ref().init(256))
        return false;

    {
        AutoLockGCBgAlloc lock(rt);

        MOZ_ALWAYS_TRUE(tunables.setParameter(JSGC_MAX_BYTES, maxbytes, lock));
        MOZ_ALWAYS_TRUE(tunables.setParameter(JSGC_MAX_NURSERY_BYTES, maxNurseryBytes, lock));
        setMaxMallocBytes(TuningDefaults::MaxMallocBytes, lock);

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

    if (!gcTracer.initTrace(*this))
        return false;

    if (!marker.init(mode))
        return false;

    if (!initSweepActions())
        return false;

    return true;
}

void
GCRuntime::finish()
{
    /* Wait for nursery background free to end and disable it to release memory. */
    if (nursery().isEnabled()) {
        nursery().waitBackgroundFreeEnd();
        nursery().disable();
    }

    /*
     * Wait until the background finalization and allocation stops and the
     * helper thread shuts down before we forcefully release any remaining GC
     * memory.
     */
    sweepTask.join();
    allocTask.cancelAndWait();
    decommitTask.cancelAndWait();

#ifdef JS_GC_ZEAL
    /* Free memory associated with GC verification. */
    finishVerifier();
#endif

    /* Delete all remaining zones. */
    if (rt->gcInitialized) {
        AutoSetThreadIsSweeping threadIsSweeping;
        for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
            for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
                for (RealmsInCompartmentIter realm(comp); !realm.done(); realm.next())
                    js_delete(realm.get());
                comp->realms().clear();
                js_delete(comp.get());
            }
            zone->compartments().clear();
            js_delete(zone.get());
        }
    }

    zones().clear();

    FreeChunkPool(fullChunks_.ref());
    FreeChunkPool(availableChunks_.ref());
    FreeChunkPool(emptyChunks_.ref());

    gcTracer.finishTrace();

    nursery().printTotalProfileTimes();
    stats().printTotalProfileTimes();
}

bool
GCRuntime::setParameter(JSGCParamKey key, uint32_t value, AutoLockGC& lock)
{
    switch (key) {
      case JSGC_MAX_MALLOC_BYTES:
        setMaxMallocBytes(value, lock);
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
        highFrequencyThreshold_ = mozilla::TimeDuration::FromMilliseconds(value);
        break;
      case JSGC_HIGH_FREQUENCY_LOW_LIMIT: {
        uint64_t newLimit = (uint64_t)value * 1024 * 1024;
        if (newLimit == UINT64_MAX)
            return false;
        setHighFrequencyLowLimit(newLimit);
        break;
      }
      case JSGC_HIGH_FREQUENCY_HIGH_LIMIT: {
        uint64_t newLimit = (uint64_t)value * 1024 * 1024;
        if (newLimit == 0)
            return false;
        setHighFrequencyHighLimit(newLimit);
        break;
      }
      case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX: {
        double newGrowth = value / 100.0;
        if (newGrowth < MinHighFrequencyHeapGrowthFactor || newGrowth > MaxHeapGrowthFactor)
            return false;
        setHighFrequencyHeapGrowthMax(newGrowth);
        break;
      }
      case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN: {
        double newGrowth = value / 100.0;
        if (newGrowth < MinHighFrequencyHeapGrowthFactor || newGrowth > MaxHeapGrowthFactor)
            return false;
        setHighFrequencyHeapGrowthMin(newGrowth);
        break;
      }
      case JSGC_LOW_FREQUENCY_HEAP_GROWTH: {
        double newGrowth = value / 100.0;
        if (newGrowth < MinLowFrequencyHeapGrowthFactor || newGrowth > MaxHeapGrowthFactor)
            return false;
        setLowFrequencyHeapGrowth(newGrowth);
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
      case JSGC_ALLOCATION_THRESHOLD_FACTOR: {
        double newFactor = value / 100.0;
        if (newFactor < MinAllocationThresholdFactor || newFactor > 1.0)
            return false;
        allocThresholdFactor_ = newFactor;
        break;
      }
      case JSGC_ALLOCATION_THRESHOLD_FACTOR_AVOID_INTERRUPT: {
        double newFactor = value / 100.0;
        if (newFactor < MinAllocationThresholdFactor || newFactor > 1.0)
            return false;
        allocThresholdFactorAvoidInterrupt_ = newFactor;
        break;
      }
      case JSGC_MIN_EMPTY_CHUNK_COUNT:
        setMinEmptyChunkCount(value);
        break;
      case JSGC_MAX_EMPTY_CHUNK_COUNT:
        setMaxEmptyChunkCount(value);
        break;
      case JSGC_NURSERY_FREE_THRESHOLD_FOR_IDLE_COLLECTION:
        if (value > gcMaxNurseryBytes())
            value = gcMaxNurseryBytes();
        nurseryFreeThresholdForIdleCollection_ = value;
        break;
      default:
        MOZ_CRASH("Unknown GC parameter.");
    }

    return true;
}

void
GCSchedulingTunables::setMaxMallocBytes(size_t value)
{
    maxMallocBytes_ = std::min(value, TuningDefaults::MallocThresholdLimit);
}

void
GCSchedulingTunables::setHighFrequencyLowLimit(uint64_t newLimit)
{
    highFrequencyLowLimitBytes_ = newLimit;
    if (highFrequencyLowLimitBytes_ >= highFrequencyHighLimitBytes_)
        highFrequencyHighLimitBytes_ = highFrequencyLowLimitBytes_ + 1;
    MOZ_ASSERT(highFrequencyHighLimitBytes_ > highFrequencyLowLimitBytes_);
}

void
GCSchedulingTunables::setHighFrequencyHighLimit(uint64_t newLimit)
{
    highFrequencyHighLimitBytes_ = newLimit;
    if (highFrequencyHighLimitBytes_ <= highFrequencyLowLimitBytes_)
        highFrequencyLowLimitBytes_ = highFrequencyHighLimitBytes_ - 1;
    MOZ_ASSERT(highFrequencyHighLimitBytes_ > highFrequencyLowLimitBytes_);
}

void
GCSchedulingTunables::setHighFrequencyHeapGrowthMin(double value)
{
    highFrequencyHeapGrowthMin_ = value;
    if (highFrequencyHeapGrowthMin_ > highFrequencyHeapGrowthMax_)
        highFrequencyHeapGrowthMax_ = highFrequencyHeapGrowthMin_;
    MOZ_ASSERT(highFrequencyHeapGrowthMin_ >= MinHighFrequencyHeapGrowthFactor);
    MOZ_ASSERT(highFrequencyHeapGrowthMin_ <= highFrequencyHeapGrowthMax_);
}

void
GCSchedulingTunables::setHighFrequencyHeapGrowthMax(double value)
{
    highFrequencyHeapGrowthMax_ = value;
    if (highFrequencyHeapGrowthMax_ < highFrequencyHeapGrowthMin_)
        highFrequencyHeapGrowthMin_ = highFrequencyHeapGrowthMax_;
    MOZ_ASSERT(highFrequencyHeapGrowthMin_ >= MinHighFrequencyHeapGrowthFactor);
    MOZ_ASSERT(highFrequencyHeapGrowthMin_ <= highFrequencyHeapGrowthMax_);
}

void
GCSchedulingTunables::setLowFrequencyHeapGrowth(double value)
{
    lowFrequencyHeapGrowth_ = value;
    MOZ_ASSERT(lowFrequencyHeapGrowth_ >= MinLowFrequencyHeapGrowthFactor);
}

void
GCSchedulingTunables::setMinEmptyChunkCount(uint32_t value)
{
    minEmptyChunkCount_ = value;
    if (minEmptyChunkCount_ > maxEmptyChunkCount_)
        maxEmptyChunkCount_ = minEmptyChunkCount_;
    MOZ_ASSERT(maxEmptyChunkCount_ >= minEmptyChunkCount_);
}

void
GCSchedulingTunables::setMaxEmptyChunkCount(uint32_t value)
{
    maxEmptyChunkCount_ = value;
    if (minEmptyChunkCount_ > maxEmptyChunkCount_)
        minEmptyChunkCount_ = maxEmptyChunkCount_;
    MOZ_ASSERT(maxEmptyChunkCount_ >= minEmptyChunkCount_);
}

GCSchedulingTunables::GCSchedulingTunables()
  : gcMaxBytes_(0),
    maxMallocBytes_(TuningDefaults::MaxMallocBytes),
    gcMaxNurseryBytes_(0),
    gcZoneAllocThresholdBase_(TuningDefaults::GCZoneAllocThresholdBase),
    allocThresholdFactor_(TuningDefaults::AllocThresholdFactor),
    allocThresholdFactorAvoidInterrupt_(TuningDefaults::AllocThresholdFactorAvoidInterrupt),
    zoneAllocDelayBytes_(TuningDefaults::ZoneAllocDelayBytes),
    dynamicHeapGrowthEnabled_(TuningDefaults::DynamicHeapGrowthEnabled),
    highFrequencyThreshold_(mozilla::TimeDuration::FromSeconds(TuningDefaults::HighFrequencyThreshold)),
    highFrequencyLowLimitBytes_(TuningDefaults::HighFrequencyLowLimitBytes),
    highFrequencyHighLimitBytes_(TuningDefaults::HighFrequencyHighLimitBytes),
    highFrequencyHeapGrowthMax_(TuningDefaults::HighFrequencyHeapGrowthMax),
    highFrequencyHeapGrowthMin_(TuningDefaults::HighFrequencyHeapGrowthMin),
    lowFrequencyHeapGrowth_(TuningDefaults::LowFrequencyHeapGrowth),
    dynamicMarkSliceEnabled_(TuningDefaults::DynamicMarkSliceEnabled),
    minEmptyChunkCount_(TuningDefaults::MinEmptyChunkCount),
    maxEmptyChunkCount_(TuningDefaults::MaxEmptyChunkCount),
    nurseryFreeThresholdForIdleCollection_(TuningDefaults::NurseryFreeThresholdForIdleCollection)
{}

void
GCRuntime::resetParameter(JSGCParamKey key, AutoLockGC& lock)
{
    switch (key) {
      case JSGC_MAX_MALLOC_BYTES:
        setMaxMallocBytes(TuningDefaults::MaxMallocBytes, lock);
        break;
      case JSGC_SLICE_TIME_BUDGET:
        defaultTimeBudget_ = TuningDefaults::DefaultTimeBudget;
        break;
      case JSGC_MARK_STACK_LIMIT:
        setMarkStackLimit(MarkStack::DefaultCapacity, lock);
        break;
      case JSGC_MODE:
        mode = TuningDefaults::Mode;
        break;
      case JSGC_COMPACTING_ENABLED:
        compactingEnabled = TuningDefaults::CompactingEnabled;
        break;
      default:
        tunables.resetParameter(key, lock);
        for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
            zone->threshold.updateAfterGC(zone->usage.gcBytes(), GC_NORMAL,
                tunables, schedulingState, lock);
        }
    }
}

void
GCSchedulingTunables::resetParameter(JSGCParamKey key, const AutoLockGC& lock)
{
    switch(key) {
      case JSGC_MAX_BYTES:
        gcMaxBytes_ = 0xffffffff;
        break;
      case JSGC_MAX_NURSERY_BYTES:
        gcMaxNurseryBytes_ = JS::DefaultNurseryBytes;
        break;
      case JSGC_HIGH_FREQUENCY_TIME_LIMIT:
        highFrequencyThreshold_ =
            mozilla::TimeDuration::FromSeconds(TuningDefaults::HighFrequencyThreshold);
        break;
      case JSGC_HIGH_FREQUENCY_LOW_LIMIT:
        setHighFrequencyLowLimit(TuningDefaults::HighFrequencyLowLimitBytes);
        break;
      case JSGC_HIGH_FREQUENCY_HIGH_LIMIT:
        setHighFrequencyHighLimit(TuningDefaults::HighFrequencyHighLimitBytes);
        break;
      case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX:
        setHighFrequencyHeapGrowthMax(TuningDefaults::HighFrequencyHeapGrowthMax);
        break;
      case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN:
        setHighFrequencyHeapGrowthMin(TuningDefaults::HighFrequencyHeapGrowthMin);
        break;
      case JSGC_LOW_FREQUENCY_HEAP_GROWTH:
        setLowFrequencyHeapGrowth(TuningDefaults::LowFrequencyHeapGrowth);
        break;
      case JSGC_DYNAMIC_HEAP_GROWTH:
        dynamicHeapGrowthEnabled_ = TuningDefaults::DynamicHeapGrowthEnabled;
        break;
      case JSGC_DYNAMIC_MARK_SLICE:
        dynamicMarkSliceEnabled_ = TuningDefaults::DynamicMarkSliceEnabled;
        break;
      case JSGC_ALLOCATION_THRESHOLD:
        gcZoneAllocThresholdBase_ = TuningDefaults::GCZoneAllocThresholdBase;
        break;
      case JSGC_ALLOCATION_THRESHOLD_FACTOR:
        allocThresholdFactor_ = TuningDefaults::AllocThresholdFactor;
        break;
      case JSGC_ALLOCATION_THRESHOLD_FACTOR_AVOID_INTERRUPT:
        allocThresholdFactorAvoidInterrupt_ = TuningDefaults::AllocThresholdFactorAvoidInterrupt;
        break;
      case JSGC_MIN_EMPTY_CHUNK_COUNT:
        setMinEmptyChunkCount(TuningDefaults::MinEmptyChunkCount);
        break;
      case JSGC_MAX_EMPTY_CHUNK_COUNT:
        setMaxEmptyChunkCount(TuningDefaults::MaxEmptyChunkCount);
        break;
      case JSGC_NURSERY_FREE_THRESHOLD_FOR_IDLE_COLLECTION:
        nurseryFreeThresholdForIdleCollection_ =
            TuningDefaults::NurseryFreeThresholdForIdleCollection;
        break;
      default:
        MOZ_CRASH("Unknown GC parameter.");
    }
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
        return tunables.highFrequencyThreshold().ToMilliseconds();
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
      case JSGC_ALLOCATION_THRESHOLD_FACTOR:
        return uint32_t(tunables.allocThresholdFactor() * 100);
      case JSGC_ALLOCATION_THRESHOLD_FACTOR_AVOID_INTERRUPT:
        return uint32_t(tunables.allocThresholdFactorAvoidInterrupt() * 100);
      case JSGC_MIN_EMPTY_CHUNK_COUNT:
        return tunables.minEmptyChunkCount(lock);
      case JSGC_MAX_EMPTY_CHUNK_COUNT:
        return tunables.maxEmptyChunkCount();
      case JSGC_COMPACTING_ENABLED:
        return compactingEnabled;
      default:
        MOZ_ASSERT(key == JSGC_NUMBER);
        return uint32_t(number);
    }
}

void
GCRuntime::setMarkStackLimit(size_t limit, AutoLockGC& lock)
{
    MOZ_ASSERT(!JS::RuntimeHeapIsBusy());
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
    MOZ_ASSERT(gcCallback.op);
    gcCallback.op(rt->mainContextFromOwnThread(), status, gcCallback.data);
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
        tenuredCallback.op(rt->mainContextFromOwnThread(), tenuredCallback.data);
}

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
        p.op(fop, status, p.data);
}

bool
GCRuntime::addWeakPointerZonesCallback(JSWeakPointerZonesCallback callback, void* data)
{
    return updateWeakPointerZonesCallbacks.ref().append(
            Callback<JSWeakPointerZonesCallback>(callback, data));
}

void
GCRuntime::removeWeakPointerZonesCallback(JSWeakPointerZonesCallback callback)
{
    for (auto& p : updateWeakPointerZonesCallbacks.ref()) {
        if (p.op == callback) {
            updateWeakPointerZonesCallbacks.ref().erase(&p);
            break;
        }
    }
}

void
GCRuntime::callWeakPointerZonesCallbacks() const
{
    JSContext* cx = rt->mainContextFromOwnThread();
    for (auto const& p : updateWeakPointerZonesCallbacks.ref())
        p.op(cx, p.data);
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
GCRuntime::callWeakPointerCompartmentCallbacks(JS::Compartment* comp) const
{
    JSContext* cx = rt->mainContextFromOwnThread();
    for (auto const& p : updateWeakPointerCompartmentCallbacks.ref())
        p.op(cx, comp, p.data);
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
    notifyRootsRemoved();
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
GCRuntime::setMaxMallocBytes(size_t value, const AutoLockGC& lock)
{
    tunables.setMaxMallocBytes(value);
    mallocCounter.setMax(value, lock);
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
        zone->setGCMaxMallocBytes(value, lock);
}

double
ZoneHeapThreshold::eagerAllocTrigger(bool highFrequencyGC) const
{
    double eagerTriggerFactor = highFrequencyGC ? HighFrequencyEagerAllocTriggerFactor
                                                : LowFrequencyEagerAllocTriggerFactor;
    return eagerTriggerFactor * gcTriggerBytes();
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

    double minRatio = tunables.highFrequencyHeapGrowthMin();
    double maxRatio = tunables.highFrequencyHeapGrowthMax();
    double lowLimit = tunables.highFrequencyLowLimitBytes();
    double highLimit = tunables.highFrequencyHighLimitBytes();

    MOZ_ASSERT(minRatio <= maxRatio);
    MOZ_ASSERT(lowLimit < highLimit);

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

MemoryCounter::MemoryCounter()
  : bytes_(0),
    maxBytes_(0),
    triggered_(NoTrigger)
{}

void
MemoryCounter::updateOnGCStart()
{
    // Record the current byte count at the start of GC.
    bytesAtStartOfGC_ = bytes_;
}

void
MemoryCounter::updateOnGCEnd(const GCSchedulingTunables& tunables, const AutoLockGC& lock)
{
    // Update the trigger threshold at the end of GC and adjust the current
    // byte count to reflect bytes allocated since the start of GC.
    MOZ_ASSERT(bytes_ >= bytesAtStartOfGC_);
    if (shouldTriggerGC(tunables)) {
        maxBytes_ = std::min(TuningDefaults::MallocThresholdLimit,
                             size_t(maxBytes_ * TuningDefaults::MallocThresholdGrowFactor));
    } else {
        maxBytes_ = std::max(tunables.maxMallocBytes(),
                             size_t(maxBytes_ * TuningDefaults::MallocThresholdShrinkFactor));
    }
    bytes_ -= bytesAtStartOfGC_;
    triggered_ = NoTrigger;
}

void
MemoryCounter::setMax(size_t newMax, const AutoLockGC& lock)
{
    maxBytes_ = newMax;
}

void
MemoryCounter::adopt(MemoryCounter& other)
{
    update(other.bytes());
    other.bytes_ = 0;
    other.triggered_ = NoTrigger;
}

void
MemoryCounter::recordTrigger(TriggerKind trigger)
{
    MOZ_ASSERT(trigger > triggered_);
    triggered_ = trigger;
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

/* Compacting GC */

bool
GCRuntime::shouldCompact()
{
    // Compact on shrinking GC if enabled.  Skip compacting in incremental GCs
    // if we are currently animating, unless the user is inactive or we're
    // responding to memory pressure.

    static const auto oneSecond = mozilla::TimeDuration::FromSeconds(1);

    if (invocationKind != GC_SHRINK || !isCompactingGCEnabled())
        return false;

    if (initialReason == JS::gcreason::USER_INACTIVE ||
        initialReason == JS::gcreason::MEM_PRESSURE)
    {
        return true;
    }

    const auto &lastAnimationTime = rt->lastAnimationTime.ref();
    return !isIncremental
        || lastAnimationTime.IsNull()
        || lastAnimationTime + oneSecond < mozilla::TimeStamp::Now();
}

bool
GCRuntime::isCompactingGCEnabled() const
{
    return compactingEnabled && rt->mainContextFromOwnThread()->compactingDisabledCount == 0;
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
    AllocKind::SHAPE,
    AllocKind::ACCESSOR_SHAPE,
    AllocKind::BASE_SHAPE,
    AllocKind::FAT_INLINE_STRING,
    AllocKind::STRING,
    AllocKind::EXTERNAL_STRING,
    AllocKind::FAT_INLINE_ATOM,
    AllocKind::ATOM,
    AllocKind::SCOPE,
    AllocKind::REGEXP_SHARED
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

static void
RelocateCell(Zone* zone, TenuredCell* src, AllocKind thingKind, size_t thingSize)
{
    JS::AutoSuppressGCAnalysis nogc(TlsContext.get());

    // Allocate a new cell.
    MOZ_ASSERT(zone == src->zone());
    TenuredCell* dst = AllocateCellInGC(zone, thingKind);

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
            if (srcNative->hasFixedElements()) {
                uint32_t numShifted = srcNative->getElementsHeader()->numShiftedElements();
                dstNative->setFixedElements(numShifted);
            }

            // For copy-on-write objects that own their elements, fix up the
            // owner pointer to point to the relocated object.
            if (srcNative->denseElementsAreCopyOnWrite()) {
                GCPtrNativeObject& owner = dstNative->getElementsHeader()->ownerObject();
                if (owner == srcNative)
                    owner = dstNative;
            }
        } else if (srcObj->is<ProxyObject>()) {
            if (srcObj->as<ProxyObject>().usingInlineValueArray())
                dstObj->as<ProxyObject>().setInlineValueArray();
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
        MOZ_ASSERT(src->isMarkedBlack() == dest->isMarkedBlack());
        MOZ_ASSERT(src->isMarkedGray() == dest->isMarkedGray());
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
ArenaLists::relocateArenas(Arena*& relocatedListOut, JS::gcreason::Reason reason,
                           SliceBudget& sliceBudget, gcstats::Statistics& stats)
{
    // This is only called from the main thread while we are doing a GC, so
    // there is no need to lock.
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime()));
    MOZ_ASSERT(runtime()->gc.isHeapCompacting());
    MOZ_ASSERT(!runtime()->gc.isBackgroundSweeping());

    // Clear all the free lists.
    clearFreeLists();

    if (ShouldRelocateAllArenas(reason)) {
        zone_->prepareForCompacting();
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

        zone_->prepareForCompacting();
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
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::COMPACT_MOVE);

    MOZ_ASSERT(!zone->isPreservingCode());
    MOZ_ASSERT(CanRelocateZone(zone));

    js::CancelOffThreadIonCompile(rt, JS::Zone::Compact);

    if (!zone->arenas.relocateArenas(relocatedListOut, reason, sliceBudget, stats()))
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

template <typename T>
inline void
MovingTracer::updateEdge(T** thingp)
{
    auto thing = *thingp;
    if (thing->runtimeFromAnyThread() == runtime() && IsForwarded(thing))
        *thingp = Forwarded(thing);
}

void MovingTracer::onObjectEdge(JSObject** objp) { updateEdge(objp); }
void MovingTracer::onShapeEdge(Shape** shapep) { updateEdge(shapep); }
void MovingTracer::onStringEdge(JSString** stringp) { updateEdge(stringp); }
void MovingTracer::onScriptEdge(JSScript** scriptp) { updateEdge(scriptp); }
void MovingTracer::onLazyScriptEdge(LazyScript** lazyp) { updateEdge(lazyp); }
void MovingTracer::onBaseShapeEdge(BaseShape** basep) { updateEdge(basep); }
void MovingTracer::onScopeEdge(Scope** scopep) { updateEdge(scopep); }
void MovingTracer::onRegExpSharedEdge(RegExpShared** sharedp) { updateEdge(sharedp); }

void
Zone::prepareForCompacting()
{
    FreeOp* fop = runtimeFromMainThread()->defaultFreeOp();
    discardJitCode(fop);
}

void
GCRuntime::sweepTypesAfterCompacting(Zone* zone)
{
    zone->beginSweepTypes(rt->gc.releaseObservedTypes && !zone->isPreservingCode());

    AutoClearTypeInferenceStateOnOOM oom(zone);

    for (auto script = zone->cellIter<JSScript>(); !script.done(); script.next())
        AutoSweepTypeScript sweep(script, &oom);
    for (auto group = zone->cellIter<ObjectGroup>(); !group.done(); group.next())
        AutoSweepObjectGroup sweep(group, &oom);

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

    if (jit::JitZone* jitZone = zone->jitZone())
        jitZone->sweep();

    for (RealmsInZoneIter r(zone); !r.done(); r.next()) {
        r->sweepObjectGroups();
        r->sweepRegExps();
        r->sweepSavedStacks();
        r->sweepVarNames();
        r->sweepGlobalObject();
        r->sweepSelfHostingScriptSource();
        r->sweepDebugEnvironments();
        r->sweepJitRealm();
        r->sweepObjectRealm();
        r->sweepTemplateObjects();
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
UpdateArenaPointersTyped(MovingTracer* trc, Arena* arena)
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
#define EXPAND_CASE(allocKind, traceKind, type, sizedType, bgFinal, nursery) \
      case AllocKind::allocKind: \
        UpdateArenaPointersTyped<type>(trc, arena); \
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

struct UpdatePointersTask : public GCParallelTaskHelper<UpdatePointersTask>
{
    // Maximum number of arenas to update in one block.
#ifdef DEBUG
    static const unsigned MaxArenasToProcess = 16;
#else
    static const unsigned MaxArenasToProcess = 256;
#endif

    UpdatePointersTask(JSRuntime* rt, ArenasToUpdate* source, AutoLockHelperThreadState& lock)
      : GCParallelTaskHelper(rt), source_(source)
    {
        arenas_.begin = nullptr;
        arenas_.end = nullptr;
    }

    void run();

  private:
    ArenasToUpdate* source_;
    ArenaListSegment arenas_;

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

void
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
GCRuntime::updateCellPointers(Zone* zone, AllocKinds kinds, size_t bgTaskCount)
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
            startTask(*bgTasks[i], gcstats::PhaseKind::COMPACT_UPDATE_CELLS, lock);
            tasksStarted++;
        }
    }

    fgTask->runFromMainThread(rt);

    {
        AutoLockHelperThreadState lock;

        for (size_t i = 0; i < tasksStarted; i++)
            joinTask(*bgTasks[i], gcstats::PhaseKind::COMPACT_UPDATE_CELLS, lock);
        for (size_t i = tasksStarted; i < MaxCellUpdateBackgroundTasks; i++)
            MOZ_ASSERT(bgTasks[i].isNothing());
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
// Also, JSScripts and LazyScripts can have pointers to each other. Each can be
// updated safely without requiring the referent to be up-to-date, but TSAN can
// warn about data races when calling IsForwarded() on the new location of a
// cell that is being updated in parallel. To avoid this, we update these in
// separate phases.
//
// Since we want to minimize the number of phases, arrange kinds into three
// arbitrary phases.

static const AllocKinds UpdatePhaseOne {
    AllocKind::SCRIPT,
    AllocKind::BASE_SHAPE,
    AllocKind::SHAPE,
    AllocKind::ACCESSOR_SHAPE,
    AllocKind::OBJECT_GROUP,
    AllocKind::STRING,
    AllocKind::JITCODE,
    AllocKind::SCOPE
};

// UpdatePhaseTwo is typed object descriptor objects.

static const AllocKinds UpdatePhaseThree {
    AllocKind::LAZY_SCRIPT,
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

    updateCellPointers(zone, UpdatePhaseOne, bgTaskCount);

    // UpdatePhaseTwo: Update TypeDescrs before all other objects as typed
    // objects access these objects when we trace them.
    updateTypeDescrObjects(trc, zone);

    updateCellPointers(zone, UpdatePhaseThree, bgTaskCount);
}

/*
 * Update pointers to relocated cells in a single zone by doing a traversal of
 * that zone's arenas and calling per-zone sweep hooks.
 *
 * The latter is necessary to update weak references which are not marked as
 * part of the traversal.
 */
void
GCRuntime::updateZonePointersToRelocatedCells(Zone* zone)
{
    MOZ_ASSERT(!rt->isBeingDestroyed());
    MOZ_ASSERT(zone->isGCCompacting());

    AutoTouchingGrayThings tgt;

    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::COMPACT_UPDATE);
    MovingTracer trc(rt);

    zone->fixupAfterMovingGC();

    // Fixup compartment global pointers as these get accessed during marking.
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next())
        comp->fixupAfterMovingGC();

    zone->externalStringCache().purge();
    zone->functionToStringCache().purge();

    // Iterate through all cells that can contain relocatable pointers to update
    // them. Since updating each cell is independent we try to parallelize this
    // as much as possible.
    updateAllCellPointers(&trc, zone);

    // Mark roots to update them.
    {
        gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::MARK_ROOTS);

        WeakMapBase::traceZone(zone, &trc);
    }

    // Sweep everything to fix up weak pointers.
    rt->gc.sweepZoneAfterCompacting(zone);

    // Call callbacks to get the rest of the system to fixup other untraced pointers.
    for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next())
        callWeakPointerCompartmentCallbacks(comp);
}

/*
 * Update runtime-wide pointers to relocated cells.
 */
void
GCRuntime::updateRuntimePointersToRelocatedCells(AutoGCSession& session)
{
    MOZ_ASSERT(!rt->isBeingDestroyed());

    gcstats::AutoPhase ap1(stats(), gcstats::PhaseKind::COMPACT_UPDATE);
    MovingTracer trc(rt);

    Compartment::fixupCrossCompartmentWrappersAfterMovingGC(&trc);

    rt->geckoProfiler().fixupStringsMapAfterMovingGC();

    traceRuntimeForMajorGC(&trc, session);

    // Mark roots to update them.
    {
        gcstats::AutoPhase ap2(stats(), gcstats::PhaseKind::MARK_ROOTS);
        Debugger::traceAllForMovingGC(&trc);
        Debugger::traceIncomingCrossCompartmentEdges(&trc);

        // Mark all gray roots, making sure we call the trace callback to get the
        // current set.
        if (JSTraceDataOp op = grayRootTracer.op)
            (*op)(&trc, grayRootTracer.data);
    }

    // Sweep everything to fix up weak pointers.
    Debugger::sweepAll(rt->defaultFreeOp());
    jit::JitRuntime::SweepJitcodeGlobalTable(rt);
    for (JS::detail::WeakCacheBase* cache : rt->weakCaches())
        cache->sweep();

    // Type inference may put more blocks here to free.
    {
        AutoLockHelperThreadState lock;
        blocksToFreeAfterSweeping.ref().freeAll();
    }

    // Call callbacks to get the rest of the system to fixup other untraced pointers.
    callWeakPointerZonesCallbacks();
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
                  JS_MOVED_TENURED_PATTERN, arena->getThingsSpan(),
                  MemCheckKind::MakeNoAccess);
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

FreeLists::FreeLists()
{
    for (auto i : AllAllocKinds())
        freeLists_[i] = &emptySentinel;
}

ArenaLists::ArenaLists(Zone* zone)
  : zone_(zone),
    freeLists_(zone),
    arenaLists_(zone),
    arenaListsToSweep_(),
    incrementalSweptArenaKind(zone, AllocKind::LIMIT),
    incrementalSweptArenas(zone),
    gcShapeArenasToUpdate(zone, nullptr),
    gcAccessorShapeArenasToUpdate(zone, nullptr),
    gcScriptArenasToUpdate(zone, nullptr),
    gcObjectGroupArenasToUpdate(zone, nullptr),
    savedEmptyArenas(zone, nullptr)
{
    for (auto i : AllAllocKinds()) {
        concurrentUse(i) = ConcurrentUse::None;
        arenaListsToSweep(i) = nullptr;
    }
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
    AutoLockGC lock(runtime());

    for (auto i : AllAllocKinds()) {
        /*
         * We can only call this during the shutdown after the last GC when
         * the background finalization is disabled.
         */
        MOZ_ASSERT(concurrentUse(i) == ConcurrentUse::None);
        ReleaseArenaList(runtime(), arenaLists(i).head(), lock);
    }
    ReleaseArenaList(runtime(), incrementalSweptArenas.ref().head(), lock);

    ReleaseArenaList(runtime(), savedEmptyArenas, lock);
}

void
ArenaLists::queueForForegroundSweep(FreeOp* fop, const FinalizePhase& phase)
{
    gcstats::AutoPhase ap(fop->runtime()->gc.stats(), phase.statsPhase);
    for (auto kind : phase.kinds)
        queueForForegroundSweep(kind);
}

void
ArenaLists::queueForForegroundSweep(AllocKind thingKind)
{
    MOZ_ASSERT(!IsBackgroundFinalized(thingKind));
    MOZ_ASSERT(concurrentUse(thingKind) == ConcurrentUse::None);
    MOZ_ASSERT(!arenaListsToSweep(thingKind));

    arenaListsToSweep(thingKind) = arenaLists(thingKind).head();
    arenaLists(thingKind).clear();
}

void
ArenaLists::queueForBackgroundSweep(FreeOp* fop, const FinalizePhase& phase)
{
    gcstats::AutoPhase ap(fop->runtime()->gc.stats(), phase.statsPhase);
    for (auto kind : phase.kinds)
        queueForBackgroundSweep(kind);
}

inline void
ArenaLists::queueForBackgroundSweep(AllocKind thingKind)
{
    MOZ_ASSERT(IsBackgroundFinalized(thingKind));

    ArenaList* al = &arenaLists(thingKind);
    if (al->isEmpty()) {
        MOZ_ASSERT(concurrentUse(thingKind) == ConcurrentUse::None);
        return;
    }

    MOZ_ASSERT(concurrentUse(thingKind) == ConcurrentUse::None);

    arenaListsToSweep(thingKind) = al->head();
    al->clear();
    concurrentUse(thingKind) = ConcurrentUse::BackgroundFinalize;
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
        AutoLockGC lock(lists->runtimeFromAnyThread());
        MOZ_ASSERT(lists->concurrentUse(thingKind) == ConcurrentUse::BackgroundFinalize);

        // Join |al| and |finalized| into a single list.
        *al = finalized.insertListWithCursorAtEnd(*al);

        lists->arenaListsToSweep(thingKind) = nullptr;
    }

    lists->concurrentUse(thingKind) = ConcurrentUse::None;
}

void
ArenaLists::releaseForegroundSweptEmptyArenas()
{
    AutoLockGC lock(runtime());
    ReleaseArenaList(runtime(), savedEmptyArenas, lock);
    savedEmptyArenas = nullptr;
}

void
ArenaLists::queueForegroundThingsForSweep()
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
        deadline = mozilla::TimeStamp::Now() + mozilla::TimeDuration::FromMilliseconds(time.budget);
        counter = CounterReset;
    }
}

SliceBudget::SliceBudget(WorkBudget work)
  : timeBudget(UnlimitedTimeBudget), workBudget(work)
{
    if (work.budget < 0) {
        makeUnlimited();
    } else {
        deadline = mozilla::TimeStamp();
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
    if (deadline.IsNull())
        return true;

    bool over = mozilla::TimeStamp::Now() >= deadline;
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
    rt->mainContextFromOwnThread()->requestInterrupt(InterruptReason::GC);
}

void
Nursery::requestMinorGC(JS::gcreason::Reason reason) const
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime()));
    MOZ_ASSERT(!CurrentThreadIsPerformingGC());

    if (minorGCRequested())
        return;

    minorGCTriggerReason_ = reason;
    runtime()->mainContextFromOwnThread()->requestInterrupt(InterruptReason::GC);
}

bool
GCRuntime::triggerGC(JS::gcreason::Reason reason)
{
    /*
     * Don't trigger GCs if this is being called off the main thread from
     * onTooMuchMalloc().
     */
    if (!CurrentThreadCanAccessRuntime(rt))
        return false;

    /* GC is already running. */
    if (JS::RuntimeHeapIsCollecting())
        return false;

    JS::PrepareForFullGC(rt->mainContextFromOwnThread());
    requestMajorGC(reason);
    return true;
}

void
GCRuntime::maybeAllocTriggerZoneGC(Zone* zone, const AutoLockGC& lock)
{
    if (!CurrentThreadCanAccessRuntime(rt)) {
        // Zones in use by a helper thread can't be collected.
        MOZ_ASSERT(zone->usedByHelperThread() || zone->isAtomsZone());
        return;
    }

    MOZ_ASSERT(!JS::RuntimeHeapIsCollecting());

    size_t usedBytes = zone->usage.gcBytes();
    size_t thresholdBytes = zone->threshold.gcTriggerBytes();

    if (usedBytes >= thresholdBytes) {
        // The threshold has been surpassed, immediately trigger a GC, which
        // will be done non-incrementally.
        triggerZoneGC(zone, JS::gcreason::ALLOC_TRIGGER, usedBytes, thresholdBytes);
        return;
    }

    bool wouldInterruptCollection = isIncrementalGCInProgress() && !zone->isCollecting();
    double zoneGCThresholdFactor =
        wouldInterruptCollection ? tunables.allocThresholdFactorAvoidInterrupt()
                                 : tunables.allocThresholdFactor();

    size_t igcThresholdBytes = thresholdBytes * zoneGCThresholdFactor;

    if (usedBytes >= igcThresholdBytes) {
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
            triggerZoneGC(zone, JS::gcreason::ALLOC_TRIGGER, usedBytes, igcThresholdBytes);

            // Delay the next slice until a certain amount of allocation
            // has been performed.
            zone->gcDelayBytes = tunables.zoneAllocDelayBytes();
            return;
        }
    }
}

bool
GCRuntime::triggerZoneGC(Zone* zone, JS::gcreason::Reason reason, size_t used, size_t threshold)
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));

    /* GC is already running. */
    if (JS::RuntimeHeapIsBusy())
        return false;

#ifdef JS_GC_ZEAL
    if (hasZealMode(ZealMode::Alloc)) {
        MOZ_RELEASE_ASSERT(triggerGC(reason));
        return true;
    }
#endif

    if (zone->isAtomsZone()) {
        /* We can't do a zone GC of just the atoms zone. */
        if (rt->hasHelperThreadZones()) {
            /* We can't collect atoms while off-thread parsing is allocating. */
            fullGCForAtomsRequested_ = true;
            return false;
        }
        stats().recordTrigger(used, threshold);
        MOZ_RELEASE_ASSERT(triggerGC(reason));
        return true;
    }

    stats().recordTrigger(used, threshold);
    PrepareZoneForGC(zone);
    requestMajorGC(reason);
    return true;
}

void
GCRuntime::maybeGC(Zone* zone)
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));

#ifdef JS_GC_ZEAL
    if (hasZealMode(ZealMode::Alloc) || hasZealMode(ZealMode::RootsChange)) {
        JS::PrepareForFullGC(rt->mainContextFromOwnThread());
        gc(GC_NORMAL, JS::gcreason::DEBUG_GC);
        return;
    }
#endif

    if (gcIfRequested())
        return;

    double threshold = zone->threshold.eagerAllocTrigger(schedulingState.inHighFrequencyGCMode());
    double usedBytes = zone->usage.gcBytes();
    if (usedBytes > 1024 * 1024 && usedBytes >= threshold &&
        !isIncrementalGCInProgress() && !isBackgroundSweeping())
    {
        stats().recordTrigger(usedBytes, threshold);
        PrepareZoneForGC(zone);
        startGC(GC_NORMAL, JS::gcreason::EAGER_ALLOC_TRIGGER);
    }
}

void
GCRuntime::triggerFullGCForAtoms(JSContext* cx)
{
    MOZ_ASSERT(fullGCForAtomsRequested_);
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
    MOZ_ASSERT(!JS::RuntimeHeapIsCollecting());
    MOZ_ASSERT(cx->canCollectAtoms());
    fullGCForAtomsRequested_ = false;
    MOZ_RELEASE_ASSERT(triggerGC(JS::gcreason::DELAYED_ATOMS_GC));
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

    decommitTask.runFromMainThread(rt);
}

void
js::gc::BackgroundDecommitTask::setChunksToScan(ChunkVector &chunks)
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime()));
    MOZ_ASSERT(!isRunning());
    MOZ_ASSERT(toDecommit.ref().empty());
    Swap(toDecommit.ref(), chunks);
}

void
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
        FreeChunkPool(toFree);
    }
}

void
GCRuntime::sweepBackgroundThings(ZoneList& zones, LifoAlloc& freeBlocks)
{
    freeBlocks.freeAll();

    if (zones.isEmpty())
        return;

    FreeOp fop(nullptr);

    // Sweep zones in order. The atoms zone must be finalized last as other
    // zones may have direct pointers into it.
    while (!zones.isEmpty()) {
        Zone* zone = zones.removeFront();
        Arena* emptyArenas = nullptr;

        // We must finalize thing kinds in the order specified by
        // BackgroundFinalizePhases.
        for (auto phase : BackgroundFinalizePhases) {
            for (auto kind : phase.kinds) {
                Arena* arenas = zone->arenas.arenaListsToSweep(kind);
                MOZ_RELEASE_ASSERT(uintptr_t(arenas) != uintptr_t(-1));
                if (arenas)
                    ArenaLists::backgroundFinalize(&fop, arenas, &emptyArenas);
            }
        }

        AutoLockGC lock(rt);

        // Release any arenas that are now empty, dropping and reaquiring the GC
        // lock every so often to avoid blocking the main thread from
        // allocating chunks.
        static const size_t LockReleasePeriod = 32;
        size_t releaseCount = 0;
        Arena* next;
        for (Arena* arena = emptyArenas; arena; arena = next) {
            next = arena->next;

            // We already calculated the zone's GC trigger after foreground
            // sweeping finished. Now we must update this value.
            arena->zone->threshold.updateForRemovedArena(tunables);

            rt->gc.releaseArena(arena, lock);
            releaseCount++;
            if (releaseCount % LockReleasePeriod == 0) {
                lock.unlock();
                lock.lock();
            }
        }
    }
}

void
GCRuntime::assertBackgroundSweepingFinished()
{
#ifdef DEBUG
    {
        AutoLockHelperThreadState lock;
        MOZ_ASSERT(backgroundSweepZones.ref().isEmpty());
        MOZ_ASSERT(blocksToFreeAfterSweeping.ref().computedSizeOfExcludingThis() == 0);
    }

    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        for (auto i : AllAllocKinds()) {
            MOZ_ASSERT(!zone->arenas.arenaListsToSweep(i));
            MOZ_ASSERT(zone->arenas.doneBackgroundFinalize(i));
        }
    }
#endif
}

void
GCRuntime::queueZonesForBackgroundSweep(ZoneList& zones)
{
    AutoLockHelperThreadState lock;
    backgroundSweepZones.ref().transferFrom(zones);
    if (sweepOnBackgroundThread)
        sweepTask.startIfIdle(lock);
}

void
GCRuntime::freeUnusedLifoBlocksAfterSweeping(LifoAlloc* lifo)
{
    MOZ_ASSERT(JS::RuntimeHeapIsBusy());
    AutoLockHelperThreadState lock;
    blocksToFreeAfterSweeping.ref().transferUnusedFrom(lifo);
}

void
GCRuntime::freeAllLifoBlocksAfterSweeping(LifoAlloc* lifo)
{
    MOZ_ASSERT(JS::RuntimeHeapIsBusy());
    AutoLockHelperThreadState lock;
    blocksToFreeAfterSweeping.ref().transferFrom(lifo);
}

void
GCRuntime::freeAllLifoBlocksAfterMinorGC(LifoAlloc* lifo)
{
    blocksToFreeAfterMinorGC.ref().transferFrom(lifo);
}

BackgroundSweepTask::BackgroundSweepTask(JSRuntime* rt)
  : GCParallelTaskHelper(rt),
    done(false)
{}

bool
BackgroundSweepTask::isRunning() const
{
    AutoLockHelperThreadState lock;
    return isRunningWithLockHeld(lock);
}

bool
BackgroundSweepTask::isRunningWithLockHeld(const AutoLockHelperThreadState& lock) const
{
    return Base::isRunningWithLockHeld(lock) && !done;
}

void
BackgroundSweepTask::startIfIdle(AutoLockHelperThreadState& lock)
{
    MOZ_ASSERT(CanUseExtraThreads());

    if (isRunningWithLockHeld(lock))
        return;

    // Join the previous invocation of the task. This will return immediately
    // if the thread has never been started.
    joinWithLockHeld(lock);

    done = false;

    if (!startWithLockHeld(lock)) {
        AutoUnlockHelperThreadState unlock(lock);
        runFromMainThread(runtime());
    }
}

void
BackgroundSweepTask::runFromMainThread(JSRuntime* rt)
{
    {
        AutoLockHelperThreadState lock;
        MOZ_ASSERT(!isRunningWithLockHeld(lock));
        joinWithLockHeld(lock);
        done = false;
    }

    Base::runFromMainThread(rt);
}

void
BackgroundSweepTask::run()
{
    AutoTraceLog logSweeping(TraceLoggerForCurrentThread(), TraceLogger_GCSweeping);

    AutoLockHelperThreadState lock;
    AutoSetThreadIsSweeping threadIsSweeping;

    MOZ_ASSERT(!done);

    JSRuntime* rt = runtime();

    // The main thread may call queueZonesForBackgroundSweep() while this is
    // running so we must check there is no more work after releasing the lock.
    do {
        ZoneList zones;
        zones.transferFrom(rt->gc.backgroundSweepZones.ref());
        LifoAlloc freeLifoAlloc(JSContext::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
        freeLifoAlloc.transferFrom(&rt->gc.blocksToFreeAfterSweeping.ref());

        AutoUnlockHelperThreadState unlock(lock);
        rt->gc.sweepBackgroundThings(zones, freeLifoAlloc);
    } while (!rt->gc.backgroundSweepZones.ref().isEmpty());

    // Signal to the main thread that we're finished, because we release the
    // lock again before GCParallelTask's state is changed to finished.
    done = true;
}

void
GCRuntime::waitBackgroundSweepEnd()
{
    sweepTask.join();

    // TODO: Improve assertion to work in incremental GC?
    if (!rt->gc.isIncrementalGCInProgress())
        rt->gc.assertBackgroundSweepingFinished();
}

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
JS::Zone::sweepUniqueIds()
{
    uniqueIds().sweep();
}

void
Realm::destroy(FreeOp* fop)
{
    JSRuntime* rt = fop->runtime();
    if (auto callback = rt->destroyRealmCallback)
        callback(fop, this);
    if (principals())
        JS_DropPrincipals(rt->mainContextFromOwnThread(), principals());
    fop->delete_(this);
}

void
Compartment::destroy(FreeOp* fop)
{
    JSRuntime* rt = fop->runtime();
    if (auto callback = rt->destroyCompartmentCallback)
        callback(fop, this);
    fop->delete_(this);
    rt->gc.stats().sweptCompartment();
}

void
Zone::destroy(FreeOp* fop)
{
    MOZ_ASSERT(compartments().empty());
    fop->delete_(this);
    fop->runtime()->gc.stats().sweptZone();
}

/*
 * It's simpler if we preserve the invariant that every zone (except the atoms
 * zone) has at least one compartment, and every compartment has at least one
 * realm. If we know we're deleting the entire zone, then sweepCompartments is
 * allowed to delete all compartments. In this case, |keepAtleastOne| is false.
 * If any cells remain alive in the zone, set |keepAtleastOne| true to prohibit
 * sweepCompartments from deleting every compartment. Instead, it preserves an
 * arbitrary compartment in the zone.
 */
void
Zone::sweepCompartments(FreeOp* fop, bool keepAtleastOne, bool destroyingRuntime)
{
    MOZ_ASSERT(!compartments().empty());
    MOZ_ASSERT_IF(destroyingRuntime, !keepAtleastOne);

    Compartment** read = compartments().begin();
    Compartment** end = compartments().end();
    Compartment** write = read;
    while (read < end) {
        Compartment* comp = *read++;

        /*
         * Don't delete the last compartment and realm if keepAtleastOne is
         * still true, meaning all the other compartments were deleted.
         */
        bool keepAtleastOneRealm = read == end && keepAtleastOne;
        comp->sweepRealms(fop, keepAtleastOneRealm, destroyingRuntime);

        if (!comp->realms().empty()) {
            *write++ = comp;
            keepAtleastOne = false;
        } else {
            comp->destroy(fop);
        }
    }
    compartments().shrinkTo(write - compartments().begin());
    MOZ_ASSERT_IF(keepAtleastOne, !compartments().empty());
    MOZ_ASSERT_IF(destroyingRuntime, compartments().empty());
}

void
Compartment::sweepRealms(FreeOp* fop, bool keepAtleastOne, bool destroyingRuntime)
{
    MOZ_ASSERT(!realms().empty());
    MOZ_ASSERT_IF(destroyingRuntime, !keepAtleastOne);

    Realm** read = realms().begin();
    Realm** end = realms().end();
    Realm** write = read;
    while (read < end) {
        Realm* realm = *read++;

        /*
         * Don't delete the last realm if keepAtleastOne is still true, meaning
         * all the other realms were deleted.
         */
        bool dontDelete = read == end && keepAtleastOne;
        if ((realm->marked() || dontDelete) && !destroyingRuntime) {
            *write++ = realm;
            keepAtleastOne = false;
        } else {
            realm->destroy(fop);
        }
    }
    realms().shrinkTo(write - realms().begin());
    MOZ_ASSERT_IF(keepAtleastOne, !realms().empty());
    MOZ_ASSERT_IF(destroyingRuntime, realms().empty());
}

void
GCRuntime::deleteEmptyZone(Zone* zone)
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
    MOZ_ASSERT(zone->compartments().empty());
    for (auto& i : zones()) {
        if (i == zone) {
            zones().erase(&i);
            zone->destroy(rt->defaultFreeOp());
            return;
        }
    }
    MOZ_CRASH("Zone not found");
}

void
GCRuntime::sweepZones(FreeOp* fop, bool destroyingRuntime)
{
    MOZ_ASSERT_IF(destroyingRuntime, numActiveZoneIters == 0);
    MOZ_ASSERT_IF(destroyingRuntime, arenasEmptyAtShutdown);

    if (rt->gc.numActiveZoneIters)
        return;

    assertBackgroundSweepingFinished();

    Zone** read = zones().begin();
    Zone** end = zones().end();
    Zone** write = read;

    while (read < end) {
        Zone* zone = *read++;

        if (zone->wasGCStarted()) {
            MOZ_ASSERT(!zone->isQueuedForBackgroundSweep());
            const bool zoneIsDead = zone->arenas.arenaListsAreEmpty() &&
                                    !zone->hasMarkedRealms();
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

                zone->sweepCompartments(fop, false, destroyingRuntime);
                MOZ_ASSERT(zone->compartments().empty());
                MOZ_ASSERT_IF(arenasEmptyAtShutdown, zone->typeDescrObjects().empty());
                zone->destroy(fop);
                continue;
            }
            zone->sweepCompartments(fop, true, destroyingRuntime);
        }
        *write++ = zone;
    }
    zones().shrinkTo(write - zones().begin());
}

#ifdef DEBUG
static const char*
AllocKindToAscii(AllocKind kind)
{
    switch(kind) {
#define MAKE_CASE(allocKind, traceKind, type, sizedType, bgFinal, nursery) \
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
    bool isEmpty = true;
#ifdef DEBUG
    size_t numLive = 0;
    if (!arenaLists(kind).isEmpty()) {
        isEmpty = false;
        size_t maxCells = 20;
        char *env = getenv("JS_GC_MAX_LIVE_CELLS");
        if (env && *env)
            maxCells = atol(env);
        for (Arena* current = arenaLists(kind).head(); current; current = current->next) {
            for (ArenaCellIterUnderGC i(current); !i.done(); i.next()) {
                TenuredCell* t = i.getCell();
                MOZ_ASSERT(t->isMarkedAny(), "unmarked cells should have been finalized");
                if (++numLive <= maxCells) {
                    fprintf(stderr, "ERROR: GC found live Cell %p of kind %s at shutdown\n",
                            t, AllocKindToAscii(kind));
                }
            }
        }
        if (numLive > 0) {
          fprintf(stderr, "ERROR: GC found %zu live Cells at shutdown\n", numLive);
        } else {
          fprintf(stderr, "ERROR: GC found empty Arenas at shutdown\n");
        }
    }
#endif // DEBUG
    return isEmpty;
}

class MOZ_RAII js::gc::AutoRunParallelTask : public GCParallelTask
{
    gcstats::PhaseKind phase_;
    AutoLockHelperThreadState& lock_;

  public:
    AutoRunParallelTask(JSRuntime* rt, TaskFunc func, gcstats::PhaseKind phase,
                        AutoLockHelperThreadState& lock)
      : GCParallelTask(rt, func),
        phase_(phase),
        lock_(lock)
    {
        runtime()->gc.startTask(*this, phase_, lock_);
    }

    ~AutoRunParallelTask() {
        runtime()->gc.joinTask(*this, phase_, lock_);
    }
};

void
GCRuntime::purgeRuntimeForMinorGC()
{
    // If external strings become nursery allocable, remember to call
    // zone->externalStringCache().purge() (and delete this assert.)
    MOZ_ASSERT(!IsNurseryAllocable(AllocKind::EXTERNAL_STRING));

    for (ZonesIter zone(rt, SkipAtoms); !zone.done(); zone.next())
        zone->functionToStringCache().purge();

    rt->caches().purgeForMinorGC(rt);
}

void
GCRuntime::purgeRuntime()
{
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::PURGE);

    for (GCRealmsIter realm(rt); !realm.done(); realm.next())
        realm->purge();

    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        zone->purgeAtomCacheOrDefer();
        zone->externalStringCache().purge();
        zone->functionToStringCache().purge();
    }

    JSContext* cx = rt->mainContextFromOwnThread();
    freeUnusedLifoBlocksAfterSweeping(&cx->tempLifoAlloc());
    cx->interpreterStack().purge(rt);
    cx->frontendCollectionPool().purge();

    rt->caches().purge();

    if (auto cache = rt->maybeThisRuntimeSharedImmutableStrings())
        cache->purge();

    MOZ_ASSERT(unmarkGrayStack.empty());
    unmarkGrayStack.clearAndFree();
}

bool
GCRuntime::shouldPreserveJITCode(Realm* realm, const mozilla::TimeStamp &currentTime,
                                 JS::gcreason::Reason reason, bool canAllocateMoreCode)
{
    static const auto oneSecond = mozilla::TimeDuration::FromSeconds(1);

    if (cleanUpEverything)
        return false;
    if (!canAllocateMoreCode)
        return false;

    if (alwaysPreserveCode)
        return true;
    if (realm->preserveJitCode())
        return true;

    const auto &lastAnimationTime = realm->lastAnimationTime.ref();
    if (!lastAnimationTime.IsNull() && lastAnimationTime + oneSecond >= currentTime)
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
    Compartment* compartment;
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
    Compartment* srccomp = src->compartment();

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
    for (Compartment::WrapperEnum e(srccomp); !e.empty(); e.popFront()) {
        if (e.front().mutableKey().applyToWrapped(IsDestComparatorFunctor(dst)) &&
            ToMarkable(e.front().value().unbarrieredGet()) == src)
        {
            return true;
        }
    }

    return false;
}

struct MaybeCompartmentFunctor {
    template <typename T> JS::Compartment* operator()(T* t) { return t->maybeCompartment(); }
};

void
CompartmentCheckTracer::onChild(const JS::GCCellPtr& thing)
{
    Compartment* comp = DispatchTyped(MaybeCompartmentFunctor(), thing);
    if (comp && compartment) {
        MOZ_ASSERT(comp == compartment ||
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
    JSContext* cx = rt->mainContextFromOwnThread();
    if (cx->disableStrictProxyCheckingCount)
        return;

    CompartmentCheckTracer trc(rt);
    AutoAssertEmptyNursery empty(cx);
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

    JSRuntime* rt = zone->runtimeFromMainThread();
    AutoAssertEmptyNursery empty(rt->mainContextFromOwnThread());

    for (auto i = zone->cellIter<JSObject>(kind, empty); !i.done(); i.next()) {
        JSFunction* fun = &i->as<JSFunction>();
        if (fun->hasScript())
            fun->maybeRelazify(rt);
    }
}

static bool
ShouldCollectZone(Zone* zone, JS::gcreason::Reason reason)
{
    // If we are repeating a GC because we noticed dead compartments haven't
    // been collected, then only collect zones containing those compartments.
    if (reason == JS::gcreason::COMPARTMENT_REVIVED) {
        for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next()) {
            if (comp->gcState.scheduledForDestruction)
                return true;
        }

        return false;
    }

    // Otherwise we only collect scheduled zones.
    if (!zone->isGCScheduled())
        return false;

    // If canCollectAtoms() is false then either an instance of AutoKeepAtoms is
    // currently on the stack or parsing is currently happening on another
    // thread. In either case we don't have information about which atoms are
    // roots, so we must skip collecting atoms.
    //
    // Note that only affects the first slice of an incremental GC since root
    // marking is completed before we return to the mutator.
    //
    // Off-thread parsing is inhibited after the start of GC which prevents
    // races between creating atoms during parsing and sweeping atoms on the
    // main thread.
    //
    // Otherwise, we always schedule a GC in the atoms zone so that atoms which
    // the other collected zones are using are marked, and we can update the
    // set of atoms in use by the other collected zones at the end of the GC.
    if (zone->isAtomsZone())
        return TlsContext.get()->canCollectAtoms();

    return zone->canCollect();
}

bool
GCRuntime::prepareZonesForCollection(JS::gcreason::Reason reason, bool* isFullOut)
{
#ifdef DEBUG
    /* Assert that zone state is as we expect */
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        MOZ_ASSERT(!zone->isCollecting());
        MOZ_ASSERT_IF(!zone->isAtomsZone(), !zone->compartments().empty());
        for (auto i : AllAllocKinds())
            MOZ_ASSERT(!zone->arenas.arenaListsToSweep(i));
    }
#endif

    *isFullOut = true;
    bool any = false;

    auto currentTime = mozilla::TimeStamp::Now();

    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        /* Set up which zones will be collected. */
        if (ShouldCollectZone(zone, reason)) {
            MOZ_ASSERT(zone->canCollect());
            any = true;
            zone->changeGCState(Zone::NoGC, Zone::Mark);
        } else {
            *isFullOut = false;
        }

        zone->setPreservingCode(false);
    }

    // Discard JIT code more aggressively if the process is approaching its
    // executable code limit.
    bool canAllocateMoreCode = jit::CanLikelyAllocateMoreExecutableMemory();

    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        c->gcState.scheduledForDestruction = false;
        c->gcState.maybeAlive = false;
        c->gcState.hasEnteredRealm = false;
        for (RealmsInCompartmentIter r(c); !r.done(); r.next()) {
            r->unmark();
            if (r->shouldTraceGlobal() || !r->zone()->isGCScheduled())
                c->gcState.maybeAlive = true;
            if (shouldPreserveJITCode(r, currentTime, reason, canAllocateMoreCode))
                r->zone()->setPreservingCode(true);
            if (r->hasBeenEnteredIgnoringJit())
                c->gcState.hasEnteredRealm = true;
        }
    }

    if (!cleanUpEverything && canAllocateMoreCode) {
        jit::JitActivationIterator activation(rt->mainContextFromOwnThread());
        if (!activation.done())
            activation->compartment()->zone()->setPreservingCode(true);
    }

    /*
     * Check that we do collect the atoms zone if we triggered a GC for that
     * purpose.
     */
    MOZ_ASSERT_IF(reason == JS::gcreason::DELAYED_ATOMS_GC, atomsZone->isGCMarking());

    /* Check that at least one zone is scheduled for collection. */
    return any;
}

static void
DiscardJITCodeForGC(JSRuntime* rt)
{
    js::CancelOffThreadIonCompile(rt, JS::Zone::Mark);
    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        gcstats::AutoPhase ap(rt->gc.stats(), gcstats::PhaseKind::MARK_DISCARD_CODE);
        zone->discardJitCode(rt->defaultFreeOp());
    }
}

static void
RelazifyFunctionsForShrinkingGC(JSRuntime* rt)
{
    gcstats::AutoPhase ap(rt->gc.stats(), gcstats::PhaseKind::RELAZIFY_FUNCTIONS);
    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        if (zone->isSelfHostingZone())
            continue;
        RelazifyFunctions(zone, AllocKind::FUNCTION);
        RelazifyFunctions(zone, AllocKind::FUNCTION_EXTENDED);
    }
}

static void
PurgeShapeTablesForShrinkingGC(JSRuntime* rt)
{
    gcstats::AutoPhase ap(rt->gc.stats(), gcstats::PhaseKind::PURGE_SHAPE_TABLES);
    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        if (!CanRelocateZone(zone) || zone->keepShapeTables())
            continue;
        for (auto baseShape = zone->cellIter<BaseShape>(); !baseShape.done(); baseShape.next())
            baseShape->maybePurgeTable();
    }
}

static void
UnmarkCollectedZones(GCParallelTask* task)
{
    JSRuntime* rt = task->runtime();
    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        /* Unmark everything in the zones being collected. */
        zone->arenas.unmarkAll();
    }

    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        /* Unmark all weak maps in the zones being collected. */
        WeakMapBase::unmarkZone(zone);
    }
}

static void
BufferGrayRoots(GCParallelTask* task)
{
    task->runtime()->gc.bufferGrayRoots();
}

bool
GCRuntime::beginMarkPhase(JS::gcreason::Reason reason, AutoGCSession& session)
{
#ifdef DEBUG
    if (fullCompartmentChecks)
        checkForCompartmentMismatches();
#endif

    if (!prepareZonesForCollection(reason, &isFull.ref()))
        return false;

    /* * Check it's safe to access the atoms zone if we are collecting it. */
    if (atomsZone->isCollecting())
        session.maybeCheckAtomsAccess.emplace(rt);

    /*
     * In an incremental GC, clear the area free lists to ensure that subsequent
     * allocations refill them and end up marking new cells back. See
     * arenaAllocatedDuringGC().
     */
    for (GCZonesIter zone(rt); !zone.done(); zone.next())
        zone->arenas.clearFreeLists();

    marker.start();
    GCMarker* gcmarker = &marker;

    {
        gcstats::AutoPhase ap1(stats(), gcstats::PhaseKind::PREPARE);
        AutoLockHelperThreadState helperLock;

        /*
         * Clear all mark state for the zones we are collecting. This is linear
         * in the size of the heap we are collecting and so can be slow. Do this
         * in parallel with the rest of this block.
         */
        AutoRunParallelTask
            unmarkCollectedZones(rt, UnmarkCollectedZones, gcstats::PhaseKind::UNMARK, helperLock);

        /*
         * Buffer gray roots for incremental collections. This is linear in the
         * number of roots which can be in the tens of thousands. Do this in
         * parallel with the rest of this block.
         */
        Maybe<AutoRunParallelTask> bufferGrayRoots;
        if (isIncremental)
            bufferGrayRoots.emplace(rt, BufferGrayRoots, gcstats::PhaseKind::BUFFER_GRAY_ROOTS, helperLock);
        AutoUnlockHelperThreadState unlock(helperLock);

        // Discard JIT code. For incremental collections, the sweep phase will
        // also discard JIT code.
        DiscardJITCodeForGC(rt);

        /*
         * Relazify functions after discarding JIT code (we can't relazify
         * functions with JIT code) and before the actual mark phase, so that
         * the current GC can collect the JSScripts we're unlinking here.  We do
         * this only when we're performing a shrinking GC, as too much
         * relazification can cause performance issues when we have to reparse
         * the same functions over and over.
         */
        if (invocationKind == GC_SHRINK) {
            RelazifyFunctionsForShrinkingGC(rt);
            PurgeShapeTablesForShrinkingGC(rt);
        }

        /*
         * We must purge the runtime at the beginning of an incremental GC. The
         * danger if we purge later is that the snapshot invariant of
         * incremental GC will be broken, as follows. If some object is
         * reachable only through some cache (say the dtoaCache) then it will
         * not be part of the snapshot.  If we purge after root marking, then
         * the mutator could obtain a pointer to the object and start using
         * it. This object might never be marked, so a GC hazard would exist.
         */
        purgeRuntime();
    }

    /*
     * Mark phase.
     */
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::MARK);
    traceRuntimeForMajorGC(gcmarker, session);

    if (isIncremental)
        markCompartments();

    updateMallocCountersOnGCStart();

    /*
     * Process any queued source compressions during the start of a major
     * GC.
     */
    {
        AutoLockHelperThreadState helperLock;
        HelperThreadState().startHandlingCompressionTasks(helperLock);
    }

    return true;
}

void
GCRuntime::markCompartments()
{
    gcstats::AutoPhase ap1(stats(), gcstats::PhaseKind::MARK_ROOTS);
    gcstats::AutoPhase ap2(stats(), gcstats::PhaseKind::MARK_COMPARTMENTS);

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

    Vector<Compartment*, 0, js::SystemAllocPolicy> workList;

    for (CompartmentsIter comp(rt); !comp.done(); comp.next()) {
        if (comp->gcState.maybeAlive) {
            if (!workList.append(comp))
                return;
        }
    }

    while (!workList.empty()) {
        Compartment* comp = workList.popCopy();
        for (Compartment::NonStringWrapperEnum e(comp); !e.empty(); e.popFront()) {
            Compartment* dest = e.front().mutableKey().compartment();
            if (dest && !dest->gcState.maybeAlive) {
                dest->gcState.maybeAlive = true;
                if (!workList.append(dest))
                    return;
            }
        }
    }

    /* Set scheduleForDestruction based on maybeAlive. */

    for (GCCompartmentsIter comp(rt); !comp.done(); comp.next()) {
        MOZ_ASSERT(!comp->gcState.scheduledForDestruction);
        if (!comp->gcState.maybeAlive)
            comp->gcState.scheduledForDestruction = true;
    }
}

void
GCRuntime::updateMallocCountersOnGCStart()
{
    // Update the malloc counters for any zones we are collecting.
    for (GCZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
        zone->updateAllGCMallocCountersOnGCStart();

    // Update the runtime malloc counter only if we are doing a full GC.
    if (isFull)
        mallocCounter.updateOnGCStart();
}

template <class ZoneIterT>
void
GCRuntime::markWeakReferences(gcstats::PhaseKind phase)
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
GCRuntime::markWeakReferencesInCurrentGroup(gcstats::PhaseKind phase)
{
    markWeakReferences<SweepGroupZonesIter>(phase);
}

template <class ZoneIterT, class CompartmentIterT>
void
GCRuntime::markGrayReferences(gcstats::PhaseKind phase)
{
    gcstats::AutoPhase ap(stats(), phase);
    if (hasValidGrayRootsBuffer()) {
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
GCRuntime::markGrayReferencesInCurrentGroup(gcstats::PhaseKind phase)
{
    markGrayReferences<SweepGroupZonesIter, SweepGroupCompartmentsIter>(phase);
}

void
GCRuntime::markAllWeakReferences(gcstats::PhaseKind phase)
{
    markWeakReferences<GCZonesIter>(phase);
}

void
GCRuntime::markAllGrayReferences(gcstats::PhaseKind phase)
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
    void nonIncrementalMark(AutoGCSession& session);
    void validate();

  private:
    GCRuntime* gc;
    bool initialized;

    using BitmapMap = HashMap<Chunk*, UniquePtr<ChunkBitmap>, GCChunkHasher, SystemAllocPolicy>;
    BitmapMap map;
};

js::gc::MarkingValidator::MarkingValidator(GCRuntime* gc)
  : gc(gc),
    initialized(false)
{}

void
js::gc::MarkingValidator::nonIncrementalMark(AutoGCSession& session)
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

    /* Wait for off-thread parsing which can allocate. */
    HelperThreadState().waitForAllThreads();

    /* Save existing mark bits. */
    {
        AutoLockGC lock(runtime);
        for (auto chunk = gc->allNonEmptyChunks(lock); !chunk.done(); chunk.next()) {
            ChunkBitmap* bitmap = &chunk->bitmap;
            auto entry = MakeUnique<ChunkBitmap>();
            if (!entry)
                return;

            memcpy((void*)entry->bitmap, (void*)bitmap->bitmap, sizeof(bitmap->bitmap));

            if (!map.putNew(chunk, std::move(entry)))
                return;
        }
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
            if (!savedWeakKeys.put(std::move(r.front().key), std::move(r.front().value)))
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
        gcstats::AutoPhase ap(gc->stats(), gcstats::PhaseKind::PREPARE);

        {
            gcstats::AutoPhase ap(gc->stats(), gcstats::PhaseKind::UNMARK);

            for (GCZonesIter zone(runtime); !zone.done(); zone.next())
                WeakMapBase::unmarkZone(zone);

            MOZ_ASSERT(gcmarker->isDrained());
            gcmarker->reset();

            AutoLockGC lock(runtime);
            for (auto chunk = gc->allNonEmptyChunks(lock); !chunk.done(); chunk.next())
                chunk->bitmap.clear();
        }
    }

    {
        gcstats::AutoPhase ap(gc->stats(), gcstats::PhaseKind::MARK);

        gc->traceRuntimeForMajorGC(gcmarker, session);

        gc->incrementalState = State::Mark;
        auto unlimited = SliceBudget::unlimited();
        MOZ_RELEASE_ASSERT(gc->marker.drainMarkStack(unlimited));
    }

    gc->incrementalState = State::Sweep;
    {
        gcstats::AutoPhase ap1(gc->stats(), gcstats::PhaseKind::SWEEP);
        gcstats::AutoPhase ap2(gc->stats(), gcstats::PhaseKind::SWEEP_MARK);

        gc->markAllWeakReferences(gcstats::PhaseKind::SWEEP_MARK_WEAK);

        /* Update zone state for gray marking. */
        for (GCZonesIter zone(runtime); !zone.done(); zone.next())
            zone->changeGCState(Zone::Mark, Zone::MarkGray);
        gc->marker.setMarkColorGray();

        gc->markAllGrayReferences(gcstats::PhaseKind::SWEEP_MARK_GRAY);
        gc->markAllWeakReferences(gcstats::PhaseKind::SWEEP_MARK_GRAY_WEAK);

        /* Restore zone state. */
        for (GCZonesIter zone(runtime); !zone.done(); zone.next())
            zone->changeGCState(Zone::MarkGray, Zone::Mark);
        MOZ_ASSERT(gc->marker.isDrained());
        gc->marker.setMarkColorBlack();
    }

    /* Take a copy of the non-incremental mark state and restore the original. */
    {
        AutoLockGC lock(runtime);
        for (auto chunk = gc->allNonEmptyChunks(lock); !chunk.done(); chunk.next()) {
            ChunkBitmap* bitmap = &chunk->bitmap;
            ChunkBitmap* entry = map.lookup(chunk)->value().get();
            Swap(*entry, *bitmap);
        }
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
        if (!zone->gcWeakKeys().put(std::move(r.front().key), std::move(r.front().value)))
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

    AutoLockGC lock(gc->rt);
    for (auto chunk = gc->allNonEmptyChunks(lock); !chunk.done(); chunk.next()) {
        BitmapMap::Ptr ptr = map.lookup(chunk);
        if (!ptr)
            continue;  /* Allocated after we did the non-incremental mark. */

        ChunkBitmap* bitmap = ptr->value().get();
        ChunkBitmap* incBitmap = &chunk->bitmap;

        for (size_t i = 0; i < ArenasPerChunk; i++) {
            if (chunk->decommittedArenas.get(i))
                continue;
            Arena* arena = &chunk->arenas[i];
            if (!arena->allocated())
                continue;
            if (!arena->zone->isGCSweeping())
                continue;

            AllocKind kind = arena->getAllocKind();
            uintptr_t thing = arena->thingsStart();
            uintptr_t end = arena->thingsEnd();
            while (thing < end) {
                auto cell = reinterpret_cast<TenuredCell*>(thing);

                /*
                 * If a non-incremental GC wouldn't have collected a cell, then
                 * an incremental GC won't collect it.
                 */
                if (bitmap->isMarkedAny(cell))
                    MOZ_RELEASE_ASSERT(incBitmap->isMarkedAny(cell));

                /*
                 * If the cycle collector isn't allowed to collect an object
                 * after a non-incremental GC has run, then it isn't allowed to
                 * collected it after an incremental GC.
                 */
                if (!bitmap->isMarkedGray(cell))
                    MOZ_RELEASE_ASSERT(!incBitmap->isMarkedGray(cell));

                thing += Arena::thingSize(kind);
            }
        }
    }
}

#endif // JS_GC_ZEAL

void
GCRuntime::computeNonIncrementalMarkingForValidation(AutoGCSession& session)
{
#ifdef JS_GC_ZEAL
    MOZ_ASSERT(!markingValidator);
    if (isIncremental && hasZealMode(ZealMode::IncrementalMarkingValidator))
        markingValidator = js_new<MarkingValidator>(this);
    if (markingValidator)
        markingValidator->nonIncrementalMark(session);
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
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        for (Compartment::StringWrapperEnum e(c); !e.empty(); e.popFront()) {
            MOZ_ASSERT(e.front().key().is<JSString*>());
            e.removeFront();
        }
    }
}

/*
 * Group zones that must be swept at the same time.
 *
 * If compartment A has an edge to an unmarked object in compartment B, then we
 * must not sweep A in a later slice than we sweep B. That's because a write
 * barrier in A could lead to the unmarked object in B becoming marked.
 * However, if we had already swept that object, we would be in trouble.
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
Compartment::findOutgoingEdges(ZoneComponentFinder& finder)
{
    for (js::WrapperMap::Enum e(crossCompartmentWrappers); !e.empty(); e.popFront()) {
        CrossCompartmentKey& key = e.front().mutableKey();
        MOZ_ASSERT(!key.is<JSString*>());
        bool needsEdge = true;
        if (key.is<JSObject*>()) {
            TenuredCell& other = key.as<JSObject*>()->asTenured();
            needsEdge = !other.isMarkedBlack();
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
    if (Zone* zone = finder.maybeAtomsZone) {
        MOZ_ASSERT(zone->isCollecting());
        finder.addEdgeTo(zone);
    }

    for (CompartmentsInZoneIter comp(this); !comp.done(); comp.next())
        comp->findOutgoingEdges(finder);

    for (ZoneSet::Range r = gcSweepGroupEdges().all(); !r.empty(); r.popFront()) {
        if (r.front()->isGCMarking())
            finder.addEdgeTo(r.front());
    }

    Debugger::findZoneEdges(this, finder);
}

bool
GCRuntime::findInterZoneEdges()
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
GCRuntime::groupZonesForSweeping(JS::gcreason::Reason reason)
{
#ifdef DEBUG
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
        MOZ_ASSERT(zone->gcSweepGroupEdges().empty());
#endif

    JSContext* cx = rt->mainContextFromOwnThread();
    Zone* maybeAtomsZone = atomsZone->wasGCStarted() ? atomsZone.ref() : nullptr;
    ZoneComponentFinder finder(cx->nativeStackLimit[JS::StackForSystemCode], maybeAtomsZone);
    if (!isIncremental || !findInterZoneEdges())
        finder.useOneComponent();

#ifdef JS_GC_ZEAL
    // Use one component for two-slice zeal modes.
    if (useZeal && hasIncrementalTwoSliceZealMode())
        finder.useOneComponent();
#endif

    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        MOZ_ASSERT(zone->isGCMarking());
        finder.addNode(zone);
    }
    sweepGroups = finder.getResultsList();
    currentSweepGroup = sweepGroups;
    sweepGroupIndex = 0;

    for (GCZonesIter zone(rt); !zone.done(); zone.next())
        zone->gcSweepGroupEdges().clear();

#ifdef DEBUG
    for (Zone* head = currentSweepGroup; head; head = head->nextGroup()) {
        for (Zone* zone = head; zone; zone = zone->nextNodeInGroup())
            MOZ_ASSERT(zone->isGCMarking());
    }

    MOZ_ASSERT_IF(!isIncremental, !currentSweepGroup->nextGroup());
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
        MOZ_ASSERT(zone->gcSweepGroupEdges().empty());
#endif
}

static void
ResetGrayList(Compartment* comp);

void
GCRuntime::getNextSweepGroup()
{
    currentSweepGroup = currentSweepGroup->nextGroup();
    ++sweepGroupIndex;
    if (!currentSweepGroup) {
        abortSweepAfterCurrentGroup = false;
        return;
    }

    for (Zone* zone = currentSweepGroup; zone; zone = zone->nextNodeInGroup()) {
        MOZ_ASSERT(zone->isGCMarking());
        MOZ_ASSERT(!zone->isQueuedForBackgroundSweep());
    }

    if (!isIncremental)
        ZoneComponentFinder::mergeGroups(currentSweepGroup);

    if (abortSweepAfterCurrentGroup) {
        MOZ_ASSERT(!isIncremental);
        for (SweepGroupZonesIter zone(rt); !zone.done(); zone.next()) {
            MOZ_ASSERT(!zone->gcNextGraphComponent);
            zone->setNeedsIncrementalBarrier(false);
            zone->changeGCState(Zone::Mark, Zone::NoGC);
            zone->gcGrayRoots().clearAndFree();
        }

        for (SweepGroupCompartmentsIter comp(rt); !comp.done(); comp.next())
            ResetGrayList(comp);

        abortSweepAfterCurrentGroup = false;
        currentSweepGroup = nullptr;
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
 * The list head is stored in Compartment::gcIncomingGrayPointers and contains
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
ProxyObject::grayLinkReservedSlot(JSObject* obj)
{
    MOZ_ASSERT(IsGrayListObject(obj));
    return CrossCompartmentWrapperObject::GrayLinkReservedSlot;
}

#ifdef DEBUG
static void
AssertNotOnGrayList(JSObject* obj)
{
    MOZ_ASSERT_IF(IsGrayListObject(obj),
                  GetProxyReservedSlot(obj, ProxyObject::grayLinkReservedSlot(obj)).isUndefined());
}
#endif

static void
AssertNoWrappersInGrayList(JSRuntime* rt)
{
#ifdef DEBUG
    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        MOZ_ASSERT(!c->gcIncomingGrayPointers);
        for (Compartment::NonStringWrapperEnum e(c); !e.empty(); e.popFront())
            AssertNotOnGrayList(&e.front().value().unbarrieredGet().toObject());
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
    unsigned slot = ProxyObject::grayLinkReservedSlot(prev);
    JSObject* next = GetProxyReservedSlot(prev, slot).toObjectOrNull();
    MOZ_ASSERT_IF(next, IsGrayListObject(next));

    if (unlink)
        SetProxyReservedSlot(prev, slot, UndefinedValue());

    return next;
}

void
js::gc::DelayCrossCompartmentGrayMarking(JSObject* src)
{
    MOZ_ASSERT(IsGrayListObject(src));
    MOZ_ASSERT(src->isMarkedGray());

    AutoTouchingGrayThings tgt;

    /* Called from MarkCrossCompartmentXXX functions. */
    unsigned slot = ProxyObject::grayLinkReservedSlot(src);
    JSObject* dest = CrossCompartmentPointerReferent(src);
    Compartment* comp = dest->compartment();

    if (GetProxyReservedSlot(src, slot).isUndefined()) {
        SetProxyReservedSlot(src, slot, ObjectOrNullValue(comp->gcIncomingGrayPointers));
        comp->gcIncomingGrayPointers = src;
    } else {
        MOZ_ASSERT(GetProxyReservedSlot(src, slot).isObjectOrNull());
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
MarkIncomingCrossCompartmentPointers(JSRuntime* rt, MarkColor color)
{
    MOZ_ASSERT(color == MarkColor::Black || color == MarkColor::Gray);

    static const gcstats::PhaseKind statsPhases[] = {
        gcstats::PhaseKind::SWEEP_MARK_INCOMING_BLACK,
        gcstats::PhaseKind::SWEEP_MARK_INCOMING_GRAY
    };
    gcstats::AutoPhase ap1(rt->gc.stats(), statsPhases[unsigned(color)]);

    bool unlinkList = color == MarkColor::Gray;

    for (SweepGroupCompartmentsIter c(rt); !c.done(); c.next()) {
        MOZ_ASSERT_IF(color == MarkColor::Gray, c->zone()->isGCMarkingGray());
        MOZ_ASSERT_IF(color == MarkColor::Black, c->zone()->isGCMarkingBlack());
        MOZ_ASSERT_IF(c->gcIncomingGrayPointers, IsGrayListObject(c->gcIncomingGrayPointers));

        for (JSObject* src = c->gcIncomingGrayPointers;
             src;
             src = NextIncomingCrossCompartmentPointer(src, unlinkList))
        {
            JSObject* dst = CrossCompartmentPointerReferent(src);
            MOZ_ASSERT(dst->compartment() == c);

            if (color == MarkColor::Gray) {
                if (IsMarkedUnbarriered(rt, &src) && src->asTenured().isMarkedGray())
                    TraceManuallyBarrieredEdge(&rt->gc.marker, &dst,
                                               "cross-compartment gray pointer");
            } else {
                if (IsMarkedUnbarriered(rt, &src) && !src->asTenured().isMarkedGray())
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
    AutoTouchingGrayThings tgt;

    if (!IsGrayListObject(wrapper))
        return false;

    unsigned slot = ProxyObject::grayLinkReservedSlot(wrapper);
    if (GetProxyReservedSlot(wrapper, slot).isUndefined())
        return false;  /* Not on our list. */

    JSObject* tail = GetProxyReservedSlot(wrapper, slot).toObjectOrNull();
    SetProxyReservedSlot(wrapper, slot, UndefinedValue());

    Compartment* comp = CrossCompartmentPointerReferent(wrapper)->compartment();
    JSObject* obj = comp->gcIncomingGrayPointers;
    if (obj == wrapper) {
        comp->gcIncomingGrayPointers = tail;
        return true;
    }

    while (obj) {
        unsigned slot = ProxyObject::grayLinkReservedSlot(obj);
        JSObject* next = GetProxyReservedSlot(obj, slot).toObjectOrNull();
        if (next == wrapper) {
            js::detail::SetProxyReservedSlotUnchecked(obj, slot, ObjectOrNullValue(tail));
            return true;
        }
        obj = next;
    }

    MOZ_CRASH("object not found in gray link list");
}

static void
ResetGrayList(Compartment* comp)
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

IncrementalProgress
GCRuntime::endMarkingSweepGroup(FreeOp* fop, SliceBudget& budget)
{
    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_MARK);

    /*
     * Mark any incoming black pointers from previously swept compartments
     * whose referents are not marked. This can occur when gray cells become
     * black by the action of UnmarkGray.
     */
    MarkIncomingCrossCompartmentPointers(rt, MarkColor::Black);
    markWeakReferencesInCurrentGroup(gcstats::PhaseKind::SWEEP_MARK_WEAK);

    /*
     * Change state of current group to MarkGray to restrict marking to this
     * group.  Note that there may be pointers to the atoms zone, and
     * these will be marked through, as they are not marked with
     * TraceCrossCompartmentEdge.
     */
    for (SweepGroupZonesIter zone(rt); !zone.done(); zone.next())
        zone->changeGCState(Zone::Mark, Zone::MarkGray);
    marker.setMarkColorGray();

    /* Mark incoming gray pointers from previously swept compartments. */
    MarkIncomingCrossCompartmentPointers(rt, MarkColor::Gray);

    /* Mark gray roots and mark transitively inside the current compartment group. */
    markGrayReferencesInCurrentGroup(gcstats::PhaseKind::SWEEP_MARK_GRAY);
    markWeakReferencesInCurrentGroup(gcstats::PhaseKind::SWEEP_MARK_GRAY_WEAK);

    /* Restore marking state. */
    for (SweepGroupZonesIter zone(rt); !zone.done(); zone.next())
        zone->changeGCState(Zone::MarkGray, Zone::Mark);
    MOZ_ASSERT(marker.isDrained());
    marker.setMarkColorBlack();

    /* We must not yield after this point before we start sweeping the group. */
    safeToYield = false;

    return Finished;
}

// Causes the given WeakCache to be swept when run.
class ImmediateSweepWeakCacheTask : public GCParallelTaskHelper<ImmediateSweepWeakCacheTask>
{
    JS::detail::WeakCacheBase& cache;

    ImmediateSweepWeakCacheTask(const ImmediateSweepWeakCacheTask&) = delete;

  public:
    ImmediateSweepWeakCacheTask(JSRuntime* rt, JS::detail::WeakCacheBase& wc)
      : GCParallelTaskHelper(rt), cache(wc)
    {}

    ImmediateSweepWeakCacheTask(ImmediateSweepWeakCacheTask&& other)
      : GCParallelTaskHelper(std::move(other)), cache(other.cache)
    {}

    void run() {
        cache.sweep();
    }
};

static void
UpdateAtomsBitmap(JSRuntime* runtime)
{
    DenseBitmap marked;
    if (runtime->gc.atomMarking.computeBitmapFromChunkMarkBits(runtime, marked)) {
        for (GCZonesIter zone(runtime); !zone.done(); zone.next())
            runtime->gc.atomMarking.refineZoneBitmapForCollectedZone(zone, marked);
    } else {
        // Ignore OOM in computeBitmapFromChunkMarkBits. The
        // refineZoneBitmapForCollectedZone call can only remove atoms from the
        // zone bitmap, so it is conservative to just not call it.
    }

    runtime->gc.atomMarking.markAtomsUsedByUncollectedZones(runtime);

    // For convenience sweep these tables non-incrementally as part of bitmap
    // sweeping; they are likely to be much smaller than the main atoms table.
    runtime->symbolRegistry().sweep();
    for (RealmsIter realm(runtime); !realm.done(); realm.next())
        realm->sweepVarNames();
}

static void
SweepCCWrappers(GCParallelTask* task)
{
    JSRuntime* runtime = task->runtime();
    for (SweepGroupCompartmentsIter c(runtime); !c.done(); c.next())
        c->sweepCrossCompartmentWrappers();
}

static void
SweepObjectGroups(GCParallelTask* task)
{
    JSRuntime* runtime = task->runtime();
    for (SweepGroupRealmsIter r(runtime); !r.done(); r.next())
        r->sweepObjectGroups();
}

static void
SweepMisc(GCParallelTask* task)
{
    JSRuntime* runtime = task->runtime();
    for (SweepGroupRealmsIter r(runtime); !r.done(); r.next()) {
        r->sweepGlobalObject();
        r->sweepTemplateObjects();
        r->sweepSavedStacks();
        r->sweepSelfHostingScriptSource();
        r->sweepObjectRealm();
        r->sweepRegExps();
    }
}

static void
SweepCompressionTasks(GCParallelTask* task)
{
    JSRuntime* runtime = task->runtime();

    AutoLockHelperThreadState lock;

    // Attach finished compression tasks.
    auto& finished = HelperThreadState().compressionFinishedList(lock);
    for (size_t i = 0; i < finished.length(); i++) {
        if (finished[i]->runtimeMatches(runtime)) {
            UniquePtr<SourceCompressionTask> compressionTask(std::move(finished[i]));
            HelperThreadState().remove(finished, &i);
            compressionTask->complete();
        }
    }

    // Sweep pending tasks that are holding onto should-be-dead ScriptSources.
    auto& pending = HelperThreadState().compressionPendingList(lock);
    for (size_t i = 0; i < pending.length(); i++) {
        if (pending[i]->shouldCancel())
            HelperThreadState().remove(pending, &i);
    }
}

static void
SweepWeakMaps(GCParallelTask* task)
{
    JSRuntime* runtime = task->runtime();
    for (SweepGroupZonesIter zone(runtime); !zone.done(); zone.next()) {
        /* Clear all weakrefs that point to unmarked things. */
        for (auto edge : zone->gcWeakRefs()) {
            /* Edges may be present multiple times, so may already be nulled. */
            if (*edge && IsAboutToBeFinalizedDuringSweep(**edge))
                *edge = nullptr;
        }
        zone->gcWeakRefs().clear();

        /* No need to look up any more weakmap keys from this sweep group. */
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!zone->gcWeakKeys().clear())
            oomUnsafe.crash("clearing weak keys in beginSweepingSweepGroup()");

        zone->sweepWeakMaps();
    }
}

static void
SweepUniqueIds(GCParallelTask* task)
{
    for (SweepGroupZonesIter zone(task->runtime()); !zone.done(); zone.next())
        zone->sweepUniqueIds();
}

void
GCRuntime::startTask(GCParallelTask& task, gcstats::PhaseKind phase,
                     AutoLockHelperThreadState& locked)
{
    if (!task.startWithLockHeld(locked)) {
        AutoUnlockHelperThreadState unlock(locked);
        gcstats::AutoPhase ap(stats(), phase);
        task.runFromMainThread(rt);
    }
}

void
GCRuntime::joinTask(GCParallelTask& task, gcstats::PhaseKind phase,
                    AutoLockHelperThreadState& locked)
{
    {
        gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::JOIN_PARALLEL_TASKS);
        task.joinWithLockHeld(locked);
    }
    stats().recordParallelPhase(phase, task.duration());
}

void
GCRuntime::sweepDebuggerOnMainThread(FreeOp* fop)
{
    // Detach unreachable debuggers and global objects from each other.
    // This can modify weakmaps and so must happen before weakmap sweeping.
    Debugger::sweepAll(fop);

    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_COMPARTMENTS);

    // Sweep debug environment information. This performs lookups in the Zone's
    // unique IDs table and so must not happen in parallel with sweeping that
    // table.
    {
        gcstats::AutoPhase ap2(stats(), gcstats::PhaseKind::SWEEP_MISC);
        for (SweepGroupRealmsIter r(rt); !r.done(); r.next())
            r->sweepDebugEnvironments();
    }

    // Sweep breakpoints. This is done here to be with the other debug sweeping,
    // although note that it can cause JIT code to be patched.
    {
        gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_BREAKPOINT);
        for (SweepGroupZonesIter zone(rt); !zone.done(); zone.next())
            zone->sweepBreakpoints(fop);
    }
}

void
GCRuntime::sweepJitDataOnMainThread(FreeOp* fop)
{
    {
        gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_JIT_DATA);

        if (initialState != State::NotActive) {
            // Cancel any active or pending off thread compilations. We also did
            // this before marking (in DiscardJITCodeForGC) so this is a no-op
            // for non-incremental GCs.
            js::CancelOffThreadIonCompile(rt, JS::Zone::Sweep);
        }

        for (SweepGroupRealmsIter r(rt); !r.done(); r.next())
            r->sweepJitRealm();

        for (SweepGroupZonesIter zone(rt); !zone.done(); zone.next()) {
            if (jit::JitZone* jitZone = zone->jitZone())
                jitZone->sweep();
        }

        // Bug 1071218: the following method has not yet been refactored to
        // work on a single zone-group at once.

        // Sweep entries containing about-to-be-finalized JitCode and
        // update relocated TypeSet::Types inside the JitcodeGlobalTable.
        jit::JitRuntime::SweepJitcodeGlobalTable(rt);
    }

    if (initialState != State::NotActive) {
        gcstats::AutoPhase apdc(stats(), gcstats::PhaseKind::SWEEP_DISCARD_CODE);
        for (SweepGroupZonesIter zone(rt); !zone.done(); zone.next())
            zone->discardJitCode(fop);
    }

    {
        gcstats::AutoPhase ap1(stats(), gcstats::PhaseKind::SWEEP_TYPES);
        gcstats::AutoPhase ap2(stats(), gcstats::PhaseKind::SWEEP_TYPES_BEGIN);
        for (SweepGroupZonesIter zone(rt); !zone.done(); zone.next())
            zone->beginSweepTypes(releaseObservedTypes && !zone->isPreservingCode());
    }
}

using WeakCacheTaskVector = mozilla::Vector<ImmediateSweepWeakCacheTask, 0, SystemAllocPolicy>;

enum WeakCacheLocation
{
    RuntimeWeakCache,
    ZoneWeakCache
};

// Call a functor for all weak caches that need to be swept in the current
// sweep group.
template <typename Functor>
static inline bool
IterateWeakCaches(JSRuntime* rt, Functor f)
{
    for (SweepGroupZonesIter zone(rt); !zone.done(); zone.next()) {
        for (JS::detail::WeakCacheBase* cache : zone->weakCaches()) {
            if (!f(cache, ZoneWeakCache))
                return false;
        }
    }

    for (JS::detail::WeakCacheBase* cache : rt->weakCaches()) {
        if (!f(cache, RuntimeWeakCache))
            return false;
    }

    return true;
}

static bool
PrepareWeakCacheTasks(JSRuntime* rt, WeakCacheTaskVector* immediateTasks)
{
    // Start incremental sweeping for caches that support it or add to a vector
    // of sweep tasks to run on a helper thread.

    MOZ_ASSERT(immediateTasks->empty());

    bool ok = IterateWeakCaches(rt, [&] (JS::detail::WeakCacheBase* cache,
                                         WeakCacheLocation location)
    {
        if (!cache->needsSweep())
            return true;

        // Caches that support incremental sweeping will be swept later.
        if (location == ZoneWeakCache && cache->setNeedsIncrementalBarrier(true))
            return true;

        return immediateTasks->emplaceBack(rt, *cache);
    });

    if (!ok)
        immediateTasks->clearAndFree();

    return ok;
}

static void
SweepWeakCachesOnMainThread(JSRuntime* rt)
{
    // If we ran out of memory, do all the work on the main thread.
    gcstats::AutoPhase ap(rt->gc.stats(), gcstats::PhaseKind::SWEEP_WEAK_CACHES);
    IterateWeakCaches(rt, [&] (JS::detail::WeakCacheBase* cache, WeakCacheLocation location) {
        if (cache->needsIncrementalBarrier())
            cache->setNeedsIncrementalBarrier(false);
        cache->sweep();
        return true;
    });
}

IncrementalProgress
GCRuntime::beginSweepingSweepGroup(FreeOp* fop, SliceBudget& budget)
{
    /*
     * Begin sweeping the group of zones in currentSweepGroup, performing
     * actions that must be done before yielding to caller.
     */

    using namespace gcstats;

    AutoSCC scc(stats(), sweepGroupIndex);

    bool sweepingAtoms = false;
    for (SweepGroupZonesIter zone(rt); !zone.done(); zone.next()) {
        /* Set the GC state to sweeping. */
        zone->changeGCState(Zone::Mark, Zone::Sweep);

        /* Purge the ArenaLists before sweeping. */
        zone->arenas.unmarkPreMarkedFreeCells();
        zone->arenas.clearFreeLists();

        if (zone->isAtomsZone())
            sweepingAtoms = true;

#ifdef DEBUG
        zone->gcLastSweepGroupIndex = sweepGroupIndex;
#endif
    }

    validateIncrementalMarking();

    {
        AutoPhase ap(stats(), PhaseKind::FINALIZE_START);
        callFinalizeCallbacks(fop, JSFINALIZE_GROUP_PREPARE);
        {
            AutoPhase ap2(stats(), PhaseKind::WEAK_ZONES_CALLBACK);
            callWeakPointerZonesCallbacks();
        }
        {
            AutoPhase ap2(stats(), PhaseKind::WEAK_COMPARTMENT_CALLBACK);
            for (SweepGroupZonesIter zone(rt); !zone.done(); zone.next()) {
                for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next())
                    callWeakPointerCompartmentCallbacks(comp);
            }
        }
        callFinalizeCallbacks(fop, JSFINALIZE_GROUP_START);
    }

    // Updating the atom marking bitmaps. This marks atoms referenced by
    // uncollected zones so cannot be done in parallel with the other sweeping
    // work below.
    if (sweepingAtoms) {
        AutoPhase ap(stats(), PhaseKind::UPDATE_ATOMS_BITMAP);
        UpdateAtomsBitmap(rt);
    }

    sweepDebuggerOnMainThread(fop);

    {
        AutoLockHelperThreadState lock;

        AutoPhase ap(stats(), PhaseKind::SWEEP_COMPARTMENTS);

        AutoRunParallelTask sweepCCWrappers(rt, SweepCCWrappers, PhaseKind::SWEEP_CC_WRAPPER, lock);
        AutoRunParallelTask sweepObjectGroups(rt, SweepObjectGroups, PhaseKind::SWEEP_TYPE_OBJECT, lock);
        AutoRunParallelTask sweepMisc(rt, SweepMisc, PhaseKind::SWEEP_MISC, lock);
        AutoRunParallelTask sweepCompTasks(rt, SweepCompressionTasks, PhaseKind::SWEEP_COMPRESSION, lock);
        AutoRunParallelTask sweepWeakMaps(rt, SweepWeakMaps, PhaseKind::SWEEP_WEAKMAPS, lock);
        AutoRunParallelTask sweepUniqueIds(rt, SweepUniqueIds, PhaseKind::SWEEP_UNIQUEIDS, lock);

        WeakCacheTaskVector sweepCacheTasks;
        if (!PrepareWeakCacheTasks(rt, &sweepCacheTasks))
            SweepWeakCachesOnMainThread(rt);

        for (auto& task : sweepCacheTasks)
            startTask(task, PhaseKind::SWEEP_WEAK_CACHES, lock);

        {
            AutoUnlockHelperThreadState unlock(lock);
            sweepJitDataOnMainThread(fop);
        }

        for (auto& task : sweepCacheTasks)
            joinTask(task, PhaseKind::SWEEP_WEAK_CACHES, lock);
    }

    if (sweepingAtoms)
        startSweepingAtomsTable();

    // Queue all GC things in all zones for sweeping, either on the foreground
    // or on the background thread.

    for (SweepGroupZonesIter zone(rt); !zone.done(); zone.next()) {

        zone->arenas.queueForForegroundSweep(fop, ForegroundObjectFinalizePhase);
        zone->arenas.queueForForegroundSweep(fop, ForegroundNonObjectFinalizePhase);
        for (unsigned i = 0; i < ArrayLength(BackgroundFinalizePhases); ++i)
            zone->arenas.queueForBackgroundSweep(fop, BackgroundFinalizePhases[i]);

        zone->arenas.queueForegroundThingsForSweep();
    }

    sweepCache = nullptr;
    safeToYield = true;

    return Finished;
}

#ifdef JS_GC_ZEAL

bool
GCRuntime::shouldYieldForZeal(ZealMode mode)
{
    return useZeal && isIncremental && hasZealMode(mode);
}

IncrementalProgress
GCRuntime::maybeYieldForSweepingZeal(FreeOp* fop, SliceBudget& budget)
{
    /*
     * Check whether we need to yield for GC zeal. We always yield when running
     * in incremental multi-slice zeal mode so RunDebugGC can reset the slice
     * budget.
     */
    if (initialState != State::Sweep && shouldYieldForZeal(ZealMode::IncrementalMultipleSlices))
        return NotFinished;

    return Finished;
}

#endif

IncrementalProgress
GCRuntime::endSweepingSweepGroup(FreeOp* fop, SliceBudget& budget)
{
    {
        gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::FINALIZE_END);
        FreeOp fop(rt);
        callFinalizeCallbacks(&fop, JSFINALIZE_GROUP_END);
    }

    /* Update the GC state for zones we have swept. */
    for (SweepGroupZonesIter zone(rt); !zone.done(); zone.next()) {
        AutoLockGC lock(rt);
        zone->changeGCState(Zone::Sweep, Zone::Finished);
        zone->threshold.updateAfterGC(zone->usage.gcBytes(), invocationKind, tunables,
                                      schedulingState, lock);
        zone->updateAllGCMallocCountersOnGCEnd(lock);
        zone->arenas.unmarkPreMarkedFreeCells();
    }

    /*
     * Start background thread to sweep zones if required, sweeping the atoms
     * zone last if present.
     */
    bool sweepAtomsZone = false;
    ZoneList zones;
    for (SweepGroupZonesIter zone(rt); !zone.done(); zone.next()) {
        if (zone->isAtomsZone())
            sweepAtomsZone = true;
        else
            zones.append(zone);
    }
    if (sweepAtomsZone)
        zones.append(atomsZone);

    queueZonesForBackgroundSweep(zones);

    if (!sweepOnBackgroundThread)
        sweepTask.runFromMainThread(rt);

    return Finished;
}

void
GCRuntime::beginSweepPhase(JS::gcreason::Reason reason, AutoGCSession& session)
{
    /*
     * Sweep phase.
     *
     * Finalize as we sweep, outside of lock but with RuntimeHeapIsBusy()
     * true so that any attempt to allocate a GC-thing from a finalizer will
     * fail, rather than nest badly and leave the unmarked newborn to be swept.
     */

    MOZ_ASSERT(!abortSweepAfterCurrentGroup);

    AutoSetThreadIsSweeping threadIsSweeping;

    releaseHeldRelocatedArenas();

    computeNonIncrementalMarkingForValidation(session);

    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP);

    sweepOnBackgroundThread =
        reason != JS::gcreason::DESTROY_RUNTIME &&
        !gcTracer.traceEnabled() &&
        CanUseExtraThreads();

    releaseObservedTypes = shouldReleaseObservedTypes();

    AssertNoWrappersInGrayList(rt);
    DropStringWrappers(rt);

    groupZonesForSweeping(reason);

    sweepActions->assertFinished();

    // We must not yield after this point until we start sweeping the first sweep
    // group.
    safeToYield = false;
}

bool
ArenaLists::foregroundFinalize(FreeOp* fop, AllocKind thingKind, SliceBudget& sliceBudget,
                               SortedArenaList& sweepList)
{
    if (!arenaListsToSweep(thingKind) && incrementalSweptArenas.ref().isEmpty())
        return true;

    // Empty object arenas are not released until all foreground GC things have
    // been swept.
    KeepArenasEnum keepArenas = IsObjectAllocKind(thingKind) ? KEEP_ARENAS : RELEASE_ARENAS;

    if (!FinalizeArenas(fop, &arenaListsToSweep(thingKind), sweepList,
                        thingKind, sliceBudget, keepArenas))
    {
        incrementalSweptArenaKind = thingKind;
        incrementalSweptArenas = sweepList.toArenaList();
        return false;
    }

    // Clear any previous incremental sweep state we may have saved.
    incrementalSweptArenas.ref().clear();

    if (IsObjectAllocKind(thingKind))
      sweepList.extractEmpty(&savedEmptyArenas.ref());

    ArenaList finalized = sweepList.toArenaList();
    arenaLists(thingKind) = finalized.insertListWithCursorAtEnd(arenaLists(thingKind));

    return true;
}

IncrementalProgress
GCRuntime::drainMarkStack(SliceBudget& sliceBudget, gcstats::PhaseKind phase)
{
    /* Run a marking slice and return whether the stack is now empty. */
    gcstats::AutoPhase ap(stats(), phase);
    return marker.drainMarkStack(sliceBudget) ? Finished : NotFinished;
}

static void
SweepThing(Shape* shape)
{
    if (!shape->isMarkedAny())
        shape->sweep();
}

static void
SweepThing(JSScript* script, AutoClearTypeInferenceStateOnOOM* oom)
{
    AutoSweepTypeScript sweep(script, oom);
}

static void
SweepThing(ObjectGroup* group, AutoClearTypeInferenceStateOnOOM* oom)
{
    AutoSweepObjectGroup sweep(group, oom);
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

IncrementalProgress
GCRuntime::sweepTypeInformation(FreeOp* fop, SliceBudget& budget, Zone* zone)
{
    // Sweep dead type information stored in scripts and object groups, but
    // don't finalize them yet. We have to sweep dead information from both live
    // and dead scripts and object groups, so that no dead references remain in
    // them. Type inference can end up crawling these zones again, such as for
    // TypeCompartment::markSetsUnknown, and if this happens after sweeping for
    // the sweep group finishes we won't be able to determine which things in
    // the zone are live.

    gcstats::AutoPhase ap1(stats(), gcstats::PhaseKind::SWEEP_COMPARTMENTS);
    gcstats::AutoPhase ap2(stats(), gcstats::PhaseKind::SWEEP_TYPES);

    ArenaLists& al = zone->arenas;

    AutoClearTypeInferenceStateOnOOM oom(zone);

    if (!SweepArenaList<JSScript>(&al.gcScriptArenasToUpdate.ref(), budget, &oom))
        return NotFinished;

    if (!SweepArenaList<ObjectGroup>(&al.gcObjectGroupArenasToUpdate.ref(), budget, &oom))
        return NotFinished;

    // Finish sweeping type information in the zone.
    {
        gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_TYPES_END);
        zone->types.endSweep(rt);
    }

    return Finished;
}

IncrementalProgress
GCRuntime::releaseSweptEmptyArenas(FreeOp* fop, SliceBudget& budget, Zone* zone)
{
    // Foreground finalized objects have already been finalized, and now their
    // arenas can be reclaimed by freeing empty ones and making non-empty ones
    // available for allocation.

    zone->arenas.releaseForegroundSweptEmptyArenas();
    return Finished;
}

void
GCRuntime::startSweepingAtomsTable()
{
    auto& maybeAtoms = maybeAtomsToSweep.ref();
    MOZ_ASSERT(maybeAtoms.isNothing());

    AtomsTable* atomsTable = rt->atomsForSweeping();
    if (!atomsTable)
        return;

    // Create secondary tables to hold new atoms added while we're sweeping the
    // main tables incrementally.
    if (!atomsTable->startIncrementalSweep()) {
        atomsTable->sweepAll(rt);
        return;
    }

    // Initialize remaining atoms to sweep.
    maybeAtoms.emplace(*atomsTable);
}

IncrementalProgress
GCRuntime::sweepAtomsTable(FreeOp* fop, SliceBudget& budget)
{
    if (!atomsZone->isGCSweeping())
        return Finished;

    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_ATOMS_TABLE);

    auto& maybeAtoms = maybeAtomsToSweep.ref();
    if (!maybeAtoms)
        return Finished;

    if (!rt->atomsForSweeping()->sweepIncrementally(maybeAtoms.ref(), budget))
        return NotFinished;

    maybeAtoms.reset();

    return Finished;
}

class js::gc::WeakCacheSweepIterator
{
    JS::Zone*& sweepZone;
    JS::detail::WeakCacheBase*& sweepCache;

  public:
    explicit WeakCacheSweepIterator(GCRuntime* gc)
      : sweepZone(gc->sweepZone.ref()), sweepCache(gc->sweepCache.ref())
    {
        // Initialize state when we start sweeping a sweep group.
        if (!sweepZone) {
            sweepZone = gc->currentSweepGroup;
            MOZ_ASSERT(!sweepCache);
            sweepCache = sweepZone->weakCaches().getFirst();
            settle();
        }

        checkState();
    }

    bool empty(AutoLockHelperThreadState& lock) {
        return !sweepZone;
    }

    JS::detail::WeakCacheBase* next(AutoLockHelperThreadState& lock) {
        if (empty(lock))
            return nullptr;

        JS::detail::WeakCacheBase* result = sweepCache;
        sweepCache = sweepCache->getNext();
        settle();
        checkState();
        return result;
    }

    void settle() {
        while (sweepZone) {
            while (sweepCache && !sweepCache->needsIncrementalBarrier())
                sweepCache = sweepCache->getNext();

            if (sweepCache)
                break;

            sweepZone = sweepZone->nextNodeInGroup();
            if (sweepZone)
                sweepCache = sweepZone->weakCaches().getFirst();
        }
    }

  private:
    void checkState() {
        MOZ_ASSERT((!sweepZone && !sweepCache) ||
                   (sweepCache && sweepCache->needsIncrementalBarrier()));
    }
};

class IncrementalSweepWeakCacheTask : public GCParallelTaskHelper<IncrementalSweepWeakCacheTask>
{
    WeakCacheSweepIterator& work_;
    SliceBudget& budget_;
    AutoLockHelperThreadState& lock_;
    JS::detail::WeakCacheBase* cache_;

  public:
    IncrementalSweepWeakCacheTask(JSRuntime* rt, WeakCacheSweepIterator& work, SliceBudget& budget,
                                  AutoLockHelperThreadState& lock)
      : GCParallelTaskHelper(rt), work_(work), budget_(budget), lock_(lock),
        cache_(work.next(lock))
    {
        MOZ_ASSERT(cache_);
        runtime()->gc.startTask(*this, gcstats::PhaseKind::SWEEP_WEAK_CACHES, lock_);
    }

    ~IncrementalSweepWeakCacheTask() {
        runtime()->gc.joinTask(*this, gcstats::PhaseKind::SWEEP_WEAK_CACHES, lock_);
    }

    void run() {
        do {
            MOZ_ASSERT(cache_->needsIncrementalBarrier());
            size_t steps = cache_->sweep();
            cache_->setNeedsIncrementalBarrier(false);

            AutoLockHelperThreadState lock;
            budget_.step(steps);
            if (budget_.isOverBudget())
                break;

            cache_ = work_.next(lock);
        } while(cache_);
    }
};

static const size_t MaxWeakCacheSweepTasks = 8;

static size_t
WeakCacheSweepTaskCount()
{
    size_t targetTaskCount = HelperThreadState().cpuCount;
    return Min(targetTaskCount, MaxWeakCacheSweepTasks);
}

IncrementalProgress
GCRuntime::sweepWeakCaches(FreeOp* fop, SliceBudget& budget)
{
    WeakCacheSweepIterator work(this);

    {
        AutoLockHelperThreadState lock;
        gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_COMPARTMENTS);

        Maybe<IncrementalSweepWeakCacheTask> tasks[MaxWeakCacheSweepTasks];
        for (size_t i = 0; !work.empty(lock) && i < WeakCacheSweepTaskCount(); i++)
            tasks[i].emplace(rt, work, budget, lock);

        // Tasks run until budget or work is exhausted.
    }

    AutoLockHelperThreadState lock;
    return work.empty(lock) ? Finished : NotFinished;
}

IncrementalProgress
GCRuntime::finalizeAllocKind(FreeOp* fop, SliceBudget& budget, Zone* zone, AllocKind kind)
{
    // Set the number of things per arena for this AllocKind.
    size_t thingsPerArena = Arena::thingsPerArena(kind);
    auto& sweepList = incrementalSweepList.ref();
    sweepList.setThingsPerArena(thingsPerArena);

    if (!zone->arenas.foregroundFinalize(fop, kind, budget, sweepList))
        return NotFinished;

    // Reset the slots of the sweep list that we used.
    sweepList.reset(thingsPerArena);

    return Finished;
}

IncrementalProgress
GCRuntime::sweepShapeTree(FreeOp* fop, SliceBudget& budget, Zone* zone)
{
    // Remove dead shapes from the shape tree, but don't finalize them yet.

    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP_SHAPE);

    ArenaLists& al = zone->arenas;

    if (!SweepArenaList<Shape>(&al.gcShapeArenasToUpdate.ref(), budget))
        return NotFinished;

    if (!SweepArenaList<AccessorShape>(&al.gcAccessorShapeArenasToUpdate.ref(), budget))
        return NotFinished;

    return Finished;
}

// An iterator for a standard container that provides an STL-like begin()/end()
// interface. This iterator provides a done()/get()/next() style interface.
template <typename Container>
class ContainerIter
{
    using Iter = decltype(mozilla::DeclVal<const Container>().begin());
    using Elem = decltype(*mozilla::DeclVal<Iter>());

    Iter iter;
    const Iter end;

  public:
    explicit ContainerIter(const Container& container)
      : iter(container.begin()), end(container.end())
    {}

    bool done() const {
        return iter == end;
    }

    Elem get() const {
        return *iter;
    }

    void next() {
        MOZ_ASSERT(!done());
        ++iter;
    }
};

// IncrementalIter is a template class that makes a normal iterator into one
// that can be used to perform incremental work by using external state that
// persists between instantiations. The state is only initialised on the first
// use and subsequent uses carry on from the previous state.
template <typename Iter>
struct IncrementalIter
{
    using State = Maybe<Iter>;
    using Elem = decltype(mozilla::DeclVal<Iter>().get());

  private:
    State& maybeIter;

  public:
    template <typename... Args>
    explicit IncrementalIter(State& maybeIter, Args&&... args)
      : maybeIter(maybeIter)
    {
        if (maybeIter.isNothing())
            maybeIter.emplace(std::forward<Args>(args)...);
    }

    ~IncrementalIter() {
        if (done())
            maybeIter.reset();
    }

    bool done() const {
        return maybeIter.ref().done();
    }

    Elem get() const {
        return maybeIter.ref().get();
    }

    void next() {
        maybeIter.ref().next();
    }
};

// Iterate through the sweep groups created by GCRuntime::groupZonesForSweeping().
class js::gc::SweepGroupsIter
{
    GCRuntime* gc;

  public:
    explicit SweepGroupsIter(JSRuntime* rt)
      : gc(&rt->gc)
    {
        MOZ_ASSERT(gc->currentSweepGroup);
    }

    bool done() const {
        return !gc->currentSweepGroup;
    }

    Zone* get() const {
        return gc->currentSweepGroup;
    }

    void next() {
        MOZ_ASSERT(!done());
        gc->getNextSweepGroup();
    }
};

namespace sweepaction {

// Implementation of the SweepAction interface that calls a method on GCRuntime.
template <typename... Args>
class SweepActionCall final : public SweepAction<GCRuntime*, Args...>
{
    using Method = IncrementalProgress (GCRuntime::*)(Args...);

    Method method;

  public:
    explicit SweepActionCall(Method m) : method(m) {}
    IncrementalProgress run(GCRuntime* gc, Args... args) override {
        return (gc->*method)(args...);
    }
    void assertFinished() const override { }
};

#ifdef JS_GC_ZEAL
// Implementation of the SweepAction interface that yields in a specified zeal
// mode and then calls another action.
template <typename... Args>
class SweepActionMaybeYield final : public SweepAction<GCRuntime*, Args...>
{
    using Action = SweepAction<GCRuntime*, Args...>;

    ZealMode mode;
    UniquePtr<Action> action;
    bool triggered;

  public:
    SweepActionMaybeYield(UniquePtr<Action> action, ZealMode mode)
      : mode(mode), action(std::move(action)), triggered(false) {}

    IncrementalProgress run(GCRuntime* gc, Args... args) override {
        if (!triggered && gc->shouldYieldForZeal(mode)) {
            triggered = true;
            return NotFinished;
        }

        triggered = false;
        return action->run(gc, args...);
    }

    void assertFinished() const override {
        MOZ_ASSERT(!triggered);
    }
};
#endif

// Implementation of the SweepAction interface that calls a list of actions in
// sequence.
template <typename... Args>
class SweepActionSequence final : public SweepAction<Args...>
{
    using Action = SweepAction<Args...>;
    using ActionVector = Vector<UniquePtr<Action>, 0, SystemAllocPolicy>;
    using Iter = IncrementalIter<ContainerIter<ActionVector>>;

    ActionVector actions;
    typename Iter::State iterState;

  public:
    bool init(UniquePtr<Action>* acts, size_t count) {
        for (size_t i = 0; i < count; i++) {
            if (!actions.emplaceBack(std::move(acts[i])))
                return false;
        }
        return true;
    }

    IncrementalProgress run(Args... args) override {
        for (Iter iter(iterState, actions); !iter.done(); iter.next()) {
            if (iter.get()->run(args...) == NotFinished)
                return NotFinished;
        }
        return Finished;
    }

    void assertFinished() const override {
        MOZ_ASSERT(iterState.isNothing());
        for (const auto& action : actions)
            action->assertFinished();
    }
};

template <typename Iter, typename Init, typename... Args>
class SweepActionForEach final : public SweepAction<Args...>
{
    using Elem = decltype(mozilla::DeclVal<Iter>().get());
    using Action = SweepAction<Args..., Elem>;
    using IncrIter = IncrementalIter<Iter>;

    Init iterInit;
    UniquePtr<Action> action;
    typename IncrIter::State iterState;

  public:
    SweepActionForEach(const Init& init, UniquePtr<Action> action)
      : iterInit(init), action(std::move(action))
    {}

    IncrementalProgress run(Args... args) override {
        for (IncrIter iter(iterState, iterInit); !iter.done(); iter.next()) {
            if (action->run(args..., iter.get()) == NotFinished)
                return NotFinished;
        }
        return Finished;
    }

    void assertFinished() const override {
        MOZ_ASSERT(iterState.isNothing());
        action->assertFinished();
    }
};

template <typename Iter, typename Init, typename... Args>
class SweepActionRepeatFor final : public SweepAction<Args...>
{
  protected:
    using Action = SweepAction<Args...>;
    using IncrIter = IncrementalIter<Iter>;

    Init iterInit;
    UniquePtr<Action> action;
    typename IncrIter::State iterState;

  public:
    SweepActionRepeatFor(const Init& init, UniquePtr<Action> action)
      : iterInit(init), action(std::move(action))
    {}

    IncrementalProgress run(Args... args) override {
        for (IncrIter iter(iterState, iterInit); !iter.done(); iter.next()) {
            if (action->run(args...) == NotFinished)
                return NotFinished;
        }
        return Finished;
    }

    void assertFinished() const override {
        MOZ_ASSERT(iterState.isNothing());
        action->assertFinished();
    }
};

// Helper class to remove the last template parameter from the instantiation of
// a variadic template. For example:
//
//   RemoveLastTemplateParameter<Foo<X, Y, Z>>::Type ==> Foo<X, Y>
//
// This works by recursively instantiating the Impl template with the contents
// of the parameter pack so long as there are at least two parameters. The
// specialization that matches when only one parameter remains discards it and
// instantiates the target template with parameters previously processed.
template <typename T>
class RemoveLastTemplateParameter {};

template <template <typename...> class Target, typename... Args>
class RemoveLastTemplateParameter<Target<Args...>>
{
    template <typename... Ts>
    struct List {};

    template <typename R, typename... Ts>
    struct Impl {};

    template <typename... Rs, typename T>
    struct Impl<List<Rs...>, T>
    {
        using Type = Target<Rs...>;
    };

    template <typename... Rs, typename H, typename T, typename... Ts>
    struct Impl<List<Rs...>, H, T, Ts...>
    {
        using Type = typename Impl<List<Rs..., H>, T, Ts...>::Type;
    };

  public:
    using Type = typename Impl<List<>, Args...>::Type;
};

template <typename... Args>
static UniquePtr<SweepAction<GCRuntime*, Args...>>
Call(IncrementalProgress (GCRuntime::*method)(Args...)) {
   return MakeUnique<SweepActionCall<Args...>>(method);
}

template <typename... Args>
static UniquePtr<SweepAction<GCRuntime*, Args...>>
MaybeYield(ZealMode zealMode, UniquePtr<SweepAction<GCRuntime*, Args...>> action) {
#ifdef JS_GC_ZEAL
    return js::MakeUnique<SweepActionMaybeYield<Args...>>(std::move(action), zealMode);
#else
    return action;
#endif
}

template <typename... Args, typename... Rest>
static UniquePtr<SweepAction<Args...>>
Sequence(UniquePtr<SweepAction<Args...>> first, Rest... rest)
{
    UniquePtr<SweepAction<Args...>> actions[] = { std::move(first), std::move(rest)... };
    auto seq = MakeUnique<SweepActionSequence<Args...>>();
    if (!seq || !seq->init(actions, ArrayLength(actions)))
        return nullptr;

    return UniquePtr<SweepAction<Args...>>(std::move(seq));
}

template <typename... Args>
static UniquePtr<SweepAction<Args...>>
RepeatForSweepGroup(JSRuntime* rt, UniquePtr<SweepAction<Args...>> action)
{
    if (!action)
        return nullptr;

    using Action = SweepActionRepeatFor<SweepGroupsIter, JSRuntime*, Args...>;
    return js::MakeUnique<Action>(rt, std::move(action));
}

template <typename... Args>
static UniquePtr<typename RemoveLastTemplateParameter<SweepAction<Args...>>::Type>
ForEachZoneInSweepGroup(JSRuntime* rt, UniquePtr<SweepAction<Args...>> action)
{
    if (!action)
        return nullptr;

    using Action = typename RemoveLastTemplateParameter<
        SweepActionForEach<SweepGroupZonesIter, JSRuntime*, Args...>>::Type;
    return js::MakeUnique<Action>(rt, std::move(action));
}

template <typename... Args>
static UniquePtr<typename RemoveLastTemplateParameter<SweepAction<Args...>>::Type>
ForEachAllocKind(AllocKinds kinds, UniquePtr<SweepAction<Args...>> action)
{
    if (!action)
        return nullptr;

    using Action = typename RemoveLastTemplateParameter<
        SweepActionForEach<ContainerIter<AllocKinds>, AllocKinds, Args...>>::Type;
    return js::MakeUnique<Action>(kinds, std::move(action));
}

} // namespace sweepaction

bool
GCRuntime::initSweepActions()
{
    using namespace sweepaction;
    using sweepaction::Call;

    sweepActions.ref() =
        RepeatForSweepGroup(rt,
            Sequence(
                Call(&GCRuntime::endMarkingSweepGroup),
                Call(&GCRuntime::beginSweepingSweepGroup),
#ifdef JS_GC_ZEAL
                Call(&GCRuntime::maybeYieldForSweepingZeal),
#endif
                MaybeYield(ZealMode::YieldBeforeSweepingAtoms,
                           Call(&GCRuntime::sweepAtomsTable)),
                MaybeYield(ZealMode::YieldBeforeSweepingCaches,
                           Call(&GCRuntime::sweepWeakCaches)),
                ForEachZoneInSweepGroup(rt,
                    Sequence(
                        MaybeYield(ZealMode::YieldBeforeSweepingTypes,
                                   Call(&GCRuntime::sweepTypeInformation)),
                        MaybeYield(ZealMode::YieldBeforeSweepingObjects,
                                   ForEachAllocKind(ForegroundObjectFinalizePhase.kinds,
                                                    Call(&GCRuntime::finalizeAllocKind))),
                        MaybeYield(ZealMode::YieldBeforeSweepingNonObjects,
                                   ForEachAllocKind(ForegroundNonObjectFinalizePhase.kinds,
                                                    Call(&GCRuntime::finalizeAllocKind))),
                        MaybeYield(ZealMode::YieldBeforeSweepingShapeTrees,
                                   Call(&GCRuntime::sweepShapeTree)),
                        Call(&GCRuntime::releaseSweptEmptyArenas))),
                Call(&GCRuntime::endSweepingSweepGroup)));

    return sweepActions != nullptr;
}

IncrementalProgress
GCRuntime::performSweepActions(SliceBudget& budget)
{
    AutoSetThreadIsSweeping threadIsSweeping;

    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP);
    FreeOp fop(rt);

    // Drain the mark stack, except in the first sweep slice where we must not
    // yield to the mutator until we've starting sweeping a sweep group.
    MOZ_ASSERT(initialState <= State::Sweep);
    if (initialState != State::Sweep) {
        MOZ_ASSERT(marker.isDrained());
    } else {
        if (drainMarkStack(budget, gcstats::PhaseKind::SWEEP_MARK) == NotFinished)
            return NotFinished;
    }

    return sweepActions->run(this, &fop, budget);
}

bool
GCRuntime::allCCVisibleZonesWereCollected() const
{
    // Calculate whether the gray marking state is now valid.
    //
    // The gray bits change from invalid to valid if we finished a full GC from
    // the point of view of the cycle collector. We ignore the following:
    //
    //  - Helper thread zones, as these are not reachable from the main heap.
    //  - The atoms zone, since strings and symbols are never marked gray.
    //  - Empty zones.
    //
    // These exceptions ensure that when the CC requests a full GC the gray mark
    // state ends up valid even it we don't collect all of the zones.

    for (ZonesIter zone(rt, SkipAtoms); !zone.done(); zone.next()) {
        if (!zone->isCollecting() &&
            !zone->usedByHelperThread() &&
            !zone->arenas.arenaListsAreEmpty())
        {
            return false;
        }
    }

    return true;
}

void
GCRuntime::endSweepPhase(bool destroyingRuntime)
{
    sweepActions->assertFinished();

    AutoSetThreadIsSweeping threadIsSweeping;

    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::SWEEP);
    FreeOp fop(rt);

    MOZ_ASSERT_IF(destroyingRuntime, !sweepOnBackgroundThread);

    // Update the runtime malloc counter only if we were doing a full GC.
    if (isFull) {
        AutoLockGC lock(rt);
        mallocCounter.updateOnGCEnd(tunables, lock);
    }

    {
        gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::DESTROY);

        /*
         * Sweep script filenames after sweeping functions in the generic loop
         * above. In this way when a scripted function's finalizer destroys the
         * script and calls rt->destroyScriptHook, the hook can still access the
         * script's filename. See bug 323267.
         */
        SweepScriptData(rt);

        /* Clear out any small pools that we're hanging on to. */
        if (rt->hasJitRuntime())
            rt->jitRuntime()->execAlloc().purge();
    }

    {
        gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::FINALIZE_END);
        callFinalizeCallbacks(&fop, JSFINALIZE_COLLECTION_END);

        if (allCCVisibleZonesWereCollected())
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

    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::COMPACT);

    MOZ_ASSERT(zonesToMaybeCompact.ref().isEmpty());
    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        if (CanRelocateZone(zone))
            zonesToMaybeCompact.ref().append(zone);
    }

    MOZ_ASSERT(!relocatedArenasToRelease);
    startedCompacting = true;
}

IncrementalProgress
GCRuntime::compactPhase(JS::gcreason::Reason reason, SliceBudget& sliceBudget,
                        AutoGCSession& session)
{
    assertBackgroundSweepingFinished();
    MOZ_ASSERT(startedCompacting);

    gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::COMPACT);

    // TODO: JSScripts can move. If the sampler interrupts the GC in the
    // middle of relocating an arena, invalid JSScript pointers may be
    // accessed. Suppress all sampling until a finer-grained solution can be
    // found. See bug 1295775.
    AutoSuppressProfilerSampling suppressSampling(rt->mainContextFromOwnThread());

    ZoneList relocatedZones;
    Arena* relocatedArenas = nullptr;
    while (!zonesToMaybeCompact.ref().isEmpty()) {

        Zone* zone = zonesToMaybeCompact.ref().front();
        zonesToMaybeCompact.ref().removeFront();

        MOZ_ASSERT(nursery().isEmpty());
        zone->changeGCState(Zone::Finished, Zone::Compact);

        if (relocateArenas(zone, reason, relocatedArenas, sliceBudget)) {
            updateZonePointersToRelocatedCells(zone);
            relocatedZones.append(zone);
        } else {
            zone->changeGCState(Zone::Compact, Zone::Finished);
        }

        if (sliceBudget.isOverBudget())
            break;
    }

    if (!relocatedZones.isEmpty()) {
        updateRuntimePointersToRelocatedCells(session);

        do {
            Zone* zone = relocatedZones.front();
            relocatedZones.removeFront();
            zone->changeGCState(Zone::Compact, Zone::Finished);
        }
        while (!relocatedZones.isEmpty());
    }

    if (ShouldProtectRelocatedArenas(reason))
        protectAndHoldArenas(relocatedArenas);
    else
        releaseRelocatedArenas(relocatedArenas);

    // Clear caches that can contain cell pointers.
    rt->caches().purgeForCompaction();

#ifdef DEBUG
    CheckHashTablesAfterMovingGC(rt);
#endif

    return zonesToMaybeCompact.ref().isEmpty() ? Finished : NotFinished;
}

void
GCRuntime::endCompactPhase()
{
    startedCompacting = false;
}

void
GCRuntime::finishCollection()
{
    assertBackgroundSweepingFinished();
    MOZ_ASSERT(marker.isDrained());
    marker.stop();
    clearBufferedGrayRoots();

    auto currentTime = mozilla::TimeStamp::Now();
    schedulingState.updateHighFrequencyMode(lastGCTime, currentTime, tunables);

    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        if (zone->isCollecting()) {
            zone->changeGCState(Zone::Finished, Zone::NoGC);
            zone->notifyObservingDebuggers();
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
        MOZ_CRASH("Should never have an Idle or CC heap state when pushing GC profiling stack frames!");
    }
    MOZ_ASSERT_UNREACHABLE("Should have exhausted every JS::HeapState variant!");
    return nullptr;
}

/* Start a new heap session. */
AutoHeapSession::AutoHeapSession(JSRuntime* rt, JS::HeapState heapState)
  : runtime(rt),
    prevState(rt->heapState_),
    profilingStackFrame(rt->mainContextFromOwnThread(), HeapStateToLabel(heapState),
                        ProfilingStackFrame::Category::GCCC)
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
    MOZ_ASSERT(prevState == JS::HeapState::Idle);
    MOZ_ASSERT(heapState != JS::HeapState::Idle);

    rt->heapState_ = heapState;
}

AutoHeapSession::~AutoHeapSession()
{
    MOZ_ASSERT(JS::RuntimeHeapIsBusy());
    runtime->heapState_ = prevState;
}

JS_PUBLIC_API(JS::HeapState)
JS::RuntimeHeapState()
{
    return TlsContext.get()->runtime()->heapState();
}

GCRuntime::IncrementalResult
GCRuntime::resetIncrementalGC(gc::AbortReason reason, AutoGCSession& session)
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
            zone->setNeedsIncrementalBarrier(false);
            zone->changeGCState(Zone::Mark, Zone::NoGC);
            zone->arenas.unmarkPreMarkedFreeCells();
        }

        {
            AutoLockHelperThreadState lock;
            blocksToFreeAfterSweeping.ref().freeAll();
        }

        lastMarkSlice = false;
        incrementalState = State::NotActive;

        MOZ_ASSERT(!marker.shouldCheckCompartments());

        break;
      }

      case State::Sweep: {
        marker.reset();

        for (CompartmentsIter c(rt); !c.done(); c.next())
            c->gcState.scheduledForDestruction = false;

        for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
            if (zone->isGCMarking())
                zone->arenas.unmarkPreMarkedFreeCells();
        }

        /* Finish sweeping the current sweep group, then abort. */
        abortSweepAfterCurrentGroup = true;

        /* Don't perform any compaction after sweeping. */
        bool wasCompacting = isCompacting;
        isCompacting = false;

        auto unlimited = SliceBudget::unlimited();
        incrementalCollectSlice(unlimited, JS::gcreason::RESET, session);

        isCompacting = wasCompacting;

        {
            gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::WAIT_BACKGROUND_THREAD);
            rt->gc.waitBackgroundSweepOrAllocEnd();
        }
        break;
      }

      case State::Finalize: {
        {
            gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::WAIT_BACKGROUND_THREAD);
            rt->gc.waitBackgroundSweepOrAllocEnd();
        }

        bool wasCompacting = isCompacting;
        isCompacting = false;

        auto unlimited = SliceBudget::unlimited();
        incrementalCollectSlice(unlimited, JS::gcreason::RESET, session);

        isCompacting = wasCompacting;

        break;
      }

      case State::Compact: {
        bool wasCompacting = isCompacting;

        isCompacting = true;
        startedCompacting = true;
        zonesToMaybeCompact.ref().clear();

        auto unlimited = SliceBudget::unlimited();
        incrementalCollectSlice(unlimited, JS::gcreason::RESET, session);

        isCompacting = wasCompacting;
        break;
      }

      case State::Decommit: {
        auto unlimited = SliceBudget::unlimited();
        incrementalCollectSlice(unlimited, JS::gcreason::RESET, session);
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

    return IncrementalResult::ResetIncremental;
}

namespace {

/*
 * Temporarily disable barriers during GC slices.
 */
class AutoDisableBarriers {
  public:
    explicit AutoDisableBarriers(JSRuntime* rt);
    ~AutoDisableBarriers();

  private:
    JSRuntime* runtime;
    AutoSetThreadIsPerformingGC performingGC;
};

} /* anonymous namespace */

AutoDisableBarriers::AutoDisableBarriers(JSRuntime* rt)
  : runtime(rt)
{
    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        /*
         * Clear needsIncrementalBarrier early so we don't do any write
         * barriers during GC. We don't need to update the Ion barriers (which
         * is expensive) because Ion code doesn't run during GC. If need be,
         * we'll update the Ion barriers in ~AutoDisableBarriers.
         */
        if (zone->isGCMarking()) {
            MOZ_ASSERT(zone->needsIncrementalBarrier());
            zone->setNeedsIncrementalBarrier(false);
        }
        MOZ_ASSERT(!zone->needsIncrementalBarrier());
    }
}

AutoDisableBarriers::~AutoDisableBarriers()
{
    /* We can't use GCZonesIter if this is the end of the last slice. */
    for (ZonesIter zone(runtime, WithAtoms); !zone.done(); zone.next()) {
        MOZ_ASSERT(!zone->needsIncrementalBarrier());
        if (zone->isGCMarking())
            zone->setNeedsIncrementalBarrier(true);
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

GCRuntime::IncrementalResult
GCRuntime::incrementalCollectSlice(SliceBudget& budget, JS::gcreason::Reason reason,
                                   AutoGCSession& session)
{
    AutoDisableBarriers disableBarriers(rt);

    bool destroyingRuntime = (reason == JS::gcreason::DESTROY_RUNTIME);

    initialState = incrementalState;

#ifdef JS_GC_ZEAL
    /*
     * Do the incremental collection type specified by zeal mode if the
     * collection was triggered by runDebugGC() and incremental GC has not been
     * cancelled by resetIncrementalGC().
     */
    useZeal = reason == JS::gcreason::DEBUG_GC && !budget.isUnlimited();
#else
    bool useZeal = false;
#endif

#ifdef DEBUG
    {
        char budgetBuffer[32];
        budget.describe(budgetBuffer, 32);
        stats().writeLogMessage(
            "Incremental: %d, lastMarkSlice: %d, useZeal: %d, budget: %s",
            bool(isIncremental), bool(lastMarkSlice), bool(useZeal),
            budgetBuffer);
    }
#endif
    MOZ_ASSERT_IF(isIncrementalGCInProgress(), isIncremental || lastMarkSlice);

    /*
     * It's possible to be in the middle of an incremental collection, but not
     * doing an incremental collection IF we had to return and re-enter the GC
     * in order to collect the nursery.
     */
    MOZ_ASSERT_IF(isIncrementalGCInProgress() && !isIncremental,
         lastMarkSlice && nursery().isEmpty());

    isIncremental = !budget.isUnlimited();

    if (useZeal && hasIncrementalTwoSliceZealMode()) {
        /*
         * Yields between slices occurs at predetermined points in these modes;
         * the budget is not used.
         */
        stats().writeLogMessage(
            "Using unlimited budget for two-slice zeal mode");
        budget.makeUnlimited();
    }

    switch (incrementalState) {
      case State::NotActive:
        initialReason = reason;
        cleanUpEverything = ShouldCleanUpEverything(reason, invocationKind);
        isCompacting = shouldCompact();
        MOZ_ASSERT(!lastMarkSlice);
        rootsRemoved = false;

        incrementalState = State::MarkRoots;

        MOZ_FALLTHROUGH;

      case State::MarkRoots:
        if (!beginMarkPhase(reason, session)) {
            incrementalState = State::NotActive;
            return IncrementalResult::Ok;
        }

        if (!destroyingRuntime)
            pushZealSelectedObjects();

        incrementalState = State::Mark;

        if (isIncremental && useZeal && hasZealMode(ZealMode::YieldBeforeMarking))
            break;

        MOZ_FALLTHROUGH;

      case State::Mark:
        AutoGCRooter::traceAllWrappers(rt->mainContextFromOwnThread(), &marker);

        /* If we needed delayed marking for gray roots, then collect until done. */
        if (isIncremental && !hasValidGrayRootsBuffer()) {
            budget.makeUnlimited();
            isIncremental = false;
            stats().nonincremental(AbortReason::GrayRootBufferingFailed);
        }

        if (drainMarkStack(budget, gcstats::PhaseKind::MARK) == NotFinished)
            break;

        MOZ_ASSERT(marker.isDrained());

        /*
         * There are a number of reasons why we break out of collection here,
         * either ending the slice or to run a new interation of the loop in
         * GCRuntime::collect()
         */

        /*
         * In incremental GCs where we have already performed more than one
         * slice we yield after marking with the aim of starting the sweep in
         * the next slice, since the first slice of sweeping can be expensive.
         *
         * This is modified by the various zeal modes.  We don't yield in
         * YieldBeforeMarking mode and we always yield in YieldBeforeSweeping
         * mode.
         *
         * We will need to mark anything new on the stack when we resume, so
         * we stay in Mark state.
         */
        if (isIncremental && !lastMarkSlice) {
            if ((initialState == State::Mark &&
                    !(useZeal && hasZealMode(ZealMode::YieldBeforeMarking))) ||
                (useZeal && hasZealMode(ZealMode::YieldBeforeSweeping)))
            {
                lastMarkSlice = true;
                stats().writeLogMessage("Yielding before starting sweeping");
                break;
            }
        }

        /*
         * If the nursery is not-empty we need to collect it before sweeping.
         *
         * This can happen regardless of 'isIncremental' since the GC may have
         * been incremental when it started and later made a decision to do
         * non-incremental collection.
         *
         * It's important to check this after the above case since this one
         * wont give the mutator a chance to run.
         */
        if (!nursery().isEmpty()) {
            lastMarkSlice = true;
            stats().writeLogMessage(
                "returning to collect the nursery before sweeping");
            return IncrementalResult::ReturnToEvictNursery;
        }

        incrementalState = State::Sweep;
        lastMarkSlice = false;
        beginSweepPhase(reason, session);

        MOZ_FALLTHROUGH;

      case State::Sweep:
        MOZ_ASSERT(nursery().isEmpty());
        if (performSweepActions(budget) == NotFinished)
            break;

        endSweepPhase(destroyingRuntime);

        incrementalState = State::Finalize;

        MOZ_FALLTHROUGH;

      case State::Finalize:
        {
            gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::WAIT_BACKGROUND_THREAD);

            // Yield until background finalization is done.
            if (!budget.isUnlimited()) {
                // Poll for end of background sweeping
                if (isBackgroundSweeping())
                    break;
            } else {
                waitBackgroundSweepEnd();
            }
        }

        {
            // Re-sweep the zones list, now that background finalization is
            // finished to actually remove and free dead zones.
            gcstats::AutoPhase ap1(stats(), gcstats::PhaseKind::SWEEP);
            gcstats::AutoPhase ap2(stats(), gcstats::PhaseKind::DESTROY);
            AutoSetThreadIsSweeping threadIsSweeping;
            FreeOp fop(rt);
            sweepZones(&fop, destroyingRuntime);
        }

        MOZ_ASSERT(!startedCompacting);
        incrementalState = State::Compact;

        // Always yield before compacting since it is not incremental.
        if (isCompacting && !budget.isUnlimited())
            break;

        MOZ_FALLTHROUGH;

      case State::Compact:
        if (isCompacting) {
            MOZ_ASSERT(nursery().isEmpty());
            if (!startedCompacting)
                beginCompactPhase();

            if (compactPhase(reason, budget, session) == NotFinished)
                break;

            endCompactPhase();
        }

        startDecommit();
        incrementalState = State::Decommit;

        MOZ_FALLTHROUGH;

      case State::Decommit:
        {
            gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::WAIT_BACKGROUND_THREAD);

            // Yield until background decommit is done.
            if (!budget.isUnlimited() && decommitTask.isRunning())
                break;

            decommitTask.join();
        }

        finishCollection();
        incrementalState = State::NotActive;
        break;
    }

    MOZ_ASSERT(safeToYield);

    return IncrementalResult::Ok;
}

gc::AbortReason
gc::IsIncrementalGCUnsafe(JSRuntime* rt)
{
    MOZ_ASSERT(!rt->mainContextFromOwnThread()->suppressGC);

    if (!rt->gc.isIncrementalGCAllowed())
        return gc::AbortReason::IncrementalDisabled;

    return gc::AbortReason::None;
}

static inline void
CheckZoneIsScheduled(Zone* zone, JS::gcreason::Reason reason, const char* trigger)
{
#ifdef DEBUG
    if (zone->isGCScheduled())
        return;

    fprintf(stderr,
            "CheckZoneIsScheduled: Zone %p not scheduled as expected in %s GC for %s trigger\n",
            zone,
            JS::gcreason::ExplainReason(reason),
            trigger);
    JSRuntime* rt = zone->runtimeFromMainThread();
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        fprintf(stderr,
                "  Zone %p:%s%s\n",
                zone.get(),
                zone->isAtomsZone() ? " atoms" : "",
                zone->isGCScheduled() ? " scheduled" : "");
    }
    fflush(stderr);
    MOZ_CRASH("Zone not scheduled");
#endif
}

GCRuntime::IncrementalResult
GCRuntime::budgetIncrementalGC(bool nonincrementalByAPI, JS::gcreason::Reason reason,
                               SliceBudget& budget, AutoGCSession& session)
{
    if (nonincrementalByAPI) {
        stats().nonincremental(gc::AbortReason::NonIncrementalRequested);
        budget.makeUnlimited();

        // Reset any in progress incremental GC if this was triggered via the
        // API. This isn't required for correctness, but sometimes during tests
        // the caller expects this GC to collect certain objects, and we need
        // to make sure to collect everything possible.
        if (reason != JS::gcreason::ALLOC_TRIGGER)
            return resetIncrementalGC(gc::AbortReason::NonIncrementalRequested, session);

        return IncrementalResult::Ok;
    }

    if (reason == JS::gcreason::ABORT_GC) {
        budget.makeUnlimited();
        stats().nonincremental(gc::AbortReason::AbortRequested);
        return resetIncrementalGC(gc::AbortReason::AbortRequested, session);
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
        return resetIncrementalGC(unsafeReason, session);
    }

    if (mallocCounter.shouldTriggerGC(tunables) == NonIncrementalTrigger) {
        budget.makeUnlimited();
        stats().nonincremental(AbortReason::MallocBytesTrigger);
    }

    bool reset = false;
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        if (!zone->canCollect())
            continue;

        if (zone->usage.gcBytes() >= zone->threshold.gcTriggerBytes()) {
            CheckZoneIsScheduled(zone, reason, "GC bytes");
            budget.makeUnlimited();
            stats().nonincremental(AbortReason::GCBytesTrigger);
        }

        if (zone->shouldTriggerGCForTooMuchMalloc() == NonIncrementalTrigger) {
            CheckZoneIsScheduled(zone, reason, "malloc bytes");
            budget.makeUnlimited();
            stats().nonincremental(AbortReason::MallocBytesTrigger);
        }

        if (isIncrementalGCInProgress() && zone->isGCScheduled() != zone->wasGCStarted())
            reset = true;
    }

    if (reset)
        return resetIncrementalGC(AbortReason::ZoneChange, session);

    return IncrementalResult::Ok;
}

namespace {

class AutoScheduleZonesForGC
{
    JSRuntime* rt_;

  public:
    explicit AutoScheduleZonesForGC(JSRuntime* rt) : rt_(rt) {
        for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
            if (!zone->canCollect())
                continue;

            if (rt->gc.gcMode() == JSGC_MODE_GLOBAL)
                zone->scheduleGC();

            // To avoid resets, continue to collect any zones that were being
            // collected in a previous slice.
            if (rt->gc.isIncrementalGCInProgress() && zone->wasGCStarted())
                zone->scheduleGC();

            // This is a heuristic to reduce the total number of collections.
            bool inHighFrequencyMode = rt->gc.schedulingState.inHighFrequencyGCMode();
            if (zone->usage.gcBytes() >= zone->threshold.eagerAllocTrigger(inHighFrequencyMode))
                zone->scheduleGC();

            // This ensures we collect zones that have reached the malloc limit.
            if (zone->shouldTriggerGCForTooMuchMalloc())
                zone->scheduleGC();
        }
    }

    ~AutoScheduleZonesForGC() {
        for (ZonesIter zone(rt_, WithAtoms); !zone.done(); zone.next())
            zone->unscheduleGC();
    }
};

} /* anonymous namespace */

class js::gc::AutoCallGCCallbacks {
    GCRuntime& gc_;

  public:
    explicit AutoCallGCCallbacks(GCRuntime& gc) : gc_(gc) {
        gc_.maybeCallGCCallback(JSGC_BEGIN);
    }
    ~AutoCallGCCallbacks() {
        gc_.maybeCallGCCallback(JSGC_END);
    }
};

void
GCRuntime::maybeCallGCCallback(JSGCStatus status)
{
    if (!gcCallback.op)
        return;

    if (isIncrementalGCInProgress())
        return;

    if (gcCallbackDepth == 0) {
        // Save scheduled zone information in case the callback changes it.
        for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
            zone->gcScheduledSaved_ = zone->gcScheduled_;
    }

    gcCallbackDepth++;

    callGCCallback(status);

    MOZ_ASSERT(gcCallbackDepth != 0);
    gcCallbackDepth--;

    if (gcCallbackDepth == 0) {
        // Restore scheduled zone information again.
        for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
            zone->gcScheduled_ = zone->gcScheduledSaved_;
    }
}

/*
 * We disable inlining to ensure that the bottom of the stack with possible GC
 * roots recorded in MarkRuntime excludes any pointers we use during the marking
 * implementation.
 */
MOZ_NEVER_INLINE GCRuntime::IncrementalResult
GCRuntime::gcCycle(bool nonincrementalByAPI, SliceBudget& budget,
    JS::gcreason::Reason reason)
{
    // Note that GC callbacks are allowed to re-enter GC.
    AutoCallGCCallbacks callCallbacks(*this);

    gcstats::AutoGCSlice agc(stats(), scanZonesBeforeGC(), invocationKind, budget, reason);

    if (shouldCollectNurseryForSlice(nonincrementalByAPI, budget))
        minorGC(reason, gcstats::PhaseKind::EVICT_NURSERY_FOR_MAJOR_GC);

    AutoGCSession session(rt, JS::HeapState::MajorCollecting);

    majorGCTriggerReason = JS::gcreason::NO_REASON;

    number++;
    if (!isIncrementalGCInProgress())
        incMajorGcNumber();

    // It's ok if threads other than the main thread have suppressGC set, as
    // they are operating on zones which will not be collected from here.
    MOZ_ASSERT(!rt->mainContextFromOwnThread()->suppressGC);

    // Assert if this is a GC unsafe region.
    rt->mainContextFromOwnThread()->verifyIsSafeToGC();

    {
        gcstats::AutoPhase ap(stats(), gcstats::PhaseKind::WAIT_BACKGROUND_THREAD);

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
        allocTask.cancelAndWait();
    }

    // We don't allow off-thread parsing to start while we're doing an
    // incremental GC of the atoms zone.
    if (rt->activeGCInAtomsZone())
        session.maybeCheckAtomsAccess.emplace(rt);

    auto result = budgetIncrementalGC(nonincrementalByAPI, reason, budget, session);

    // If an ongoing incremental GC was reset, we may need to restart.
    if (result == IncrementalResult::ResetIncremental) {
        MOZ_ASSERT(!isIncrementalGCInProgress());
        return result;
    }

    gcTracer.traceMajorGCStart();

    result = incrementalCollectSlice(budget, reason, session);

    chunkAllocationSinceLastGC = false;

#ifdef JS_GC_ZEAL
    /* Keeping these around after a GC is dangerous. */
    clearSelectedForMarking();
#endif

    gcTracer.traceMajorGCEnd();

    return result;
}

bool
GCRuntime::shouldCollectNurseryForSlice(bool nonincrementalByAPI,
    SliceBudget& budget)
{
    if (!nursery().isEnabled())
        return false;

    switch (incrementalState) {
        case State::NotActive:
        case State::Sweep:
        case State::Finalize:
        case State::Compact:
        case State::Decommit:
            return true;
        case State::Mark:
            return (nonincrementalByAPI ||
                budget.isUnlimited() ||
                lastMarkSlice ||
                nursery().minorGCRequested() ||
                nursery().freeSpace() <
                    tunables.nurseryFreeThresholdForIdleCollection() ||
                hasIncrementalTwoSliceZealMode());
        default:
            // State::MarkRoots can't ever happen here.
            MOZ_CRASH("Unhandled GC state");
    }
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
      case JS::gcreason::TOO_MUCH_WASM_MEMORY:
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
        zoneStats.compartmentCount += zone->compartments().length();
        if (zone->canCollect())
            zoneStats.collectableZoneCount++;
        if (zone->isGCScheduled()) {
            zoneStats.collectedZoneCount++;
            zoneStats.collectedCompartmentCount += zone->compartments().length();
        }
    }

    return zoneStats;
}

// The GC can only clean up scheduledForDestruction realms that were marked live
// by a barrier (e.g. by RemapWrappers from a navigation event). It is also
// common to have realms held live because they are part of a cycle in gecko,
// e.g. involving the HTMLDocument wrapper. In this case, we need to run the
// CycleCollector in order to remove these edges before the realm can be freed.
void
GCRuntime::maybeDoCycleCollection()
{
    const static double ExcessiveGrayRealms = 0.8;
    const static size_t LimitGrayRealms = 200;

    size_t realmsTotal = 0;
    size_t realmsGray = 0;
    for (RealmsIter realm(rt); !realm.done(); realm.next()) {
        ++realmsTotal;
        GlobalObject* global = realm->unsafeUnbarrieredMaybeGlobal();
        if (global && global->isMarkedGray())
            ++realmsGray;
    }
    double grayFraction = double(realmsGray) / double(realmsTotal);
    if (grayFraction > ExcessiveGrayRealms || realmsGray > LimitGrayRealms)
        callDoCycleCollectionCallback(rt->mainContextFromOwnThread());
}

void
GCRuntime::checkCanCallAPI()
{
    MOZ_RELEASE_ASSERT(CurrentThreadCanAccessRuntime(rt));

    /* If we attempt to invoke the GC while we are running in the GC, assert. */
    MOZ_RELEASE_ASSERT(!JS::RuntimeHeapIsBusy());

    MOZ_ASSERT(rt->mainContextFromOwnThread()->isAllocAllowed());
}

bool
GCRuntime::checkIfGCAllowedInCurrentState(JS::gcreason::Reason reason)
{
    if (rt->mainContextFromOwnThread()->suppressGC)
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
    MOZ_ASSERT(!isIncrementalGCInProgress());

    if (!isIncremental)
        return false;

    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        if (c->gcState.scheduledForDestruction)
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

    stats().writeLogMessage("GC starting in state %s",
        StateName(incrementalState));

    AutoTraceLog logGC(TraceLoggerForCurrentThread(), TraceLogger_GC);
    AutoStopVerifyingBarriers av(rt, IsShutdownGC(reason));
    AutoEnqueuePendingParseTasksAfterGC aept(*this);
    AutoScheduleZonesForGC asz(rt);

    bool repeat;
    do {
        IncrementalResult cycleResult = gcCycle(nonincrementalByAPI, budget, reason);

        if (reason == JS::gcreason::ABORT_GC) {
            MOZ_ASSERT(!isIncrementalGCInProgress());
            stats().writeLogMessage("GC aborted by request");
            break;
        }

        /*
         * Sometimes when we finish a GC we need to immediately start a new one.
         * This happens in the following cases:
         *  - when we reset the current GC
         *  - when finalizers drop roots during shutdown
         *  - when zones that we thought were dead at the start of GC are
         *    not collected (see the large comment in beginMarkPhase)
         */
        repeat = false;
        if (!isIncrementalGCInProgress()) {
            if (cycleResult == ResetIncremental) {
                repeat = true;
            } else if (rootsRemoved && IsShutdownGC(reason)) {
                /* Need to re-schedule all zones for GC. */
                JS::PrepareForFullGC(rt->mainContextFromOwnThread());
                repeat = true;
                reason = JS::gcreason::ROOTS_REMOVED;
            } else if (shouldRepeatForDeadZone(reason)) {
                repeat = true;
                reason = JS::gcreason::COMPARTMENT_REVIVED;
            }
        } else if (cycleResult == ReturnToEvictNursery) {
            /* Repeat so we can collect the nursery and run another slice. */
            repeat = true;
        }
    } while (repeat);

    if (reason == JS::gcreason::COMPARTMENT_REVIVED)
        maybeDoCycleCollection();

#ifdef JS_GC_ZEAL
    if (rt->hasZealMode(ZealMode::CheckHeapAfterGC)) {
        gcstats::AutoPhase ap(rt->gc.stats(), gcstats::PhaseKind::TRACE_HEAP);
        CheckHeapAfterGC(rt);
    }
    if (rt->hasZealMode(ZealMode::CheckGrayMarking) && !isIncrementalGCInProgress()) {
        MOZ_RELEASE_ASSERT(CheckGrayMarkingState(rt));
    }
#endif
    stats().writeLogMessage("GC ending");
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
    if (!JS::IsIncrementalGCEnabled(rt->mainContextFromOwnThread())) {
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
    MOZ_ASSERT(!rt->mainContextFromOwnThread()->suppressGC);

    collect(false, SliceBudget::unlimited(), JS::gcreason::ABORT_GC);
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
        JS::PrepareForFullGC(rt->mainContextFromOwnThread());
    invocationKind = gckind;
    collect(false, budget, JS::gcreason::DEBUG_GC);
}

void
GCRuntime::debugGCSlice(SliceBudget& budget)
{
    MOZ_ASSERT(isIncrementalGCInProgress());
    if (!ZonesSelected(rt))
        JS::PrepareForIncrementalGC(rt->mainContextFromOwnThread());
    collect(false, budget, JS::gcreason::DEBUG_GC);
}

/* Schedule a full GC unless a zone will already be collected. */
void
js::PrepareForDebugGC(JSRuntime* rt)
{
    if (!ZonesSelected(rt))
        JS::PrepareForFullGC(rt->mainContextFromOwnThread());
}

void
GCRuntime::onOutOfMallocMemory()
{
    // Stop allocating new chunks.
    allocTask.cancelAndWait();

    // Make sure we release anything queued for release.
    decommitTask.join();

    // Wait for background free of nursery huge slots to finish.
    nursery().waitBackgroundFreeEnd();

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
    freeEmptyChunks(lock);

    // Immediately decommit as many arenas as possible in the hopes that this
    // might let the OS scrape together enough pages to satisfy the failing
    // malloc request.
    decommitAllWithoutUnlocking(lock);
}

void
GCRuntime::minorGC(JS::gcreason::Reason reason, gcstats::PhaseKind phase)
{
    MOZ_ASSERT(!JS::RuntimeHeapIsBusy());

    MOZ_ASSERT_IF(reason == JS::gcreason::EVICT_NURSERY,
                  !rt->mainContextFromOwnThread()->suppressGC);
    if (rt->mainContextFromOwnThread()->suppressGC)
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
    if (--cx->generationalDisabled == 0)
        cx->nursery().enable();
}

JS_PUBLIC_API(bool)
JS::IsGenerationalGCEnabled(JSRuntime* rt)
{
    return !rt->mainContextFromOwnThread()->generationalDisabled;
}

bool
GCRuntime::gcIfRequested()
{
    // This method returns whether a major GC was performed.

    if (nursery().minorGCRequested())
        minorGC(nursery().minorGCTriggerReason());

    if (majorGCRequested()) {
        if (majorGCTriggerReason == JS::gcreason::DELAYED_ATOMS_GC &&
            !rt->mainContextFromOwnThread()->canCollectAtoms())
        {
            // A GC was requested to collect the atoms zone, but it's no longer
            // possible. Skip this collection.
            majorGCTriggerReason = JS::gcreason::NO_REASON;
            return false;
        }

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

    cx->nursery().waitBackgroundFreeEnd();
}

Realm*
js::NewRealm(JSContext* cx, JSPrincipals* principals, const JS::RealmOptions& options)
{
    JSRuntime* rt = cx->runtime();
    JS_AbortIfWrongThread(cx);

    UniquePtr<Zone> zoneHolder;
    UniquePtr<Compartment> compHolder;

    Compartment* comp = nullptr;
    Zone* zone = nullptr;
    JS::CompartmentSpecifier compSpec = options.creationOptions().compartmentSpecifier();
    switch (compSpec) {
      case JS::CompartmentSpecifier::NewCompartmentInSystemZone:
        // systemZone might be null here, in which case we'll make a zone and
        // set this field below.
        zone = rt->gc.systemZone;
        break;
      case JS::CompartmentSpecifier::NewCompartmentInExistingZone:
        zone = options.creationOptions().zone();
        MOZ_ASSERT(zone);
        break;
      case JS::CompartmentSpecifier::ExistingCompartment:
        comp = options.creationOptions().compartment();
        zone = comp->zone();
        break;
      case JS::CompartmentSpecifier::NewCompartmentAndZone:
        break;
    }

    if (!zone) {
        zoneHolder = cx->make_unique<Zone>(cx->runtime());
        if (!zoneHolder)
            return nullptr;

        const JSPrincipals* trusted = rt->trustedPrincipals();
        bool isSystem = principals && principals == trusted;
        if (!zoneHolder->init(isSystem)) {
            ReportOutOfMemory(cx);
            return nullptr;
        }

        zone = zoneHolder.get();
    }

    if (!comp) {
        compHolder = cx->make_unique<JS::Compartment>(zone);
        if (!compHolder || !compHolder->init(cx))
            return nullptr;

        comp = compHolder.get();
    }

    UniquePtr<Realm> realm(cx->new_<Realm>(comp, options));
    if (!realm || !realm->init(cx))
        return nullptr;

    // Set up the principals.
    JS::SetRealmPrincipals(realm.get(), principals);

    AutoLockGC lock(rt);

    // Reserve space in the Vectors before we start mutating them.
    if (!comp->realms().reserve(comp->realms().length() + 1) ||
        (compHolder && !zone->compartments().reserve(zone->compartments().length() + 1)) ||
        (zoneHolder && !rt->gc.zones().reserve(rt->gc.zones().length() + 1)))
    {
        ReportOutOfMemory(cx);
        return nullptr;
    }

    // After this everything must be infallible.

    comp->realms().infallibleAppend(realm.get());

    if (compHolder)
        zone->compartments().infallibleAppend(compHolder.release());

    if (zoneHolder) {
        rt->gc.zones().infallibleAppend(zoneHolder.release());

        // Lazily set the runtime's sytem zone.
        if (compSpec == JS::CompartmentSpecifier::NewCompartmentInSystemZone) {
            MOZ_RELEASE_ASSERT(!rt->gc.systemZone);
            rt->gc.systemZone = zone;
            zone->isSystem = true;
        }
    }

    return realm.release();
}

void
gc::MergeRealms(Realm* source, Realm* target)
{
    JSRuntime* rt = source->runtimeFromMainThread();
    rt->gc.mergeRealms(source, target);

    AutoLockGC lock(rt);
    rt->gc.maybeAllocTriggerZoneGC(target->zone(), lock);
}

void
GCRuntime::mergeRealms(Realm* source, Realm* target)
{
    // The source realm must be specifically flagged as mergable.  This
    // also implies that the realm is not visible to the debugger.
    MOZ_ASSERT(source->creationOptions().mergeable());
    MOZ_ASSERT(source->creationOptions().invisibleToDebugger());

    MOZ_ASSERT(!source->hasBeenEnteredIgnoringJit());
    MOZ_ASSERT(source->zone()->compartments().length() == 1);

    JSContext* cx = rt->mainContextFromOwnThread();

    MOZ_ASSERT(!source->zone()->wasGCStarted());
    JS::AutoAssertNoGC nogc(cx);

    AutoTraceSession session(rt);

    // Cleanup tables and other state in the source realm/zone that will be
    // meaningless after merging into the target realm/zone.

    source->clearTables();
    source->zone()->clearTables();
    source->unsetIsDebuggee();

    // The delazification flag indicates the presence of LazyScripts in a
    // realm for the Debugger API, so if the source realm created LazyScripts,
    // the flag must be propagated to the target realm.
    if (source->needsDelazificationForDebugger())
        target->scheduleDelazificationForDebugger();

    // Release any relocated arenas which we may be holding on to as they might
    // be in the source zone
    releaseHeldRelocatedArenas();

    // Fixup realm pointers in source to refer to target, and make sure
    // type information generations are in sync.

    for (auto script = source->zone()->cellIter<JSScript>(); !script.done(); script.next()) {
        MOZ_ASSERT(script->realm() == source);
        script->realm_ = target;
        script->setTypesGeneration(target->zone()->types.generation);
    }

    GlobalObject* global = target->maybeGlobal();
    MOZ_ASSERT(global);

    for (auto group = source->zone()->cellIter<ObjectGroup>(); !group.done(); group.next()) {
        // Replace placeholder object prototypes with the correct prototype in
        // the target realm.
        TaggedProto proto(group->proto());
        if (proto.isObject()) {
            JSObject* obj = proto.toObject();
            if (GlobalObject::isOffThreadPrototypePlaceholder(obj)) {
                JSObject* targetProto = global->getPrototypeForOffThreadPlaceholder(obj);
                MOZ_ASSERT(targetProto->isDelegate());
                MOZ_ASSERT_IF(targetProto->staticPrototypeIsImmutable(),
                              obj->staticPrototypeIsImmutable());
                MOZ_ASSERT_IF(targetProto->isNewGroupUnknown(),
                              obj->isNewGroupUnknown());
                group->setProtoUnchecked(TaggedProto(targetProto));
            }
        }

        group->setGeneration(target->zone()->types.generation);
        group->realm_ = target;

        // Remove any unboxed layouts from the list in the off thread
        // realm. These do not need to be reinserted in the target
        // realm's list, as the list is not required to be complete.
        if (UnboxedLayout* layout = group->maybeUnboxedLayoutDontCheckGeneration())
            layout->detachFromRealm();
    }

    // Fixup zone pointers in source's zone to refer to target's zone.

    bool targetZoneIsCollecting = isIncrementalGCInProgress() && target->zone()->wasGCStarted();
    for (auto thingKind : AllAllocKinds()) {
        for (ArenaIter aiter(source->zone(), thingKind); !aiter.done(); aiter.next()) {
            Arena* arena = aiter.get();
            arena->zone = target->zone();
            if (MOZ_UNLIKELY(targetZoneIsCollecting)) {
                // If we are currently collecting the target zone then we must
                // treat all merged things as if they were allocated during the
                // collection.
                for (ArenaCellIterUnbarriered iter(arena); !iter.done(); iter.next()) {
                    TenuredCell* cell = iter.getCell();
                    MOZ_ASSERT(!cell->isMarkedAny());
                    cell->markBlack();
                }
            }
        }
    }

    // The source should be the only realm in its zone.
    for (RealmsInZoneIter r(source->zone()); !r.done(); r.next())
        MOZ_ASSERT(r.get() == source);

    // Merge the allocator, stats and UIDs in source's zone into target's zone.
    target->zone()->arenas.adoptArenas(&source->zone()->arenas, targetZoneIsCollecting);
    target->zone()->usage.adopt(source->zone()->usage);
    target->zone()->adoptUniqueIds(source->zone());
    target->zone()->adoptMallocBytes(source->zone());

    // Merge other info in source's zone into target's zone.
    target->zone()->types.typeLifoAlloc().transferFrom(&source->zone()->types.typeLifoAlloc());
    MOZ_RELEASE_ASSERT(source->zone()->types.sweepTypeLifoAlloc.ref().isEmpty());

    // Atoms which are marked in source's zone are now marked in target's zone.
    atomMarking.adoptMarkedAtoms(target->zone(), source->zone());

    // Merge script name maps in the target realm's map.
    if (rt->lcovOutput().isEnabled() && source->scriptNameMap) {
        AutoEnterOOMUnsafeRegion oomUnsafe;

        if (!target->scriptNameMap) {
            target->scriptNameMap = cx->make_unique<ScriptNameMap>();

            if (!target->scriptNameMap)
                oomUnsafe.crash("Failed to create a script name map.");

            if (!target->scriptNameMap->init())
                oomUnsafe.crash("Failed to initialize a script name map.");
        }

        for (ScriptNameMap::Range r = source->scriptNameMap->all(); !r.empty(); r.popFront()) {
            JSScript* key = r.front().key();
            auto value = std::move(r.front().value());
            if (!target->scriptNameMap->putNew(key, std::move(value)))
                oomUnsafe.crash("Failed to add an entry in the script name map.");
        }

        source->scriptNameMap->clear();
    }

    // The source realm is now completely empty, and is the only realm in its
    // compartment, which is the only compartment in its zone. Delete realm,
    // compartment and zone without waiting for this to be cleaned up by a full
    // GC.

    Zone* sourceZone = source->zone();
    sourceZone->deleteEmptyCompartment(source->compartment());
    deleteEmptyZone(sourceZone);
}

void
GCRuntime::runDebugGC()
{
#ifdef JS_GC_ZEAL
    if (rt->mainContextFromOwnThread()->suppressGC)
        return;

    if (hasZealMode(ZealMode::GenerationalGC))
        return minorGC(JS::gcreason::DEBUG_GC);

    PrepareForDebugGC(rt);

    auto budget = SliceBudget::unlimited();
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

        js::gc::State initialState = incrementalState;
        if (!isIncrementalGCInProgress())
            invocationKind = GC_SHRINK;
        collect(false, budget, JS::gcreason::DEBUG_GC);

        /* Reset the slice size when we get to the sweep or compact phases. */
        if ((initialState == State::Mark && incrementalState == State::Sweep) ||
            (initialState == State::Sweep && incrementalState == State::Compact))
        {
            incrementalLimit = zealFrequency / 2;
        }
    } else if (hasIncrementalTwoSliceZealMode()) {
        // These modes trigger incremental GC that happens in two slices and the
        // supplied budget is ignored by incrementalCollectSlice.
        budget = SliceBudget(WorkBudget(1));

        if (!isIncrementalGCInProgress())
            invocationKind = GC_NORMAL;
        collect(false, budget, JS::gcreason::DEBUG_GC);
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
    MOZ_ASSERT(!JS::RuntimeHeapIsMajorCollecting());
    fullCompartmentChecks = enabled;
}

void
GCRuntime::notifyRootsRemoved()
{
    rootsRemoved = true;

#ifdef JS_GC_ZEAL
    /* Schedule a GC to happen "soon". */
    if (hasZealMode(ZealMode::RootsChange))
        nextScheduled = 1;
#endif
}

#ifdef JS_GC_ZEAL
bool
GCRuntime::selectForMarking(JSObject* object)
{
    MOZ_ASSERT(!JS::RuntimeHeapIsMajorCollecting());
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
    MOZ_ASSERT(!JS::RuntimeHeapIsMajorCollecting());
    deterministicOnly = enabled;
}
#endif

#ifdef ENABLE_WASM_GC
/* static */ bool
GCRuntime::temporaryAbortIfWasmGc(JSContext* cx) {
    return cx->options().wasmGc() && cx->suppressGC;
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

    for (ZonesIter zone(fop->runtime(), SkipAtoms); !zone.done(); zone.next()) {
        zone->setPreservingCode(false);
        zone->discardJitCode(fop);
    }
}

void
ArenaLists::adoptArenas(ArenaLists* fromArenaLists, bool targetZoneIsCollecting)
{
    // GC may be active so take the lock here so we can mutate the arena lists.
    AutoLockGC lock(runtime());

    fromArenaLists->clearFreeLists();

    for (auto thingKind : AllAllocKinds()) {
        MOZ_ASSERT(fromArenaLists->concurrentUse(thingKind) == ConcurrentUse::None);
        ArenaList* fromList = &fromArenaLists->arenaLists(thingKind);
        ArenaList* toList = &arenaLists(thingKind);
        fromList->check();
        toList->check();
        Arena* next;
        for (Arena* fromArena = fromList->head(); fromArena; fromArena = next) {
            // Copy fromArena->next before releasing/reinserting.
            next = fromArena->next;

            MOZ_ASSERT(!fromArena->isEmpty());

            // If the target zone is being collected then we need to add the
            // arenas before the cursor because the collector assumes that the
            // cursor is always at the end of the list. This has the side-effect
            // of preventing allocation into any non-full arenas until the end
            // of the next GC.
            if (targetZoneIsCollecting)
                toList->insertBeforeCursor(fromArena);
            else
                toList->insertAtCursor(fromArena);
        }
        fromList->clear();
        toList->check();
    }
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
JS::AssertGCThingIsNotNurseryAllocable(Cell* cell)
{
    MOZ_ASSERT(cell);
    MOZ_ASSERT(!cell->is<JSObject>() && !cell->is<JSString>());
}

JS_FRIEND_API(void)
js::gc::AssertGCThingHasType(js::gc::Cell* cell, JS::TraceKind kind)
{
    if (!cell) {
        MOZ_ASSERT(kind == JS::TraceKind::Null);
        return;
    }

    MOZ_ASSERT(IsCellPointerValid(cell));

    if (IsInsideNursery(cell)) {
        MOZ_ASSERT(kind == (JSString::nurseryCellIsString(cell) ? JS::TraceKind::String
                                                                : JS::TraceKind::Object));
        return;
    }

    MOZ_ASSERT(MapAllocToTraceKind(cell->asTenured().getAllocKind()) == kind);
}
#endif

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED

JS::AutoAssertNoGC::AutoAssertNoGC(JSContext* maybecx)
  : cx_(maybecx ? maybecx : TlsContext.get())
{
    if (cx_)
        cx_->inUnsafeRegion++;
}

JS::AutoAssertNoGC::~AutoAssertNoGC()
{
    if (cx_) {
        MOZ_ASSERT(cx_->inUnsafeRegion > 0);
        cx_->inUnsafeRegion--;
    }
}

#endif // MOZ_DIAGNOSTIC_ASSERT_ENABLED

#ifdef DEBUG

AutoAssertNoNurseryAlloc::AutoAssertNoNurseryAlloc()
{
    TlsContext.get()->disallowNurseryAlloc();
}

AutoAssertNoNurseryAlloc::~AutoAssertNoNurseryAlloc()
{
    TlsContext.get()->allowNurseryAlloc();
}

JS::AutoEnterCycleCollection::AutoEnterCycleCollection(JSRuntime* rt)
  : runtime_(rt)
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));
    MOZ_ASSERT(!JS::RuntimeHeapIsBusy());
    runtime_->heapState_ = HeapState::CycleCollecting;
}

JS::AutoEnterCycleCollection::~AutoEnterCycleCollection()
{
    MOZ_ASSERT(JS::RuntimeHeapIsCycleCollecting());
    runtime_->heapState_ = HeapState::Idle;
}

JS::AutoAssertGCCallback::AutoAssertGCCallback()
  : AutoSuppressGCAnalysis()
{
    MOZ_ASSERT(JS::RuntimeHeapIsCollecting());
}

#endif // DEBUG

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
#ifdef ENABLE_BIGINT
    else if (v.isBigInt())
        ptr = checkedCast(v.toBigInt(), JS::TraceKind::BigInt);
#endif
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

    for (CompartmentsIter c(rt); !c.done(); c.next()) {
        c->checkWrapperMapAfterMovingGC();

        for (RealmsInCompartmentIter r(c); !r.done(); r.next()) {
            r->checkObjectGroupTablesAfterMovingGC();
            r->dtoaCache.checkCacheAfterMovingGC();
            r->checkScriptMapsAfterMovingGC();
            if (r->debugEnvs())
                r->debugEnvs()->checkHashTablesAfterMovingGC();
        }
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
JS::NonIncrementalGC(JSContext* cx, JSGCInvocationKind gckind, gcreason::Reason reason)
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
    UniqueChars cstr = cx->runtime()->gc.stats().renderJsonMessage(timestamp);

    size_t nchars = strlen(cstr.get());
    UniqueTwoByteChars out(js_pod_malloc<char16_t>(nchars + 1));
    if (!out)
        return nullptr;
    out.get()[nchars] = 0;

    CopyAndInflateChars(out.get(), cstr.get(), nchars);
    return out.release();
}

TimeStamp
JS::GCDescription::startTime(JSContext* cx) const
{
    return cx->runtime()->gc.stats().start();
}

TimeStamp
JS::GCDescription::endTime(JSContext* cx) const
{
    return cx->runtime()->gc.stats().end();
}

TimeStamp
JS::GCDescription::lastSliceStart(JSContext* cx) const
{
    return cx->runtime()->gc.stats().slices().back().start;
}

TimeStamp
JS::GCDescription::lastSliceEnd(JSContext* cx) const
{
    return cx->runtime()->gc.stats().slices().back().end;
}

JS::UniqueChars
JS::GCDescription::sliceToJSON(JSContext* cx) const
{
    size_t slices = cx->runtime()->gc.stats().slices().length();
    MOZ_ASSERT(slices > 0);
    return cx->runtime()->gc.stats().renderJsonSlice(slices - 1);
}

JS::UniqueChars
JS::GCDescription::summaryToJSON(JSContext* cx) const
{
    return cx->runtime()->gc.stats().renderJsonMessage(0, false);
}

JS_PUBLIC_API(JS::UniqueChars)
JS::MinorGcToJSON(JSContext* cx)
{
    JSRuntime* rt = cx->runtime();
    return rt->gc.stats().renderNurseryJson(rt);
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
JS::IsIncrementalGCInProgress(JSRuntime* rt)
{
    return rt->gc.isIncrementalGCInProgress() && !rt->gc.isVerifyPreBarriersEnabled();
}

JS_PUBLIC_API(bool)
JS::IsIncrementalBarrierNeeded(JSContext* cx)
{
    if (JS::RuntimeHeapIsBusy())
        return false;

    auto state = cx->runtime()->gc.state();
    return state != gc::State::NotActive && state <= gc::State::Sweep;
}

JS_PUBLIC_API(void)
JS::IncrementalPreWriteBarrier(JSObject* obj)
{
    if (!obj)
        return;

    MOZ_ASSERT(!JS::RuntimeHeapIsMajorCollecting());
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

    MOZ_ASSERT(!JS::RuntimeHeapIsMajorCollecting());
    DispatchTyped(IncrementalReadBarrierFunctor(), thing);
}

JS_PUBLIC_API(bool)
JS::WasIncrementalGC(JSRuntime* rt)
{
    return rt->gc.isIncrementalGc();
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
    args.rval().setNumber(double(cx->zone()->threshold.eagerAllocTrigger(highFrequency)));
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
        if (!JS_DefineProperty(cx, obj, pair.name,
                               getter, nullptr,
                               JSPROP_ENUMERATE))
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
        if (!JS_DefineProperty(cx, zoneObj, pair.name,
                               getter, nullptr,
                               JSPROP_ENUMERATE))
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
AutoAssertEmptyNursery::checkCondition(JSContext* cx) {
    if (!noAlloc)
        noAlloc.emplace();
    this->cx = cx;
    MOZ_ASSERT(cx->nursery().isEmpty());
}

AutoEmptyNursery::AutoEmptyNursery(JSContext* cx)
  : AutoAssertEmptyNursery()
{
    MOZ_ASSERT(!cx->suppressGC);
    cx->runtime()->gc.stats().suspendPhases();
    cx->runtime()->gc.evictNursery(JS::gcreason::EVICT_NURSERY);
    cx->runtime()->gc.stats().resumePhases();
    checkCondition(cx);
}

} /* namespace gc */
} /* namespace js */

#ifdef DEBUG

namespace js {

// We don't want jsfriendapi.h to depend on GenericPrinter,
// so these functions are declared directly in the cpp.

extern JS_FRIEND_API(void)
DumpString(JSString* str, js::GenericPrinter& out);

}

void
js::gc::Cell::dump(js::GenericPrinter& out) const
{
    switch (getTraceKind()) {
      case JS::TraceKind::Object:
        reinterpret_cast<const JSObject*>(this)->dump(out);
        break;

      case JS::TraceKind::String:
          js::DumpString(reinterpret_cast<JSString*>(const_cast<Cell*>(this)), out);
        break;

      case JS::TraceKind::Shape:
        reinterpret_cast<const Shape*>(this)->dump(out);
        break;

      default:
        out.printf("%s(%p)\n", JS::GCTraceKindToAscii(getTraceKind()), (void*) this);
    }
}

// For use in a debugger.
void
js::gc::Cell::dump() const
{
    js::Fprinter out(stderr);
    dump(out);
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
    // 3) When we are not on the runtime's main thread. Helper threads might
    // call this while parsing, and they are not allowed to inspect the
    // runtime's incremental state. The objects being operated on are not able
    // to be collected and will not be marked any color.

    if (!CanCheckGrayBits(cell))
        return false;

    auto tc = &cell->asTenured();
    MOZ_ASSERT(!tc->zoneFromAnyThread()->usedByHelperThread());

    auto rt = tc->runtimeFromMainThread();
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

    if (!CanCheckGrayBits(cell))
        return true;

    // TODO: I'd like to AssertHeapIsIdle() here, but this ends up getting
    // called during GC and while iterating the heap for memory reporting.
    MOZ_ASSERT(!JS::RuntimeHeapIsCycleCollecting());

    auto tc = &cell->asTenured();
    if (!detail::CellIsMarkedGray(tc))
        return true;

    // The cell is gray, but may eventually be marked black if we are in an
    // incremental GC and the cell is reachable by something on the mark stack.

    auto rt = tc->runtimeFromAnyThread();
    if (!rt->gc.isIncrementalGCInProgress() || tc->zone()->wasGCStarted())
        return false;

    Zone* sourceZone = rt->gc.marker.stackContainsCrossZonePointerTo(tc);
    if (sourceZone && sourceZone->wasGCStarted())
        return true;

    return false;
}

extern JS_PUBLIC_API(bool)
js::gc::detail::ObjectIsMarkedBlack(const JSObject* obj)
{
    return obj->isMarkedBlack();
}

#endif

js::gc::ClearEdgesTracer::ClearEdgesTracer()
  : CallbackTracer(TlsContext.get(), TraceWeakMapKeysValues)
{}

template <typename S>
inline void
js::gc::ClearEdgesTracer::clearEdge(S** thingp)
{
    InternalBarrierMethods<S*>::preBarrier(*thingp);
    InternalBarrierMethods<S*>::postBarrier(thingp, *thingp, nullptr);
    *thingp = nullptr;
}

void js::gc::ClearEdgesTracer::onObjectEdge(JSObject** objp) { clearEdge(objp); }
void js::gc::ClearEdgesTracer::onStringEdge(JSString** strp) { clearEdge(strp); }
void js::gc::ClearEdgesTracer::onSymbolEdge(JS::Symbol** symp) { clearEdge(symp); }
#ifdef ENABLE_BIGINT
void js::gc::ClearEdgesTracer::onBigIntEdge(JS::BigInt** bip) { clearEdge(bip); }
#endif
void js::gc::ClearEdgesTracer::onScriptEdge(JSScript** scriptp) { clearEdge(scriptp); }
void js::gc::ClearEdgesTracer::onShapeEdge(js::Shape** shapep) { clearEdge(shapep); }
void js::gc::ClearEdgesTracer::onObjectGroupEdge(js::ObjectGroup** groupp) { clearEdge(groupp); }
void js::gc::ClearEdgesTracer::onBaseShapeEdge(js::BaseShape** basep) { clearEdge(basep); }
void js::gc::ClearEdgesTracer::onJitCodeEdge(js::jit::JitCode** codep) { clearEdge(codep); }
void js::gc::ClearEdgesTracer::onLazyScriptEdge(js::LazyScript** lazyp) { clearEdge(lazyp); }
void js::gc::ClearEdgesTracer::onScopeEdge(js::Scope** scopep) { clearEdge(scopep); }
void js::gc::ClearEdgesTracer::onRegExpSharedEdge(js::RegExpShared** sharedp) { clearEdge(sharedp); }
void js::gc::ClearEdgesTracer::onChild(const JS::GCCellPtr& thing) { MOZ_CRASH(); }
