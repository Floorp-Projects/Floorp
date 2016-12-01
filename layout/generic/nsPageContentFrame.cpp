/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsPageContentFrame.h"
#include "nsCSSFrameConstructor.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsSimplePageSequenceFrame.h"

using namespace mozilla;

nsPageContentFrame*
NS_NewPageContentFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsPageContentFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsPageContentFrame)

void
nsPageContentFrame::Reflow(nsPresContext*           aPresContext,
                           ReflowOutput&     aDesiredSize,
                           const ReflowInput& aReflowInput,
                           nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsPageContentFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  aStatus = NS_FRAME_COMPLETE;  // initialize out parameter

  if (GetPrevInFlow() && (GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    nsresult rv = aPresContext->PresShell()->FrameConstructor()
                    ->ReplicateFixedFrames(this);
    if (NS_FAILED(rv)) {
      return;
    }
  }

  // Set our size up front, since some parts of reflow depend on it
  // being already set.  Note that the computed height may be
  // unconstrained; that's ok.  Consumers should watch out for that.
  nsSize  maxSize(aReflowInput.ComputedWidth(),
                  aReflowInput.ComputedHeight());
  SetSize(maxSize);
 
  // A PageContentFrame must always have one child: the canvas frame.
  // Resize our frame allowing it only to be as big as we are
  // XXX Pay attention to the page's border and padding...
  if (mFrames.NotEmpty()) {
    nsIFrame* frame = mFrames.FirstChild();
    WritingMode wm = frame->GetWritingMode();
    LogicalSize logicalSize(wm, maxSize);
    ReflowInput kidReflowInput(aPresContext, aReflowInput,
                                     frame, logicalSize);
    kidReflowInput.SetComputedBSize(logicalSize.BSize(wm));

    // Reflow the page content area
    ReflowChild(frame, aPresContext, aDesiredSize, kidReflowInput, 0, 0, 0, aStatus);

    // The document element's background should cover the entire canvas, so
    // take into account the combined area and any space taken up by
    // absolutely positioned elements
    nsMargin padding(0,0,0,0);

    // XXXbz this screws up percentage padding (sets padding to zero
    // in the percentage padding case)
    kidReflowInput.mStylePadding->GetPadding(padding);

    // This is for shrink-to-fit, and therefore we want to use the
    // scrollable overflow, since the purpose of shrink to fit is to
    // make the content that ought to be reachable (represented by the
    // scrollable overflow) fit in the page.
    if (frame->HasOverflowAreas()) {
      // The background covers the content area and padding area, so check
      // for children sticking outside the child frame's padding edge
      nscoord xmost = aDesiredSize.ScrollableOverflow().XMost();
      if (xmost > aDesiredSize.Width()) {
        nscoord widthToFit = xmost + padding.right +
          kidReflowInput.mStyleBorder->GetComputedBorderWidth(eSideRight);
        float ratio = float(maxSize.width) / widthToFit;
        NS_ASSERTION(ratio >= 0.0 && ratio < 1.0, "invalid shrink-to-fit ratio");
        mPD->mShrinkToFitRatio = std::min(mPD->mShrinkToFitRatio, ratio);
      }
    }

    // Place and size the child
    FinishReflowChild(frame, aPresContext, aDesiredSize, &kidReflowInput, 0, 0, 0);

    NS_ASSERTION(aPresContext->IsDynamic() || !NS_FRAME_IS_FULLY_COMPLETE(aStatus) ||
                  !frame->GetNextInFlow(), "bad child flow list");
  }

  // Reflow our fixed frames
  nsReflowStatus fixedStatus = NS_FRAME_COMPLETE;
  ReflowAbsoluteFrames(aPresContext, aDesiredSize, aReflowInput, fixedStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(fixedStatus), "fixed frames can be truncated, but not incomplete");

  // Return our desired size
  WritingMode wm = aReflowInput.GetWritingMode();
  aDesiredSize.ISize(wm) = aReflowInput.ComputedISize();
  if (aReflowInput.ComputedBSize() != NS_UNCONSTRAINEDSIZE) {
    aDesiredSize.BSize(wm) = aReflowInput.ComputedBSize();
  }
  FinishAndStoreOverflow(&aDesiredSize);

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

nsIAtom*
nsPageContentFrame::GetType() const
{
  return nsGkAtoms::pageContentFrame; 
}

#ifdef DEBUG_FRAME_DUMP
nsresult
nsPageContentFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("PageContent"), aResult);
}
#endif
