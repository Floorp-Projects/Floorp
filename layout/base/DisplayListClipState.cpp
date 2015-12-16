/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayListClipState.h"

#include "nsDisplayList.h"

namespace mozilla {

const DisplayItemClip*
DisplayListClipState::GetCurrentCombinedClip(nsDisplayListBuilder* aBuilder)
{
  if (mCurrentCombinedClip) {
    return mCurrentCombinedClip;
  }
  if (!mClipContentDescendants && !mClipContainingBlockDescendants) {
    return nullptr;
  }
  if (mClipContentDescendants) {
    if (mClipContainingBlockDescendants) {
      DisplayItemClip intersection = *mClipContentDescendants;
      intersection.IntersectWith(*mClipContainingBlockDescendants);
      mCurrentCombinedClip = aBuilder->AllocateDisplayItemClip(intersection);
    } else {
      mCurrentCombinedClip =
        aBuilder->AllocateDisplayItemClip(*mClipContentDescendants);
    }
  } else {
    mCurrentCombinedClip =
      aBuilder->AllocateDisplayItemClip(*mClipContainingBlockDescendants);
  }
  return mCurrentCombinedClip;
}

void
DisplayListClipState::ClipContainingBlockDescendants(const nsRect& aRect,
                                                     const nscoord* aRadii,
                                                     DisplayItemClip& aClipOnStack)
{
  if (aRadii) {
    aClipOnStack.SetTo(aRect, aRadii);
  } else {
    aClipOnStack.SetTo(aRect);
  }
  if (mClipContainingBlockDescendants) {
    aClipOnStack.IntersectWith(*mClipContainingBlockDescendants);
  }
  mClipContainingBlockDescendants = &aClipOnStack;
  mCurrentCombinedClip = nullptr;
}

void
DisplayListClipState::ClipContentDescendants(const nsRect& aRect,
                                             const nscoord* aRadii,
                                             DisplayItemClip& aClipOnStack)
{
  if (aRadii) {
    aClipOnStack.SetTo(aRect, aRadii);
  } else {
    aClipOnStack.SetTo(aRect);
  }
  if (mClipContentDescendants) {
    aClipOnStack.IntersectWith(*mClipContentDescendants);
  }
  mClipContentDescendants = &aClipOnStack;
  mCurrentCombinedClip = nullptr;
}

void
DisplayListClipState::ClipContentDescendants(const nsRect& aRect,
                                             const nsRect& aRoundedRect,
                                             const nscoord* aRadii,
                                             DisplayItemClip& aClipOnStack)
{
  if (aRadii) {
    aClipOnStack.SetTo(aRect, aRoundedRect, aRadii);
  } else {
    nsRect intersect = aRect.Intersect(aRoundedRect);
    aClipOnStack.SetTo(intersect);
  }
  if (mClipContentDescendants) {
    aClipOnStack.IntersectWith(*mClipContentDescendants);
  }
  mClipContentDescendants = &aClipOnStack;
  mCurrentCombinedClip = nullptr;
}

void
DisplayListClipState::ClipContainingBlockDescendantsToContentBox(nsDisplayListBuilder* aBuilder,
                                                                 nsIFrame* aFrame,
                                                                 DisplayItemClip& aClipOnStack,
                                                                 uint32_t aFlags)
{
  nscoord radii[8];
  bool hasBorderRadius = aFrame->GetContentBoxBorderRadii(radii);
  if (!hasBorderRadius && (aFlags & ASSUME_DRAWING_RESTRICTED_TO_CONTENT_RECT)) {
    return;
  }

  nsRect clipRect = aFrame->GetContentRectRelativeToSelf() +
    aBuilder->ToReferenceFrame(aFrame);
  // If we have a border-radius, we have to clip our content to that
  // radius.
  ClipContainingBlockDescendants(clipRect, hasBorderRadius ? radii : nullptr,
                                 aClipOnStack);
}

DisplayListClipState::AutoSaveRestore::AutoSaveRestore(nsDisplayListBuilder* aBuilder)
  : mState(aBuilder->ClipState())
  , mSavedState(aBuilder->ClipState())
  , mClipUsed(false)
  , mRestored(false)
{}

} // namespace mozilla
