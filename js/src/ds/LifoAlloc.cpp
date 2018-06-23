/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ds/LifoAlloc.h"

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
BumpChunk::newWithCapacity(size_t size, bool protect)
{
    MOZ_DIAGNOSTIC_ASSERT(RoundUpPow2(size) == size);
    MOZ_DIAGNOSTIC_ASSERT(size >= sizeof(BumpChunk));
    void* mem = js_malloc(size);
    if (!mem)
        return nullptr;

    UniquePtr<BumpChunk> result(new (mem) BumpChunk(size, protect));

    // We assume that the alignment of LIFO_ALLOC_ALIGN is less than that of the
    // underlying memory allocator -- creating a new BumpChunk should always
    // satisfy the LIFO_ALLOC_ALIGN alignment constraint.
    MOZ_ASSERT(AlignPtr(result->begin()) == result->begin());
    return result;
}

bool
BumpChunk::canAlloc(size_t n)
{
    uint8_t* aligned = AlignPtr(bump_);
    uint8_t* newBump = aligned + n;
    // bump_ <= newBump, is necessary to catch overflow.
    return bump_ <= newBump && newBump <= capacity_;
}

#ifdef LIFO_CHUNK_PROTECT

static const uint8_t*
AlignPtrUp(const uint8_t* ptr, uintptr_t align) {
    MOZ_ASSERT(mozilla::IsPowerOfTwo(align));
    uintptr_t uptr = uintptr_t(ptr);
    uintptr_t diff = uptr & (align - 1);
    diff = (align - diff) & (align - 1);
    uptr = uptr + diff;
    return (uint8_t*) uptr;
}

static const uint8_t*
AlignPtrDown(const uint8_t* ptr, uintptr_t align) {
    MOZ_ASSERT(mozilla::IsPowerOfTwo(align));
    uintptr_t uptr = uintptr_t(ptr);
    uptr = uptr & ~(align - 1);
    return (uint8_t*) uptr;
}

void
BumpChunk::setRWUntil(Loc loc) const
{
    if (!protect_)
        return;

    uintptr_t pageSize = gc::SystemPageSize();
    // The allocated chunks might not be aligned on page boundaries. This code
    // is used to ensure that we are changing the memory protection of pointers
    // which are within the range of the BumpChunk, or that the range formed by
    // [b .. e] is empty.
    const uint8_t* b = base();
    const uint8_t* e = capacity_;
    b = AlignPtrUp(b, pageSize);
    e = AlignPtrDown(e, pageSize);
    if (e < b)
        e = b;
    // The mid-point is aligned to the next page, and clamp to the end-point to
    // ensure that it remains in the [b .. e] range.
    const uint8_t* m = nullptr;
    switch (loc) {
      case Loc::Header:
        m = b;
        break;
      case Loc::Allocated:
        m = begin();
        break;
      case Loc::Reserved:
        m = end();
        break;
      case Loc::End:
        m = e;
        break;
    }
    m = AlignPtrUp(m, pageSize);
    if (e < m)
        m = e;

    if (b < m)
        gc::UnprotectPages(const_cast<uint8_t*>(b), m - b);
    // Note: We could use no-access protection for everything after begin(), but
    // we need to read capabilities for reading the bump_ / capacity_ fields
    // from this function to unprotect the memory later.
    if (m < e)
        gc::MakePagesReadOnly(const_cast<uint8_t*>(m), e - m);
}

// The memory protection handler is catching memory accesses error on the
// regions registered into it. These method, instead of registering sub-ranges
// of the BumpChunk within setRWUntil, we just register the full BumpChunk
// ranges, and let the MemoryProtectionExceptionHandler catch bad memory
// accesses when it is being protected by setRWUntil.
void
BumpChunk::addMProtectHandler() const
{
    if (!protect_)
        return;
    js::MemoryProtectionExceptionHandler::addRegion(const_cast<uint8_t*>(base()), capacity_ - base());
}

void
BumpChunk::removeMProtectHandler() const
{
    if (!protect_)
        return;
    js::MemoryProtectionExceptionHandler::removeRegion(const_cast<uint8_t*>(base()));
}

#endif

} // namespace detail
} // namespace js

void
LifoAlloc::freeAll()
{
    while (!chunks_.empty()) {
        chunks_.begin()->setRWUntil(Loc::End);
        BumpChunk bc = chunks_.popFirst();
        decrementCurSize(bc->computedSizeOfIncludingThis());
    }
    while (!unused_.empty()) {
        unused_.begin()->setRWUntil(Loc::End);
        BumpChunk bc = unused_.popFirst();
        decrementCurSize(bc->computedSizeOfIncludingThis());
    }

    // Nb: maintaining curSize_ correctly isn't easy.  Fortunately, this is an
    // excellent sanity check.
    MOZ_ASSERT(curSize_ == 0);
}

LifoAlloc::BumpChunk
LifoAlloc::newChunkWithCapacity(size_t n)
{
    MOZ_ASSERT(fallibleScope_, "[OOM] Cannot allocate a new chunk in an infallible scope.");

    // Compute the size which should be requested in order to be able to fit |n|
    // bytes in the newly allocated chunk, or default the |defaultChunkSize_|.
    size_t defaultChunkFreeSpace = defaultChunkSize_ - detail::BumpChunk::reservedSpace;
    size_t chunkSize;
    if (n > defaultChunkFreeSpace) {
        MOZ_ASSERT(defaultChunkFreeSpace < defaultChunkSize_);
        size_t allocSizeWithCanaries = n + (defaultChunkSize_ - defaultChunkFreeSpace);

        // Guard for overflow.
        if (allocSizeWithCanaries < n ||
            (allocSizeWithCanaries & (size_t(1) << (BitSize<size_t>::value - 1))))
        {
            return nullptr;
        }

        chunkSize = RoundUpPow2(allocSizeWithCanaries);
    } else {
        chunkSize = defaultChunkSize_;
    }

    bool protect = false;
#ifdef LIFO_CHUNK_PROTECT
    protect = protect_;
#endif

    // Create a new BumpChunk, and allocate space for it.
    BumpChunk result = detail::BumpChunk::newWithCapacity(chunkSize, protect);
    if (!result)
        return nullptr;
    MOZ_ASSERT(result->computedSizeOfIncludingThis() == chunkSize);
    return result;
}

bool
LifoAlloc::getOrCreateChunk(size_t n)
{
    // This function is adding a new BumpChunk in which all upcoming allocation
    // would be made. Thus, we protect against out-of-bounds the last chunk in
    // which we did our previous allocations.
    if (!chunks_.empty())
        chunks_.last()->setRWUntil(Loc::Reserved);

    // Look for existing unused BumpChunks to satisfy the request, and pick the
    // first one which is large enough, and move it into the list of used
    // chunks.
    if (!unused_.empty()) {
        if (unused_.begin()->canAlloc(n)) {
            chunks_.append(unused_.popFirst());
            chunks_.last()->setRWUntil(Loc::End);
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
                chunks_.last()->setRWUntil(Loc::End);
                return true;
            }
        }
    }

    // Allocate a new BumpChunk with enough space for the next allocation.
    BumpChunk newChunk = newChunkWithCapacity(n);
    if (!newChunk)
        return false;
    size_t size = newChunk->computedSizeOfIncludingThis();
    // The last chunk in which allocations are performed should be protected
    // with setRWUntil(Loc::End), but this is not necessary here because any new
    // allocation should be protected as RW already.
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
    for (detail::BumpChunk& bc : other->unused_)
        size += bc.computedSizeOfIncludingThis();

    appendUnused(std::move(other->unused_));
    incrementCurSize(size);
    other->decrementCurSize(size);
}
