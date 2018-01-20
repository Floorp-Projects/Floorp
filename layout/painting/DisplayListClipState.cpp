/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayListClipState.h"

#include "nsDisplayList.h"

namespace mozilla {

const DisplayItemClipChain*
DisplayListClipState::GetCurrentCombinedClipChain(nsDisplayListBuilder* aBuilder)
{
  if (mCurrentCombinedClipChainIsValid) {
    return mCurrentCombinedClipChain;
  }
  if (!mClipChainContentDescendants && !mClipChainContainingBlockDescendants) {
    mCurrentCombinedClipChain = nullptr;
    mCurrentCombinedClipChainIsValid = true;
    return nullptr;
  }

  mCurrentCombinedClipChain =
    aBuilder->CreateClipChainIntersection(mCurrentCombinedClipChain,
                                             mClipChainContentDescendants,
                                             mClipChainContainingBlockDescendants);
  mCurrentCombinedClipChainIsValid = true;
  return mCurrentCombinedClipChain;
}

static void
ApplyClip(nsDisplayListBuilder* aBuilder,
          const DisplayItemClipChain*& aClipToModify,
          const ActiveScrolledRoot* aASR,
          DisplayItemClipChain& aClipChainOnStack)
{
  aClipChainOnStack.mASR = aASR;
  if (aClipToModify && aClipToModify->mASR == aASR) {
    // Intersect with aClipToModify and replace the clip chain item.
    aClipChainOnStack.mClip.IntersectWith(aClipToModify->mClip);
    aClipChainOnStack.mParent = aClipToModify->mParent;
    aClipToModify = &aClipChainOnStack;
  } else if (!aClipToModify ||
             ActiveScrolledRoot::IsAncestor(aClipToModify->mASR, aASR)) {
    // Add a new clip chain item at the bottom.
    aClipChainOnStack.mParent = aClipToModify;
    aClipToModify = &aClipChainOnStack;
  } else {
    // We need to insert / intersect a DisplayItemClipChain in the middle of the
    // aClipToModify chain. This is a very rare case.
    // Find the common ancestor and have the builder create the DisplayItemClipChain
    // intersection. This will create new DisplayItemClipChain objects for all
    // descendants of ancestorSC and we will not hold on to a pointer to
    // aClipChainOnStack.
    const DisplayItemClipChain* ancestorSC = aClipToModify;
    while (ancestorSC && ActiveScrolledRoot::IsAncestor(aASR, ancestorSC->mASR)) {
      ancestorSC = ancestorSC->mParent;
    }
    ancestorSC = aBuilder->CopyWholeChain(ancestorSC);
    aClipChainOnStack.mParent = nullptr;
    aClipToModify =
      aBuilder->CreateClipChainIntersection(ancestorSC, aClipToModify, &aClipChainOnStack);
  }
}

void
DisplayListClipState::ClipContainingBlockDescendants(nsDisplayListBuilder* aBuilder,
                                                     const nsRect& aRect,
                                                     const nscoord* aRadii,
                                                     DisplayItemClipChain& aClipChainOnStack)
{
  if (aRadii) {
    aClipChainOnStack.mClip.SetTo(aRect, aRadii);
  } else {
    aClipChainOnStack.mClip.SetTo(aRect);
  }
  const ActiveScrolledRoot* asr = aBuilder->CurrentActiveScrolledRoot();
  ApplyClip(aBuilder, mClipChainContainingBlockDescendants, asr, aClipChainOnStack);
  InvalidateCurrentCombinedClipChain(asr);
}

void
DisplayListClipState::ClipContentDescendants(nsDisplayListBuilder* aBuilder,
                                             const nsRect& aRect,
                                             const nscoord* aRadii,
                                             DisplayItemClipChain& aClipChainOnStack)
{
  if (aRadii) {
    aClipChainOnStack.mClip.SetTo(aRect, aRadii);
  } else {
    aClipChainOnStack.mClip.SetTo(aRect);
  }
  const ActiveScrolledRoot* asr = aBuilder->CurrentActiveScrolledRoot();
  ApplyClip(aBuilder, mClipChainContentDescendants, asr, aClipChainOnStack);
  InvalidateCurrentCombinedClipChain(asr);
}

void
DisplayListClipState::ClipContentDescendants(nsDisplayListBuilder* aBuilder,
                                             const nsRect& aRect,
                                             const nsRect& aRoundedRect,
                                             const nscoord* aRadii,
                                             DisplayItemClipChain& aClipChainOnStack)
{
  if (aRadii) {
    aClipChainOnStack.mClip.SetTo(aRect, aRoundedRect, aRadii);
  } else {
    nsRect intersect = aRect.Intersect(aRoundedRect);
    aClipChainOnStack.mClip.SetTo(intersect);
  }
  const ActiveScrolledRoot* asr = aBuilder->CurrentActiveScrolledRoot();
  ApplyClip(aBuilder, mClipChainContentDescendants, asr, aClipChainOnStack);
  InvalidateCurrentCombinedClipChain(asr);
}


void
DisplayListClipState::InvalidateCurrentCombinedClipChain(const ActiveScrolledRoot* aInvalidateUpTo)
{
  mCurrentCombinedClipChainIsValid = false;
  while (mCurrentCombinedClipChain &&
         ActiveScrolledRoot::IsAncestor(aInvalidateUpTo, mCurrentCombinedClipChain->mASR)) {
    mCurrentCombinedClipChain = mCurrentCombinedClipChain->mParent;
  }
}

void
DisplayListClipState::ClipContainingBlockDescendantsToContentBox(nsDisplayListBuilder* aBuilder,
                                                                 nsIFrame* aFrame,
                                                                 DisplayItemClipChain& aClipChainOnStack,
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
  ClipContainingBlockDescendants(aBuilder, clipRect, hasBorderRadius ? radii : nullptr,
                                 aClipChainOnStack);
}

DisplayListClipState::AutoSaveRestore::AutoSaveRestore(nsDisplayListBuilder* aBuilder)
  : mBuilder(aBuilder)
  , mState(aBuilder->ClipState())
  , mSavedState(aBuilder->ClipState())
#ifdef DEBUG
  , mClipUsed(false)
  , mRestored(false)
#endif
{}

} // namespace mozilla
