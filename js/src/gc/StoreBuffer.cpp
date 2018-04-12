/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gc/StoreBuffer-inl.h"

#include "mozilla/Assertions.h"

#include "gc/Statistics.h"
#include "vm/ArgumentsObject.h"
#include "vm/Runtime.h"

#include "gc/GC-inl.h"

using namespace js;
using namespace js::gc;

void
StoreBuffer::GenericBuffer::trace(StoreBuffer* owner, JSTracer* trc)
{
    mozilla::ReentrancyGuard g(*owner);
    MOZ_ASSERT(owner->isEnabled());
    if (!storage_)
        return;

    for (LifoAlloc::Enum e(*storage_); !e.empty();) {
        unsigned size = *e.read<unsigned>();
        BufferableRef* edge = e.read<BufferableRef>(size);
        edge->trace(trc);
    }
}

inline void
StoreBuffer::checkEmpty() const
{
    MOZ_ASSERT(bufferVal.isEmpty());
    MOZ_ASSERT(bufferCell.isEmpty());
    MOZ_ASSERT(bufferSlot.isEmpty());
    MOZ_ASSERT(bufferWholeCell.isEmpty());
    MOZ_ASSERT(bufferGeneric.isEmpty());
}

bool
StoreBuffer::enable()
{
    if (enabled_)
        return true;

    checkEmpty();

    if (!bufferVal.init() ||
        !bufferCell.init() ||
        !bufferSlot.init() ||
        !bufferWholeCell.init() ||
        !bufferGeneric.init())
    {
        return false;
    }

    enabled_ = true;
    return true;
}

void
StoreBuffer::disable()
{
    checkEmpty();

    if (!enabled_)
        return;

    aboutToOverflow_ = false;

    enabled_ = false;
}

void
StoreBuffer::clear()
{
    if (!enabled_)
        return;

    aboutToOverflow_ = false;
    cancelIonCompilations_ = false;

    bufferVal.clear();
    bufferCell.clear();
    bufferSlot.clear();
    bufferWholeCell.clear();
    bufferGeneric.clear();
}

void
StoreBuffer::setAboutToOverflow(JS::gcreason::Reason reason)
{
    if (!aboutToOverflow_) {
        aboutToOverflow_ = true;
        runtime_->gc.stats().count(gcstats::STAT_STOREBUFFER_OVERFLOW);
    }
    nursery_.requestMinorGC(reason);
}

void
StoreBuffer::addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf, JS::GCSizes
*sizes)
{
    sizes->storeBufferVals       += bufferVal.sizeOfExcludingThis(mallocSizeOf);
    sizes->storeBufferCells      += bufferCell.sizeOfExcludingThis(mallocSizeOf);
    sizes->storeBufferSlots      += bufferSlot.sizeOfExcludingThis(mallocSizeOf);
    sizes->storeBufferWholeCells += bufferWholeCell.sizeOfExcludingThis(mallocSizeOf);
    sizes->storeBufferGenerics   += bufferGeneric.sizeOfExcludingThis(mallocSizeOf);
}

ArenaCellSet ArenaCellSet::Empty;

ArenaCellSet::ArenaCellSet()
  : arena(nullptr)
  , next(nullptr)
#ifdef DEBUG
  , minorGCNumberAtCreation(0)
#endif
{}

ArenaCellSet::ArenaCellSet(Arena* arena, ArenaCellSet* next)
  : arena(arena)
  , next(next)
#ifdef DEBUG
  , minorGCNumberAtCreation(arena->zone->runtimeFromActiveCooperatingThread()->gc.minorGCCount())
#endif
{
    MOZ_ASSERT(arena);
    bits.clear(false);
}

ArenaCellSet*
StoreBuffer::WholeCellBuffer::allocateCellSet(Arena* arena)
{
    Zone* zone = arena->zone;
    JSRuntime* rt = zone->runtimeFromActiveCooperatingThread();
    if (!rt->gc.nursery().isEnabled())
        return nullptr;

    AutoEnterOOMUnsafeRegion oomUnsafe;
    auto cells = storage_->new_<ArenaCellSet>(arena, head_);
    if (!cells)
        oomUnsafe.crash("Failed to allocate ArenaCellSet");

    arena->bufferedCells() = cells;
    head_ = cells;

    if (isAboutToOverflow())
        rt->gc.storeBuffer().setAboutToOverflow(JS::gcreason::FULL_WHOLE_CELL_BUFFER);

    return cells;
}

void
StoreBuffer::WholeCellBuffer::clear()
{
    for (ArenaCellSet* set = head_; set; set = set->next)
        set->arena->bufferedCells() = &ArenaCellSet::Empty;
    head_ = nullptr;

    if (storage_)
        storage_->used() ? storage_->releaseAll() : storage_->freeAll();
}

template struct StoreBuffer::MonoTypeBuffer<StoreBuffer::ValueEdge>;
template struct StoreBuffer::MonoTypeBuffer<StoreBuffer::CellPtrEdge>;
template struct StoreBuffer::MonoTypeBuffer<StoreBuffer::SlotsEdge>;
