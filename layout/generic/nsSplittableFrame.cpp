/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * base class for rendering objects that can be split across lines,
 * columns, or pages
 */

#include "nsSplittableFrame.h"
#include "nsContainerFrame.h"
#include "nsFieldSetFrame.h"
#include "nsIFrameInlines.h"

using namespace mozilla;

void nsSplittableFrame::Init(nsIContent* aContent, nsContainerFrame* aParent,
                             nsIFrame* aPrevInFlow) {
  if (aPrevInFlow) {
    // Hook the frame into the flow
    SetPrevInFlow(aPrevInFlow);
    aPrevInFlow->SetNextInFlow(this);
  }
  nsIFrame::Init(aContent, aParent, aPrevInFlow);
}

void nsSplittableFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                    PostDestroyData& aPostDestroyData) {
  // Disconnect from the flow list
  if (mPrevContinuation || mNextContinuation) {
    RemoveFromFlow(this);
  }

  // Let the base class destroy the frame
  nsIFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

nsIFrame* nsSplittableFrame::GetPrevContinuation() const {
  return mPrevContinuation;
}

void nsSplittableFrame::SetPrevContinuation(nsIFrame* aFrame) {
  NS_ASSERTION(!aFrame || Type() == aFrame->Type(),
               "setting a prev continuation with incorrect type!");
  NS_ASSERTION(!IsInPrevContinuationChain(aFrame, this),
               "creating a loop in continuation chain!");
  mPrevContinuation = aFrame;
  RemoveStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
}

nsIFrame* nsSplittableFrame::GetNextContinuation() const {
  return mNextContinuation;
}

void nsSplittableFrame::SetNextContinuation(nsIFrame* aFrame) {
  NS_ASSERTION(!aFrame || Type() == aFrame->Type(),
               "setting a next continuation with incorrect type!");
  NS_ASSERTION(!IsInNextContinuationChain(aFrame, this),
               "creating a loop in continuation chain!");
  mNextContinuation = aFrame;
  if (aFrame) aFrame->RemoveStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
}

nsIFrame* nsSplittableFrame::FirstContinuation() const {
  nsSplittableFrame* firstContinuation = const_cast<nsSplittableFrame*>(this);
  while (firstContinuation->mPrevContinuation) {
    firstContinuation =
        static_cast<nsSplittableFrame*>(firstContinuation->mPrevContinuation);
  }
  MOZ_ASSERT(firstContinuation, "post-condition failed");
  return firstContinuation;
}

nsIFrame* nsSplittableFrame::LastContinuation() const {
  nsSplittableFrame* lastContinuation = const_cast<nsSplittableFrame*>(this);
  while (lastContinuation->mNextContinuation) {
    lastContinuation =
        static_cast<nsSplittableFrame*>(lastContinuation->mNextContinuation);
  }
  MOZ_ASSERT(lastContinuation, "post-condition failed");
  return lastContinuation;
}

#ifdef DEBUG
bool nsSplittableFrame::IsInPrevContinuationChain(nsIFrame* aFrame1,
                                                  nsIFrame* aFrame2) {
  int32_t iterations = 0;
  while (aFrame1 && iterations < 10) {
    // Bail out after 10 iterations so we don't bog down debug builds too much
    if (aFrame1 == aFrame2) return true;
    aFrame1 = aFrame1->GetPrevContinuation();
    ++iterations;
  }
  return false;
}

bool nsSplittableFrame::IsInNextContinuationChain(nsIFrame* aFrame1,
                                                  nsIFrame* aFrame2) {
  int32_t iterations = 0;
  while (aFrame1 && iterations < 10) {
    // Bail out after 10 iterations so we don't bog down debug builds too much
    if (aFrame1 == aFrame2) return true;
    aFrame1 = aFrame1->GetNextContinuation();
    ++iterations;
  }
  return false;
}
#endif

nsIFrame* nsSplittableFrame::GetPrevInFlow() const {
  return HasAnyStateBits(NS_FRAME_IS_FLUID_CONTINUATION) ? mPrevContinuation
                                                         : nullptr;
}

void nsSplittableFrame::SetPrevInFlow(nsIFrame* aFrame) {
  NS_ASSERTION(!aFrame || Type() == aFrame->Type(),
               "setting a prev in flow with incorrect type!");
  NS_ASSERTION(!IsInPrevContinuationChain(aFrame, this),
               "creating a loop in continuation chain!");
  mPrevContinuation = aFrame;
  AddStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
}

nsIFrame* nsSplittableFrame::GetNextInFlow() const {
  return mNextContinuation && mNextContinuation->HasAnyStateBits(
                                  NS_FRAME_IS_FLUID_CONTINUATION)
             ? mNextContinuation
             : nullptr;
}

void nsSplittableFrame::SetNextInFlow(nsIFrame* aFrame) {
  NS_ASSERTION(!aFrame || Type() == aFrame->Type(),
               "setting a next in flow with incorrect type!");
  NS_ASSERTION(!IsInNextContinuationChain(aFrame, this),
               "creating a loop in continuation chain!");
  mNextContinuation = aFrame;
  if (aFrame) aFrame->AddStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
}

nsIFrame* nsSplittableFrame::FirstInFlow() const {
  nsSplittableFrame* firstInFlow = const_cast<nsSplittableFrame*>(this);
  while (nsIFrame* prev = firstInFlow->GetPrevInFlow()) {
    firstInFlow = static_cast<nsSplittableFrame*>(prev);
  }
  MOZ_ASSERT(firstInFlow, "post-condition failed");
  return firstInFlow;
}

nsIFrame* nsSplittableFrame::LastInFlow() const {
  nsSplittableFrame* lastInFlow = const_cast<nsSplittableFrame*>(this);
  while (nsIFrame* next = lastInFlow->GetNextInFlow()) {
    lastInFlow = static_cast<nsSplittableFrame*>(next);
  }
  MOZ_ASSERT(lastInFlow, "post-condition failed");
  return lastInFlow;
}

// Remove this frame from the flow. Connects prev in flow and next in flow
void nsSplittableFrame::RemoveFromFlow(nsIFrame* aFrame) {
  nsIFrame* prevContinuation = aFrame->GetPrevContinuation();
  nsIFrame* nextContinuation = aFrame->GetNextContinuation();

  // The new continuation is fluid only if the continuation on both sides
  // of the removed frame was fluid
  if (aFrame->GetPrevInFlow() && aFrame->GetNextInFlow()) {
    if (prevContinuation) {
      prevContinuation->SetNextInFlow(nextContinuation);
    }
    if (nextContinuation) {
      nextContinuation->SetPrevInFlow(prevContinuation);
    }
  } else {
    if (prevContinuation) {
      prevContinuation->SetNextContinuation(nextContinuation);
    }
    if (nextContinuation) {
      nextContinuation->SetPrevContinuation(prevContinuation);
    }
  }

  // **Note: it is important here that we clear the Next link from aFrame
  // BEFORE clearing its Prev link, because in nsContinuingTextFrame,
  // SetPrevInFlow() would follow the Next pointers, wiping out the cached
  // mFirstContinuation field from each following frame in the list.
  aFrame->SetNextInFlow(nullptr);
  aFrame->SetPrevInFlow(nullptr);
}

NS_DECLARE_FRAME_PROPERTY_SMALL_VALUE(ConsumedBSizeProperty, nscoord);

nscoord nsSplittableFrame::CalcAndCacheConsumedBSize() {
  nsIFrame* prev = GetPrevContinuation();
  if (!prev) {
    return 0;
  }
  const auto wm = GetWritingMode();
  nscoord bSize = 0;
  for (; prev; prev = prev->GetPrevContinuation()) {
    if (prev->IsTrueOverflowContainer()) {
      // Overflow containers might not get reflowed, and they have no bSize
      // anyways.
      continue;
    }

    bSize += prev->ContentSize(wm).BSize(wm);
    bool found = false;
    nscoord consumed = prev->GetProperty(ConsumedBSizeProperty(), &found);
    if (found) {
      bSize += consumed;
      break;
    }
    MOZ_ASSERT(!prev->GetPrevContinuation(),
               "Property should always be set on prev continuation if not "
               "the first continuation");
  }
  SetProperty(ConsumedBSizeProperty(), bSize);
  return bSize;
}

nscoord nsSplittableFrame::GetEffectiveComputedBSize(
    const ReflowInput& aReflowInput, nscoord aConsumedBSize) const {
  nscoord bSize = aReflowInput.ComputedBSize();
  if (bSize == NS_UNCONSTRAINEDSIZE) {
    return NS_UNCONSTRAINEDSIZE;
  }

  bSize -= aConsumedBSize;

  // nsFieldSetFrame's inner frames are special since some of their content-box
  // BSize may be consumed by positioning it below the legend.  So we always
  // report zero for true overflow containers here.
  // XXXmats: hmm, can we fix this so that the sizes actually adds up instead?
  if (IsTrueOverflowContainer() &&
      Style()->GetPseudoType() == PseudoStyleType::fieldsetContent) {
    for (nsFieldSetFrame* fieldset = do_QueryFrame(GetParent()); fieldset;
         fieldset = static_cast<nsFieldSetFrame*>(fieldset->GetPrevInFlow())) {
      bSize -= fieldset->LegendSpace();
    }
  }

  // We may have stretched the frame beyond its computed height. Oh well.
  return std::max(0, bSize);
}

LogicalSides nsSplittableFrame::GetBlockLevelLogicalSkipSides(
    bool aAfterReflow) const {
  LogicalSides skip(mWritingMode);
  if (MOZ_UNLIKELY(IsTrueOverflowContainer())) {
    skip |= eLogicalSideBitsBBoth;
    return skip;
  }

  if (MOZ_UNLIKELY(StyleBorder()->mBoxDecorationBreak ==
                   StyleBoxDecorationBreak::Clone)) {
    return skip;
  }

  if (GetPrevContinuation()) {
    skip |= eLogicalSideBitsBStart;
  }

  // Always skip block-end side if we have a *later* sibling across column-span
  // split.
  if (HasColumnSpanSiblings()) {
    skip |= eLogicalSideBitsBEnd;
  }

  if (aAfterReflow) {
    nsIFrame* nif = GetNextContinuation();
    if (nif && !nif->IsTrueOverflowContainer()) {
      skip |= eLogicalSideBitsBEnd;
    }
  }

  return skip;
}
