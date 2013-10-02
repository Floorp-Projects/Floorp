/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * base class for rendering objects that can be split across lines,
 * columns, or pages
 */

#include "nsSplittableFrame.h"
#include "nsContainerFrame.h"
#include "nsIFrameInlines.h"

NS_IMPL_FRAMEARENA_HELPERS(nsSplittableFrame)

void
nsSplittableFrame::Init(nsIContent*      aContent,
                        nsIFrame*        aParent,
                        nsIFrame*        aPrevInFlow)
{
  nsFrame::Init(aContent, aParent, aPrevInFlow);

  if (aPrevInFlow) {
    // Hook the frame into the flow
    SetPrevInFlow(aPrevInFlow);
    aPrevInFlow->SetNextInFlow(this);
  }
}

void
nsSplittableFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  // Disconnect from the flow list
  if (mPrevContinuation || mNextContinuation) {
    RemoveFromFlow(this);
  }

  // Let the base class destroy the frame
  nsFrame::DestroyFrom(aDestructRoot);
}

nsSplittableType
nsSplittableFrame::GetSplittableType() const
{
  return NS_FRAME_SPLITTABLE;
}

nsIFrame* nsSplittableFrame::GetPrevContinuation() const
{
  return mPrevContinuation;
}

void
nsSplittableFrame::SetPrevContinuation(nsIFrame* aFrame)
{
  NS_ASSERTION (!aFrame || GetType() == aFrame->GetType(), "setting a prev continuation with incorrect type!");
  NS_ASSERTION (!IsInPrevContinuationChain(aFrame, this), "creating a loop in continuation chain!");
  mPrevContinuation = aFrame;
  RemoveStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
}

nsIFrame* nsSplittableFrame::GetNextContinuation() const
{
  return mNextContinuation;
}

void
nsSplittableFrame::SetNextContinuation(nsIFrame* aFrame)
{
  NS_ASSERTION (!aFrame || GetType() == aFrame->GetType(),  "setting a next continuation with incorrect type!");
  NS_ASSERTION (!IsInNextContinuationChain(aFrame, this), "creating a loop in continuation chain!");
  mNextContinuation = aFrame;
  if (aFrame)
    aFrame->RemoveStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
}

nsIFrame*
nsSplittableFrame::FirstContinuation() const
{
  nsSplittableFrame* firstContinuation = const_cast<nsSplittableFrame*>(this);
  while (firstContinuation->mPrevContinuation)  {
    firstContinuation = static_cast<nsSplittableFrame*>(firstContinuation->mPrevContinuation);
  }
  MOZ_ASSERT(firstContinuation, "post-condition failed");
  return firstContinuation;
}

nsIFrame*
nsSplittableFrame::LastContinuation() const
{
  nsSplittableFrame* lastContinuation = const_cast<nsSplittableFrame*>(this);
  while (lastContinuation->mNextContinuation)  {
    lastContinuation = static_cast<nsSplittableFrame*>(lastContinuation->mNextContinuation);
  }
  MOZ_ASSERT(lastContinuation, "post-condition failed");
  return lastContinuation;
}

#ifdef DEBUG
bool nsSplittableFrame::IsInPrevContinuationChain(nsIFrame* aFrame1, nsIFrame* aFrame2)
{
  int32_t iterations = 0;
  while (aFrame1 && iterations < 10) {
    // Bail out after 10 iterations so we don't bog down debug builds too much
    if (aFrame1 == aFrame2)
      return true;
    aFrame1 = aFrame1->GetPrevContinuation();
    ++iterations;
  }
  return false;
}

bool nsSplittableFrame::IsInNextContinuationChain(nsIFrame* aFrame1, nsIFrame* aFrame2)
{
  int32_t iterations = 0;
  while (aFrame1 && iterations < 10) {
    // Bail out after 10 iterations so we don't bog down debug builds too much
    if (aFrame1 == aFrame2)
      return true;
    aFrame1 = aFrame1->GetNextContinuation();
    ++iterations;
  }
  return false;
}
#endif

nsIFrame* nsSplittableFrame::GetPrevInFlow() const
{
  return (GetStateBits() & NS_FRAME_IS_FLUID_CONTINUATION) ? mPrevContinuation : nullptr;
}

void
nsSplittableFrame::SetPrevInFlow(nsIFrame* aFrame)
{
  NS_ASSERTION (!aFrame || GetType() == aFrame->GetType(), "setting a prev in flow with incorrect type!");
  NS_ASSERTION (!IsInPrevContinuationChain(aFrame, this), "creating a loop in continuation chain!");
  mPrevContinuation = aFrame;
  AddStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
}

nsIFrame* nsSplittableFrame::GetNextInFlow() const
{
  return mNextContinuation && (mNextContinuation->GetStateBits() & NS_FRAME_IS_FLUID_CONTINUATION) ? 
    mNextContinuation : nullptr;
}

void
nsSplittableFrame::SetNextInFlow(nsIFrame* aFrame)
{
  NS_ASSERTION (!aFrame || GetType() == aFrame->GetType(),  "setting a next in flow with incorrect type!");
  NS_ASSERTION (!IsInNextContinuationChain(aFrame, this), "creating a loop in continuation chain!");
  mNextContinuation = aFrame;
  if (aFrame)
    aFrame->AddStateBits(NS_FRAME_IS_FLUID_CONTINUATION);
}

nsIFrame*
nsSplittableFrame::FirstInFlow() const
{
  nsSplittableFrame* firstInFlow = const_cast<nsSplittableFrame*>(this);
  while (nsIFrame* prev = firstInFlow->GetPrevInFlow())  {
    firstInFlow = static_cast<nsSplittableFrame*>(prev);
  }
  MOZ_ASSERT(firstInFlow, "post-condition failed");
  return firstInFlow;
}

nsIFrame*
nsSplittableFrame::LastInFlow() const
{
  nsSplittableFrame* lastInFlow = const_cast<nsSplittableFrame*>(this);
  while (nsIFrame* next = lastInFlow->GetNextInFlow())  {
    lastInFlow = static_cast<nsSplittableFrame*>(next);
  }
  MOZ_ASSERT(lastInFlow, "post-condition failed");
  return lastInFlow;
}

// Remove this frame from the flow. Connects prev in flow and next in flow
void
nsSplittableFrame::RemoveFromFlow(nsIFrame* aFrame)
{
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

  aFrame->SetPrevInFlow(nullptr);
  aFrame->SetNextInFlow(nullptr);
}

nscoord
nsSplittableFrame::GetConsumedHeight() const
{
  nscoord height = 0;

  // Reduce the height by the computed height of prev-in-flows.
  for (nsIFrame* prev = GetPrevInFlow(); prev; prev = prev->GetPrevInFlow()) {
    height += prev->GetRect().height;
  }

  return height;
}

nscoord
nsSplittableFrame::GetEffectiveComputedHeight(const nsHTMLReflowState& aReflowState,
                                              nscoord aConsumedHeight) const
{
  nscoord height = aReflowState.ComputedHeight();
  if (height == NS_INTRINSICSIZE) {
    return NS_INTRINSICSIZE;
  }

  if (aConsumedHeight == NS_INTRINSICSIZE) {
    aConsumedHeight = GetConsumedHeight();
  }

  height -= aConsumedHeight;

  if (aConsumedHeight != 0 && aConsumedHeight != NS_INTRINSICSIZE) {
    // We just subtracted our top-border padding, since it was included in the
    // first frame's height. Add it back to get the content height.
    height += aReflowState.mComputedBorderPadding.top;
  }

  // We may have stretched the frame beyond its computed height. Oh well.
  height = std::max(0, height);

  return height;
}

int
nsSplittableFrame::GetSkipSides(const nsHTMLReflowState* aReflowState) const
{
  if (IS_TRUE_OVERFLOW_CONTAINER(this)) {
    return (1 << NS_SIDE_TOP) | (1 << NS_SIDE_BOTTOM);
  }

  int skip = 0;

  if (GetPrevInFlow()) {
    skip |= 1 << NS_SIDE_TOP;
  }

  if (aReflowState) {
    // We're in the midst of reflow right now, so it's possible that we haven't
    // created a nif yet. If our content height is going to exceed our available
    // height, though, then we're going to need a next-in-flow, it just hasn't
    // been created yet.

    if (NS_UNCONSTRAINEDSIZE != aReflowState->availableHeight) {
      nscoord effectiveCH = this->GetEffectiveComputedHeight(*aReflowState);
      if (effectiveCH > aReflowState->availableHeight) {
        // Our content height is going to exceed our available height, so we're
        // going to need a next-in-flow.
        skip |= 1 << NS_SIDE_BOTTOM;
      }
    }
  } else {
    nsIFrame* nif = GetNextInFlow();
    if (nif && !IS_TRUE_OVERFLOW_CONTAINER(nif)) {
      skip |= 1 << NS_SIDE_BOTTOM;
    }
  }

 return skip;
}

#ifdef DEBUG
void
nsSplittableFrame::DumpBaseRegressionData(nsPresContext* aPresContext, FILE* out, int32_t aIndent)
{
  nsFrame::DumpBaseRegressionData(aPresContext, out, aIndent);
  if (nullptr != mNextContinuation) {
    IndentBy(out, aIndent);
    fprintf(out, "<next-continuation va=\"%p\"/>\n", (void*)mNextContinuation);
  }
  if (nullptr != mPrevContinuation) {
    IndentBy(out, aIndent);
    fprintf(out, "<prev-continuation va=\"%p\"/>\n", (void*)mPrevContinuation);
  }

}
#endif
