/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/AtomMarking.h"

#include "jscompartment.h"

#include "jsgcinlines.h"
#include "gc/Heap-inl.h"

namespace js {
namespace gc {

// Atom Marking Overview
//
// Things in the atoms zone (which includes atomized strings and other things,
// all of which we will refer to as 'atoms' here) may be pointed to freely by
// things in other zones. To avoid the need to perform garbage collections of
// the entire runtime to collect atoms, we compute a separate atom mark bitmap
// for each zone that is always an overapproximation of the atoms that zone is
// using. When an atom is not in the mark bitmap for any zone, it can be
// destroyed.
//
// To minimize interference with the rest of the GC, atom marking and sweeping
// is done by manipulating the mark bitmaps in the chunks used for the atoms.
// When the atoms zone is being collected, the mark bitmaps for the chunk(s)
// used by the atoms are updated normally during marking. After marking
// finishes, the chunk mark bitmaps are translated to a more efficient atom
// mark bitmap (see below) that is stored on the zones which the GC collected
// (computeBitmapFromChunkMarkBits). Before sweeping begins, the chunk mark
// bitmaps are updated with any atoms that might be referenced by zones which
// weren't collected (updateChunkMarkBits). The GC sweeping will then release
// all atoms which are not marked by any zone.
//
// The representation of atom mark bitmaps is as follows:
//
// Each arena in the atoms zone has an atomBitmapStart() value indicating the
// word index into the bitmap of the first thing in the arena. Each arena uses
// ArenaBitmapWords of data to store its bitmap, which uses the same
// representation as chunk mark bitmaps: one bit is allocated per Cell, with
// bits for space between things being unused when things are larger than a
// single Cell.

void
AtomMarkingRuntime::registerArena(Arena* arena)
{
    MOZ_ASSERT(arena->getThingSize() != 0);
    MOZ_ASSERT(arena->getThingSize() % CellSize == 0);
    MOZ_ASSERT(arena->zone->isAtomsZone());
    MOZ_ASSERT(arena->zone->runtimeFromAnyThread()->currentThreadHasExclusiveAccess());

    // We need to find a range of bits from the atoms bitmap for this arena.

    // Look for a free range of bits compatible with this arena.
    if (freeArenaIndexes.ref().length()) {
        arena->atomBitmapStart() = freeArenaIndexes.ref().popCopy();
        return;
    }

    // Allocate a range of bits from the end for this arena.
    arena->atomBitmapStart() = allocatedWords;
    allocatedWords += ArenaBitmapWords;
}

void
AtomMarkingRuntime::unregisterArena(Arena* arena)
{
    MOZ_ASSERT(arena->zone->isAtomsZone());

    // Leak these atom bits if we run out of memory.
    mozilla::Unused << freeArenaIndexes.ref().emplaceBack(arena->atomBitmapStart());
}

bool
AtomMarkingRuntime::computeBitmapFromChunkMarkBits(JSRuntime* runtime, DenseBitmap& bitmap)
{
    MOZ_ASSERT(runtime->currentThreadHasExclusiveAccess());

    if (!bitmap.ensureSpace(allocatedWords))
        return false;

    Zone* atomsZone = runtime->unsafeAtomsCompartment()->zone();
    for (auto thingKind : AllAllocKinds()) {
        for (ArenaIter aiter(atomsZone, thingKind); !aiter.done(); aiter.next()) {
            Arena* arena = aiter.get();
            uintptr_t* chunkWords = arena->chunk()->bitmap.arenaBits(arena);
            bitmap.copyBitsFrom(arena->atomBitmapStart(), ArenaBitmapWords, chunkWords);
        }
    }

    return true;
}

void
AtomMarkingRuntime::updateZoneBitmap(Zone* zone, const DenseBitmap& bitmap)
{
    if (zone->isAtomsZone())
        return;

    // Take the bitwise and between the two mark bitmaps to get the best new
    // overapproximation we can. |bitmap| might include bits that are not in
    // the zone's mark bitmap, if additional zones were collected by the GC.
    zone->markedAtoms().bitwiseAndWith(bitmap);
}

// Set any bits in the chunk mark bitmaps for atoms which are marked in bitmap.
template <typename Bitmap>
static void
AddBitmapToChunkMarkBits(JSRuntime* runtime, Bitmap& bitmap)
{
    // Make sure that by copying the mark bits for one arena in word sizes we
    // do not affect the mark bits for other arenas.
    static_assert(ArenaBitmapBits == ArenaBitmapWords * JS_BITS_PER_WORD,
                  "ArenaBitmapWords must evenly divide ArenaBitmapBits");

    Zone* atomsZone = runtime->unsafeAtomsCompartment()->zone();
    for (auto thingKind : AllAllocKinds()) {
        for (ArenaIter aiter(atomsZone, thingKind); !aiter.done(); aiter.next()) {
            Arena* arena = aiter.get();
            uintptr_t* chunkWords = arena->chunk()->bitmap.arenaBits(arena);
            bitmap.bitwiseOrRangeInto(arena->atomBitmapStart(), ArenaBitmapWords, chunkWords);
        }
    }
}

void
AtomMarkingRuntime::updateChunkMarkBits(JSRuntime* runtime)
{
    MOZ_ASSERT(runtime->currentThreadHasExclusiveAccess());

    // Try to compute a simple union of the zone atom bitmaps before updating
    // the chunk mark bitmaps. If this allocation fails then fall back to
    // updating the chunk mark bitmaps separately for each zone.
    DenseBitmap markedUnion;
    if (markedUnion.ensureSpace(allocatedWords)) {
        for (ZonesIter zone(runtime, SkipAtoms); !zone.done(); zone.next()) {
            // We only need to update the chunk mark bits for zones which were
            // not collected in the current GC. Atoms which are referenced by
            // collected zones have already been marked.
            if (!zone->isCollectingFromAnyThread())
                zone->markedAtoms().bitwiseOrInto(markedUnion);
        }
        AddBitmapToChunkMarkBits(runtime, markedUnion);
    } else {
        for (ZonesIter zone(runtime, SkipAtoms); !zone.done(); zone.next()) {
            if (!zone->isCollectingFromAnyThread())
                AddBitmapToChunkMarkBits(runtime, zone->markedAtoms());
        }
    }
}

static inline size_t
GetAtomBit(TenuredCell* thing)
{
    MOZ_ASSERT(thing->zoneFromAnyThread()->isAtomsZone());
    Arena* arena = thing->arena();
    size_t arenaBit = (reinterpret_cast<uintptr_t>(thing) - arena->address()) / CellSize;
    return arena->atomBitmapStart() * JS_BITS_PER_WORD + arenaBit;
}

static bool
ThingIsPermanent(TenuredCell* thing)
{
    JS::TraceKind kind = thing->getTraceKind();
    if (kind == JS::TraceKind::String && static_cast<JSString*>(thing)->isPermanentAtom())
        return true;
    if (kind == JS::TraceKind::Symbol && static_cast<JS::Symbol*>(thing)->isWellKnownSymbol())
        return true;
    return false;
}

void
AtomMarkingRuntime::markAtom(JSContext* cx, TenuredCell* thing)
{
    // The context's zone will be null during initialization of the runtime.
    if (!thing || !cx->zone())
        return;
    MOZ_ASSERT(!cx->zone()->isAtomsZone());

    if (ThingIsPermanent(thing) || !thing->zoneFromAnyThread()->isAtomsZone())
        return;

    size_t bit = GetAtomBit(thing);
    MOZ_ASSERT(bit / JS_BITS_PER_WORD < allocatedWords);

    cx->zone()->markedAtoms().setBit(bit);

    if (!cx->helperThread()) {
        // Trigger a read barrier on the atom, in case there is an incremental
        // GC in progress. This is necessary if the atom is being marked
        // because a reference to it was obtained from another zone which is
        // not being collected by the incremental GC.
        TenuredCell::readBarrier(thing);
    }

    // Children of the thing also need to be marked in the context's zone.
    // We don't have a JSTracer for this so manually handle the cases in which
    // an atom can reference other atoms.
    if (thing->getTraceKind() == JS::TraceKind::Symbol) {
        JSAtom* description = static_cast<JS::Symbol*>(thing)->description();
        markAtom(cx, description);
    }
}

void
AtomMarkingRuntime::markId(JSContext* cx, jsid id)
{
    if (JSID_IS_GCTHING(id))
        markAtom(cx, &JSID_TO_GCTHING(id).asCell()->asTenured());
}

void
AtomMarkingRuntime::markAtomValue(JSContext* cx, const Value& value)
{
    if (value.isGCThing()) {
        Cell* thing = value.toGCThing();
        if (thing && !IsInsideNursery(thing))
            markAtom(cx, &thing->asTenured());
    }
}

void
AtomMarkingRuntime::adoptMarkedAtoms(Zone* target, Zone* source)
{
    MOZ_ASSERT(target->runtimeFromAnyThread()->currentThreadHasExclusiveAccess());
    target->markedAtoms().bitwiseOrWith(source->markedAtoms());
}

#ifdef DEBUG

bool
AtomMarkingRuntime::atomIsMarked(Zone* zone, Cell* thingArg)
{
    if (!thingArg || IsInsideNursery(thingArg))
        return true;
    TenuredCell* thing = &thingArg->asTenured();

    if (!zone->runtimeFromAnyThread()->permanentAtoms)
        return true;

    if (ThingIsPermanent(thing) || !thing->zoneFromAnyThread()->isAtomsZone())
        return true;

    JS::TraceKind kind = thing->getTraceKind();
    if (kind == JS::TraceKind::String) {
        JSAtom* atom = static_cast<JSAtom*>(thing);
        if (AtomIsPinnedInRuntime(zone->runtimeFromAnyThread(), atom))
            return true;
    }

    size_t bit = GetAtomBit(thing);
    return zone->markedAtoms().getBit(bit);
}

bool
AtomMarkingRuntime::idIsMarked(Zone* zone, jsid id)
{
    if (JSID_IS_GCTHING(id))
        return atomIsMarked(zone, JSID_TO_GCTHING(id).asCell());
    return true;
}

bool
AtomMarkingRuntime::valueIsMarked(Zone* zone, const Value& value)
{
    if (value.isGCThing())
        return atomIsMarked(zone, value.toGCThing());
    return true;
}

#endif // DEBUG

} // namespace gc

#ifdef DEBUG

bool
AtomIsMarked(Zone* zone, JSAtom* atom)
{
    return zone->runtimeFromAnyThread()->gc.atomMarking.atomIsMarked(zone, atom);
}

bool
AtomIsMarked(Zone* zone, jsid id)
{
    return zone->runtimeFromAnyThread()->gc.atomMarking.idIsMarked(zone, id);
}

bool
AtomIsMarked(Zone* zone, const Value& value)
{
    return zone->runtimeFromAnyThread()->gc.atomMarking.valueIsMarked(zone, value);
}

#endif // DEBUG

} // namespace js
