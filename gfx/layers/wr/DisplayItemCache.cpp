/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayItemCache.h"

namespace mozilla {
namespace layers {

void DisplayItemCache::UpdateState(const bool aPartialDisplayListBuildFailed,
                                   const wr::PipelineId& aPipelineId) {
  if (!IsEnabled()) {
    return;
  }

  // Clear the cache if the partial display list build failed, or if the
  // pipeline id changed.
  const bool clearCache =
      UpdatePipelineId(aPipelineId) || aPartialDisplayListBuildFailed;

  if (clearCache) {
    memset(mCachedItemState.Elements(), 0,
           mCachedItemState.Length() * sizeof(CacheEntry));
    mNextIndex = 0;
    mFreeList.Clear();
  }

  PopulateFreeList(clearCache);
}

void DisplayItemCache::PopulateFreeList(const bool aAddAll) {
  uint16_t index = 0;
  for (auto& state : mCachedItemState) {
    if (aAddAll || (!state.mUsed && state.mCached)) {
      // This entry contained a cached item, but was not used.
      state.mCached = false;
      mFreeList.AppendElement(index);
    }

    state.mUsed = false;
    index++;
  }
}

static bool CanCacheItem(const nsDisplayItem* aItem) {
  // Only cache leaf display items that can be reused.
  if (!aItem->CanBeReused()) {
    return false;
  }

  switch (aItem->GetType()) {
    case DisplayItemType::TYPE_BACKGROUND_COLOR:
      // case DisplayItemType::TYPE_TEXT:
      MOZ_ASSERT(!aItem->HasChildren());
      return true;
    default:
      return false;
  }
}

void DisplayItemCache::MaybeStartCaching(nsPaintedDisplayItem* aItem,
                                         wr::DisplayListBuilder& aBuilder) {
  if (!IsEnabled()) {
    return;
  }

  Stats().AddTotal();

  auto& index = aItem->CacheIndex();
  if (!index) {
    if (!CanCacheItem(aItem)) {
      // The item cannot be cached.
      return;
    }

    index = GetNextCacheIndex();
    if (!index) {
      // The item does not fit in the cache.
      return;
    }
  }

  // Update the current cache index, which is used by |MaybeEndCaching()| below.
  MOZ_ASSERT(!mCurrentIndex);
  mCurrentIndex = index;

  MOZ_ASSERT(CanCacheItem(aItem));
  MOZ_ASSERT(mCurrentIndex && CurrentCacheSize() > *mCurrentIndex);

  auto& state = mCachedItemState[*mCurrentIndex];
  MOZ_ASSERT(!state.mCached);
  state.mCached = true;
  MOZ_ASSERT(!state.mUsed);
  state.mUsed = true;
  state.mSpaceAndClip = aBuilder.CurrentSpaceAndClipChain();

  Stats().AddCached();
  aBuilder.StartCachedItem(*mCurrentIndex);
}

void DisplayItemCache::MaybeEndCaching(wr::DisplayListBuilder& aBuilder) {
  if (IsEnabled() && mCurrentIndex) {
    aBuilder.EndCachedItem(*mCurrentIndex);
    mCurrentIndex = Nothing();
  }
}

bool DisplayItemCache::ReuseItem(nsPaintedDisplayItem* aItem,
                                 wr::DisplayListBuilder& aBuilder) {
  if (!IsEnabled()) {
    return false;
  }

  auto& index = aItem->CacheIndex();
  if (!index) {
    return false;
  }

  auto& state = mCachedItemState[*index];
  if (!state.mCached) {
    // The display item has a stale cache state.
    return false;  // Recache item.
  }

  // Spatial id and clip id can change between display lists.
  if (!(aBuilder.CurrentSpaceAndClipChain() == state.mSpaceAndClip)) {
    // TODO(miko): Technically we might be able to update just the changed data
    // here but it adds a lot of complexity.
    // Mark the cache state false and recache the item.
    state.mCached = false;
    return false;
  }

  Stats().AddReused();
  Stats().AddTotal();

  state.mUsed = true;
  aBuilder.ReuseItem(*index);
  return true;
}

}  // namespace layers
}  // namespace mozilla
