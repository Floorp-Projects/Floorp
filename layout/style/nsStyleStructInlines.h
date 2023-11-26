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
#include "nsIContent.h"   // for GetParent()
#include "nsTextFrame.h"  // for nsTextFrame::ShouldSuppressLineBreak

bool nsStyleText::NewlineIsSignificant(const nsTextFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleText() == this, "unexpected aContextFrame");
  return NewlineIsSignificantStyle() &&
         !aContextFrame->ShouldSuppressLineBreak() &&
         !aContextFrame->Style()->IsTextCombined();
}

bool nsStyleText::WhiteSpaceCanWrap(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleText() == this, "unexpected aContextFrame");
  return WhiteSpaceCanWrapStyle() && !aContextFrame->IsInSVGTextSubtree() &&
         !aContextFrame->Style()->IsTextCombined();
}

bool nsStyleText::WordCanWrap(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleText() == this, "unexpected aContextFrame");
  return WordCanWrapStyle() && !aContextFrame->IsInSVGTextSubtree();
}

bool nsStyleDisplay::IsBlockOutside(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  if (aContextFrame->IsInSVGTextSubtree()) {
    return aContextFrame->IsBlockFrame();
  }
  return IsBlockOutsideStyle();
}

bool nsStyleDisplay::IsInlineOutside(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  if (aContextFrame->IsInSVGTextSubtree()) {
    return !aContextFrame->IsBlockFrame();
  }
  return IsInlineOutsideStyle();
}

mozilla::StyleDisplay nsStyleDisplay::GetDisplay(
    const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  if (aContextFrame->IsInSVGTextSubtree() &&
      mDisplay != mozilla::StyleDisplay::None) {
    return aContextFrame->IsBlockFrame() ? mozilla::StyleDisplay::Block
                                         : mozilla::StyleDisplay::Inline;
  }
  return mDisplay;
}

bool nsStyleDisplay::IsFloating(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return IsFloatingStyle() && !aContextFrame->IsInSVGTextSubtree();
}

// If you change this function, also change the corresponding block in
// nsCSSFrameConstructor::ConstructFrameFromItemInternal that references
// this function in comments.
bool nsStyleDisplay::HasTransform(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return HasTransformStyle() && aContextFrame->SupportsCSSTransforms();
}

bool nsStyleDisplay::HasPerspective(const nsIFrame* aContextFrame) const {
  MOZ_ASSERT(aContextFrame->StyleDisplay() == this, "unexpected aContextFrame");
  return HasPerspectiveStyle() && aContextFrame->SupportsCSSTransforms();
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

bool nsStyleDisplay::IsRelativelyOrStickyPositioned(
    const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return IsRelativelyOrStickyPositionedStyle() &&
         !aContextFrame->IsInSVGTextSubtree();
}

bool nsStyleDisplay::IsRelativelyPositioned(
    const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return IsRelativelyPositionedStyle() && !aContextFrame->IsInSVGTextSubtree();
}

bool nsStyleDisplay::IsStickyPositioned(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return IsStickyPositionedStyle() && !aContextFrame->IsInSVGTextSubtree();
}

bool nsStyleDisplay::IsAbsolutelyPositioned(
    const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return IsAbsolutelyPositionedStyle() && !aContextFrame->IsInSVGTextSubtree();
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
