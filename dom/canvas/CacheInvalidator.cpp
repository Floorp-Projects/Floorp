/* -*- Mode: C++; tab-width: 13; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=13 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CacheInvalidator.h"

namespace mozilla {

void CacheInvalidator::InvalidateCaches() const {
  // The only sane approach is to require caches to remove invalidators.
  while (!mCaches.empty()) {
    const auto& itr = mCaches.begin();
    const auto pEntry = *itr;
    pEntry->OnInvalidate();
    MOZ_ASSERT(mCaches.find(pEntry) == mCaches.end());
  }
}

// -

void AbstractCache::ResetInvalidators(InvalidatorListT&& newList) {
  for (const auto& cur : mInvalidators) {
    if (cur) {
      (void)cur->mCaches.erase(this);
    }
  }

  mInvalidators = std::move(newList);

  for (const auto& cur : mInvalidators) {
    // Don't assert that we insert, since there may be dupes in `invalidators`.
    // (and it's not worth removing the dupes)
    if (cur) {
      (void)cur->mCaches.insert(this);
    }
  }
}

void AbstractCache::AddInvalidator(const CacheInvalidator& x) {
  mInvalidators.push_back(&x);
  x.mCaches.insert(this);
}

}  // namespace mozilla
