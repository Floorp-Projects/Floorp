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
#include "nsSVGUtils.h"   // for nsSVGUtils::IsInSVGTextSubtree

inline void nsStyleImage::EnsureCachedBIData() const {
  if (!mCachedBIData) {
    const_cast<nsStyleImage*>(this)->mCachedBIData =
        mozilla::MakeUnique<CachedBorderImageData>();
  }
}

inline void nsStyleImage::SetSubImage(uint8_t aIndex,
                                      imgIContainer* aSubImage) const {
  EnsureCachedBIData();
  mCachedBIData->SetSubImage(aIndex, aSubImage);
}

inline imgIContainer* nsStyleImage::GetSubImage(uint8_t aIndex) const {
  return (mCachedBIData) ? mCachedBIData->GetSubImage(aIndex) : nullptr;
}

bool nsStyleText::NewlineIsSignificant(const nsTextFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleText() == this, "unexpected aContextFrame");
  return NewlineIsSignificantStyle() &&
         !aContextFrame->ShouldSuppressLineBreak() &&
         !aContextFrame->Style()->IsTextCombined();
}

bool nsStyleText::WhiteSpaceCanWrap(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleText() == this, "unexpected aContextFrame");
  return WhiteSpaceCanWrapStyle() &&
         !nsSVGUtils::IsInSVGTextSubtree(aContextFrame) &&
         !aContextFrame->Style()->IsTextCombined();
}

bool nsStyleText::WordCanWrap(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleText() == this, "unexpected aContextFrame");
  return WordCanWrapStyle() && !nsSVGUtils::IsInSVGTextSubtree(aContextFrame);
}

bool nsStyleDisplay::IsBlockInside(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  if (nsSVGUtils::IsInSVGTextSubtree(aContextFrame)) {
    return aContextFrame->IsBlockFrame();
  }
  return IsBlockInsideStyle();
}

bool nsStyleDisplay::IsBlockOutside(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  if (nsSVGUtils::IsInSVGTextSubtree(aContextFrame)) {
    return aContextFrame->IsBlockFrame();
  }
  return IsBlockOutsideStyle();
}

bool nsStyleDisplay::IsInlineOutside(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  if (nsSVGUtils::IsInSVGTextSubtree(aContextFrame)) {
    return !aContextFrame->IsBlockFrame();
  }
  return IsInlineOutsideStyle();
}

bool nsStyleDisplay::IsOriginalDisplayInlineOutside(
    const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  if (nsSVGUtils::IsInSVGTextSubtree(aContextFrame)) {
    return !aContextFrame->IsBlockFrame();
  }
  return IsOriginalDisplayInlineOutsideStyle();
}

mozilla::StyleDisplay nsStyleDisplay::GetDisplay(
    const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  if (nsSVGUtils::IsInSVGTextSubtree(aContextFrame) &&
      mDisplay != mozilla::StyleDisplay::None) {
    return aContextFrame->IsBlockFrame() ? mozilla::StyleDisplay::Block
                                         : mozilla::StyleDisplay::Inline;
  }
  return mDisplay;
}

bool nsStyleDisplay::IsFloating(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return IsFloatingStyle() && !nsSVGUtils::IsInSVGTextSubtree(aContextFrame);
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
  // should have the FIXPOS_CB flag set on them.
  NS_ASSERTION(aStyle.StyleDisplay() == this, "unexpected aStyle");

  if (mWillChange.bits & mozilla::StyleWillChangeBits_FIXPOS_CB) {
    return true;
  }

  return aStyle.StyleEffects()->HasFilters();
}

bool nsStyleDisplay::
    IsFixedPosContainingBlockForContainLayoutAndPaintSupportingFrames() const {
  return IsContainPaint() || IsContainLayout();
}

bool nsStyleDisplay::IsFixedPosContainingBlockForTransformSupportingFrames()
    const {
  // NOTE: Any CSS properties that influence the output of this function
  // should have the FIXPOS_CB flag set on them.
  return HasTransformStyle() || HasPerspectiveStyle();
}

bool nsStyleDisplay::IsFixedPosContainingBlock(
    const nsIFrame* aContextFrame) const {
  mozilla::ComputedStyle* style = aContextFrame->Style();
  NS_ASSERTION(style->StyleDisplay() == this, "unexpected aContextFrame");
  // NOTE: Any CSS properties that influence the output of this function
  // should have the FIXPOS_CB flag set on them.
  if (!IsFixedPosContainingBlockForNonSVGTextFrames(*style) &&
      (!IsFixedPosContainingBlockForContainLayoutAndPaintSupportingFrames() ||
       !aContextFrame->IsFrameOfType(
           nsIFrame::eSupportsContainLayoutAndPaint)) &&
      (!IsFixedPosContainingBlockForTransformSupportingFrames() ||
       !aContextFrame->IsFrameOfType(nsIFrame::eSupportsCSSTransforms))) {
    return false;
  }
  if (nsSVGUtils::IsInSVGTextSubtree(aContextFrame)) {
    return false;
  }
  MOZ_ASSERT(IsAbsPosContainingBlock(aContextFrame),
             "Any fixed-pos CB should also be an abs-pos CB");
  return true;
}

bool nsStyleDisplay::IsAbsPosContainingBlockForNonSVGTextFrames() const {
  // NOTE: Any CSS properties that influence the output of this function
  // should have the ABSPOS_CB set on them.
  return IsAbsolutelyPositionedStyle() || IsRelativelyPositionedStyle() ||
         (mWillChange.bits & mozilla::StyleWillChangeBits_ABSPOS_CB);
}

bool nsStyleDisplay::IsAbsPosContainingBlock(
    const nsIFrame* aContextFrame) const {
  mozilla::ComputedStyle* style = aContextFrame->Style();
  NS_ASSERTION(style->StyleDisplay() == this, "unexpected aContextFrame");
  // NOTE: Any CSS properties that influence the output of this function
  // should have the ABSPOS_CB set on them.
  if (!IsAbsPosContainingBlockForNonSVGTextFrames() &&
      !IsFixedPosContainingBlockForNonSVGTextFrames(*style) &&
      (!IsFixedPosContainingBlockForContainLayoutAndPaintSupportingFrames() ||
       !aContextFrame->IsFrameOfType(
           nsIFrame::eSupportsContainLayoutAndPaint)) &&
      (!IsFixedPosContainingBlockForTransformSupportingFrames() ||
       !aContextFrame->IsFrameOfType(nsIFrame::eSupportsCSSTransforms))) {
    return false;
  }
  return !nsSVGUtils::IsInSVGTextSubtree(aContextFrame);
}

bool nsStyleDisplay::IsRelativelyPositioned(
    const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return IsRelativelyPositionedStyle() &&
         !nsSVGUtils::IsInSVGTextSubtree(aContextFrame);
}

bool nsStyleDisplay::IsStickyPositioned(const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return IsStickyPositionedStyle() &&
         !nsSVGUtils::IsInSVGTextSubtree(aContextFrame);
}

bool nsStyleDisplay::IsAbsolutelyPositioned(
    const nsIFrame* aContextFrame) const {
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return IsAbsolutelyPositionedStyle() &&
         !nsSVGUtils::IsInSVGTextSubtree(aContextFrame);
}

uint8_t nsStyleUI::GetEffectivePointerEvents(nsIFrame* aFrame) const {
  if (aFrame->GetContent() && !aFrame->GetContent()->GetParent()) {
    // The root frame is not allowed to have pointer-events: none, or else
    // no frames could be hit test against and scrolling the viewport would
    // not work.
    return NS_STYLE_POINTER_EVENTS_AUTO;
  }
  return mPointerEvents;
}

bool nsStyleBackground::HasLocalBackground() const {
  NS_FOR_VISIBLE_IMAGE_LAYERS_BACK_TO_FRONT(i, mImage) {
    const nsStyleImageLayers::Layer& layer = mImage.mLayers[i];
    if (!layer.mImage.IsEmpty() &&
        layer.mAttachment == mozilla::StyleImageLayerAttachment::Local) {
      return true;
    }
  }
  return false;
}

#endif /* !defined(nsStyleStructInlines_h_) */
