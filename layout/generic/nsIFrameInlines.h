/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIFrameInlines_h___
#define nsIFrameInlines_h___

#include "mozilla/dom/ElementInlines.h"
#include "nsContainerFrame.h"
#include "nsPlaceholderFrame.h"
#include "nsStyleStructInlines.h"
#include "nsCSSAnonBoxes.h"
#include "nsFrameManager.h"

bool nsIFrame::IsSVGGeometryFrameOrSubclass() const {
  return IsSVGGeometryFrame() || IsSVGImageFrame();
}

bool nsIFrame::IsFlexItem() const {
  return GetParent() && GetParent()->IsFlexContainerFrame() &&
         !(GetStateBits() & NS_FRAME_OUT_OF_FLOW);
}

bool nsIFrame::IsGridItem() const {
  return GetParent() && GetParent()->IsGridContainerFrame() &&
         !(GetStateBits() & NS_FRAME_OUT_OF_FLOW);
}

bool nsIFrame::IsFlexOrGridContainer() const {
  return IsFlexContainerFrame() || IsGridContainerFrame();
}

bool nsIFrame::IsFlexOrGridItem() const {
  return !(GetStateBits() & NS_FRAME_OUT_OF_FLOW) && GetParent() &&
         GetParent()->IsFlexOrGridContainer();
}

bool nsIFrame::IsMasonry(mozilla::LogicalAxis aAxis) const {
  MOZ_DIAGNOSTIC_ASSERT(IsGridContainerFrame());
  return HasAnyStateBits(aAxis == mozilla::eLogicalAxisBlock
                             ? NS_STATE_GRID_IS_ROW_MASONRY
                             : NS_STATE_GRID_IS_COL_MASONRY);
}

bool nsIFrame::IsTableCaption() const {
  return StyleDisplay()->mDisplay == mozilla::StyleDisplay::TableCaption &&
         GetParent()->Style()->GetPseudoType() ==
             mozilla::PseudoStyleType::tableWrapper;
}

bool nsIFrame::IsFloating() const { return StyleDisplay()->IsFloating(this); }

bool nsIFrame::IsAbsPosContainingBlock() const {
  return StyleDisplay()->IsAbsPosContainingBlock(this);
}

bool nsIFrame::IsFixedPosContainingBlock() const {
  return StyleDisplay()->IsFixedPosContainingBlock(this);
}

bool nsIFrame::IsRelativelyPositioned() const {
  return StyleDisplay()->IsRelativelyPositioned(this);
}

bool nsIFrame::IsStickyPositioned() const {
  return StyleDisplay()->IsStickyPositioned(this);
}

bool nsIFrame::IsAbsolutelyPositioned(
    const nsStyleDisplay* aStyleDisplay) const {
  const nsStyleDisplay* disp = StyleDisplayWithOptionalParam(aStyleDisplay);
  return disp->IsAbsolutelyPositioned(this);
}

bool nsIFrame::IsBlockOutside() const {
  return StyleDisplay()->IsBlockOutside(this);
}

bool nsIFrame::IsInlineOutside() const {
  return StyleDisplay()->IsInlineOutside(this);
}

bool nsIFrame::IsColumnSpan() const {
  return IsBlockOutside() && StyleColumn()->IsColumnSpanStyle();
}

bool nsIFrame::IsColumnSpanInMulticolSubtree() const {
  return IsColumnSpan() &&
         (HasAnyStateBits(NS_FRAME_HAS_MULTI_COLUMN_ANCESTOR) ||
          // A frame other than inline and block won't have
          // NS_FRAME_HAS_MULTI_COLUMN_ANCESTOR. We instead test its parent.
          (GetParent() && GetParent()->Style()->GetPseudoType() ==
                              mozilla::PseudoStyleType::columnSpanWrapper));
}

mozilla::StyleDisplay nsIFrame::GetDisplay() const {
  return StyleDisplay()->GetDisplay(this);
}

nscoord nsIFrame::SynthesizeBaselineBOffsetFromMarginBox(
    mozilla::WritingMode aWM, BaselineSharingGroup aGroup) const {
  MOZ_ASSERT(!aWM.IsOrthogonalTo(GetWritingMode()));
  auto margin = GetLogicalUsedMargin(aWM);
  if (aGroup == BaselineSharingGroup::First) {
    if (aWM.IsAlphabeticalBaseline()) {
      // First baseline for inverted-line content is the block-start margin
      // edge, as the frame is in effect "flipped" for alignment purposes.
      return MOZ_UNLIKELY(aWM.IsLineInverted()) ? -margin.BStart(aWM)
                                                : BSize(aWM) + margin.BEnd(aWM);
    }
    nscoord marginBoxCenter = (BSize(aWM) + margin.BStartEnd(aWM)) / 2;
    return marginBoxCenter - margin.BStart(aWM);
  }
  MOZ_ASSERT(aGroup == BaselineSharingGroup::Last);
  if (aWM.IsAlphabeticalBaseline()) {
    // Last baseline for inverted-line content is the block-start margin edge,
    // as the frame is in effect "flipped" for alignment purposes.
    return MOZ_UNLIKELY(aWM.IsLineInverted()) ? BSize(aWM) + margin.BStart(aWM)
                                              : -margin.BEnd(aWM);
  }
  // Round up for central baseline offset, to be consistent with ::First.
  nscoord marginBoxSize = BSize(aWM) + margin.BStartEnd(aWM);
  nscoord marginBoxCenter = (marginBoxSize / 2) + (marginBoxSize % 2);
  return marginBoxCenter - margin.BEnd(aWM);
}

nscoord nsIFrame::SynthesizeBaselineBOffsetFromBorderBox(
    mozilla::WritingMode aWM, BaselineSharingGroup aGroup) const {
  MOZ_ASSERT(!aWM.IsOrthogonalTo(GetWritingMode()));
  nscoord borderBoxSize = BSize(aWM);
  if (aGroup == BaselineSharingGroup::First) {
    return MOZ_LIKELY(aWM.IsAlphabeticalBaseline()) ? borderBoxSize
                                                    : borderBoxSize / 2;
  }
  MOZ_ASSERT(aGroup == BaselineSharingGroup::Last);
  // Round up for central baseline offset, to be consistent with ::First.
  auto borderBoxCenter = (borderBoxSize / 2) + (borderBoxSize % 2);
  return MOZ_LIKELY(aWM.IsAlphabeticalBaseline()) ? 0 : borderBoxCenter;
}

nscoord nsIFrame::SynthesizeBaselineBOffsetFromContentBox(
    mozilla::WritingMode aWM, BaselineSharingGroup aGroup) const {
  mozilla::WritingMode wm = GetWritingMode();
  MOZ_ASSERT(!aWM.IsOrthogonalTo(wm));
  const auto bp = GetLogicalUsedBorderAndPadding(wm)
                      .ApplySkipSides(GetLogicalSkipSides())
                      .ConvertTo(aWM, wm);

  if (MOZ_UNLIKELY(aWM.IsCentralBaseline())) {
    nscoord contentBoxBSize = BSize(aWM) - bp.BStartEnd(aWM);
    if (aGroup == BaselineSharingGroup::First) {
      return contentBoxBSize / 2 + bp.BStart(aWM);
    }
    // Return the same center position as for ::First, but as offset from end:
    nscoord halfContentBoxBSize = (contentBoxBSize / 2) + (contentBoxBSize % 2);
    return halfContentBoxBSize + bp.BEnd(aWM);
  }
  if (aGroup == BaselineSharingGroup::First) {
    // First baseline for inverted-line content is the block-start content
    // edge, as the frame is in effect "flipped" for alignment purposes.
    return MOZ_UNLIKELY(aWM.IsLineInverted()) ? bp.BStart(aWM)
                                              : BSize(aWM) - bp.BEnd(aWM);
  }
  // Last baseline for inverted-line content is the block-start content edge,
  // as the frame is in effect "flipped" for alignment purposes.
  return MOZ_UNLIKELY(aWM.IsLineInverted()) ? BSize(aWM) - bp.BStart(aWM)
                                            : bp.BEnd(aWM);
}

nscoord nsIFrame::BaselineBOffset(mozilla::WritingMode aWM,
                                  BaselineSharingGroup aBaselineGroup,
                                  AlignmentContext aAlignmentContext) const {
  MOZ_ASSERT(!aWM.IsOrthogonalTo(GetWritingMode()));
  nscoord baseline;
  if (GetNaturalBaselineBOffset(aWM, aBaselineGroup, &baseline)) {
    return baseline;
  }
  if (aAlignmentContext == AlignmentContext::Inline) {
    return SynthesizeBaselineBOffsetFromMarginBox(aWM, aBaselineGroup);
  }
  if (aAlignmentContext == AlignmentContext::Table) {
    return SynthesizeBaselineBOffsetFromContentBox(aWM, aBaselineGroup);
  }
  return SynthesizeBaselineBOffsetFromBorderBox(aWM, aBaselineGroup);
}

void nsIFrame::PropagateWritingModeToSelfAndAncestors(
    mozilla::WritingMode aWM) {
  MOZ_ASSERT(IsCanvasFrame());
  for (auto f = this; f; f = f->GetParent()) {
    f->mWritingMode = aWM;
  }
}

nsContainerFrame* nsIFrame::GetInFlowParent() const {
  if (GetStateBits() & NS_FRAME_OUT_OF_FLOW) {
    nsIFrame* ph =
        FirstContinuation()->GetProperty(nsIFrame::PlaceholderFrameProperty());
    return ph->GetParent();
  }

  return GetParent();
}

// We generally want to follow the style tree for preserve-3d, jumping through
// display: contents.
//
// There are various fun mismatches between the flattened tree and the frame
// tree which makes this non-trivial to do looking at the frame tree state:
//
//  - Anon boxes. You'd have to step through them, because you generally want to
//    ignore them.
//
//  - IB-splits, which produce a frame tree where frames for the block inside
//    the inline are not children of any frame from the inline.
//
//  - display: contents, which makes DOM ancestors not have frames even when a
//    descendant does.
//
// See GetFlattenedTreeParentElementForStyle for the difference between it and
// plain GetFlattenedTreeParentElement.
nsIFrame* nsIFrame::GetClosestFlattenedTreeAncestorPrimaryFrame() const {
  if (!mContent) {
    return nullptr;
  }
  mozilla::dom::Element* parent =
      mContent->GetFlattenedTreeParentElementForStyle();
  while (parent) {
    if (nsIFrame* frame = parent->GetPrimaryFrame()) {
      return frame;
    }
    // NOTE(emilio): This should be an assert except we have code in tree which
    // violates invariants like the <frameset> frame construction code.
    if (MOZ_UNLIKELY(!parent->IsDisplayContents())) {
      return nullptr;
    }
    parent = parent->GetFlattenedTreeParentElementForStyle();
  }
  return nullptr;
}

nsPoint nsIFrame::GetNormalPosition(bool* aHasProperty) const {
  nsPoint* normalPosition = GetProperty(NormalPositionProperty());
  if (normalPosition) {
    if (aHasProperty) {
      *aHasProperty = true;
    }
    return *normalPosition;
  }
  if (aHasProperty) {
    *aHasProperty = false;
  }
  return GetPosition();
}

mozilla::LogicalPoint nsIFrame::GetLogicalNormalPosition(
    mozilla::WritingMode aWritingMode, const nsSize& aContainerSize) const {
  // Subtract the size of this frame from the container size to get
  // the correct position in rtl frames where the origin is on the
  // right instead of the left
  return mozilla::LogicalPoint(aWritingMode, GetNormalPosition(),
                               aContainerSize - mRect.Size());
}

#endif
