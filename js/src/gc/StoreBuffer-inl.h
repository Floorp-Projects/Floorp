/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_StoreBuffer_inl_h
#define gc_StoreBuffer_inl_h

#include "gc/StoreBuffer.h"

#include "gc/Heap.h"

namespace js {
namespace gc {

inline /* static */ size_t
ArenaCellSet::getCellIndex(const TenuredCell* cell)
{
    MOZ_ASSERT((uintptr_t(cell) & ~ArenaMask) % CellSize == 0);
    return (uintptr_t(cell) & ArenaMask) / CellSize;
}

inline /* static */ void
ArenaCellSet::getWordIndexAndMask(size_t cellIndex, size_t* wordp, uint32_t* maskp)
{
    BitArray<ArenaCellCount>::getIndexAndMask(cellIndex, wordp, maskp);
}

inline bool
ArenaCellSet::hasCell(size_t cellIndex) const
{
    MOZ_ASSERT(cellIndex < ArenaCellCount);
    return bits.get(cellIndex);
}

inline void
ArenaCellSet::putCell(size_t cellIndex)
{
    MOZ_ASSERT(cellIndex < ArenaCellCount);
    bits.set(cellIndex);
}

inline void
ArenaCellSet::check() const
{
#ifdef DEBUG
    bool bitsZero = bits.isAllClear();
    MOZ_ASSERT(isEmpty() == bitsZero);
    MOZ_ASSERT(isEmpty() == !arena);
    MOZ_ASSERT_IF(!isEmpty(), arena->bufferedCells == this);
#endif
}

inline void
StoreBuffer::putWholeCell(Cell* cell)
{
    MOZ_ASSERT(cell->isTenured());

    Arena* arena = cell->asTenured().arena();
    ArenaCellSet* cells = arena->bufferedCells;
    if (cells->isEmpty()) {
        cells = AllocateWholeCellSet(arena);
        if (!cells)
            return;
    }

    cells->putCell(&cell->asTenured());
    cells->check();
}

} // namespace gc
} // namespace js

#endif // gc_StoreBuffer_inl_h
