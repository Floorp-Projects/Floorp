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
 * The atoms zone is only collected in a full GC since objects in any zone may
 * have pointers to atoms, and these are not recorded in the cross compartment
 * pointer map. Also, the atoms zone is not collected if any thread has an
 * AutoKeepAtoms instance on the stack, or there are any exclusive threads using
 * the runtime.
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
 *  - all native objects that have their own trace hook must indicate that they
 *    implement read and write barriers with the JSCLASS_IMPLEMENTS_BARRIERS
 *    flag
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
 *  - MARK_ROOTS - marks the stack and other roots
 *  - MARK       - incrementally marks reachable things
 *  - SWEEP      - sweeps zones in groups and continues marking unswept zones
 *
 * The MARK_ROOTS activity always takes place in the first slice. The next two
 * states can take place over one or more slices.
 *
 * In other words an incremental collection proceeds like this:
 *
 * Slice 1:   MARK_ROOTS: Roots pushed onto the mark stack.
 *            MARK:       The mark stack is processed by popping an element,
 *                        marking it, and pushing its children.
 *
 *          ... JS code runs ...
 *
 * Slice 2:   MARK:       More mark stack processing.
 *
 *          ... JS code runs ...
 *
 * Slice n-1: MARK:       More mark stack processing.
 *
 *          ... JS code runs ...
 *
 * Slice n:   MARK:       Mark stack is completely drained.
 *            SWEEP:      Select first group of zones to sweep and sweep them.
 *
 *          ... JS code runs ...
 *
 * Slice n+1: SWEEP:      Mark objects in unswept zones that were newly
 *                        identified as alive (see below). Then sweep more zone
 *                        groups.
 *
 *          ... JS code runs ...
 *
 * Slice n+2: SWEEP:      Mark objects in unswept zones that were newly
 *                        identified as alive. Then sweep more zone groups.
 *
 *          ... JS code runs ...
 *
 * Slice m:   SWEEP:      Sweeping is finished, and background sweeping
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
 * neither object was marked when we transitioned to the SWEEP phase. Imagine we
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
 */

#include "jsgcinlines.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"

#include <string.h>     /* for memset used when DEBUG */
#ifndef XP_WIN
# include <unistd.h>
#endif

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jscompartment.h"
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
#include "prmjtime.h"

#include "gc/FindSCCs.h"
#include "gc/GCInternals.h"
#include "gc/GCTrace.h"
#include "gc/Marking.h"
#include "gc/Memory.h"
#include "jit/BaselineJIT.h"
#include "jit/IonCode.h"
#include "js/SliceBudget.h"
#include "proxy/DeadObjectProxy.h"
#include "vm/Debugger.h"
#include "vm/ForkJoin.h"
#include "vm/ProxyObject.h"
#include "vm/Shape.h"
#include "vm/String.h"
#include "vm/Symbol.h"
#include "vm/TraceLogging.h"
#include "vm/WrapperObject.h"

#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "vm/Stack-inl.h"
#include "vm/String-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::Maybe;
using mozilla::Swap;

using JS::AutoGCRooter;

/* Perform a Full GC every 20 seconds if MaybeGC is called */
static const uint64_t GC_IDLE_FULL_SPAN = 20 * 1000 * 1000;

/* Increase the IGC marking slice time if we are in highFrequencyGC mode. */
static const int IGC_MARK_SLICE_MULTIPLIER = 2;

const AllocKind gc::slotsToThingKind[] = {
    /* 0 */  FINALIZE_OBJECT0,  FINALIZE_OBJECT2,  FINALIZE_OBJECT2,  FINALIZE_OBJECT4,
    /* 4 */  FINALIZE_OBJECT4,  FINALIZE_OBJECT8,  FINALIZE_OBJECT8,  FINALIZE_OBJECT8,
    /* 8 */  FINALIZE_OBJECT8,  FINALIZE_OBJECT12, FINALIZE_OBJECT12, FINALIZE_OBJECT12,
    /* 12 */ FINALIZE_OBJECT12, FINALIZE_OBJECT16, FINALIZE_OBJECT16, FINALIZE_OBJECT16,
    /* 16 */ FINALIZE_OBJECT16
};

static_assert(JS_ARRAY_LENGTH(slotsToThingKind) == SLOTS_TO_THING_KIND_LIMIT,
              "We have defined a slot count for each kind.");

// Assert that SortedArenaList::MinThingSize is <= the real minimum thing size.
#define CHECK_MIN_THING_SIZE_INNER(x_)                                         \
    static_assert(x_ >= SortedArenaList::MinThingSize,                         \
    #x_ " is less than SortedArenaList::MinThingSize!");
#define CHECK_MIN_THING_SIZE(...) { __VA_ARGS__ }; /* Define the array. */     \
    MOZ_FOR_EACH(CHECK_MIN_THING_SIZE_INNER, (), (__VA_ARGS__ UINT32_MAX))

const uint32_t Arena::ThingSizes[] = CHECK_MIN_THING_SIZE(
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
    sizeof(JSFatInlineString),  /* FINALIZE_FAT_INLINE_STRING   */
    sizeof(JSString),           /* FINALIZE_STRING              */
    sizeof(JSExternalString),   /* FINALIZE_EXTERNAL_STRING     */
    sizeof(JS::Symbol),         /* FINALIZE_SYMBOL              */
    sizeof(jit::JitCode),       /* FINALIZE_JITCODE             */
);

#undef CHECK_MIN_THING_SIZE_INNER
#undef CHECK_MIN_THING_SIZE

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
    OFFSET(JSFatInlineString),  /* FINALIZE_FAT_INLINE_STRING   */
    OFFSET(JSString),           /* FINALIZE_STRING              */
    OFFSET(JSExternalString),   /* FINALIZE_EXTERNAL_STRING     */
    OFFSET(JS::Symbol),         /* FINALIZE_SYMBOL              */
    OFFSET(jit::JitCode),       /* FINALIZE_JITCODE             */
};

#undef OFFSET

const char *
js::gc::TraceKindAsAscii(JSGCTraceKind kind)
{
    switch(kind) {
      case JSTRACE_OBJECT: return "JSTRACE_OBJECT";
      case JSTRACE_STRING: return "JSTRACE_STRING";
      case JSTRACE_SYMBOL: return "JSTRACE_SYMBOL";
      case JSTRACE_SCRIPT: return "JSTRACE_SCRIPT";
      case JSTRACE_LAZY_SCRIPT: return "JSTRACE_SCRIPT";
      case JSTRACE_JITCODE: return "JSTRACE_JITCODE";
      case JSTRACE_SHAPE: return "JSTRACE_SHAPE";
      case JSTRACE_BASE_SHAPE: return "JSTRACE_BASE_SHAPE";
      case JSTRACE_TYPE_OBJECT: return "JSTRACE_TYPE_OBJECT";
      default: return "INVALID";
    }
}

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

static const AllocKind FinalizePhaseJitCode[] = {
    FINALIZE_JITCODE
};

static const AllocKind * const FinalizePhases[] = {
    FinalizePhaseStrings,
    FinalizePhaseScripts,
    FinalizePhaseJitCode
};
static const int FinalizePhaseCount = sizeof(FinalizePhases) / sizeof(AllocKind*);

static const int FinalizePhaseLength[] = {
    sizeof(FinalizePhaseStrings) / sizeof(AllocKind),
    sizeof(FinalizePhaseScripts) / sizeof(AllocKind),
    sizeof(FinalizePhaseJitCode) / sizeof(AllocKind)
};

static const gcstats::Phase FinalizePhaseStatsPhase[] = {
    gcstats::PHASE_SWEEP_STRING,
    gcstats::PHASE_SWEEP_SCRIPT,
    gcstats::PHASE_SWEEP_JITCODE
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

static const AllocKind BackgroundPhaseStringsAndSymbols[] = {
    FINALIZE_FAT_INLINE_STRING,
    FINALIZE_STRING,
    FINALIZE_SYMBOL
};

static const AllocKind BackgroundPhaseShapes[] = {
    FINALIZE_SHAPE,
    FINALIZE_BASE_SHAPE,
    FINALIZE_TYPE_OBJECT
};

static const AllocKind * const BackgroundPhases[] = {
    BackgroundPhaseObjects,
    BackgroundPhaseStringsAndSymbols,
    BackgroundPhaseShapes
};
static const int BackgroundPhaseCount = sizeof(BackgroundPhases) / sizeof(AllocKind*);

static const int BackgroundPhaseLength[] = {
    sizeof(BackgroundPhaseObjects) / sizeof(AllocKind),
    sizeof(BackgroundPhaseStringsAndSymbols) / sizeof(AllocKind),
    sizeof(BackgroundPhaseShapes) / sizeof(AllocKind)
};

template<>
JSObject *
ArenaCellIterImpl::get<JSObject>() const
{
    MOZ_ASSERT(!done());
    return reinterpret_cast<JSObject *>(getCell());
}

#ifdef DEBUG
void
ArenaHeader::checkSynchronizedWithFreeList() const
{
    /*
     * Do not allow to access the free list when its real head is still stored
     * in FreeLists and is not synchronized with this one.
     */
    MOZ_ASSERT(allocated());

    /*
     * We can be called from the background finalization thread when the free
     * list in the zone can mutate at any moment. We cannot do any
     * checks in this case.
     */
    if (IsBackgroundFinalized(getAllocKind()) && zone->runtimeFromAnyThread()->gc.onBackgroundThread())
        return;

    FreeSpan firstSpan = firstFreeSpan.decompact(arenaAddress());
    if (firstSpan.isEmpty())
        return;
    const FreeList *freeList = zone->allocator.arenas.getFreeList(getAllocKind());
    if (freeList->isEmpty() || firstSpan.arenaAddress() != freeList->arenaAddress())
        return;

    /*
     * Here this arena has free things, FreeList::lists[thingKind] is not
     * empty and also points to this arena. Thus they must be the same.
     */
    MOZ_ASSERT(freeList->isSameNonEmptySpan(firstSpan));
}
#endif

void
ArenaHeader::unmarkAll()
{
    uintptr_t *word = chunk()->bitmap.arenaBits(this);
    memset(word, 0, ArenaBitmapWords * sizeof(uintptr_t));
}

/* static */ void
Arena::staticAsserts()
{
    static_assert(JS_ARRAY_LENGTH(ThingSizes) == FINALIZE_LIMIT, "We have defined all thing sizes.");
    static_assert(JS_ARRAY_LENGTH(FirstThingOffsets) == FINALIZE_LIMIT, "We have defined all offsets.");
}

void
Arena::setAsFullyUnused(AllocKind thingKind)
{
    FreeSpan fullSpan;
    size_t thingSize = Arena::thingSize(thingKind);
    fullSpan.initFinal(thingsStart(thingKind), thingsEnd() - thingSize, thingSize);
    aheader.setFirstFreeSpan(&fullSpan);
}

template<typename T>
inline size_t
Arena::finalize(FreeOp *fop, AllocKind thingKind, size_t thingSize)
{
    /* Enforce requirements on size of T. */
    MOZ_ASSERT(thingSize % CellSize == 0);
    MOZ_ASSERT(thingSize <= 255);

    MOZ_ASSERT(aheader.allocated());
    MOZ_ASSERT(thingKind == aheader.getAllocKind());
    MOZ_ASSERT(thingSize == aheader.getThingSize());
    MOZ_ASSERT(!aheader.hasDelayedMarking);
    MOZ_ASSERT(!aheader.markOverflow);
    MOZ_ASSERT(!aheader.allocatedDuringIncremental);

    uintptr_t firstThing = thingsStart(thingKind);
    uintptr_t firstThingOrSuccessorOfLastMarkedThing = firstThing;
    uintptr_t lastThing = thingsEnd() - thingSize;

    FreeSpan newListHead;
    FreeSpan *newListTail = &newListHead;
    size_t nmarked = 0;

    for (ArenaCellIterUnderFinalize i(&aheader); !i.done(); i.next()) {
        T *t = i.get<T>();
        if (t->asTenured().isMarked()) {
            uintptr_t thing = reinterpret_cast<uintptr_t>(t);
            if (thing != firstThingOrSuccessorOfLastMarkedThing) {
                // We just finished passing over one or more free things,
                // so record a new FreeSpan.
                newListTail->initBoundsUnchecked(firstThingOrSuccessorOfLastMarkedThing,
                                                 thing - thingSize);
                newListTail = newListTail->nextSpanUnchecked();
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
        // Do nothing. The caller will update the arena header appropriately.
        MOZ_ASSERT(newListTail == &newListHead);
        JS_EXTRA_POISON(data, JS_SWEPT_TENURED_PATTERN, sizeof(data));
        return nmarked;
    }

    MOZ_ASSERT(firstThingOrSuccessorOfLastMarkedThing != firstThing);
    uintptr_t lastMarkedThing = firstThingOrSuccessorOfLastMarkedThing - thingSize;
    if (lastThing == lastMarkedThing) {
        // If the last thing was marked, we will have already set the bounds of
        // the final span, and we just need to terminate the list.
        newListTail->initAsEmpty();
    } else {
        // Otherwise, end the list with a span that covers the final stretch of free things.
        newListTail->initFinal(firstThingOrSuccessorOfLastMarkedThing, lastThing, thingSize);
    }

#ifdef DEBUG
    size_t nfree = 0;
    for (const FreeSpan *span = &newListHead; !span->isEmpty(); span = span->nextSpan())
        nfree += span->length(thingSize);
    MOZ_ASSERT(nfree + nmarked == thingsPerArena(thingSize));
#endif
    aheader.setFirstFreeSpan(&newListHead);
    return nmarked;
}

template<typename T>
static inline bool
FinalizeTypedArenas(FreeOp *fop,
                    ArenaHeader **src,
                    SortedArenaList &dest,
                    AllocKind thingKind,
                    SliceBudget &budget)
{
    /*
     * Finalize arenas from src list, releasing empty arenas and inserting the
     * others into the appropriate destination size bins.
     */

    /*
     * During parallel sections, we sometimes finalize the parallel arenas,
     * but in that case, we want to hold on to the memory in our arena
     * lists, not offer it up for reuse.
     */
    bool releaseArenas = !InParallelSection();

    size_t thingSize = Arena::thingSize(thingKind);
    size_t thingsPerArena = Arena::thingsPerArena(thingSize);

    while (ArenaHeader *aheader = *src) {
        *src = aheader->next;
        size_t nmarked = aheader->getArena()->finalize<T>(fop, thingKind, thingSize);
        size_t nfree = thingsPerArena - nmarked;

        if (nmarked)
            dest.insertAt(aheader, nfree);
        else if (releaseArenas)
            aheader->chunk()->releaseArena(aheader);
        else
            aheader->chunk()->recycleArena(aheader, dest, thingKind, thingsPerArena);

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
FinalizeArenas(FreeOp *fop,
               ArenaHeader **src,
               SortedArenaList &dest,
               AllocKind thingKind,
               SliceBudget &budget)
{
    switch (thingKind) {
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
      case FINALIZE_FAT_INLINE_STRING:
        return FinalizeTypedArenas<JSFatInlineString>(fop, src, dest, thingKind, budget);
      case FINALIZE_EXTERNAL_STRING:
        return FinalizeTypedArenas<JSExternalString>(fop, src, dest, thingKind, budget);
      case FINALIZE_SYMBOL:
        return FinalizeTypedArenas<JS::Symbol>(fop, src, dest, thingKind, budget);
      case FINALIZE_JITCODE:
      {
        // JitCode finalization may release references on an executable
        // allocator that is accessed when requesting interrupts.
        JSRuntime::AutoLockForInterrupt lock(fop->runtime());
        return FinalizeTypedArenas<jit::JitCode>(fop, src, dest, thingKind, budget);
      }
      default:
        MOZ_CRASH("Invalid alloc kind");
    }
}

static inline Chunk *
AllocChunk(JSRuntime *rt)
{
    return static_cast<Chunk *>(MapAlignedPages(ChunkSize, ChunkSize));
}

static inline void
FreeChunk(JSRuntime *rt, Chunk *p)
{
    UnmapPages(static_cast<void *>(p), ChunkSize);
}

/* Must be called with the GC lock taken. */
inline Chunk *
ChunkPool::get(JSRuntime *rt)
{
    Chunk *chunk = emptyChunkListHead;
    if (!chunk) {
        MOZ_ASSERT(!emptyCount);
        return nullptr;
    }

    MOZ_ASSERT(emptyCount);
    emptyChunkListHead = chunk->info.next;
    --emptyCount;
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

inline Chunk *
ChunkPool::Enum::front()
{
    Chunk *chunk = *chunkp;
    MOZ_ASSERT_IF(chunk, pool.getEmptyCount() != 0);
    return chunk;
}

inline void
ChunkPool::Enum::popFront()
{
    MOZ_ASSERT(!empty());
    chunkp = &front()->info.next;
}

inline void
ChunkPool::Enum::removeAndPopFront()
{
    MOZ_ASSERT(!empty());
    *chunkp = front()->info.next;
    --pool.emptyCount;
}

/* Must be called either during the GC or with the GC lock taken. */
Chunk *
GCRuntime::expireChunkPool(bool shrinkBuffers, bool releaseAll)
{
    /*
     * Return old empty chunks to the system while preserving the order of
     * other chunks in the list. This way, if the GC runs several times
     * without emptying the list, the older chunks will stay at the tail
     * and are more likely to reach the max age.
     */
    Chunk *freeList = nullptr;
    unsigned freeChunkCount = 0;
    for (ChunkPool::Enum e(chunkPool); !e.empty(); ) {
        Chunk *chunk = e.front();
        MOZ_ASSERT(chunk->unused());
        MOZ_ASSERT(!chunkSet.has(chunk));
        if (releaseAll || freeChunkCount >= tunables.maxEmptyChunkCount() ||
            (freeChunkCount >= tunables.minEmptyChunkCount() &&
             (shrinkBuffers || chunk->info.age == MAX_EMPTY_CHUNK_AGE)))
        {
            e.removeAndPopFront();
            prepareToFreeChunk(chunk->info);
            chunk->info.next = freeList;
            freeList = chunk;
        } else {
            /* Keep the chunk but increase its age. */
            ++freeChunkCount;
            ++chunk->info.age;
            e.popFront();
        }
    }
    MOZ_ASSERT(chunkPool.getEmptyCount() <= tunables.maxEmptyChunkCount());
    MOZ_ASSERT_IF(shrinkBuffers, chunkPool.getEmptyCount() <= tunables.minEmptyChunkCount());
    MOZ_ASSERT_IF(releaseAll, chunkPool.getEmptyCount() == 0);
    return freeList;
}

void
GCRuntime::freeChunkList(Chunk *chunkListHead)
{
    while (Chunk *chunk = chunkListHead) {
        MOZ_ASSERT(!chunk->info.numArenasFreeCommitted);
        chunkListHead = chunk->info.next;
        FreeChunk(rt, chunk);
    }
}

void
GCRuntime::expireAndFreeChunkPool(bool releaseAll)
{
    freeChunkList(expireChunkPool(true, releaseAll));
}

/* static */ Chunk *
Chunk::allocate(JSRuntime *rt)
{
    Chunk *chunk = AllocChunk(rt);
    if (!chunk)
        return nullptr;
    chunk->init(rt);
    rt->gc.stats.count(gcstats::STAT_NEW_CHUNK);
    return chunk;
}

/* Must be called with the GC lock taken. */
inline void
GCRuntime::releaseChunk(Chunk *chunk)
{
    MOZ_ASSERT(chunk);
    prepareToFreeChunk(chunk->info);
    FreeChunk(rt, chunk);
}

inline void
GCRuntime::prepareToFreeChunk(ChunkInfo &info)
{
    MOZ_ASSERT(numArenasFreeCommitted >= info.numArenasFreeCommitted);
    numArenasFreeCommitted -= info.numArenasFreeCommitted;
    stats.count(gcstats::STAT_DESTROY_CHUNK);
#ifdef DEBUG
    /*
     * Let FreeChunkList detect a missing prepareToFreeChunk call before it
     * frees chunk.
     */
    info.numArenasFreeCommitted = 0;
#endif
}

void Chunk::decommitAllArenas(JSRuntime *rt)
{
    decommittedArenas.clear(true);
    MarkPagesUnused(&arenas[0], ArenasPerChunk * ArenaSize);

    info.freeArenasHead = nullptr;
    info.lastDecommittedArenaOffset = 0;
    info.numArenasFree = ArenasPerChunk;
    info.numArenasFreeCommitted = 0;
}

void
Chunk::init(JSRuntime *rt)
{
    JS_POISON(this, JS_FRESH_TENURED_PATTERN, ChunkSize);

    /*
     * We clear the bitmap to guard against xpc_IsGrayGCThing being called on
     * uninitialized data, which would happen before the first GC cycle.
     */
    bitmap.clear();

    /*
     * Decommit the arenas. We do this after poisoning so that if the OS does
     * not have to recycle the pages, we still get the benefit of poisoning.
     */
    decommitAllArenas(rt);

    /* Initialize the chunk info. */
    info.age = 0;
    info.trailer.storeBuffer = nullptr;
    info.trailer.location = ChunkLocationBitTenuredHeap;
    info.trailer.runtime = rt;

    /* The rest of info fields are initialized in pickChunk. */
}

inline Chunk **
GCRuntime::getAvailableChunkList(Zone *zone)
{
    return zone->isSystem
           ? &systemAvailableChunkListHead
           : &userAvailableChunkListHead;
}

inline void
Chunk::addToAvailableList(Zone *zone)
{
    JSRuntime *rt = zone->runtimeFromAnyThread();
    insertToAvailableList(rt->gc.getAvailableChunkList(zone));
}

inline void
Chunk::insertToAvailableList(Chunk **insertPoint)
{
    MOZ_ASSERT(hasAvailableArenas());
    MOZ_ASSERT(!info.prevp);
    MOZ_ASSERT(!info.next);
    info.prevp = insertPoint;
    Chunk *insertBefore = *insertPoint;
    if (insertBefore) {
        MOZ_ASSERT(insertBefore->info.prevp == insertPoint);
        insertBefore->info.prevp = &info.next;
    }
    info.next = insertBefore;
    *insertPoint = this;
}

inline void
Chunk::removeFromAvailableList()
{
    MOZ_ASSERT(info.prevp);
    *info.prevp = info.next;
    if (info.next) {
        MOZ_ASSERT(info.next->info.prevp == &info.next);
        info.next->info.prevp = info.prevp;
    }
    info.prevp = nullptr;
    info.next = nullptr;
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
    MOZ_CRASH("No decommitted arenas found.");
}

ArenaHeader *
Chunk::fetchNextDecommittedArena()
{
    MOZ_ASSERT(info.numArenasFreeCommitted == 0);
    MOZ_ASSERT(info.numArenasFree > 0);

    unsigned offset = findDecommittedArenaOffset();
    info.lastDecommittedArenaOffset = offset + 1;
    --info.numArenasFree;
    decommittedArenas.unset(offset);

    Arena *arena = &arenas[offset];
    MarkPagesInUse(arena, ArenaSize);
    arena->aheader.setAsNotAllocated();

    return &arena->aheader;
}

inline void
GCRuntime::updateOnFreeArenaAlloc(const ChunkInfo &info)
{
    MOZ_ASSERT(info.numArenasFreeCommitted <= numArenasFreeCommitted);
    --numArenasFreeCommitted;
}

inline ArenaHeader *
Chunk::fetchNextFreeArena(JSRuntime *rt)
{
    MOZ_ASSERT(info.numArenasFreeCommitted > 0);
    MOZ_ASSERT(info.numArenasFreeCommitted <= info.numArenasFree);

    ArenaHeader *aheader = info.freeArenasHead;
    info.freeArenasHead = aheader->next;
    --info.numArenasFreeCommitted;
    --info.numArenasFree;
    rt->gc.updateOnFreeArenaAlloc(info);

    return aheader;
}

ArenaHeader *
Chunk::allocateArena(Zone *zone, AllocKind thingKind)
{
    MOZ_ASSERT(hasAvailableArenas());

    JSRuntime *rt = zone->runtimeFromAnyThread();
    if (!rt->isHeapMinorCollecting() &&
        !rt->isHeapCompacting() &&
        rt->gc.usage.gcBytes() >= rt->gc.tunables.gcMaxBytes())
    {
#ifdef JSGC_FJGENERATIONAL
        // This is an approximation to the best test, which would check that
        // this thread is currently promoting into the tenured area.  I doubt
        // the better test would make much difference.
        if (!rt->isFJMinorCollecting())
            return nullptr;
#else
        return nullptr;
#endif
    }

    ArenaHeader *aheader = MOZ_LIKELY(info.numArenasFreeCommitted > 0)
                           ? fetchNextFreeArena(rt)
                           : fetchNextDecommittedArena();
    aheader->init(zone, thingKind);
    if (MOZ_UNLIKELY(!hasAvailableArenas()))
        removeFromAvailableList();

    zone->usage.addGCArena();

    if (!rt->isHeapCompacting() && zone->usage.gcBytes() >= zone->threshold.gcTriggerBytes()) {
        AutoUnlockGC unlock(rt);
        rt->gc.triggerZoneGC(zone, JS::gcreason::ALLOC_TRIGGER);
    }

    return aheader;
}

inline void
GCRuntime::updateOnArenaFree(const ChunkInfo &info)
{
    ++numArenasFreeCommitted;
}

inline void
Chunk::addArenaToFreeList(JSRuntime *rt, ArenaHeader *aheader)
{
    MOZ_ASSERT(!aheader->allocated());
    aheader->next = info.freeArenasHead;
    info.freeArenasHead = aheader;
    ++info.numArenasFreeCommitted;
    ++info.numArenasFree;
    rt->gc.updateOnArenaFree(info);
}

void
Chunk::recycleArena(ArenaHeader *aheader, SortedArenaList &dest, AllocKind thingKind,
                    size_t thingsPerArena)
{
    aheader->getArena()->setAsFullyUnused(thingKind);
    dest.insertAt(aheader, thingsPerArena);
}

void
Chunk::releaseArena(ArenaHeader *aheader)
{
    MOZ_ASSERT(aheader->allocated());
    MOZ_ASSERT(!aheader->hasDelayedMarking);
    Zone *zone = aheader->zone;
    JSRuntime *rt = zone->runtimeFromAnyThread();
    AutoLockGC maybeLock;
    if (rt->gc.isBackgroundSweeping())
        maybeLock.lock(rt);

    if (rt->gc.isBackgroundSweeping())
        zone->threshold.updateForRemovedArena(rt->gc.tunables);
    zone->usage.removeGCArena();

    aheader->setAsNotAllocated();
    addArenaToFreeList(rt, aheader);

    if (info.numArenasFree == 1) {
        MOZ_ASSERT(!info.prevp);
        MOZ_ASSERT(!info.next);
        addToAvailableList(zone);
    } else if (!unused()) {
        MOZ_ASSERT(info.prevp);
    } else {
        MOZ_ASSERT(unused());
        removeFromAvailableList();
        decommitAllArenas(rt);
        rt->gc.moveChunkToFreePool(this);
    }
}

void
GCRuntime::moveChunkToFreePool(Chunk *chunk)
{
    MOZ_ASSERT(chunk->unused());
    MOZ_ASSERT(chunkSet.has(chunk));
    chunkSet.remove(chunk);
    chunkPool.put(chunk);
}

inline bool
GCRuntime::wantBackgroundAllocation() const
{
    /*
     * To minimize memory waste we do not want to run the background chunk
     * allocation if we have empty chunks or when the runtime needs just few
     * of them.
     */
    return helperState.canBackgroundAllocate() &&
           chunkPool.getEmptyCount() < tunables.minEmptyChunkCount() &&
           chunkSet.count() >= 4;
}

class js::gc::AutoMaybeStartBackgroundAllocation
{
  private:
    JSRuntime *runtime;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    explicit AutoMaybeStartBackgroundAllocation(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
      : runtime(nullptr)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    void tryToStartBackgroundAllocation(JSRuntime *rt) {
        runtime = rt;
    }

    ~AutoMaybeStartBackgroundAllocation() {
        if (runtime && !runtime->currentThreadOwnsInterruptLock()) {
            AutoLockHelperThreadState helperLock;
            AutoLockGC lock(runtime);
            runtime->gc.startBackgroundAllocationIfIdle();
        }
    }
};

/* The caller must hold the GC lock. */
Chunk *
GCRuntime::pickChunk(Zone *zone, AutoMaybeStartBackgroundAllocation &maybeStartBackgroundAllocation)
{
    Chunk **listHeadp = getAvailableChunkList(zone);
    Chunk *chunk = *listHeadp;
    if (chunk)
        return chunk;

    chunk = chunkPool.get(rt);
    if (!chunk) {
        chunk = Chunk::allocate(rt);
        if (!chunk)
            return nullptr;
        MOZ_ASSERT(chunk->info.numArenasFreeCommitted == 0);
    }

    MOZ_ASSERT(chunk->unused());
    MOZ_ASSERT(!chunkSet.has(chunk));

    if (wantBackgroundAllocation())
        maybeStartBackgroundAllocation.tryToStartBackgroundAllocation(rt);

    chunkAllocationSinceLastGC = true;

    /*
     * FIXME bug 583732 - chunk is newly allocated and cannot be present in
     * the table so using ordinary lookupForAdd is suboptimal here.
     */
    GCChunkSet::AddPtr p = chunkSet.lookupForAdd(chunk);
    MOZ_ASSERT(!p);
    if (!chunkSet.add(p, chunk)) {
        releaseChunk(chunk);
        return nullptr;
    }

    chunk->info.prevp = nullptr;
    chunk->info.next = nullptr;
    chunk->addToAvailableList(zone);

    return chunk;
}

GCRuntime::GCRuntime(JSRuntime *rt) :
    rt(rt),
    systemZone(nullptr),
#ifdef JSGC_GENERATIONAL
    nursery(rt),
    storeBuffer(rt, nursery),
#endif
    stats(rt),
    marker(rt),
    usage(nullptr),
    systemAvailableChunkListHead(nullptr),
    userAvailableChunkListHead(nullptr),
    maxMallocBytes(0),
    numArenasFreeCommitted(0),
    verifyPreData(nullptr),
    verifyPostData(nullptr),
    chunkAllocationSinceLastGC(false),
    nextFullGCTime(0),
    lastGCTime(0),
    mode(JSGC_MODE_INCREMENTAL),
    decommitThreshold(32 * 1024 * 1024),
    cleanUpEverything(false),
    grayBitsValid(false),
    isNeeded(0),
    majorGCNumber(0),
    jitReleaseNumber(0),
    number(0),
    startNumber(0),
    isFull(false),
    triggerReason(JS::gcreason::NO_REASON),
#ifdef DEBUG
    disableStrictProxyCheckingCount(0),
#endif
    incrementalState(gc::NO_INCREMENTAL),
    lastMarkSlice(false),
    sweepOnBackgroundThread(false),
    foundBlackGrayEdges(false),
    sweepingZones(nullptr),
    zoneGroupIndex(0),
    zoneGroups(nullptr),
    currentZoneGroup(nullptr),
    sweepZone(nullptr),
    sweepKindIndex(0),
    abortSweepAfterCurrentGroup(false),
    arenasAllocatedDuringSweep(nullptr),
#ifdef JS_GC_MARKING_VALIDATION
    markingValidator(nullptr),
#endif
    interFrameGC(0),
    sliceBudget(SliceBudget::Unlimited),
    incrementalAllowed(true),
    generationalDisabled(0),
#ifdef JSGC_COMPACTING
    compactingDisabled(0),
#endif
    manipulatingDeadZones(false),
    objectsMarkedInDeadZones(0),
    poked(false),
    heapState(Idle),
#ifdef JS_GC_ZEAL
    zealMode(0),
    zealFrequency(0),
    nextScheduled(0),
    deterministicOnly(false),
    incrementalLimit(0),
#endif
    validate(true),
    fullCompartmentChecks(false),
    mallocBytes(0),
    mallocGCTriggered(false),
#ifdef DEBUG
    inUnsafeRegion(0),
#endif
    alwaysPreserveCode(false),
#ifdef DEBUG
    noGCOrAllocationCheck(0),
#endif
    lock(nullptr),
    lockOwner(nullptr),
    helperState(rt)
{
    setGCMode(JSGC_MODE_GLOBAL);
}

#ifdef JS_GC_ZEAL

void
GCRuntime::setZeal(uint8_t zeal, uint32_t frequency)
{
    if (verifyPreData)
        VerifyBarriers(rt, PreBarrierVerifier);
    if (verifyPostData)
        VerifyBarriers(rt, PostBarrierVerifier);

#ifdef JSGC_GENERATIONAL
    if (zealMode == ZealGenerationalGCValue) {
        evictNursery(JS::gcreason::DEBUG_GC);
        nursery.leaveZealMode();
    }

    if (zeal == ZealGenerationalGCValue)
        nursery.enterZealMode();
#endif

    bool schedule = zeal >= js::gc::ZealAllocValue;
    zealMode = zeal;
    zealFrequency = frequency;
    nextScheduled = schedule ? frequency : 0;
}

void
GCRuntime::setNextScheduled(uint32_t count)
{
    nextScheduled = count;
}

bool
GCRuntime::initZeal()
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
                "  7: Collect the nursery every N nursery allocations\n"
                "  8: Incremental GC in two slices: 1) mark roots 2) finish collection\n"
                "  9: Incremental GC in two slices: 1) mark all 2) new marking and finish\n"
                " 10: Incremental GC in multiple slices\n"
                " 11: Verify post write barriers between instructions\n"
                " 12: Verify post write barriers between paints\n"
                " 13: Purge analysis state every F allocations (default: 100)\n");
        return false;
    }

    setZeal(zeal, frequency);
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

    lock = PR_NewLock();
    if (!lock)
        return false;

    if (!chunkSet.init(INITIAL_CHUNK_CAPACITY))
        return false;

    if (!rootsHash.init(256))
        return false;

    if (!helperState.init())
        return false;

    /*
     * Separate gcMaxMallocBytes from gcMaxBytes but initialize to maxbytes
     * for default backward API compatibility.
     */
    tunables.setParameter(JSGC_MAX_BYTES, maxbytes);
    setMaxMallocBytes(maxbytes);

    jitReleaseNumber = majorGCNumber + JIT_SCRIPT_RELEASE_TYPES_PERIOD;

#ifdef JSGC_GENERATIONAL
    if (!nursery.init(maxNurseryBytes))
        return false;

    if (!nursery.isEnabled()) {
        MOZ_ASSERT(nursery.nurserySize() == 0);
        ++rt->gc.generationalDisabled;
    } else {
        MOZ_ASSERT(nursery.nurserySize() > 0);
        if (!storeBuffer.enable())
            return false;
    }
#endif

#ifdef JS_GC_ZEAL
    if (!initZeal())
        return false;
#endif

    if (!InitTrace(*this))
        return false;

    if (!marker.init(mode))
        return false;

    return true;
}

void
GCRuntime::recordNativeStackTop()
{
    /* Record the stack top here only if we are called from a request. */
    if (!rt->requestDepth)
        return;
    conservativeGC.recordStackTop();
}

void
GCRuntime::finish()
{
    /*
     * Wait until the background finalization stops and the helper thread
     * shuts down before we forcefully release any remaining GC memory.
     */
    helperState.finish();

#ifdef JS_GC_ZEAL
    /* Free memory associated with GC verification. */
    finishVerifier();
#endif

    /* Delete all remaining zones. */
    if (rt->gcInitialized) {
        for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
            for (CompartmentsInZoneIter comp(zone); !comp.done(); comp.next())
                js_delete(comp.get());
            js_delete(zone.get());
        }
    }

    zones.clear();

    systemAvailableChunkListHead = nullptr;
    userAvailableChunkListHead = nullptr;
    if (chunkSet.initialized()) {
        for (GCChunkSet::Range r(chunkSet.all()); !r.empty(); r.popFront())
            releaseChunk(r.front());
        chunkSet.clear();
    }

    expireAndFreeChunkPool(true);

    if (rootsHash.initialized())
        rootsHash.clear();

    FinishPersistentRootedChains(rt);

    if (lock) {
        PR_DestroyLock(lock);
        lock = nullptr;
    }

    FinishTrace();
}

void
js::gc::FinishPersistentRootedChains(JSRuntime *rt)
{
    /* The lists of persistent roots are stored on the shadow runtime. */
    rt->functionPersistentRooteds.clear();
    rt->idPersistentRooteds.clear();
    rt->objectPersistentRooteds.clear();
    rt->scriptPersistentRooteds.clear();
    rt->stringPersistentRooteds.clear();
    rt->valuePersistentRooteds.clear();
}

void
GCRuntime::setParameter(JSGCParamKey key, uint32_t value)
{
    switch (key) {
      case JSGC_MAX_MALLOC_BYTES:
        setMaxMallocBytes(value);
        break;
      case JSGC_SLICE_TIME_BUDGET:
        sliceBudget = SliceBudget::TimeBudget(value);
        break;
      case JSGC_MARK_STACK_LIMIT:
        setMarkStackLimit(value);
        break;
      case JSGC_DECOMMIT_THRESHOLD:
        decommitThreshold = value * 1024 * 1024;
        break;
      case JSGC_MODE:
        mode = JSGCMode(value);
        MOZ_ASSERT(mode == JSGC_MODE_GLOBAL ||
                   mode == JSGC_MODE_COMPARTMENT ||
                   mode == JSGC_MODE_INCREMENTAL);
        break;
      default:
        tunables.setParameter(key, value);
    }
}

void
GCSchedulingTunables::setParameter(JSGCParamKey key, uint32_t value)
{
    switch(key) {
      case JSGC_MAX_BYTES:
        gcMaxBytes_ = value;
        break;
      case JSGC_HIGH_FREQUENCY_TIME_LIMIT:
        highFrequencyThresholdUsec_ = value * PRMJ_USEC_PER_MSEC;
        break;
      case JSGC_HIGH_FREQUENCY_LOW_LIMIT:
        highFrequencyLowLimitBytes_ = value * 1024 * 1024;
        if (highFrequencyLowLimitBytes_ >= highFrequencyHighLimitBytes_)
            highFrequencyHighLimitBytes_ = highFrequencyLowLimitBytes_ + 1;
        MOZ_ASSERT(highFrequencyHighLimitBytes_ > highFrequencyLowLimitBytes_);
        break;
      case JSGC_HIGH_FREQUENCY_HIGH_LIMIT:
        MOZ_ASSERT(value > 0);
        highFrequencyHighLimitBytes_ = value * 1024 * 1024;
        if (highFrequencyHighLimitBytes_ <= highFrequencyLowLimitBytes_)
            highFrequencyLowLimitBytes_ = highFrequencyHighLimitBytes_ - 1;
        MOZ_ASSERT(highFrequencyHighLimitBytes_ > highFrequencyLowLimitBytes_);
        break;
      case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX:
        highFrequencyHeapGrowthMax_ = value / 100.0;
        MOZ_ASSERT(highFrequencyHeapGrowthMax_ / 0.85 > 1.0);
        break;
      case JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN:
        highFrequencyHeapGrowthMin_ = value / 100.0;
        MOZ_ASSERT(highFrequencyHeapGrowthMin_ / 0.85 > 1.0);
        break;
      case JSGC_LOW_FREQUENCY_HEAP_GROWTH:
        lowFrequencyHeapGrowth_ = value / 100.0;
        MOZ_ASSERT(lowFrequencyHeapGrowth_ / 0.9 > 1.0);
        break;
      case JSGC_DYNAMIC_HEAP_GROWTH:
        dynamicHeapGrowthEnabled_ = value;
        break;
      case JSGC_DYNAMIC_MARK_SLICE:
        dynamicMarkSliceEnabled_ = value;
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
      default:
        MOZ_CRASH("Unknown GC parameter.");
    }
}

uint32_t
GCRuntime::getParameter(JSGCParamKey key)
{
    switch (key) {
      case JSGC_MAX_BYTES:
        return uint32_t(tunables.gcMaxBytes());
      case JSGC_MAX_MALLOC_BYTES:
        return maxMallocBytes;
      case JSGC_BYTES:
        return uint32_t(usage.gcBytes());
      case JSGC_MODE:
        return uint32_t(mode);
      case JSGC_UNUSED_CHUNKS:
        return uint32_t(chunkPool.getEmptyCount());
      case JSGC_TOTAL_CHUNKS:
        return uint32_t(chunkSet.count() + chunkPool.getEmptyCount());
      case JSGC_SLICE_TIME_BUDGET:
        return uint32_t(sliceBudget > 0 ? sliceBudget / PRMJ_USEC_PER_MSEC : 0);
      case JSGC_MARK_STACK_LIMIT:
        return marker.maxCapacity();
      case JSGC_HIGH_FREQUENCY_TIME_LIMIT:
        return tunables.highFrequencyThresholdUsec();
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
        return tunables.minEmptyChunkCount();
      case JSGC_MAX_EMPTY_CHUNK_COUNT:
        return tunables.maxEmptyChunkCount();
      default:
        MOZ_ASSERT(key == JSGC_NUMBER);
        return uint32_t(number);
    }
}

void
GCRuntime::setMarkStackLimit(size_t limit)
{
    MOZ_ASSERT(!isHeapBusy());
    AutoStopVerifyingBarriers pauseVerification(rt, false);
    marker.setMaxCapacity(limit);
}

template <typename T> struct BarrierOwner {};
template <typename T> struct BarrierOwner<T *> { typedef T result; };
template <> struct BarrierOwner<Value> { typedef HeapValue result; };

bool
GCRuntime::addBlackRootsTracer(JSTraceDataOp traceOp, void *data)
{
    AssertHeapIsIdle(rt);
    return !!blackRootTracers.append(Callback<JSTraceDataOp>(traceOp, data));
}

void
GCRuntime::removeBlackRootsTracer(JSTraceDataOp traceOp, void *data)
{
    // Can be called from finalizers
    for (size_t i = 0; i < blackRootTracers.length(); i++) {
        Callback<JSTraceDataOp> *e = &blackRootTracers[i];
        if (e->op == traceOp && e->data == data) {
            blackRootTracers.erase(e);
        }
    }
}

void
GCRuntime::setGrayRootsTracer(JSTraceDataOp traceOp, void *data)
{
    AssertHeapIsIdle(rt);
    grayRootTracer.op = traceOp;
    grayRootTracer.data = data;
}

void
GCRuntime::setGCCallback(JSGCCallback callback, void *data)
{
    gcCallback.op = callback;
    gcCallback.data = data;
}

bool
GCRuntime::addFinalizeCallback(JSFinalizeCallback callback, void *data)
{
    return finalizeCallbacks.append(Callback<JSFinalizeCallback>(callback, data));
}

void
GCRuntime::removeFinalizeCallback(JSFinalizeCallback callback)
{
    for (Callback<JSFinalizeCallback> *p = finalizeCallbacks.begin();
         p < finalizeCallbacks.end(); p++)
    {
        if (p->op == callback) {
            finalizeCallbacks.erase(p);
            break;
        }
    }
}

void
GCRuntime::callFinalizeCallbacks(FreeOp *fop, JSFinalizeStatus status) const
{
    for (const Callback<JSFinalizeCallback> *p = finalizeCallbacks.begin();
         p < finalizeCallbacks.end(); p++)
    {
        p->op(fop, status, !isFull, p->data);
    }
}

bool
GCRuntime::addWeakPointerCallback(JSWeakPointerCallback callback, void *data)
{
    return updateWeakPointerCallbacks.append(Callback<JSWeakPointerCallback>(callback, data));
}

void
GCRuntime::removeWeakPointerCallback(JSWeakPointerCallback callback)
{
    for (Callback<JSWeakPointerCallback> *p = updateWeakPointerCallbacks.begin();
         p < updateWeakPointerCallbacks.end(); p++)
    {
        if (p->op == callback) {
            updateWeakPointerCallbacks.erase(p);
            break;
        }
    }
}

void
GCRuntime::callWeakPointerCallbacks() const
{
    for (const Callback<JSWeakPointerCallback> *p = updateWeakPointerCallbacks.begin();
         p < updateWeakPointerCallbacks.end(); p++)
    {
        p->op(rt, p->data);
    }
}

JS::GCSliceCallback
GCRuntime::setSliceCallback(JS::GCSliceCallback callback) {
    return stats.setSliceCallback(callback);
}

template <typename T>
bool
GCRuntime::addRoot(T *rp, const char *name, JSGCRootType rootType)
{
    /*
     * Sometimes Firefox will hold weak references to objects and then convert
     * them to strong references by calling AddRoot (e.g., via PreserveWrapper,
     * or ModifyBusyCount in workers). We need a read barrier to cover these
     * cases.
     */
    if (rt->gc.incrementalState != NO_INCREMENTAL)
        BarrierOwner<T>::result::writeBarrierPre(*rp);

    return rt->gc.rootsHash.put((void *)rp, RootInfo(name, rootType));
}

void
GCRuntime::removeRoot(void *rp)
{
    rootsHash.remove(rp);
    poke();
}

template <typename T>
static bool
AddRoot(JSRuntime *rt, T *rp, const char *name, JSGCRootType rootType)
{
    return rt->gc.addRoot(rp, name, rootType);
}

template <typename T>
static bool
AddRoot(JSContext *cx, T *rp, const char *name, JSGCRootType rootType)
{
    bool ok = cx->runtime()->gc.addRoot(rp, name, rootType);
    if (!ok)
        JS_ReportOutOfMemory(cx);
    return ok;
}

bool
js::AddValueRoot(JSContext *cx, Value *vp, const char *name)
{
    return AddRoot(cx, vp, name, JS_GC_ROOT_VALUE_PTR);
}

extern bool
js::AddValueRootRT(JSRuntime *rt, js::Value *vp, const char *name)
{
    return AddRoot(rt, vp, name, JS_GC_ROOT_VALUE_PTR);
}

extern bool
js::AddStringRoot(JSContext *cx, JSString **rp, const char *name)
{
    return AddRoot(cx, rp, name, JS_GC_ROOT_STRING_PTR);
}

extern bool
js::AddObjectRoot(JSContext *cx, JSObject **rp, const char *name)
{
    return AddRoot(cx, rp, name, JS_GC_ROOT_OBJECT_PTR);
}

extern bool
js::AddObjectRoot(JSRuntime *rt, JSObject **rp, const char *name)
{
    return AddRoot(rt, rp, name, JS_GC_ROOT_OBJECT_PTR);
}

extern bool
js::AddScriptRoot(JSContext *cx, JSScript **rp, const char *name)
{
    return AddRoot(cx, rp, name, JS_GC_ROOT_SCRIPT_PTR);
}

extern JS_FRIEND_API(bool)
js::AddRawValueRoot(JSContext *cx, Value *vp, const char *name)
{
    return AddRoot(cx, vp, name, JS_GC_ROOT_VALUE_PTR);
}

extern JS_FRIEND_API(void)
js::RemoveRawValueRoot(JSContext *cx, Value *vp)
{
    RemoveRoot(cx->runtime(), vp);
}

void
js::RemoveRoot(JSRuntime *rt, void *rp)
{
    rt->gc.removeRoot(rp);
}

void
GCRuntime::setMaxMallocBytes(size_t value)
{
    /*
     * For compatibility treat any value that exceeds PTRDIFF_T_MAX to
     * mean that value.
     */
    maxMallocBytes = (ptrdiff_t(value) >= 0) ? value : size_t(-1) >> 1;
    resetMallocBytes();
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
        zone->setGCMaxMallocBytes(value);
}

void
GCRuntime::resetMallocBytes()
{
    mallocBytes = ptrdiff_t(maxMallocBytes);
    mallocGCTriggered = false;
}

void
GCRuntime::updateMallocCounter(JS::Zone *zone, size_t nbytes)
{
    mallocBytes -= ptrdiff_t(nbytes);
    if (MOZ_UNLIKELY(isTooMuchMalloc()))
        onTooMuchMalloc();
    else if (zone)
        zone->updateMallocCounter(nbytes);
}

void
GCRuntime::onTooMuchMalloc()
{
    if (!mallocGCTriggered)
        mallocGCTriggered = triggerGC(JS::gcreason::TOO_MUCH_MALLOC);
}

/* static */ double
ZoneHeapThreshold::computeZoneHeapGrowthFactorForHeapSize(size_t lastBytes,
                                                          const GCSchedulingTunables &tunables,
                                                          const GCSchedulingState &state)
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
                                           const GCSchedulingTunables &tunables)
{
    size_t base = gckind == GC_SHRINK
                ? lastBytes
                : Max(lastBytes, tunables.gcZoneAllocThresholdBase());
    double trigger = double(base) * growthFactor;
    return size_t(Min(double(tunables.gcMaxBytes()), trigger));
}

void
ZoneHeapThreshold::updateAfterGC(size_t lastBytes, JSGCInvocationKind gckind,
                                 const GCSchedulingTunables &tunables,
                                 const GCSchedulingState &state)
{
    gcHeapGrowthFactor_ = computeZoneHeapGrowthFactorForHeapSize(lastBytes, tunables, state);
    gcTriggerBytes_ = computeZoneTriggerBytes(gcHeapGrowthFactor_, lastBytes, gckind, tunables);
}

void
ZoneHeapThreshold::updateForRemovedArena(const GCSchedulingTunables &tunables)
{
    size_t amount = ArenaSize * gcHeapGrowthFactor_;

    MOZ_ASSERT(amount > 0);
    MOZ_ASSERT(gcTriggerBytes_ >= amount);

    if (gcTriggerBytes_ - amount < tunables.gcZoneAllocThresholdBase() * gcHeapGrowthFactor_)
        return;

    gcTriggerBytes_ -= amount;
}

Allocator::Allocator(Zone *zone)
  : zone_(zone)
{}

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
    const TenuredCell *cell = TenuredCell::fromPointer(thing);
    cell->arenaHeader()->markOverflow = 1;
    delayMarkingArena(cell->arenaHeader());
}

inline void
ArenaLists::prepareForIncrementalGC(JSRuntime *rt)
{
    for (size_t i = 0; i != FINALIZE_LIMIT; ++i) {
        FreeList *freeList = &freeLists[i];
        if (!freeList->isEmpty()) {
            ArenaHeader *aheader = freeList->arenaHeader();
            aheader->allocatedDuringIncremental = true;
            rt->gc.marker.delayMarkingArena(aheader);
        }
    }
}

inline void
GCRuntime::arenaAllocatedDuringGC(JS::Zone *zone, ArenaHeader *arena)
{
    if (zone->needsIncrementalBarrier()) {
        arena->allocatedDuringIncremental = true;
        marker.delayMarkingArena(arena);
    } else if (zone->isGCSweeping()) {
        arena->setNextAllocDuringSweep(arenasAllocatedDuringSweep);
        arenasAllocatedDuringSweep = arena;
    }
}

inline void *
ArenaLists::allocateFromArenaInline(Zone *zone, AllocKind thingKind,
                                    AutoMaybeStartBackgroundAllocation &maybeStartBackgroundAllocation)
{
    /*
     * Parallel JS Note:
     *
     * This function can be called from parallel threads all of which
     * are associated with the same compartment. In that case, each
     * thread will have a distinct ArenaLists.  Therefore, whenever we
     * fall through to pickChunk() we must be sure that we are holding
     * a lock.
     */

    AutoLockGC maybeLock;

    bool backgroundFinalizationIsRunning = false;
    ArenaLists::BackgroundFinalizeState *bfs = &backgroundFinalizeState[thingKind];
    if (*bfs != BFS_DONE) {
        /*
         * We cannot search the arena list for free things while background
         * finalization runs and can modify it at any moment. So we always
         * allocate a new arena in that case.
         */
        JSRuntime *rt = zone->runtimeFromAnyThread();
        maybeLock.lock(rt);
        if (*bfs == BFS_RUN) {
            backgroundFinalizationIsRunning = true;
        } else if (*bfs == BFS_JUST_FINISHED) {
            /* See comments before BackgroundFinalizeState definition. */
            *bfs = BFS_DONE;
        } else {
            MOZ_ASSERT(*bfs == BFS_DONE);
        }
    }

    ArenaHeader *aheader;
    ArenaList *al = &arenaLists[thingKind];
    if (!backgroundFinalizationIsRunning && (aheader = al->arenaAfterCursor())) {
        /*
         * Normally, the empty arenas are returned to the chunk
         * and should not be present on the list. In parallel
         * execution, however, we keep empty arenas in the arena
         * list to avoid synchronizing on the chunk.
         */
        MOZ_ASSERT(!aheader->isEmpty() || InParallelSection());

        al->moveCursorPast(aheader);

        /*
         * Move the free span stored in the arena to the free list and
         * allocate from it.
         */
        FreeSpan firstFreeSpan = aheader->getFirstFreeSpan();
        freeLists[thingKind].setHead(&firstFreeSpan);
        aheader->setAsFullyUsed();
        if (MOZ_UNLIKELY(zone->wasGCStarted()))
            zone->runtimeFromMainThread()->gc.arenaAllocatedDuringGC(zone, aheader);
        void *thing = freeLists[thingKind].allocate(Arena::thingSize(thingKind));
        MOZ_ASSERT(thing);   // This allocation is infallible.
        return thing;
    }

    /* Make sure we hold the GC lock before we call pickChunk. */
    JSRuntime *rt = zone->runtimeFromAnyThread();
    if (!maybeLock.locked())
        maybeLock.lock(rt);
    Chunk *chunk = rt->gc.pickChunk(zone, maybeStartBackgroundAllocation);
    if (!chunk)
        return nullptr;

    /*
     * While we still hold the GC lock get an arena from some chunk, mark it
     * as full as its single free span is moved to the free lists, and insert
     * it to the list as a fully allocated arena.
     */
    MOZ_ASSERT(al->isCursorAtEnd());
    aheader = chunk->allocateArena(zone, thingKind);
    if (!aheader)
        return nullptr;

    if (MOZ_UNLIKELY(zone->wasGCStarted()))
        rt->gc.arenaAllocatedDuringGC(zone, aheader);
    al->insertAtCursor(aheader);

    /*
     * Allocate from a newly allocated arena. The arena will have been set up
     * as fully used during the initialization so we have to re-mark it as
     * empty before allocating.
     */
    MOZ_ASSERT(!aheader->hasFreeThings());
    Arena *arena = aheader->getArena();
    size_t thingSize = Arena::thingSize(thingKind);
    FreeSpan fullSpan;
    fullSpan.initFinal(arena->thingsStart(thingKind), arena->thingsEnd() - thingSize, thingSize);
    freeLists[thingKind].setHead(&fullSpan);
    return freeLists[thingKind].allocate(thingSize);
}

void *
ArenaLists::allocateFromArena(JS::Zone *zone, AllocKind thingKind)
{
    AutoMaybeStartBackgroundAllocation maybeStartBackgroundAllocation;
    return allocateFromArenaInline(zone, thingKind, maybeStartBackgroundAllocation);
}

void
ArenaLists::wipeDuringParallelExecution(JSRuntime *rt)
{
    MOZ_ASSERT(InParallelSection());

    // First, check that we all objects we have allocated are eligible
    // for background finalization. The idea is that we will free
    // (below) ALL background finalizable objects, because we know (by
    // the rules of parallel execution) they are not reachable except
    // by other thread-local objects. However, if there were any
    // object ineligible for background finalization, it might retain
    // a reference to one of these background finalizable objects, and
    // that'd be bad.
    for (unsigned i = 0; i < FINALIZE_LAST; i++) {
        AllocKind thingKind = AllocKind(i);
        if (!IsBackgroundFinalized(thingKind) && !arenaLists[thingKind].isEmpty())
            return;
    }

    // Finalize all background finalizable objects immediately and
    // return the (now empty) arenas back to arena list.
    FreeOp fop(rt);
    for (unsigned i = 0; i < FINALIZE_OBJECT_LAST; i++) {
        AllocKind thingKind = AllocKind(i);

        if (!IsBackgroundFinalized(thingKind))
            continue;

        if (!arenaLists[i].isEmpty()) {
            purge(thingKind);
            forceFinalizeNow(&fop, thingKind);
        }
    }
}

/* Compacting GC */

bool
GCRuntime::shouldCompact()
{
#ifdef JSGC_COMPACTING
    return invocationKind == GC_SHRINK && !compactingDisabled;
#else
    return false;
#endif
}

#ifdef JSGC_COMPACTING

void
GCRuntime::disableCompactingGC()
{
    ++rt->gc.compactingDisabled;
}

void
GCRuntime::enableCompactingGC()
{
    MOZ_ASSERT(compactingDisabled > 0);
    --compactingDisabled;
}

AutoDisableCompactingGC::AutoDisableCompactingGC(JSRuntime *rt)
  : gc(rt->gc)
{
    gc.disableCompactingGC();
}

AutoDisableCompactingGC::~AutoDisableCompactingGC()
{
    gc.enableCompactingGC();
}

static bool
ArenaContainsGlobal(ArenaHeader *arena)
{
    if (arena->getAllocKind() > FINALIZE_OBJECT_LAST)
        return false;

    for (ArenaCellIterUnderGC i(arena); !i.done(); i.next()) {
        JSObject *obj = i.get<JSObject>();
        if (obj->is<GlobalObject>())
            return true;
    }

    return false;
}

static bool
CanRelocateArena(ArenaHeader *arena)
{
    /*
     * We can't currently move global objects because their address can be baked
     * into compiled code so we skip relocation of any area containing one.
     */
    JSRuntime *rt = arena->zone->runtimeFromMainThread();
    return arena->getAllocKind() <= FINALIZE_OBJECT_LAST && !ArenaContainsGlobal(arena);
}

static bool
ShouldRelocateArena(ArenaHeader *arena)
{
#ifdef JS_GC_ZEAL
    if (arena->zone->runtimeFromMainThread()->gc.zeal() == ZealCompactValue)
        return true;
#endif

    /*
     * Eventually, this will be based on brilliant heuristics that look at fill
     * percentage and fragmentation and... stuff.
     */
    return arena->hasFreeThings();
}

/*
 * Choose some arenas to relocate all cells out of and remove them from the
 * arena list. Return the head of the list of arenas to relocate.
 */
ArenaHeader *
ArenaList::pickArenasToRelocate()
{
    check();
    ArenaHeader *head = nullptr;
    ArenaHeader **tailp = &head;

    // TODO: Only scan through the arenas with space available.
    ArenaHeader **arenap = &head_;
    while (*arenap) {
        ArenaHeader *arena = *arenap;
        MOZ_ASSERT(arena);
        if (CanRelocateArena(arena) && ShouldRelocateArena(arena)) {
            // Remove from arena list
            if (cursorp_ == &arena->next)
                cursorp_ = arenap;
            *arenap = arena->next;
            arena->next = nullptr;

            // Append to relocation list
            *tailp = arena;
            tailp = &arena->next;
        } else {
            arenap = &arena->next;
        }
    }

    check();
    return head;
}

#ifdef DEBUG
inline bool
PtrIsInRange(const void *ptr, const void *start, size_t length)
{
    return uintptr_t(ptr) - uintptr_t(start) < length;
}
#endif

static bool
RelocateCell(Zone *zone, TenuredCell *src, AllocKind thingKind, size_t thingSize)
{
    // Allocate a new cell.
    MOZ_ASSERT(zone == src->zone());
    void *dstAlloc = zone->allocator.arenas.allocateFromFreeList(thingKind, thingSize);
    if (!dstAlloc)
        dstAlloc = js::gc::ArenaLists::refillFreeListInGC(zone, thingKind);
    if (!dstAlloc)
        return false;
    TenuredCell *dst = TenuredCell::fromPointer(dstAlloc);

    // Copy source cell contents to destination.
    memcpy(dst, src, thingSize);

    if (thingKind <= FINALIZE_OBJECT_LAST) {
        JSObject *srcObj = static_cast<JSObject *>(static_cast<Cell *>(src));
        JSObject *dstObj = static_cast<JSObject *>(static_cast<Cell *>(dst));

        // Fixup the pointer to inline object elements if necessary.
        if (srcObj->hasFixedElements())
            dstObj->setFixedElements();

        // Call object moved hook if present.
        if (JSObjectMovedOp op = srcObj->getClass()->ext.objectMovedOp)
            op(dstObj, srcObj);

        MOZ_ASSERT_IF(dstObj->isNative(),
                      !PtrIsInRange((const Value*)dstObj->getDenseElements(), src, thingSize));
    }

    // Copy the mark bits.
    dst->copyMarkBitsFrom(src);

    // Mark source cell as forwarded and leave a pointer to the destination.
    RelocationOverlay* overlay = RelocationOverlay::fromCell(src);
    overlay->forwardTo(dst);

    return true;
}

static bool
RelocateArena(ArenaHeader *aheader)
{
    MOZ_ASSERT(aheader->allocated());
    MOZ_ASSERT(!aheader->hasDelayedMarking);
    MOZ_ASSERT(!aheader->markOverflow);
    MOZ_ASSERT(!aheader->allocatedDuringIncremental);

    Zone *zone = aheader->zone;

    AllocKind thingKind = aheader->getAllocKind();
    size_t thingSize = aheader->getThingSize();

    for (ArenaCellIterUnderFinalize i(aheader); !i.done(); i.next()) {
        if (!RelocateCell(zone, i.getCell(), thingKind, thingSize)) {
            MOZ_CRASH(); // TODO: Handle failure here.
            return false;
        }
    }

    return true;
}

/*
 * Relocate all arenas identified by pickArenasToRelocate: for each arena,
 * relocate each cell within it, then tack it onto a list of relocated arenas.
 * Currently, we allow the relocation to fail, in which case the arena will be
 * moved back onto the list of arenas with space available. (I did this
 * originally to test my list manipulation before implementing the actual
 * moving, with half a thought to allowing pinning (moving only a portion of
 * the cells in an arena), but now it's probably just dead weight. FIXME)
 */
ArenaHeader *
ArenaList::relocateArenas(ArenaHeader *toRelocate, ArenaHeader *relocated)
{
    check();

    while (ArenaHeader *arena = toRelocate) {
        toRelocate = arena->next;

        if (RelocateArena(arena)) {
            // Prepend to list of relocated arenas
            arena->next = relocated;
            relocated = arena;
        } else {
            // For some reason, the arena did not end up empty. Prepend it to
            // the portion of the list that the cursor is pointing to (the
            // arenas with space available) so that it will be used for future
            // allocations.
            MOZ_ASSERT(arena->hasFreeThings());
            insertAtCursor(arena);
        }
    }

    check();

    return relocated;
}

ArenaHeader *
ArenaLists::relocateArenas(ArenaHeader *relocatedList)
{
    // Flush all the freeLists back into the arena headers
    purge();
    checkEmptyFreeLists();

    for (size_t i = 0; i < FINALIZE_LIMIT; i++) {
        ArenaList &al = arenaLists[i];
        ArenaHeader *toRelocate = al.pickArenasToRelocate();
        if (toRelocate)
            relocatedList = al.relocateArenas(toRelocate, relocatedList);
    }

    /*
     * When we allocate new locations for cells, we use
     * allocateFromFreeList(). Reset the free list again so that
     * AutoCopyFreeListToArenasForGC doesn't complain that the free lists
     * are different now.
     */
    purge();
    checkEmptyFreeLists();

    return relocatedList;
}

ArenaHeader *
GCRuntime::relocateArenas()
{
    gcstats::AutoPhase ap(stats, gcstats::PHASE_COMPACT_MOVE);

    ArenaHeader *relocatedList = nullptr;
    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        MOZ_ASSERT(zone->isGCFinished());
        MOZ_ASSERT(!zone->isPreservingCode());

        // We cannot move atoms as we depend on their addresses being constant.
        if (!rt->isAtomsZone(zone)) {
            zone->setGCState(Zone::Compact);
            relocatedList = zone->allocator.arenas.relocateArenas(relocatedList);
        }
    }

    return relocatedList;
}

void
MovingTracer::Visit(JSTracer *jstrc, void **thingp, JSGCTraceKind kind)
{
    TenuredCell *thing = TenuredCell::fromPointer(*thingp);
    Zone *zone = thing->zoneFromAnyThread();
    if (!zone->isGCCompacting()) {
        MOZ_ASSERT(!IsForwarded(thing));
        return;
    }
    MOZ_ASSERT(CurrentThreadCanAccessZone(zone));

    if (IsForwarded(thing)) {
        Cell *dst = Forwarded(thing);
        *thingp = dst;
    }
}

void
MovingTracer::Sweep(JSTracer *jstrc)
{
    JSRuntime *rt = jstrc->runtime();
    FreeOp *fop = rt->defaultFreeOp();

    WatchpointMap::sweepAll(rt);

    Debugger::sweepAll(fop);

    for (ZonesIter zone(rt, SkipAtoms); !zone.done(); zone.next()) {
        if (zone->isCollecting()) {
            bool oom = false;
            zone->sweep(fop, false, &oom);
            MOZ_ASSERT(!oom);

            for (CompartmentsInZoneIter c(zone); !c.done(); c.next()) {
                c->sweep(fop, false);
            }
        } else {
            /* Update cross compartment wrappers into moved zones. */
            for (CompartmentsInZoneIter c(zone); !c.done(); c.next())
                c->sweepCrossCompartmentWrappers();
        }
    }

    /* Type inference may put more blocks here to free. */
    rt->freeLifoAlloc.freeAll();

    /* Clear runtime caches that can contain cell pointers. */
    // TODO: Should possibly just call PurgeRuntime() here.
    rt->newObjectCache.purge();
    rt->nativeIterCache.purge();
}

/*
 * Update the interal pointers in a single cell.
 */
static void
UpdateCellPointers(MovingTracer *trc, Cell *cell, JSGCTraceKind traceKind) {
    if (traceKind == JSTRACE_OBJECT) {
        JSObject *obj = static_cast<JSObject *>(cell);
        obj->fixupAfterMovingGC();
    } else if (traceKind == JSTRACE_SHAPE) {
        Shape *shape = static_cast<Shape *>(cell);
        shape->fixupAfterMovingGC();
    } else if (traceKind == JSTRACE_BASE_SHAPE) {
        BaseShape *base = static_cast<BaseShape *>(cell);
        base->fixupAfterMovingGC();
    } else if (traceKind == JSTRACE_TYPE_OBJECT) {
        types::TypeObject *type = static_cast<types::TypeObject *>(cell);
        type->fixupAfterMovingGC();
    }

    TraceChildren(trc, cell, traceKind);
}

/*
 * Update pointers to relocated cells by doing a full heap traversal and sweep.
 *
 * The latter is necessary to update weak references which are not marked as
 * part of the traversal.
 */
void
GCRuntime::updatePointersToRelocatedCells()
{
    MOZ_ASSERT(rt->currentThreadHasExclusiveAccess());

    gcstats::AutoPhase ap(stats, gcstats::PHASE_COMPACT_UPDATE);
    MovingTracer trc(rt);

    // TODO: We may need to fix up other weak pointers here.

    // Fixup compartment global pointers as these get accessed during marking.
    for (GCCompartmentsIter comp(rt); !comp.done(); comp.next())
        comp->fixupAfterMovingGC();

    // Fixup cross compartment wrappers as we assert the existence of wrappers in the map.
    for (CompartmentsIter comp(rt, SkipAtoms); !comp.done(); comp.next())
        comp->fixupCrossCompartmentWrappers(&trc);

    // Fixup generators as these are not normally traced.
    for (ContextIter i(rt); !i.done(); i.next()) {
        for (JSGenerator *gen = i.get()->innermostGenerator(); gen; gen = gen->prevGenerator)
            gen->obj = MaybeForwarded(gen->obj.get());
    }

    // Iterate through all allocated cells to update internal pointers.
    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        ArenaLists &al = zone->allocator.arenas;
        for (unsigned i = 0; i < FINALIZE_LIMIT; ++i) {
            AllocKind thingKind = static_cast<AllocKind>(i);
            JSGCTraceKind traceKind = MapAllocToTraceKind(thingKind);
            for (ArenaHeader *arena = al.getFirstArena(thingKind); arena; arena = arena->next) {
                for (ArenaCellIterUnderGC i(arena); !i.done(); i.next()) {
                    UpdateCellPointers(&trc, i.getCell(), traceKind);
                }
            }
        }
    }

    // Mark roots to update them.
    markRuntime(&trc, MarkRuntime);
    Debugger::markAll(&trc);
    Debugger::markCrossCompartmentDebuggerObjectReferents(&trc);

    for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
        WeakMapBase::markAll(c, &trc);
        if (c->watchpointMap)
            c->watchpointMap->markAll(&trc);
    }

    // Mark all gray roots, making sure we call the trace callback to get the
    // current set.
    if (JSTraceDataOp op = grayRootTracer.op)
        (*op)(&trc, grayRootTracer.data);

    MovingTracer::Sweep(&trc);

    // Call callbacks to get the rest of the system to fixup other untraced pointers.
    callWeakPointerCallbacks();
}

void
GCRuntime::releaseRelocatedArenas(ArenaHeader *relocatedList)
{
    // Release the relocated arenas, now containing only forwarding pointers

#ifdef DEBUG
    for (ArenaHeader *arena = relocatedList; arena; arena = arena->next) {
        for (ArenaCellIterUnderFinalize i(arena); !i.done(); i.next()) {
            TenuredCell *src = i.getCell();
            MOZ_ASSERT(IsForwarded(src));
            TenuredCell *dest = Forwarded(src);
            MOZ_ASSERT(src->isMarked(BLACK) == dest->isMarked(BLACK));
            MOZ_ASSERT(src->isMarked(GRAY) == dest->isMarked(GRAY));
        }
    }
#endif

    unsigned count = 0;
    while (relocatedList) {
        ArenaHeader *aheader = relocatedList;
        relocatedList = relocatedList->next;

        // Clear the mark bits
        aheader->unmarkAll();

        // Mark arena as empty
        AllocKind thingKind = aheader->getAllocKind();
        size_t thingSize = aheader->getThingSize();
        Arena *arena = aheader->getArena();
        FreeSpan fullSpan;
        fullSpan.initFinal(arena->thingsStart(thingKind), arena->thingsEnd() - thingSize, thingSize);
        aheader->setFirstFreeSpan(&fullSpan);

#if defined(JS_CRASH_DIAGNOSTICS) || defined(JS_GC_ZEAL)
        JS_POISON(reinterpret_cast<void *>(arena->thingsStart(thingKind)),
                  JS_MOVED_TENURED_PATTERN, Arena::thingsSpan(thingSize));
#endif

        aheader->chunk()->releaseArena(aheader);
        ++count;
    }

    AutoLockGC lock(rt);
    expireChunksAndArenas(true);
}

#endif // JSGC_COMPACTING

void
ArenaLists::finalizeNow(FreeOp *fop, AllocKind thingKind)
{
    MOZ_ASSERT(!IsBackgroundFinalized(thingKind));
    forceFinalizeNow(fop, thingKind);
}

void
ArenaLists::forceFinalizeNow(FreeOp *fop, AllocKind thingKind)
{
    MOZ_ASSERT(backgroundFinalizeState[thingKind] == BFS_DONE);

    ArenaHeader *arenas = arenaLists[thingKind].head();
    if (!arenas)
        return;
    arenaLists[thingKind].clear();

    size_t thingsPerArena = Arena::thingsPerArena(Arena::thingSize(thingKind));
    SortedArenaList finalizedSorted(thingsPerArena);

    SliceBudget budget;
    FinalizeArenas(fop, &arenas, finalizedSorted, thingKind, budget);
    MOZ_ASSERT(!arenas);

    arenaLists[thingKind] = finalizedSorted.toArenaList();
}

void
ArenaLists::queueForForegroundSweep(FreeOp *fop, AllocKind thingKind)
{
    MOZ_ASSERT(!IsBackgroundFinalized(thingKind));
    MOZ_ASSERT(backgroundFinalizeState[thingKind] == BFS_DONE);
    MOZ_ASSERT(!arenaListsToSweep[thingKind]);

    arenaListsToSweep[thingKind] = arenaLists[thingKind].head();
    arenaLists[thingKind].clear();
}

inline void
ArenaLists::queueForBackgroundSweep(FreeOp *fop, AllocKind thingKind)
{
    MOZ_ASSERT(IsBackgroundFinalized(thingKind));
    MOZ_ASSERT(!fop->runtime()->gc.isBackgroundSweeping());

    ArenaList *al = &arenaLists[thingKind];
    if (al->isEmpty()) {
        MOZ_ASSERT(backgroundFinalizeState[thingKind] == BFS_DONE);
        return;
    }

    /*
     * The state can be done, or just-finished if we have not allocated any GC
     * things from the arena list after the previous background finalization.
     */
    MOZ_ASSERT(backgroundFinalizeState[thingKind] == BFS_DONE ||
               backgroundFinalizeState[thingKind] == BFS_JUST_FINISHED);

    arenaListsToSweep[thingKind] = al->head();
    al->clear();
    backgroundFinalizeState[thingKind] = BFS_RUN;
}

/*static*/ void
ArenaLists::backgroundFinalize(FreeOp *fop, ArenaHeader *listHead, bool onBackgroundThread)
{
    MOZ_ASSERT(listHead);
    AllocKind thingKind = listHead->getAllocKind();
    Zone *zone = listHead->zone;

    size_t thingsPerArena = Arena::thingsPerArena(Arena::thingSize(thingKind));
    SortedArenaList finalizedSorted(thingsPerArena);

    SliceBudget budget;
    FinalizeArenas(fop, &listHead, finalizedSorted, thingKind, budget);
    MOZ_ASSERT(!listHead);

    // When arenas are queued for background finalization, all arenas are moved
    // to arenaListsToSweep[], leaving the arenaLists[] empty. However, new
    // arenas may be allocated before background finalization finishes; now that
    // finalization is complete, we want to merge these lists back together.
    ArenaLists *lists = &zone->allocator.arenas;
    ArenaList *al = &lists->arenaLists[thingKind];

    // Flatten |finalizedSorted| into a regular ArenaList.
    ArenaList finalized = finalizedSorted.toArenaList();

    // Store this for later, since merging may change the state of |finalized|.
    bool allClear = finalized.isEmpty();

    AutoLockGC lock(fop->runtime());
    MOZ_ASSERT(lists->backgroundFinalizeState[thingKind] == BFS_RUN);

    // Join |al| and |finalized| into a single list.
    *al = finalized.insertListWithCursorAtEnd(*al);

    /*
     * We must set the state to BFS_JUST_FINISHED if we are running on the
     * background thread and we have touched arenaList list, even if we add to
     * the list only fully allocated arenas without any free things. It ensures
     * that the allocation thread takes the GC lock and all writes to the free
     * list elements are propagated. As we always take the GC lock when
     * allocating new arenas from the chunks we can set the state to BFS_DONE if
     * we have released all finalized arenas back to their chunks.
     */
    if (onBackgroundThread && !allClear)
        lists->backgroundFinalizeState[thingKind] = BFS_JUST_FINISHED;
    else
        lists->backgroundFinalizeState[thingKind] = BFS_DONE;

    lists->arenaListsToSweep[thingKind] = nullptr;
}

void
ArenaLists::queueObjectsForSweep(FreeOp *fop)
{
    gcstats::AutoPhase ap(fop->runtime()->gc.stats, gcstats::PHASE_SWEEP_OBJECT);

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
ArenaLists::queueStringsAndSymbolsForSweep(FreeOp *fop)
{
    gcstats::AutoPhase ap(fop->runtime()->gc.stats, gcstats::PHASE_SWEEP_STRING);

    queueForBackgroundSweep(fop, FINALIZE_FAT_INLINE_STRING);
    queueForBackgroundSweep(fop, FINALIZE_STRING);
    queueForBackgroundSweep(fop, FINALIZE_SYMBOL);

    queueForForegroundSweep(fop, FINALIZE_EXTERNAL_STRING);
}

void
ArenaLists::queueScriptsForSweep(FreeOp *fop)
{
    gcstats::AutoPhase ap(fop->runtime()->gc.stats, gcstats::PHASE_SWEEP_SCRIPT);
    queueForForegroundSweep(fop, FINALIZE_SCRIPT);
    queueForForegroundSweep(fop, FINALIZE_LAZY_SCRIPT);
}

void
ArenaLists::queueJitCodeForSweep(FreeOp *fop)
{
    gcstats::AutoPhase ap(fop->runtime()->gc.stats, gcstats::PHASE_SWEEP_JITCODE);
    queueForForegroundSweep(fop, FINALIZE_JITCODE);
}

void
ArenaLists::queueShapesForSweep(FreeOp *fop)
{
    gcstats::AutoPhase ap(fop->runtime()->gc.stats, gcstats::PHASE_SWEEP_SHAPE);

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
    MOZ_ASSERT(!InParallelSection());

    PrepareZoneForGC(zone);

    JSRuntime *rt = cx->runtime();

    /* The last ditch GC preserves all atoms. */
    AutoKeepAtoms keepAtoms(cx->perThreadData);
    rt->gc.gc(GC_NORMAL, JS::gcreason::LAST_DITCH);

    /*
     * The JSGC_END callback can legitimately allocate new GC
     * things and populate the free list. If that happens, just
     * return that list head.
     */
    size_t thingSize = Arena::thingSize(thingKind);
    if (void *thing = zone->allocator.arenas.allocateFromFreeList(thingKind, thingSize))
        return thing;

    return nullptr;
}

template <AllowGC allowGC>
/* static */ void *
ArenaLists::refillFreeList(ThreadSafeContext *cx, AllocKind thingKind)
{
    MOZ_ASSERT(cx->allocator()->arenas.freeLists[thingKind].isEmpty());
    MOZ_ASSERT_IF(cx->isJSContext(), !cx->asJSContext()->runtime()->isHeapBusy());

    Zone *zone = cx->allocator()->zone_;

    bool runGC = cx->allowGC() && allowGC &&
                 cx->asJSContext()->runtime()->gc.incrementalState != NO_INCREMENTAL &&
                 zone->usage.gcBytes() > zone->threshold.gcTriggerBytes();

    MOZ_ASSERT_IF(cx->isJSContext() && allowGC,
                  !cx->asJSContext()->runtime()->currentThreadHasExclusiveAccess());

    for (;;) {
        if (MOZ_UNLIKELY(runGC)) {
            if (void *thing = RunLastDitchGC(cx->asJSContext(), zone, thingKind))
                return thing;
        }

        AutoMaybeStartBackgroundAllocation maybeStartBackgroundAllocation;

        if (cx->isJSContext()) {
            /*
             * allocateFromArena may fail while the background finalization still
             * run. If we are on the main thread, we want to wait for it to finish
             * and restart. However, checking for that is racy as the background
             * finalization could free some things after allocateFromArena decided
             * to fail but at this point it may have already stopped. To avoid
             * this race we always try to allocate twice.
             */
            for (bool secondAttempt = false; ; secondAttempt = true) {
                void *thing = cx->allocator()->arenas.allocateFromArenaInline(zone, thingKind,
                                                                              maybeStartBackgroundAllocation);
                if (MOZ_LIKELY(!!thing))
                    return thing;
                if (secondAttempt)
                    break;

                cx->asJSContext()->runtime()->gc.waitBackgroundSweepEnd();
            }
        } else {
            /*
             * If we're off the main thread, we try to allocate once and
             * return whatever value we get. If we aren't in a ForkJoin
             * session (i.e. we are in a helper thread async with the main
             * thread), we need to first ensure the main thread is not in a GC
             * session.
             */
            mozilla::Maybe<AutoLockHelperThreadState> lock;
            JSRuntime *rt = zone->runtimeFromAnyThread();
            if (rt->exclusiveThreadsPresent()) {
                lock.emplace();
                while (rt->isHeapBusy())
                    HelperThreadState().wait(GlobalHelperThreadState::PRODUCER);
            }

            void *thing = cx->allocator()->arenas.allocateFromArenaInline(zone, thingKind,
                                                                          maybeStartBackgroundAllocation);
            if (thing)
                return thing;
        }

        if (!cx->allowGC() || !allowGC)
            return nullptr;

        /*
         * We failed to allocate. Run the GC if we haven't done it already.
         * Otherwise report OOM.
         */
        if (runGC)
            break;
        runGC = true;
    }

    MOZ_ASSERT(allowGC);
    js_ReportOutOfMemory(cx);
    return nullptr;
}

template void *
ArenaLists::refillFreeList<NoGC>(ThreadSafeContext *cx, AllocKind thingKind);

template void *
ArenaLists::refillFreeList<CanGC>(ThreadSafeContext *cx, AllocKind thingKind);

/* static */ void *
ArenaLists::refillFreeListInGC(Zone *zone, AllocKind thingKind)
{
    /*
     * Called by compacting GC to refill a free list while we are in a GC.
     */

    Allocator &allocator = zone->allocator;
    MOZ_ASSERT(allocator.arenas.freeLists[thingKind].isEmpty());
    mozilla::DebugOnly<JSRuntime *> rt = zone->runtimeFromMainThread();
    MOZ_ASSERT(rt->isHeapMajorCollecting());
    MOZ_ASSERT(!rt->gc.isBackgroundSweeping());

    return allocator.arenas.allocateFromArena(zone, thingKind);
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
{
    reset();
}

SliceBudget::SliceBudget(int64_t budget)
{
    if (budget == Unlimited) {
        reset();
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

void
js::MarkCompartmentActive(InterpreterFrame *fp)
{
    fp->script()->compartment()->zone()->active = true;
}

void
GCRuntime::requestInterrupt(JS::gcreason::Reason reason)
{
    if (isNeeded)
        return;

    isNeeded = true;
    triggerReason = reason;
    rt->requestInterrupt(JSRuntime::RequestInterruptMainThread);
}

bool
GCRuntime::triggerGC(JS::gcreason::Reason reason)
{
    /* Wait till end of parallel section to trigger GC. */
    if (InParallelSection()) {
        ForkJoinContext::current()->requestGC(reason);
        return true;
    }

    /*
     * Don't trigger GCs if this is being called off the main thread from
     * onTooMuchMalloc().
     */
    if (!CurrentThreadCanAccessRuntime(rt))
        return false;

    /* Don't trigger GCs when allocating under the interrupt callback lock. */
    if (rt->currentThreadOwnsInterruptLock())
        return false;

    /* GC is already running. */
    if (rt->isHeapCollecting())
        return false;

    JS::PrepareForFullGC(rt);
    requestInterrupt(reason);
    return true;
}

bool
GCRuntime::triggerZoneGC(Zone *zone, JS::gcreason::Reason reason)
{
    /*
     * If parallel threads are running, wait till they
     * are stopped to trigger GC.
     */
    if (InParallelSection()) {
        ForkJoinContext::current()->requestZoneGC(zone, reason);
        return true;
    }

    /* Zones in use by a thread with an exclusive context can't be collected. */
    if (zone->usedByExclusiveThread)
        return false;

    /* Don't trigger GCs when allocating under the interrupt callback lock. */
    if (rt->currentThreadOwnsInterruptLock())
        return false;

    /* GC is already running. */
    if (rt->isHeapCollecting())
        return false;

#ifdef JS_GC_ZEAL
    if (zealMode == ZealAllocValue) {
        triggerGC(reason);
        return true;
    }
#endif

    if (rt->isAtomsZone(zone)) {
        /* We can't do a zone GC of the atoms compartment. */
        triggerGC(reason);
        return true;
    }

    PrepareZoneForGC(zone);
    requestInterrupt(reason);
    return true;
}

bool
GCRuntime::maybeGC(Zone *zone)
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));

#ifdef JS_GC_ZEAL
    if (zealMode == ZealAllocValue || zealMode == ZealPokeValue) {
        JS::PrepareForFullGC(rt);
        gc(GC_NORMAL, JS::gcreason::MAYBEGC);
        return true;
    }
#endif

    if (isNeeded) {
        gcSlice(GC_NORMAL, JS::gcreason::MAYBEGC);
        return true;
    }

    double factor = schedulingState.inHighFrequencyGCMode() ? 0.85 : 0.9;
    if (zone->usage.gcBytes() > 1024 * 1024 &&
        zone->usage.gcBytes() >= factor * zone->threshold.gcTriggerBytes() &&
        incrementalState == NO_INCREMENTAL &&
        !isBackgroundSweeping())
    {
        PrepareZoneForGC(zone);
        gcSlice(GC_NORMAL, JS::gcreason::MAYBEGC);
        return true;
    }

    return false;
}

void
GCRuntime::maybePeriodicFullGC()
{
    /*
     * Trigger a periodic full GC.
     *
     * This is a source of non-determinism, but is not called from the shell.
     *
     * Access to the counters and, on 32 bit, setting gcNextFullGCTime below
     * is not atomic and a race condition could trigger or suppress the GC. We
     * tolerate this.
     */
#ifndef JS_MORE_DETERMINISTIC
    int64_t now = PRMJ_Now();
    if (nextFullGCTime && nextFullGCTime <= now) {
        if (chunkAllocationSinceLastGC ||
            numArenasFreeCommitted > decommitThreshold)
        {
            JS::PrepareForFullGC(rt);
            gcSlice(GC_SHRINK, JS::gcreason::MAYBEGC);
        } else {
            nextFullGCTime = now + GC_IDLE_FULL_SPAN;
        }
    }
#endif
}

void
GCRuntime::decommitArenasFromAvailableList(Chunk **availableListHeadp)
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
    MOZ_ASSERT(chunk->info.prevp == availableListHeadp);
    while (Chunk *next = chunk->info.next) {
        MOZ_ASSERT(next->info.prevp == &chunk->info.next);
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
                if (!isHeapBusy())
                    maybeUnlock.emplace(rt);
                ok = MarkPagesUnused(aheader->getArena(), ArenaSize);
            }

            if (ok) {
                ++chunk->info.numArenasFree;
                chunk->decommittedArenas.set(arenaIndex);
            } else {
                chunk->addArenaToFreeList(rt, aheader);
            }
            MOZ_ASSERT(chunk->hasAvailableArenas());
            MOZ_ASSERT(!chunk->unused());
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
                MOZ_ASSERT(chunk->info.prevp);
            }

            if (chunkAllocationSinceLastGC || !ok) {
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
        MOZ_ASSERT_IF(chunk->info.prevp, *chunk->info.prevp == chunk);
        if (chunk->info.prevp == availableListHeadp || !chunk->info.prevp)
            break;

        /*
         * prevp exists and is not the list head. It must point to the next
         * field of the previous chunk.
         */
        chunk = chunk->getPrevious();
    }
}

void
GCRuntime::decommitArenas()
{
    decommitArenasFromAvailableList(&systemAvailableChunkListHead);
    decommitArenasFromAvailableList(&userAvailableChunkListHead);
}

/* Must be called with the GC lock taken. */
void
GCRuntime::expireChunksAndArenas(bool shouldShrink)
{
#ifdef JSGC_FJGENERATIONAL
    rt->threadPool.pruneChunkCache();
#endif

    if (Chunk *toFree = expireChunkPool(shouldShrink, false)) {
        AutoUnlockGC unlock(rt);
        freeChunkList(toFree);
    }

    if (shouldShrink)
        decommitArenas();
}

void
GCRuntime::sweepBackgroundThings(bool onBackgroundThread)
{
    /*
     * We must finalize in the correct order, see comments in
     * finalizeObjects.
     */
    FreeOp fop(rt);
    for (int phase = 0 ; phase < BackgroundPhaseCount ; ++phase) {
        for (Zone *zone = sweepingZones; zone; zone = zone->gcNextGraphNode) {
            for (int index = 0 ; index < BackgroundPhaseLength[phase] ; ++index) {
                AllocKind kind = BackgroundPhases[phase][index];
                ArenaHeader *arenas = zone->allocator.arenas.arenaListsToSweep[kind];
                if (arenas)
                    ArenaLists::backgroundFinalize(&fop, arenas, onBackgroundThread);
            }
        }
    }

    sweepingZones = nullptr;
}

void
GCRuntime::assertBackgroundSweepingFinished()
{
#ifdef DEBUG
    MOZ_ASSERT(!sweepingZones);
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        for (unsigned i = 0; i < FINALIZE_LIMIT; ++i) {
            MOZ_ASSERT(!zone->allocator.arenas.arenaListsToSweep[i]);
            MOZ_ASSERT(zone->allocator.arenas.doneBackgroundFinalize(AllocKind(i)));
        }
    }
#endif
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

bool
GCHelperState::init()
{
    if (!(done = PR_NewCondVar(rt->gc.lock)))
        return false;

    if (CanUseExtraThreads()) {
        backgroundAllocation = (GetCPUCount() >= 2);
        HelperThreadState().ensureInitialized();
    } else {
        backgroundAllocation = false;
    }

    return true;
}

void
GCHelperState::finish()
{
    if (!rt->gc.lock) {
        MOZ_ASSERT(state_ == IDLE);
        return;
    }

    // Wait for any lingering background sweeping to finish.
    waitBackgroundSweepEnd();

    if (done)
        PR_DestroyCondVar(done);
}

GCHelperState::State
GCHelperState::state()
{
    MOZ_ASSERT(rt->gc.currentThreadOwnsGCLock());
    return state_;
}

void
GCHelperState::setState(State state)
{
    MOZ_ASSERT(rt->gc.currentThreadOwnsGCLock());
    state_ = state;
}

void
GCHelperState::startBackgroundThread(State newState)
{
    MOZ_ASSERT(!thread && state() == IDLE && newState != IDLE);
    setState(newState);

    if (!HelperThreadState().gcHelperWorklist().append(this))
        CrashAtUnhandlableOOM("Could not add to pending GC helpers list");
    HelperThreadState().notifyAll(GlobalHelperThreadState::PRODUCER);
}

void
GCHelperState::waitForBackgroundThread()
{
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(rt));

    rt->gc.lockOwner = nullptr;
    PR_WaitCondVar(done, PR_INTERVAL_NO_TIMEOUT);
#ifdef DEBUG
    rt->gc.lockOwner = PR_GetCurrentThread();
#endif
}

void
GCHelperState::work()
{
    MOZ_ASSERT(CanUseExtraThreads());

    AutoLockGC lock(rt);

    MOZ_ASSERT(!thread);
    thread = PR_GetCurrentThread();

    TraceLogger *logger = TraceLoggerForCurrentThread();

    switch (state()) {

      case IDLE:
        MOZ_CRASH("GC helper triggered on idle state");
        break;

      case SWEEPING: {
        AutoTraceLog logSweeping(logger, TraceLogger::GCSweeping);
        doSweep();
        MOZ_ASSERT(state() == SWEEPING);
        break;
      }

      case ALLOCATING: {
        AutoTraceLog logAllocation(logger, TraceLogger::GCAllocation);
        do {
            Chunk *chunk;
            {
                AutoUnlockGC unlock(rt);
                chunk = Chunk::allocate(rt);
            }

            /* OOM stops the background allocation. */
            if (!chunk)
                break;
            MOZ_ASSERT(chunk->info.numArenasFreeCommitted == 0);
            rt->gc.chunkPool.put(chunk);
        } while (state() == ALLOCATING && rt->gc.wantBackgroundAllocation());

        MOZ_ASSERT(state() == ALLOCATING || state() == CANCEL_ALLOCATION);
        break;
      }

      case CANCEL_ALLOCATION:
        break;
    }

    setState(IDLE);
    thread = nullptr;

    PR_NotifyAllCondVar(done);
}

void
GCHelperState::startBackgroundSweep(bool shouldShrink)
{
    MOZ_ASSERT(CanUseExtraThreads());

    AutoLockHelperThreadState helperLock;
    AutoLockGC lock(rt);
    MOZ_ASSERT(state() == IDLE);
    MOZ_ASSERT(!sweepFlag);
    sweepFlag = true;
    shrinkFlag = shouldShrink;
    startBackgroundThread(SWEEPING);
}

/* Must be called with the GC lock taken. */
void
GCHelperState::startBackgroundShrink()
{
    MOZ_ASSERT(CanUseExtraThreads());
    switch (state()) {
      case IDLE:
        MOZ_ASSERT(!sweepFlag);
        shrinkFlag = true;
        startBackgroundThread(SWEEPING);
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
    }
}

void
GCHelperState::waitBackgroundSweepEnd()
{
    AutoLockGC lock(rt);
    while (state() == SWEEPING)
        waitForBackgroundThread();
    if (rt->gc.incrementalState == NO_INCREMENTAL)
        rt->gc.assertBackgroundSweepingFinished();
}

void
GCHelperState::waitBackgroundSweepOrAllocEnd()
{
    AutoLockGC lock(rt);
    if (state() == ALLOCATING)
        setState(CANCEL_ALLOCATION);
    while (state() == SWEEPING || state() == CANCEL_ALLOCATION)
        waitForBackgroundThread();
    if (rt->gc.incrementalState == NO_INCREMENTAL)
        rt->gc.assertBackgroundSweepingFinished();
}

/* Must be called with the GC lock taken. */
inline void
GCHelperState::startBackgroundAllocationIfIdle()
{
    if (state_ == IDLE)
        startBackgroundThread(ALLOCATING);
}

/* Must be called with the GC lock taken. */
void
GCHelperState::doSweep()
{
    if (sweepFlag) {
        sweepFlag = false;
        AutoUnlockGC unlock(rt);

        rt->gc.sweepBackgroundThings(true);

        rt->freeLifoAlloc.freeAll();
    }

    bool shrinking = shrinkFlag;
    rt->gc.expireChunksAndArenas(shrinking);

    /*
     * The main thread may have called ShrinkGCBuffers while
     * ExpireChunksAndArenas(rt, false) was running, so we recheck the flag
     * afterwards.
     */
    if (!shrinking && shrinkFlag) {
        shrinkFlag = false;
        rt->gc.expireChunksAndArenas(true);
    }
}

bool
GCHelperState::onBackgroundThread()
{
    return PR_GetCurrentThread() == thread;
}

bool
GCRuntime::shouldReleaseObservedTypes()
{
    bool releaseTypes = false;

#ifdef JS_GC_ZEAL
    if (zealMode != 0)
        releaseTypes = true;
#endif

    /* We may miss the exact target GC due to resets. */
    if (majorGCNumber >= jitReleaseNumber)
        releaseTypes = true;

    if (releaseTypes)
        jitReleaseNumber = majorGCNumber + JIT_SCRIPT_RELEASE_TYPES_PERIOD;

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
void
Zone::sweepCompartments(FreeOp *fop, bool keepAtleastOne, bool lastGC)
{
    JSRuntime *rt = runtimeFromMainThread();
    JSDestroyCompartmentCallback callback = rt->destroyCompartmentCallback;

    JSCompartment **read = compartments.begin();
    JSCompartment **end = compartments.end();
    JSCompartment **write = read;
    bool foundOne = false;
    while (read < end) {
        JSCompartment *comp = *read++;
        MOZ_ASSERT(!rt->isAtomsCompartment(comp));

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
    compartments.resize(write - compartments.begin());
    MOZ_ASSERT_IF(keepAtleastOne, !compartments.empty());
}

void
GCRuntime::sweepZones(FreeOp *fop, bool lastGC)
{
    JSZoneCallback callback = rt->destroyZoneCallback;

    /* Skip the atomsCompartment zone. */
    Zone **read = zones.begin() + 1;
    Zone **end = zones.end();
    Zone **write = read;
    MOZ_ASSERT(zones.length() >= 1);
    MOZ_ASSERT(rt->isAtomsZone(zones[0]));

    while (read < end) {
        Zone *zone = *read++;

        if (zone->wasGCStarted()) {
            if ((zone->allocator.arenas.arenaListsAreEmpty() && !zone->hasMarkedCompartments()) ||
                lastGC)
            {
                zone->allocator.arenas.checkEmptyFreeLists();
                if (callback)
                    callback(zone);
                zone->sweepCompartments(fop, false, lastGC);
                MOZ_ASSERT(zone->compartments.empty());
                fop->delete_(zone);
                continue;
            }
            zone->sweepCompartments(fop, true, lastGC);
        }
        *write++ = zone;
    }
    zones.resize(write - zones.begin());
}

static void
PurgeRuntime(JSRuntime *rt)
{
    for (GCCompartmentsIter comp(rt); !comp.done(); comp.next())
        comp->purge();

    rt->freeLifoAlloc.transferUnusedFrom(&rt->tempLifoAlloc);
    rt->interpreterStack().purge(rt);

    rt->gsnCache.purge();
    rt->scopeCoordinateNameCache.purge();
    rt->newObjectCache.purge();
    rt->nativeIterCache.purge();
    rt->uncompressedSourceCache.purge();
    rt->evalCache.clear();

    if (!rt->hasActiveCompilations())
        rt->parseMapPool().purgeAll();
}

bool
GCRuntime::shouldPreserveJITCode(JSCompartment *comp, int64_t currentTime,
                                 JS::gcreason::Reason reason)
{
    if (cleanUpEverything)
        return false;

    if (alwaysPreserveCode)
        return true;
    if (comp->preserveJitCode())
        return true;
    if (comp->lastAnimationTime + PRMJ_USEC_PER_SEC >= currentTime)
        return true;
    if (reason == JS::gcreason::DEBUG_GC)
        return true;

    if (comp->jitCompartment() && comp->jitCompartment()->hasRecentParallelActivity())
        return true;

    return false;
}

#ifdef DEBUG
class CompartmentCheckTracer : public JSTracer
{
  public:
    CompartmentCheckTracer(JSRuntime *rt, JSTraceCallback callback)
      : JSTracer(rt, callback)
    {}

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
            if (*p->value().unsafeGet() == ObjectValue(*src))
                return true;
        }
    }

    /*
     * If the cross-compartment edge is caused by the debugger, then we don't
     * know the right hashtable key, so we have to iterate.
     */
    for (JSCompartment::WrapperEnum e(srccomp); !e.empty(); e.popFront()) {
        if (e.front().key().wrapped == dst && ToMarkable(e.front().value()) == src)
            return true;
    }

    return false;
}

static void
CheckCompartment(CompartmentCheckTracer *trc, JSCompartment *thingCompartment,
                 Cell *thing, JSGCTraceKind kind)
{
    MOZ_ASSERT(thingCompartment == trc->compartment ||
               trc->runtime()->isAtomsCompartment(thingCompartment) ||
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
        return nullptr;
}

static void
CheckCompartmentCallback(JSTracer *trcArg, void **thingp, JSGCTraceKind kind)
{
    CompartmentCheckTracer *trc = static_cast<CompartmentCheckTracer *>(trcArg);
    TenuredCell *thing = TenuredCell::fromPointer(*thingp);

    JSCompartment *comp = CompartmentOfCell(thing, kind);
    if (comp && trc->compartment) {
        CheckCompartment(trc, comp, thing, kind);
    } else {
        MOZ_ASSERT(thing->zone() == trc->zone ||
                   trc->runtime()->isAtomsZone(thing->zone()));
    }
}

void
GCRuntime::checkForCompartmentMismatches()
{
    if (disableStrictProxyCheckingCount)
        return;

    CompartmentCheckTracer trc(rt, CheckCompartmentCallback);
    for (ZonesIter zone(rt, SkipAtoms); !zone.done(); zone.next()) {
        trc.zone = zone;
        for (size_t thingKind = 0; thingKind < FINALIZE_LAST; thingKind++) {
            for (ZoneCellIterUnderGC i(zone, AllocKind(thingKind)); !i.done(); i.next()) {
                trc.src = i.getCell();
                trc.srcKind = MapAllocToTraceKind(AllocKind(thingKind));
                trc.compartment = CompartmentOfCell(trc.src, trc.srcKind);
                JS_TraceChildren(&trc, trc.src, trc.srcKind);
            }
        }
    }
}
#endif

bool
GCRuntime::beginMarkPhase(JS::gcreason::Reason reason)
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
        MOZ_ASSERT(!zone->compartments.empty());
        for (unsigned i = 0; i < FINALIZE_LIMIT; ++i)
            MOZ_ASSERT(!zone->allocator.arenas.arenaListsToSweep[i]);

        /* Set up which zones will be collected. */
        if (zone->isGCScheduled()) {
            if (!rt->isAtomsZone(zone)) {
                any = true;
                zone->setGCState(Zone::Mark);
            }
        } else {
            isFull = false;
        }

        zone->setPreservingCode(false);
    }

    for (CompartmentsIter c(rt, WithAtoms); !c.done(); c.next()) {
        c->marked = false;
        c->scheduledForDestruction = false;
        c->maybeAlive = false;
        if (shouldPreserveJITCode(c, currentTime, reason))
            c->zone()->setPreservingCode(true);
    }

    if (!rt->gc.cleanUpEverything) {
        if (JSCompartment *comp = jit::TopmostIonActivationCompartment(rt))
            comp->zone()->setPreservingCode(true);
    }

    /*
     * Atoms are not in the cross-compartment map. So if there are any
     * zones that are not being collected, we are not allowed to collect
     * atoms. Otherwise, the non-collected zones could contain pointers
     * to atoms that we would miss.
     *
     * keepAtoms() will only change on the main thread, which we are currently
     * on. If the value of keepAtoms() changes between GC slices, then we'll
     * cancel the incremental GC. See IsIncrementalGCSafe.
     */
    if (isFull && !rt->keepAtoms()) {
        Zone *atomsZone = rt->atomsCompartment()->zone();
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
     * At the end of each incremental slice, we call prepareForIncrementalGC,
     * which marks objects in all arenas that we're currently allocating
     * into. This can cause leaks if unreachable objects are in these
     * arenas. This purge call ensures that we only mark arenas that have had
     * allocations after the incremental GC started.
     */
    if (isIncremental) {
        for (GCZonesIter zone(rt); !zone.done(); zone.next())
            zone->allocator.arenas.purge();
    }

    marker.start();
    MOZ_ASSERT(!marker.callback);
    MOZ_ASSERT(IS_GC_MARKING_TRACER(&marker));

    /* For non-incremental GC the following sweep discards the jit code. */
    if (isIncremental) {
        for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
            gcstats::AutoPhase ap(stats, gcstats::PHASE_MARK_DISCARD_CODE);
            zone->discardJitCode(rt->defaultFreeOp());
        }
    }

    GCMarker *gcmarker = &marker;

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
        gcstats::AutoPhase ap(stats, gcstats::PHASE_PURGE);
        PurgeRuntime(rt);
    }

    /*
     * Mark phase.
     */
    gcstats::AutoPhase ap1(stats, gcstats::PHASE_MARK);
    gcstats::AutoPhase ap2(stats, gcstats::PHASE_MARK_ROOTS);

    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        /* Unmark everything in the zones being collected. */
        zone->allocator.arenas.unmarkAll();
    }

    for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
        /* Unmark all weak maps in the compartments being collected. */
        WeakMapBase::unmarkCompartment(c);
    }

    if (isFull)
        UnmarkScriptData(rt);

    markRuntime(gcmarker, MarkRuntime);
    if (isIncremental)
        bufferGrayRoots();

    /*
     * This code ensures that if a compartment is "dead", then it will be
     * collected in this GC. A compartment is considered dead if its maybeAlive
     * flag is false. The maybeAlive flag is set if:
     *   (1) the compartment has incoming cross-compartment edges, or
     *   (2) an object in the compartment was marked during root marking, either
     *       as a black root or a gray root.
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

    /* Set the maybeAlive flag based on cross-compartment edges. */
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
        for (JSCompartment::WrapperEnum e(c); !e.empty(); e.popFront()) {
            const CrossCompartmentKey &key = e.front().key();
            JSCompartment *dest;
            switch (key.kind) {
              case CrossCompartmentKey::ObjectWrapper:
              case CrossCompartmentKey::DebuggerObject:
              case CrossCompartmentKey::DebuggerSource:
              case CrossCompartmentKey::DebuggerEnvironment:
                dest = static_cast<JSObject *>(key.wrapped)->compartment();
                break;
              case CrossCompartmentKey::DebuggerScript:
                dest = static_cast<JSScript *>(key.wrapped)->compartment();
                break;
              default:
                dest = nullptr;
                break;
            }
            if (dest)
                dest->maybeAlive = true;
        }
    }

    /*
     * For black roots, code in gc/Marking.cpp will already have set maybeAlive
     * during MarkRuntime.
     */

    for (GCCompartmentsIter c(rt); !c.done(); c.next()) {
        if (!c->maybeAlive && !rt->isAtomsCompartment(c))
            c->scheduledForDestruction = true;
    }
    foundBlackGrayEdges = false;

    return true;
}

template <class CompartmentIterT>
void
GCRuntime::markWeakReferences(gcstats::Phase phase)
{
    MOZ_ASSERT(marker.isDrained());

    gcstats::AutoPhase ap1(stats, phase);

    for (;;) {
        bool markedAny = false;
        for (CompartmentIterT c(rt); !c.done(); c.next()) {
            markedAny |= WatchpointMap::markCompartmentIteratively(c, &marker);
            markedAny |= WeakMapBase::markCompartmentIteratively(c, &marker);
        }
        markedAny |= Debugger::markAllIteratively(&marker);

        if (!markedAny)
            break;

        SliceBudget budget;
        marker.drainMarkStack(budget);
    }
    MOZ_ASSERT(marker.isDrained());
}

void
GCRuntime::markWeakReferencesInCurrentGroup(gcstats::Phase phase)
{
    markWeakReferences<GCCompartmentGroupIter>(phase);
}

template <class ZoneIterT, class CompartmentIterT>
void
GCRuntime::markGrayReferences(gcstats::Phase phase)
{
    gcstats::AutoPhase ap(stats, phase);
    if (marker.hasBufferedGrayRoots()) {
        for (ZoneIterT zone(rt); !zone.done(); zone.next())
            marker.markBufferedGrayRoots(zone);
    } else {
        MOZ_ASSERT(!isIncremental);
        if (JSTraceDataOp op = grayRootTracer.op)
            (*op)(&marker, grayRootTracer.data);
    }
    SliceBudget budget;
    marker.drainMarkStack(budget);
}

void
GCRuntime::markGrayReferencesInCurrentGroup(gcstats::Phase phase)
{
    markGrayReferences<GCZoneGroupIter, GCCompartmentGroupIter>(phase);
}

void
GCRuntime::markAllWeakReferences(gcstats::Phase phase)
{
    markWeakReferences<GCCompartmentsIter>(phase);
}

void
GCRuntime::markAllGrayReferences(gcstats::Phase phase)
{
    markGrayReferences<GCZonesIter, GCCompartmentsIter>(phase);
}

#ifdef DEBUG

class js::gc::MarkingValidator
{
  public:
    explicit MarkingValidator(GCRuntime *gc);
    ~MarkingValidator();
    void nonIncrementalMark();
    void validate();

  private:
    GCRuntime *gc;
    bool initialized;

    typedef HashMap<Chunk *, ChunkBitmap *, GCChunkHasher, SystemAllocPolicy> BitmapMap;
    BitmapMap map;
};

#endif // DEBUG

#ifdef JS_GC_MARKING_VALIDATION

js::gc::MarkingValidator::MarkingValidator(GCRuntime *gc)
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

    JSRuntime *runtime = gc->rt;
    GCMarker *gcmarker = &gc->marker;

    /* Save existing mark bits. */
    for (GCChunkSet::Range r(gc->chunkSet.all()); !r.empty(); r.popFront()) {
        ChunkBitmap *bitmap = &r.front()->bitmap;
	ChunkBitmap *entry = js_new<ChunkBitmap>();
        if (!entry)
            return;

        memcpy((void *)entry->bitmap, (void *)bitmap->bitmap, sizeof(bitmap->bitmap));
        if (!map.putNew(r.front(), entry))
            return;
    }

    /*
     * Temporarily clear the weakmaps' mark flags for the compartments we are
     * collecting.
     */

    WeakMapSet markedWeakMaps;
    if (!markedWeakMaps.init())
        return;

    for (GCCompartmentsIter c(runtime); !c.done(); c.next()) {
        if (!WeakMapBase::saveCompartmentMarkedWeakMaps(c, markedWeakMaps))
            return;
    }

    /*
     * After this point, the function should run to completion, so we shouldn't
     * do anything fallible.
     */
    initialized = true;

    for (GCCompartmentsIter c(runtime); !c.done(); c.next())
        WeakMapBase::unmarkCompartment(c);

    /* Re-do all the marking, but non-incrementally. */
    js::gc::State state = gc->incrementalState;
    gc->incrementalState = MARK_ROOTS;

    MOZ_ASSERT(gcmarker->isDrained());
    gcmarker->reset();

    for (GCChunkSet::Range r(gc->chunkSet.all()); !r.empty(); r.popFront())
        r.front()->bitmap.clear();

    {
        gcstats::AutoPhase ap1(gc->stats, gcstats::PHASE_MARK);
        gcstats::AutoPhase ap2(gc->stats, gcstats::PHASE_MARK_ROOTS);
        gc->markRuntime(gcmarker, GCRuntime::MarkRuntime, GCRuntime::UseSavedRoots);
    }

    {
        gcstats::AutoPhase ap1(gc->stats, gcstats::PHASE_MARK);
        SliceBudget budget;
        gc->incrementalState = MARK;
        gc->marker.drainMarkStack(budget);
    }

    gc->incrementalState = SWEEP;
    {
        gcstats::AutoPhase ap1(gc->stats, gcstats::PHASE_SWEEP);
        gcstats::AutoPhase ap2(gc->stats, gcstats::PHASE_SWEEP_MARK);
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
    for (GCChunkSet::Range r(gc->chunkSet.all()); !r.empty(); r.popFront()) {
        Chunk *chunk = r.front();
        ChunkBitmap *bitmap = &chunk->bitmap;
        ChunkBitmap *entry = map.lookup(chunk)->value();
        Swap(*entry, *bitmap);
    }

    for (GCCompartmentsIter c(runtime); !c.done(); c.next())
        WeakMapBase::unmarkCompartment(c);
    WeakMapBase::restoreCompartmentMarkedWeakMaps(markedWeakMaps);

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

    for (GCChunkSet::Range r(gc->chunkSet.all()); !r.empty(); r.popFront()) {
        Chunk *chunk = r.front();
        BitmapMap::Ptr ptr = map.lookup(chunk);
        if (!ptr)
            continue;  /* Allocated after we did the non-incremental mark. */

        ChunkBitmap *bitmap = ptr->value();
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

#endif // JS_GC_MARKING_VALIDATION

void
GCRuntime::computeNonIncrementalMarkingForValidation()
{
#ifdef JS_GC_MARKING_VALIDATION
    MOZ_ASSERT(!markingValidator);
    if (isIncremental && validate)
        markingValidator = js_new<MarkingValidator>(this);
    if (markingValidator)
        markingValidator->nonIncrementalMark();
#endif
}

void
GCRuntime::validateIncrementalMarking()
{
#ifdef JS_GC_MARKING_VALIDATION
    if (markingValidator)
        markingValidator->validate();
#endif
}

void
GCRuntime::finishMarkingValidation()
{
#ifdef JS_GC_MARKING_VALIDATION
    js_delete(markingValidator);
    markingValidator = nullptr;
#endif
}

static void
AssertNeedsBarrierFlagsConsistent(JSRuntime *rt)
{
#ifdef JS_GC_MARKING_VALIDATION
    bool anyNeedsBarrier = false;
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
        anyNeedsBarrier |= zone->needsIncrementalBarrier();
    MOZ_ASSERT(rt->needsIncrementalBarrier() == anyNeedsBarrier);
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
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
        for (JSCompartment::WrapperEnum e(c); !e.empty(); e.popFront()) {
            if (e.front().key().kind == CrossCompartmentKey::StringWrapper)
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
        CrossCompartmentKey::Kind kind = e.front().key().kind;
        MOZ_ASSERT(kind != CrossCompartmentKey::StringWrapper);
        TenuredCell &other = e.front().key().wrapped->asTenured();
        if (kind == CrossCompartmentKey::ObjectWrapper) {
            /*
             * Add edge to wrapped object compartment if wrapped object is not
             * marked black to indicate that wrapper compartment not be swept
             * after wrapped compartment.
             */
            if (!other.isMarked(BLACK) || other.isMarked(GRAY)) {
                JS::Zone *w = other.zone();
                if (w->isGCMarking())
                    finder.addEdgeTo(w);
            }
        } else {
            MOZ_ASSERT(kind == CrossCompartmentKey::DebuggerScript ||
                       kind == CrossCompartmentKey::DebuggerSource ||
                       kind == CrossCompartmentKey::DebuggerObject ||
                       kind == CrossCompartmentKey::DebuggerEnvironment);
            /*
             * Add edge for debugger object wrappers, to ensure (in conjuction
             * with call to Debugger::findCompartmentEdges below) that debugger
             * and debuggee objects are always swept in the same group.
             */
            JS::Zone *w = other.zone();
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
    if (rt->atomsCompartment()->zone()->isGCMarking())
        finder.addEdgeTo(rt->atomsCompartment()->zone());

    for (CompartmentsInZoneIter comp(this); !comp.done(); comp.next())
        comp->findOutgoingEdges(finder);

    for (ZoneSet::Range r = gcZoneGroupEdges.all(); !r.empty(); r.popFront()) {
        if (r.front()->isGCMarking())
            finder.addEdgeTo(r.front());
    }
    gcZoneGroupEdges.clear();
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

    for (GCCompartmentsIter comp(rt); !comp.done(); comp.next()) {
        if (!WeakMapBase::findZoneEdgesForCompartment(comp))
            return false;
    }

    return true;
}

void
GCRuntime::findZoneGroups()
{
    ComponentFinder<Zone> finder(rt->mainThread.nativeStackLimit[StackForSystemCode]);
    if (!isIncremental || !findZoneEdgesForWeakMaps())
        finder.useOneComponent();

    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        MOZ_ASSERT(zone->isGCMarking());
        finder.addNode(zone);
    }
    zoneGroups = finder.getResultsList();
    currentZoneGroup = zoneGroups;
    zoneGroupIndex = 0;

    for (Zone *head = currentZoneGroup; head; head = head->nextGroup()) {
        for (Zone *zone = head; zone; zone = zone->nextNodeInGroup())
            MOZ_ASSERT(zone->isGCMarking());
    }

    MOZ_ASSERT_IF(!isIncremental, !currentZoneGroup->nextGroup());
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

    for (Zone *zone = currentZoneGroup; zone; zone = zone->nextNodeInGroup())
        MOZ_ASSERT(zone->isGCMarking());

    if (!isIncremental)
        ComponentFinder<Zone>::mergeGroups(currentZoneGroup);

    if (abortSweepAfterCurrentGroup) {
        MOZ_ASSERT(!isIncremental);
        for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
            MOZ_ASSERT(!zone->gcNextGraphComponent);
            MOZ_ASSERT(zone->isGCMarking());
            zone->setNeedsIncrementalBarrier(false, Zone::UpdateJit);
            zone->setGCState(Zone::NoGC);
            zone->gcGrayRoots.clearAndFree();
        }
        rt->setNeedsIncrementalBarrier(false);
        AssertNeedsBarrierFlagsConsistent(rt);

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
IsGrayListObject(JSObject *obj)
{
    MOZ_ASSERT(obj);
    return obj->is<CrossCompartmentWrapperObject>() && !IsDeadProxyObject(obj);
}

/* static */ unsigned
ProxyObject::grayLinkSlot(JSObject *obj)
{
    MOZ_ASSERT(IsGrayListObject(obj));
    return ProxyObject::EXTRA_SLOT + 1;
}

#ifdef DEBUG
static void
AssertNotOnGrayList(JSObject *obj)
{
    MOZ_ASSERT_IF(IsGrayListObject(obj),
                  obj->getReservedSlot(ProxyObject::grayLinkSlot(obj)).isUndefined());
}
#endif

static JSObject *
CrossCompartmentPointerReferent(JSObject *obj)
{
    MOZ_ASSERT(IsGrayListObject(obj));
    return &obj->as<ProxyObject>().private_().toObject();
}

static JSObject *
NextIncomingCrossCompartmentPointer(JSObject *prev, bool unlink)
{
    unsigned slot = ProxyObject::grayLinkSlot(prev);
    JSObject *next = prev->getReservedSlot(slot).toObjectOrNull();
    MOZ_ASSERT_IF(next, IsGrayListObject(next));

    if (unlink)
        prev->setSlot(slot, UndefinedValue());

    return next;
}

void
js::DelayCrossCompartmentGrayMarking(JSObject *src)
{
    MOZ_ASSERT(IsGrayListObject(src));

    /* Called from MarkCrossCompartmentXXX functions. */
    unsigned slot = ProxyObject::grayLinkSlot(src);
    JSObject *dest = CrossCompartmentPointerReferent(src);
    JSCompartment *comp = dest->compartment();

    if (src->getReservedSlot(slot).isUndefined()) {
        src->setCrossCompartmentSlot(slot, ObjectOrNullValue(comp->gcIncomingGrayPointers));
        comp->gcIncomingGrayPointers = src;
    } else {
        MOZ_ASSERT(src->getReservedSlot(slot).isObjectOrNull());
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
    MOZ_ASSERT(found);
#endif
}

static void
MarkIncomingCrossCompartmentPointers(JSRuntime *rt, const uint32_t color)
{
    MOZ_ASSERT(color == BLACK || color == GRAY);

    static const gcstats::Phase statsPhases[] = {
        gcstats::PHASE_SWEEP_MARK_INCOMING_BLACK,
        gcstats::PHASE_SWEEP_MARK_INCOMING_GRAY
    };
    gcstats::AutoPhase ap1(rt->gc.stats, statsPhases[color]);

    bool unlinkList = color == GRAY;

    for (GCCompartmentGroupIter c(rt); !c.done(); c.next()) {
        MOZ_ASSERT_IF(color == GRAY, c->zone()->isGCMarkingGray());
        MOZ_ASSERT_IF(color == BLACK, c->zone()->isGCMarkingBlack());
        MOZ_ASSERT_IF(c->gcIncomingGrayPointers, IsGrayListObject(c->gcIncomingGrayPointers));

        for (JSObject *src = c->gcIncomingGrayPointers;
             src;
             src = NextIncomingCrossCompartmentPointer(src, unlinkList))
        {
            JSObject *dst = CrossCompartmentPointerReferent(src);
            MOZ_ASSERT(dst->compartment() == c);

            if (color == GRAY) {
                if (IsObjectMarked(&src) && src->asTenured().isMarked(GRAY))
                    MarkGCThingUnbarriered(&rt->gc.marker, (void**)&dst,
                                           "cross-compartment gray pointer");
            } else {
                if (IsObjectMarked(&src) && !src->asTenured().isMarked(GRAY))
                    MarkGCThingUnbarriered(&rt->gc.marker, (void**)&dst,
                                           "cross-compartment black pointer");
            }
        }

        if (unlinkList)
            c->gcIncomingGrayPointers = nullptr;
    }

    SliceBudget budget;
    rt->gc.marker.drainMarkStack(budget);
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

    MOZ_CRASH("object not found in gray link list");
}

static void
ResetGrayList(JSCompartment *comp)
{
    JSObject *src = comp->gcIncomingGrayPointers;
    while (src)
        src = NextIncomingCrossCompartmentPointer(src, true);
    comp->gcIncomingGrayPointers = nullptr;
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

void
GCRuntime::endMarkingZoneGroup()
{
    gcstats::AutoPhase ap(stats, gcstats::PHASE_SWEEP_MARK);

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

void
GCRuntime::beginSweepingZoneGroup()
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
        zone->allocator.arenas.purge();

        if (rt->isAtomsZone(zone))
            sweepingAtoms = true;

        if (rt->sweepZoneCallback)
            rt->sweepZoneCallback(zone);

        zone->gcLastZoneGroupIndex = zoneGroupIndex;
    }

    validateIncrementalMarking();

    FreeOp fop(rt);

    {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_FINALIZE_START);
        callFinalizeCallbacks(&fop, JSFINALIZE_GROUP_START);
        callWeakPointerCallbacks();
    }

    if (sweepingAtoms) {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_SWEEP_ATOMS);
        rt->sweepAtoms();
    }

    {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_SWEEP_COMPARTMENTS);
        gcstats::AutoSCC scc(stats, zoneGroupIndex);

        {
            gcstats::AutoPhase apiv(stats, gcstats::PHASE_SWEEP_INNER_VIEWS);
            for (GCCompartmentGroupIter c(rt); !c.done(); c.next()) {
                c->sweepInnerViews();
            }
        }

        {
            gcstats::AutoPhase apccw(stats, gcstats::PHASE_SWEEP_CC_WRAPPER);
            for (GCCompartmentGroupIter c(rt); !c.done(); c.next()) {
                c->sweepCrossCompartmentWrappers();
            }
        }

        {
            gcstats::AutoPhase apbs(stats, gcstats::PHASE_SWEEP_BASE_SHAPE);
            for (GCCompartmentGroupIter c(rt); !c.done(); c.next()) {
                c->sweepBaseShapeTable();
            }
        }

        {
            gcstats::AutoPhase apis(stats, gcstats::PHASE_SWEEP_INITIAL_SHAPE);
            for (GCCompartmentGroupIter c(rt); !c.done(); c.next()) {
                c->sweepInitialShapeTable();
            }
        }

        {
            gcstats::AutoPhase apto(stats, gcstats::PHASE_SWEEP_TYPE_OBJECT);
            for (GCCompartmentGroupIter c(rt); !c.done(); c.next()) {
                c->sweepTypeObjectTables();
            }
        }

        {
            gcstats::AutoPhase apre(stats, gcstats::PHASE_SWEEP_REGEXP);
            for (GCCompartmentGroupIter c(rt); !c.done(); c.next()) {
                c->sweepRegExps();
            }
        }

        {
            gcstats::AutoPhase apmisc(stats, gcstats::PHASE_SWEEP_MISC);
            for (GCCompartmentGroupIter c(rt); !c.done(); c.next()) {
                c->sweepCallsiteClones();
                c->sweepSavedStacks();
                c->sweepGlobalObject(&fop);
                c->sweepSelfHostingScriptSource();
                c->sweepDebugScopes();
                c->sweepJitCompartment(&fop);
                c->sweepWeakMaps();
                c->sweepNativeIterators();
            }

            // Bug 1071218: the following two methods have not yet been
            // refactored to work on a single zone-group at once.

            // Collect watch points associated with unreachable objects.
            WatchpointMap::sweepAll(rt);

            // Detach unreachable debuggers and global objects from each other.
            Debugger::sweepAll(&fop);
        }

        {
            gcstats::AutoPhase apdc(stats, gcstats::PHASE_SWEEP_DISCARD_CODE);
            for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
                zone->discardJitCode(&fop);
            }
        }

        {
            gcstats::AutoPhase ap(stats, gcstats::PHASE_DISCARD_ANALYSIS);
            for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
                zone->sweepAnalysis(&fop, releaseObservedTypes && !zone->isPreservingCode());
            }
        }

        {
            gcstats::AutoPhase ap(stats, gcstats::PHASE_SWEEP_BREAKPOINT);
            for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
                zone->sweepBreakpoints(&fop);
            }
        }
    }

    if (sweepingAtoms) {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_SWEEP_SYMBOL_REGISTRY);
        rt->symbolRegistry().sweep();
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
        gcstats::AutoSCC scc(stats, zoneGroupIndex);
        zone->allocator.arenas.queueObjectsForSweep(&fop);
    }
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        gcstats::AutoSCC scc(stats, zoneGroupIndex);
        zone->allocator.arenas.queueStringsAndSymbolsForSweep(&fop);
    }
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        gcstats::AutoSCC scc(stats, zoneGroupIndex);
        zone->allocator.arenas.queueScriptsForSweep(&fop);
    }
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        gcstats::AutoSCC scc(stats, zoneGroupIndex);
        zone->allocator.arenas.queueJitCodeForSweep(&fop);
    }
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        gcstats::AutoSCC scc(stats, zoneGroupIndex);
        zone->allocator.arenas.queueShapesForSweep(&fop);
        zone->allocator.arenas.gcShapeArenasToSweep =
            zone->allocator.arenas.arenaListsToSweep[FINALIZE_SHAPE];
    }

    finalizePhase = 0;
    sweepZone = currentZoneGroup;
    sweepKindIndex = 0;

    {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_FINALIZE_END);
        callFinalizeCallbacks(&fop, JSFINALIZE_GROUP_END);
    }
}

void
GCRuntime::endSweepingZoneGroup()
{
    /* Update the GC state for zones we have swept and unlink the list. */
    for (GCZoneGroupIter zone(rt); !zone.done(); zone.next()) {
        MOZ_ASSERT(zone->isGCSweeping());
        zone->setGCState(Zone::Finished);
    }

    /* Reset the list of arenas marked as being allocated during sweep phase. */
    while (ArenaHeader *arena = arenasAllocatedDuringSweep) {
        arenasAllocatedDuringSweep = arena->getNextAllocDuringSweep();
        arena->unsetAllocDuringSweep();
    }
}

void
GCRuntime::beginSweepPhase(bool lastGC)
{
    /*
     * Sweep phase.
     *
     * Finalize as we sweep, outside of lock but with rt->isHeapBusy()
     * true so that any attempt to allocate a GC-thing from a finalizer will
     * fail, rather than nest badly and leave the unmarked newborn to be swept.
     */

    MOZ_ASSERT(!abortSweepAfterCurrentGroup);

    computeNonIncrementalMarkingForValidation();

    gcstats::AutoPhase ap(stats, gcstats::PHASE_SWEEP);

    sweepOnBackgroundThread =
        !lastGC && !TraceEnabled() && CanUseExtraThreads() && !shouldCompact();

    releaseObservedTypes = shouldReleaseObservedTypes();

#ifdef DEBUG
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
        MOZ_ASSERT(!c->gcIncomingGrayPointers);
        for (JSCompartment::WrapperEnum e(c); !e.empty(); e.popFront()) {
            if (e.front().key().kind != CrossCompartmentKey::StringWrapper)
                AssertNotOnGrayList(&e.front().value().get().toObject());
        }
    }
#endif

    DropStringWrappers(rt);
    findZoneGroups();
    endMarkingZoneGroup();
    beginSweepingZoneGroup();
}

bool
ArenaLists::foregroundFinalize(FreeOp *fop, AllocKind thingKind, SliceBudget &sliceBudget,
                               SortedArenaList &sweepList)
{
    if (!arenaListsToSweep[thingKind] && incrementalSweptArenas.isEmpty())
        return true;

    if (!FinalizeArenas(fop, &arenaListsToSweep[thingKind], sweepList, thingKind, sliceBudget)) {
        incrementalSweptArenaKind = thingKind;
        incrementalSweptArenas = sweepList.toArenaList();
        return false;
    }

    // Clear any previous incremental sweep state we may have saved.
    incrementalSweptArenas.clear();

    // Join |arenaLists[thingKind]| and |sweepList| into a single list.
    ArenaList finalized = sweepList.toArenaList();
    arenaLists[thingKind] = finalized.insertListWithCursorAtEnd(arenaLists[thingKind]);

    return true;
}

bool
GCRuntime::drainMarkStack(SliceBudget &sliceBudget, gcstats::Phase phase)
{
    /* Run a marking slice and return whether the stack is now empty. */
    gcstats::AutoPhase ap(stats, phase);
    return marker.drainMarkStack(sliceBudget);
}

bool
GCRuntime::sweepPhase(SliceBudget &sliceBudget)
{
    gcstats::AutoPhase ap(stats, gcstats::PHASE_SWEEP);
    FreeOp fop(rt);

    bool finished = drainMarkStack(sliceBudget, gcstats::PHASE_SWEEP_MARK);
    if (!finished)
        return false;

    for (;;) {
        /* Finalize foreground finalized things. */
        for (; finalizePhase < FinalizePhaseCount ; ++finalizePhase) {
            gcstats::AutoPhase ap(stats, FinalizePhaseStatsPhase[finalizePhase]);

            for (; sweepZone; sweepZone = sweepZone->nextNodeInGroup()) {
                Zone *zone = sweepZone;

                while (sweepKindIndex < FinalizePhaseLength[finalizePhase]) {
                    AllocKind kind = FinalizePhases[finalizePhase][sweepKindIndex];

                    /* Set the number of things per arena for this AllocKind. */
                    size_t thingsPerArena = Arena::thingsPerArena(Arena::thingSize(kind));
                    incrementalSweepList.setThingsPerArena(thingsPerArena);

                    if (!zone->allocator.arenas.foregroundFinalize(&fop, kind, sliceBudget,
                                                                   incrementalSweepList))
                        return false;  /* Yield to the mutator. */

                    /* Reset the slots of the sweep list that we used. */
                    incrementalSweepList.reset(thingsPerArena);

                    ++sweepKindIndex;
                }
                sweepKindIndex = 0;
            }
            sweepZone = currentZoneGroup;
        }

        /* Remove dead shapes from the shape tree, but don't finalize them yet. */
        {
            gcstats::AutoPhase ap(stats, gcstats::PHASE_SWEEP_SHAPE);

            for (; sweepZone; sweepZone = sweepZone->nextNodeInGroup()) {
                Zone *zone = sweepZone;
                while (ArenaHeader *arena = zone->allocator.arenas.gcShapeArenasToSweep) {
                    for (ArenaCellIterUnderGC i(arena); !i.done(); i.next()) {
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

        endSweepingZoneGroup();
        getNextZoneGroup();
        if (!currentZoneGroup)
            return true;  /* We're finished. */
        endMarkingZoneGroup();
        beginSweepingZoneGroup();
    }
}

void
GCRuntime::endSweepPhase(bool lastGC)
{
    gcstats::AutoPhase ap(stats, gcstats::PHASE_SWEEP);
    FreeOp fop(rt);

    MOZ_ASSERT_IF(lastGC, !sweepOnBackgroundThread);

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

    /*
     * If we found any black->gray edges during marking, we completely clear the
     * mark bits of all uncollected zones, or if a reset has occured, zones that
     * will no longer be collected. This is safe, although it may
     * prevent the cycle collector from collecting some dead objects.
     */
    if (foundBlackGrayEdges) {
        for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
            if (!zone->isCollecting())
                zone->allocator.arenas.unmarkAll();
        }
    }

    {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_DESTROY);

        /*
         * Sweep script filenames after sweeping functions in the generic loop
         * above. In this way when a scripted function's finalizer destroys the
         * script and calls rt->destroyScriptHook, the hook can still access the
         * script's filename. See bug 323267.
         */
        if (isFull)
            SweepScriptData(rt);

        /* Clear out any small pools that we're hanging on to. */
        if (jit::ExecutableAllocator *execAlloc = rt->maybeExecAlloc())
            execAlloc->purge();

        if (rt->jitRuntime() && rt->jitRuntime()->hasIonAlloc()) {
            JSRuntime::AutoLockForInterrupt lock(rt);
            rt->jitRuntime()->ionAlloc(rt)->purge();
        }

        /*
         * This removes compartments from rt->compartment, so we do it last to make
         * sure we don't miss sweeping any compartments.
         */
        if (!lastGC)
            sweepZones(&fop, lastGC);

        if (!sweepOnBackgroundThread) {
            /*
             * Destroy arenas after we finished the sweeping so finalizers can
             * safely use IsAboutToBeFinalized(). This is done on the
             * GCHelperState if possible. We acquire the lock only because
             * Expire needs to unlock it for other callers.
             */
            AutoLockGC lock(rt);
            expireChunksAndArenas(invocationKind == GC_SHRINK);
        }
    }

    {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_FINALIZE_END);
        callFinalizeCallbacks(&fop, JSFINALIZE_COLLECTION_END);

        /* If we finished a full GC, then the gray bits are correct. */
        if (isFull)
            grayBitsValid = true;
    }

    /* Set up list of zones for sweeping of background things. */
    MOZ_ASSERT(!sweepingZones);
    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        zone->gcNextGraphNode = sweepingZones;
        sweepingZones = zone;
    }

    /* If not sweeping on background thread then we must do it here. */
    if (!sweepOnBackgroundThread) {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_DESTROY);

        sweepBackgroundThings(false);

        rt->freeLifoAlloc.freeAll();

        /* Ensure the compartments get swept if it's the last GC. */
        if (lastGC)
            sweepZones(&fop, lastGC);
    }

    finishMarkingValidation();

#ifdef DEBUG
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        for (unsigned i = 0 ; i < FINALIZE_LIMIT ; ++i) {
            MOZ_ASSERT_IF(!IsBackgroundFinalized(AllocKind(i)) ||
                          !sweepOnBackgroundThread,
                          !zone->allocator.arenas.arenaListsToSweep[i]);
        }
    }

    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
        MOZ_ASSERT(!c->gcIncomingGrayPointers);

        for (JSCompartment::WrapperEnum e(c); !e.empty(); e.popFront()) {
            if (e.front().key().kind != CrossCompartmentKey::StringWrapper)
                AssertNotOnGrayList(&e.front().value().unbarrieredGet().toObject());
        }
    }
#endif
}

#ifdef JSGC_COMPACTING
void
GCRuntime::compactPhase()
{
    MOZ_ASSERT(rt->gc.nursery.isEmpty());
    MOZ_ASSERT(!sweepOnBackgroundThread);

    gcstats::AutoPhase ap(stats, gcstats::PHASE_COMPACT);

    ArenaHeader *relocatedList = relocateArenas();
    updatePointersToRelocatedCells();
    releaseRelocatedArenas(relocatedList);

#ifdef DEBUG
    CheckHashTablesAfterMovingGC(rt);
    for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
        if (!rt->isAtomsZone(zone) && !zone->isPreservingCode())
            zone->allocator.arenas.checkEmptyFreeLists();
    }
#endif
}
#endif // JSGC_COMPACTING

void
GCRuntime::finishCollection()
{
    MOZ_ASSERT(marker.isDrained());
    marker.stop();

    uint64_t currentTime = PRMJ_Now();
    schedulingState.updateHighFrequencyMode(lastGCTime, currentTime, tunables);

    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        zone->threshold.updateAfterGC(zone->usage.gcBytes(), invocationKind, tunables,
                                      schedulingState);
        if (zone->isCollecting()) {
            MOZ_ASSERT(zone->isGCFinished() || zone->isGCCompacting());
            zone->setGCState(Zone::NoGC);
            zone->active = false;
        }

        MOZ_ASSERT(!zone->isCollecting());
        MOZ_ASSERT(!zone->wasGCStarted());
    }

    lastGCTime = currentTime;
}

/* Start a new heap session. */
AutoTraceSession::AutoTraceSession(JSRuntime *rt, js::HeapState heapState)
  : lock(rt),
    runtime(rt),
    prevState(rt->gc.heapState)
{
    MOZ_ASSERT(rt->gc.isAllocAllowed());
    MOZ_ASSERT(rt->gc.heapState == Idle);
    MOZ_ASSERT(heapState != Idle);
#ifdef JSGC_GENERATIONAL
    MOZ_ASSERT_IF(heapState == MajorCollecting, rt->gc.nursery.isEmpty());
#endif

    // Threads with an exclusive context can hit refillFreeList while holding
    // the exclusive access lock. To avoid deadlocking when we try to acquire
    // this lock during GC and the other thread is waiting, make sure we hold
    // the exclusive access lock during GC sessions.
    MOZ_ASSERT(rt->currentThreadHasExclusiveAccess());

    if (rt->exclusiveThreadsPresent()) {
        // Lock the helper thread state when changing the heap state in the
        // presence of exclusive threads, to avoid racing with refillFreeList.
        AutoLockHelperThreadState lock;
        rt->gc.heapState = heapState;
    } else {
        rt->gc.heapState = heapState;
    }
}

AutoTraceSession::~AutoTraceSession()
{
    MOZ_ASSERT(runtime->isHeapBusy());

    if (runtime->exclusiveThreadsPresent()) {
        AutoLockHelperThreadState lock;
        runtime->gc.heapState = prevState;

        // Notify any helper threads waiting for the trace session to end.
        HelperThreadState().notifyAll(GlobalHelperThreadState::PRODUCER);
    } else {
        runtime->gc.heapState = prevState;
    }
}

AutoCopyFreeListToArenas::AutoCopyFreeListToArenas(JSRuntime *rt, ZoneSelector selector)
  : runtime(rt),
    selector(selector)
{
    for (ZonesIter zone(rt, selector); !zone.done(); zone.next())
        zone->allocator.arenas.copyFreeListsToArenas();
}

AutoCopyFreeListToArenas::~AutoCopyFreeListToArenas()
{
    for (ZonesIter zone(runtime, selector); !zone.done(); zone.next())
        zone->allocator.arenas.clearFreeListsInArenas();
}

class AutoCopyFreeListToArenasForGC
{
    JSRuntime *runtime;

  public:
    explicit AutoCopyFreeListToArenasForGC(JSRuntime *rt) : runtime(rt) {
        MOZ_ASSERT(rt->currentThreadHasExclusiveAccess());
        for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next())
            zone->allocator.arenas.copyFreeListsToArenas();
    }
    ~AutoCopyFreeListToArenasForGC() {
        for (ZonesIter zone(runtime, WithAtoms); !zone.done(); zone.next())
            zone->allocator.arenas.clearFreeListsInArenas();
    }
};

void
GCRuntime::resetIncrementalGC(const char *reason)
{
    switch (incrementalState) {
      case NO_INCREMENTAL:
        return;

      case MARK: {
        /* Cancel any ongoing marking. */
        AutoCopyFreeListToArenasForGC copy(rt);

        marker.reset();
        marker.stop();

        for (GCCompartmentsIter c(rt); !c.done(); c.next())
            ResetGrayList(c);

        for (GCZonesIter zone(rt); !zone.done(); zone.next()) {
            MOZ_ASSERT(zone->isGCMarking());
            zone->setNeedsIncrementalBarrier(false, Zone::UpdateJit);
            zone->setGCState(Zone::NoGC);
        }
        rt->setNeedsIncrementalBarrier(false);
        AssertNeedsBarrierFlagsConsistent(rt);

        incrementalState = NO_INCREMENTAL;

        MOZ_ASSERT(!marker.shouldCheckCompartments());

        break;
      }

      case SWEEP:
        marker.reset();

        for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next())
            c->scheduledForDestruction = false;

        /* Finish sweeping the current zone group, then abort. */
        abortSweepAfterCurrentGroup = true;
        incrementalCollectSlice(SliceBudget::Unlimited, JS::gcreason::RESET);

        {
            gcstats::AutoPhase ap(stats, gcstats::PHASE_WAIT_BACKGROUND_THREAD);
            rt->gc.waitBackgroundSweepOrAllocEnd();
        }
        break;

      default:
        MOZ_CRASH("Invalid incremental GC state");
    }

    stats.reset(reason);

#ifdef DEBUG
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        MOZ_ASSERT(!zone->needsIncrementalBarrier());
        for (unsigned i = 0; i < FINALIZE_LIMIT; ++i)
            MOZ_ASSERT(!zone->allocator.arenas.arenaListsToSweep[i]);
    }
#endif
}

namespace {

class AutoGCSlice {
  public:
    explicit AutoGCSlice(JSRuntime *rt);
    ~AutoGCSlice();

  private:
    JSRuntime *runtime;
};

} /* anonymous namespace */

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
        iter->compartment()->zone()->active = true;

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
    rt->setNeedsIncrementalBarrier(false);
    AssertNeedsBarrierFlagsConsistent(rt);
}

AutoGCSlice::~AutoGCSlice()
{
    /* We can't use GCZonesIter if this is the end of the last slice. */
    bool haveBarriers = false;
    for (ZonesIter zone(runtime, WithAtoms); !zone.done(); zone.next()) {
        if (zone->isGCMarking()) {
            zone->setNeedsIncrementalBarrier(true, Zone::UpdateJit);
            zone->allocator.arenas.prepareForIncrementalGC(runtime);
            haveBarriers = true;
        } else {
            zone->setNeedsIncrementalBarrier(false, Zone::UpdateJit);
        }
    }
    runtime->setNeedsIncrementalBarrier(haveBarriers);
    AssertNeedsBarrierFlagsConsistent(runtime);
}

void
GCRuntime::pushZealSelectedObjects()
{
#ifdef JS_GC_ZEAL
    /* Push selected objects onto the mark stack and clear the list. */
    for (JSObject **obj = selectedForMarking.begin(); obj != selectedForMarking.end(); obj++)
        MarkObjectUnbarriered(&marker, obj, "selected obj");
#endif
}

void
GCRuntime::incrementalCollectSlice(int64_t budget,
                                   JS::gcreason::Reason reason)
{
    MOZ_ASSERT(rt->currentThreadHasExclusiveAccess());

    AutoCopyFreeListToArenasForGC copy(rt);
    AutoGCSlice slice(rt);

    bool lastGC = (reason == JS::gcreason::DESTROY_RUNTIME);

    gc::State initialState = incrementalState;

    int zeal = 0;
#ifdef JS_GC_ZEAL
    if (reason == JS::gcreason::DEBUG_GC && budget != SliceBudget::Unlimited) {
        /*
         * Do the incremental collection type specified by zeal mode if the
         * collection was triggered by runDebugGC() and incremental GC has not
         * been cancelled by resetIncrementalGC().
         */
        zeal = zealMode;
    }
#endif

    MOZ_ASSERT_IF(incrementalState != NO_INCREMENTAL, isIncremental);
    isIncremental = budget != SliceBudget::Unlimited;

    if (zeal == ZealIncrementalRootsThenFinish || zeal == ZealIncrementalMarkAllThenFinish) {
        /*
         * Yields between slices occurs at predetermined points in these modes;
         * the budget is not used.
         */
        budget = SliceBudget::Unlimited;
    }

    SliceBudget sliceBudget(budget);

    if (incrementalState == NO_INCREMENTAL) {
        incrementalState = MARK_ROOTS;
        lastMarkSlice = false;
    }

    if (incrementalState == MARK)
        AutoGCRooter::traceAllWrappers(&marker);

    switch (incrementalState) {

      case MARK_ROOTS:
        if (!beginMarkPhase(reason)) {
            incrementalState = NO_INCREMENTAL;
            return;
        }

        if (!lastGC)
            pushZealSelectedObjects();

        incrementalState = MARK;

        if (isIncremental && zeal == ZealIncrementalRootsThenFinish)
            break;

        /* fall through */

      case MARK: {
        /* If we needed delayed marking for gray roots, then collect until done. */
        if (!marker.hasBufferedGrayRoots()) {
            sliceBudget.reset();
            isIncremental = false;
        }

        bool finished = drainMarkStack(sliceBudget, gcstats::PHASE_MARK);
        if (!finished)
            break;

        MOZ_ASSERT(marker.isDrained());

        if (!lastMarkSlice && isIncremental &&
            ((initialState == MARK && zeal != ZealIncrementalRootsThenFinish) ||
             zeal == ZealIncrementalMarkAllThenFinish))
        {
            /*
             * Yield with the aim of starting the sweep in the next
             * slice.  We will need to mark anything new on the stack
             * when we resume, so we stay in MARK state.
             */
            lastMarkSlice = true;
            break;
        }

        incrementalState = SWEEP;

        /*
         * This runs to completion, but we don't continue if the budget is
         * now exhasted.
         */
        beginSweepPhase(lastGC);
        if (sliceBudget.isOverBudget())
            break;

        /*
         * Always yield here when running in incremental multi-slice zeal
         * mode, so RunDebugGC can reset the slice buget.
         */
        if (isIncremental && zeal == ZealIncrementalMultipleSlices)
            break;

        /* fall through */
      }

      case SWEEP: {
        bool finished = sweepPhase(sliceBudget);
        if (!finished)
            break;

        endSweepPhase(lastGC);

        if (sweepOnBackgroundThread)
            helperState.startBackgroundSweep(invocationKind == GC_SHRINK);

#ifdef JSGC_COMPACTING
        if (shouldCompact()) {
            incrementalState = COMPACT;
            compactPhase();
        }
#endif

        finishCollection();
        incrementalState = NO_INCREMENTAL;
        break;
      }

      default:
        MOZ_ASSERT(false);
    }
}

IncrementalSafety
gc::IsIncrementalGCSafe(JSRuntime *rt)
{
    MOZ_ASSERT(!rt->mainThread.suppressGC);

    if (rt->keepAtoms())
        return IncrementalSafety::Unsafe("keepAtoms set");

    if (!rt->gc.isIncrementalGCAllowed())
        return IncrementalSafety::Unsafe("incremental permanently disabled");

    return IncrementalSafety::Safe();
}

void
GCRuntime::budgetIncrementalGC(int64_t *budget)
{
    IncrementalSafety safe = IsIncrementalGCSafe(rt);
    if (!safe) {
        resetIncrementalGC(safe.reason());
        *budget = SliceBudget::Unlimited;
        stats.nonincremental(safe.reason());
        return;
    }

    if (mode != JSGC_MODE_INCREMENTAL) {
        resetIncrementalGC("GC mode change");
        *budget = SliceBudget::Unlimited;
        stats.nonincremental("GC mode");
        return;
    }

    if (isTooMuchMalloc()) {
        *budget = SliceBudget::Unlimited;
        stats.nonincremental("malloc bytes trigger");
    }

    bool reset = false;
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        if (zone->usage.gcBytes() >= zone->threshold.gcTriggerBytes()) {
            *budget = SliceBudget::Unlimited;
            stats.nonincremental("allocation trigger");
        }

        if (incrementalState != NO_INCREMENTAL &&
            zone->isGCScheduled() != zone->wasGCStarted())
        {
            reset = true;
        }

        if (zone->isTooMuchMalloc()) {
            *budget = SliceBudget::Unlimited;
            stats.nonincremental("malloc bytes trigger");
        }
    }

    if (reset)
        resetIncrementalGC("zone change");
}

namespace {

#ifdef JSGC_GENERATIONAL
class AutoDisableStoreBuffer
{
    StoreBuffer &sb;
    bool prior;

  public:
    explicit AutoDisableStoreBuffer(GCRuntime *gc) : sb(gc->storeBuffer) {
        prior = sb.isEnabled();
        sb.disable();
    }
    ~AutoDisableStoreBuffer() {
        if (prior)
            sb.enable();
    }
};
#else
struct AutoDisableStoreBuffer
{
    AutoDisableStoreBuffer(GCRuntime *gc) {}
};
#endif

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
MOZ_NEVER_INLINE bool
GCRuntime::gcCycle(bool incremental, int64_t budget, JSGCInvocationKind gckind,
                   JS::gcreason::Reason reason)
{
    minorGC(reason);

    /*
     * Marking can trigger many incidental post barriers, some of them for
     * objects which are not going to be live after the GC.
     */
    AutoDisableStoreBuffer adsb(this);

    AutoTraceSession session(rt, MajorCollecting);

    isNeeded = false;
    interFrameGC = true;

    number++;
    if (incrementalState == NO_INCREMENTAL)
        majorGCNumber++;

    // It's ok if threads other than the main thread have suppressGC set, as
    // they are operating on zones which will not be collected from here.
    MOZ_ASSERT(!rt->mainThread.suppressGC);

    // Assert if this is a GC unsafe region.
    JS::AutoAssertOnGC::VerifyIsSafeToGC(rt);

    /*
     * As we about to purge caches and clear the mark bits we must wait for
     * any background finalization to finish. We must also wait for the
     * background allocation to finish so we can avoid taking the GC lock
     * when manipulating the chunks during the GC.
     */
    {
        gcstats::AutoPhase ap(stats, gcstats::PHASE_WAIT_BACKGROUND_THREAD);
        waitBackgroundSweepOrAllocEnd();
    }

    State prevState = incrementalState;

    if (!incremental) {
        /* If non-incremental GC was requested, reset incremental GC. */
        resetIncrementalGC("requested");
        stats.nonincremental("requested");
        budget = SliceBudget::Unlimited;
    } else {
        budgetIncrementalGC(&budget);
    }

    /* The GC was reset, so we need a do-over. */
    if (prevState != NO_INCREMENTAL && incrementalState == NO_INCREMENTAL)
        return true;

    TraceMajorGCStart();

    /* Set the invocation kind in the first slice. */
    if (incrementalState == NO_INCREMENTAL)
        invocationKind = gckind;

    incrementalCollectSlice(budget, reason);

#ifndef JS_MORE_DETERMINISTIC
    nextFullGCTime = PRMJ_Now() + GC_IDLE_FULL_SPAN;
#endif

    chunkAllocationSinceLastGC = false;

#ifdef JS_GC_ZEAL
    /* Keeping these around after a GC is dangerous. */
    clearSelectedForMarking();
#endif

    /* Clear gcMallocBytes for all compartments */
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        zone->resetGCMallocBytes();
        zone->unscheduleGC();
    }

    resetMallocBytes();

    TraceMajorGCEnd();

    return false;
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
ShouldCleanUpEverything(JS::gcreason::Reason reason, JSGCInvocationKind gckind)
{
    // During shutdown, we must clean everything up, for the sake of leak
    // detection. When a runtime has no contexts, or we're doing a GC before a
    // shutdown CC, those are strong indications that we're shutting down.
    return reason == JS::gcreason::DESTROY_RUNTIME ||
           reason == JS::gcreason::SHUTDOWN_CC ||
           gckind == GC_SHRINK;
}

gcstats::ZoneGCStats
GCRuntime::scanZonesBeforeGC()
{
    gcstats::ZoneGCStats zoneStats;
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        if (mode == JSGC_MODE_GLOBAL)
            zone->scheduleGC();

        /* This is a heuristic to avoid resets. */
        if (incrementalState != NO_INCREMENTAL && zone->needsIncrementalBarrier())
            zone->scheduleGC();

        zoneStats.zoneCount++;
        if (zone->isGCScheduled()) {
            zoneStats.collectedZoneCount++;
            zoneStats.collectedCompartmentCount += zone->compartments.length();
        }
    }

    for (CompartmentsIter c(rt, WithAtoms); !c.done(); c.next())
        zoneStats.compartmentCount++;

    return zoneStats;
}

void
GCRuntime::collect(bool incremental, int64_t budget, JSGCInvocationKind gckind,
                   JS::gcreason::Reason reason)
{
    /* GC shouldn't be running in parallel execution mode */
    MOZ_ALWAYS_TRUE(!InParallelSection());

    JS_AbortIfWrongThread(rt);

    /* If we attempt to invoke the GC while we are running in the GC, assert. */
    MOZ_ALWAYS_TRUE(!rt->isHeapBusy());

    /* The engine never locks across anything that could GC. */
    MOZ_ASSERT(!rt->currentThreadHasExclusiveAccess());

    if (rt->mainThread.suppressGC)
        return;

    TraceLogger *logger = TraceLoggerForMainThread(rt);
    AutoTraceLog logGC(logger, TraceLogger::GC);

#ifdef JS_GC_ZEAL
    if (deterministicOnly && !IsDeterministicGCReason(reason))
        return;
#endif

    MOZ_ASSERT_IF(!incremental || budget != SliceBudget::Unlimited, JSGC_INCREMENTAL);

    AutoStopVerifyingBarriers av(rt, reason == JS::gcreason::SHUTDOWN_CC ||
                                     reason == JS::gcreason::DESTROY_RUNTIME);

    recordNativeStackTop();

    gcstats::AutoGCSlice agc(stats, scanZonesBeforeGC(), reason);

    cleanUpEverything = ShouldCleanUpEverything(reason, gckind);

    bool repeat = false;
    do {
        /*
         * Let the API user decide to defer a GC if it wants to (unless this
         * is the last context). Invoke the callback regardless.
         */
        if (incrementalState == NO_INCREMENTAL) {
            gcstats::AutoPhase ap(stats, gcstats::PHASE_GC_BEGIN);
            if (gcCallback.op)
                gcCallback.op(rt, JSGC_BEGIN, gcCallback.data);
        }

        poked = false;
        bool wasReset = gcCycle(incremental, budget, gckind, reason);

        if (incrementalState == NO_INCREMENTAL) {
            gcstats::AutoPhase ap(stats, gcstats::PHASE_GC_END);
            if (gcCallback.op)
                gcCallback.op(rt, JSGC_END, gcCallback.data);
        }

        /* Need to re-schedule all zones for GC. */
        if (poked && cleanUpEverything)
            JS::PrepareForFullGC(rt);

        /*
         * This code makes an extra effort to collect compartments that we
         * thought were dead at the start of the GC. See the large comment in
         * beginMarkPhase.
         */
        bool repeatForDeadZone = false;
        if (incremental && incrementalState == NO_INCREMENTAL) {
            for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
                if (c->scheduledForDestruction) {
                    incremental = false;
                    repeatForDeadZone = true;
                    reason = JS::gcreason::COMPARTMENT_REVIVED;
                    c->zone()->scheduleGC();
                }
            }
        }

        /*
         * If we reset an existing GC, we need to start a new one. Also, we
         * repeat GCs that happen during shutdown (the gcShouldCleanUpEverything
         * case) until we can be sure that no additional garbage is created
         * (which typically happens if roots are dropped during finalizers).
         */
        repeat = (poked && cleanUpEverything) || wasReset || repeatForDeadZone;
    } while (repeat);

    if (incrementalState == NO_INCREMENTAL)
        EnqueuePendingParseTasksAfterGC(rt);
}

void
GCRuntime::gc(JSGCInvocationKind gckind, JS::gcreason::Reason reason)
{
    collect(false, SliceBudget::Unlimited, gckind, reason);
}

void
GCRuntime::gcSlice(JSGCInvocationKind gckind, JS::gcreason::Reason reason, int64_t millis)
{
    int64_t budget;
    if (millis)
        budget = SliceBudget::TimeBudget(millis);
    else if (schedulingState.inHighFrequencyGCMode() && tunables.isDynamicMarkSliceEnabled())
        budget = sliceBudget * IGC_MARK_SLICE_MULTIPLIER;
    else
        budget = sliceBudget;

    collect(true, budget, gckind, reason);
}

void
GCRuntime::gcFinalSlice(JSGCInvocationKind gckind, JS::gcreason::Reason reason)
{
    collect(true, SliceBudget::Unlimited, gckind, reason);
}

void
GCRuntime::notifyDidPaint()
{
#ifdef JS_GC_ZEAL
    if (zealMode == ZealFrameVerifierPreValue) {
        verifyPreBarriers();
        return;
    }

    if (zealMode == ZealFrameVerifierPostValue) {
        verifyPostBarriers();
        return;
    }

    if (zealMode == ZealFrameGCValue) {
        JS::PrepareForFullGC(rt);
        gcSlice(GC_NORMAL, JS::gcreason::REFRESH_FRAME);
        return;
    }
#endif

    if (JS::IsIncrementalGCInProgress(rt) && !interFrameGC) {
        JS::PrepareForIncrementalGC(rt);
        gcSlice(GC_NORMAL, JS::gcreason::REFRESH_FRAME);
    }

    interFrameGC = false;
}

static bool
ZonesSelected(JSRuntime *rt)
{
    for (ZonesIter zone(rt, WithAtoms); !zone.done(); zone.next()) {
        if (zone->isGCScheduled())
            return true;
    }
    return false;
}

void
GCRuntime::gcDebugSlice(bool limit, int64_t objCount)
{
    int64_t budget = limit ? SliceBudget::WorkBudget(objCount) : SliceBudget::Unlimited;
    if (!ZonesSelected(rt)) {
        if (JS::IsIncrementalGCInProgress(rt))
            JS::PrepareForIncrementalGC(rt);
        else
            JS::PrepareForFullGC(rt);
    }
    collect(true, budget, GC_NORMAL, JS::gcreason::DEBUG_GC);
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
    rt->gc.shrinkBuffers();
}

void
GCRuntime::shrinkBuffers()
{
    AutoLockHelperThreadState helperLock;
    AutoLockGC lock(rt);
    MOZ_ASSERT(!rt->isHeapBusy());

    if (CanUseExtraThreads())
        helperState.startBackgroundShrink();
    else
        expireChunksAndArenas(true);
}

void
GCRuntime::minorGC(JS::gcreason::Reason reason)
{
#ifdef JSGC_GENERATIONAL
    TraceLogger *logger = TraceLoggerForMainThread(rt);
    AutoTraceLog logMinorGC(logger, TraceLogger::MinorGC);
    nursery.collect(rt, reason, nullptr);
    MOZ_ASSERT_IF(!rt->mainThread.suppressGC, nursery.isEmpty());
#endif
}

void
GCRuntime::minorGC(JSContext *cx, JS::gcreason::Reason reason)
{
    // Alternate to the runtime-taking form above which allows marking type
    // objects as needing pretenuring.
#ifdef JSGC_GENERATIONAL
    TraceLogger *logger = TraceLoggerForMainThread(rt);
    AutoTraceLog logMinorGC(logger, TraceLogger::MinorGC);
    Nursery::TypeObjectList pretenureTypes;
    nursery.collect(rt, reason, &pretenureTypes);
    for (size_t i = 0; i < pretenureTypes.length(); i++) {
        if (pretenureTypes[i]->canPreTenure())
            pretenureTypes[i]->setShouldPreTenure(cx);
    }
    MOZ_ASSERT_IF(!rt->mainThread.suppressGC, nursery.isEmpty());
#endif
}

void
GCRuntime::disableGenerationalGC()
{
#ifdef JSGC_GENERATIONAL
    if (isGenerationalGCEnabled()) {
        minorGC(JS::gcreason::API);
        nursery.disable();
        storeBuffer.disable();
    }
#endif
    ++rt->gc.generationalDisabled;
}

void
GCRuntime::enableGenerationalGC()
{
    MOZ_ASSERT(generationalDisabled > 0);
    --generationalDisabled;
#ifdef JSGC_GENERATIONAL
    if (generationalDisabled == 0) {
        nursery.enable();
        storeBuffer.enable();
    }
#endif
}

void
GCRuntime::gcIfNeeded(JSContext *cx)
{
#ifdef JSGC_GENERATIONAL
    /*
     * In case of store buffer overflow perform minor GC first so that the
     * correct reason is seen in the logs.
     */
    if (storeBuffer.isAboutToOverflow())
        minorGC(cx, JS::gcreason::FULL_STORE_BUFFER);
#endif

    if (isNeeded)
        gcSlice(GC_NORMAL, rt->gc.triggerReason, 0);
}

AutoFinishGC::AutoFinishGC(JSRuntime *rt)
{
    if (JS::IsIncrementalGCInProgress(rt)) {
        JS::PrepareForIncrementalGC(rt);
        JS::FinishIncrementalGC(rt, JS::gcreason::API);
    }

    rt->gc.waitBackgroundSweepEnd();
}

AutoPrepareForTracing::AutoPrepareForTracing(JSRuntime *rt, ZoneSelector selector)
  : finish(rt),
    session(rt),
    copy(rt, selector)
{
    rt->gc.recordNativeStackTop();
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
            return nullptr;

        zoneHolder.reset(zone);

        const JSPrincipals *trusted = rt->trustedPrincipals();
        bool isSystem = principals && principals == trusted;
        if (!zone->init(isSystem))
            return nullptr;
    }

    ScopedJSDeletePtr<JSCompartment> compartment(cx->new_<JSCompartment>(zone, options));
    if (!compartment || !compartment->init(cx))
        return nullptr;

    // Set up the principals.
    JS_SetCompartmentPrincipals(compartment, principals);

    AutoLockGC lock(rt);

    if (!zone->compartments.append(compartment.get())) {
        js_ReportOutOfMemory(cx);
        return nullptr;
    }

    if (zoneHolder && !rt->gc.zones.append(zone)) {
        js_ReportOutOfMemory(cx);
        return nullptr;
    }

    zoneHolder.forget();
    return compartment.forget();
}

void
gc::MergeCompartments(JSCompartment *source, JSCompartment *target)
{
    // The source compartment must be specifically flagged as mergable.  This
    // also implies that the compartment is not visible to the debugger.
    MOZ_ASSERT(source->options_.mergeable());

    MOZ_ASSERT(source->addonId == target->addonId);

    JSRuntime *rt = source->runtimeFromMainThread();

    AutoPrepareForTracing prepare(rt, SkipAtoms);

    // Cleanup tables and other state in the source compartment that will be
    // meaningless after merging into the target compartment.

    source->clearTables();

    // Fixup compartment pointers in source to refer to target.

    for (ZoneCellIter iter(source->zone(), FINALIZE_SCRIPT); !iter.done(); iter.next()) {
        JSScript *script = iter.get<JSScript>();
        MOZ_ASSERT(script->compartment() == source);
        script->compartment_ = target;
    }

    for (ZoneCellIter iter(source->zone(), FINALIZE_BASE_SHAPE); !iter.done(); iter.next()) {
        BaseShape *base = iter.get<BaseShape>();
        MOZ_ASSERT(base->compartment() == source);
        base->compartment_ = target;
    }

    // Fixup zone pointers in source's zone to refer to target's zone.

    for (size_t thingKind = 0; thingKind != FINALIZE_LIMIT; thingKind++) {
        for (ArenaIter aiter(source->zone(), AllocKind(thingKind)); !aiter.done(); aiter.next()) {
            ArenaHeader *aheader = aiter.get();
            aheader->zone = target->zone();
        }
    }

    // The source should be the only compartment in its zone.
    for (CompartmentsInZoneIter c(source->zone()); !c.done(); c.next())
        MOZ_ASSERT(c.get() == source);

    // Merge the allocator in source's zone into target's zone.
    target->zone()->allocator.arenas.adoptArenas(rt, &source->zone()->allocator.arenas);
    target->zone()->usage.adopt(source->zone()->usage);

    // Merge other info in source's zone into target's zone.
    target->zone()->types.typeLifoAlloc.transferFrom(&source->zone()->types.typeLifoAlloc);
}

void
GCRuntime::runDebugGC()
{
#ifdef JS_GC_ZEAL
    int type = zealMode;

    if (rt->mainThread.suppressGC)
        return;

    if (type == js::gc::ZealGenerationalGCValue)
        return minorGC(JS::gcreason::DEBUG_GC);

    PrepareForDebugGC(rt);

    if (type == ZealIncrementalRootsThenFinish ||
        type == ZealIncrementalMarkAllThenFinish ||
        type == ZealIncrementalMultipleSlices)
    {
        js::gc::State initialState = incrementalState;
        int64_t budget;
        if (type == ZealIncrementalMultipleSlices) {
            /*
             * Start with a small slice limit and double it every slice. This
             * ensure that we get multiple slices, and collection runs to
             * completion.
             */
            if (initialState == NO_INCREMENTAL)
                incrementalLimit = zealFrequency / 2;
            else
                incrementalLimit *= 2;
            budget = SliceBudget::WorkBudget(incrementalLimit);
        } else {
            // This triggers incremental GC but is actually ignored by IncrementalMarkSlice.
            budget = SliceBudget::WorkBudget(1);
        }

        collect(true, budget, GC_NORMAL, JS::gcreason::DEBUG_GC);

        /*
         * For multi-slice zeal, reset the slice size when we get to the sweep
         * phase.
         */
        if (type == ZealIncrementalMultipleSlices &&
            initialState == MARK && incrementalState == SWEEP)
        {
            incrementalLimit = zealFrequency / 2;
        }
    } else if (type == ZealCompactValue) {
        collect(false, SliceBudget::Unlimited, GC_SHRINK, JS::gcreason::DEBUG_GC);
    } else {
        collect(false, SliceBudget::Unlimited, GC_NORMAL, JS::gcreason::DEBUG_GC);
    }

#endif
}

void
GCRuntime::setValidate(bool enabled)
{
    MOZ_ASSERT(!isHeapMajorCollecting());
    validate = enabled;
}

void
GCRuntime::setFullCompartmentChecks(bool enabled)
{
    MOZ_ASSERT(!isHeapMajorCollecting());
    fullCompartmentChecks = enabled;
}

#ifdef JS_GC_ZEAL
bool
GCRuntime::selectForMarking(JSObject *object)
{
    MOZ_ASSERT(!isHeapMajorCollecting());
    return selectedForMarking.append(object);
}

void
GCRuntime::clearSelectedForMarking()
{
    selectedForMarking.clearAndFree();
}

void
GCRuntime::setDeterministic(bool enabled)
{
    MOZ_ASSERT(!isHeapMajorCollecting());
    deterministicOnly = enabled;
}
#endif

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
#ifdef JSGC_GENERATIONAL
    /*
     * Scripts can entrain nursery things, inserting references to the script
     * into the store buffer. Clear the store buffer before discarding scripts.
     */
    fop->runtime()->gc.evictNursery();
#endif

    for (ZonesIter zone(fop->runtime(), SkipAtoms); !zone.done(); zone.next()) {
        if (!zone->jitZone())
            continue;

#ifdef DEBUG
        /* Assert no baseline scripts are marked as active. */
        for (ZoneCellIter i(zone, FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            MOZ_ASSERT_IF(script->hasBaselineScript(), !script->baselineScript()->active());
        }
#endif

        /* Mark baseline scripts on the stack as active. */
        jit::MarkActiveBaselineScripts(zone);

        jit::InvalidateAll(fop, zone);

        for (ZoneCellIter i(zone, FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            jit::FinishInvalidation<SequentialExecution>(fop, script);
            jit::FinishInvalidation<ParallelExecution>(fop, script);

            /*
             * Discard baseline script if it's not marked as active. Note that
             * this also resets the active flag.
             */
            jit::FinishDiscardBaselineScript(fop, script);
        }

        zone->jitZone()->optimizedStubSpace()->free();
    }
}

void
js::PurgeJITCaches(Zone *zone)
{
    for (ZoneCellIterUnderGC i(zone, FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();

        /* Discard Ion caches. */
        jit::PurgeCaches(script);
    }
}

void
ArenaLists::normalizeBackgroundFinalizeState(AllocKind thingKind)
{
    ArenaLists::BackgroundFinalizeState *bfs = &backgroundFinalizeState[thingKind];
    switch (*bfs) {
      case BFS_DONE:
        break;
      case BFS_JUST_FINISHED:
        // No allocations between end of last sweep and now.
        // Transfering over arenas is a kind of allocation.
        *bfs = BFS_DONE;
        break;
      default:
        MOZ_ASSERT(!"Background finalization in progress, but it should not be.");
        break;
    }
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
        // When we enter a parallel section, we join the background
        // thread, and we do not run GC while in the parallel section,
        // so no finalizer should be active!
        normalizeBackgroundFinalizeState(AllocKind(thingKind));
        fromArenaLists->normalizeBackgroundFinalizeState(AllocKind(thingKind));

        ArenaList *fromList = &fromArenaLists->arenaLists[thingKind];
        ArenaList *toList = &arenaLists[thingKind];
        fromList->check();
        toList->check();
        ArenaHeader *next;
        for (ArenaHeader *fromHeader = fromList->head(); fromHeader; fromHeader = next) {
            // Copy fromHeader->next before releasing/reinserting.
            next = fromHeader->next;

            // During parallel execution, we sometimes keep empty arenas
            // on the lists rather than sending them back to the chunk.
            // Therefore, if fromHeader is empty, send it back to the
            // chunk now. Otherwise, attach to |toList|.
            if (fromHeader->isEmpty())
                fromHeader->chunk()->releaseArena(fromHeader);
            else
                toList->insertAtCursor(fromHeader);
        }
        fromList->clear();
        toList->check();
    }
}

bool
ArenaLists::containsArena(JSRuntime *rt, ArenaHeader *needle)
{
    AutoLockGC lock(rt);
    size_t allocKind = needle->getAllocKind();
    for (ArenaHeader *aheader = arenaLists[allocKind].head(); aheader; aheader = aheader->next) {
        if (aheader == needle)
            return true;
    }
    return false;
}


AutoSuppressGC::AutoSuppressGC(ExclusiveContext *cx)
  : suppressGC_(cx->perThreadData->suppressGC)
{
    suppressGC_++;
}

AutoSuppressGC::AutoSuppressGC(JSCompartment *comp)
  : suppressGC_(comp->runtimeFromMainThread()->mainThread.suppressGC)
{
    suppressGC_++;
}

AutoSuppressGC::AutoSuppressGC(JSRuntime *rt)
  : suppressGC_(rt->mainThread.suppressGC)
{
    suppressGC_++;
}

bool
js::UninlinedIsInsideNursery(const gc::Cell *cell)
{
    return IsInsideNursery(cell);
}

#ifdef DEBUG
AutoDisableProxyCheck::AutoDisableProxyCheck(JSRuntime *rt
                                             MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
  : gc(rt->gc)
{
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    gc.disableStrictProxyChecking();
}

AutoDisableProxyCheck::~AutoDisableProxyCheck()
{
    gc.enableStrictProxyChecking();
}

JS_FRIEND_API(void)
JS::AssertGCThingMustBeTenured(JSObject *obj)
{
    MOZ_ASSERT(obj->isTenured() &&
               (!IsNurseryAllocable(obj->asTenured().getAllocKind()) || obj->getClass()->finalize));
}

JS_FRIEND_API(void)
js::gc::AssertGCThingHasType(js::gc::Cell *cell, JSGCTraceKind kind)
{
    MOZ_ASSERT(cell);
    if (IsInsideNursery(cell))
        MOZ_ASSERT(kind == JSTRACE_OBJECT);
    else
        MOZ_ASSERT(MapAllocToTraceKind(cell->asTenured().getAllocKind()) == kind);
}

JS_FRIEND_API(size_t)
JS::GetGCNumber()
{
    JSRuntime *rt = js::TlsPerThreadData.get()->runtimeFromMainThread();
    if (!rt)
        return 0;
    return rt->gc.gcNumber();
}
#endif

#ifdef DEBUG
JS::AutoAssertOnGC::AutoAssertOnGC()
  : gc(nullptr), gcNumber(0)
{
    js::PerThreadData *data = js::TlsPerThreadData.get();
    if (data) {
        /*
         * GC's from off-thread will always assert, so off-thread is implicitly
         * AutoAssertOnGC. We still need to allow AutoAssertOnGC to be used in
         * code that works from both threads, however. We also use this to
         * annotate the off thread run loops.
         */
        JSRuntime *runtime = data->runtimeIfOnOwnerThread();
        if (runtime) {
            gc = &runtime->gc;
            gcNumber = gc->gcNumber();
            gc->enterUnsafeRegion();
        }
    }
}

JS::AutoAssertOnGC::AutoAssertOnGC(JSRuntime *rt)
  : gc(&rt->gc), gcNumber(rt->gc.gcNumber())
{
    gc->enterUnsafeRegion();
}

JS::AutoAssertOnGC::~AutoAssertOnGC()
{
    if (gc) {
        gc->leaveUnsafeRegion();

        /*
         * The following backstop assertion should never fire: if we bumped the
         * gcNumber, we should have asserted because inUnsafeRegion was true.
         */
        MOZ_ASSERT(gcNumber == gc->gcNumber(), "GC ran inside an AutoAssertOnGC scope.");
    }
}

/* static */ void
JS::AutoAssertOnGC::VerifyIsSafeToGC(JSRuntime *rt)
{
    if (rt->gc.isInsideUnsafeRegion())
        MOZ_CRASH("[AutoAssertOnGC] possible GC in GC-unsafe region");
}

JS::AutoAssertNoAlloc::AutoAssertNoAlloc(JSRuntime *rt)
  : gc(nullptr)
{
    disallowAlloc(rt);
}

void JS::AutoAssertNoAlloc::disallowAlloc(JSRuntime *rt)
{
    MOZ_ASSERT(!gc);
    gc = &rt->gc;
    gc->disallowAlloc();
}

JS::AutoAssertNoAlloc::~AutoAssertNoAlloc()
{
    if (gc)
        gc->allowAlloc();
}
#endif

JS::AutoAssertGCCallback::AutoAssertGCCallback(JSObject *obj)
  : AutoSuppressGCAnalysis()
{
    MOZ_ASSERT(obj->runtimeFromMainThread()->isHeapMajorCollecting());
}

#ifdef JSGC_HASH_TABLE_CHECKS
void
js::gc::CheckHashTablesAfterMovingGC(JSRuntime *rt)
{
    /*
     * Check that internal hash tables no longer have any pointers to things
     * that have been moved.
     */
    for (CompartmentsIter c(rt, SkipAtoms); !c.done(); c.next()) {
        c->checkTypeObjectTablesAfterMovingGC();
        c->checkInitialShapesTableAfterMovingGC();
        c->checkWrapperMapAfterMovingGC();
        if (c->debugScopes)
            c->debugScopes->checkHashTablesAfterMovingGC(rt);
    }
}
#endif
