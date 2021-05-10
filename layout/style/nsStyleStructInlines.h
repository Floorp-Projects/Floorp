/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Inline methods that belong in nsStyleStruct.h, except that they
 * require more headers.
 */

#ifndef nsStyleStructInlines_h_
#define nsStyleStructInlines_h_

#include "nsIFrame.h"
#include "nsStyleStruct.h"
#include "nsIContent.h"        // for GetParent()
#include "nsTextFrame.h"       // for nsTextFrame::ShouldSuppressLineBreak
#include "mozilla/SVGUtils.h"  // for SVGUtils::IsInSVGTextSubtree

bool nsStyleText::NewlineIsSignificant(const nsTextFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleText() == this, "unexpected aContextFrame");
  return NewlineIsSignificantStyle() &&
         !aContextFrame->ShouldSuppressLineBreak() &&
         !aContextFrame->Style()->IsTextCombined();
}

bool nsStyleText::WhiteSpaceCanWrap(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleText() == this, "unexpected aContextFrame");
  return WhiteSpaceCanWrapStyle() &&
         !mozilla::SVGUtils::IsInSVGTextSubtree(aContextFrame) &&
         !aContextFrame->Style()->IsTextCombined();
}

bool nsStyleText::WordCanWrap(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleText() == this, "unexpected aContextFrame");
  return WordCanWrapStyle() &&
         !mozilla::SVGUtils::IsInSVGTextSubtree(aContextFrame);
}

bool nsStyleDisplay::IsBlockOutside(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  if (mozilla::SVGUtils::IsInSVGTextSubtree(aContextFrame)) {
    return aContextFrame->IsBlockFrame();
  }
  return IsBlockOutsideStyle();
}

bool nsStyleDisplay::IsInlineOutside(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  if (mozilla::SVGUtils::IsInSVGTextSubtree(aContextFrame)) {
    return !aContextFrame->IsBlockFrame();
  }
  return IsInlineOutsideStyle();
}

mozilla::StyleDisplay nsStyleDisplay::GetDisplay(
    const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  if (mozilla::SVGUtils::IsInSVGTextSubtree(aContextFrame) &&
      mDisplay != mozilla::StyleDisplay::None) {
    return aContextFrame->IsBlockFrame() ? mozilla::StyleDisplay::Block
                                         : mozilla::StyleDisplay::Inline;
  }
  return mDisplay;
}

bool nsStyleDisplay::IsFloating(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return IsFloatingStyle() &&
         !mozilla::SVGUtils::IsInSVGTextSubtree(aContextFrame);
}

// If you change this function, also change the corresponding block in
// nsCSSFrameConstructor::ConstructFrameFromItemInternal that references
// this function in comments.
bool nsStyleDisplay::HasTransform(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return HasTransformStyle() &&
         aContextFrame->IsFrameOfType(nsIFrame::eSupportsCSSTransforms);
}

bool nsStyleDisplay::HasPerspective(const nsIFrame* aContextFrame) const {
  MOZ_ASSERT(aContextFrame->StyleDisplay() == this, "unexpected aContextFrame");
  return HasPerspectiveStyle() &&
         aContextFrame->IsFrameOfType(nsIFrame::eSupportsCSSTransforms);
}

bool nsStyleDisplay::IsFixedPosContainingBlockForNonSVGTextFrames(
    const mozilla::ComputedStyle& aStyle) const {
  // NOTE: Any CSS properties that influence the output of this function
  // should return FIXPOS_CB_NON_SVG for will-change.
  NS_ASSERTION(aStyle.StyleDisplay() == this, "unexpected aStyle");

  if (mWillChange.bits & mozilla::StyleWillChangeBits::FIXPOS_CB_NON_SVG) {
    return true;
  }

  return aStyle.StyleEffects()->HasFilters() ||
         aStyle.StyleEffects()->HasBackdropFilters();
}

bool nsStyleDisplay::
    IsFixedPosContainingBlockForContainLayoutAndPaintSupportingFrames() const {
  return IsContainPaint() || IsContainLayout() ||
         mWillChange.bits & mozilla::StyleWillChangeBits::CONTAIN;
}

bool nsStyleDisplay::IsFixedPosContainingBlockForTransformSupportingFrames()
    const {
  // NOTE: Any CSS properties that influence the output of this function
  // should also look at mWillChange as necessary.
  return HasTransformStyle() || HasPerspectiveStyle() ||
         mWillChange.bits & mozilla::StyleWillChangeBits::PERSPECTIVE;
}

bool nsStyleDisplay::IsFixedPosContainingBlock(
    const nsIFrame* aContextFrame) const {
  const auto* style = aContextFrame->Style();
  NS_ASSERTION(style->StyleDisplay() == this, "unexpected aContextFrame");
  // NOTE: Any CSS properties that influence the output of this function
  // should also handle will-change appropriately.
  if (mozilla::SVGUtils::IsInSVGTextSubtree(aContextFrame)) {
    return false;
  }
  if (IsFixedPosContainingBlockForNonSVGTextFrames(*style)) {
    return true;
  }
  if (IsFixedPosContainingBlockForContainLayoutAndPaintSupportingFrames() &&
      aContextFrame->IsFrameOfType(nsIFrame::eSupportsContainLayoutAndPaint)) {
    return true;
  }
  if (IsFixedPosContainingBlockForTransformSupportingFrames() &&
      aContextFrame->IsFrameOfType(nsIFrame::eSupportsCSSTransforms)) {
    return true;
  }
  return false;
}

bool nsStyleDisplay::IsAbsPosContainingBlock(
    const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this, "unexpected aContextFrame");
  if (IsFixedPosContainingBlock(aContextFrame)) {
    return true;
  }
  // NOTE: Any CSS properties that influence the output of this function
  // should also handle will-change appropriately.
  return IsPositionedStyle() &&
         !mozilla::SVGUtils::IsInSVGTextSubtree(aContextFrame);
}

bool nsStyleDisplay::IsRelativelyPositioned(
    const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return IsRelativelyPositionedStyle() &&
         !mozilla::SVGUtils::IsInSVGTextSubtree(aContextFrame);
}

bool nsStyleDisplay::IsStickyPositioned(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return IsStickyPositionedStyle() &&
         !mozilla::SVGUtils::IsInSVGTextSubtree(aContextFrame);
}

bool nsStyleDisplay::IsAbsolutelyPositioned(
    const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return IsAbsolutelyPositionedStyle() &&
         !mozilla::SVGUtils::IsInSVGTextSubtree(aContextFrame);
}

mozilla::StylePointerEvents nsStyleUI::GetEffectivePointerEvents(
    nsIFrame* aFrame) const {
  if (aFrame->GetContent() && !aFrame->GetContent()->GetParent()) {
    // The root frame is not allowed to have pointer-events: none, or else
    // no frames could be hit test against and scrolling the viewport would
    // not work.
    return mozilla::StylePointerEvents::Auto;
  }
  return mPointerEvents;
}

bool nsStyleBackground::HasLocalBackground() const {
  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, mImage) {
    const nsStyleImageLayers::Layer& layer = mImage.mLayers[i];
    if (!layer.mImage.IsNone() &&
        layer.mAttachment == mozilla::StyleImageLayerAttachment::Local) {
      return true;
    }
  }
  return false;
}

#endif /* !defined(nsStyleStructInlines_h_) */
