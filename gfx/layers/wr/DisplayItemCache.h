/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_DISPLAY_ITEM_CACHE_H
#define GFX_DISPLAY_ITEM_CACHE_H

#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/Maybe.h"
#include "nsTArray.h"

class nsPaintedDisplayItem;

namespace mozilla {

namespace wr {
class DisplayListBuilder;
}  // namespace wr

namespace layers {

class CacheStats {
 public:
  CacheStats() = default;

  void Reset() { mCached = mReused = mTotal = 0; }

  void Print() {
    printf("Cached: %zu, Reused: %zu, Total: %zu\n", mCached, mReused, mTotal);
  }

  void AddCached() { mCached++; }
  void AddReused() { mReused++; }
  void AddTotal() { mTotal++; }

 private:
  size_t mCached = 0;
  size_t mReused = 0;
  size_t mTotal = 0;
};

/**
 * DisplayItemCache keeps track of which Gecko display items have already had
 * their respective WebRender display items sent to WebRender backend.
 *
 * Ideally creating the WR display items for a Gecko display item would not
 * depend on any external state. However currently pipeline id, clip id, and
 * spatial id can change between display lists, even if the Gecko display items
 * have not. This state is tracked by DisplayItemCache.
 */
class DisplayItemCache final {
 public:
  DisplayItemCache() : mMaxCacheSize(0), mNextIndex(0) {}

  bool IsEnabled() const { return mMaxCacheSize > 0; }

  /**
   * Updates the cache state based on the given display list build information
   * and pipeline id.
   *
   * This is necessary because Gecko display items can only be reused for the
   * partial display list builds following a full display list build.
   */
  void UpdateState(const bool aPartialDisplayListBuildFailed,
                   const wr::PipelineId& aPipelineId);

  /**
   * Returns the current cache size.
   */
  size_t CurrentCacheSize() const {
    return IsEnabled() ? mCachedItemState.Length() : 0;
  }

  /**
   * Sets the initial and max cache size to given |aInitialSize| and |aMaxSize|.
   *
   * Currently the cache size is constant, but a good improvement would be to
   * set the initial and maximum size based on the display list length.
   */
  void SetCapacity(const size_t aInitialSize, const size_t aMaxSize) {
    mMaxCacheSize = aMaxSize;
    mCachedItemState.SetCapacity(aMaxSize);
    mCachedItemState.SetLength(aInitialSize);
    mFreeList.SetCapacity(aMaxSize);
  }

  /**
   * If the given display item |aItem| can be cached, update the cache state of
   * the item and tell WR DisplayListBuilder |aBuilder| to cache WR display
   * items until |EndCaching()| is called.
   *
   * If the display item cannot be cached, this function does nothing.
   */
  void MaybeStartCaching(nsPaintedDisplayItem* aItem,
                         wr::DisplayListBuilder& aBuilder);

  /**
   * Tell WR DisplayListBuilder |aBuilder| to stop caching WR display items.
   *
   * If the display item cannot be cached, this function does nothing.
   */
  void MaybeEndCaching(wr::DisplayListBuilder& aBuilder);

  /**
   * If the given |aItem| has been cached, tell WR DisplayListBuilder |aBuilder|
   * to reuse it.
   * Returns true if the item was reused, otherwise returns false.
   */
  bool ReuseItem(nsPaintedDisplayItem* aItem, wr::DisplayListBuilder& aBuilder);

  CacheStats& Stats() { return mCacheStats; }

 private:
  struct CacheEntry {
    wr::WrSpaceAndClipChain mSpaceAndClip;
    bool mCached;
    bool mUsed;
  };

  Maybe<uint16_t> GetNextCacheIndex() {
    if (mFreeList.IsEmpty()) {
      return Nothing();
    }

    return Some(mFreeList.PopLastElement());
  }

  /**
   * Iterates through |mCachedItemState| and adds unused entries to free list.
   * If |aAddAll| is true, adds every entry regardless of the state.
   */
  void PopulateFreeList(const bool aAddAll);

  /**
   * Returns true if the given |aPipelineId| is different from the previous one,
   * otherwise returns false.
   */
  bool UpdatePipelineId(const wr::PipelineId& aPipelineId) {
    const bool isSame = mPreviousPipelineId.refOr(aPipelineId) == aPipelineId;
    mPreviousPipelineId = Some(aPipelineId);
    return !isSame;
  }

  nsTArray<CacheEntry> mCachedItemState;
  nsTArray<uint16_t> mFreeList;
  size_t mMaxCacheSize;
  uint16_t mNextIndex;
  Maybe<uint16_t> mCurrentIndex;
  Maybe<wr::PipelineId> mPreviousPipelineId;
  CacheStats mCacheStats;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_DISPLAY_ITEM_CACHE_H */
