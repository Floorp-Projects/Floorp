/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayListClipState.h"

#include "DisplayItemScrollClip.h"
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
DisplayListClipState::SetScrollClipForContainingBlockDescendants(
    nsDisplayListBuilder* aBuilder,
    const DisplayItemScrollClip* aScrollClip)
{
  if (aBuilder->IsPaintingToWindow() &&
      mClipContentDescendants &&
      aScrollClip != mScrollClipContainingBlockDescendants) {
    // Disable paint skipping for all scroll frames on the way to aScrollClip.
    for (const DisplayItemScrollClip* sc = mClipContentDescendantsScrollClip;
         sc && !DisplayItemScrollClip::IsAncestor(sc, aScrollClip);
         sc = sc->mParent) {
      if (sc->mScrollableFrame) {
        sc->mScrollableFrame->SetScrollsClipOnUnscrolledOutOfFlow();
      }
    }
    mClipContentDescendantsScrollClip = nullptr;
  }
  mScrollClipContainingBlockDescendants = aScrollClip;
  mStackingContextAncestorSC = DisplayItemScrollClip::PickAncestor(mStackingContextAncestorSC, aScrollClip);
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
  mClipContentDescendantsScrollClip = GetCurrentInnermostScrollClip();
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

const DisplayItemScrollClip*
DisplayListClipState::GetCurrentInnermostScrollClip()
{
  return DisplayItemScrollClip::PickDescendant(
    mScrollClipContentDescendants, mScrollClipContainingBlockDescendants);
}

void
DisplayListClipState::TurnClipIntoScrollClipForContentDescendants(
    nsDisplayListBuilder* aBuilder, nsIScrollableFrame* aScrollableFrame)
{
  const DisplayItemScrollClip* parent = GetCurrentInnermostScrollClip();
  mScrollClipContentDescendants =
    aBuilder->AllocateDisplayItemScrollClip(parent,
                                      aScrollableFrame,
                                      GetCurrentCombinedClip(aBuilder), true);
  Clear();
}

void
DisplayListClipState::TurnClipIntoScrollClipForContainingBlockDescendants(
    nsDisplayListBuilder* aBuilder, nsIScrollableFrame* aScrollableFrame)
{
  const DisplayItemScrollClip* parent = GetCurrentInnermostScrollClip();
  mScrollClipContainingBlockDescendants =
    aBuilder->AllocateDisplayItemScrollClip(parent,
                                      aScrollableFrame,
                                      GetCurrentCombinedClip(aBuilder), true);
  Clear();
}

const DisplayItemClip*
WithoutRoundedCorners(nsDisplayListBuilder* aBuilder, const DisplayItemClip* aClip)
{
  if (!aClip) {
    return nullptr;
  }
  DisplayItemClip rectClip(*aClip);
  rectClip.RemoveRoundedCorners();
  return aBuilder->AllocateDisplayItemClip(rectClip);
}

DisplayItemScrollClip*
DisplayListClipState::CreateInactiveScrollClip(
    nsDisplayListBuilder* aBuilder, nsIScrollableFrame* aScrollableFrame)
{
  // We ignore the rounded corners on the current clip because we don't want
  // them to be double-applied (as scroll clip and as regular clip).
  // Double-applying rectangle clips doesn't make a visual difference so it's
  // fine.
  const DisplayItemClip* rectClip =
    WithoutRoundedCorners(aBuilder, GetCurrentCombinedClip(aBuilder));

  const DisplayItemScrollClip* parent = GetCurrentInnermostScrollClip();
  DisplayItemScrollClip* scrollClip =
    aBuilder->AllocateDisplayItemScrollClip(parent,
                                            aScrollableFrame,
                                            rectClip, false);
  return scrollClip;
}

DisplayItemScrollClip*
DisplayListClipState::InsertInactiveScrollClipForContentDescendants(
    nsDisplayListBuilder* aBuilder, nsIScrollableFrame* aScrollableFrame)
{
  DisplayItemScrollClip* scrollClip =
    CreateInactiveScrollClip(aBuilder, aScrollableFrame);
  mScrollClipContentDescendants = scrollClip;
  return scrollClip;
}

DisplayItemScrollClip*
DisplayListClipState::InsertInactiveScrollClipForContainingBlockDescendants(
    nsDisplayListBuilder* aBuilder, nsIScrollableFrame* aScrollableFrame)
{
  DisplayItemScrollClip* scrollClip =
    CreateInactiveScrollClip(aBuilder, aScrollableFrame);
  mScrollClipContainingBlockDescendants = scrollClip;
  return scrollClip;
}

DisplayListClipState::AutoSaveRestore::AutoSaveRestore(nsDisplayListBuilder* aBuilder)
  : mState(aBuilder->ClipState())
  , mSavedState(aBuilder->ClipState())
#ifdef DEBUG
  , mClipUsed(false)
  , mRestored(false)
#endif
  , mClearedForStackingContextContents(false)
{
  mState.mStackingContextAncestorSC = mState.GetCurrentInnermostScrollClip();
}

} // namespace mozilla
