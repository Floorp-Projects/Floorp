/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ds/LifoAlloc.h"

#include "mozilla/Likely.h"
#include "mozilla/MathAlgorithms.h"

#include "ds/MemoryProtectionExceptionHandler.h"

#ifdef LIFO_CHUNK_PROTECT
# include "gc/Memory.h"
#endif

using namespace js;

using mozilla::RoundUpPow2;
using mozilla::tl::BitSize;

namespace js {
namespace detail {

/* static */
UniquePtr<BumpChunk>
BumpChunk::newWithCapacity(size_t size)
{
    MOZ_DIAGNOSTIC_ASSERT(RoundUpPow2(size) == size);
    MOZ_DIAGNOSTIC_ASSERT(size >= sizeof(BumpChunk));
    void* mem = js_malloc(size);
    if (!mem) {
        return nullptr;
    }

    UniquePtr<BumpChunk> result(new (mem) BumpChunk(size));

    // We assume that the alignment of LIFO_ALLOC_ALIGN is less than that of the
    // underlying memory allocator -- creating a new BumpChunk should always
    // satisfy the LIFO_ALLOC_ALIGN alignment constraint.
    MOZ_ASSERT(AlignPtr(result->begin()) == result->begin());
    return result;
}

#ifdef LIFO_CHUNK_PROTECT

static uint8_t*
AlignPtrUp(uint8_t* ptr, uintptr_t align) {
    MOZ_ASSERT(mozilla::IsPowerOfTwo(align));
    uintptr_t uptr = uintptr_t(ptr);
    uintptr_t diff = uptr & (align - 1);
    diff = (align - diff) & (align - 1);
    uptr = uptr + diff;
    return (uint8_t*) uptr;
}

static uint8_t*
AlignPtrDown(uint8_t* ptr, uintptr_t align) {
    MOZ_ASSERT(mozilla::IsPowerOfTwo(align));
    uintptr_t uptr = uintptr_t(ptr);
    uptr = uptr & ~(align - 1);
    return (uint8_t*) uptr;
}

void
BumpChunk::setReadOnly()
{
    uintptr_t pageSize = gc::SystemPageSize();
    // The allocated chunks might not be aligned on page boundaries. This code
    // is used to ensure that we are changing the memory protection of pointers
    // which are within the range of the BumpChunk, or that the range formed by
    // [b .. e] is empty.
    uint8_t* b = base();
    uint8_t* e = capacity_;
    b = AlignPtrUp(b, pageSize);
    e = AlignPtrDown(e, pageSize);
    if (e <= b) {
        return;
    }
    js::MemoryProtectionExceptionHandler::addRegion(base(), capacity_ - base());
    gc::MakePagesReadOnly(b, e - b);
}

void
BumpChunk::setReadWrite()
{
    uintptr_t pageSize = gc::SystemPageSize();
    // The allocated chunks might not be aligned on page boundaries. This code
    // is used to ensure that we are changing the memory protection of pointers
    // which are within the range of the BumpChunk, or that the range formed by
    // [b .. e] is empty.
    uint8_t* b = base();
    uint8_t* e = capacity_;
    b = AlignPtrUp(b, pageSize);
    e = AlignPtrDown(e, pageSize);
    if (e <= b) {
        return;
    }
    gc::UnprotectPages(b, e - b);
    js::MemoryProtectionExceptionHandler::removeRegion(base());
}

#endif

} // namespace detail
} // namespace js

void
LifoAlloc::reset(size_t defaultChunkSize)
{
    MOZ_ASSERT(mozilla::IsPowerOfTwo(defaultChunkSize));

    while (!chunks_.empty()) {
        chunks_.popFirst();
    }
    while (!unused_.empty()) {
        unused_.popFirst();
    }
    defaultChunkSize_ = defaultChunkSize;
    markCount = 0;
    curSize_ = 0;
}

void
LifoAlloc::freeAll()
{
    while (!chunks_.empty()) {
        UniqueBumpChunk bc = chunks_.popFirst();
        decrementCurSize(bc->computedSizeOfIncludingThis());
    }
    while (!unused_.empty()) {
        UniqueBumpChunk bc = unused_.popFirst();
        decrementCurSize(bc->computedSizeOfIncludingThis());
    }

    // Nb: maintaining curSize_ correctly isn't easy.  Fortunately, this is an
    // excellent sanity check.
    MOZ_ASSERT(curSize_ == 0);
}

LifoAlloc::UniqueBumpChunk
LifoAlloc::newChunkWithCapacity(size_t n)
{
    MOZ_ASSERT(fallibleScope_, "[OOM] Cannot allocate a new chunk in an infallible scope.");

    // Compute the size which should be requested in order to be able to fit |n|
    // bytes in a newly allocated chunk, or default to |defaultChunkSize_|.

    size_t minSize;
    if (MOZ_UNLIKELY(!detail::BumpChunk::allocSizeWithRedZone(n, &minSize) ||
                     (minSize & (size_t(1) << (BitSize<size_t>::value - 1)))))
    {
        return nullptr;
    }

    const size_t chunkSize = minSize > defaultChunkSize_
                             ?  RoundUpPow2(minSize)
                             : defaultChunkSize_;

    // Create a new BumpChunk, and allocate space for it.
    UniqueBumpChunk result = detail::BumpChunk::newWithCapacity(chunkSize);
    if (!result) {
        return nullptr;
    }
    MOZ_ASSERT(result->computedSizeOfIncludingThis() == chunkSize);
    return result;
}

bool
LifoAlloc::getOrCreateChunk(size_t n)
{
    // Look for existing unused BumpChunks to satisfy the request, and pick the
    // first one which is large enough, and move it into the list of used
    // chunks.
    if (!unused_.empty()) {
        if (unused_.begin()->canAlloc(n)) {
            chunks_.append(unused_.popFirst());
            return true;
        }

        BumpChunkList::Iterator e(unused_.end());
        for (BumpChunkList::Iterator i(unused_.begin()); i->next() != e.get(); ++i) {
            detail::BumpChunk* elem = i->next();
            MOZ_ASSERT(elem->empty());
            if (elem->canAlloc(n)) {
                BumpChunkList temp = unused_.splitAfter(i.get());
                chunks_.append(temp.popFirst());
                unused_.appendAll(std::move(temp));
                return true;
            }
        }
    }

    // Allocate a new BumpChunk with enough space for the next allocation.
    UniqueBumpChunk newChunk = newChunkWithCapacity(n);
    if (!newChunk) {
        return false;
    }
    size_t size = newChunk->computedSizeOfIncludingThis();
    chunks_.append(std::move(newChunk));
    incrementCurSize(size);
    return true;
}

void
LifoAlloc::transferFrom(LifoAlloc* other)
{
    MOZ_ASSERT(!markCount);
    MOZ_ASSERT(!other->markCount);

    incrementCurSize(other->curSize_);
    appendUnused(std::move(other->unused_));
    appendUsed(std::move(other->chunks_));
    other->curSize_ = 0;
}

void
LifoAlloc::transferUnusedFrom(LifoAlloc* other)
{
    MOZ_ASSERT(!markCount);

    size_t size = 0;
    for (detail::BumpChunk& bc : other->unused_) {
        size += bc.computedSizeOfIncludingThis();
    }

    appendUnused(std::move(other->unused_));
    incrementCurSize(size);
    other->decrementCurSize(size);
}
