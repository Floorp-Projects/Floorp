/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/IntegerRange.h"

#include "js/Vector.h"
#include "jsapi-tests/tests.h"

#include "vm/SharedImmutableStringsCache.h"

const int NUM_THREADS = 256;
const int NUM_ITERATIONS = 256;

const int NUM_STRINGS = 4;
const char16_t* STRINGS[NUM_STRINGS] = {
    MOZ_UTF16("uno"),
    MOZ_UTF16("dos"),
    MOZ_UTF16("tres"),
    MOZ_UTF16("quattro")
};

struct CacheAndIndex
{
    js::SharedImmutableStringsCache* cache;
    int index;

    CacheAndIndex(js::SharedImmutableStringsCache* cache, int index)
      : cache(cache)
      , index(index)
    { }
};

static void
getString(void* data)
{
    auto cacheAndIndex = reinterpret_cast<CacheAndIndex*>(data);

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        auto str = STRINGS[cacheAndIndex->index % NUM_STRINGS];

        auto dupe = js::DuplicateString(str);
        MOZ_RELEASE_ASSERT(dupe);

        auto deduped = cacheAndIndex->cache->getOrCreate(mozilla::Move(dupe), js_strlen(str));
        MOZ_RELEASE_ASSERT(deduped.isSome());
        MOZ_RELEASE_ASSERT(js_strcmp(str, deduped->chars()) == 0);

        {
            auto cloned = deduped->clone();
            // We should be de-duplicating and giving back the same string.
            MOZ_RELEASE_ASSERT(deduped->chars() == cloned.chars());
        }
    }

    js_delete(cacheAndIndex);
}

BEGIN_TEST(testSharedImmutableStringsCache)
{
    auto maybeCache = js::SharedImmutableStringsCache::Create();
    CHECK(maybeCache.isSome());
    auto& cache = *maybeCache;

    js::Vector<PRThread*> threads(cx);
    CHECK(threads.reserve(NUM_THREADS));

    for (auto i : mozilla::MakeRange(NUM_THREADS)) {
        auto cacheAndIndex = js_new<CacheAndIndex>(&cache, i);
        CHECK(cacheAndIndex);
        auto thread = PR_CreateThread(PR_USER_THREAD,
                                      getString,
                                      (void *) cacheAndIndex,
                                      PR_PRIORITY_NORMAL,
                                      PR_LOCAL_THREAD,
                                      PR_JOINABLE_THREAD,
                                      0);
        CHECK(thread);
        threads.infallibleAppend(thread);
    }

    for (auto thread : threads) {
        CHECK(PR_JoinThread(thread) == PR_SUCCESS);
    }

    return true;
}
END_TEST(testSharedImmutableStringsCache)
