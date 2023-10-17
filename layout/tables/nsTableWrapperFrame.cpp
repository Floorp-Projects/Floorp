/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTableWrapperFrame.h"

#include "LayoutConstants.h"
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

nscoord nsTableWrapperFrame::SynthesizeFallbackBaseline(
    mozilla::WritingMode aWM, BaselineSharingGroup aBaselineGroup) const {
  const auto marginBlockEnd = GetLogicalUsedMargin(aWM).BEnd(aWM);
  if (aWM.IsCentralBaseline()) {
    return (BSize(aWM) + marginBlockEnd) / 2;
  }
  // Our fallback baseline is the block-end margin-edge, with respect to the
  // given writing mode.
  if (aBaselineGroup == BaselineSharingGroup::Last) {
    return -marginBlockEnd;
  }
  return BSize(aWM) + marginBlockEnd;
}

Maybe<nscoord> nsTableWrapperFrame::GetNaturalBaselineBOffset(
    WritingMode aWM, BaselineSharingGroup aBaselineGroup,
    BaselineExportContext aExportContext) const {
  // Baseline is determined by row
  // (https://drafts.csswg.org/css-align-3/#baseline-export). If the row
  // direction is going to be orthogonal to the parent's writing mode, the
  // resulting baseline wouldn't be valid, so we use the fallback baseline
  // instead.
  if (StyleDisplay()->IsContainLayout() ||
      GetWritingMode().IsOrthogonalTo(aWM)) {
    return Nothing{};
  }
  auto* innerTable = InnerTableFrame();
  return innerTable
      ->GetNaturalBaselineBOffset(aWM, aBaselineGroup, aExportContext)
      .map([this, aWM, aBaselineGroup, innerTable](nscoord aBaseline) {
        auto bStart = innerTable->BStart(aWM, mRect.Size());
        if (aBaselineGroup == BaselineSharingGroup::First) {
          return aBaseline + bStart;
        }
        auto bEnd = bStart + innerTable->BSize(aWM);
        return BSize(aWM) - (bEnd - aBaseline);
      });
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

void nsTableWrapperFrame::Destroy(DestroyContext& aContext) {
  DestroyAbsoluteFrames(aContext);
  mCaptionFrames.DestroyFrames(aContext);
  nsContainerFrame::Destroy(aContext);
}

const nsFrameList& nsTableWrapperFrame::GetChildList(
    ChildListID aListID) const {
  if (aListID == FrameChildListID::Caption) {
    return mCaptionFrames;
  }

  return nsContainerFrame::GetChildList(aListID);
}

void nsTableWrapperFrame::GetChildLists(nsTArray<ChildList>* aLists) const {
  nsContainerFrame::GetChildLists(aLists);
  mCaptionFrames.AppendIfNonempty(aLists, FrameChildListID::Caption);
}

void nsTableWrapperFrame::SetInitialChildList(ChildListID aListID,
                                              nsFrameList&& aChildList) {
  if (FrameChildListID::Caption == aListID) {
#ifdef DEBUG
    nsIFrame::VerifyDirtyBitSet(aChildList);
    for (nsIFrame* f : aChildList) {
      MOZ_ASSERT(f->GetParent() == this, "Unexpected parent");
    }
#endif
    // the frame constructor already checked for table-caption display type
    MOZ_ASSERT(mCaptionFrames.IsEmpty(),
               "already have child frames in CaptionList");
    mCaptionFrames = std::move(aChildList);
  } else {
    MOZ_ASSERT(FrameChildListID::Principal != aListID ||
                   (aChildList.FirstChild() &&
                    aChildList.FirstChild() == aChildList.LastChild() &&
                    aChildList.FirstChild()->IsTableFrame()),
               "expected a single table frame in principal child list");
    nsContainerFrame::SetInitialChildList(aListID, std::move(aChildList));
  }
}

void nsTableWrapperFrame::AppendFrames(ChildListID aListID,
                                       nsFrameList&& aFrameList) {
  // We only have two child frames: the inner table and a caption frame.
  // The inner frame is provided when we're initialized, and it cannot change
  MOZ_ASSERT(FrameChildListID::Caption == aListID, "unexpected child list");
  MOZ_ASSERT(aFrameList.IsEmpty() || aFrameList.FirstChild()->IsTableCaption(),
             "appending non-caption frame to captionList");
  mCaptionFrames.AppendFrames(nullptr, std::move(aFrameList));

  // Reflow the new caption frame. It's already marked dirty, so
  // just tell the pres shell.
  PresShell()->FrameNeedsReflow(this, IntrinsicDirty::FrameAndAncestors,
                                NS_FRAME_HAS_DIRTY_CHILDREN);
  // The presence of caption frames makes us sort our display
  // list differently, so mark us as changed for the new
  // ordering.
  MarkNeedsDisplayItemRebuild();
}

void nsTableWrapperFrame::InsertFrames(
    ChildListID aListID, nsIFrame* aPrevFrame,
    const nsLineList::iterator* aPrevFrameLine, nsFrameList&& aFrameList) {
  MOZ_ASSERT(FrameChildListID::Caption == aListID, "unexpected child list");
  MOZ_ASSERT(aFrameList.IsEmpty() || aFrameList.FirstChild()->IsTableCaption(),
             "inserting non-caption frame into captionList");
  MOZ_ASSERT(!aPrevFrame || aPrevFrame->GetParent() == this,
             "inserting after sibling frame with different parent");
  mCaptionFrames.InsertFrames(nullptr, aPrevFrame, std::move(aFrameList));

  // Reflow the new caption frame. It's already marked dirty, so
  // just tell the pres shell.
  PresShell()->FrameNeedsReflow(this, IntrinsicDirty::FrameAndAncestors,
                                NS_FRAME_HAS_DIRTY_CHILDREN);
  MarkNeedsDisplayItemRebuild();
}

void nsTableWrapperFrame::RemoveFrame(DestroyContext& aContext,
                                      ChildListID aListID,
                                      nsIFrame* aOldFrame) {
  // We only have two child frames: the inner table and one caption frame.
  // The inner frame can't be removed so this should be the caption
  MOZ_ASSERT(FrameChildListID::Caption == aListID, "can't remove inner frame");

  // Remove the frame and destroy it
  mCaptionFrames.DestroyFrame(aContext, aOldFrame);

  PresShell()->FrameNeedsReflow(this, IntrinsicDirty::FrameAndAncestors,
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
    if (capISize > iSize) {
      iSize = capISize;
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
  if (mCaptionFrames.NotEmpty()) {
    // Don't let the caption's pref isize expand the table's pref isize.
    const nscoord capMinISize = nsLayoutUtils::IntrinsicForContainer(
        aRenderingContext, mCaptionFrames.FirstChild(),
        IntrinsicISizeType::MinISize);
    maxISize = std::max(maxISize, capMinISize);
  }
  return maxISize;
}

LogicalSize nsTableWrapperFrame::InnerTableShrinkWrapSize(
    gfxContext* aRenderingContext, nsTableFrame* aTableFrame, WritingMode aWM,
    const LogicalSize& aCBSize, nscoord aAvailableISize,
    const StyleSizeOverrides& aSizeOverrides, ComputeSizeFlags aFlags) const {
  MOZ_ASSERT(InnerTableFrame() == aTableFrame);

  AutoMaybeDisableFontInflation an(aTableFrame);

  Maybe<LogicalMargin> collapseBorder;
  Maybe<LogicalMargin> collapsePadding;
  aTableFrame->GetCollapsedBorderPadding(collapseBorder, collapsePadding);

  SizeComputationInput input(aTableFrame, aRenderingContext, aWM,
                             aCBSize.ISize(aWM), collapseBorder,
                             collapsePadding);
  LogicalSize marginSize(aWM);  // Inner table doesn't have any margin
  LogicalSize bpSize = input.ComputedLogicalBorderPadding(aWM).Size(aWM);

  // Note that we pass an empty caption-area here (rather than the caption's
  // actual size). This is fine because:
  //
  // 1) nsTableWrapperFrame::ComputeSize() only uses the size returned by this
  //    method (indirectly via calling nsTableWrapperFrame::ComputeAutoSize())
  //    if it get a aSizeOverrides arg containing any size overrides with
  //    mApplyOverridesVerbatim=true. The aSizeOverrides arg is passed to this
  //    method without any modifications.
  //
  // 2) With 1), that means the aSizeOverrides passing into this method should
  //    be applied to the inner table directly, so we don't need to subtract
  //    caption-area when preparing innerOverrides for
  //    nsTableFrame::ComputeSize().
  StyleSizeOverrides innerOverrides = ComputeSizeOverridesForInnerTable(
      aTableFrame, aSizeOverrides, bpSize, /* aBSizeOccupiedByCaption = */ 0);
  auto size =
      aTableFrame
          ->ComputeSize(aRenderingContext, aWM, aCBSize, aAvailableISize,
                        marginSize, bpSize, innerOverrides, aFlags)
          .mLogicalSize;
  size.ISize(aWM) += bpSize.ISize(aWM);
  if (size.BSize(aWM) != NS_UNCONSTRAINEDSIZE) {
    size.BSize(aWM) += bpSize.BSize(aWM);
  }
  return size;
}

LogicalSize nsTableWrapperFrame::CaptionShrinkWrapSize(
    gfxContext* aRenderingContext, nsIFrame* aCaptionFrame, WritingMode aWM,
    const LogicalSize& aCBSize, nscoord aAvailableISize,
    ComputeSizeFlags aFlags) const {
  MOZ_ASSERT(aCaptionFrame == mCaptionFrames.FirstChild());

  AutoMaybeDisableFontInflation an(aCaptionFrame);

  SizeComputationInput input(aCaptionFrame, aRenderingContext, aWM,
                             aCBSize.ISize(aWM));
  LogicalSize marginSize = input.ComputedLogicalMargin(aWM).Size(aWM);
  LogicalSize bpSize = input.ComputedLogicalBorderPadding(aWM).Size(aWM);

  auto size = aCaptionFrame
                  ->ComputeSize(aRenderingContext, aWM, aCBSize,
                                aAvailableISize, marginSize, bpSize, {}, aFlags)
                  .mLogicalSize;
  size.ISize(aWM) += (marginSize.ISize(aWM) + bpSize.ISize(aWM));
  if (size.BSize(aWM) != NS_UNCONSTRAINEDSIZE) {
    size.BSize(aWM) += (marginSize.BSize(aWM) + bpSize.BSize(aWM));
  }
  return size;
}

StyleSize nsTableWrapperFrame::ReduceStyleSizeBy(
    const StyleSize& aStyleSize, const nscoord aAmountToReduce) const {
  MOZ_ASSERT(aStyleSize.ConvertsToLength(), "Only handles 'Length' StyleSize!");
  const nscoord size = std::max(0, aStyleSize.ToLength() - aAmountToReduce);
  return StyleSize::LengthPercentage(LengthPercentage::FromAppUnits(size));
}

StyleSizeOverrides nsTableWrapperFrame::ComputeSizeOverridesForInnerTable(
    const nsTableFrame* aTableFrame,
    const StyleSizeOverrides& aWrapperSizeOverrides,
    const LogicalSize& aBorderPadding, nscoord aBSizeOccupiedByCaption) const {
  if (aWrapperSizeOverrides.mApplyOverridesVerbatim ||
      !aWrapperSizeOverrides.HasAnyLengthOverrides()) {
    // We are asked to apply the size overrides directly to the inner table, or
    // there's no 'Length' size overrides. No need to tweak the size overrides.
    return aWrapperSizeOverrides;
  }

  const auto wm = aTableFrame->GetWritingMode();
  LogicalSize areaOccupied(wm, 0, aBSizeOccupiedByCaption);
  if (aTableFrame->StylePosition()->mBoxSizing == StyleBoxSizing::Content) {
    // If the inner table frame has 'box-sizing: content', enlarge the occupied
    // area by adding border & padding because they should also be subtracted
    // from the size overrides.
    areaOccupied += aBorderPadding;
  }

  StyleSizeOverrides innerSizeOverrides;
  const auto& wrapperISize = aWrapperSizeOverrides.mStyleISize;
  if (wrapperISize) {
    MOZ_ASSERT(!wrapperISize->HasPercent(),
               "Table doesn't support size overrides containing percentages!");
    innerSizeOverrides.mStyleISize.emplace(
        wrapperISize->ConvertsToLength()
            ? ReduceStyleSizeBy(*wrapperISize, areaOccupied.ISize(wm))
            : *wrapperISize);
  }

  const auto& wrapperBSize = aWrapperSizeOverrides.mStyleBSize;
  if (wrapperBSize) {
    MOZ_ASSERT(!wrapperBSize->HasPercent(),
               "Table doesn't support size overrides containing percentages!");
    innerSizeOverrides.mStyleBSize.emplace(
        wrapperBSize->ConvertsToLength()
            ? ReduceStyleSizeBy(*wrapperBSize, areaOccupied.BSize(wm))
            : *wrapperBSize);
  }

  return innerSizeOverrides;
}

/* virtual */
nsIFrame::SizeComputationResult nsTableWrapperFrame::ComputeSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorderPadding, const StyleSizeOverrides& aSizeOverrides,
    ComputeSizeFlags aFlags) {
  auto result = nsContainerFrame::ComputeSize(
      aRenderingContext, aWM, aCBSize, aAvailableISize, aMargin, aBorderPadding,
      aSizeOverrides, aFlags);

  if (aSizeOverrides.mApplyOverridesVerbatim &&
      aSizeOverrides.HasAnyOverrides()) {
    // We are asked to apply the size overrides directly to the inner table, but
    // we still want ourselves to remain 'auto'-sized and shrink-wrapping our
    // children's sizes. (Table wrapper frames always have 'auto' inline-size
    // and block-size, since we don't inherit those properties from inner table,
    // and authors can't target them with styling.)
    auto size =
        ComputeAutoSize(aRenderingContext, aWM, aCBSize, aAvailableISize,
                        aMargin, aBorderPadding, aSizeOverrides, aFlags);
    result.mLogicalSize = size;
  }

  return result;
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

  // Match the logic in Reflow() that sets aside space for the caption.
  Maybe<StyleCaptionSide> captionSide = GetCaptionSide();

  const LogicalSize innerTableSize = InnerTableShrinkWrapSize(
      aRenderingContext, InnerTableFrame(), aWM, aCBSize, kidAvailableISize,
      aSizeOverrides, flags);
  if (!captionSide) {
    return innerTableSize;
  }
  const LogicalSize captionSize =
      CaptionShrinkWrapSize(aRenderingContext, mCaptionFrames.FirstChild(), aWM,
                            aCBSize, innerTableSize.ISize(aWM), flags);
  const nscoord iSize =
      std::max(innerTableSize.ISize(aWM), captionSize.ISize(aWM));
  nscoord bSize = NS_UNCONSTRAINEDSIZE;
  if (innerTableSize.BSize(aWM) != NS_UNCONSTRAINEDSIZE &&
      captionSize.BSize(aWM) != NS_UNCONSTRAINEDSIZE) {
    bSize = innerTableSize.BSize(aWM) + captionSize.BSize(aWM);
  }
  return LogicalSize(aWM, iSize, bSize);
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
  // negative sizes can upset overflow-area code
  return std::max(0, aInnerSize.BSize(aWM) +
                         std::max(0, aCaptionSize.BSize(aWM) +
                                         aCaptionMargin.BStartEnd(aWM)));
}

void nsTableWrapperFrame::GetCaptionOrigin(StyleCaptionSide aCaptionSide,
                                           const LogicalSize& aContainBlockSize,
                                           const LogicalSize& aInnerSize,
                                           const LogicalSize& aCaptionSize,
                                           LogicalMargin& aCaptionMargin,
                                           LogicalPoint& aOrigin,
                                           WritingMode aWM) const {
  aOrigin.I(aWM) = aOrigin.B(aWM) = 0;
  if ((NS_UNCONSTRAINEDSIZE == aInnerSize.ISize(aWM)) ||
      (NS_UNCONSTRAINEDSIZE == aInnerSize.BSize(aWM)) ||
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.ISize(aWM)) ||
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.BSize(aWM))) {
    return;
  }
  if (mCaptionFrames.IsEmpty()) {
    return;
  }

  NS_ASSERTION(NS_AUTOMARGIN != aCaptionMargin.IStart(aWM) &&
                   NS_AUTOMARGIN != aCaptionMargin.BStart(aWM) &&
                   NS_AUTOMARGIN != aCaptionMargin.BEnd(aWM),
               "The computed caption margin is auto?");

  aOrigin.I(aWM) = aCaptionMargin.IStart(aWM);

  // block-dir computation
  switch (aCaptionSide) {
    case StyleCaptionSide::Bottom:
      aOrigin.B(aWM) = aInnerSize.BSize(aWM) + aCaptionMargin.BStart(aWM);
      break;
    case StyleCaptionSide::Top:
      aOrigin.B(aWM) = aCaptionMargin.BStart(aWM);
      break;
  }
}

void nsTableWrapperFrame::GetInnerOrigin(const MaybeCaptionSide& aCaptionSide,
                                         const LogicalSize& aContainBlockSize,
                                         const LogicalSize& aCaptionSize,
                                         const LogicalMargin& aCaptionMargin,
                                         const LogicalSize& aInnerSize,
                                         LogicalPoint& aOrigin,
                                         WritingMode aWM) const {
  NS_ASSERTION(NS_AUTOMARGIN != aCaptionMargin.IStart(aWM) &&
                   NS_AUTOMARGIN != aCaptionMargin.IEnd(aWM),
               "The computed caption margin is auto?");

  aOrigin.I(aWM) = aOrigin.B(aWM) = 0;
  if ((NS_UNCONSTRAINEDSIZE == aInnerSize.ISize(aWM)) ||
      (NS_UNCONSTRAINEDSIZE == aInnerSize.BSize(aWM)) ||
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.ISize(aWM)) ||
      (NS_UNCONSTRAINEDSIZE == aCaptionSize.BSize(aWM))) {
    return;
  }

  // block-dir computation
  if (aCaptionSide) {
    switch (*aCaptionSide) {
      case StyleCaptionSide::Bottom:
        // Leave at zero.
        break;
      case StyleCaptionSide::Top:
        aOrigin.B(aWM) =
            aCaptionSize.BSize(aWM) + aCaptionMargin.BStartEnd(aWM);
        break;
    }
  }
}

void nsTableWrapperFrame::CreateReflowInputForInnerTable(
    nsPresContext* aPresContext, nsTableFrame* aTableFrame,
    const ReflowInput& aOuterRI, Maybe<ReflowInput>& aChildRI,
    const nscoord aAvailISize, nscoord aBSizeOccupiedByCaption) const {
  MOZ_ASSERT(InnerTableFrame() == aTableFrame);

  const WritingMode wm = aTableFrame->GetWritingMode();
  // If we have a caption occupied our content-box area, reduce the available
  // block-size by the amount.
  nscoord availBSize = aOuterRI.AvailableBSize();
  if (availBSize != NS_UNCONSTRAINEDSIZE) {
    availBSize = std::max(0, availBSize - aBSizeOccupiedByCaption);
  }
  const LogicalSize availSize(wm, aAvailISize, availBSize);

  // For inner table frames, the containing block is the same as for the outer
  // table frame.
  Maybe<LogicalSize> cbSize = Some(aOuterRI.mContainingBlockSize);

  // However, if we are a grid item, the CB size needs to subtract our margins
  // and the area occupied by the caption.
  //
  // Note that inner table computed margins are always zero, they're inherited
  // by the table wrapper, so we need to get our margin from aOuterRI.
  if (IsGridItem()) {
    const LogicalMargin margin = aOuterRI.ComputedLogicalMargin(wm);
    cbSize->ISize(wm) = std::max(0, cbSize->ISize(wm) - margin.IStartEnd(wm));
    if (cbSize->BSize(wm) != NS_UNCONSTRAINEDSIZE) {
      cbSize->BSize(wm) = std::max(0, cbSize->BSize(wm) - margin.BStartEnd(wm) -
                                          aBSizeOccupiedByCaption);
    }
  }

  if (!aTableFrame->IsBorderCollapse() &&
      !aOuterRI.mStyleSizeOverrides.HasAnyOverrides()) {
    // We are not border-collapsed and not given any size overrides. It's
    // sufficient to call the standard ReflowInput constructor.
    aChildRI.emplace(aPresContext, aOuterRI, aTableFrame, availSize, cbSize);
    return;
  }

  Maybe<LogicalMargin> borderPadding;
  Maybe<LogicalMargin> padding;
  {
    // Compute inner table frame's border & padding because we may need to
    // reduce the size for inner table's size overrides. We won't waste time if
    // they are not used, because we can use them directly by passing them into
    // ReflowInput::Init().
    Maybe<LogicalMargin> collapseBorder;
    Maybe<LogicalMargin> collapsePadding;
    aTableFrame->GetCollapsedBorderPadding(collapseBorder, collapsePadding);
    SizeComputationInput input(aTableFrame, aOuterRI.mRenderingContext, wm,
                               cbSize->ISize(wm), collapseBorder,
                               collapsePadding);
    borderPadding.emplace(input.ComputedLogicalBorderPadding(wm));
    padding.emplace(input.ComputedLogicalPadding(wm));
  }

  StyleSizeOverrides innerOverrides = ComputeSizeOverridesForInnerTable(
      aTableFrame, aOuterRI.mStyleSizeOverrides, borderPadding->Size(wm),
      aBSizeOccupiedByCaption);

  aChildRI.emplace(aPresContext, aOuterRI, aTableFrame, availSize, Nothing(),
                   ReflowInput::InitFlag::CallerWillInit, innerOverrides);
  aChildRI->Init(aPresContext, cbSize, Some(*borderPadding - *padding),
                 padding);
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
      if (*captionSide == StyleCaptionSide::Bottom) {
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
  if (captionSide) {
    // It's good that CSS 2.1 says not to include margins, since we can't, since
    // they already been converted so they exactly fill the available isize
    // (ignoring the margin on one side if neither are auto).  (We take
    // advantage of that later when we call GetCaptionOrigin, though.)
    nscoord innerBorderISize =
        innerRI->ComputedSizeWithBorderPadding(wm).ISize(wm);
    CreateReflowInputForCaption(aPresContext, mCaptionFrames.FirstChild(),
                                aOuterRI, captionRI, innerBorderISize);
    captionWM = captionRI->GetWritingMode();
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
    captionSize = captionMet->Size(wm);
    captionMargin = captionRI->ComputedLogicalMargin(wm);
    nscoord bSizeOccupiedByCaption =
        captionSize.BSize(wm) + captionMargin.BStartEnd(wm);
    if (bSizeOccupiedByCaption) {
      // Reset the inner table's ReflowInput to reduce various sizes because of
      // the area occupied by caption.
      innerRI.reset();
      CreateReflowInputForInnerTable(aPresContext, InnerTableFrame(), aOuterRI,
                                     innerRI, contentBoxISize,
                                     bSizeOccupiedByCaption);
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
                      ReflowChildFlags::ApplyRelativePositioning);
    captionRI.reset();
  }
  // XXX If the bsize is constrained then we need to check whether
  // everything still fits...

  LogicalPoint innerOrigin(wm);
  GetInnerOrigin(captionSide, containSize, captionSize, captionMargin,
                 innerSize, innerOrigin, wm);
  // NOTE: Relative positioning on the table applies to the whole table wrapper.
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
