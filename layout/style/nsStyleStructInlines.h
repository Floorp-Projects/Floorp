/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
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
#include "nsIContent.h" // for GetParent()
#include "nsTextFrame.h" // for nsTextFrame::ShouldSuppressLineBreak

inline void
nsStyleImage::SetSubImage(uint8_t aIndex, imgIContainer* aSubImage) const
{
  const_cast<nsStyleImage*>(this)->mSubImages.ReplaceObjectAt(aSubImage, aIndex);
}

inline imgIContainer*
nsStyleImage::GetSubImage(uint8_t aIndex) const
{
  imgIContainer* subImage = nullptr;
  if (aIndex < mSubImages.Count())
    subImage = mSubImages[aIndex];
  return subImage;
}

bool
nsStyleText::HasTextShadow() const
{
  return mTextShadow;
}

nsCSSShadowArray*
nsStyleText::GetTextShadow() const
{
  return mTextShadow;
}

bool
nsStyleText::NewlineIsSignificant(const nsTextFrame* aContextFrame) const
{
  NS_ASSERTION(aContextFrame->StyleText() == this, "unexpected aContextFrame");
  return NewlineIsSignificantStyle() &&
         !aContextFrame->ShouldSuppressLineBreak() &&
         !aContextFrame->StyleContext()->IsTextCombined();
}

bool
nsStyleText::WhiteSpaceCanWrap(const nsIFrame* aContextFrame) const
{
  NS_ASSERTION(aContextFrame->StyleText() == this, "unexpected aContextFrame");
  return WhiteSpaceCanWrapStyle() && !aContextFrame->IsSVGText() &&
         !aContextFrame->StyleContext()->IsTextCombined();
}

bool
nsStyleText::WordCanWrap(const nsIFrame* aContextFrame) const
{
  NS_ASSERTION(aContextFrame->StyleText() == this, "unexpected aContextFrame");
  return WordCanWrapStyle() && !aContextFrame->IsSVGText();
}

bool
nsStyleDisplay::IsBlockInside(const nsIFrame* aContextFrame) const
{
  NS_ASSERTION(aContextFrame->StyleDisplay() == this, "unexpected aContextFrame");
  if (aContextFrame->IsSVGText()) {
    return aContextFrame->GetType() == nsGkAtoms::blockFrame;
  }
  return IsBlockInsideStyle();
}

bool
nsStyleDisplay::IsBlockOutside(const nsIFrame* aContextFrame) const
{
  NS_ASSERTION(aContextFrame->StyleDisplay() == this, "unexpected aContextFrame");
  if (aContextFrame->IsSVGText()) {
    return aContextFrame->GetType() == nsGkAtoms::blockFrame;
  }
  return IsBlockOutsideStyle();
}

bool
nsStyleDisplay::IsInlineOutside(const nsIFrame* aContextFrame) const
{
  NS_ASSERTION(aContextFrame->StyleDisplay() == this, "unexpected aContextFrame");
  if (aContextFrame->IsSVGText()) {
    return aContextFrame->GetType() != nsGkAtoms::blockFrame;
  }
  return IsInlineOutsideStyle();
}

bool
nsStyleDisplay::IsOriginalDisplayInlineOutside(const nsIFrame* aContextFrame) const
{
  NS_ASSERTION(aContextFrame->StyleDisplay() == this, "unexpected aContextFrame");
  if (aContextFrame->IsSVGText()) {
    return aContextFrame->GetType() != nsGkAtoms::blockFrame;
  }
  return IsOriginalDisplayInlineOutsideStyle();
}

uint8_t
nsStyleDisplay::GetDisplay(const nsIFrame* aContextFrame) const
{
  NS_ASSERTION(aContextFrame->StyleDisplay() == this, "unexpected aContextFrame");
  if (aContextFrame->IsSVGText() &&
      mDisplay != NS_STYLE_DISPLAY_NONE) {
    return aContextFrame->GetType() == nsGkAtoms::blockFrame ?
             NS_STYLE_DISPLAY_BLOCK :
             NS_STYLE_DISPLAY_INLINE;
  }
  return mDisplay;
}

bool
nsStyleDisplay::IsFloating(const nsIFrame* aContextFrame) const
{
  NS_ASSERTION(aContextFrame->StyleDisplay() == this, "unexpected aContextFrame");
  return IsFloatingStyle() && !aContextFrame->IsSVGText();
}

// If you change this function, also change the corresponding block in
// nsCSSFrameConstructor::ConstructFrameFromItemInternal that references
// this function in comments.
bool
nsStyleDisplay::HasTransform(const nsIFrame* aContextFrame) const
{
  NS_ASSERTION(aContextFrame->StyleDisplay() == this, "unexpected aContextFrame");
  return HasTransformStyle() && aContextFrame->IsFrameOfType(nsIFrame::eSupportsCSSTransforms);
}

bool
nsStyleDisplay::IsFixedPosContainingBlock(const nsIFrame* aContextFrame) const
{
  // NOTE: Any CSS properties that influence the output of this function
  // should have the CSS_PROPERTY_FIXPOS_CB set on them.
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return (IsContainPaint() || HasTransform(aContextFrame) ||
          HasPerspectiveStyle() ||
          (mWillChangeBitField & NS_STYLE_WILL_CHANGE_FIXPOS_CB) ||
          aContextFrame->StyleEffects()->HasFilters()) &&
      !aContextFrame->IsSVGText();
}

bool
nsStyleDisplay::IsAbsPosContainingBlock(const nsIFrame* aContextFrame) const
{
  NS_ASSERTION(aContextFrame->StyleDisplay() == this,
               "unexpected aContextFrame");
  return ((IsAbsolutelyPositionedStyle() ||
           IsRelativelyPositionedStyle() ||
           (mWillChangeBitField & NS_STYLE_WILL_CHANGE_ABSPOS_CB)) &&
          !aContextFrame->IsSVGText()) ||
         IsFixedPosContainingBlock(aContextFrame);
}

bool
nsStyleDisplay::IsRelativelyPositioned(const nsIFrame* aContextFrame) const
{
  NS_ASSERTION(aContextFrame->StyleDisplay() == this, "unexpected aContextFrame");
  return IsRelativelyPositionedStyle() && !aContextFrame->IsSVGText();
}

bool
nsStyleDisplay::IsAbsolutelyPositioned(const nsIFrame* aContextFrame) const
{
  NS_ASSERTION(aContextFrame->StyleDisplay() == this, "unexpected aContextFrame");
  return IsAbsolutelyPositionedStyle() && !aContextFrame->IsSVGText();
}

uint8_t
nsStyleUserInterface::GetEffectivePointerEvents(nsIFrame* aFrame) const
{
  if (aFrame->GetContent() && !aFrame->GetContent()->GetParent()) {
    // The root element has a cluster of frames associated with it
    // (root scroll frame, canvas frame, the actual primary frame). Make
    // those take their pointer-events value from the root element's primary
    // frame.
    nsIFrame* f = aFrame->GetContent()->GetPrimaryFrame();
    if (f) {
      return f->StyleUserInterface()->mPointerEvents;
    }
  }
  return mPointerEvents;
}

#endif /* !defined(nsStyleStructInlines_h_) */
