/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Leary <cdleary@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

bool
BumpChunk::canAllocUnaligned(size_t n)
{
    char *bumped = bump + n;
    return bumped <= limit && bumped > headerBase();
}

void *
BumpChunk::tryAllocUnaligned(size_t n)
{
    char *oldBump = bump;
    char *newBump = bump + n;
    if (newBump > limit)
        return NULL;

    JS_ASSERT(canAllocUnaligned(n));
    setBump(newBump);
    return oldBump;
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
    first = latest = NULL;
}

void
LifoAlloc::freeUnused()
{
    /* Don't free anything if we have outstanding marks. */
    if (markCount || !first)
        return; 

    JS_ASSERT(first && latest);

    /* Rewind through any unused chunks. */
    if (!latest->used()) {
        BumpChunk *lastUsed = NULL;
        for (BumpChunk *it = first; it != latest; it = it->next()) {
            if (it->used())
                lastUsed = it;
        }
        if (!lastUsed) {
            freeAll();
            return;
        }
        latest = lastUsed;
    }

    /* Free all chunks after |latest|. */
    size_t freed = 0;
    for (BumpChunk *victim = latest->next(); victim; victim = victim->next()) {
        BumpChunk::delete_(victim);
        freed++;
    }
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
        latest = first = newChunk;
    } else {
        JS_ASSERT(latest && !latest->next());
        latest->setNext(newChunk);
        latest = newChunk;
    }
    return newChunk;
}

void *
LifoAlloc::allocUnaligned(size_t n)
{
    void *result;
    if (latest && (result = latest->tryAllocUnaligned(n)))
        return result;

    return alloc(n);
}

void *
LifoAlloc::reallocUnaligned(void *origPtr, size_t origSize, size_t incr)
{
    JS_ASSERT(first && latest);

    /*
     * Maybe we can grow the latest allocation in a BumpChunk.
     *
     * Note: we could also realloc the whole BumpChunk in the !canAlloc
     * case, but this should not be a frequently hit case.
     */
    if (latest
        && origPtr == (char *) latest->mark() - origSize
        && latest->canAllocUnaligned(incr)) {
        JS_ALWAYS_TRUE(allocUnaligned(incr));
        return origPtr;
    }

    /* Otherwise, memcpy. */
    size_t newSize = origSize + incr;
    void *newPtr = allocUnaligned(newSize);
    return newPtr ? memcpy(newPtr, origPtr, origSize) : NULL;
}
