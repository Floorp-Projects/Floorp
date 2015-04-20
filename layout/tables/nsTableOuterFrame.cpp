/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTableOuterFrame.h"

#include "nsFrameManager.h"
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
#include "nsIFrameInlines.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::layout;

#define NO_SIDE 100

/* virtual */ nscoord
nsTableOuterFrame::GetLogicalBaseline(WritingMode aWritingMode) const
{
  nsIFrame* kid = mFrames.FirstChild();
  if (!kid) {
    NS_NOTREACHED("no inner table");
    return nsContainerFrame::GetLogicalBaseline(aWritingMode);
  }

  return kid->GetLogicalBaseline(aWritingMode) +
         kid->BStart(aWritingMode, mRect.width);
}

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
  MOZ_ASSERT(kCaptionList == aListID || aListID == kPrincipalList,
             "unexpected child list");
  MOZ_ASSERT(GetChildList(aListID).IsEmpty(),
             "already have child frames in SetInitialChildList");

  if (kCaptionList == aListID) {
    // the frame constructor already checked for table-caption display type
    mCaptionFrames.SetFrames(aChildList);
  } else {
    MOZ_ASSERT(aChildList.FirstChild() &&
               aChildList.FirstChild() == aChildList.LastChild() &&
               nsGkAtoms::tableFrame == aChildList.FirstChild()->GetType(),
               "expected a single table frame");
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
  MOZ_ASSERT(aFrameList.IsEmpty() ||
             aFrameList.FirstChild()->IsTableCaption(),
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
  MOZ_ASSERT(kCaptionList == aListID, "unexpected child list");
  MOZ_ASSERT(aFrameList.IsEmpty() ||
             aFrameList.FirstChild()->IsTableCaption(),
             "inserting non-caption frame into captionList");
  MOZ_ASSERT(!aPrevFrame || aPrevFrame->GetParent() == this,
             "inserting after sibling frame with different parent");
  mCaptionFrames.InsertFrames(nullptr, aPrevFrame, aFrameList);

  // Reflow the new caption frame. It's already marked dirty, so
  // just tell the pres shell.
  PresContext()->PresShell()->
    FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                     NS_FRAME_HAS_DIRTY_CHILDREN);
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

nsStyleContext*
nsTableOuterFrame::GetParentStyleContext(nsIFrame** aProviderFrame) const
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

  return (*aProviderFrame = InnerTableFrame())->StyleContext();
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
                                  nscoord                  aAvailISize,
                                  LogicalMargin&           aMargin)
{
  // construct a reflow state to compute margin and padding. Auto margins
  // will not be computed at this time.

  // create and init the child reflow state
  // XXX We really shouldn't construct a reflow state to do this.
  WritingMode wm = aChildFrame->GetWritingMode();
  LogicalSize availSize(wm, aAvailISize, aOuterRS.AvailableSize(wm).BSize(wm));
  nsHTMLReflowState childRS(aPresContext, aOuterRS, aChildFrame, availSize,
                            -1, -1, nsHTMLReflowState::CALLER_WILL_INIT);
  InitChildReflowState(*aPresContext, childRS);

  aMargin = childRS.ComputedLogicalMargin();
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
nsTableOuterFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  nscoord width = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                    InnerTableFrame(), nsLayoutUtils::MIN_ISIZE);
  DISPLAY_MIN_WIDTH(this, width);
  if (mCaptionFrames.NotEmpty()) {
    nscoord capWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                           mCaptionFrames.FirstChild(),
                                           nsLayoutUtils::MIN_ISIZE);
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
nsTableOuterFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  nscoord maxWidth;
  DISPLAY_PREF_WIDTH(this, maxWidth);

  maxWidth = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
               InnerTableFrame(), nsLayoutUtils::PREF_ISIZE);
  if (mCaptionFrames.NotEmpty()) {
    uint8_t captionSide = GetCaptionSide();
    switch(captionSide) {
    case NS_STYLE_CAPTION_SIDE_LEFT:
    case NS_STYLE_CAPTION_SIDE_RIGHT:
      {
        nscoord capMin =
          nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                               mCaptionFrames.FirstChild(),
                                               nsLayoutUtils::MIN_ISIZE);
        maxWidth += capMin;
      }
      break;
    default:
      {
        nsLayoutUtils::IntrinsicISizeType iwt;
        if (captionSide == NS_STYLE_CAPTION_SIDE_TOP ||
            captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM) {
          // Don't let the caption's pref width expand the table's pref
          // width.
          iwt = nsLayoutUtils::MIN_ISIZE;
        } else {
          NS_ASSERTION(captionSide == NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE ||
                       captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE,
                       "unexpected caption side");
          iwt = nsLayoutUtils::PREF_ISIZE;
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
  WritingMode wm = offsets.GetWritingMode();
  LogicalSize size =
    aChildFrame->ComputeSize(aRenderingContext,
                  wm, LogicalSize(wm, aCBSize),
                  aAvailableWidth,
                  offsets.ComputedLogicalMargin().Size(wm),
                  offsets.ComputedLogicalBorderPadding().Size(wm) -
                    offsets.ComputedLogicalPadding().Size(wm),
                  offsets.ComputedLogicalPadding().Size(wm),
                  nsIFrame::ComputeSizeFlags::eShrinkWrap);
  if (aMarginResult)
    *aMarginResult = offsets.ComputedLogicalMargin().IStartEnd(wm);
  return size.ISize(wm) + offsets.ComputedLogicalMargin().IStartEnd(wm) +
                      offsets.ComputedLogicalBorderPadding().IStartEnd(wm);
}

/* virtual */
LogicalSize
nsTableOuterFrame::ComputeAutoSize(nsRenderingContext *aRenderingContext,
                                   WritingMode aWM,
                                   const LogicalSize& aCBSize,
                                   nscoord aAvailableISize,
                                   const LogicalSize& aMargin,
                                   const LogicalSize& aBorder,
                                   const LogicalSize& aPadding,
                                   bool aShrinkWrap)
{
  nscoord kidAvailableWidth = aAvailableISize - aMargin.ISize(aWM);
  NS_ASSERTION(aBorder.IsAllZero() && aPadding.IsAllZero(),
               "Table outer frames cannot have borders or paddings");

  // When we're shrink-wrapping, our auto size needs to wrap around the
  // actual size of the table, which (if it is specified as a percent)
  // could be something that is not reflected in our GetMinISize and
  // GetPrefISize.  See bug 349457 for an example.

  // XXX The use of aCBSize.GetPhysicalSize(aWM) below is temporary,
  // until ChildShrinkWrapWidth is updated and width becomes inlineSize.

  // Match the availableWidth logic in Reflow.
  uint8_t captionSide = GetCaptionSide();
  nscoord width;
  if (captionSide == NO_SIDE) {
    width = ChildShrinkWrapWidth(aRenderingContext, InnerTableFrame(),
                                 aCBSize.GetPhysicalSize(aWM),
                                 kidAvailableWidth);
  } else if (captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
             captionSide == NS_STYLE_CAPTION_SIDE_RIGHT) {
    nscoord capWidth = ChildShrinkWrapWidth(aRenderingContext,
                                            mCaptionFrames.FirstChild(),
                                            aCBSize.GetPhysicalSize(aWM),
                                            kidAvailableWidth);
    width = capWidth + ChildShrinkWrapWidth(aRenderingContext,
                                            InnerTableFrame(),
                                            aCBSize.GetPhysicalSize(aWM),
                                            kidAvailableWidth - capWidth);
  } else if (captionSide == NS_STYLE_CAPTION_SIDE_TOP ||
             captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM) {
    nscoord margin;
    width = ChildShrinkWrapWidth(aRenderingContext, InnerTableFrame(),
                                 aCBSize.GetPhysicalSize(aWM),
                                 kidAvailableWidth, &margin);
    nscoord capWidth = ChildShrinkWrapWidth(aRenderingContext,
                                            mCaptionFrames.FirstChild(),
                                            aCBSize.GetPhysicalSize(aWM),
                                            width - margin);
    if (capWidth > width)
      width = capWidth;
  } else {
    NS_ASSERTION(captionSide == NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE ||
                 captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE,
                 "unexpected caption-side");
    width = ChildShrinkWrapWidth(aRenderingContext, InnerTableFrame(),
                                 aCBSize.GetPhysicalSize(aWM),
                                 kidAvailableWidth);
    nscoord capWidth = ChildShrinkWrapWidth(aRenderingContext,
                                            mCaptionFrames.FirstChild(),
                                            aCBSize.GetPhysicalSize(aWM),
                                            kidAvailableWidth);
    if (capWidth > width)
      width = capWidth;
  }

  return LogicalSize(aWM, width, NS_UNCONSTRAINEDSIZE);
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
nsTableOuterFrame::OuterBeginReflowChild(nsPresContext*            aPresContext,
                                         nsIFrame*                 aChildFrame,
                                         const nsHTMLReflowState&  aOuterRS,
                                         Maybe<nsHTMLReflowState>& aChildRS,
                                         nscoord                   aAvailISize)
{
  // work around pixel rounding errors, round down to ensure we don't exceed the avail height in
  WritingMode wm = aChildFrame->GetWritingMode();
  LogicalSize outerSize = aOuterRS.AvailableSize(wm);
  nscoord availBSize = outerSize.BSize(wm);
  if (NS_UNCONSTRAINEDSIZE != availBSize) {
    if (mCaptionFrames.FirstChild() == aChildFrame) {
      availBSize = NS_UNCONSTRAINEDSIZE;
    } else {
      LogicalMargin margin(wm);
      GetChildMargin(aPresContext, aOuterRS, aChildFrame,
                     outerSize.ISize(wm), margin);

      NS_ASSERTION(NS_UNCONSTRAINEDSIZE != margin.BStart(wm),
                   "No unconstrainedsize arithmetic, please");
      availBSize -= margin.BStart(wm);

      NS_ASSERTION(NS_UNCONSTRAINEDSIZE != margin.BEnd(wm),
                   "No unconstrainedsize arithmetic, please");
      availBSize -= margin.BEnd(wm);
    }
  }
  LogicalSize availSize(wm, aAvailISize, availBSize);
  // create and init the child reflow state, using passed-in Maybe<>,
  // so that caller can use it after we return.
  aChildRS.emplace(aPresContext, aOuterRS, aChildFrame, availSize,
                  -1, -1, nsHTMLReflowState::CALLER_WILL_INIT);
  InitChildReflowState(*aPresContext, *aChildRS);

  // see if we need to reset top-of-page due to a caption
  if (aChildRS->mFlags.mIsTopOfPage &&
      mCaptionFrames.FirstChild() == aChildFrame) {
    uint8_t captionSide = GetCaptionSide();
    if (captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM ||
        captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE) {
      aChildRS->mFlags.mIsTopOfPage = false;
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
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsTableOuterFrame");
  DISPLAY_REFLOW(aPresContext, this, aOuterRS, aDesiredSize, aStatus);

  uint8_t captionSide = GetCaptionSide();

  // Initialize out parameters
  aDesiredSize.ClearSize();
  aStatus = NS_FRAME_COMPLETE;

  if (!(GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
    // Set up our kids.  They're already present, on an overflow list, 
    // or there are none so we'll create them now
    MoveOverflowToChildList();
  }

  Maybe<nsHTMLReflowState> captionRS;
  Maybe<nsHTMLReflowState> innerRS;

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
  WritingMode wm;
  if (captionSide == NO_SIDE) {
    // We don't have a caption.
    wm = InnerTableFrame()->GetWritingMode();
    OuterBeginReflowChild(aPresContext, InnerTableFrame(), aOuterRS,
                          innerRS, aOuterRS.ComputedSize(wm).ISize(wm));
  } else if (captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
             captionSide == NS_STYLE_CAPTION_SIDE_RIGHT) {
    // ComputeAutoSize takes care of making side captions small. Compute
    // the caption's size first, and tell the table to fit in what's left.
    wm = mCaptionFrames.FirstChild()->GetWritingMode();
    OuterBeginReflowChild(aPresContext, mCaptionFrames.FirstChild(), aOuterRS,
                          captionRS, aOuterRS.ComputedSize(wm).ISize(wm));
    nscoord innerAvailISize = aOuterRS.ComputedSize(wm).ISize(wm) -
      captionRS->ComputedSizeWithMarginBorderPadding(wm).ISize(wm);
    OuterBeginReflowChild(aPresContext, InnerTableFrame(), aOuterRS,
                          innerRS, innerAvailISize);

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
    wm = InnerTableFrame()->GetWritingMode();
    OuterBeginReflowChild(aPresContext, InnerTableFrame(), aOuterRS,
                          innerRS, aOuterRS.ComputedSize(wm).ISize(wm));
    // It's good that CSS 2.1 says not to include margins, since we
    // can't, since they already been converted so they exactly
    // fill the available width (ignoring the margin on one side if
    // neither are auto).  (We take advantage of that later when we call
    // GetCaptionOrigin, though.)
    nscoord innerBorderWidth =
      innerRS->ComputedSizeWithBorderPadding(wm).ISize(wm);
    OuterBeginReflowChild(aPresContext, mCaptionFrames.FirstChild(), aOuterRS,
                          captionRS, innerBorderWidth);
  } else {
    NS_ASSERTION(captionSide == NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE ||
                 captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE,
                 "unexpected caption-side");
    // Size the table and the caption independently.
    wm = mCaptionFrames.FirstChild()->GetWritingMode();
    OuterBeginReflowChild(aPresContext, mCaptionFrames.FirstChild(), aOuterRS,
                          captionRS, aOuterRS.ComputedSize(wm).ISize(wm));
    wm = InnerTableFrame()->GetWritingMode();
    OuterBeginReflowChild(aPresContext, InnerTableFrame(), aOuterRS,
                          innerRS, aOuterRS.ComputedSize(wm).ISize(wm));
  }

  // First reflow the caption.
  Maybe<nsHTMLReflowMetrics> captionMet;
  nsSize captionSize;
  nsMargin captionMargin;
  if (mCaptionFrames.NotEmpty()) {
    captionMet.emplace(captionRS->GetWritingMode());
    nsReflowStatus capStatus; // don't let the caption cause incomplete
    OuterDoReflowChild(aPresContext, mCaptionFrames.FirstChild(),
                       *captionRS, *captionMet, capStatus);
    captionSize.width = captionMet->Width();
    captionSize.height = captionMet->Height();
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
    FinishReflowChild(mCaptionFrames.FirstChild(), aPresContext, *captionMet,
                      captionRS.ptr(), captionOrigin.x, captionOrigin.y, 0);
    captionRS.reset();
  }
  // XXX If the height is constrained then we need to check whether
  // everything still fits...

  nsPoint innerOrigin;
  GetInnerOrigin(captionSide, containSize, captionSize, 
                 captionMargin, innerSize, innerMargin, innerOrigin);
  FinishReflowChild(InnerTableFrame(), aPresContext, innerMet, innerRS.ptr(),
                    innerOrigin.x, innerOrigin.y, 0);
  innerRS.reset();

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

