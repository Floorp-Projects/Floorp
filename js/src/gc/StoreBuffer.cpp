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

/*** Edges ***/

void
StoreBuffer::SlotsEdge::mark(JSTracer *trc)
{
    JSObject *obj = object();

    if (IsInsideNursery(obj))
        return;

    if (!obj->isNative()) {
        const Class *clasp = obj->getClass();
        if (clasp)
            clasp->trace(trc, obj);
        return;
    }

    if (kind() == ElementKind) {
        int32_t initLen = obj->getDenseInitializedLength();
        int32_t clampedStart = Min(start_, initLen);
        int32_t clampedEnd = Min(start_ + count_, initLen);
        gc::MarkArraySlots(trc, clampedEnd - clampedStart,
                           obj->getDenseElements() + clampedStart, "element");
    } else {
        int32_t start = Min(uint32_t(start_), obj->slotSpan());
        int32_t end = Min(uint32_t(start_) + count_, obj->slotSpan());
        MOZ_ASSERT(end >= start);
        MarkObjectSlots(trc, obj, start, end - start);
    }
}

void
StoreBuffer::WholeCellEdges::mark(JSTracer *trc)
{
    JS_ASSERT(edge->isTenured());
    JSGCTraceKind kind = GetGCThingTraceKind(edge);
    if (kind <= JSTRACE_OBJECT) {
        JSObject *object = static_cast<JSObject *>(edge);
        if (object->is<ArgumentsObject>())
            ArgumentsObject::trace(trc, object);
        MarkChildren(trc, object);
        return;
    }
#ifdef JS_ION
    JS_ASSERT(kind == JSTRACE_JITCODE);
    static_cast<jit::JitCode *>(edge)->trace(trc);
#else
    MOZ_CRASH("Only objects can be in the wholeCellBuffer if IonMonkey is disabled.");
#endif
}

void
StoreBuffer::CellPtrEdge::mark(JSTracer *trc)
{
    if (!*edge)
        return;

    JS_ASSERT(GetGCThingTraceKind(*edge) == JSTRACE_OBJECT);
    MarkObjectRoot(trc, reinterpret_cast<JSObject**>(edge), "store buffer edge");
}

void
StoreBuffer::ValueEdge::mark(JSTracer *trc)
{
    if (!deref())
        return;

    MarkValueRoot(trc, edge, "store buffer edge");
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
            maybeCompact(owner);
    }
}

template <typename T>
void
StoreBuffer::MonoTypeBuffer<T>::compactRemoveDuplicates(StoreBuffer *owner)
{
    typedef HashSet<T, typename T::Hasher, SystemAllocPolicy> DedupSet;

    DedupSet duplicates;
    if (!duplicates.init())
        return; /* Failure to de-dup is acceptable. */

    LifoAlloc::Enum insert(*storage_);
    for (LifoAlloc::Enum e(*storage_); !e.empty(); e.popFront<T>()) {
        T *edge = e.get<T>();
        if (!duplicates.has(*edge)) {
            insert.updateFront<T>(*edge);
            insert.popFront<T>();

            /* Failure to insert will leave the set with duplicates. Oh well. */
            duplicates.put(*edge);
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
            if (!invalidated.put(edge->untagged().edge))
                CrashAtUnhandlableOOM("RelocatableMonoTypeBuffer::compactMoved: Failed to put removal.");
        } else {
            invalidated.remove(edge->untagged().edge);
        }
    }

    /* Remove all entries which are in the invalidated set. */
    LifoAlloc::Enum insert(storage);
    for (LifoAlloc::Enum e(storage); !e.empty(); e.popFront<T>()) {
        T *edge = e.get<T>();
        if (!edge->isTagged() && !invalidated.has(edge->untagged().edge)) {
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
    runtime_->requestInterrupt(JSRuntime::RequestInterruptMainThread);
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
    JS_ASSERT(cellp);
    JS_ASSERT(*cellp);
    StoreBuffer *storeBuffer = (*cellp)->storeBuffer();
    if (storeBuffer)
        storeBuffer->putRelocatableCellFromAnyThread(cellp);
}

JS_PUBLIC_API(void)
JS::HeapCellRelocate(js::gc::Cell **cellp)
{
    /* Called with old contents of *cellp before overwriting. */
    JS_ASSERT(cellp);
    JS_ASSERT(*cellp);
    JSRuntime *runtime = (*cellp)->runtimeFromMainThread();
    runtime->gc.storeBuffer.removeRelocatableCellFromAnyThread(cellp);
}

JS_PUBLIC_API(void)
JS::HeapValuePostBarrier(JS::Value *valuep)
{
    JS_ASSERT(valuep);
    JS_ASSERT(valuep->isMarkable());
    if (valuep->isObject()) {
        StoreBuffer *storeBuffer = valuep->toObject().storeBuffer();
        if (storeBuffer)
            storeBuffer->putRelocatableValueFromAnyThread(valuep);
    }
}

JS_PUBLIC_API(void)
JS::HeapValueRelocate(JS::Value *valuep)
{
    /* Called with old contents of *valuep before overwriting. */
    JS_ASSERT(valuep);
    JS_ASSERT(valuep->isMarkable());
    if (valuep->isString() && valuep->toString()->isPermanentAtom())
        return;
    JSRuntime *runtime = static_cast<js::gc::Cell *>(valuep->toGCThing())->runtimeFromMainThread();
    runtime->gc.storeBuffer.removeRelocatableValueFromAnyThread(valuep);
}

template struct StoreBuffer::MonoTypeBuffer<StoreBuffer::ValueEdge>;
template struct StoreBuffer::MonoTypeBuffer<StoreBuffer::CellPtrEdge>;
template struct StoreBuffer::MonoTypeBuffer<StoreBuffer::SlotsEdge>;
template struct StoreBuffer::MonoTypeBuffer<StoreBuffer::WholeCellEdges>;
template struct StoreBuffer::RelocatableMonoTypeBuffer<StoreBuffer::ValueEdge>;
template struct StoreBuffer::RelocatableMonoTypeBuffer<StoreBuffer::CellPtrEdge>;

#endif /* JSGC_GENERATIONAL */
