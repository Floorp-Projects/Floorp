/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsTableOuterFrame.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "prinrval.h"
#include "nsGkAtoms.h"
#include "nsHTMLParts.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsIDOMNode.h"
#include "nsDisplayList.h"
#include "nsLayoutUtils.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::layout;

/* ----------- nsTableCaptionFrame ---------- */

#define NS_TABLE_FRAME_CAPTION_LIST_INDEX 1
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
    return nsContainerFrame::GetBaseline();
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

  // If we're a container for font size inflation, then shrink
  // wrapping inside of us should not apply font size inflation.
  AutoMaybeDisableFontInflation an(this);

  uint8_t captionSide = StyleTableBorder()->mCaptionSide;
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
nsTableCaptionFrame::GetParentStyleContextFrame() const
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
                                              StyleContext()->GetPseudo());
    }
  }

  NS_NOTREACHED("Where is our inner table frame?");
  return nsBlockFrame::GetParentStyleContextFrame();
}

#ifdef ACCESSIBILITY
a11y::AccType
nsTableCaptionFrame::AccessibleType()
{
  if (!GetRect().IsEmpty()) {
    return a11y::eHTMLCaptionType;
  }

  return a11y::eNoType;
}
#endif

#ifdef DEBUG_FRAME_DUMP
nsresult
nsTableCaptionFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Caption"), aResult);
}
#endif

nsTableCaptionFrame* 
NS_NewTableCaptionFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTableCaptionFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTableCaptionFrame)

/* ----------- nsTableOuterFrame ---------- */

nsTableOuterFrame::nsTableOuterFrame(nsStyleContext* aContext):
  nsContainerFrame(aContext)
{
}

nsTableOuterFrame::~nsTableOuterFrame()
{
}

NS_QUERYFRAME_HEAD(nsTableOuterFrame)
  NS_QUERYFRAME_ENTRY(nsTableOuterFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

#ifdef ACCESSIBILITY
a11y::AccType
nsTableOuterFrame::AccessibleType()
{
  return a11y::eHTMLTableType;
}
#endif

void
nsTableOuterFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  DestroyAbsoluteFrames(aDestructRoot);
  mCaptionFrames.DestroyFramesFrom(aDestructRoot);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

const nsFrameList&
nsTableOuterFrame::GetChildList(ChildListID aListID) const
{
  if (aListID == kCaptionList) {
    return mCaptionFrames;
  }

  return nsContainerFrame::GetChildList(aListID);
}

void
nsTableOuterFrame::GetChildLists(nsTArray<ChildList>* aLists) const
{
  nsContainerFrame::GetChildLists(aLists);
  mCaptionFrames.AppendIfNonempty(aLists, kCaptionList);
}

void 
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
}

void
nsTableOuterFrame::AppendFrames(ChildListID     aListID,
                                nsFrameList&    aFrameList)
{
  // We only have two child frames: the inner table and a caption frame.
  // The inner frame is provided when we're initialized, and it cannot change
  MOZ_ASSERT(kCaptionList == aListID, "unexpected child list");
  NS_ASSERTION(aFrameList.IsEmpty() ||
               aFrameList.FirstChild()->GetType() == nsGkAtoms::tableCaptionFrame,
               "appending non-caption frame to captionList");
  mCaptionFrames.AppendFrames(this, aFrameList);

  // Reflow the new caption frame. It's already marked dirty, so
  // just tell the pres shell.
  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN);
}

void
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
    mCaptionFrames.InsertFrames(nullptr, aPrevFrame, aFrameList);

    // Reflow the new caption frame. It's already marked dirty, so
    // just tell the pres shell.
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                       NS_FRAME_HAS_DIRTY_CHILDREN);
  }
  else {
    // XXX this block is dead code? (AppendFrames only handles kCaptionList)
    NS_PRECONDITION(!aPrevFrame, "invalid previous frame");
    AppendFrames(aListID, aFrameList);
  }
}

void
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
}

void
nsTableOuterFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                    const nsRect&           aDirtyRect,
                                    const nsDisplayListSet& aLists)
{
  // No border, background or outline are painted because they all belong
  // to the inner table.

  // If there's no caption, take a short cut to avoid having to create
  // the special display list set and then sort it.
  if (mCaptionFrames.IsEmpty()) {
    BuildDisplayListForInnerTable(aBuilder, aDirtyRect, aLists);
    return;
  }

  nsDisplayListCollection set;
  BuildDisplayListForInnerTable(aBuilder, aDirtyRect, set);
  
  nsDisplayListSet captionSet(set, set.BlockBorderBackgrounds());
  BuildDisplayListForChild(aBuilder, mCaptionFrames.FirstChild(),
                           aDirtyRect, captionSet);
  
  // Now we have to sort everything by content order, since the caption
  // may be somewhere inside the table
  set.SortAllByContentOrder(aBuilder, GetContent());
  set.MoveTo(aLists);
}

void
nsTableOuterFrame::BuildDisplayListForInnerTable(nsDisplayListBuilder*   aBuilder,
                                                 const nsRect&           aDirtyRect,
                                                 const nsDisplayListSet& aLists)
{
  // Just paint the regular children, but the children's background is our
  // true background (there should only be one, the real table)
  nsIFrame* kid = mFrames.FirstChild();
  // The children should be in content order
  while (kid) {
    BuildDisplayListForChild(aBuilder, kid, aDirtyRect, aLists);
    kid = kid->GetNextSibling();
  }
}

nsIFrame*
nsTableOuterFrame::GetParentStyleContextFrame() const
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
  nsMargin* pCollapseBorder  = nullptr;
  nsMargin* pCollapsePadding = nullptr;
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
                            nsSize(aAvailWidth, aOuterRS.AvailableHeight()),
                            -1, -1, nsHTMLReflowState::CALLER_WILL_INIT);
  InitChildReflowState(*aPresContext, childRS);

  aMargin = childRS.ComputedPhysicalMargin();
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
    uint8_t captionSide = GetCaptionSide();
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
        maxWidth = std::max(maxWidth, capPref);
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
                     nscoord *aMarginResult = nullptr)
{
  AutoMaybeDisableFontInflation an(aChildFrame);

  nsCSSOffsetState offsets(aChildFrame, aRenderingContext, aCBSize.width);
  nsSize size = aChildFrame->ComputeSize(aRenderingContext, aCBSize,
                  aAvailableWidth,
                  nsSize(offsets.ComputedPhysicalMargin().LeftRight(),
                         offsets.ComputedPhysicalMargin().TopBottom()),
                  nsSize(offsets.ComputedPhysicalBorderPadding().LeftRight() -
                           offsets.ComputedPhysicalPadding().LeftRight(),
                         offsets.ComputedPhysicalBorderPadding().TopBottom() -
                           offsets.ComputedPhysicalPadding().TopBottom()),
                  nsSize(offsets.ComputedPhysicalPadding().LeftRight(),
                         offsets.ComputedPhysicalPadding().TopBottom()),
                  true);
  if (aMarginResult)
    *aMarginResult = offsets.ComputedPhysicalMargin().LeftRight();
  return size.width + offsets.ComputedPhysicalMargin().LeftRight() +
                      offsets.ComputedPhysicalBorderPadding().LeftRight();
}

/* virtual */ nsSize
nsTableOuterFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                   nsSize aCBSize, nscoord aAvailableWidth,
                                   nsSize aMargin, nsSize aBorder,
                                   nsSize aPadding, bool aShrinkWrap)
{
  nscoord kidAvailableWidth = aAvailableWidth - aMargin.width;
  NS_ASSERTION(aBorder == nsSize(0, 0) &&
               aPadding == nsSize(0, 0),
               "Table outer frames cannot hae borders or paddings");

  // When we're shrink-wrapping, our auto size needs to wrap around the
  // actual size of the table, which (if it is specified as a percent)
  // could be something that is not reflected in our GetMinWidth and
  // GetPrefWidth.  See bug 349457 for an example.

  // Match the availableWidth logic in Reflow.
  uint8_t captionSide = GetCaptionSide();
  nscoord width;
  if (captionSide == NO_SIDE) {
    width = ChildShrinkWrapWidth(aRenderingContext, InnerTableFrame(),
                                 aCBSize, kidAvailableWidth);
  } else if (captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
             captionSide == NS_STYLE_CAPTION_SIDE_RIGHT) {
    nscoord capWidth = ChildShrinkWrapWidth(aRenderingContext,
                                            mCaptionFrames.FirstChild(),
                                            aCBSize, kidAvailableWidth);
    width = capWidth + ChildShrinkWrapWidth(aRenderingContext,
                                            InnerTableFrame(), aCBSize,
                                            kidAvailableWidth - capWidth);
  } else if (captionSide == NS_STYLE_CAPTION_SIDE_TOP ||
             captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM) {
    nscoord margin;
    width = ChildShrinkWrapWidth(aRenderingContext, InnerTableFrame(),
                                 aCBSize, kidAvailableWidth, &margin);
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
                                 aCBSize, kidAvailableWidth);
    nscoord capWidth = ChildShrinkWrapWidth(aRenderingContext,
                                            mCaptionFrames.FirstChild(),
                                            aCBSize, kidAvailableWidth);
    if (capWidth > width)
      width = capWidth;
  }

  return nsSize(width, NS_UNCONSTRAINEDSIZE);
}

uint8_t
nsTableOuterFrame::GetCaptionSide()
{
  if (mCaptionFrames.NotEmpty()) {
    return mCaptionFrames.FirstChild()->StyleTableBorder()->mCaptionSide;
  }
  else {
    return NO_SIDE; // no caption
  }
}

uint8_t
nsTableOuterFrame::GetCaptionVerticalAlign()
{
  const nsStyleCoord& va =
    mCaptionFrames.FirstChild()->StyleTextReset()->mVerticalAlign;
  return (va.GetUnit() == eStyleUnit_Enumerated)
           ? va.GetIntValue()
           : NS_STYLE_VERTICAL_ALIGN_TOP;
}

void
nsTableOuterFrame::SetDesiredSize(uint8_t         aCaptionSide,
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
      aWidth = std::max(aInnerMargin.left, aCaptionMargin.left + captionWidth + aCaptionMargin.right) +
               innerWidth + aInnerMargin.right;
      break;
    case NS_STYLE_CAPTION_SIDE_RIGHT:
      aWidth = std::max(aInnerMargin.right, aCaptionMargin.left + captionWidth + aCaptionMargin.right) +
               innerWidth + aInnerMargin.left;
      break;
    default:
      aWidth = aInnerMargin.left + innerWidth + aInnerMargin.right;
      aWidth = std::max(aWidth, captionRect.XMost() + aCaptionMargin.right);
  }
  aHeight = innerRect.YMost() + aInnerMargin.bottom;
  if (NS_STYLE_CAPTION_SIDE_BOTTOM != aCaptionSide) {
    aHeight = std::max(aHeight, captionRect.YMost() + aCaptionMargin.bottom);
  }
  else {
    aHeight = std::max(aHeight, captionRect.YMost() + aCaptionMargin.bottom +
                              aInnerMargin.bottom);
  }

}

nsresult 
nsTableOuterFrame::GetCaptionOrigin(uint32_t         aCaptionSide,
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
          aOrigin.y = std::max(0, aInnerMargin.top + ((aInnerSize.height - aCaptionSize.height) / 2));
          break;
        case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
          aOrigin.y = std::max(0, aInnerMargin.top + aInnerSize.height - aCaptionSize.height);
          break;
        default:
          break;
      }
      break;
    case NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE:
    case NS_STYLE_CAPTION_SIDE_BOTTOM: {
      aOrigin.y = aInnerMargin.top + aInnerSize.height + aCaptionMargin.top;
    } break;
    case NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE:
    case NS_STYLE_CAPTION_SIDE_TOP: {
      aOrigin.y = aInnerMargin.top + aCaptionMargin.top;
    } break;
    default:
      NS_NOTREACHED("Unknown caption alignment type");
      break;
  }
  return NS_OK;
}

nsresult 
nsTableOuterFrame::GetInnerOrigin(uint32_t         aCaptionSide,
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
      aInnerMargin.right  = std::max(0, aInnerMargin.right);
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
          aOrigin.y = std::max(aInnerMargin.top, (aCaptionSize.height - aInnerSize.height) / 2);
          break;
        case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
          aOrigin.y = std::max(aInnerMargin.top, aCaptionSize.height - aInnerSize.height);
          break;
        default:
          break;
      }
    } break;
    case NO_SIDE:
    case NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE:
    case NS_STYLE_CAPTION_SIDE_TOP: {
      aOrigin.y = aInnerMargin.top + aCaptionMargin.top + aCaptionSize.height +
                  aCaptionMargin.bottom;
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
  nscoord availHeight = aOuterRS.AvailableHeight();
  if (NS_UNCONSTRAINEDSIZE != availHeight) {
    if (mCaptionFrames.FirstChild() == aChildFrame) {
      availHeight = NS_UNCONSTRAINEDSIZE;
    } else {
      nsMargin margin;
      GetChildMargin(aPresContext, aOuterRS, aChildFrame,
                     aOuterRS.AvailableWidth(), margin);
    
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
                      -1, -1, nsHTMLReflowState::CALLER_WILL_INIT);
  InitChildReflowState(*aPresContext, childRS);

  // see if we need to reset top-of-page due to a caption
  if (childRS.mFlags.mIsTopOfPage &&
      mCaptionFrames.FirstChild() == aChildFrame) {
    uint8_t captionSide = GetCaptionSide();
    if (captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM ||
        captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE) {
      childRS.mFlags.mIsTopOfPage = false;
    }
  }
}

void
nsTableOuterFrame::OuterDoReflowChild(nsPresContext*             aPresContext,
                                      nsIFrame*                  aChildFrame,
                                      const nsHTMLReflowState&   aChildRS,
                                      nsHTMLReflowMetrics&       aMetrics,
                                      nsReflowStatus&            aStatus)
{ 

  // use the current position as a best guess for placement
  nsPoint childPt = aChildFrame->GetPosition();
  uint32_t flags = NS_FRAME_NO_MOVE_FRAME;

  // We don't want to delete our next-in-flow's child if it's an inner table
  // frame, because outer table frames always assume that their inner table
  // frames don't go away. If an outer table frame is removed because it is
  // a next-in-flow of an already complete outer table frame, then it will
  // take care of removing it's inner table frame.
  if (aChildFrame == InnerTableFrame()) {
    flags |= NS_FRAME_NO_DELETE_NEXT_IN_FLOW_CHILD;
  }

  ReflowChild(aChildFrame, aPresContext, aMetrics, aChildRS,
              childPt.x, childPt.y, flags, aStatus);
}

void 
nsTableOuterFrame::UpdateReflowMetrics(uint8_t              aCaptionSide,
                                       nsHTMLReflowMetrics& aMet,
                                       const nsMargin&      aInnerMargin,
                                       const nsMargin&      aCaptionMargin)
{
  SetDesiredSize(aCaptionSide, aInnerMargin, aCaptionMargin,
                 aMet.Width(), aMet.Height());

  aMet.SetOverflowAreasToDesiredBounds();
  ConsiderChildOverflow(aMet.mOverflowAreas, InnerTableFrame());
  if (mCaptionFrames.NotEmpty()) {
    ConsiderChildOverflow(aMet.mOverflowAreas, mCaptionFrames.FirstChild());
  }
}

void
nsTableOuterFrame::Reflow(nsPresContext*           aPresContext,
                                    nsHTMLReflowMetrics&     aDesiredSize,
                                    const nsHTMLReflowState& aOuterRS,
                                    nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableOuterFrame");
  DISPLAY_REFLOW(aPresContext, this, aOuterRS, aDesiredSize, aStatus);

  uint8_t captionSide = GetCaptionSide();

  // Initialize out parameters
  aDesiredSize.Width() = aDesiredSize.Height() = 0;
  aStatus = NS_FRAME_COMPLETE;

  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    // Set up our kids.  They're already present, on an overflow list, 
    // or there are none so we'll create them now
    MoveOverflowToChildList();
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
      (captionRS->ComputedWidth() + captionRS->ComputedPhysicalMargin().LeftRight() +
       captionRS->ComputedPhysicalBorderPadding().LeftRight());
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
                               innerRS->ComputedPhysicalBorderPadding().LeftRight();
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
  nsHTMLReflowMetrics captionMet(captionRS->GetWritingMode());
  nsSize captionSize;
  nsMargin captionMargin;
  if (mCaptionFrames.NotEmpty()) {
    nsReflowStatus capStatus; // don't let the caption cause incomplete
    OuterDoReflowChild(aPresContext, mCaptionFrames.FirstChild(),
                       *captionRS, captionMet, capStatus);
    captionSize.width = captionMet.Width();
    captionSize.height = captionMet.Height();
    captionMargin = captionRS->ComputedPhysicalMargin();
    // Now that we know the height of the caption, reduce the available height
    // for the table frame if we are height constrained and the caption is above
    // or below the inner table.
    if (NS_UNCONSTRAINEDSIZE != aOuterRS.AvailableHeight()) {
      nscoord captionHeight = 0;
      switch (captionSide) {
        case NS_STYLE_CAPTION_SIDE_TOP:
        case NS_STYLE_CAPTION_SIDE_BOTTOM:
        case NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE:
        case NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE: {
          captionHeight = captionSize.height + captionMargin.TopBottom();
          break;
        }
      }
      innerRS->AvailableHeight() =
        std::max(0, innerRS->AvailableHeight() - captionHeight);
    }
  } else {
    captionSize.SizeTo(0,0);
    captionMargin.SizeTo(0,0,0,0);
  }

  // Then, now that we know how much to reduce the width of the inner
  // table to account for side captions, reflow the inner table.
  nsHTMLReflowMetrics innerMet(innerRS->GetWritingMode());
  OuterDoReflowChild(aPresContext, InnerTableFrame(), *innerRS,
                     innerMet, aStatus);
  nsSize innerSize;
  innerSize.width = innerMet.Width();
  innerSize.height = innerMet.Height();
  nsMargin innerMargin = innerRS->ComputedPhysicalMargin();

  nsSize   containSize = GetContainingBlockSize(aOuterRS);

  // Now that we've reflowed both we can place them.
  // XXXldb Most of the input variables here are now uninitialized!

  // XXX Need to recompute inner table's auto margins for the case of side
  // captions.  (Caption's are broken too, but that should be fixed earlier.)

  if (mCaptionFrames.NotEmpty()) {
    nsPoint captionOrigin;
    GetCaptionOrigin(captionSide, containSize, innerSize, 
                     innerMargin, captionSize, captionMargin, captionOrigin);
    FinishReflowChild(mCaptionFrames.FirstChild(), aPresContext, captionMet,
                      captionRS, captionOrigin.x, captionOrigin.y, 0);
    captionRS->~nsHTMLReflowState();
  }
  // XXX If the height is constrained then we need to check whether
  // everything still fits...

  nsPoint innerOrigin;
  GetInnerOrigin(captionSide, containSize, captionSize, 
                 captionMargin, innerSize, innerMargin, innerOrigin);
  FinishReflowChild(InnerTableFrame(), aPresContext, innerMet, innerRS,
                    innerOrigin.x, innerOrigin.y, 0);
  innerRS->~nsHTMLReflowState();

  nsTableFrame::InvalidateTableFrame(InnerTableFrame(), origInnerRect,
                                     origInnerVisualOverflow, innerFirstReflow);
  if (mCaptionFrames.NotEmpty()) {
    nsTableFrame::InvalidateTableFrame(mCaptionFrames.FirstChild(), origCaptionRect,
                                       origCaptionVisualOverflow,
                                       captionFirstReflow);
  }

  UpdateReflowMetrics(captionSide, aDesiredSize, innerMargin, captionMargin);

  if (GetPrevInFlow()) {
    ReflowOverflowContainerChildren(aPresContext, aOuterRS,
                                    aDesiredSize.mOverflowAreas, 0,
                                    aStatus);
  }

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aOuterRS, aStatus);

  // Return our desired rect

  NS_FRAME_SET_TRUNCATION(aStatus, aOuterRS, aDesiredSize);
}

nsIAtom*
nsTableOuterFrame::GetType() const
{
  return nsGkAtoms::tableOuterFrame;
}

/* ----- global methods ----- */

nsIContent*
nsTableOuterFrame::GetCellAt(uint32_t aRowIdx, uint32_t aColIdx) const
{
  nsTableCellMap* cellMap = InnerTableFrame()->GetCellMap();
  if (!cellMap) {
    return nullptr;
  }

  nsTableCellFrame* cell = cellMap->GetCellInfoAt(aRowIdx, aColIdx);
  if (!cell) {
    return nullptr;
  }

  return cell->GetContent();
}


nsTableOuterFrame*
NS_NewTableOuterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTableOuterFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTableOuterFrame)

#ifdef DEBUG_FRAME_DUMP
nsresult
nsTableOuterFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("TableOuter"), aResult);
}
#endif

