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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsTableOuterFrame.h"
#include "nsTableFrame.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "prinrval.h"
#include "nsGkAtoms.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#ifdef ACCESSIBILITY
#include "nsAccessibilityService.h"
#endif
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsDisplayList.h"
#include "nsLayoutUtils.h"

/* ----------- nsTableCaptionFrame ---------- */

#define NS_TABLE_FRAME_CAPTION_LIST_INDEX 0
#define NO_SIDE 100

// caption frame
nsTableCaptionFrame::nsTableCaptionFrame(nsStyleContext* aContext):
  nsBlockFrame(aContext)
{
  // shrink wrap 
  SetFlags(NS_BLOCK_FLOAT_MGR);
}

nsTableCaptionFrame::~nsTableCaptionFrame()
{
}

nsIAtom*
nsTableCaptionFrame::GetType() const
{
  return nsGkAtoms::tableCaptionFrame;
}

/* virtual */ nscoord
nsTableOuterFrame::GetBaseline() const
{
  nsIFrame* kid = mFrames.FirstChild();
  if (!kid) {
    NS_NOTREACHED("no inner table");
    return nsHTMLContainerFrame::GetBaseline();
  }

  return kid->GetBaseline() + kid->GetPosition().y;
}

/* virtual */ nsSize
nsTableCaptionFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                     nsSize aCBSize, nscoord aAvailableWidth,
                                     nsSize aMargin, nsSize aBorder,
                                     nsSize aPadding, bool aShrinkWrap)
{
  nsSize result = nsBlockFrame::ComputeAutoSize(aRenderingContext, aCBSize,
                    aAvailableWidth, aMargin, aBorder, aPadding, aShrinkWrap);
  PRUint8 captionSide = GetStyleTableBorder()->mCaptionSide;
  if (captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
      captionSide == NS_STYLE_CAPTION_SIDE_RIGHT) {
    result.width = GetMinWidth(aRenderingContext);
  } else if (captionSide == NS_STYLE_CAPTION_SIDE_TOP ||
             captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM) {
    // The outer frame constrains our available width to the width of
    // the table.  Grow if our min-width is bigger than that, but not
    // larger than the containing block width.  (It would really be nice
    // to transmit that information another way, so we could grow up to
    // the table's available width, but that's harder.)
    nscoord min = GetMinWidth(aRenderingContext);
    if (min > aCBSize.width)
      min = aCBSize.width;
    if (min > result.width)
      result.width = min;
  }
  return result;
}

nsIFrame*
nsTableCaptionFrame::GetParentStyleContextFrame()
{
  NS_PRECONDITION(mContent->GetParent(),
                  "How could we not have a parent here?");
    
  // The caption's style context parent is the inner frame, unless
  // it's anonymous.
  nsIFrame* outerFrame = GetParent();
  if (outerFrame && outerFrame->GetType() == nsGkAtoms::tableOuterFrame) {
    nsIFrame* innerFrame = outerFrame->GetFirstPrincipalChild();
    if (innerFrame) {
      return nsFrame::CorrectStyleParentFrame(innerFrame,
                                              GetStyleContext()->GetPseudo());
    }
  }

  NS_NOTREACHED("Where is our inner table frame?");
  return nsBlockFrame::GetParentStyleContextFrame();
}

#ifdef ACCESSIBILITY
already_AddRefed<nsAccessible>
nsTableCaptionFrame::CreateAccessible()
{
  if (!GetRect().IsEmpty()) {
    nsAccessibilityService* accService = nsIPresShell::AccService();
    if (accService) {
      return accService->CreateHTMLCaptionAccessible(mContent,
                                                     PresContext()->PresShell());
    }
  }

  return nsnull;
}
#endif

#ifdef NS_DEBUG
NS_IMETHODIMP
nsTableCaptionFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Caption"), aResult);
}
#endif

nsIFrame* 
NS_NewTableCaptionFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTableCaptionFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTableCaptionFrame)

/* ----------- nsTableOuterFrame ---------- */

nsTableOuterFrame::nsTableOuterFrame(nsStyleContext* aContext):
  nsHTMLContainerFrame(aContext)
{
}

nsTableOuterFrame::~nsTableOuterFrame()
{
}

NS_QUERYFRAME_HEAD(nsTableOuterFrame)
  NS_QUERYFRAME_ENTRY(nsITableLayout)
NS_QUERYFRAME_TAIL_INHERITING(nsHTMLContainerFrame)

#ifdef ACCESSIBILITY
already_AddRefed<nsAccessible>
nsTableOuterFrame::CreateAccessible()
{
  nsAccessibilityService* accService = nsIPresShell::AccService();
  if (accService) {
    return accService->CreateHTMLTableAccessible(mContent,
                                                 PresContext()->PresShell());
  }

  return nsnull;
}
#endif

/* virtual */ bool
nsTableOuterFrame::IsContainingBlock() const
{
  return PR_FALSE;
}

void
nsTableOuterFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  mCaptionFrames.DestroyFramesFrom(aDestructRoot);
  nsHTMLContainerFrame::DestroyFrom(aDestructRoot);
}

nsFrameList
nsTableOuterFrame::GetChildList(ChildListID aListID) const
{
  switch (aListID) {
    case kPrincipalList:
      return mFrames;
    case kCaptionList:
      return mCaptionFrames;
    default:
      return nsFrameList::EmptyList();
  }
}

void
nsTableOuterFrame::GetChildLists(nsTArray<ChildList>* aLists) const
{
  mFrames.AppendIfNonempty(aLists, kPrincipalList);
  mCaptionFrames.AppendIfNonempty(aLists, kCaptionList);
}

NS_IMETHODIMP 
nsTableOuterFrame::SetInitialChildList(ChildListID     aListID,
                                       nsFrameList&    aChildList)
{
  if (kCaptionList == aListID) {
    // the frame constructor already checked for table-caption display type
    mCaptionFrames.SetFrames(aChildList);
  }
  else {
    NS_ASSERTION(aListID == kPrincipalList, "wrong childlist");
    NS_ASSERTION(mFrames.IsEmpty(), "Frame leak!");
    NS_ASSERTION(aChildList.FirstChild() &&
                 nsGkAtoms::tableFrame == aChildList.FirstChild()->GetType(),
                 "expected a table frame");
    mFrames.SetFrames(aChildList);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsTableOuterFrame::AppendFrames(ChildListID     aListID,
                                nsFrameList&    aFrameList)
{
  nsresult rv;

  // We only have two child frames: the inner table and a caption frame.
  // The inner frame is provided when we're initialized, and it cannot change
  if (kCaptionList == aListID) {
    NS_ASSERTION(aFrameList.IsEmpty() ||
                 aFrameList.FirstChild()->GetType() == nsGkAtoms::tableCaptionFrame,
                 "appending non-caption frame to captionList");
    mCaptionFrames.AppendFrames(this, aFrameList);
    rv = NS_OK;

    // Reflow the new caption frame. It's already marked dirty, so
    // just tell the pres shell.
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                       NS_FRAME_HAS_DIRTY_CHILDREN);
  }
  else {
    NS_PRECONDITION(PR_FALSE, "unexpected child list");
    rv = NS_ERROR_UNEXPECTED;
  }

  return rv;
}

NS_IMETHODIMP
nsTableOuterFrame::InsertFrames(ChildListID     aListID,
                                nsIFrame*       aPrevFrame,
                                nsFrameList&    aFrameList)
{
  if (kCaptionList == aListID) {
    NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == this,
                 "inserting after sibling frame with different parent");
    NS_ASSERTION(aFrameList.IsEmpty() ||
                 aFrameList.FirstChild()->GetType() == nsGkAtoms::tableCaptionFrame,
                 "inserting non-caption frame into captionList");
    mCaptionFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);

    // Reflow the new caption frame. It's already marked dirty, so
    // just tell the pres shell.
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                       NS_FRAME_HAS_DIRTY_CHILDREN);
    return NS_OK;
  }
  else {
    NS_PRECONDITION(!aPrevFrame, "invalid previous frame");
    return AppendFrames(aListID, aFrameList);
  }
}

NS_IMETHODIMP
nsTableOuterFrame::RemoveFrame(ChildListID     aListID,
                               nsIFrame*       aOldFrame)
{
  // We only have two child frames: the inner table and one caption frame.
  // The inner frame can't be removed so this should be the caption
  NS_PRECONDITION(kCaptionList == aListID, "can't remove inner frame");

  if (HasSideCaption()) {
    // The old caption width had an effect on the inner table width so
    // we're going to need to reflow it. Mark it dirty
    InnerTableFrame()->AddStateBits(NS_FRAME_IS_DIRTY);
  }

  // Remove the frame and destroy it
  mCaptionFrames.DestroyFrame(aOldFrame);
  
  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN); // also means child removed

  return NS_OK;
}

NS_METHOD 
nsTableOuterFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                    const nsRect&           aDirtyRect,
                                    const nsDisplayListSet& aLists)
{
  // No border, background or outline are painted because they all belong
  // to the inner table.
  if (!IsVisibleInSelection(aBuilder))
    return NS_OK;

  // If there's no caption, take a short cut to avoid having to create
  // the special display list set and then sort it.
  if (mCaptionFrames.IsEmpty())
    return BuildDisplayListForInnerTable(aBuilder, aDirtyRect, aLists);
    
  nsDisplayListCollection set;
  nsresult rv = BuildDisplayListForInnerTable(aBuilder, aDirtyRect, set);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsDisplayListSet captionSet(set, set.BlockBorderBackgrounds());
  rv = BuildDisplayListForChild(aBuilder, mCaptionFrames.FirstChild(),
                                aDirtyRect, captionSet);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // Now we have to sort everything by content order, since the caption
  // may be somewhere inside the table
  set.SortAllByContentOrder(aBuilder, GetContent());
  set.MoveTo(aLists);
  return NS_OK;
}

nsresult
nsTableOuterFrame::BuildDisplayListForInnerTable(nsDisplayListBuilder*   aBuilder,
                                                 const nsRect&           aDirtyRect,
                                                 const nsDisplayListSet& aLists)
{
  // Just paint the regular children, but the children's background is our
  // true background (there should only be one, the real table)
  nsIFrame* kid = mFrames.FirstChild();
  // The children should be in content order
  while (kid) {
    nsresult rv = BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);
    kid = kid->GetNextSibling();
  }
  return NS_OK;
}

void
nsTableOuterFrame::SetSelected(bool          aSelected,
                               SelectionType aType)
{
  nsFrame::SetSelected(aSelected, aType);
  InnerTableFrame()->SetSelected(aSelected, aType);
}

nsIFrame*
nsTableOuterFrame::GetParentStyleContextFrame()
{
  // The table outer frame and the (inner) table frame split the style
  // data by giving the table frame the style context associated with
  // the table content node and creating a style context for the outer
  // frame that is a *child* of the table frame's style context,
  // matching the ::-moz-table-outer pseudo-element.  html.css has a
  // rule that causes that pseudo-element (and thus the outer table)
  // to inherit *some* style properties from the table frame.  The
  // children of the table inherit directly from the inner table, and
  // the outer table's style context is a leaf.

  return InnerTableFrame();
}

// INCREMENTAL REFLOW HELPER FUNCTIONS 

void
nsTableOuterFrame::InitChildReflowState(nsPresContext&    aPresContext,                     
                                        nsHTMLReflowState& aReflowState)
                                    
{
  nsMargin collapseBorder;
  nsMargin collapsePadding(0,0,0,0);
  nsMargin* pCollapseBorder  = nsnull;
  nsMargin* pCollapsePadding = nsnull;
  if (aReflowState.frame == InnerTableFrame() &&
      InnerTableFrame()->IsBorderCollapse()) {
    collapseBorder  = InnerTableFrame()->GetIncludedOuterBCBorder();
    pCollapseBorder = &collapseBorder;
    pCollapsePadding = &collapsePadding;
  }
  aReflowState.Init(&aPresContext, -1, -1, pCollapseBorder, pCollapsePadding);
}

// get the margin and padding data. nsHTMLReflowState doesn't handle the
// case of auto margins
void
nsTableOuterFrame::GetChildMargin(nsPresContext*           aPresContext,
                                  const nsHTMLReflowState& aOuterRS,
                                  nsIFrame*                aChildFrame,
                                  nscoord                  aAvailWidth,
                                  nsMargin&                aMargin)
{
  // construct a reflow state to compute margin and padding. Auto margins
  // will not be computed at this time.

  // create and init the child reflow state
  // XXX We really shouldn't construct a reflow state to do this.
  nsHTMLReflowState childRS(aPresContext, aOuterRS, aChildFrame,
                            nsSize(aAvailWidth, aOuterRS.availableHeight),
                            -1, -1, PR_FALSE);
  InitChildReflowState(*aPresContext, childRS);

  aMargin = childRS.mComputedMargin;
}

static nsSize
GetContainingBlockSize(const nsHTMLReflowState& aOuterRS)
{
  nsSize size(0,0);
  const nsHTMLReflowState* containRS =
    aOuterRS.mCBReflowState;

  if (containRS) {
    size.width = containRS->ComputedWidth();
    if (NS_UNCONSTRAINEDSIZE == size.width) {
      size.width = 0;
    }
    size.height = containRS->ComputedHeight();
    if (NS_UNCONSTRAINEDSIZE == size.height) {
      size.height = 0;
    }
  }
  return size;
}

/* virtual */ nscoord
nsTableOuterFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord width = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                    InnerTableFrame(), nsLayoutUtils::MIN_WIDTH);
  DISPLAY_MIN_WIDTH(this, width);
  if (mCaptionFrames.NotEmpty()) {
    nscoord capWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                           mCaptionFrames.FirstChild(),
                                           nsLayoutUtils::MIN_WIDTH);
    if (HasSideCaption()) {
      width += capWidth;
    } else {
      if (capWidth > width) {
        width = capWidth;
      }
    }
  }
  return width;
}

/* virtual */ nscoord
nsTableOuterFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  nscoord maxWidth;
  DISPLAY_PREF_WIDTH(this, maxWidth);

  maxWidth = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
               InnerTableFrame(), nsLayoutUtils::PREF_WIDTH);
  if (mCaptionFrames.NotEmpty()) {
    PRUint8 captionSide = GetCaptionSide();
    switch(captionSide) {
    case NS_STYLE_CAPTION_SIDE_LEFT:
    case NS_STYLE_CAPTION_SIDE_RIGHT:
      {
        nscoord capMin =
          nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                               mCaptionFrames.FirstChild(),
                                               nsLayoutUtils::MIN_WIDTH);
        maxWidth += capMin;
      }
      break;
    default:
      {
        nsLayoutUtils::IntrinsicWidthType iwt;
        if (captionSide == NS_STYLE_CAPTION_SIDE_TOP ||
            captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM) {
          // Don't let the caption's pref width expand the table's pref
          // width.
          iwt = nsLayoutUtils::MIN_WIDTH;
        } else {
          NS_ASSERTION(captionSide == NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE ||
                       captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE,
                       "unexpected caption side");
          iwt = nsLayoutUtils::PREF_WIDTH;
        }
        nscoord capPref =
          nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                               mCaptionFrames.FirstChild(),
                                               iwt);
        maxWidth = NS_MAX(maxWidth, capPref);
      }
      break;
    }
  }
  return maxWidth;
}

// Compute the margin-box width of aChildFrame given the inputs.  If
// aMarginResult is non-null, fill it with the part of the margin-width
// that was contributed by the margin.
static nscoord
ChildShrinkWrapWidth(nsRenderingContext *aRenderingContext,
                     nsIFrame *aChildFrame,
                     nsSize aCBSize, nscoord aAvailableWidth,
                     nscoord *aMarginResult = nsnull)
{
  // The outer table's children do not use it as a containing block.
  nsCSSOffsetState offsets(aChildFrame, aRenderingContext, aCBSize.width);
  nsSize size = aChildFrame->ComputeSize(aRenderingContext, aCBSize,
                  aAvailableWidth,
                  nsSize(offsets.mComputedMargin.LeftRight(),
                         offsets.mComputedMargin.TopBottom()),
                  nsSize(offsets.mComputedBorderPadding.LeftRight() -
                           offsets.mComputedPadding.LeftRight(),
                         offsets.mComputedBorderPadding.TopBottom() -
                           offsets.mComputedPadding.TopBottom()),
                  nsSize(offsets.mComputedPadding.LeftRight(),
                         offsets.mComputedPadding.TopBottom()),
                  PR_TRUE);
  if (aMarginResult)
    *aMarginResult = offsets.mComputedMargin.LeftRight();
  return size.width + offsets.mComputedMargin.LeftRight() +
                      offsets.mComputedBorderPadding.LeftRight();
}

/* virtual */ nsSize
nsTableOuterFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                   nsSize aCBSize, nscoord aAvailableWidth,
                                   nsSize aMargin, nsSize aBorder,
                                   nsSize aPadding, bool aShrinkWrap)
{
  if (!aShrinkWrap)
    return nsHTMLContainerFrame::ComputeAutoSize(aRenderingContext, aCBSize,
               aAvailableWidth, aMargin, aBorder, aPadding, aShrinkWrap);

  // When we're shrink-wrapping, our auto size needs to wrap around the
  // actual size of the table, which (if it is specified as a percent)
  // could be something that is not reflected in our GetMinWidth and
  // GetPrefWidth.  See bug 349457 for an example.

  // Match the availableWidth logic in Reflow.
  PRUint8 captionSide = GetCaptionSide();
  nscoord width;
  if (captionSide == NO_SIDE) {
    width = ChildShrinkWrapWidth(aRenderingContext, InnerTableFrame(),
                                 aCBSize, aAvailableWidth);
  } else if (captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
             captionSide == NS_STYLE_CAPTION_SIDE_RIGHT) {
    nscoord capWidth = ChildShrinkWrapWidth(aRenderingContext,
                                            mCaptionFrames.FirstChild(),
                                            aCBSize, aAvailableWidth);
    width = capWidth + ChildShrinkWrapWidth(aRenderingContext,
                                            InnerTableFrame(), aCBSize,
                                            aAvailableWidth - capWidth);
  } else if (captionSide == NS_STYLE_CAPTION_SIDE_TOP ||
             captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM) {
    nscoord margin;
    width = ChildShrinkWrapWidth(aRenderingContext, InnerTableFrame(),
                                 aCBSize, aAvailableWidth, &margin);
    nscoord capWidth = ChildShrinkWrapWidth(aRenderingContext,
                                            mCaptionFrames.FirstChild(), aCBSize,
                                            width - margin);
    if (capWidth > width)
      width = capWidth;
  } else {
    NS_ASSERTION(captionSide == NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE ||
                 captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE,
                 "unexpected caption-side");
    width = ChildShrinkWrapWidth(aRenderingContext, InnerTableFrame(),
                                 aCBSize, aAvailableWidth);
    nscoord capWidth = ChildShrinkWrapWidth(aRenderingContext,
                                            mCaptionFrames.FirstChild(),
                                            aCBSize, aAvailableWidth);
    if (capWidth > width)
      width = capWidth;
  }

  return nsSize(width, NS_UNCONSTRAINEDSIZE);
}

PRUint8
nsTableOuterFrame::GetCaptionSide()
{
  if (mCaptionFrames.NotEmpty()) {
    return mCaptionFrames.FirstChild()->GetStyleTableBorder()->mCaptionSide;
  }
  else {
    return NO_SIDE; // no caption
  }
}

PRUint8
nsTableOuterFrame::GetCaptionVerticalAlign()
{
  const nsStyleCoord& va =
    mCaptionFrames.FirstChild()->GetStyleTextReset()->mVerticalAlign;
  return (va.GetUnit() == eStyleUnit_Enumerated)
           ? va.GetIntValue()
           : NS_STYLE_VERTICAL_ALIGN_TOP;
}

void
nsTableOuterFrame::SetDesiredSize(PRUint8         aCaptionSide,
                                  const nsMargin& aInnerMargin,
                                  const nsMargin& aCaptionMargin,
                                  nscoord&        aWidth,
                                  nscoord&        aHeight)
{
  aWidth = aHeight = 0;

  nsRect innerRect = InnerTableFrame()->GetRect();
  nscoord innerWidth = innerRect.width;

  nsRect captionRect(0,0,0,0);
  nscoord captionWidth = 0;
  if (mCaptionFrames.NotEmpty()) {
    captionRect = mCaptionFrames.FirstChild()->GetRect();
    captionWidth = captionRect.width;
  }
  switch(aCaptionSide) {
    case NS_STYLE_CAPTION_SIDE_LEFT:
      aWidth = NS_MAX(aInnerMargin.left, aCaptionMargin.left + captionWidth + aCaptionMargin.right) +
               innerWidth + aInnerMargin.right;
      break;
    case NS_STYLE_CAPTION_SIDE_RIGHT:
      aWidth = NS_MAX(aInnerMargin.right, aCaptionMargin.left + captionWidth + aCaptionMargin.right) +
               innerWidth + aInnerMargin.left;
      break;
    default:
      aWidth = aInnerMargin.left + innerWidth + aInnerMargin.right;
      aWidth = NS_MAX(aWidth, captionRect.XMost() + aCaptionMargin.right);
  }
  aHeight = innerRect.YMost() + aInnerMargin.bottom;
  if (NS_STYLE_CAPTION_SIDE_BOTTOM != aCaptionSide) {
    aHeight = NS_MAX(aHeight, captionRect.YMost() + aCaptionMargin.bottom);
  }
  else {
    aHeight = NS_MAX(aHeight, captionRect.YMost() + aCaptionMargin.bottom +
                              aInnerMargin.bottom);
  }

}

nsresult 
nsTableOuterFrame::GetCaptionOrigin(PRUint32         aCaptionSide,
                                    const nsSize&    aContainBlockSize,
                                    const nsSize&    aInnerSize, 
                                    const nsMargin&  aInnerMargin,
                                    const nsSize&    aCaptionSize,
                                    nsMargin&        aCaptionMargin,
                                    nsPoint&         aOrigin)
{
  aOrigin.x = aOrigin.y = 0;
  if ((NS_UNCONSTRAINEDSIZE == aInnerSize.width) || (NS_UNCONSTRAINEDSIZE == aInnerSize.height) ||  
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.width) || (NS_UNCONSTRAINEDSIZE == aCaptionSize.height)) {
    return NS_OK;
  }
  if (mCaptionFrames.IsEmpty()) return NS_OK;
  
  NS_ASSERTION(NS_AUTOMARGIN != aCaptionMargin.left,   "The computed caption margin is auto?");
  NS_ASSERTION(NS_AUTOMARGIN != aCaptionMargin.top,    "The computed caption margin is auto?");
  NS_ASSERTION(NS_AUTOMARGIN != aCaptionMargin.bottom, "The computed caption margin is auto?");

  // horizontal computation
  switch(aCaptionSide) {
  case NS_STYLE_CAPTION_SIDE_BOTTOM:
  case NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE: {
    // FIXME: Position relative to right edge for RTL.  (Based on table
    // direction or table parent direction?)
    aOrigin.x = aCaptionMargin.left;
    if (aCaptionSide == NS_STYLE_CAPTION_SIDE_BOTTOM) {
      // We placed the caption using only the table's width as available
      // width, and we should position it this way as well.
      aOrigin.x += aInnerMargin.left;
    }
  } break;
  case NS_STYLE_CAPTION_SIDE_LEFT: {
    aOrigin.x = aCaptionMargin.left;
  } break;
  case NS_STYLE_CAPTION_SIDE_RIGHT: {
    aOrigin.x = aInnerMargin.left + aInnerSize.width + aCaptionMargin.left;
  } break;
  default: { // top
    NS_ASSERTION(aCaptionSide == NS_STYLE_CAPTION_SIDE_TOP ||
                 aCaptionSide == NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE,
                 "unexpected caption side");
    // FIXME: Position relative to right edge for RTL.  (Based on table
    // direction or table parent direction?)
    aOrigin.x = aCaptionMargin.left;
    if (aCaptionSide == NS_STYLE_CAPTION_SIDE_TOP) {
      // We placed the caption using only the table's width as available
      // width, and we should position it this way as well.
      aOrigin.x += aInnerMargin.left;
    }
    
  } break;
  }
  // vertical computation
  switch (aCaptionSide) {
    case NS_STYLE_CAPTION_SIDE_RIGHT:
    case NS_STYLE_CAPTION_SIDE_LEFT:
      aOrigin.y = aInnerMargin.top;
      switch (GetCaptionVerticalAlign()) {
        case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
          aOrigin.y = NS_MAX(0, aInnerMargin.top + ((aInnerSize.height - aCaptionSize.height) / 2));
          break;
        case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
          aOrigin.y = NS_MAX(0, aInnerMargin.top + aInnerSize.height - aCaptionSize.height);
          break;
        default:
          break;
      }
      break;
    case NS_STYLE_CAPTION_SIDE_BOTTOM: {
      aOrigin.y = aInnerMargin.top + aInnerSize.height + aCaptionMargin.top;
    } break;
    case NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE: {
      nsCollapsingMargin marg;
      marg.Include(aCaptionMargin.top);
      marg.Include(aInnerMargin.bottom);
      nscoord collapseMargin = marg.get();
      aOrigin.y = aInnerMargin.top + aInnerSize.height + collapseMargin;
    } break;
    case NS_STYLE_CAPTION_SIDE_TOP: {
      aOrigin.y = aInnerMargin.top + aCaptionMargin.top;
    } break;
    case NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE: {
      aOrigin.y = aCaptionMargin.top;
    } break;
    default:
      NS_NOTREACHED("Unknown caption alignment type");
      break;
  }
  return NS_OK;
}

nsresult 
nsTableOuterFrame::GetInnerOrigin(PRUint32         aCaptionSide,
                                  const nsSize&    aContainBlockSize,
                                  const nsSize&    aCaptionSize, 
                                  const nsMargin&  aCaptionMargin,
                                  const nsSize&    aInnerSize,
                                  nsMargin&        aInnerMargin,
                                  nsPoint&         aOrigin)
{
  
  NS_ASSERTION(NS_AUTOMARGIN != aCaptionMargin.left,  "The computed caption margin is auto?");
  NS_ASSERTION(NS_AUTOMARGIN != aCaptionMargin.right, "The computed caption margin is auto?");
  NS_ASSERTION(NS_AUTOMARGIN != aInnerMargin.left,    "The computed inner margin is auto?");
  NS_ASSERTION(NS_AUTOMARGIN != aInnerMargin.right,   "The computed inner margin is auto?");
  NS_ASSERTION(NS_AUTOMARGIN != aInnerMargin.top,     "The computed inner margin is auto?");
  NS_ASSERTION(NS_AUTOMARGIN != aInnerMargin.bottom,  "The computed inner margin is auto?");
  
  aOrigin.x = aOrigin.y = 0;
  if ((NS_UNCONSTRAINEDSIZE == aInnerSize.width) || (NS_UNCONSTRAINEDSIZE == aInnerSize.height) ||  
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.width) || (NS_UNCONSTRAINEDSIZE == aCaptionSize.height)) {
    return NS_OK;
  }

  nscoord minCapWidth = aCaptionSize.width;
  
  minCapWidth += aCaptionMargin.left;
  minCapWidth += aCaptionMargin.right;

  // horizontal computation
  switch (aCaptionSide) {
  case NS_STYLE_CAPTION_SIDE_LEFT: {
    if (aInnerMargin.left < minCapWidth) {
      // shift the inner table to get some place for the caption
      aInnerMargin.right += aInnerMargin.left - minCapWidth;
      aInnerMargin.right  = NS_MAX(0, aInnerMargin.right);
      aInnerMargin.left   = minCapWidth;
    }
    aOrigin.x = aInnerMargin.left;
  } break;
  default: {
    NS_ASSERTION(aCaptionSide == NS_STYLE_CAPTION_SIDE_TOP ||
                 aCaptionSide == NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE ||
                 aCaptionSide == NS_STYLE_CAPTION_SIDE_BOTTOM ||
                 aCaptionSide == NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE ||
                 aCaptionSide == NS_STYLE_CAPTION_SIDE_RIGHT ||
                 aCaptionSide == NO_SIDE,
                 "unexpected caption side");
    aOrigin.x = aInnerMargin.left;
  } break;
  }
  
  // vertical computation
  switch (aCaptionSide) {
    case NS_STYLE_CAPTION_SIDE_BOTTOM:
    case NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE: {
      aOrigin.y = aInnerMargin.top;
    } break;
    case NS_STYLE_CAPTION_SIDE_LEFT:
    case NS_STYLE_CAPTION_SIDE_RIGHT: {
      aOrigin.y = aInnerMargin.top;
      switch (GetCaptionVerticalAlign()) {
        case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
          aOrigin.y = NS_MAX(aInnerMargin.top, (aCaptionSize.height - aInnerSize.height) / 2);
          break;
        case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
          aOrigin.y = NS_MAX(aInnerMargin.top, aCaptionSize.height - aInnerSize.height);
          break;
        default:
          break;
      }
    } break;
    case NO_SIDE:
    case NS_STYLE_CAPTION_SIDE_TOP: {
      aOrigin.y = aInnerMargin.top + aCaptionMargin.top + aCaptionSize.height +
                  aCaptionMargin.bottom;
    } break;
    case NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE: {
      nsCollapsingMargin marg;
      marg.Include(aCaptionMargin.bottom);
      marg.Include(aInnerMargin.top);
      nscoord collapseMargin = marg.get();
      aOrigin.y = aCaptionMargin.top + aCaptionSize.height + collapseMargin;
    } break;
    default:
      NS_NOTREACHED("Unknown caption alignment type");
      break;
  }
  return NS_OK;
}

void
nsTableOuterFrame::OuterBeginReflowChild(nsPresContext*           aPresContext,
                                         nsIFrame*                aChildFrame,
                                         const nsHTMLReflowState& aOuterRS,
                                         void*                    aChildRSSpace,
                                         nscoord                  aAvailWidth)
{ 
  // work around pixel rounding errors, round down to ensure we don't exceed the avail height in
  nscoord availHeight = aOuterRS.availableHeight;
  if (NS_UNCONSTRAINEDSIZE != availHeight) {
    if (mCaptionFrames.FirstChild() == aChildFrame) {
      availHeight = NS_UNCONSTRAINEDSIZE;
    } else {
      nsMargin margin;
      GetChildMargin(aPresContext, aOuterRS, aChildFrame,
                     aOuterRS.availableWidth, margin);
    
      NS_ASSERTION(NS_UNCONSTRAINEDSIZE != margin.top, "No unconstrainedsize arithmetic, please");
      availHeight -= margin.top;
 
      NS_ASSERTION(NS_UNCONSTRAINEDSIZE != margin.bottom, "No unconstrainedsize arithmetic, please");
      availHeight -= margin.bottom;
    }
  }
  nsSize availSize(aAvailWidth, availHeight);
  // create and init the child reflow state, using placement new on
  // stack space allocated by the caller, so that the caller can destroy
  // it
  nsHTMLReflowState &childRS = * new (aChildRSSpace)
    nsHTMLReflowState(aPresContext, aOuterRS, aChildFrame, availSize,
                      -1, -1, PR_FALSE);
  InitChildReflowState(*aPresContext, childRS);

  // see if we need to reset top of page due to a caption
  if (mCaptionFrames.NotEmpty()) {
    PRUint8 captionSide = GetCaptionSide();
    if (((captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM ||
          captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE) &&
         mCaptionFrames.FirstChild() == aChildFrame) || 
        ((captionSide == NS_STYLE_CAPTION_SIDE_TOP ||
          captionSide == NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE) &&
         InnerTableFrame() == aChildFrame)) {
      childRS.mFlags.mIsTopOfPage = PR_FALSE;
    }
  }
}

nsresult
nsTableOuterFrame::OuterDoReflowChild(nsPresContext*             aPresContext,
                                      nsIFrame*                  aChildFrame,
                                      const nsHTMLReflowState&   aChildRS,
                                      nsHTMLReflowMetrics&       aMetrics,
                                      nsReflowStatus&            aStatus)
{ 

  // use the current position as a best guess for placement
  nsPoint childPt = aChildFrame->GetPosition();
  return ReflowChild(aChildFrame, aPresContext, aMetrics, aChildRS,
                     childPt.x, childPt.y, NS_FRAME_NO_MOVE_FRAME, aStatus);
}

void 
nsTableOuterFrame::UpdateReflowMetrics(PRUint8              aCaptionSide,
                                       nsHTMLReflowMetrics& aMet,
                                       const nsMargin&      aInnerMargin,
                                       const nsMargin&      aCaptionMargin)
{
  SetDesiredSize(aCaptionSide, aInnerMargin, aCaptionMargin,
                 aMet.width, aMet.height);

  aMet.SetOverflowAreasToDesiredBounds();
  ConsiderChildOverflow(aMet.mOverflowAreas, InnerTableFrame());
  if (mCaptionFrames.NotEmpty()) {
    ConsiderChildOverflow(aMet.mOverflowAreas, mCaptionFrames.FirstChild());
  }
  FinishAndStoreOverflow(&aMet);
}

NS_METHOD nsTableOuterFrame::Reflow(nsPresContext*           aPresContext,
                                    nsHTMLReflowMetrics&     aDesiredSize,
                                    const nsHTMLReflowState& aOuterRS,
                                    nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableOuterFrame");
  DISPLAY_REFLOW(aPresContext, this, aOuterRS, aDesiredSize, aStatus);

  nsresult rv = NS_OK;
  PRUint8 captionSide = GetCaptionSide();

  // Initialize out parameters
  aDesiredSize.width = aDesiredSize.height = 0;
  aStatus = NS_FRAME_COMPLETE;

  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    // Set up our kids.  They're already present, on an overflow list, 
    // or there are none so we'll create them now
    MoveOverflowToChildList(aPresContext);
  }

  // Use longs to get more-aligned space.
  #define LONGS_IN_HTMLRS \
    ((sizeof(nsHTMLReflowState) + sizeof(long) - 1) / sizeof(long))
  long captionRSSpace[LONGS_IN_HTMLRS];
  nsHTMLReflowState *captionRS =
    static_cast<nsHTMLReflowState*>((void*)captionRSSpace);
  long innerRSSpace[LONGS_IN_HTMLRS];
  nsHTMLReflowState *innerRS =
    static_cast<nsHTMLReflowState*>((void*) innerRSSpace);

  nsRect origInnerRect = InnerTableFrame()->GetRect();
  nsRect origInnerVisualOverflow = InnerTableFrame()->GetVisualOverflowRect();
  bool innerFirstReflow =
    (InnerTableFrame()->GetStateBits() & NS_FRAME_FIRST_REFLOW) != 0;
  nsRect origCaptionRect;
  nsRect origCaptionVisualOverflow;
  bool captionFirstReflow;
  if (mCaptionFrames.NotEmpty()) {
    origCaptionRect = mCaptionFrames.FirstChild()->GetRect();
    origCaptionVisualOverflow =
      mCaptionFrames.FirstChild()->GetVisualOverflowRect();
    captionFirstReflow =
      (mCaptionFrames.FirstChild()->GetStateBits() & NS_FRAME_FIRST_REFLOW) != 0;
  }
  
  // ComputeAutoSize has to match this logic.
  if (captionSide == NO_SIDE) {
    // We don't have a caption.
    OuterBeginReflowChild(aPresContext, InnerTableFrame(), aOuterRS,
                          innerRSSpace, aOuterRS.ComputedWidth());
  } else if (captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
             captionSide == NS_STYLE_CAPTION_SIDE_RIGHT) {
    // nsTableCaptionFrame::ComputeAutoSize takes care of making side
    // captions small.  Compute the caption's size first, and tell the
    // table to fit in what's left.
    OuterBeginReflowChild(aPresContext, mCaptionFrames.FirstChild(), aOuterRS,
                          captionRSSpace, aOuterRS.ComputedWidth());
    nscoord innerAvailWidth = aOuterRS.ComputedWidth() -
      (captionRS->ComputedWidth() + captionRS->mComputedMargin.LeftRight() +
       captionRS->mComputedBorderPadding.LeftRight());
    OuterBeginReflowChild(aPresContext, InnerTableFrame(), aOuterRS,
                          innerRSSpace, innerAvailWidth);

  } else if (captionSide == NS_STYLE_CAPTION_SIDE_TOP ||
             captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM) {
    // Compute the table's size first, and then prevent the caption from
    // being wider unless it has to be.
    //
    // Note that CSS 2.1 (but not 2.0) says:
    //   The width of the anonymous box is the border-edge width of the
    //   table box inside it
    // We don't actually make our anonymous box that width (if we did,
    // it would break 'auto' margins), but this effectively does that.
    OuterBeginReflowChild(aPresContext, InnerTableFrame(), aOuterRS,
                          innerRSSpace, aOuterRS.ComputedWidth());
    // It's good that CSS 2.1 says not to include margins, since we
    // can't, since they already been converted so they exactly
    // fill the available width (ignoring the margin on one side if
    // neither are auto).  (We take advantage of that later when we call
    // GetCaptionOrigin, though.)
    nscoord innerBorderWidth = innerRS->ComputedWidth() +
                               innerRS->mComputedBorderPadding.LeftRight();
    OuterBeginReflowChild(aPresContext, mCaptionFrames.FirstChild(), aOuterRS,
                          captionRSSpace, innerBorderWidth);
  } else {
    NS_ASSERTION(captionSide == NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE ||
                 captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE,
                 "unexpected caption-side");
    // Size the table and the caption independently.
    OuterBeginReflowChild(aPresContext, mCaptionFrames.FirstChild(), aOuterRS,
                          captionRSSpace, aOuterRS.ComputedWidth());
    OuterBeginReflowChild(aPresContext, InnerTableFrame(), aOuterRS,
                          innerRSSpace, aOuterRS.ComputedWidth());
  }

  // First reflow the caption.
  nsHTMLReflowMetrics captionMet;
  nsSize captionSize;
  nsMargin captionMargin;
  if (mCaptionFrames.NotEmpty()) {
    nsReflowStatus capStatus; // don't let the caption cause incomplete
    rv = OuterDoReflowChild(aPresContext, mCaptionFrames.FirstChild(),
                            *captionRS, captionMet, capStatus);
    if (NS_FAILED(rv)) return rv;
    captionSize.width = captionMet.width;
    captionSize.height = captionMet.height;
    captionMargin = captionRS->mComputedMargin;
    // Now that we know the height of the caption, reduce the available height
    // for the table frame if we are height constrained and the caption is above
    // or below the inner table.
    if (NS_UNCONSTRAINEDSIZE != aOuterRS.availableHeight) {
      nscoord captionHeight = 0;
      switch (captionSide) {
        case NS_STYLE_CAPTION_SIDE_TOP:
        case NS_STYLE_CAPTION_SIDE_BOTTOM: {
          captionHeight = captionSize.height + captionMargin.TopBottom();
          break;
        }
        case NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE: {
          nsCollapsingMargin belowCaptionMargin;
          belowCaptionMargin.Include(captionMargin.bottom);
          belowCaptionMargin.Include(innerRS->mComputedMargin.top);
          captionHeight = captionSize.height + captionMargin.top +
                          belowCaptionMargin.get();
          break;
        }
        case NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE: {
          nsCollapsingMargin aboveCaptionMargin;
          aboveCaptionMargin.Include(captionMargin.top);
          aboveCaptionMargin.Include(innerRS->mComputedMargin.bottom);
          captionHeight = captionSize.height + captionMargin.bottom +
                          aboveCaptionMargin.get();
          break;
        }
      }
      innerRS->availableHeight =
        NS_MAX(0, innerRS->availableHeight - captionHeight);
    }
  } else {
    captionSize.SizeTo(0,0);
    captionMargin.SizeTo(0,0,0,0);
  }

  // Then, now that we know how much to reduce the width of the inner
  // table to account for side captions, reflow the inner table.
  nsHTMLReflowMetrics innerMet;
  rv = OuterDoReflowChild(aPresContext, InnerTableFrame(), *innerRS,
                          innerMet, aStatus);
  if (NS_FAILED(rv)) return rv;
  nsSize innerSize;
  innerSize.width = innerMet.width;
  innerSize.height = innerMet.height;
  nsMargin innerMargin = innerRS->mComputedMargin;

  nsSize   containSize = GetContainingBlockSize(aOuterRS);

  // Now that we've reflowed both we can place them.
  // XXXldb Most of the input variables here are now uninitialized!

  // XXX Need to recompute inner table's auto margins for the case of side
  // captions.  (Caption's are broken too, but that should be fixed earlier.)

  if (mCaptionFrames.NotEmpty()) {
    nsPoint captionOrigin;
    GetCaptionOrigin(captionSide, containSize, innerSize, 
                     innerMargin, captionSize, captionMargin, captionOrigin);
    FinishReflowChild(mCaptionFrames.FirstChild(), aPresContext, captionRS,
                      captionMet, captionOrigin.x, captionOrigin.y, 0);
    captionRS->~nsHTMLReflowState();
  }
  // XXX If the height is constrained then we need to check whether
  // everything still fits...

  nsPoint innerOrigin;
  GetInnerOrigin(captionSide, containSize, captionSize, 
                 captionMargin, innerSize, innerMargin, innerOrigin);
  FinishReflowChild(InnerTableFrame(), aPresContext, innerRS, innerMet,
                    innerOrigin.x, innerOrigin.y, 0);
  innerRS->~nsHTMLReflowState();

  nsTableFrame::InvalidateFrame(InnerTableFrame(), origInnerRect,
                                origInnerVisualOverflow, innerFirstReflow);
  if (mCaptionFrames.NotEmpty()) {
    nsTableFrame::InvalidateFrame(mCaptionFrames.FirstChild(), origCaptionRect,
                                  origCaptionVisualOverflow,
                                  captionFirstReflow);
  }

  UpdateReflowMetrics(captionSide, aDesiredSize, innerMargin, captionMargin);
  
  // Return our desired rect

  NS_FRAME_SET_TRUNCATION(aStatus, aOuterRS, aDesiredSize);
  return rv;
}

nsIAtom*
nsTableOuterFrame::GetType() const
{
  return nsGkAtoms::tableOuterFrame;
}

/* ----- global methods ----- */

/*------------------ nsITableLayout methods ------------------------------*/
NS_IMETHODIMP 
nsTableOuterFrame::GetCellDataAt(PRInt32 aRowIndex, PRInt32 aColIndex, 
                                 nsIDOMElement* &aCell,   //out params
                                 PRInt32& aStartRowIndex, PRInt32& aStartColIndex, 
                                 PRInt32& aRowSpan, PRInt32& aColSpan,
                                 PRInt32& aActualRowSpan, PRInt32& aActualColSpan,
                                 bool& aIsSelected)
{
  return InnerTableFrame()->GetCellDataAt(aRowIndex, aColIndex, aCell,
                                          aStartRowIndex, aStartColIndex, 
                                          aRowSpan, aColSpan, aActualRowSpan,
                                          aActualColSpan, aIsSelected);
}

NS_IMETHODIMP
nsTableOuterFrame::GetTableSize(PRInt32& aRowCount, PRInt32& aColCount)
{
  return InnerTableFrame()->GetTableSize(aRowCount, aColCount);
}

NS_IMETHODIMP
nsTableOuterFrame::GetIndexByRowAndColumn(PRInt32 aRow, PRInt32 aColumn,
                                          PRInt32 *aIndex)
{
  NS_ENSURE_ARG_POINTER(aIndex);
  return InnerTableFrame()->GetIndexByRowAndColumn(aRow, aColumn, aIndex);
}

NS_IMETHODIMP
nsTableOuterFrame::GetRowAndColumnByIndex(PRInt32 aIndex,
                                          PRInt32 *aRow, PRInt32 *aColumn)
{
  NS_ENSURE_ARG_POINTER(aRow);
  NS_ENSURE_ARG_POINTER(aColumn);
  return InnerTableFrame()->GetRowAndColumnByIndex(aIndex, aRow, aColumn);
}

/*---------------- end of nsITableLayout implementation ------------------*/


nsIFrame*
NS_NewTableOuterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTableOuterFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTableOuterFrame)

#ifdef DEBUG
NS_IMETHODIMP
nsTableOuterFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("TableOuter"), aResult);
}
#endif

