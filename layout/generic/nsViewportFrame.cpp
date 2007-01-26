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

/*
 * rendering object that is the root of the frame tree, which contains
 * the document's scrollbars and contains fixed-positioned elements
 */

#include "nsCOMPtr.h"
#include "nsViewportFrame.h"
#include "nsHTMLParts.h"
#include "nsLayoutAtoms.h"
#include "nsIViewManager.h"
#include "nsIScrollableFrame.h"
#include "nsIDeviceContext.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsDisplayList.h"

nsIFrame*
NS_NewViewportFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) ViewportFrame(aContext);
}

NS_IMETHODIMP
ViewportFrame::Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        aPrevInFlow)
{
  return Super::Init(aContent, aParent, aPrevInFlow);
}

void
ViewportFrame::Destroy()
{
  mFixedContainer.DestroyFrames(this);
  nsContainerFrame::Destroy();
}

NS_IMETHODIMP
ViewportFrame::SetInitialChildList(nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  nsresult rv = NS_OK;

  // See which child list to add the frames to
#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aChildList);
#endif
  if (mFixedContainer.GetChildListName() == aListName) {
    rv = mFixedContainer.SetInitialChildList(this, aListName, aChildList);
  } 
  else {
    rv = nsContainerFrame::SetInitialChildList(aListName, aChildList);
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists)
{
  // We don't need any special painting or event handling. We just need to
  // mark our visible out-of-flow frames (i.e., the fixed position frames) so
  // that display list construction is guaranteed to recurse into their
  // ancestors.
  aBuilder->MarkFramesForDisplayList(this, mFixedContainer.GetFirstChild(), aDirtyRect);

  nsIFrame* kid = mFrames.FirstChild();
  if (!kid)
    return NS_OK;

  // make the kid's BorderBackground our own. This ensures that the canvas
  // frame's background becomes our own background and therefore appears
  // below negative z-index elements.
  return BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
}

NS_IMETHODIMP
ViewportFrame::AppendFrames(nsIAtom*        aListName,
                            nsIFrame*       aFrameList)
{
  nsresult rv = NS_OK;

  if (mFixedContainer.GetChildListName() == aListName) {
    rv = mFixedContainer.AppendFrames(this, aListName, aFrameList);
  }
  else {
    // We only expect incremental changes for our fixed frames
    NS_ERROR("unexpected child list");
    rv = NS_ERROR_INVALID_ARG;
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::InsertFrames(nsIAtom*        aListName,
                            nsIFrame*       aPrevFrame,
                            nsIFrame*       aFrameList)
{
  nsresult rv = NS_OK;

  if (mFixedContainer.GetChildListName() == aListName) {
    rv = mFixedContainer.InsertFrames(this, aListName, aPrevFrame, aFrameList);
  }
  else {
    // We only expect incremental changes for our fixed frames
    NS_ERROR("unexpected child list");
    rv = NS_ERROR_INVALID_ARG;
  }

  return rv;
}

NS_IMETHODIMP
ViewportFrame::RemoveFrame(nsIAtom*        aListName,
                           nsIFrame*       aOldFrame)
{
  nsresult rv = NS_OK;

  if (mFixedContainer.GetChildListName() == aListName) {
    rv = mFixedContainer.RemoveFrame(this, aListName, aOldFrame);
  }
  else {
    // We only expect incremental changes for our fixed frames
    NS_ERROR("unexpected child list");
    rv = NS_ERROR_INVALID_ARG;
  }

  return rv;
}

nsIAtom*
ViewportFrame::GetAdditionalChildListName(PRInt32 aIndex) const
{
  NS_PRECONDITION(aIndex >= 0, "illegal index");

  if (0 == aIndex) {
    return mFixedContainer.GetChildListName();
  }

  return nsnull;
}

nsIFrame*
ViewportFrame::GetFirstChild(nsIAtom* aListName) const
{
  if (mFixedContainer.GetChildListName() == aListName) {
    nsIFrame* result = nsnull;
    mFixedContainer.FirstChild(this, aListName, &result);
    return result;
  }

  return nsContainerFrame::GetFirstChild(aListName);
}

/* virtual */ nscoord
ViewportFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetMinWidth(aRenderingContext);
    
  // XXXldb Deal with mFixedContainer (matters for SizeToContent)!

  return result;
}

/* virtual */ nscoord
ViewportFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetPrefWidth(aRenderingContext);
    
  // XXXldb Deal with mFixedContainer (matters for SizeToContent)!

  return result;
}

nsPoint
 ViewportFrame::AdjustReflowStateForScrollbars(nsHTMLReflowState* aReflowState) const
{
  // Calculate how much room is available for fixed frames. That means
  // determining if the viewport is scrollable and whether the vertical and/or
  // horizontal scrollbars are visible

  // Get our prinicpal child frame and see if we're scrollable
  nsIFrame* kidFrame = mFrames.FirstChild();
  nsCOMPtr<nsIScrollableFrame> scrollingFrame(do_QueryInterface(kidFrame));

  if (scrollingFrame) {
    nsMargin scrollbars = scrollingFrame->GetActualScrollbarSizes();
    aReflowState->SetComputedWidth(aReflowState->ComputedWidth() -
                                   (scrollbars.left + scrollbars.right));
    aReflowState->availableWidth -= scrollbars.left + scrollbars.right;
    aReflowState->mComputedHeight -= scrollbars.top + scrollbars.bottom;
    // XXX why don't we also adjust "aReflowState->availableHeight"?
    return nsPoint(scrollbars.left, scrollbars.top);
  }
  return nsPoint(0, 0);
}

NS_IMETHODIMP
ViewportFrame::Reflow(nsPresContext*          aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("ViewportFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  NS_FRAME_TRACE_REFLOW_IN("ViewportFrame::Reflow");

  // Initialize OUT parameters
  aStatus = NS_FRAME_COMPLETE;

  // Because |Reflow| sets mComputedHeight on the child to
  // availableHeight.
  AddStateBits(NS_FRAME_CONTAINS_RELATIVE_HEIGHT);
  
  // Reflow the main content first so that the placeholders of the
  // fixed-position frames will be in the right places on an initial
  // reflow.
  nsRect kidRect(0,0,aReflowState.availableWidth,aReflowState.availableHeight);

  nsresult rv = NS_OK;
  
  if (mFrames.NotEmpty()) {
    // Deal with a non-incremental reflow or an incremental reflow
    // targeted at our one-and-only principal child frame.
    if (aReflowState.ShouldReflowAllKids() ||
        aReflowState.mFlags.mVResize ||
        (mFrames.FirstChild()->GetStateBits() &
         (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN))) {
      // Reflow our one-and-only principal child frame
      nsIFrame*           kidFrame = mFrames.FirstChild();
      nsHTMLReflowMetrics kidDesiredSize;
      nsSize              availableSpace(aReflowState.availableWidth,
                                         aReflowState.availableHeight);
      nsHTMLReflowState   kidReflowState(aPresContext, aReflowState,
                                         kidFrame, availableSpace);

      // Reflow the frame
      kidReflowState.mComputedHeight = aReflowState.availableHeight;
      rv = ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                       0, 0, 0, aStatus);
      kidRect.width = kidDesiredSize.width;
      kidRect.height = kidDesiredSize.height;

      FinishReflowChild(kidFrame, aPresContext, nsnull, kidDesiredSize, 0, 0, 0);
    }
  }

  NS_ASSERTION(aReflowState.availableWidth != NS_UNCONSTRAINEDSIZE,
               "shouldn't happen anymore");

  // Return the max size as our desired size
  aDesiredSize.width = aReflowState.availableWidth;
  // Being flowed initially at an unconstrained height means we should
  // return our child's intrinsic size.
  aDesiredSize.height = aReflowState.availableHeight != NS_UNCONSTRAINEDSIZE
                          ? aReflowState.availableHeight
                          : kidRect.height;

  // Make a copy of the reflow state and change the computed width and height
  // to reflect the available space for the fixed items
  nsHTMLReflowState reflowState(aReflowState);
  nsPoint offset = AdjustReflowStateForScrollbars(&reflowState);
  
#ifdef DEBUG
  nsIFrame* f;
  mFixedContainer.FirstChild(this, nsGkAtoms::fixedList, &f);
  NS_ASSERTION(!f || (offset.x == 0 && offset.y == 0),
               "We don't handle correct positioning of fixed frames with "
               "scrollbars in odd positions");
#endif

  // Just reflow all the fixed-pos frames.
  rv = mFixedContainer.Reflow(this, aPresContext, reflowState,
                              reflowState.ComputedWidth(), 
                              reflowState.mComputedHeight,
                              PR_TRUE, PR_TRUE); // XXX could be optimized

  // If we were dirty then do a repaint
  if (GetStateBits() & NS_FRAME_IS_DIRTY) {
    nsRect damageRect(0, 0, aDesiredSize.width, aDesiredSize.height);
    Invalidate(damageRect, PR_FALSE);
  }

  // XXX Should we do something to clip our children to this?
  aDesiredSize.mOverflowArea =
    nsRect(nsPoint(0, 0), nsSize(aDesiredSize.width, aDesiredSize.height));

  NS_FRAME_TRACE_REFLOW_OUT("ViewportFrame::Reflow", aStatus);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv; 
}

nsIAtom*
ViewportFrame::GetType() const
{
  return nsGkAtoms::viewportFrame;
}

/* virtual */ PRBool
ViewportFrame::IsContainingBlock() const
{
  return PR_TRUE;
}

void
ViewportFrame::InvalidateInternal(const nsRect& aDamageRect,
                                  nscoord aX, nscoord aY, nsIFrame* aForChild,
                                  PRBool aImmediate)
{
  nsIFrame* parent = nsLayoutUtils::GetCrossDocParentFrame(this);
  if (parent) {
    nsPoint pt = GetOffsetTo(parent);
    parent->InvalidateInternal(aDamageRect, aX + pt.x, aY + pt.y, this, aImmediate);
    return;
  }
  InvalidateRoot(aDamageRect, aX, aY, aImmediate);
}

#ifdef DEBUG
NS_IMETHODIMP
ViewportFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Viewport"), aResult);
}
#endif
