/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsPageContentFrame.h"
#include "nsPageFrame.h"
#include "nsPlaceholderFrame.h"
#include "nsCSSFrameConstructor.h"
#include "nsHTMLContainerFrame.h"
#include "nsHTMLParts.h"
#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsReadableUtils.h"
#include "nsSimplePageSequence.h"
#include "nsDisplayList.h"

nsIFrame*
NS_NewPageContentFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsPageContentFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsPageContentFrame)

/* virtual */ nsSize
nsPageContentFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                                nsSize aCBSize, nscoord aAvailableWidth,
                                nsSize aMargin, nsSize aBorder, nsSize aPadding,
                                bool aShrinkWrap)
{
  NS_ASSERTION(mPD, "Pages are supposed to have page data");
  nscoord height = (!mPD || mPD->mReflowSize.height == NS_UNCONSTRAINEDSIZE)
                   ? NS_UNCONSTRAINEDSIZE
                   : (mPD->mReflowSize.height - mPD->mReflowMargin.TopBottom());
  return nsSize(aAvailableWidth, height);
}

NS_IMETHODIMP
nsPageContentFrame::Reflow(nsPresContext*           aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsPageContentFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  aStatus = NS_FRAME_COMPLETE;  // initialize out parameter
  nsresult rv = NS_OK;

  if (GetPrevInFlow() && (GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    nsresult rv = aPresContext->PresShell()->FrameConstructor()
                    ->ReplicateFixedFrames(this);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Set our size up front, since some parts of reflow depend on it
  // being already set.  Note that the computed height may be
  // unconstrained; that's ok.  Consumers should watch out for that.
  SetSize(nsSize(aReflowState.availableWidth, aReflowState.availableHeight));
 
  // A PageContentFrame must always have one child: the canvas frame.
  // Resize our frame allowing it only to be as big as we are
  // XXX Pay attention to the page's border and padding...
  if (mFrames.NotEmpty()) {
    nsIFrame* frame = mFrames.FirstChild();
    nsSize  maxSize(aReflowState.availableWidth, aReflowState.availableHeight);
    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, frame, maxSize);
    kidReflowState.SetComputedHeight(aReflowState.availableHeight);

    mPD->mPageContentSize  = aReflowState.availableWidth;

    // Reflow the page content area
    rv = ReflowChild(frame, aPresContext, aDesiredSize, kidReflowState, 0, 0, 0, aStatus);
    NS_ENSURE_SUCCESS(rv, rv);

    // The document element's background should cover the entire canvas, so
    // take into account the combined area and any space taken up by
    // absolutely positioned elements
    nsMargin padding(0,0,0,0);

    // XXXbz this screws up percentage padding (sets padding to zero
    // in the percentage padding case)
    kidReflowState.mStylePadding->GetPadding(padding);

    // This is for shrink-to-fit, and therefore we want to use the
    // scrollable overflow, since the purpose of shrink to fit is to
    // make the content that ought to be reachable (represented by the
    // scrollable overflow) fit in the page.
    if (frame->HasOverflowAreas()) {
      // The background covers the content area and padding area, so check
      // for children sticking outside the child frame's padding edge
      nscoord xmost = aDesiredSize.ScrollableOverflow().XMost();
      if (xmost > aDesiredSize.width) {
        mPD->mPageContentXMost =
          xmost +
          kidReflowState.mStyleBorder->GetActualBorderWidth(NS_SIDE_RIGHT) +
          padding.right;
      }
    }

    // Place and size the child
    FinishReflowChild(frame, aPresContext, &kidReflowState, aDesiredSize, 0, 0, 0);

    NS_ASSERTION(aPresContext->IsDynamic() || !NS_FRAME_IS_FULLY_COMPLETE(aStatus) ||
                  !frame->GetNextInFlow(), "bad child flow list");
  }
  // Reflow our fixed frames 
  nsReflowStatus fixedStatus = NS_FRAME_COMPLETE;
  mFixedContainer.Reflow(this, aPresContext, aReflowState, fixedStatus,
                         aReflowState.availableWidth,
                         aReflowState.availableHeight,
                         PR_FALSE, PR_TRUE, PR_TRUE, // XXX could be optimized
                         nsnull /* ignore overflow */);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(fixedStatus), "fixed frames can be truncated, but not incomplete");

  // Return our desired size
  aDesiredSize.width = aReflowState.availableWidth;
  if (aReflowState.availableHeight != NS_UNCONSTRAINEDSIZE) {
    aDesiredSize.height = aReflowState.availableHeight;
  }

  FinishAndStoreOverflow(&aDesiredSize);

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

nsIAtom*
nsPageContentFrame::GetType() const
{
  return nsGkAtoms::pageContentFrame; 
}

#ifdef DEBUG
NS_IMETHODIMP
nsPageContentFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("PageContent"), aResult);
}
#endif

/* virtual */ bool
nsPageContentFrame::IsContainingBlock() const
{
  return PR_TRUE;
}
