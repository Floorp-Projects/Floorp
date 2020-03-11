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
    static uint64_t avgC = 1;
    static uint64_t avgR = 1;
    static uint64_t avgT = 1;

    avgC += mCached;
    avgR += mReused;
    avgT += mTotal;

    printf("Cached: %zu (avg: %f), Reused: %zu (avg: %f), Total: %zu\n",
           mCached, (double)avgC / (double)avgT, mReused,
           (double)avgR / (double)avgT, mTotal);
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
  DisplayItemCache();

  /**
   * Returns true if display item caching is enabled, otherwise false.
   */
  bool IsEnabled() const { return mMaximumSize > 0; }

  /**
   * Returns true if there are no cached items, otherwise false.
   */
  bool IsEmpty() const { return mFreeSlots.Length() == CurrentSize(); }

  /**
   * Returns true if the cache has reached the maximum size, otherwise false.
   */
  bool IsFull() const {
    return mFreeSlots.IsEmpty() && CurrentSize() == mMaximumSize;
  }

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
  size_t CurrentSize() const { return mSlots.Length(); }

  /**
   * If there are free slots in the cache, assigns a cache slot to the given
   * display item |aItem| and returns it. Otherwise returns Nothing().
   */
  Maybe<uint16_t> AssignSlot(nsPaintedDisplayItem* aItem);

  /**
   * Marks the slot with the given |slotIndex| occupied and used.
   * Also stores the current space and clipchain |aSpaceAndClip|.
   */
  void MarkSlotOccupied(uint16_t slotIndex,
                        const wr::WrSpaceAndClipChain& aSpaceAndClip);

  /**
   * Returns the slot index of the the given display item |aItem|, if the item
   * can be reused. The current space and clipchain |aSpaceAndClip| is used to
   * check whether the cached item is still valid.
   * If the item cannot be reused, returns Nothing().
   */
  Maybe<uint16_t> CanReuseItem(nsPaintedDisplayItem* aItem,
                               const wr::WrSpaceAndClipChain& aSpaceAndClip);

  CacheStats& Stats() { return mCacheStats; }

 private:
  struct Slot {
    Slot() : mSpaceAndClip{}, mOccupied(false), mUsed(false) {}

    wr::WrSpaceAndClipChain mSpaceAndClip;
    bool mOccupied;
    bool mUsed;
  };

  void ClearCache();
  void FreeUnusedSlots();
  bool GrowIfPossible();
  Maybe<uint16_t> GetNextFreeSlot();

  /**
   * Sets the initial and max cache size to given |aInitialSize| and |aMaxSize|.
   */
  void SetCapacity(const size_t aInitialSize, const size_t aMaximumSize);

  /**
   * Returns true if the given |aPipelineId| is different from the previous one,
   * otherwise returns false.
   */
  bool UpdatePipelineId(const wr::PipelineId& aPipelineId) {
    const bool isSame = mPreviousPipelineId.refOr(aPipelineId) == aPipelineId;
    mPreviousPipelineId = Some(aPipelineId);
    return !isSame;
  }

  size_t mMaximumSize;
  nsTArray<Slot> mSlots;
  nsTArray<uint16_t> mFreeSlots;
  Maybe<wr::PipelineId> mPreviousPipelineId;
  size_t mConsecutivePartialDisplayLists;
  CacheStats mCacheStats;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_DISPLAY_ITEM_CACHE_H */
