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
#include "nsGkAtoms.h"
#include "nsIScrollableFrame.h"
#include "nsDisplayList.h"
#include "FrameLayerBuilder.h"
#include "nsSubDocumentFrame.h"
#include "nsAbsoluteContainingBlock.h"
#include "sampler.h"

using namespace mozilla;

nsIFrame*
NS_NewViewportFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) ViewportFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(ViewportFrame)

NS_IMETHODIMP
ViewportFrame::Init(nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIFrame*        aPrevInFlow)
{
  return Super::Init(aContent, aParent, aPrevInFlow);
}

void
ViewportFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  DestroyAbsoluteFrames(aDestructRoot);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

NS_IMETHODIMP
ViewportFrame::SetInitialChildList(ChildListID     aListID,
                                   nsFrameList&    aChildList)
{
  // See which child list to add the frames to
#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aChildList);
#endif
  return nsContainerFrame::SetInitialChildList(aListID, aChildList);
}

NS_IMETHODIMP
ViewportFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists)
{
  SAMPLE_LABEL("ViewportFrame", "BuildDisplayList");
  nsIFrame* kid = mFrames.FirstChild();
  if (!kid)
    return NS_OK;

  // make the kid's BorderBackground our own. This ensures that the canvas
  // frame's background becomes our own background and therefore appears
  // below negative z-index elements.
  return BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
}

NS_IMETHODIMP
ViewportFrame::AppendFrames(ChildListID     aListID,
                            nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList ||
               aListID == GetAbsoluteListID(), "unexpected child list");
  NS_ASSERTION(aListID != GetAbsoluteListID() ||
               GetChildList(aListID).IsEmpty(), "Shouldn't have any kids!");
  return nsContainerFrame::AppendFrames(aListID, aFrameList);
}

NS_IMETHODIMP
ViewportFrame::InsertFrames(ChildListID     aListID,
                            nsIFrame*       aPrevFrame,
                            nsFrameList&    aFrameList)
{
  NS_ASSERTION(aListID == kPrincipalList ||
               aListID == GetAbsoluteListID(), "unexpected child list");
  NS_ASSERTION(aListID != GetAbsoluteListID() ||
               GetChildList(aListID).IsEmpty(), "Shouldn't have any kids!");
  return nsContainerFrame::InsertFrames(aListID, aPrevFrame, aFrameList);
}

NS_IMETHODIMP
ViewportFrame::RemoveFrame(ChildListID     aListID,
                           nsIFrame*       aOldFrame)
{
  NS_ASSERTION(aListID == kPrincipalList ||
               aListID == GetAbsoluteListID(), "unexpected child list");
  return nsContainerFrame::RemoveFrame(aListID, aOldFrame);
}

/* virtual */ nscoord
ViewportFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetMinWidth(aRenderingContext);

  return result;
}

/* virtual */ nscoord
ViewportFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);
  if (mFrames.IsEmpty())
    result = 0;
  else
    result = mFrames.FirstChild()->GetPrefWidth(aRenderingContext);

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
  nsIScrollableFrame *scrollingFrame = do_QueryFrame(kidFrame);

  if (scrollingFrame) {
    nsMargin scrollbars = scrollingFrame->GetActualScrollbarSizes();
    aReflowState->SetComputedWidth(aReflowState->ComputedWidth() -
                                   scrollbars.LeftRight());
    aReflowState->availableWidth -= scrollbars.LeftRight();
    aReflowState->SetComputedHeightWithoutResettingResizeFlags(
      aReflowState->ComputedHeight() - scrollbars.TopBottom());
    return nsPoint(scrollbars.left, scrollbars.top);
  }
  return nsPoint(0, 0);
}

NS_IMETHODIMP
ViewportFrame::Reflow(nsPresContext*           aPresContext,
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

  // Set our size up front, since some parts of reflow depend on it
  // being already set.  Note that the computed height may be
  // unconstrained; that's ok.  Consumers should watch out for that.
  SetSize(nsSize(aReflowState.ComputedWidth(), aReflowState.ComputedHeight()));
 
  // Reflow the main content first so that the placeholders of the
  // fixed-position frames will be in the right places on an initial
  // reflow.
  nscoord kidHeight = 0;

  nsresult rv = NS_OK;
  
  if (mFrames.NotEmpty()) {
    // Deal with a non-incremental reflow or an incremental reflow
    // targeted at our one-and-only principal child frame.
    if (aReflowState.ShouldReflowAllKids() ||
        aReflowState.mFlags.mVResize ||
        NS_SUBTREE_DIRTY(mFrames.FirstChild())) {
      // Reflow our one-and-only principal child frame
      nsIFrame*           kidFrame = mFrames.FirstChild();
      nsHTMLReflowMetrics kidDesiredSize;
      nsSize              availableSpace(aReflowState.availableWidth,
                                         aReflowState.availableHeight);
      nsHTMLReflowState   kidReflowState(aPresContext, aReflowState,
                                         kidFrame, availableSpace);

      // Reflow the frame
      kidReflowState.SetComputedHeight(aReflowState.ComputedHeight());
      rv = ReflowChild(kidFrame, aPresContext, kidDesiredSize, kidReflowState,
                       0, 0, 0, aStatus);
      kidHeight = kidDesiredSize.height;

      FinishReflowChild(kidFrame, aPresContext, nsnull, kidDesiredSize, 0, 0, 0);
    } else {
      kidHeight = mFrames.FirstChild()->GetSize().height;
    }
  }

  NS_ASSERTION(aReflowState.availableWidth != NS_UNCONSTRAINEDSIZE,
               "shouldn't happen anymore");

  // Return the max size as our desired size
  aDesiredSize.width = aReflowState.availableWidth;
  // Being flowed initially at an unconstrained height means we should
  // return our child's intrinsic size.
  aDesiredSize.height = aReflowState.ComputedHeight() != NS_UNCONSTRAINEDSIZE
                          ? aReflowState.ComputedHeight()
                          : kidHeight;
  aDesiredSize.SetOverflowAreasToDesiredBounds();

  if (mFrames.NotEmpty()) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, mFrames.FirstChild());
  }

  // Make a copy of the reflow state and change the computed width and height
  // to reflect the available space for the fixed items
  nsHTMLReflowState reflowState(aReflowState);
  nsPoint offset = AdjustReflowStateForScrollbars(&reflowState);

  if (IsAbsoluteContainer()) {
    NS_ASSERTION(GetAbsoluteContainingBlock()->GetChildList().IsEmpty() ||
                 (offset.x == 0 && offset.y == 0),
                 "We don't handle correct positioning of fixed frames with "
                 "scrollbars in odd positions");

    // Just reflow all the fixed-pos frames.
    rv = GetAbsoluteContainingBlock()->Reflow(this, aPresContext, reflowState, aStatus,
                                              reflowState.ComputedWidth(),
                                              reflowState.ComputedHeight(),
                                              false, true, true, // XXX could be optimized
                                              &aDesiredSize.mOverflowAreas);
  }

  // If we were dirty then do a repaint
  if (GetStateBits() & NS_FRAME_IS_DIRTY) {
    nsRect damageRect(0, 0, aDesiredSize.width, aDesiredSize.height);
    Invalidate(damageRect);
  }

  // Clipping is handled by the document container (e.g., nsSubDocumentFrame),
  // so we don't need to change our overflow areas.
  bool overflowChanged = FinishAndStoreOverflow(&aDesiredSize);
  if (overflowChanged) {
    // We may need to alert our container to get it to pick up the
    // overflow change.
    nsSubDocumentFrame* container = static_cast<nsSubDocumentFrame*>
      (nsLayoutUtils::GetCrossDocParentFrame(this));
    if (container && !container->ShouldClipSubdocument()) {
      container->PresContext()->PresShell()->
        FrameNeedsReflow(container, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
    }
  }

  NS_FRAME_TRACE_REFLOW_OUT("ViewportFrame::Reflow", aStatus);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv; 
}

nsIAtom*
ViewportFrame::GetType() const
{
  return nsGkAtoms::viewportFrame;
}

void
ViewportFrame::InvalidateInternal(const nsRect& aDamageRect,
                                  nscoord aX, nscoord aY, nsIFrame* aForChild,
                                  PRUint32 aFlags)
{
  nsRect r = aDamageRect + nsPoint(aX, aY);
  nsPresContext* presContext = PresContext();
  presContext->NotifyInvalidation(r, aFlags);

  if ((mState & NS_FRAME_HAS_CONTAINER_LAYER) &&
      !(aFlags & INVALIDATE_NO_THEBES_LAYERS)) {
    FrameLayerBuilder::InvalidateThebesLayerContents(this, r);
    // Don't need to invalidate any more Thebes layers
    aFlags |= INVALIDATE_NO_THEBES_LAYERS;
    if (aFlags & INVALIDATE_ONLY_THEBES_LAYERS) {
      return;
    }
  }

  nsIFrame* parent = nsLayoutUtils::GetCrossDocParentFrame(this);
  if (parent) {
    if (!presContext->PresShell()->IsActive())
      return;
    nsPoint pt = -parent->GetOffsetToCrossDoc(this);
    PRInt32 ourAPD = presContext->AppUnitsPerDevPixel();
    PRInt32 parentAPD = parent->PresContext()->AppUnitsPerDevPixel();
    r = r.ConvertAppUnitsRoundOut(ourAPD, parentAPD);
    parent->InvalidateInternal(r, pt.x, pt.y, this,
                               aFlags | INVALIDATE_CROSS_DOC);
    return;
  }
  InvalidateRoot(r, aFlags);
}

#ifdef DEBUG
NS_IMETHODIMP
ViewportFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Viewport"), aResult);
}
#endif
