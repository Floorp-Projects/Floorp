/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_AtomMarking_h
#define gc_AtomMarking_h

#include "NamespaceImports.h"
#include "gc/Heap.h"

namespace js {
namespace gc {

// This class manages state used for marking atoms during GCs.
// See AtomMarking.cpp for details.
class AtomMarkingRuntime
{
    // Unused arena atom bitmap indexes. Protected by the GC lock.
    Vector<size_t, 0, SystemAllocPolicy> freeArenaIndexes;

    // The extent of all allocated and free words in atom mark bitmaps.
    // This monotonically increases and may be read from without locking.
    mozilla::Atomic<size_t> allocatedWords;

  public:
    typedef Vector<uintptr_t, 0, SystemAllocPolicy> Bitmap;

    AtomMarkingRuntime()
      : allocatedWords(0)
    {}

    // Mark an arena as holding things in the atoms zone.
    void registerArena(Arena* arena);

    // Mark an arena as no longer holding things in the atoms zone.
    void unregisterArena(Arena* arena);

    // Fill |bitmap| with an atom marking bitmap based on the things that are
    // currently marked in the chunks used by atoms zone arenas. This returns
    // false on an allocation failure (but does not report an exception).
    bool computeBitmapFromChunkMarkBits(JSRuntime* runtime, Bitmap& bitmap);

    // Update the atom marking bitmap in |zone| according to another
    // overapproximation of the reachable atoms in |bitmap|.
    void updateZoneBitmap(Zone* zone, const Bitmap& bitmap);

    // Set any bits in the chunk mark bitmaps for atoms which are marked in any
    // zone in the runtime.
    void updateChunkMarkBits(JSRuntime* runtime);

    // Mark an atom or id as being newly reachable by the context's zone.
    void markAtom(ExclusiveContext* cx, TenuredCell* thing);
    void markId(ExclusiveContext* cx, jsid id);
    void markAtomValue(ExclusiveContext* cx, const Value& value);

    // Mark all atoms in |source| as being reachable within |target|.
    void adoptMarkedAtoms(Zone* target, Zone* source);

#ifdef DEBUG
    // Return whether |thing/id| is in the atom marking bitmap for |zone|.
    bool atomIsMarked(Zone* zone, Cell* thing);
    bool idIsMarked(Zone* zone, jsid id);
    bool valueIsMarked(Zone* zone, const Value& value);
#endif
};

} // namespace gc
} // namespace js

#endif // gc_AtomMarking_h
