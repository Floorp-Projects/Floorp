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
#include "nsIRenderingContext.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsIDeviceContext.h"
#include "nsReadableUtils.h"
#include "nsSimplePageSequence.h"
#include "nsDisplayList.h"

nsIFrame*
NS_NewPageContentFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsPageContentFrame(aContext);
}

/* virtual */ nsSize
nsPageContentFrame::ComputeSize(nsIRenderingContext *aRenderingContext,
                                nsSize aCBSize, nscoord aAvailableWidth,
                                nsSize aMargin, nsSize aBorder, nsSize aPadding,
                                PRBool aShrinkWrap)
{
  NS_ASSERTION(mPD, "Pages are supposed to have page data");
  nscoord height = (!mPD || mPD->mReflowSize.height == NS_UNCONSTRAINEDSIZE)
                   ? NS_UNCONSTRAINEDSIZE
                   : (mPD->mReflowSize.height - mPD->mReflowMargin.TopBottom());
  return nsSize(aAvailableWidth, height);
}

/**
 * Returns true if aFrame is a placeholder for one of our fixed frames.
 */
inline PRBool
nsPageContentFrame::IsFixedPlaceholder(nsIFrame* aFrame)
{
  if (!aFrame || nsGkAtoms::placeholderFrame != aFrame->GetType())
    return PR_FALSE;

  return static_cast<nsPlaceholderFrame*>(aFrame)->GetOutOfFlowFrame()
           ->GetParent() == this;
}

/**
 * Steals replicated fixed placeholder frames from aDocRoot so they don't
 * get in the way of reflow.
 */
inline nsFrameList
nsPageContentFrame::StealFixedPlaceholders(nsIFrame* aDocRoot)
{
  nsPresContext* presContext = PresContext();
  nsFrameList list;
  if (GetPrevInFlow()) {
    for (nsIFrame* f = aDocRoot->GetFirstChild(nsnull);
        IsFixedPlaceholder(f); f = aDocRoot->GetFirstChild(nsnull)) {
      nsresult rv = static_cast<nsContainerFrame*>(aDocRoot)
                      ->StealFrame(presContext, f);
      NS_ENSURE_SUCCESS(rv, list);
      list.AppendFrame(nsnull, f);
    }
  }
  return list;
}

/**
 * Restores stolen replicated fixed placeholder frames to aDocRoot.
 */
static inline nsresult
ReplaceFixedPlaceholders(nsIFrame*    aDocRoot,
                         nsFrameList& aPlaceholderList)
{
  nsresult rv = NS_OK;
  if (aPlaceholderList.NotEmpty()) {
    rv = static_cast<nsContainerFrame*>(aDocRoot)
           ->AddFrames(aPlaceholderList.FirstChild(), nsnull);
  }
  return rv;
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

  // A PageContentFrame must always have one child: the doc root element's frame.
  // We only need to get overflow frames if we don't already have that child;
  // Also we need to avoid repeating the call to ReplicateFixedFrames.
  nsPageContentFrame* prevPageContentFrame = static_cast<nsPageContentFrame*>
                                               (GetPrevInFlow());
  if (mFrames.IsEmpty() && prevPageContentFrame) {
    // Pull the doc root frame's continuation and copy fixed frames.
    nsIFrame* overflow = prevPageContentFrame->GetOverflowFrames(aPresContext, PR_TRUE);
    NS_ASSERTION(overflow && !overflow->GetNextSibling(),
                 "must have doc root as pageContentFrame's only child");
    nsHTMLContainerFrame::ReparentFrameView(aPresContext, overflow, prevPageContentFrame, this);
    // Prepend overflow to the page content frame. There may already be
    // children placeholders which don't get reflowed but must not be
    // lost until the page content frame is destroyed.
    mFrames.InsertFrames(this, nsnull, overflow);
    nsresult rv = aPresContext->PresShell()->FrameConstructor()
                    ->ReplicateFixedFrames(this);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Resize our frame allowing it only to be as big as we are
  // XXX Pay attention to the page's border and padding...
  if (mFrames.NotEmpty()) {
    nsIFrame* frame = mFrames.FirstChild();
    nsSize  maxSize(aReflowState.availableWidth, aReflowState.availableHeight);
    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, frame, maxSize);

    mPD->mPageContentSize  = aReflowState.availableWidth;

    // Get replicated fixed frames' placeholders out of the way
    nsFrameList stolenPlaceholders = StealFixedPlaceholders(frame);

    // Reflow the page content area
    rv = ReflowChild(frame, aPresContext, aDesiredSize, kidReflowState, 0, 0, 0, aStatus);
    NS_ENSURE_SUCCESS(rv, rv);

    // Put removed fixed placeholders back
    rv = ReplaceFixedPlaceholders(frame, stolenPlaceholders);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!NS_FRAME_IS_FULLY_COMPLETE(aStatus)) {
      nsIFrame* nextFrame = frame->GetNextInFlow();
      NS_ASSERTION(nextFrame || aStatus & NS_FRAME_REFLOW_NEXTINFLOW,
        "If it's incomplete and has no nif yet, it must flag a nif reflow.");
      if (!nextFrame) {
        nsresult rv = nsHTMLContainerFrame::CreateNextInFlow(aPresContext,
                                              this, frame, nextFrame);
        NS_ENSURE_SUCCESS(rv, rv);
        frame->SetNextSibling(nextFrame->GetNextSibling());
        nextFrame->SetNextSibling(nsnull);
        SetOverflowFrames(aPresContext, nextFrame);
        // Root overflow containers will be normal children of
        // the pageContentFrame, but that's ok because there
        // aren't any other frames we need to isolate them from
        // during reflow.
      }
      if (NS_FRAME_OVERFLOW_IS_INCOMPLETE(aStatus)) {
        nextFrame->AddStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER);
      }
    }

    // The document element's background should cover the entire canvas, so
    // take into account the combined area and any space taken up by
    // absolutely positioned elements
    nsMargin padding(0,0,0,0);

    // XXXbz this screws up percentage padding (sets padding to zero
    // in the percentage padding case)
    kidReflowState.mStylePadding->GetPadding(padding);

    // First check the combined area
    if (NS_FRAME_OUTSIDE_CHILDREN & frame->GetStateBits()) {
      // The background covers the content area and padding area, so check
      // for children sticking outside the child frame's padding edge
      if (aDesiredSize.mOverflowArea.XMost() > aDesiredSize.width) {
        mPD->mPageContentXMost =
          aDesiredSize.mOverflowArea.XMost() +
          kidReflowState.mStyleBorder->GetBorderWidth(NS_SIDE_RIGHT) +
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
                         PR_FALSE, PR_TRUE, PR_TRUE); // XXX could be optimized
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(fixedStatus), "fixed frames can be truncated, but not incomplete");

  // Return our desired size
  aDesiredSize.width = aReflowState.availableWidth;
  if (aReflowState.availableHeight != NS_UNCONSTRAINEDSIZE) {
    aDesiredSize.height = aReflowState.availableHeight;
  }

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

/* virtual */ PRBool
nsPageContentFrame::IsContainingBlock() const
{
  return PR_TRUE;
}
