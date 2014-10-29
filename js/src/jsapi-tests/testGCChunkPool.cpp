/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
* vim: set ts=8 sts=4 et sw=4 tw=99:
*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Move.h"

#include "gc/GCRuntime.h"
#include "gc/Heap.h"

#include "jsapi-tests/tests.h"

BEGIN_TEST(testGCChunkPool)
{
    const int N = 10;
    js::gc::ChunkPool pool;

    // Create.
    for (int i = 0; i < N; ++i) {
        js::gc::Chunk *chunk = js::gc::Chunk::allocate(rt);
        CHECK(chunk);
        pool.push(chunk);
    }
    MOZ_ASSERT(pool.verify());

    // Iterate.
    uint32_t i = 0;
    for (js::gc::ChunkPool::Enum e(pool); !e.empty(); e.popFront(), ++i)
        CHECK(e.front());
    CHECK(i == pool.count());
    MOZ_ASSERT(pool.verify());

    // Push/Pop.
    for (int i = 0; i < N; ++i) {
        js::gc::Chunk *chunkA = pool.pop();
        js::gc::Chunk *chunkB = pool.pop();
        js::gc::Chunk *chunkC = pool.pop();
        pool.push(chunkA);
        pool.push(chunkB);
        pool.push(chunkC);
    }
    MOZ_ASSERT(pool.verify());

    // Remove.
    js::gc::Chunk *chunk = nullptr;
    int offset = N / 2;
    for (js::gc::ChunkPool::Enum e(pool); !e.empty(); e.popFront(), --offset) {
        if (offset == 0) {
            chunk = pool.remove(e.front());
            break;
        }
    }
    CHECK(chunk);
    MOZ_ASSERT(!pool.contains(chunk));
    MOZ_ASSERT(pool.verify());
    pool.push(chunk);

    // Destruct.
    js::AutoLockGC lock(rt);
    for (js::gc::ChunkPool::Enum e(pool); !e.empty();) {
        js::gc::Chunk *chunk = e.front();
        e.removeAndPopFront();
        js::gc::UnmapPages(chunk, js::gc::ChunkSize);
    }

    return true;
}
END_TEST(testGCChunkPool)
