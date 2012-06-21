/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef JSGC_GENERATIONAL

#include "jsgc.h"

#include "gc/Barrier-inl.h"
#include "gc/StoreBuffer.h"
#include "vm/ObjectImpl-inl.h"

namespace js {
namespace gc {

/*** MonoTypeBuffer ***/

template <typename T>
bool
StoreBuffer::MonoTypeBuffer<T>::enable(uint8_t *region, size_t len)
{
    JS_ASSERT(len % sizeof(T) == 0);
    base = pos = reinterpret_cast<T *>(region);
    top = reinterpret_cast<T *>(region + len);
    return true;
}

template <typename T>
void
StoreBuffer::MonoTypeBuffer<T>::disable()
{
    base = pos = top = NULL;
}

template <typename T>
void
StoreBuffer::MonoTypeBuffer<T>::compactNotInSet()
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
StoreBuffer::MonoTypeBuffer<T>::compact()
{
    compactNotInSet();
}

template <typename T>
void
StoreBuffer::MonoTypeBuffer<T>::put(const T &v)
{
    /* Check if we have been enabled. */
    if (!pos)
        return;

    /*
     * Note: it is sometimes valid for a put to happen in the middle of a GC,
     * e.g. a rekey of a Relocatable may end up here. In general, we do not
     * care about these new entries or any overflows they cause.
     */
    *pos++ = v;
    if (isFull()) {
        owner->setOverflowed();
        pos = base;
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

template <typename T>
void
StoreBuffer::RelocatableMonoTypeBuffer<T>::unput(const T &v)
{
    put(v.tagged());
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

/*** StoreBuffer ***/

bool
StoreBuffer::enable()
{
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
    bufferVal.disable();
    bufferCell.disable();
    bufferSlot.disable();
    bufferRelocVal.disable();
    bufferRelocCell.disable();
    bufferGeneric.disable();

    js_free(buffer);
    enabled = false;
    overflowed = false;
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

template class StoreBuffer::MonoTypeBuffer<StoreBuffer::ValueEdge>;
template class StoreBuffer::MonoTypeBuffer<StoreBuffer::CellPtrEdge>;
template class StoreBuffer::MonoTypeBuffer<StoreBuffer::SlotEdge>;
template class StoreBuffer::RelocatableMonoTypeBuffer<StoreBuffer::ValueEdge>;
template class StoreBuffer::RelocatableMonoTypeBuffer<StoreBuffer::CellPtrEdge>;

} /* namespace gc */
} /* namespace js */

#endif /* JSGC_GENERATIONAL */
