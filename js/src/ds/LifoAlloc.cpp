/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LifoAlloc.h"

#include <new>

using namespace js;

namespace js {
namespace detail {

BumpChunk *
BumpChunk::new_(size_t chunkSize)
{
    JS_ASSERT(RoundUpPow2(chunkSize) == chunkSize);
    void *mem = js_malloc(chunkSize);
    if (!mem)
        return NULL;
    BumpChunk *result = new (mem) BumpChunk(chunkSize - sizeof(BumpChunk));

    /* 
     * We assume that the alignment of sAlign is less than that of
     * the underlying memory allocator -- creating a new BumpChunk should
     * always satisfy the sAlign alignment constraint.
     */
    JS_ASSERT(AlignPtr(result->bump) == result->bump);
    return result;
}

void
BumpChunk::delete_(BumpChunk *chunk)
{
#ifdef DEBUG
        memset(chunk, 0xcd, sizeof(*chunk) + chunk->bumpSpaceSize);
#endif
        js_free(chunk);
}

bool
BumpChunk::canAlloc(size_t n)
{
    char *aligned = AlignPtr(bump);
    char *bumped = aligned + n;
    return bumped <= limit && bumped > headerBase();
}

} /* namespace detail */
} /* namespace js */

void
LifoAlloc::freeAll()
{
    while (first) {
        BumpChunk *victim = first;
        first = first->next();
        BumpChunk::delete_(victim);
    }
    first = latest = last = NULL;
}

LifoAlloc::BumpChunk *
LifoAlloc::getOrCreateChunk(size_t n)
{
    if (first) {
        /* Look for existing, unused BumpChunks to satisfy the request. */
        while (latest->next()) {
            latest = latest->next();
            latest->resetBump(); /* This was an unused BumpChunk on the chain. */
            if (latest->canAlloc(n))
                return latest;
        }
    }

    size_t defaultChunkFreeSpace = defaultChunkSize_ - sizeof(BumpChunk);
    size_t chunkSize;
    if (n > defaultChunkFreeSpace) {
        size_t allocSizeWithHeader = n + sizeof(BumpChunk);

        /* Guard for overflow. */
        if (allocSizeWithHeader < n ||
            (allocSizeWithHeader & (size_t(1) << (tl::BitSize<size_t>::result - 1)))) {
            return NULL;
        }

        chunkSize = RoundUpPow2(allocSizeWithHeader);
    } else {
        chunkSize = defaultChunkSize_;
    }

    /* If we get here, we couldn't find an existing BumpChunk to fill the request. */
    BumpChunk *newChunk = BumpChunk::new_(chunkSize);
    if (!newChunk)
        return NULL;
    if (!first) {
        latest = first = last = newChunk;
    } else {
        JS_ASSERT(latest && !latest->next());
        latest->setNext(newChunk);
        latest = last = newChunk;
    }
    return newChunk;
}

void
LifoAlloc::transferFrom(LifoAlloc *other)
{
    JS_ASSERT(!markCount);
    JS_ASSERT(latest == first);
    JS_ASSERT(!other->markCount);

    if (!other->first)
        return;

    append(other->first, other->last);
    other->first = other->last = other->latest = NULL;
}

void
LifoAlloc::transferUnusedFrom(LifoAlloc *other)
{
    JS_ASSERT(!markCount);
    JS_ASSERT(latest == first);

    if (other->markCount || !other->first)
        return;

    /*
     * Because of how getOrCreateChunk works, there may be unused chunks before
     * |last|. We do not transfer those chunks. In most cases it is expected
     * that last == first, so this should be a rare situation that, when it
     * happens, should not recur.
     */

    if (other->latest->next()) {
        append(other->latest->next(), other->last);
        other->latest->setNext(NULL);
        other->last = other->latest;
    }
}
