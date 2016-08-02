/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/IntegerRange.h"

#include "js/Vector.h"
#include "jsapi-tests/tests.h"
#include "threading/Thread.h"
#include "vm/SharedImmutableStringsCache.h"

const int NUM_THREADS = 256;
const int NUM_ITERATIONS = 256;

const int NUM_STRINGS = 4;
const char16_t *const STRINGS[NUM_STRINGS] = {
    u"uno",
    u"dos",
    u"tres",
    u"quattro"
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
getString(CacheAndIndex* cacheAndIndex)
{
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

    js::Vector<js::Thread> threads(cx);
    CHECK(threads.reserve(NUM_THREADS));

    for (auto i : mozilla::MakeRange(NUM_THREADS)) {
        auto cacheAndIndex = js_new<CacheAndIndex>(&cache, i);
        CHECK(cacheAndIndex);
        threads.infallibleEmplaceBack(getString, cacheAndIndex);
    }

    for (auto& thread : threads)
        thread.join();

    return true;
}
END_TEST(testSharedImmutableStringsCache)
