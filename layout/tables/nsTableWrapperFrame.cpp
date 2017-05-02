/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTableWrapperFrame.h"

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
nsTableWrapperFrame::GetLogicalBaseline(WritingMode aWritingMode) const
{
  nsIFrame* kid = mFrames.FirstChild();
  if (!kid) {
    NS_NOTREACHED("no inner table");
    return nsContainerFrame::GetLogicalBaseline(aWritingMode);
  }

  return kid->GetLogicalBaseline(aWritingMode) +
         kid->BStart(aWritingMode, mRect.Size());
}

nsTableWrapperFrame::nsTableWrapperFrame(nsStyleContext* aContext)
  : nsContainerFrame(aContext, FrameType::TableWrapper)
{
}

nsTableWrapperFrame::~nsTableWrapperFrame()
{
}

NS_QUERYFRAME_HEAD(nsTableWrapperFrame)
  NS_QUERYFRAME_ENTRY(nsTableWrapperFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

#ifdef ACCESSIBILITY
a11y::AccType
nsTableWrapperFrame::AccessibleType()
{
  return a11y::eHTMLTableType;
}
#endif

void
nsTableWrapperFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  DestroyAbsoluteFrames(aDestructRoot);
  mCaptionFrames.DestroyFramesFrom(aDestructRoot);
  nsContainerFrame::DestroyFrom(aDestructRoot);
}

const nsFrameList&
nsTableWrapperFrame::GetChildList(ChildListID aListID) const
{
  if (aListID == kCaptionList) {
    return mCaptionFrames;
  }

  return nsContainerFrame::GetChildList(aListID);
}

void
nsTableWrapperFrame::GetChildLists(nsTArray<ChildList>* aLists) const
{
  nsContainerFrame::GetChildLists(aLists);
  mCaptionFrames.AppendIfNonempty(aLists, kCaptionList);
}

void
nsTableWrapperFrame::SetInitialChildList(ChildListID     aListID,
                                         nsFrameList&    aChildList)
{
  if (kCaptionList == aListID) {
    // the frame constructor already checked for table-caption display type
    MOZ_ASSERT(mCaptionFrames.IsEmpty(),
               "already have child frames in CaptionList");
    mCaptionFrames.SetFrames(aChildList);
  } else {
    MOZ_ASSERT(kPrincipalList != aListID ||
               (aChildList.FirstChild() &&
                aChildList.FirstChild() == aChildList.LastChild() &&
                aChildList.FirstChild()->IsTableFrame()),
               "expected a single table frame in principal child list");
    nsContainerFrame::SetInitialChildList(aListID, aChildList);
  }
}

void
nsTableWrapperFrame::AppendFrames(ChildListID     aListID,
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
  PresContext()->PresShell()->FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                                               NS_FRAME_HAS_DIRTY_CHILDREN);
}

void
nsTableWrapperFrame::InsertFrames(ChildListID     aListID,
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
  PresContext()->PresShell()->FrameNeedsReflow(this, nsIPresShell::eTreeChange,
                                               NS_FRAME_HAS_DIRTY_CHILDREN);
}

void
nsTableWrapperFrame::RemoveFrame(ChildListID  aListID,
                                 nsIFrame*    aOldFrame)
{
  // We only have two child frames: the inner table and one caption frame.
  // The inner frame can't be removed so this should be the caption
  NS_PRECONDITION(kCaptionList == aListID, "can't remove inner frame");

  if (HasSideCaption()) {
    // The old caption isize had an effect on the inner table isize, so
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
nsTableWrapperFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
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
  set.SortAllByContentOrder(GetContent());
  set.MoveTo(aLists);
}

void
nsTableWrapperFrame::BuildDisplayListForInnerTable(nsDisplayListBuilder*   aBuilder,
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
nsTableWrapperFrame::GetParentStyleContext(nsIFrame** aProviderFrame) const
{
  // The table wrapper frame and the (inner) table frame split the style
  // data by giving the table frame the style context associated with
  // the table content node and creating a style context for the wrapper
  // frame that is a *child* of the table frame's style context,
  // matching the ::-moz-table-wrapper pseudo-element. html.css has a
  // rule that causes that pseudo-element (and thus the wrapper table)
  // to inherit *some* style properties from the table frame.  The
  // children of the table inherit directly from the inner table, and
  // the table wrapper's style context is a leaf.

  return (*aProviderFrame = InnerTableFrame())->StyleContext();
}

// INCREMENTAL REFLOW HELPER FUNCTIONS 

void
nsTableWrapperFrame::InitChildReflowInput(nsPresContext& aPresContext,
                                          ReflowInput&   aReflowInput)
{
  nsMargin collapseBorder;
  nsMargin collapsePadding(0,0,0,0);
  nsMargin* pCollapseBorder  = nullptr;
  nsMargin* pCollapsePadding = nullptr;
  Maybe<LogicalSize> cbSize;
  if (aReflowInput.mFrame == InnerTableFrame()) {
    WritingMode wm = aReflowInput.GetWritingMode();
    if (InnerTableFrame()->IsBorderCollapse()) {
      LogicalMargin border = InnerTableFrame()->GetIncludedOuterBCBorder(wm);
      collapseBorder = border.GetPhysicalMargin(wm);
      pCollapseBorder = &collapseBorder;
      pCollapsePadding = &collapsePadding;
    }
    // Propagate our stored CB size if present, minus any margins.
    if (!HasAnyStateBits(NS_FRAME_OUT_OF_FLOW)) {
      LogicalSize* cb = Properties().Get(GridItemCBSizeProperty());
      if (cb) {
        cbSize.emplace(*cb);
        *cbSize -= aReflowInput.ComputedLogicalMargin().Size(wm);
      }
    }
  }
  aReflowInput.Init(&aPresContext, cbSize.ptrOr(nullptr), pCollapseBorder,
                    pCollapsePadding);
}

// get the margin and padding data. ReflowInput doesn't handle the
// case of auto margins
void
nsTableWrapperFrame::GetChildMargin(nsPresContext*           aPresContext,
                                    const ReflowInput& aOuterRI,
                                    nsIFrame*                aChildFrame,
                                    nscoord                  aAvailISize,
                                    LogicalMargin&           aMargin)
{
  NS_ASSERTION(!aChildFrame->IsTableCaption(),
               "didn't expect caption frame; writing-mode may be wrong!");

  // construct a reflow state to compute margin and padding. Auto margins
  // will not be computed at this time.

  // create and init the child reflow state
  // XXX We really shouldn't construct a reflow state to do this.
  WritingMode wm = aOuterRI.GetWritingMode();
  LogicalSize availSize(wm, aAvailISize, aOuterRI.AvailableSize(wm).BSize(wm));
  ReflowInput childRI(aPresContext, aOuterRI, aChildFrame, availSize,
                            nullptr, ReflowInput::CALLER_WILL_INIT);
  InitChildReflowInput(*aPresContext, childRI);

  aMargin = childRI.ComputedLogicalMargin();
}

static nsSize
GetContainingBlockSize(const ReflowInput& aOuterRI)
{
  nsSize size(0,0);
  const ReflowInput* containRS = aOuterRI.mCBReflowInput;

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
nsTableWrapperFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  nscoord iSize = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                    InnerTableFrame(), nsLayoutUtils::MIN_ISIZE);
  DISPLAY_MIN_WIDTH(this, iSize);
  if (mCaptionFrames.NotEmpty()) {
    nscoord capISize =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                           mCaptionFrames.FirstChild(),
                                           nsLayoutUtils::MIN_ISIZE);
    if (HasSideCaption()) {
      iSize += capISize;
    } else {
      if (capISize > iSize) {
        iSize = capISize;
      }
    }
  }
  return iSize;
}

/* virtual */ nscoord
nsTableWrapperFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  nscoord maxISize;
  DISPLAY_PREF_WIDTH(this, maxISize);

  maxISize = nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
               InnerTableFrame(), nsLayoutUtils::PREF_ISIZE);
  if (mCaptionFrames.NotEmpty()) {
    uint8_t captionSide = GetCaptionSide();
    switch (captionSide) {
    case NS_STYLE_CAPTION_SIDE_LEFT:
    case NS_STYLE_CAPTION_SIDE_RIGHT:
      {
        nscoord capMin =
          nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                               mCaptionFrames.FirstChild(),
                                               nsLayoutUtils::MIN_ISIZE);
        maxISize += capMin;
      }
      break;
    default:
      {
        nsLayoutUtils::IntrinsicISizeType iwt;
        if (captionSide == NS_STYLE_CAPTION_SIDE_TOP ||
            captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM) {
          // Don't let the caption's pref isize expand the table's pref
          // isize.
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
        maxISize = std::max(maxISize, capPref);
      }
      break;
    }
  }
  return maxISize;
}

nscoord
nsTableWrapperFrame::ChildShrinkWrapISize(nsRenderingContext* aRenderingContext,
                                          nsIFrame*           aChildFrame,
                                          WritingMode         aWM,
                                          LogicalSize         aCBSize,
                                          nscoord             aAvailableISize,
                                          nscoord*            aMarginResult) const
{
  AutoMaybeDisableFontInflation an(aChildFrame);

  // For the caption frame, child's WM may differ from the table's main WM.
  WritingMode childWM = aChildFrame->GetWritingMode();

  SizeComputationInput offsets(aChildFrame, aRenderingContext, aWM,
                               aCBSize.ISize(aWM));
  LogicalSize marginSize =
    offsets.ComputedLogicalMargin().Size(childWM).ConvertTo(aWM, childWM);
  LogicalSize paddingSize =
    offsets.ComputedLogicalPadding().Size(childWM).ConvertTo(aWM, childWM);
  LogicalSize bpSize =
    offsets.ComputedLogicalBorderPadding().Size(childWM).ConvertTo(aWM, childWM);

  // Shrink-wrap aChildFrame by default, except if we're a stretched grid item.
  auto flags = ComputeSizeFlags::eShrinkWrap;
  auto parent = GetParent();
  bool isGridItem = parent && parent->IsGridContainerFrame() &&
                    !HasAnyStateBits(NS_FRAME_OUT_OF_FLOW);
  if (MOZ_UNLIKELY(isGridItem) &&
      !StyleMargin()->HasInlineAxisAuto(aWM)) {
    auto inlineAxisAlignment = aWM.IsOrthogonalTo(parent->GetWritingMode()) ?
                     StylePosition()->UsedAlignSelf(parent->StyleContext()) :
                     StylePosition()->UsedJustifySelf(parent->StyleContext());
    if (inlineAxisAlignment == NS_STYLE_ALIGN_NORMAL ||
        inlineAxisAlignment == NS_STYLE_ALIGN_STRETCH) {
      flags = nsIFrame::ComputeSizeFlags::eDefault;
    }
  }

  LogicalSize size =
    aChildFrame->ComputeSize(aRenderingContext, aWM, aCBSize, aAvailableISize,
                             marginSize, bpSize - paddingSize, paddingSize,
                             flags);
  if (aMarginResult) {
    *aMarginResult = offsets.ComputedLogicalMargin().IStartEnd(aWM);
  }
  return size.ISize(aWM) + marginSize.ISize(aWM) + bpSize.ISize(aWM);
}

/* virtual */
LogicalSize
nsTableWrapperFrame::ComputeAutoSize(nsRenderingContext* aRenderingContext,
                                     WritingMode         aWM,
                                     const LogicalSize&  aCBSize,
                                     nscoord             aAvailableISize,
                                     const LogicalSize&  aMargin,
                                     const LogicalSize&  aBorder,
                                     const LogicalSize&  aPadding,
                                     ComputeSizeFlags    aFlags)
{
  nscoord kidAvailableISize = aAvailableISize - aMargin.ISize(aWM);
  NS_ASSERTION(aBorder.IsAllZero() && aPadding.IsAllZero(),
               "Table wrapper frames cannot have borders or paddings");

  // When we're shrink-wrapping, our auto size needs to wrap around the
  // actual size of the table, which (if it is specified as a percent)
  // could be something that is not reflected in our GetMinISize and
  // GetPrefISize.  See bug 349457 for an example.

  // Match the availableISize logic in Reflow.
  uint8_t captionSide = GetCaptionSide();
  nscoord inlineSize;
  if (captionSide == NO_SIDE) {
    inlineSize = ChildShrinkWrapISize(aRenderingContext, InnerTableFrame(), aWM,
                                      aCBSize, kidAvailableISize);
  } else if (captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
             captionSide == NS_STYLE_CAPTION_SIDE_RIGHT) {
    nscoord capISize = ChildShrinkWrapISize(aRenderingContext,
                                            mCaptionFrames.FirstChild(), aWM,
                                            aCBSize, kidAvailableISize);
    inlineSize = capISize + ChildShrinkWrapISize(aRenderingContext,
                                                 InnerTableFrame(), aWM,
                                                 aCBSize,
                                                 kidAvailableISize - capISize);
  } else if (captionSide == NS_STYLE_CAPTION_SIDE_TOP ||
             captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM) {
    nscoord margin;
    inlineSize = ChildShrinkWrapISize(aRenderingContext, InnerTableFrame(), aWM,
                                      aCBSize, kidAvailableISize, &margin);
    nscoord capISize = ChildShrinkWrapISize(aRenderingContext,
                                            mCaptionFrames.FirstChild(), aWM,
                                            aCBSize, inlineSize - margin);
    if (capISize > inlineSize) {
      inlineSize = capISize;
    }
  } else {
    NS_ASSERTION(captionSide == NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE ||
                 captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE,
                 "unexpected caption-side");
    inlineSize = ChildShrinkWrapISize(aRenderingContext, InnerTableFrame(), aWM,
                                      aCBSize, kidAvailableISize);
    nscoord capISize = ChildShrinkWrapISize(aRenderingContext,
                                            mCaptionFrames.FirstChild(), aWM,
                                            aCBSize, kidAvailableISize);
    if (capISize > inlineSize) {
      inlineSize = capISize;
    }
  }

  return LogicalSize(aWM, inlineSize, NS_UNCONSTRAINEDSIZE);
}

uint8_t
nsTableWrapperFrame::GetCaptionSide()
{
  if (mCaptionFrames.NotEmpty()) {
    return mCaptionFrames.FirstChild()->StyleTableBorder()->mCaptionSide;
  }
  else {
    return NO_SIDE; // no caption
  }
}

uint8_t
nsTableWrapperFrame::GetCaptionVerticalAlign()
{
  const nsStyleCoord& va =
    mCaptionFrames.FirstChild()->StyleDisplay()->mVerticalAlign;

  return (va.GetUnit() == eStyleUnit_Enumerated)
           ? va.GetIntValue()
           : NS_STYLE_VERTICAL_ALIGN_TOP;
}

void
nsTableWrapperFrame::SetDesiredSize(uint8_t              aCaptionSide,
                                    const LogicalSize&   aInnerSize,
                                    const LogicalSize&   aCaptionSize,
                                    const LogicalMargin& aInnerMargin,
                                    const LogicalMargin& aCaptionMargin,
                                    nscoord&             aISize,
                                    nscoord&             aBSize,
                                    WritingMode          aWM)
{
  aISize = aBSize = 0;

  // compute the overall inline-size
  switch (aCaptionSide) {
    case NS_STYLE_CAPTION_SIDE_LEFT:
      aISize =
        std::max(aInnerMargin.LineLeft(aWM),
                 aCaptionMargin.IStartEnd(aWM) + aCaptionSize.ISize(aWM)) +
        aInnerSize.ISize(aWM) + aInnerMargin.LineRight(aWM);
      break;
    case NS_STYLE_CAPTION_SIDE_RIGHT:
      aISize =
        std::max(aInnerMargin.LineRight(aWM),
                 aCaptionMargin.IStartEnd(aWM) + aCaptionSize.ISize(aWM)) +
        aInnerSize.ISize(aWM) + aInnerMargin.LineLeft(aWM);
      break;
    default:
      aISize =
        std::max(aInnerMargin.IStartEnd(aWM) + aInnerSize.ISize(aWM),
                 aCaptionMargin.IStartEnd(aWM) + aCaptionSize.ISize(aWM));
      break;
  }

  // compute the overall block-size
  switch (aCaptionSide) {
    case NS_STYLE_CAPTION_SIDE_TOP:
    case NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE:
      aBSize = aInnerSize.BSize(aWM) + aInnerMargin.BEnd(aWM);
      aBSize +=
        std::max(aInnerMargin.BStart(aWM),
                 aCaptionSize.BSize(aWM) + aCaptionMargin.BStartEnd(aWM));
      break;
    case NS_STYLE_CAPTION_SIDE_BOTTOM:
    case NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE:
      aBSize = aInnerSize.BSize(aWM) + aInnerMargin.BStart(aWM);
      aBSize +=
        std::max(aInnerMargin.BEnd(aWM),
                 aCaptionSize.BSize(aWM) + aCaptionMargin.BStartEnd(aWM));
      break;
    case NS_STYLE_CAPTION_SIDE_LEFT:
    case NS_STYLE_CAPTION_SIDE_RIGHT:
      aBSize = aInnerMargin.BStart(aWM);
      aBSize +=
        std::max(aInnerSize.BSize(aWM) + aInnerMargin.BEnd(aWM),
                 aCaptionSize.BSize(aWM) + aCaptionMargin.BEnd(aWM));
      break;
    default:
      NS_ASSERTION(aCaptionSide == NO_SIDE, "unexpected caption side");
      aBSize = aInnerSize.BSize(aWM) + aInnerMargin.BStartEnd(aWM);
      break;
  }

  // negative sizes can upset overflow-area code
  aISize = std::max(aISize, 0);
  aBSize = std::max(aBSize, 0);
}

nsresult
nsTableWrapperFrame::GetCaptionOrigin(uint32_t             aCaptionSide,
                                      const LogicalSize&   aContainBlockSize,
                                      const LogicalSize&   aInnerSize,
                                      const LogicalMargin& aInnerMargin,
                                      const LogicalSize&   aCaptionSize,
                                      LogicalMargin&       aCaptionMargin,
                                      LogicalPoint&        aOrigin,
                                      WritingMode          aWM)
{
  aOrigin.I(aWM) = aOrigin.B(aWM) = 0;
  if ((NS_UNCONSTRAINEDSIZE == aInnerSize.ISize(aWM)) ||
      (NS_UNCONSTRAINEDSIZE == aInnerSize.BSize(aWM)) ||
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.ISize(aWM)) ||
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.BSize(aWM))) {
    return NS_OK;
  }
  if (mCaptionFrames.IsEmpty()) {
    return NS_OK;
  }
  
  NS_ASSERTION(NS_AUTOMARGIN != aCaptionMargin.IStart(aWM) &&
               NS_AUTOMARGIN != aCaptionMargin.BStart(aWM) &&
               NS_AUTOMARGIN != aCaptionMargin.BEnd(aWM),
               "The computed caption margin is auto?");

  // inline-dir computation
  switch (aCaptionSide) {
    case NS_STYLE_CAPTION_SIDE_BOTTOM:
    case NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE:
      aOrigin.I(aWM) = aCaptionMargin.IStart(aWM);
      if (aCaptionSide == NS_STYLE_CAPTION_SIDE_BOTTOM) {
        // We placed the caption using only the table's isize as available
        // isize, and we should position it this way as well.
        aOrigin.I(aWM) += aInnerMargin.IStart(aWM);
      }
      break;
    case NS_STYLE_CAPTION_SIDE_LEFT:
    case NS_STYLE_CAPTION_SIDE_RIGHT:
      aOrigin.I(aWM) = aCaptionMargin.IStart(aWM);
      if (aWM.IsBidiLTR() == (aCaptionSide == NS_STYLE_CAPTION_SIDE_RIGHT)) {
        aOrigin.I(aWM) += aInnerMargin.IStart(aWM) + aInnerSize.ISize(aWM);
      }
      break;
    default: // block-start
      NS_ASSERTION(aCaptionSide == NS_STYLE_CAPTION_SIDE_TOP ||
                   aCaptionSide == NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE,
                   "unexpected caption side");
      aOrigin.I(aWM) = aCaptionMargin.IStart(aWM);
      if (aCaptionSide == NS_STYLE_CAPTION_SIDE_TOP) {
        // We placed the caption using only the table's isize as available
        // isize, and we should position it this way as well.
        aOrigin.I(aWM) += aInnerMargin.IStart(aWM);
      }
      break;
  }
  // block-dir computation
  switch (aCaptionSide) {
    case NS_STYLE_CAPTION_SIDE_RIGHT:
    case NS_STYLE_CAPTION_SIDE_LEFT:
      aOrigin.B(aWM) = aInnerMargin.BStart(aWM);
      switch (GetCaptionVerticalAlign()) {
        case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
          aOrigin.B(aWM) = std::max(0, aInnerMargin.BStart(aWM) +
                                       ((aInnerSize.BSize(aWM) -
                                         aCaptionSize.BSize(aWM)) / 2));
          break;
        case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
          aOrigin.B(aWM) = std::max(0, aInnerMargin.BStart(aWM) +
                                       aInnerSize.BSize(aWM) -
                                       aCaptionSize.BSize(aWM));
          break;
        default:
          break;
      }
      break;
    case NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE:
    case NS_STYLE_CAPTION_SIDE_BOTTOM:
      aOrigin.B(aWM) = aInnerMargin.BStart(aWM) + aInnerSize.BSize(aWM) +
                       aCaptionMargin.BStart(aWM);
      break;
    case NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE:
    case NS_STYLE_CAPTION_SIDE_TOP:
      aOrigin.B(aWM) = aInnerMargin.BStart(aWM) + aCaptionMargin.BStart(aWM);
      break;
    default:
      NS_NOTREACHED("Unknown caption alignment type");
      break;
  }
  return NS_OK;
}

nsresult
nsTableWrapperFrame::GetInnerOrigin(uint32_t             aCaptionSide,
                                    const LogicalSize&   aContainBlockSize,
                                    const LogicalSize&   aCaptionSize,
                                    const LogicalMargin& aCaptionMargin,
                                    const LogicalSize&   aInnerSize,
                                    LogicalMargin&       aInnerMargin,
                                    LogicalPoint&        aOrigin,
                                    WritingMode          aWM)
{
  NS_ASSERTION(NS_AUTOMARGIN != aCaptionMargin.IStart(aWM) &&
               NS_AUTOMARGIN != aCaptionMargin.IEnd(aWM),
               "The computed caption margin is auto?");
  NS_ASSERTION(NS_AUTOMARGIN != aInnerMargin.IStart(aWM) &&
               NS_AUTOMARGIN != aInnerMargin.IEnd(aWM) &&
               NS_AUTOMARGIN != aInnerMargin.BStart(aWM) &&
               NS_AUTOMARGIN != aInnerMargin.BEnd(aWM),
               "The computed inner margin is auto?");
  
  aOrigin.I(aWM) = aOrigin.B(aWM) = 0;
  if ((NS_UNCONSTRAINEDSIZE == aInnerSize.ISize(aWM)) ||
      (NS_UNCONSTRAINEDSIZE == aInnerSize.BSize(aWM)) ||
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.ISize(aWM)) ||
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.BSize(aWM))) {
    return NS_OK;
  }

  nscoord minCapISize =
    aCaptionSize.ISize(aWM) + aCaptionMargin.IStartEnd(aWM);

  // inline-dir computation
  switch (aCaptionSide) {
    case NS_STYLE_CAPTION_SIDE_LEFT:
    case NS_STYLE_CAPTION_SIDE_RIGHT:
      if (aWM.IsBidiLTR() == (aCaptionSide == NS_STYLE_CAPTION_SIDE_LEFT)) {
        if (aInnerMargin.IStart(aWM) < minCapISize) {
          // shift the inner table to get some place for the caption
          aInnerMargin.IEnd(aWM) += aInnerMargin.IStart(aWM) - minCapISize;
          aInnerMargin.IEnd(aWM)  = std::max(0, aInnerMargin.IEnd(aWM));
          aInnerMargin.IStart(aWM) = minCapISize;
        }
      }
      aOrigin.I(aWM) = aInnerMargin.IStart(aWM);
      break;
    default:
      NS_ASSERTION(aCaptionSide == NS_STYLE_CAPTION_SIDE_TOP ||
                   aCaptionSide == NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE ||
                   aCaptionSide == NS_STYLE_CAPTION_SIDE_BOTTOM ||
                   aCaptionSide == NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE ||
                   aCaptionSide == NO_SIDE,
                   "unexpected caption side");
      aOrigin.I(aWM) = aInnerMargin.IStart(aWM);
      break;
  }
  
  // block-dir computation
  switch (aCaptionSide) {
    case NS_STYLE_CAPTION_SIDE_BOTTOM:
    case NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE:
      aOrigin.B(aWM) = aInnerMargin.BStart(aWM);
      break;
    case NS_STYLE_CAPTION_SIDE_LEFT:
    case NS_STYLE_CAPTION_SIDE_RIGHT:
      aOrigin.B(aWM) = aInnerMargin.BStart(aWM);
      switch (GetCaptionVerticalAlign()) {
        case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
          aOrigin.B(aWM) =
            std::max(aInnerMargin.BStart(aWM),
                     (aCaptionSize.BSize(aWM) - aInnerSize.BSize(aWM)) / 2);
          break;
        case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
          aOrigin.B(aWM) =
            std::max(aInnerMargin.BStart(aWM),
                     aCaptionSize.BSize(aWM) - aInnerSize.BSize(aWM));
          break;
        default:
          break;
      }
      break;
    case NO_SIDE:
    case NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE:
    case NS_STYLE_CAPTION_SIDE_TOP:
      aOrigin.B(aWM) = aInnerMargin.BStart(aWM) + aCaptionSize.BSize(aWM) +
                       aCaptionMargin.BStartEnd(aWM);
      break;
    default:
      NS_NOTREACHED("Unknown caption alignment type");
      break;
  }
  return NS_OK;
}

void
nsTableWrapperFrame::OuterBeginReflowChild(nsPresContext*            aPresContext,
                                           nsIFrame*                 aChildFrame,
                                           const ReflowInput&  aOuterRI,
                                           Maybe<ReflowInput>& aChildRI,
                                           nscoord                   aAvailISize)
{
  // work around pixel rounding errors, round down to ensure we don't exceed the avail height in
  WritingMode wm = aChildFrame->GetWritingMode();
  LogicalSize outerSize = aOuterRI.AvailableSize(wm);
  nscoord availBSize = outerSize.BSize(wm);
  if (NS_UNCONSTRAINEDSIZE != availBSize) {
    if (mCaptionFrames.FirstChild() == aChildFrame) {
      availBSize = NS_UNCONSTRAINEDSIZE;
    } else {
      LogicalMargin margin(wm);
      GetChildMargin(aPresContext, aOuterRI, aChildFrame,
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
  aChildRI.emplace(aPresContext, aOuterRI, aChildFrame, availSize,
                  nullptr, ReflowInput::CALLER_WILL_INIT);
  InitChildReflowInput(*aPresContext, *aChildRI);

  // see if we need to reset top-of-page due to a caption
  if (aChildRI->mFlags.mIsTopOfPage &&
      mCaptionFrames.FirstChild() == aChildFrame) {
    uint8_t captionSide = GetCaptionSide();
    if (captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM ||
        captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE) {
      aChildRI->mFlags.mIsTopOfPage = false;
    }
  }
}

void
nsTableWrapperFrame::OuterDoReflowChild(nsPresContext*             aPresContext,
                                        nsIFrame*                  aChildFrame,
                                        const ReflowInput&   aChildRI,
                                        ReflowOutput&       aMetrics,
                                        nsReflowStatus&            aStatus)
{
  // Using zero as containerSize here because we want consistency between
  // the GetLogicalPosition and ReflowChild calls, to avoid unnecessarily
  // changing the frame's coordinates; but we don't yet know its final
  // position anyway so the actual value is unimportant.
  const nsSize zeroCSize;
  WritingMode wm = aChildRI.GetWritingMode();

  // Use the current position as a best guess for placement.
  LogicalPoint childPt = aChildFrame->GetLogicalPosition(wm, zeroCSize);
  uint32_t flags = NS_FRAME_NO_MOVE_FRAME;

  // We don't want to delete our next-in-flow's child if it's an inner table
  // frame, because table wrapper frames always assume that their inner table
  // frames don't go away. If a table wrapper frame is removed because it is
  // a next-in-flow of an already complete table wrapper frame, then it will
  // take care of removing it's inner table frame.
  if (aChildFrame == InnerTableFrame()) {
    flags |= NS_FRAME_NO_DELETE_NEXT_IN_FLOW_CHILD;
  }

  ReflowChild(aChildFrame, aPresContext, aMetrics, aChildRI,
              wm, childPt, zeroCSize, flags, aStatus);
}

void
nsTableWrapperFrame::UpdateOverflowAreas(ReflowOutput& aMet)
{
  aMet.SetOverflowAreasToDesiredBounds();
  ConsiderChildOverflow(aMet.mOverflowAreas, InnerTableFrame());
  if (mCaptionFrames.NotEmpty()) {
    ConsiderChildOverflow(aMet.mOverflowAreas, mCaptionFrames.FirstChild());
  }
}

void
nsTableWrapperFrame::Reflow(nsPresContext*           aPresContext,
                            ReflowOutput&     aDesiredSize,
                            const ReflowInput& aOuterRI,
                            nsReflowStatus&          aStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsTableWrapperFrame");
  DISPLAY_REFLOW(aPresContext, this, aOuterRI, aDesiredSize, aStatus);

  // Initialize out parameters
  aDesiredSize.ClearSize();
  aStatus.Reset();

  if (!HasAnyStateBits(NS_FRAME_FIRST_REFLOW)) {
    // Set up our kids.  They're already present, on an overflow list,
    // or there are none so we'll create them now
    MoveOverflowToChildList();
  }

  Maybe<ReflowInput> captionRI;
  Maybe<ReflowInput> innerRI;

  nsRect origInnerRect = InnerTableFrame()->GetRect();
  nsRect origInnerVisualOverflow = InnerTableFrame()->GetVisualOverflowRect();
  bool innerFirstReflow =
    InnerTableFrame()->HasAnyStateBits(NS_FRAME_FIRST_REFLOW);
  nsRect origCaptionRect;
  nsRect origCaptionVisualOverflow;
  bool captionFirstReflow = false;
  if (mCaptionFrames.NotEmpty()) {
    origCaptionRect = mCaptionFrames.FirstChild()->GetRect();
    origCaptionVisualOverflow =
      mCaptionFrames.FirstChild()->GetVisualOverflowRect();
    captionFirstReflow =
      mCaptionFrames.FirstChild()->HasAnyStateBits(NS_FRAME_FIRST_REFLOW);
  }
  
  // ComputeAutoSize has to match this logic.
  WritingMode wm = aOuterRI.GetWritingMode();
  uint8_t captionSide = GetCaptionSide();
  WritingMode captionWM = wm; // will be changed below if necessary

  if (captionSide == NO_SIDE) {
    // We don't have a caption.
    OuterBeginReflowChild(aPresContext, InnerTableFrame(), aOuterRI,
                          innerRI, aOuterRI.ComputedSize(wm).ISize(wm));
  } else if (captionSide == NS_STYLE_CAPTION_SIDE_LEFT ||
             captionSide == NS_STYLE_CAPTION_SIDE_RIGHT) {
    // ComputeAutoSize takes care of making side captions small. Compute
    // the caption's size first, and tell the table to fit in what's left.
    OuterBeginReflowChild(aPresContext, mCaptionFrames.FirstChild(), aOuterRI,
                          captionRI, aOuterRI.ComputedSize(wm).ISize(wm));
    captionWM = captionRI->GetWritingMode();
    nscoord innerAvailISize = aOuterRI.ComputedSize(wm).ISize(wm) -
      captionRI->ComputedSizeWithMarginBorderPadding(wm).ISize(wm);
    OuterBeginReflowChild(aPresContext, InnerTableFrame(), aOuterRI,
                          innerRI, innerAvailISize);
  } else if (captionSide == NS_STYLE_CAPTION_SIDE_TOP ||
             captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM) {
    // Compute the table's size first, and then prevent the caption from
    // being larger in the inline dir unless it has to be.
    //
    // Note that CSS 2.1 (but not 2.0) says:
    //   The width of the anonymous box is the border-edge width of the
    //   table box inside it
    // We don't actually make our anonymous box that isize (if we did,
    // it would break 'auto' margins), but this effectively does that.
    OuterBeginReflowChild(aPresContext, InnerTableFrame(), aOuterRI,
                          innerRI, aOuterRI.ComputedSize(wm).ISize(wm));
    // It's good that CSS 2.1 says not to include margins, since we
    // can't, since they already been converted so they exactly
    // fill the available isize (ignoring the margin on one side if
    // neither are auto).  (We take advantage of that later when we call
    // GetCaptionOrigin, though.)
    nscoord innerBorderISize =
      innerRI->ComputedSizeWithBorderPadding(wm).ISize(wm);
    OuterBeginReflowChild(aPresContext, mCaptionFrames.FirstChild(), aOuterRI,
                          captionRI, innerBorderISize);
    captionWM = captionRI->GetWritingMode();
  } else {
    NS_ASSERTION(captionSide == NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE ||
                 captionSide == NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE,
                 "unexpected caption-side");
    // Size the table and the caption independently.
    captionWM = mCaptionFrames.FirstChild()->GetWritingMode();
    OuterBeginReflowChild(aPresContext, mCaptionFrames.FirstChild(),
                          aOuterRI, captionRI,
                          aOuterRI.ComputedSize(captionWM).ISize(captionWM));
    OuterBeginReflowChild(aPresContext, InnerTableFrame(), aOuterRI,
                          innerRI, aOuterRI.ComputedSize(wm).ISize(wm));
  }

  // First reflow the caption.
  Maybe<ReflowOutput> captionMet;
  LogicalSize captionSize(wm);
  LogicalMargin captionMargin(wm);
  if (mCaptionFrames.NotEmpty()) {
    captionMet.emplace(wm);
    nsReflowStatus capStatus; // don't let the caption cause incomplete
    OuterDoReflowChild(aPresContext, mCaptionFrames.FirstChild(),
                       *captionRI, *captionMet, capStatus);
    captionSize.ISize(wm) = captionMet->ISize(wm);
    captionSize.BSize(wm) = captionMet->BSize(wm);
    captionMargin =
      captionRI->ComputedLogicalMargin().ConvertTo(wm, captionWM);
    // Now that we know the bsize of the caption, reduce the available bsize
    // for the table frame if we are bsize constrained and the caption is above
    // or below the inner table.  Also reduce the CB size that we store for
    // our children in case we're a grid item, by the same amount.
    LogicalSize* cbSize = Properties().Get(GridItemCBSizeProperty());
    if (NS_UNCONSTRAINEDSIZE != aOuterRI.AvailableBSize() || cbSize) {
      nscoord captionBSize = 0;
      nscoord captionISize = 0;
      switch (captionSide) {
        case NS_STYLE_CAPTION_SIDE_TOP:
        case NS_STYLE_CAPTION_SIDE_BOTTOM:
        case NS_STYLE_CAPTION_SIDE_TOP_OUTSIDE:
        case NS_STYLE_CAPTION_SIDE_BOTTOM_OUTSIDE:
          captionBSize = captionSize.BSize(wm) + captionMargin.BStartEnd(wm);
          break;
        case NS_STYLE_CAPTION_SIDE_LEFT:
        case NS_STYLE_CAPTION_SIDE_RIGHT:
          captionISize = captionSize.ISize(wm) + captionMargin.IStartEnd(wm);
          break;
      }
      if (NS_UNCONSTRAINEDSIZE != aOuterRI.AvailableBSize()) {
        innerRI->AvailableBSize() =
          std::max(0, innerRI->AvailableBSize() - captionBSize);
      }
      if (cbSize) {
        // Shrink the CB size by the size reserved for the caption.
        LogicalSize oldCBSize = *cbSize;
        cbSize->ISize(wm) = std::max(0, cbSize->ISize(wm) - captionISize);
        cbSize->BSize(wm) = std::max(0, cbSize->BSize(wm) - captionBSize);
        if (oldCBSize != *cbSize) {
          // Reset the inner table's ReflowInput to stretch it to the new size.
          innerRI.reset();
          OuterBeginReflowChild(aPresContext, InnerTableFrame(), aOuterRI,
                                innerRI, aOuterRI.ComputedSize(wm).ISize(wm));
        }
      }
    }
  }

  // Then, now that we know how much to reduce the isize of the inner
  // table to account for side captions, reflow the inner table.
  ReflowOutput innerMet(innerRI->GetWritingMode());
  OuterDoReflowChild(aPresContext, InnerTableFrame(), *innerRI,
                     innerMet, aStatus);
  LogicalSize innerSize(wm, innerMet.ISize(wm), innerMet.BSize(wm));
  LogicalMargin innerMargin = innerRI->ComputedLogicalMargin();

  LogicalSize containSize(wm, GetContainingBlockSize(aOuterRI));

  // Now that we've reflowed both we can place them.
  // XXXldb Most of the input variables here are now uninitialized!

  // XXX Need to recompute inner table's auto margins for the case of side
  // captions.  (Caption's are broken too, but that should be fixed earlier.)

  // Compute the desiredSize so that we can use it as the containerSize
  // for the FinishReflowChild calls below.
  LogicalSize desiredSize(wm);
  SetDesiredSize(captionSide, innerSize, captionSize,
                 innerMargin, captionMargin,
                 desiredSize.ISize(wm), desiredSize.BSize(wm), wm);
  aDesiredSize.SetSize(wm, desiredSize);
  nsSize containerSize = aDesiredSize.PhysicalSize();
  // XXX It's possible for this to be NS_UNCONSTRAINEDSIZE, which will result
  // in assertions from FinishReflowChild.

  if (mCaptionFrames.NotEmpty()) {
    LogicalPoint captionOrigin(wm);
    GetCaptionOrigin(captionSide, containSize, innerSize, innerMargin,
                     captionSize, captionMargin, captionOrigin, wm);
    FinishReflowChild(mCaptionFrames.FirstChild(), aPresContext, *captionMet,
                      captionRI.ptr(), wm, captionOrigin, containerSize, 0);
    captionRI.reset();
  }
  // XXX If the bsize is constrained then we need to check whether
  // everything still fits...

  LogicalPoint innerOrigin(wm);
  GetInnerOrigin(captionSide, containSize, captionSize, captionMargin,
                 innerSize, innerMargin, innerOrigin, wm);
  FinishReflowChild(InnerTableFrame(), aPresContext, innerMet, innerRI.ptr(),
                    wm, innerOrigin, containerSize, 0);
  innerRI.reset();

  nsTableFrame::InvalidateTableFrame(InnerTableFrame(), origInnerRect,
                                     origInnerVisualOverflow,
                                     innerFirstReflow);
  if (mCaptionFrames.NotEmpty()) {
    nsTableFrame::InvalidateTableFrame(mCaptionFrames.FirstChild(),
                                       origCaptionRect,
                                       origCaptionVisualOverflow,
                                       captionFirstReflow);
  }

  UpdateOverflowAreas(aDesiredSize);

  if (GetPrevInFlow()) {
    ReflowOverflowContainerChildren(aPresContext, aOuterRI,
                                    aDesiredSize.mOverflowAreas, 0,
                                    aStatus);
  }

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aOuterRI, aStatus);

  // Return our desired rect

  NS_FRAME_SET_TRUNCATION(aStatus, aOuterRI, aDesiredSize);
}

/* ----- global methods ----- */

nsIContent*
nsTableWrapperFrame::GetCellAt(uint32_t aRowIdx, uint32_t aColIdx) const
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


nsTableWrapperFrame*
NS_NewTableWrapperFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsTableWrapperFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsTableWrapperFrame)

#ifdef DEBUG_FRAME_DUMP
nsresult
nsTableWrapperFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("TableWrapper"), aResult);
}
#endif

