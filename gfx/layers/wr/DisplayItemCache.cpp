/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayItemCache.h"

namespace mozilla {
namespace layers {

static const size_t kInitialCacheSize = 1024;
static const size_t kMaximumCacheSize = 10240;
static const size_t kCacheThreshold = 1;

DisplayItemCache::DisplayItemCache()
    : mMaximumSize(0), mConsecutivePartialDisplayLists(0) {
  if (XRE_IsContentProcess() &&
      StaticPrefs::gfx_webrender_enable_item_cache_AtStartup()) {
    SetCapacity(kInitialCacheSize, kMaximumCacheSize);
  }
}

void DisplayItemCache::ClearCache() {
  memset(mSlots.Elements(), 0, mSlots.Length() * sizeof(Slot));
  mFreeSlots.ClearAndRetainStorage();

  for (size_t i = 0; i < CurrentSize(); ++i) {
    mFreeSlots.AppendElement(i);
  }
}

Maybe<uint16_t> DisplayItemCache::GetNextFreeSlot() {
  if (mFreeSlots.IsEmpty() && !GrowIfPossible()) {
    return Nothing();
  }

  return Some(mFreeSlots.PopLastElement());
}

bool DisplayItemCache::GrowIfPossible() {
  if (IsFull()) {
    return false;
  }

  const auto currentSize = CurrentSize();
  MOZ_ASSERT(currentSize <= mMaximumSize);

  // New slots are added one by one, which is amortized O(1) time complexity due
  // to underlying storage implementation.
  mSlots.AppendElement();
  mFreeSlots.AppendElement(currentSize);
  return true;
}

void DisplayItemCache::FreeUnusedSlots() {
  for (size_t i = 0; i < CurrentSize(); ++i) {
    auto& slot = mSlots[i];

    if (!slot.mUsed && slot.mOccupied) {
      // This entry contained a cached item, but was not used.
      slot.mOccupied = false;
      mFreeSlots.AppendElement(i);
    }

    slot.mUsed = false;
  }
}

void DisplayItemCache::SetCapacity(const size_t aInitialSize,
                                   const size_t aMaximumSize) {
  mMaximumSize = aMaximumSize;
  mSlots.SetCapacity(aMaximumSize);
  mSlots.SetLength(aInitialSize);
  mFreeSlots.SetCapacity(aMaximumSize);
  ClearCache();
}

Maybe<uint16_t> DisplayItemCache::AssignSlot(nsPaintedDisplayItem* aItem) {
  if (kCacheThreshold > mConsecutivePartialDisplayLists) {
    // Wait for |kCacheThreshold| partial display list builds, before caching
    // display items. This is meant to avoid caching overhead for interactions
    // or pages that do not work well with retained display lists.
    // TODO: This is a speculative optimization to minimize regressions.
    return Nothing();
  }

  if (!aItem->CanBeReused()) {
    // Do not try to cache items that cannot be reused.
    return Nothing();
  }

  auto& slot = aItem->CacheIndex();
  if (!slot) {
    slot = GetNextFreeSlot();
    if (!slot) {
      // The item does not fit in the cache.
      return Nothing();
    }
  }

  MOZ_ASSERT(slot && CurrentSize() > *slot);
  return slot;
}

void DisplayItemCache::MarkSlotOccupied(
    uint16_t aSlotIndex, const wr::WrSpaceAndClipChain& aSpaceAndClip) {
  // Caching of the item succeeded, update the slot state.
  auto& slot = mSlots[aSlotIndex];
  MOZ_ASSERT(!slot.mOccupied);
  slot.mOccupied = true;
  MOZ_ASSERT(!slot.mUsed);
  slot.mUsed = true;
  slot.mSpaceAndClip = aSpaceAndClip;
}

Maybe<uint16_t> DisplayItemCache::CanReuseItem(
    nsPaintedDisplayItem* aItem, const wr::WrSpaceAndClipChain& aSpaceAndClip) {
  auto& slotIndex = aItem->CacheIndex();
  if (!slotIndex) {
    return Nothing();
  }

  MOZ_ASSERT(slotIndex && CurrentSize() > *slotIndex);

  auto& slot = mSlots[*slotIndex];
  if (!slot.mOccupied) {
    // The display item has a stale cache slot. Recache the item.
    return Nothing();
  }

  // Spatial id and clip id can change between display lists.
  if (!(aSpaceAndClip == slot.mSpaceAndClip)) {
    // Mark the cache slot inactive and recache the item.
    slot.mOccupied = false;
    return Nothing();
  }

  slot.mUsed = true;
  return slotIndex;
}

void DisplayItemCache::UpdateState(const bool aPartialDisplayListBuildFailed,
                                   const wr::PipelineId& aPipelineId) {
  const bool pipelineIdChanged = UpdatePipelineId(aPipelineId);
  const bool invalidate = pipelineIdChanged || aPartialDisplayListBuildFailed;

  mConsecutivePartialDisplayLists =
      invalidate ? 0 : mConsecutivePartialDisplayLists + 1;

  if (IsEmpty()) {
    // The cache is empty so nothing needs to be updated.
    return;
  }

  // Clear the cache if the partial display list build failed, or if the
  // pipeline id changed.
  if (invalidate) {
    ClearCache();
  } else {
    FreeUnusedSlots();
  }
}

}  // namespace layers
}  // namespace mozilla
