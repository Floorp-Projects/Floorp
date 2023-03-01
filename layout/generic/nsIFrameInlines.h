/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIFrameInlines_h___
#define nsIFrameInlines_h___

#include "mozilla/dom/ElementInlines.h"
#include "mozilla/ComputedStyleInlines.h"
#include "nsContainerFrame.h"
#include "nsLayoutUtils.h"
#include "nsPlaceholderFrame.h"
#include "nsCSSAnonBoxes.h"
#include "nsFrameManager.h"

bool nsIFrame::IsFlexItem() const {
  return GetParent() && GetParent()->IsFlexContainerFrame() &&
         !HasAnyStateBits(NS_FRAME_OUT_OF_FLOW);
}

bool nsIFrame::IsGridItem() const {
  return GetParent() && GetParent()->IsGridContainerFrame() &&
         !HasAnyStateBits(NS_FRAME_OUT_OF_FLOW);
}

bool nsIFrame::IsFlexOrGridContainer() const {
  return IsFlexContainerFrame() || IsGridContainerFrame();
}

bool nsIFrame::IsFlexOrGridItem() const {
  return !HasAnyStateBits(NS_FRAME_OUT_OF_FLOW) && GetParent() &&
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

bool nsIFrame::IsFloating() const {
  return HasAnyStateBits(NS_FRAME_OUT_OF_FLOW) &&
         StyleDisplay()->IsFloating(this);
}

bool nsIFrame::IsAbsPosContainingBlock() const {
  return Style()->IsAbsPosContainingBlock(this);
}

bool nsIFrame::IsFixedPosContainingBlock() const {
  return Style()->IsFixedPosContainingBlock(this);
}

bool nsIFrame::IsRelativelyOrStickyPositioned() const {
  return StyleDisplay()->IsRelativelyOrStickyPositioned(this);
}

bool nsIFrame::IsRelativelyPositioned() const {
  return StyleDisplay()->IsRelativelyPositioned(this);
}

bool nsIFrame::IsStickyPositioned() const {
  return StyleDisplay()->IsStickyPositioned(this);
}

bool nsIFrame::IsAbsolutelyPositioned(
    const nsStyleDisplay* aStyleDisplay) const {
  return HasAnyStateBits(NS_FRAME_OUT_OF_FLOW) &&
         StyleDisplayWithOptionalParam(aStyleDisplay)
             ->IsAbsolutelyPositioned(this);
}

inline bool nsIFrame::IsTrueOverflowContainer() const {
  return HasAnyStateBits(NS_FRAME_IS_OVERFLOW_CONTAINER) &&
         !IsAbsolutelyPositioned();
  // XXXfr This check isn't quite correct, because it doesn't handle cases
  //      where the out-of-flow has overflow.. but that's rare.
  //      We'll need to revisit the way abspos continuations are handled later
  //      for various reasons, this detail is one of them. See bug 154892
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

void nsIFrame::PropagateWritingModeToSelfAndAncestors(
    mozilla::WritingMode aWM) {
  MOZ_ASSERT(IsCanvasFrame());
  for (auto f = this; f; f = f->GetParent()) {
    f->mWritingMode = aWM;
  }
}

nsContainerFrame* nsIFrame::GetInFlowParent() const {
  if (HasAnyStateBits(NS_FRAME_OUT_OF_FLOW)) {
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
  bool hasProperty;
  nsPoint normalPosition = GetProperty(NormalPositionProperty(), &hasProperty);
  if (aHasProperty) {
    *aHasProperty = hasProperty;
  }
  return hasProperty ? normalPosition : GetPosition();
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
