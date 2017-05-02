/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "::backdrop" */

#include "nsBackdropFrame.h"

#include "nsDisplayList.h"

using namespace mozilla;

NS_IMPL_FRAMEARENA_HELPERS(nsBackdropFrame)

#ifdef DEBUG_FRAME_DUMP
nsresult
nsBackdropFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Backdrop"), aResult);
}
#endif

/* virtual */ nsStyleContext*
nsBackdropFrame::GetParentStyleContext(nsIFrame** aProviderFrame) const
{
  // Style context of backdrop pseudo-element does not inherit from
  // any element, per the Fullscreen API spec.
  *aProviderFrame = nullptr;
  return nullptr;
}

/* virtual */ void
nsBackdropFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                  const nsRect& aDirtyRect,
                                  const nsDisplayListSet& aLists)
{
  DO_GLOBAL_REFLOW_COUNT_DSP("nsBackdropFrame");
  // We want this frame to always be there even if its display value is
  // none or contents so that we can respond to style change on it. To
  // support those values, we skip painting ourselves in those cases.
  auto display = StyleDisplay()->mDisplay;
  if (display == mozilla::StyleDisplay::None ||
      display == mozilla::StyleDisplay::Contents) {
    return;
  }

  DisplayBorderBackgroundOutline(aBuilder, aLists);
}

/* virtual */ LogicalSize
nsBackdropFrame::ComputeAutoSize(nsRenderingContext* aRenderingContext,
                                 WritingMode         aWM,
                                 const LogicalSize&  aCBSize,
                                 nscoord             aAvailableISize,
                                 const LogicalSize&  aMargin,
                                 const LogicalSize&  aBorder,
                                 const LogicalSize&  aPadding,
                                 ComputeSizeFlags    aFlags)
{
  // Note that this frame is a child of the viewport frame.
  LogicalSize result(aWM, 0xdeadbeef, NS_UNCONSTRAINEDSIZE);
  if (aFlags & ComputeSizeFlags::eShrinkWrap) {
    result.ISize(aWM) = 0;
  } else {
    result.ISize(aWM) = aAvailableISize - aMargin.ISize(aWM) -
                        aBorder.ISize(aWM) - aPadding.ISize(aWM);
  }
  return result;
}

/* virtual */ void
nsBackdropFrame::Reflow(nsPresContext* aPresContext,
                        ReflowOutput& aDesiredSize,
                        const ReflowInput& aReflowInput,
                        nsReflowStatus& aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsBackdropFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);

  // Note that this frame is a child of the viewport frame.
  WritingMode wm = aReflowInput.GetWritingMode();
  LogicalMargin borderPadding = aReflowInput.ComputedLogicalBorderPadding();
  nscoord isize = aReflowInput.ComputedISize() + borderPadding.IStartEnd(wm);
  nscoord bsize = aReflowInput.ComputedBSize() + borderPadding.BStartEnd(wm);
  aDesiredSize.SetSize(wm, LogicalSize(wm, isize, bsize));
  aStatus.Reset();
}
