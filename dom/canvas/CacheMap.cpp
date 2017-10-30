/* -*- Mode: C++; tab-width: 13; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=13 sts=4 et sw=4 tw=90: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheMap.h"

namespace mozilla {

void
CacheMapInvalidator::InvalidateCaches() const
{
    while (mCacheEntries.size()) {
        const auto& entry = *(mCacheEntries.begin());
        entry->Invalidate();
        MOZ_ASSERT(mCacheEntries.find(entry) == mCacheEntries.end());
    }
}

namespace detail {

CacheMapUntypedEntry::CacheMapUntypedEntry(std::vector<const CacheMapInvalidator*>&& invalidators)
    : mInvalidators(Move(invalidators))
{
    for (const auto& cur : mInvalidators) {
        // Don't assert that we insert, since there may be dupes in `invalidators`.
        // (and it's not worth removing the dupes)
        (void)cur->mCacheEntries.insert(this);
    }
}

CacheMapUntypedEntry::~CacheMapUntypedEntry()
{
    for (const auto& cur : mInvalidators) {
        // There might be dupes, so erase might return >1.
        (void)cur->mCacheEntries.erase(this);
    }
}

} // namespace detail

} // namespace mozilla
