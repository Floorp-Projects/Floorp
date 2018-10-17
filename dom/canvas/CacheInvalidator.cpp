/* -*- Mode: C++; tab-width: 13; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=13 sts=4 et sw=4 tw=90: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheInvalidator.h"

namespace mozilla {

void
CacheInvalidator::InvalidateCaches() const
{
    // The only sane approach is to require caches to remove invalidators.
    while (mCaches.size()) {
        const auto& itr = mCaches.begin();
        const auto pEntry = *itr;
        pEntry->OnInvalidate();
        MOZ_ASSERT(mCaches.find(pEntry) == mCaches.end());
    }
}

// -

AbstractCache::InvalidatorListT
AbstractCache::ResetInvalidators(InvalidatorListT&& newList)
{
    for (const auto& cur : mInvalidators) {
        if (cur) {
            (void)cur->mCaches.erase(this);
        }
    }

    auto ret = std::move(mInvalidators);
    mInvalidators = std::move(newList);

    for (const auto& cur : mInvalidators) {
        // Don't assert that we insert, since there may be dupes in `invalidators`.
        // (and it's not worth removing the dupes)
        if (cur) {
            (void)cur->mCaches.insert(this);
        }
    }

    return ret;
}

void
AbstractCache::AddInvalidator(const CacheInvalidator& x)
{
    mInvalidators.push_back(&x);
    x.mCaches.insert(this);
}

} // namespace mozilla
