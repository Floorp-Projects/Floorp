/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTableWrapperFrame.h"

#include "mozilla/ComputedStyle.h"
#include "mozilla/PresShell.h"
#include "nsFrameManager.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "prinrval.h"
#include "nsGkAtoms.h"
#include "nsHTMLParts.h"
#include "nsDisplayList.h"
#include "nsLayoutUtils.h"
#include "nsIFrameInlines.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::layout;

/* virtual */
nscoord nsTableWrapperFrame::GetLogicalBaseline(
    WritingMode aWritingMode) const {
  if (StyleDisplay()->IsContainLayout()) {
    // We have no baseline. Fall back to the inherited impl which is
    // appropriate for this situation.
    return nsContainerFrame::GetLogicalBaseline(aWritingMode);
  }

  nsIFrame* kid = mFrames.FirstChild();
  if (!kid) {
    MOZ_ASSERT_UNREACHABLE("no inner table");
    return nsContainerFrame::GetLogicalBaseline(aWritingMode);
  }

  return kid->GetLogicalBaseline(aWritingMode) +
         kid->BStart(aWritingMode, mRect.Size());
}

nsTableWrapperFrame::nsTableWrapperFrame(ComputedStyle* aStyle,
                                         nsPresContext* aPresContext,
                                         ClassID aID)
    : nsContainerFrame(aStyle, aPresContext, aID) {}

nsTableWrapperFrame::~nsTableWrapperFrame() = default;

NS_QUERYFRAME_HEAD(nsTableWrapperFrame)
  NS_QUERYFRAME_ENTRY(nsTableWrapperFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

#ifdef ACCESSIBILITY
a11y::AccType nsTableWrapperFrame::AccessibleType() {
  return a11y::eHTMLTableType;
}
#endif

void nsTableWrapperFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                      PostDestroyData& aPostDestroyData) {
  DestroyAbsoluteFrames(aDestructRoot, aPostDestroyData);
  mCaptionFrames.DestroyFramesFrom(aDestructRoot, aPostDestroyData);
  nsContainerFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

const nsFrameList& nsTableWrapperFrame::GetChildList(
    ChildListID aListID) const {
  if (aListID == kCaptionList) {
    return mCaptionFrames;
  }

  return nsContainerFrame::GetChildList(aListID);
}

void nsTableWrapperFrame::GetChildLists(nsTArray<ChildList>* aLists) const {
  nsContainerFrame::GetChildLists(aLists);
  mCaptionFrames.AppendIfNonempty(aLists, kCaptionList);
}

void nsTableWrapperFrame::SetInitialChildList(ChildListID aListID,
                                              nsFrameList& aChildList) {
  if (kCaptionList == aListID) {
#ifdef DEBUG
    nsIFrame::VerifyDirtyBitSet(aChildList);
    for (nsIFrame* f : aChildList) {
      MOZ_ASSERT(f->GetParent() == this, "Unexpected parent");
    }
#endif
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

void nsTableWrapperFrame::AppendFrames(ChildListID aListID,
                                       nsFrameList& aFrameList) {
  // We only have two child frames: the inner table and a caption frame.
  // The inner frame is provided when we're initialized, and it cannot change
  MOZ_ASSERT(kCaptionList == aListID, "unexpected child list");
  MOZ_ASSERT(aFrameList.IsEmpty() || aFrameList.FirstChild()->IsTableCaption(),
             "appending non-caption frame to captionList");
  mCaptionFrames.AppendFrames(nullptr, aFrameList);

  // Reflow the new caption frame. It's already marked dirty, so
  // just tell the pres shell.
  PresShell()->FrameNeedsReflow(this, IntrinsicDirty::TreeChange,
                                NS_FRAME_HAS_DIRTY_CHILDREN);
  // The presence of caption frames makes us sort our display
  // list differently, so mark us as changed for the new
  // ordering.
  MarkNeedsDisplayItemRebuild();
}

void nsTableWrapperFrame::InsertFrames(
    ChildListID aListID, nsIFrame* aPrevFrame,
    const nsLineList::iterator* aPrevFrameLine, nsFrameList& aFrameList) {
  MOZ_ASSERT(kCaptionList == aListID, "unexpected child list");
  MOZ_ASSERT(aFrameList.IsEmpty() || aFrameList.FirstChild()->IsTableCaption(),
             "inserting non-caption frame into captionList");
  MOZ_ASSERT(!aPrevFrame || aPrevFrame->GetParent() == this,
             "inserting after sibling frame with different parent");
  mCaptionFrames.InsertFrames(nullptr, aPrevFrame, aFrameList);

  // Reflow the new caption frame. It's already marked dirty, so
  // just tell the pres shell.
  PresShell()->FrameNeedsReflow(this, IntrinsicDirty::TreeChange,
                                NS_FRAME_HAS_DIRTY_CHILDREN);
  MarkNeedsDisplayItemRebuild();
}

void nsTableWrapperFrame::RemoveFrame(ChildListID aListID,
                                      nsIFrame* aOldFrame) {
  // We only have two child frames: the inner table and one caption frame.
  // The inner frame can't be removed so this should be the caption
  MOZ_ASSERT(kCaptionList == aListID, "can't remove inner frame");

  if (HasSideCaption()) {
    // The old caption isize had an effect on the inner table isize, so
    // we're going to need to reflow it. Mark it dirty
    InnerTableFrame()->MarkSubtreeDirty();
  }

  // Remove the frame and destroy it
  mCaptionFrames.DestroyFrame(aOldFrame);

  PresShell()->FrameNeedsReflow(this, IntrinsicDirty::TreeChange,
                                NS_FRAME_HAS_DIRTY_CHILDREN);
  MarkNeedsDisplayItemRebuild();
}

void nsTableWrapperFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                           const nsDisplayListSet& aLists) {
  // No border or background is painted because they belong to the inner table.
  // The outline belongs to the wrapper frame so it can contain the caption.

  // If there's no caption, take a short cut to avoid having to create
  // the special display list set and then sort it.
  if (mCaptionFrames.IsEmpty()) {
    BuildDisplayListForInnerTable(aBuilder, aLists);
    DisplayOutline(aBuilder, aLists);
    return;
  }

  nsDisplayListCollection set(aBuilder);
  BuildDisplayListForInnerTable(aBuilder, set);

  nsDisplayListSet captionSet(set, set.BlockBorderBackgrounds());
  BuildDisplayListForChild(aBuilder, mCaptionFrames.FirstChild(), captionSet);

  // Now we have to sort everything by content order, since the caption
  // may be somewhere inside the table.
  // We don't sort BlockBorderBackgrounds and BorderBackgrounds because the
  // display items in those lists should stay out of content order in order to
  // follow the rules in https://www.w3.org/TR/CSS21/zindex.html#painting-order
  // and paint the caption background after all of the rest.
  set.Floats()->SortByContentOrder(GetContent());
  set.Content()->SortByContentOrder(GetContent());
  set.PositionedDescendants()->SortByContentOrder(GetContent());
  set.Outlines()->SortByContentOrder(GetContent());
  set.MoveTo(aLists);

  DisplayOutline(aBuilder, aLists);
}

void nsTableWrapperFrame::BuildDisplayListForInnerTable(
    nsDisplayListBuilder* aBuilder, const nsDisplayListSet& aLists) {
  // Just paint the regular children, but the children's background is our
  // true background (there should only be one, the real table)
  nsIFrame* kid = mFrames.FirstChild();
  // The children should be in content order
  while (kid) {
    BuildDisplayListForChild(aBuilder, kid, aLists);
    kid = kid->GetNextSibling();
  }
}

ComputedStyle* nsTableWrapperFrame::GetParentComputedStyle(
    nsIFrame** aProviderFrame) const {
  // The table wrapper frame and the (inner) table frame split the style
  // data by giving the table frame the ComputedStyle associated with
  // the table content node and creating a ComputedStyle for the wrapper
  // frame that is a *child* of the table frame's ComputedStyle,
  // matching the ::-moz-table-wrapper pseudo-element. html.css has a
  // rule that causes that pseudo-element (and thus the wrapper table)
  // to inherit *some* style properties from the table frame.  The
  // children of the table inherit directly from the inner table, and
  // the table wrapper's ComputedStyle is a leaf.

  return (*aProviderFrame = InnerTableFrame())->Style();
}

static nsSize GetContainingBlockSize(const ReflowInput& aOuterRI) {
  nsSize size(0, 0);
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

/* virtual */
nscoord nsTableWrapperFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord iSize = nsLayoutUtils::IntrinsicForContainer(
      aRenderingContext, InnerTableFrame(), IntrinsicISizeType::MinISize);
  DISPLAY_MIN_INLINE_SIZE(this, iSize);
  if (mCaptionFrames.NotEmpty()) {
    nscoord capISize = nsLayoutUtils::IntrinsicForContainer(
        aRenderingContext, mCaptionFrames.FirstChild(),
        IntrinsicISizeType::MinISize);
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

/* virtual */
nscoord nsTableWrapperFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord maxISize;
  DISPLAY_PREF_INLINE_SIZE(this, maxISize);

  maxISize = nsLayoutUtils::IntrinsicForContainer(
      aRenderingContext, InnerTableFrame(), IntrinsicISizeType::PrefISize);
  if (Maybe<StyleCaptionSide> captionSide = GetCaptionSide()) {
    switch (*captionSide) {
      case StyleCaptionSide::Left:
      case StyleCaptionSide::Right: {
        nscoord capMin = nsLayoutUtils::IntrinsicForContainer(
            aRenderingContext, mCaptionFrames.FirstChild(),
            IntrinsicISizeType::MinISize);
        maxISize += capMin;
      } break;
      default: {
        IntrinsicISizeType iwt;
        if (*captionSide == StyleCaptionSide::Top ||
            *captionSide == StyleCaptionSide::Bottom) {
          // Don't let the caption's pref isize expand the table's pref
          // isize.
          iwt = IntrinsicISizeType::MinISize;
        } else {
          MOZ_ASSERT(*captionSide == StyleCaptionSide::TopOutside ||
                         *captionSide == StyleCaptionSide::BottomOutside,
                     "unexpected caption side");
          iwt = IntrinsicISizeType::PrefISize;
        }
        nscoord capPref = nsLayoutUtils::IntrinsicForContainer(
            aRenderingContext, mCaptionFrames.FirstChild(), iwt);
        maxISize = std::max(maxISize, capPref);
      } break;
    }
  }
  return maxISize;
}

nscoord nsTableWrapperFrame::ChildShrinkWrapISize(
    gfxContext* aRenderingContext, nsIFrame* aChildFrame, WritingMode aWM,
    LogicalSize aCBSize, nscoord aAvailableISize,
    const StyleSizeOverrides& aSizeOverrides, nscoord* aMarginResult) const {
  AutoMaybeDisableFontInflation an(aChildFrame);

  SizeComputationInput offsets(aChildFrame, aRenderingContext, aWM,
                               aCBSize.ISize(aWM));
  LogicalSize marginSize = offsets.ComputedLogicalMargin(aWM).Size(aWM);
  LogicalSize bpSize = offsets.ComputedLogicalBorderPadding(aWM).Size(aWM);

  // Shrink-wrap aChildFrame by default, except if we're a stretched grid item.
  ComputeSizeFlags flags(ComputeSizeFlag::ShrinkWrap);
  if (MOZ_UNLIKELY(IsGridItem()) && !StyleMargin()->HasInlineAxisAuto(aWM)) {
    const auto* parent = GetParent();
    auto inlineAxisAlignment =
        aWM.IsOrthogonalTo(parent->GetWritingMode())
            ? StylePosition()->UsedAlignSelf(parent->Style())._0
            : StylePosition()->UsedJustifySelf(parent->Style())._0;
    if (inlineAxisAlignment == StyleAlignFlags::NORMAL ||
        inlineAxisAlignment == StyleAlignFlags::STRETCH) {
      flags -= ComputeSizeFlag::ShrinkWrap;
    }
  }

  auto size =
      aChildFrame->ComputeSize(aRenderingContext, aWM, aCBSize, aAvailableISize,
                               marginSize, bpSize, aSizeOverrides, flags);
  if (aMarginResult) {
    *aMarginResult = offsets.ComputedLogicalMargin(aWM).IStartEnd(aWM);
  }
  return size.mLogicalSize.ISize(aWM) + marginSize.ISize(aWM) +
         bpSize.ISize(aWM);
}

/* virtual */
LogicalSize nsTableWrapperFrame::ComputeAutoSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorderPadding, const StyleSizeOverrides& aSizeOverrides,
    ComputeSizeFlags aFlags) {
  nscoord kidAvailableISize = aAvailableISize - aMargin.ISize(aWM);
  NS_ASSERTION(aBorderPadding.IsAllZero(),
               "Table wrapper frames cannot have borders or paddings");

  // When we're shrink-wrapping, our auto size needs to wrap around the
  // actual size of the table, which (if it is specified as a percent)
  // could be something that is not reflected in our GetMinISize and
  // GetPrefISize.  See bug 349457 for an example.

  // Match the availableISize logic in Reflow.
  Maybe<StyleCaptionSide> captionSide = GetCaptionSide();
  nscoord inlineSize;
  if (!captionSide) {
    inlineSize =
        ChildShrinkWrapISize(aRenderingContext, InnerTableFrame(), aWM, aCBSize,
                             kidAvailableISize, aSizeOverrides);
  } else if (*captionSide == StyleCaptionSide::Left ||
             *captionSide == StyleCaptionSide::Right) {
    nscoord capISize =
        ChildShrinkWrapISize(aRenderingContext, mCaptionFrames.FirstChild(),
                             aWM, aCBSize, kidAvailableISize, aSizeOverrides);
    inlineSize =
        capISize +
        ChildShrinkWrapISize(aRenderingContext, InnerTableFrame(), aWM, aCBSize,
                             kidAvailableISize - capISize, aSizeOverrides);
  } else if (*captionSide == StyleCaptionSide::Top ||
             *captionSide == StyleCaptionSide::Bottom) {
    nscoord margin;
    inlineSize =
        ChildShrinkWrapISize(aRenderingContext, InnerTableFrame(), aWM, aCBSize,
                             kidAvailableISize, aSizeOverrides, &margin);
    nscoord capISize =
        ChildShrinkWrapISize(aRenderingContext, mCaptionFrames.FirstChild(),
                             aWM, aCBSize, inlineSize - margin, aSizeOverrides);
    if (capISize > inlineSize) {
      inlineSize = capISize;
    }
  } else {
    MOZ_ASSERT(*captionSide == StyleCaptionSide::TopOutside ||
                   *captionSide == StyleCaptionSide::BottomOutside,
               "unexpected caption-side");
    inlineSize =
        ChildShrinkWrapISize(aRenderingContext, InnerTableFrame(), aWM, aCBSize,
                             kidAvailableISize, aSizeOverrides);
    nscoord capISize =
        ChildShrinkWrapISize(aRenderingContext, mCaptionFrames.FirstChild(),
                             aWM, aCBSize, kidAvailableISize, aSizeOverrides);
    if (capISize > inlineSize) {
      inlineSize = capISize;
    }
  }

  return LogicalSize(aWM, inlineSize, NS_UNCONSTRAINEDSIZE);
}

Maybe<StyleCaptionSide> nsTableWrapperFrame::GetCaptionSide() const {
  if (mCaptionFrames.IsEmpty()) {
    return Nothing();
  }
  return Some(mCaptionFrames.FirstChild()->StyleTableBorder()->mCaptionSide);
}

StyleVerticalAlignKeyword nsTableWrapperFrame::GetCaptionVerticalAlign() const {
  const auto& va = mCaptionFrames.FirstChild()->StyleDisplay()->mVerticalAlign;
  return va.IsKeyword() ? va.AsKeyword() : StyleVerticalAlignKeyword::Top;
}

nscoord nsTableWrapperFrame::ComputeFinalBSize(
    const MaybeCaptionSide& aCaptionSide, const LogicalSize& aInnerSize,
    const LogicalSize& aCaptionSize, const LogicalMargin& aCaptionMargin,
    const WritingMode aWM) const {
  nscoord bSize = aInnerSize.BSize(aWM);

  // compute the overall block-size
  if (aCaptionSide) {
    switch (*aCaptionSide) {
      case StyleCaptionSide::Top:
      case StyleCaptionSide::TopOutside:
      case StyleCaptionSide::Bottom:
      case StyleCaptionSide::BottomOutside:
        bSize = aInnerSize.BSize(aWM) +
                std::max(
                    0, aCaptionSize.BSize(aWM) + aCaptionMargin.BStartEnd(aWM));
        break;
      case StyleCaptionSide::Left:
      case StyleCaptionSide::Right:
        bSize = std::max(aInnerSize.BSize(aWM),
                         aCaptionSize.BSize(aWM) + aCaptionMargin.BEnd(aWM));
        break;
    }
  }

  // negative sizes can upset overflow-area code
  return std::max(bSize, 0);
}

nsresult nsTableWrapperFrame::GetCaptionOrigin(
    StyleCaptionSide aCaptionSide, const LogicalSize& aContainBlockSize,
    const LogicalSize& aInnerSize, const LogicalSize& aCaptionSize,
    LogicalMargin& aCaptionMargin, LogicalPoint& aOrigin, WritingMode aWM) {
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
    case StyleCaptionSide::Top:
    case StyleCaptionSide::TopOutside:
    case StyleCaptionSide::Bottom:
    case StyleCaptionSide::BottomOutside:
      aOrigin.I(aWM) = aCaptionMargin.IStart(aWM);
      break;
    case StyleCaptionSide::Left:
    case StyleCaptionSide::Right:
      aOrigin.I(aWM) = aCaptionMargin.IStart(aWM);
      if (aWM.IsBidiLTR() == (aCaptionSide == StyleCaptionSide::Right)) {
        aOrigin.I(aWM) += aInnerSize.ISize(aWM);
      }
      break;
  }
  // block-dir computation
  switch (aCaptionSide) {
    case StyleCaptionSide::Right:
    case StyleCaptionSide::Left:
      aOrigin.B(aWM) = 0;
      switch (GetCaptionVerticalAlign()) {
        case StyleVerticalAlignKeyword::Middle:
          aOrigin.B(aWM) = std::max(
              0, (aInnerSize.BSize(aWM) - aCaptionSize.BSize(aWM)) / 2);
          break;
        case StyleVerticalAlignKeyword::Bottom:
          aOrigin.B(aWM) =
              std::max(0, aInnerSize.BSize(aWM) - aCaptionSize.BSize(aWM));
          break;
        default:
          break;
      }
      break;
    case StyleCaptionSide::BottomOutside:
    case StyleCaptionSide::Bottom:
      aOrigin.B(aWM) = aInnerSize.BSize(aWM) + aCaptionMargin.BStart(aWM);
      break;
    case StyleCaptionSide::TopOutside:
    case StyleCaptionSide::Top:
      aOrigin.B(aWM) = aCaptionMargin.BStart(aWM);
      break;
  }
  return NS_OK;
}

nsresult nsTableWrapperFrame::GetInnerOrigin(
    const MaybeCaptionSide& aCaptionSide, const LogicalSize& aContainBlockSize,
    const LogicalSize& aCaptionSize, const LogicalMargin& aCaptionMargin,
    const LogicalSize& aInnerSize, LogicalPoint& aOrigin, WritingMode aWM) {
  NS_ASSERTION(NS_AUTOMARGIN != aCaptionMargin.IStart(aWM) &&
                   NS_AUTOMARGIN != aCaptionMargin.IEnd(aWM),
               "The computed caption margin is auto?");

  aOrigin.I(aWM) = aOrigin.B(aWM) = 0;
  if ((NS_UNCONSTRAINEDSIZE == aInnerSize.ISize(aWM)) ||
      (NS_UNCONSTRAINEDSIZE == aInnerSize.BSize(aWM)) ||
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.ISize(aWM)) ||
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.BSize(aWM))) {
    return NS_OK;
  }

  nscoord minCapISize = aCaptionSize.ISize(aWM) + aCaptionMargin.IStartEnd(aWM);

  // inline-dir computation
  if (aCaptionSide && IsSideCaption(*aCaptionSide)) {
    aOrigin.I(aWM) =
        (aWM.IsBidiLTR() == (*aCaptionSide == StyleCaptionSide::Left))
            ? minCapISize
            : 0;
  }

  // block-dir computation
  if (aCaptionSide) {
    switch (*aCaptionSide) {
      case StyleCaptionSide::Bottom:
      case StyleCaptionSide::BottomOutside:
        // Leave at zero.
        break;
      case StyleCaptionSide::Left:
      case StyleCaptionSide::Right:
        switch (GetCaptionVerticalAlign()) {
          case StyleVerticalAlignKeyword::Middle:
            aOrigin.B(aWM) = std::max(
                0, (aCaptionSize.BSize(aWM) - aInnerSize.BSize(aWM)) / 2);
            break;
          case StyleVerticalAlignKeyword::Bottom:
            aOrigin.B(aWM) =
                std::max(0, aCaptionSize.BSize(aWM) - aInnerSize.BSize(aWM));
            break;
          default:
            // Leave at zero.
            break;
        }
        break;
      case StyleCaptionSide::TopOutside:
      case StyleCaptionSide::Top:
        aOrigin.B(aWM) =
            aCaptionSize.BSize(aWM) + aCaptionMargin.BStartEnd(aWM);
        break;
    }
  }
  return NS_OK;
}

void nsTableWrapperFrame::CreateReflowInputForInnerTable(
    nsPresContext* aPresContext, nsTableFrame* aTableFrame,
    const ReflowInput& aOuterRI, Maybe<ReflowInput>& aChildRI,
    const nscoord aAvailISize) const {
  MOZ_ASSERT(InnerTableFrame() == aTableFrame);

  const WritingMode wm = aTableFrame->GetWritingMode();
  const LogicalSize availSize(wm, aAvailISize, aOuterRI.AvailableBSize());
  aChildRI.emplace(aPresContext, aOuterRI, aTableFrame, availSize, Nothing(),
                   ReflowInput::InitFlag::CallerWillInit);

  Maybe<LogicalMargin> collapseBorder;
  Maybe<LogicalMargin> collapsePadding;
  Maybe<LogicalSize> cbSize;
  if (InnerTableFrame()->IsBorderCollapse()) {
    collapseBorder.emplace(InnerTableFrame()->GetIncludedOuterBCBorder(wm));
    collapsePadding.emplace(wm);
  }
  // Propagate our stored CB size if present, minus any margins.
  //
  // Note that inner table computed margins are always zero, they're inherited
  // by the table wrapper, so we need to get our margin from aOuterRI.
  if (!HasAnyStateBits(NS_FRAME_OUT_OF_FLOW)) {
    if (LogicalSize* cb = GetProperty(GridItemCBSizeProperty())) {
      cbSize.emplace(*cb);
      *cbSize -= aOuterRI.ComputedLogicalMargin(wm).Size(wm);
    }
  }
  if (!cbSize) {
    // For inner table frames, the containing block is the same as for
    // the outer table frame.
    cbSize.emplace(aOuterRI.mContainingBlockSize);
  }
  aChildRI->Init(aPresContext, cbSize, collapseBorder, collapsePadding);
}

void nsTableWrapperFrame::CreateReflowInputForCaption(
    nsPresContext* aPresContext, nsIFrame* aCaptionFrame,
    const ReflowInput& aOuterRI, Maybe<ReflowInput>& aChildRI,
    const nscoord aAvailISize) const {
  MOZ_ASSERT(aCaptionFrame == mCaptionFrames.FirstChild());

  const WritingMode wm = aCaptionFrame->GetWritingMode();

  // Use unconstrained available block-size so that the caption is always
  // fully-complete.
  const LogicalSize availSize(wm, aAvailISize, NS_UNCONSTRAINEDSIZE);
  aChildRI.emplace(aPresContext, aOuterRI, aCaptionFrame, availSize);

  // See if we need to reset mIsTopOfPage flag.
  if (aChildRI->mFlags.mIsTopOfPage) {
    if (auto captionSide = GetCaptionSide()) {
      if (*captionSide == StyleCaptionSide::Bottom ||
          *captionSide == StyleCaptionSide::BottomOutside) {
        aChildRI->mFlags.mIsTopOfPage = false;
      }
    }
  }
}

void nsTableWrapperFrame::ReflowChild(nsPresContext* aPresContext,
                                      nsIFrame* aChildFrame,
                                      const ReflowInput& aChildRI,
                                      ReflowOutput& aMetrics,
                                      nsReflowStatus& aStatus) {
  // Using zero as containerSize here because we want consistency between
  // the GetLogicalPosition and ReflowChild calls, to avoid unnecessarily
  // changing the frame's coordinates; but we don't yet know its final
  // position anyway so the actual value is unimportant.
  const nsSize zeroCSize;
  WritingMode wm = aChildRI.GetWritingMode();

  // Use the current position as a best guess for placement.
  LogicalPoint childPt = aChildFrame->GetLogicalPosition(wm, zeroCSize);
  ReflowChildFlags flags = ReflowChildFlags::NoMoveFrame;

  // We don't want to delete our next-in-flow's child if it's an inner table
  // frame, because table wrapper frames always assume that their inner table
  // frames don't go away. If a table wrapper frame is removed because it is
  // a next-in-flow of an already complete table wrapper frame, then it will
  // take care of removing it's inner table frame.
  if (aChildFrame == InnerTableFrame()) {
    flags |= ReflowChildFlags::NoDeleteNextInFlowChild;
  }

  nsContainerFrame::ReflowChild(aChildFrame, aPresContext, aMetrics, aChildRI,
                                wm, childPt, zeroCSize, flags, aStatus);
}

void nsTableWrapperFrame::UpdateOverflowAreas(ReflowOutput& aMet) {
  aMet.SetOverflowAreasToDesiredBounds();
  ConsiderChildOverflow(aMet.mOverflowAreas, InnerTableFrame());
  if (mCaptionFrames.NotEmpty()) {
    ConsiderChildOverflow(aMet.mOverflowAreas, mCaptionFrames.FirstChild());
  }
}

void nsTableWrapperFrame::Reflow(nsPresContext* aPresContext,
                                 ReflowOutput& aDesiredSize,
                                 const ReflowInput& aOuterRI,
                                 nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsTableWrapperFrame");
  DISPLAY_REFLOW(aPresContext, this, aOuterRI, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  // Initialize out parameters
  aDesiredSize.ClearSize();

  if (!HasAnyStateBits(NS_FRAME_FIRST_REFLOW)) {
    // Set up our kids.  They're already present, on an overflow list,
    // or there are none so we'll create them now
    MoveOverflowToChildList();
  }

  Maybe<ReflowInput> captionRI;
  Maybe<ReflowInput> innerRI;

  nsRect origCaptionRect;
  nsRect origCaptionInkOverflow;
  bool captionFirstReflow = false;
  if (mCaptionFrames.NotEmpty()) {
    origCaptionRect = mCaptionFrames.FirstChild()->GetRect();
    origCaptionInkOverflow = mCaptionFrames.FirstChild()->InkOverflowRect();
    captionFirstReflow =
        mCaptionFrames.FirstChild()->HasAnyStateBits(NS_FRAME_FIRST_REFLOW);
  }

  // ComputeAutoSize has to match this logic.
  WritingMode wm = aOuterRI.GetWritingMode();
  Maybe<StyleCaptionSide> captionSide = GetCaptionSide();
  WritingMode captionWM = wm;  // will be changed below if necessary
  const nscoord contentBoxISize = aOuterRI.ComputedSize(wm).ISize(wm);

  MOZ_ASSERT(mCaptionFrames.NotEmpty() == captionSide.isSome());

  if (!captionSide) {
    // We don't have a caption.
    CreateReflowInputForInnerTable(aPresContext, InnerTableFrame(), aOuterRI,
                                   innerRI, contentBoxISize);
  } else if (*captionSide == StyleCaptionSide::Left ||
             *captionSide == StyleCaptionSide::Right) {
    // ComputeAutoSize takes care of making side captions small. Compute
    // the caption's size first, and tell the table to fit in what's left.
    CreateReflowInputForCaption(aPresContext, mCaptionFrames.FirstChild(),
                                aOuterRI, captionRI, contentBoxISize);
    captionWM = captionRI->GetWritingMode();
    nscoord innerAvailISize =
        contentBoxISize -
        captionRI->ComputedSizeWithMarginBorderPadding(wm).ISize(wm);
    CreateReflowInputForInnerTable(aPresContext, InnerTableFrame(), aOuterRI,
                                   innerRI, innerAvailISize);
  } else if (*captionSide == StyleCaptionSide::Top ||
             *captionSide == StyleCaptionSide::Bottom) {
    // Compute the table's size first, and then prevent the caption from
    // being larger in the inline dir unless it has to be.
    //
    // Note that CSS 2.1 (but not 2.0) says:
    //   The width of the anonymous box is the border-edge width of the
    //   table box inside it
    // We don't actually make our anonymous box that isize (if we did,
    // it would break 'auto' margins), but this effectively does that.
    CreateReflowInputForInnerTable(aPresContext, InnerTableFrame(), aOuterRI,
                                   innerRI, contentBoxISize);
    // It's good that CSS 2.1 says not to include margins, since we
    // can't, since they already been converted so they exactly
    // fill the available isize (ignoring the margin on one side if
    // neither are auto).  (We take advantage of that later when we call
    // GetCaptionOrigin, though.)
    nscoord innerBorderISize =
        innerRI->ComputedSizeWithBorderPadding(wm).ISize(wm);
    CreateReflowInputForCaption(aPresContext, mCaptionFrames.FirstChild(),
                                aOuterRI, captionRI, innerBorderISize);
    captionWM = captionRI->GetWritingMode();
  } else {
    MOZ_ASSERT(*captionSide == StyleCaptionSide::TopOutside ||
                   *captionSide == StyleCaptionSide::BottomOutside,
               "unexpected caption-side");
    // Size the table and the caption independently.
    captionWM = mCaptionFrames.FirstChild()->GetWritingMode();
    CreateReflowInputForCaption(
        aPresContext, mCaptionFrames.FirstChild(), aOuterRI, captionRI,
        aOuterRI.ComputedSize(captionWM).ISize(captionWM));
    CreateReflowInputForInnerTable(aPresContext, InnerTableFrame(), aOuterRI,
                                   innerRI, contentBoxISize);
  }

  // First reflow the caption.
  Maybe<ReflowOutput> captionMet;
  LogicalSize captionSize(wm);
  LogicalMargin captionMargin(wm);
  if (captionSide) {
    captionMet.emplace(wm);
    // We intentionally don't merge capStatus into aStatus, since we currently
    // can't handle caption continuations, but we probably should.
    nsReflowStatus capStatus;
    ReflowChild(aPresContext, mCaptionFrames.FirstChild(), *captionRI,
                *captionMet, capStatus);
    captionSize.ISize(wm) = captionMet->ISize(wm);
    captionSize.BSize(wm) = captionMet->BSize(wm);
    captionMargin = captionRI->ComputedLogicalMargin(wm);
    // Now that we know the bsize of the caption, reduce the available bsize
    // for the table frame if we are bsize constrained and the caption is above
    // or below the inner table.  Also reduce the CB size that we store for
    // our children in case we're a grid item, by the same amount.
    LogicalSize* cbSize = GetProperty(GridItemCBSizeProperty());
    if (NS_UNCONSTRAINEDSIZE != aOuterRI.AvailableBSize() || cbSize) {
      nscoord captionBSize = 0;
      nscoord captionISize = 0;
      switch (*captionSide) {
        case StyleCaptionSide::Top:
        case StyleCaptionSide::Bottom:
        case StyleCaptionSide::TopOutside:
        case StyleCaptionSide::BottomOutside:
          captionBSize = captionSize.BSize(wm) + captionMargin.BStartEnd(wm);
          break;
        case StyleCaptionSide::Left:
        case StyleCaptionSide::Right:
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
          CreateReflowInputForInnerTable(aPresContext, InnerTableFrame(),
                                         aOuterRI, innerRI, contentBoxISize);
        }
      }
    }
  }

  // Then, now that we know how much to reduce the isize of the inner
  // table to account for side captions, reflow the inner table.
  ReflowOutput innerMet(innerRI->GetWritingMode());
  ReflowChild(aPresContext, InnerTableFrame(), *innerRI, innerMet, aStatus);
  LogicalSize innerSize(wm, innerMet.ISize(wm), innerMet.BSize(wm));

  LogicalSize containSize(wm, GetContainingBlockSize(aOuterRI));

  // Now that we've reflowed both we can place them.
  // XXXldb Most of the input variables here are now uninitialized!

  // XXX Need to recompute inner table's auto margins for the case of side
  // captions.  (Caption's are broken too, but that should be fixed earlier.)

  // Compute the desiredSize so that we can use it as the containerSize
  // for the FinishReflowChild calls below.
  LogicalSize desiredSize(wm);

  // We have zero border and padding, so content-box inline-size is our desired
  // border-box inline-size.
  desiredSize.ISize(wm) = contentBoxISize;
  desiredSize.BSize(wm) =
      ComputeFinalBSize(captionSide, innerSize, captionSize, captionMargin, wm);

  aDesiredSize.SetSize(wm, desiredSize);
  nsSize containerSize = aDesiredSize.PhysicalSize();
  // XXX It's possible for this to be NS_UNCONSTRAINEDSIZE, which will result
  // in assertions from FinishReflowChild.

  MOZ_ASSERT(mCaptionFrames.NotEmpty() == captionSide.isSome());
  if (mCaptionFrames.NotEmpty()) {
    LogicalPoint captionOrigin(wm);
    GetCaptionOrigin(*captionSide, containSize, innerSize, captionSize,
                     captionMargin, captionOrigin, wm);
    FinishReflowChild(mCaptionFrames.FirstChild(), aPresContext, *captionMet,
                      captionRI.ptr(), wm, captionOrigin, containerSize,
                      ReflowChildFlags::Default);
    captionRI.reset();
  }
  // XXX If the bsize is constrained then we need to check whether
  // everything still fits...

  LogicalPoint innerOrigin(wm);
  GetInnerOrigin(captionSide, containSize, captionSize, captionMargin,
                 innerSize, innerOrigin, wm);
  FinishReflowChild(InnerTableFrame(), aPresContext, innerMet, innerRI.ptr(),
                    wm, innerOrigin, containerSize, ReflowChildFlags::Default);
  innerRI.reset();

  if (mCaptionFrames.NotEmpty()) {
    nsTableFrame::InvalidateTableFrame(mCaptionFrames.FirstChild(),
                                       origCaptionRect, origCaptionInkOverflow,
                                       captionFirstReflow);
  }

  UpdateOverflowAreas(aDesiredSize);

  if (GetPrevInFlow()) {
    ReflowOverflowContainerChildren(aPresContext, aOuterRI,
                                    aDesiredSize.mOverflowAreas,
                                    ReflowChildFlags::Default, aStatus);
  }

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aOuterRI, aStatus);

  // Return our desired rect

  NS_FRAME_SET_TRUNCATION(aStatus, aOuterRI, aDesiredSize);
}

/* ----- global methods ----- */

nsIContent* nsTableWrapperFrame::GetCellAt(uint32_t aRowIdx,
                                           uint32_t aColIdx) const {
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

nsTableWrapperFrame* NS_NewTableWrapperFrame(PresShell* aPresShell,
                                             ComputedStyle* aStyle) {
  return new (aPresShell)
      nsTableWrapperFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsTableWrapperFrame)

#ifdef DEBUG_FRAME_DUMP
nsresult nsTableWrapperFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"TableWrapper"_ns, aResult);
}
#endif
