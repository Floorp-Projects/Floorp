/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayItemCache.h"
#include "nsDisplayList.h"

namespace mozilla {
namespace layers {

DisplayItemCache::DisplayItemCache()
    : mDisplayList(nullptr),
      mMaximumSize(0),
      mPipelineId{},
      mCaching(false),
      mInvalid(false) {}

void DisplayItemCache::SetDisplayList(nsDisplayListBuilder* aBuilder,
                                      nsDisplayList* aList) {
  if (!IsEnabled()) {
    return;
  }

  MOZ_ASSERT(aBuilder);
  MOZ_ASSERT(aList);

  const bool listChanged = mDisplayList != aList;
  const bool partialBuild = !aBuilder->PartialBuildFailed();

  if (listChanged && partialBuild) {
    // If the display list changed during a partial update, it means that
    // |SetDisplayList()| has missed one rebuilt display list.
    mDisplayList = nullptr;
    return;
  }

  if (listChanged || !partialBuild) {
    // The display list has been changed or rebuilt.
    mDisplayList = aList;
    mInvalid = true;
  }

  UpdateState();
}

void DisplayItemCache::SetPipelineId(const wr::PipelineId& aPipelineId) {
  mInvalid = mInvalid || !(mPipelineId == aPipelineId);
  mPipelineId = aPipelineId;
}

void DisplayItemCache::UpdateState() {
  // |mCaching == true| if:
  // 1) |SetDisplayList()| is called with a fully rebuilt display list
  // followed by
  // 2a) |SetDisplayList()| is called with a partially updated display list
  // or
  // 2b) |SkipWaitingForPartialDisplayList()| is called
  mCaching = !mInvalid;

#if 0
  Stats().Print();
  Stats().Reset();
#endif

  if (IsEmpty()) {
    // The cache is empty so nothing needs to be updated.
    mInvalid = false;
    return;
  }

  // Clear the cache if the current state is invalid.
  if (mInvalid) {
    Clear();
  } else {
    FreeUnusedSlots();
  }

  mInvalid = false;
}

void DisplayItemCache::Clear() {
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
  Clear();
}

Maybe<uint16_t> DisplayItemCache::AssignSlot(nsPaintedDisplayItem* aItem) {
  if (!mCaching || !aItem->CanBeReused() || !aItem->CanBeCached()) {
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

  if (!(aSpaceAndClip == slot.mSpaceAndClip)) {
    // Spatial id and clip id can change between display lists, if items that
    // generate them change their order.
    slot.mOccupied = false;
    aItem->SetCantBeCached();
    slotIndex = Nothing();
  } else {
    slot.mUsed = true;
  }

  return slotIndex;
}

}  // namespace layers
}  // namespace mozilla
