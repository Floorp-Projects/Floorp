/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef JSGC_GENERATIONAL

#include "gc/StoreBuffer.h"

#include "mozilla/Assertions.h"

#include "vm/ArgumentsObject.h"
#include "vm/ForkJoin.h"

#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;
using mozilla::ReentrancyGuard;

/*** SlotEdge ***/

MOZ_ALWAYS_INLINE HeapSlot *
StoreBuffer::SlotEdge::slotLocation() const
{
    if (kind == HeapSlot::Element) {
        if (offset >= object->getDenseInitializedLength())
            return nullptr;
        return (HeapSlot *)&object->getDenseElement(offset);
    }
    if (offset >= object->slotSpan())
        return nullptr;
    return &object->getSlotRef(offset);
}

MOZ_ALWAYS_INLINE void *
StoreBuffer::SlotEdge::deref() const
{
    HeapSlot *loc = slotLocation();
    return (loc && loc->isGCThing()) ? loc->toGCThing() : nullptr;
}

MOZ_ALWAYS_INLINE void *
StoreBuffer::SlotEdge::location() const
{
    return (void *)slotLocation();
}

bool
StoreBuffer::SlotEdge::inRememberedSet(const Nursery &nursery) const
{
    return !nursery.isInside(object) && nursery.isInside(deref());
}

MOZ_ALWAYS_INLINE bool
StoreBuffer::SlotEdge::isNullEdge() const
{
    return !deref();
}

void
StoreBuffer::WholeCellEdges::mark(JSTracer *trc)
{
    JS_ASSERT(tenured->isTenured());
    JSGCTraceKind kind = GetGCThingTraceKind(tenured);
    if (kind <= JSTRACE_OBJECT) {
        JSObject *object = static_cast<JSObject *>(tenured);
        if (object->is<ArgumentsObject>())
            ArgumentsObject::trace(trc, object);
        else
            MarkChildren(trc, object);
        return;
    }
#ifdef JS_ION
    JS_ASSERT(kind == JSTRACE_JITCODE);
    static_cast<jit::JitCode *>(tenured)->trace(trc);
#else
    MOZ_ASSUME_UNREACHABLE("Only objects can be in the wholeCellBuffer if IonMonkey is disabled.");
#endif
}

/*** MonoTypeBuffer ***/

template <typename T>
void
StoreBuffer::MonoTypeBuffer<T>::handleOverflow(StoreBuffer *owner)
{
    if (!owner->isAboutToOverflow()) {
        /*
         * Compact the buffer now, and if that fails to free enough space then
         * trigger a minor collection.
         */
        compact(owner);
        if (isAboutToOverflow())
            owner->setAboutToOverflow();
    } else {
         /*
          * A minor GC has already been triggered, so there's no point
          * compacting unless the buffer is totally full.
          */
        if (storage_->availableInCurrentChunk() < sizeof(T))
            compact(owner);
    }
}

template <typename T>
void
StoreBuffer::MonoTypeBuffer<T>::compactRemoveDuplicates(StoreBuffer *owner)
{
    EdgeSet duplicates;
    if (!duplicates.init())
        return; /* Failure to de-dup is acceptable. */

    LifoAlloc::Enum insert(*storage_);
    for (LifoAlloc::Enum e(*storage_); !e.empty(); e.popFront<T>()) {
        T *edge = e.get<T>();
        if (!duplicates.has(edge->location())) {
            insert.updateFront<T>(*edge);
            insert.popFront<T>();

            /* Failure to insert will leave the set with duplicates. Oh well. */
            duplicates.put(edge->location());
        }
    }
    storage_->release(insert.mark());

    duplicates.clear();
}

template <typename T>
void
StoreBuffer::MonoTypeBuffer<T>::compact(StoreBuffer *owner)
{
    JS_ASSERT(storage_);
    compactRemoveDuplicates(owner);
    usedAtLastCompact_ = storage_->used();
}

template <typename T>
void
StoreBuffer::MonoTypeBuffer<T>::maybeCompact(StoreBuffer *owner)
{
    JS_ASSERT(storage_);
    if (storage_->used() != usedAtLastCompact_)
        compact(owner);
}

template <typename T>
void
StoreBuffer::MonoTypeBuffer<T>::mark(StoreBuffer *owner, JSTracer *trc)
{
    JS_ASSERT(owner->isEnabled());
    ReentrancyGuard g(*owner);
    if (!storage_)
        return;

    maybeCompact(owner);
    for (LifoAlloc::Enum e(*storage_); !e.empty(); e.popFront<T>()) {
        T *edge = e.get<T>();
        if (edge->isNullEdge())
            continue;
        edge->mark(trc);

    }
}

/*** RelocatableMonoTypeBuffer ***/

template <typename T>
void
StoreBuffer::RelocatableMonoTypeBuffer<T>::compactMoved(StoreBuffer *owner)
{
    LifoAlloc &storage = *this->storage_;
    EdgeSet invalidated;
    if (!invalidated.init())
        CrashAtUnhandlableOOM("RelocatableMonoTypeBuffer::compactMoved: Failed to init table.");

    /* Collect the set of entries which are currently invalid. */
    for (LifoAlloc::Enum e(storage); !e.empty(); e.popFront<T>()) {
        T *edge = e.get<T>();
        if (edge->isTagged()) {
            if (!invalidated.put(edge->location()))
                CrashAtUnhandlableOOM("RelocatableMonoTypeBuffer::compactMoved: Failed to put removal.");
        } else {
            invalidated.remove(edge->location());
        }
    }

    /* Remove all entries which are in the invalidated set. */
    LifoAlloc::Enum insert(storage);
    for (LifoAlloc::Enum e(storage); !e.empty(); e.popFront<T>()) {
        T *edge = e.get<T>();
        if (!edge->isTagged() && !invalidated.has(edge->location())) {
            insert.updateFront<T>(*edge);
            insert.popFront<T>();
        }
    }
    storage.release(insert.mark());

    invalidated.clear();

#ifdef DEBUG
    for (LifoAlloc::Enum e(storage); !e.empty(); e.popFront<T>())
        JS_ASSERT(!e.get<T>()->isTagged());
#endif
}

template <typename T>
void
StoreBuffer::RelocatableMonoTypeBuffer<T>::compact(StoreBuffer *owner)
{
    compactMoved(owner);
    StoreBuffer::MonoTypeBuffer<T>::compact(owner);
}

/*** GenericBuffer ***/

void
StoreBuffer::GenericBuffer::mark(StoreBuffer *owner, JSTracer *trc)
{
    JS_ASSERT(owner->isEnabled());
    ReentrancyGuard g(*owner);
    if (!storage_)
        return;

    for (LifoAlloc::Enum e(*storage_); !e.empty();) {
        unsigned size = *e.get<unsigned>();
        e.popFront<unsigned>();
        BufferableRef *edge = e.get<BufferableRef>(size);
        edge->mark(trc);
        e.popFront(size);
    }
}

/*** Edges ***/

void
StoreBuffer::CellPtrEdge::mark(JSTracer *trc)
{
    JS_ASSERT(GetGCThingTraceKind(*edge) == JSTRACE_OBJECT);
    MarkObjectRoot(trc, reinterpret_cast<JSObject**>(edge), "store buffer edge");
}

void
StoreBuffer::ValueEdge::mark(JSTracer *trc)
{
    MarkValueRoot(trc, edge, "store buffer edge");
}

void
StoreBuffer::SlotEdge::mark(JSTracer *trc)
{
    if (kind == HeapSlot::Element)
        MarkSlot(trc, (HeapSlot*)&object->getDenseElement(offset), "store buffer edge");
    else
        MarkSlot(trc, &object->getSlotRef(offset), "store buffer edge");
}

/*** StoreBuffer ***/

bool
StoreBuffer::enable()
{
    if (enabled_)
        return true;

    if (!bufferVal.init() ||
        !bufferCell.init() ||
        !bufferSlot.init() ||
        !bufferWholeCell.init() ||
        !bufferRelocVal.init() ||
        !bufferRelocCell.init() ||
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
    if (!enabled_)
        return;

    aboutToOverflow_ = false;

    enabled_ = false;
}

bool
StoreBuffer::clear()
{
    if (!enabled_)
        return true;

    aboutToOverflow_ = false;

    bufferVal.clear();
    bufferCell.clear();
    bufferSlot.clear();
    bufferWholeCell.clear();
    bufferRelocVal.clear();
    bufferRelocCell.clear();
    bufferGeneric.clear();

    return true;
}

void
StoreBuffer::markAll(JSTracer *trc)
{
    bufferVal.mark(this, trc);
    bufferCell.mark(this, trc);
    bufferSlot.mark(this, trc);
    bufferWholeCell.mark(this, trc);
    bufferRelocVal.mark(this, trc);
    bufferRelocCell.mark(this, trc);
    bufferGeneric.mark(this, trc);
}

void
StoreBuffer::setAboutToOverflow()
{
    aboutToOverflow_ = true;
    runtime_->triggerOperationCallback(JSRuntime::TriggerCallbackMainThread);
}

bool
StoreBuffer::inParallelSection() const
{
    return InParallelSection();
}

void
StoreBuffer::addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf, JS::GCSizes
*sizes)
{
    sizes->storeBufferVals       += bufferVal.sizeOfExcludingThis(mallocSizeOf);
    sizes->storeBufferCells      += bufferCell.sizeOfExcludingThis(mallocSizeOf);
    sizes->storeBufferSlots      += bufferSlot.sizeOfExcludingThis(mallocSizeOf);
    sizes->storeBufferWholeCells += bufferWholeCell.sizeOfExcludingThis(mallocSizeOf);
    sizes->storeBufferRelocVals  += bufferRelocVal.sizeOfExcludingThis(mallocSizeOf);
    sizes->storeBufferRelocCells += bufferRelocCell.sizeOfExcludingThis(mallocSizeOf);
    sizes->storeBufferGenerics   += bufferGeneric.sizeOfExcludingThis(mallocSizeOf);
}

JS_PUBLIC_API(void)
JS::HeapCellPostBarrier(js::gc::Cell **cellp)
{
    JS_ASSERT(*cellp);
    JSRuntime *runtime = (*cellp)->runtimeFromMainThread();
    runtime->gcStoreBuffer.putRelocatableCell(cellp);
}

JS_PUBLIC_API(void)
JS::HeapCellRelocate(js::gc::Cell **cellp)
{
    /* Called with old contents of *pp before overwriting. */
    JS_ASSERT(*cellp);
    JSRuntime *runtime = (*cellp)->runtimeFromMainThread();
    runtime->gcStoreBuffer.removeRelocatableCell(cellp);
}

JS_PUBLIC_API(void)
JS::HeapValuePostBarrier(JS::Value *valuep)
{
    JS_ASSERT(JSVAL_IS_TRACEABLE(*valuep));
    JSRuntime *runtime = static_cast<js::gc::Cell *>(valuep->toGCThing())->runtimeFromMainThread();
    runtime->gcStoreBuffer.putRelocatableValue(valuep);
}

JS_PUBLIC_API(void)
JS::HeapValueRelocate(JS::Value *valuep)
{
    /* Called with old contents of *valuep before overwriting. */
    JS_ASSERT(JSVAL_IS_TRACEABLE(*valuep));
    JSRuntime *runtime = static_cast<js::gc::Cell *>(valuep->toGCThing())->runtimeFromMainThread();
    runtime->gcStoreBuffer.removeRelocatableValue(valuep);
}

template class StoreBuffer::MonoTypeBuffer<StoreBuffer::ValueEdge>;
template class StoreBuffer::MonoTypeBuffer<StoreBuffer::CellPtrEdge>;
template class StoreBuffer::MonoTypeBuffer<StoreBuffer::SlotEdge>;
template class StoreBuffer::MonoTypeBuffer<StoreBuffer::WholeCellEdges>;
template class StoreBuffer::RelocatableMonoTypeBuffer<StoreBuffer::ValueEdge>;
template class StoreBuffer::RelocatableMonoTypeBuffer<StoreBuffer::CellPtrEdge>;

#endif /* JSGC_GENERATIONAL */
