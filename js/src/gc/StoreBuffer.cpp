/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef JSGC_GENERATIONAL

#include "jsgc.h"

#include "gc/Barrier-inl.h"
#include "gc/StoreBuffer.h"
#include "vm/ForkJoin.h"
#include "vm/ObjectImpl-inl.h"

using namespace js;
using namespace js::gc;

/*** SlotEdge ***/

JS_ALWAYS_INLINE HeapSlot *
StoreBuffer::SlotEdge::slotLocation() const
{
    if (kind == HeapSlot::Element) {
        if (offset >= object->getDenseInitializedLength())
            return NULL;
        return (HeapSlot *)&object->getDenseElement(offset);
    }
    if (offset >= object->slotSpan())
        return NULL;
    return &object->getSlotRef(offset);
}

JS_ALWAYS_INLINE void *
StoreBuffer::SlotEdge::deref() const
{
    HeapSlot *loc = slotLocation();
    return (loc && loc->isGCThing()) ? loc->toGCThing() : NULL;
}

JS_ALWAYS_INLINE void *
StoreBuffer::SlotEdge::location() const
{
    return (void *)slotLocation();
}

template <typename NurseryType>
JS_ALWAYS_INLINE bool
StoreBuffer::SlotEdge::inRememberedSet(NurseryType *nursery) const
{
    return !nursery->isInside(object) && nursery->isInside(deref());
}

JS_ALWAYS_INLINE bool
StoreBuffer::SlotEdge::isNullEdge() const
{
    return !deref();
}

void
StoreBuffer::WholeCellEdges::mark(JSTracer *trc)
{
    JSGCTraceKind kind = GetGCThingTraceKind(tenured);
    if (kind <= JSTRACE_OBJECT) {
        MarkChildren(trc, static_cast<JSObject *>(tenured));
        return;
    }
    JS_ASSERT(kind == JSTRACE_IONCODE);
    static_cast<ion::IonCode *>(tenured)->trace(trc);
}

/*** MonoTypeBuffer ***/

/* How full we allow a store buffer to become before we request a MinorGC. */
const static double HighwaterRatio = 7.0 / 8.0;

template <typename T>
bool
StoreBuffer::MonoTypeBuffer<T>::enable(uint8_t *region, size_t len)
{
    JS_ASSERT(len % sizeof(T) == 0);
    base = pos = reinterpret_cast<T *>(region);
    top = reinterpret_cast<T *>(region + len);
    highwater = reinterpret_cast<T *>(region + size_t(double(len) * HighwaterRatio));
    JS_ASSERT(highwater > base);
    JS_ASSERT(highwater < top);
    return true;
}

template <typename T>
void
StoreBuffer::MonoTypeBuffer<T>::disable()
{
    base = pos = top = highwater = NULL;
}

template <typename T>
void
StoreBuffer::MonoTypeBuffer<T>::clear()
{
    pos = base;
}

template <typename T>
template <typename NurseryType>
void
StoreBuffer::MonoTypeBuffer<T>::compactNotInSet(NurseryType *nursery)
{
    T *insert = base;
    for (T *v = base; v != pos; ++v) {
        if (v->inRememberedSet(nursery))
            *insert++ = *v;
    }
    pos = insert;
}

template <typename T>
void
StoreBuffer::MonoTypeBuffer<T>::compactRemoveDuplicates()
{
    JS_ASSERT(duplicates.empty());

    T *insert = base;
    for (T *v = base; v != pos; ++v) {
        if (!duplicates.has(v->location())) {
            *insert++ = *v;
            /* Failure to insert will leave the set with duplicates. Oh well. */
            duplicates.put(v->location());
        }
    }
    pos = insert;
    duplicates.clear();
}

template <typename T>
void
StoreBuffer::MonoTypeBuffer<T>::compact()
{
#ifdef JS_GC_ZEAL
    if (owner->runtime->gcVerifyPostData)
        compactNotInSet(&owner->runtime->gcVerifierNursery);
    else
#endif
        compactNotInSet(&owner->runtime->gcNursery);
    compactRemoveDuplicates();
}

template <typename T>
void
StoreBuffer::MonoTypeBuffer<T>::mark(JSTracer *trc)
{
    compact();
    T *cursor = base;
    while (cursor != pos) {
        T edge = *cursor++;

        if (edge.isNullEdge())
            continue;

        edge.mark(trc);
    }
}

template <typename T>
bool
StoreBuffer::MonoTypeBuffer<T>::accumulateEdges(EdgeSet &edges)
{
    compact();
    T *cursor = base;
    while (cursor != pos) {
        T edge = *cursor++;

        /* Note: the relocatable buffer is allowed to store pointers to NULL. */
        if (edge.isNullEdge())
            continue;
        if (!edges.putNew(edge.location()))
            return false;
    }
    return true;
}

namespace js {
namespace gc {
class AccumulateEdgesTracer : public JSTracer
{
    EdgeSet *edges;

    static void tracer(JSTracer *jstrc, void **thingp, JSGCTraceKind kind) {
        AccumulateEdgesTracer *trc = static_cast<AccumulateEdgesTracer *>(jstrc);
        trc->edges->put(thingp);
    }

  public:
    AccumulateEdgesTracer(JSRuntime *rt, EdgeSet *edgesArg) : edges(edgesArg) {
        JS_TracerInit(this, rt, AccumulateEdgesTracer::tracer);
    }
};

template <>
bool
StoreBuffer::MonoTypeBuffer<StoreBuffer::WholeCellEdges>::accumulateEdges(EdgeSet &edges)
{
    compact();
    AccumulateEdgesTracer trc(owner->runtime, &edges);
    StoreBuffer::WholeCellEdges *cursor = base;
    while (cursor != pos) {
        cursor->mark(&trc);
        cursor++;
    }
    return true;
}
} /* namespace gc */
} /* namespace js */

/*** RelocatableMonoTypeBuffer ***/

template <typename T>
void
StoreBuffer::RelocatableMonoTypeBuffer<T>::compactMoved()
{
    for (T *v = this->base; v != this->pos; ++v) {
        if (v->isTagged()) {
            T match = v->untagged();
            for (T *r = this->base; r != v; ++r) {
                T check = r->untagged();
                if (check == match)
                    *r = NULL;
            }
            *v = NULL;
        }
    }
    T *insert = this->base;
    for (T *cursor = this->base; cursor != this->pos; ++cursor) {
        if (*cursor != NULL)
            *insert++ = *cursor;
    }
    this->pos = insert;
#ifdef DEBUG
    for (T *cursor = this->base; cursor != this->pos; ++cursor)
        JS_ASSERT(!cursor->isTagged());
#endif
}

template <typename T>
void
StoreBuffer::RelocatableMonoTypeBuffer<T>::compact()
{
    compactMoved();
    StoreBuffer::MonoTypeBuffer<T>::compact();
}

/*** GenericBuffer ***/

bool
StoreBuffer::GenericBuffer::enable(uint8_t *region, size_t len)
{
    base = pos = region;
    top = region + len;
    return true;
}

void
StoreBuffer::GenericBuffer::disable()
{
    base = pos = top = NULL;
}

void
StoreBuffer::GenericBuffer::clear()
{
    pos = base;
}

void
StoreBuffer::GenericBuffer::mark(JSTracer *trc)
{
    uint8_t *p = base;
    while (p < pos) {
        unsigned size = *((unsigned *)p);
        p += sizeof(unsigned);

        BufferableRef *edge = reinterpret_cast<BufferableRef *>(p);
        edge->mark(trc);

        p += size;
    }
}

bool
StoreBuffer::GenericBuffer::containsEdge(void *location) const
{
    uint8_t *p = base;
    while (p < pos) {
        unsigned size = *((unsigned *)p);
        p += sizeof(unsigned);

        if (((BufferableRef *)p)->match(location))
            return true;

        p += size;
    }
    return false;
}

/*** Edges ***/

void
StoreBuffer::CellPtrEdge::mark(JSTracer *trc)
{
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
    if (enabled)
        return true;

    buffer = js_malloc(TotalSize);
    if (!buffer)
        return false;

    /* Initialize the individual edge buffers in sub-regions. */
    uint8_t *asBytes = static_cast<uint8_t *>(buffer);
    size_t offset = 0;

    if (!bufferVal.enable(&asBytes[offset], ValueBufferSize))
        return false;
    offset += ValueBufferSize;

    if (!bufferCell.enable(&asBytes[offset], CellBufferSize))
        return false;
    offset += CellBufferSize;

    if (!bufferSlot.enable(&asBytes[offset], SlotBufferSize))
        return false;
    offset += SlotBufferSize;

    if (!bufferWholeCell.enable(&asBytes[offset], WholeCellBufferSize))
        return false;
    offset += WholeCellBufferSize;

    if (!bufferRelocVal.enable(&asBytes[offset], RelocValueBufferSize))
        return false;
    offset += RelocValueBufferSize;

    if (!bufferRelocCell.enable(&asBytes[offset], RelocCellBufferSize))
        return false;
    offset += RelocCellBufferSize;

    if (!bufferGeneric.enable(&asBytes[offset], GenericBufferSize))
        return false;
    offset += GenericBufferSize;

    JS_ASSERT(offset == TotalSize);

    enabled = true;
    return true;
}

void
StoreBuffer::disable()
{
    if (!enabled)
        return;

    aboutToOverflow = false;

    bufferVal.disable();
    bufferCell.disable();
    bufferSlot.disable();
    bufferWholeCell.disable();
    bufferRelocVal.disable();
    bufferRelocCell.disable();
    bufferGeneric.disable();

    js_free(buffer);
    enabled = false;
    overflowed = false;
}

bool
StoreBuffer::clear()
{
    if (!enabled)
        return true;

    aboutToOverflow = false;

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
StoreBuffer::mark(JSTracer *trc)
{
    JS_ASSERT(isEnabled());
    JS_ASSERT(!overflowed);

    bufferVal.mark(trc);
    bufferCell.mark(trc);
    bufferSlot.mark(trc);
    bufferWholeCell.mark(trc);
    bufferRelocVal.mark(trc);
    bufferRelocCell.mark(trc);
    bufferGeneric.mark(trc);
}

void
StoreBuffer::setAboutToOverflow()
{
    aboutToOverflow = true;
    runtime->triggerOperationCallback();
}

void
StoreBuffer::setOverflowed()
{
    JS_ASSERT(enabled);
    overflowed = true;
}

bool
StoreBuffer::coalesceForVerification()
{
    if (!edgeSet.initialized()) {
        if (!edgeSet.init())
            return false;
    }
    JS_ASSERT(edgeSet.empty());
    if (!bufferVal.accumulateEdges(edgeSet))
        return false;
    if (!bufferCell.accumulateEdges(edgeSet))
        return false;
    if (!bufferSlot.accumulateEdges(edgeSet))
        return false;
    if (!bufferWholeCell.accumulateEdges(edgeSet))
        return false;
    if (!bufferRelocVal.accumulateEdges(edgeSet))
        return false;
    if (!bufferRelocCell.accumulateEdges(edgeSet))
        return false;
    return true;
}

bool
StoreBuffer::containsEdgeAt(void *loc) const
{
    return edgeSet.has(loc) || bufferGeneric.containsEdge(loc);
}

void
StoreBuffer::releaseVerificationData()
{
    edgeSet.finish();
}

bool
StoreBuffer::inParallelSection() const
{
    return InParallelSection();
}

JS_PUBLIC_API(void)
JS::HeapCellPostBarrier(js::gc::Cell **cellp)
{
    JS_ASSERT(*cellp);
    JSRuntime *runtime = (*cellp)->runtime();
    runtime->gcStoreBuffer.putRelocatableCell(cellp);
}

JS_PUBLIC_API(void)
JS::HeapCellRelocate(js::gc::Cell **cellp)
{
    /* Called with old contents of *pp before overwriting. */
    JS_ASSERT(*cellp);
    JSRuntime *runtime = (*cellp)->runtime();
    runtime->gcStoreBuffer.removeRelocatableCell(cellp);
}

JS_PUBLIC_API(void)
JS::HeapValuePostBarrier(JS::Value *valuep)
{
    JS_ASSERT(JSVAL_IS_TRACEABLE(*valuep));
    JSRuntime *runtime = static_cast<js::gc::Cell *>(valuep->toGCThing())->runtime();
    runtime->gcStoreBuffer.putRelocatableValue(valuep);
}

JS_PUBLIC_API(void)
JS::HeapValueRelocate(JS::Value *valuep)
{
    /* Called with old contents of *valuep before overwriting. */
    JS_ASSERT(JSVAL_IS_TRACEABLE(*valuep));
    JSRuntime *runtime = static_cast<js::gc::Cell *>(valuep->toGCThing())->runtime();
    runtime->gcStoreBuffer.removeRelocatableValue(valuep);
}

template class StoreBuffer::MonoTypeBuffer<StoreBuffer::ValueEdge>;
template class StoreBuffer::MonoTypeBuffer<StoreBuffer::CellPtrEdge>;
template class StoreBuffer::MonoTypeBuffer<StoreBuffer::SlotEdge>;
template class StoreBuffer::MonoTypeBuffer<StoreBuffer::WholeCellEdges>;
template class StoreBuffer::RelocatableMonoTypeBuffer<StoreBuffer::ValueEdge>;
template class StoreBuffer::RelocatableMonoTypeBuffer<StoreBuffer::CellPtrEdge>;

#endif /* JSGC_GENERATIONAL */
