/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGTextContentElement.h"
#include "nsISVGPoint.h"
#include "SVGTextFrame.h"
#include "mozilla/dom/SVGIRect.h"

namespace mozilla {
namespace dom {

nsSVGEnumMapping SVGTextContentElement::sLengthAdjustMap[] = {
  { &nsGkAtoms::spacing, SVG_LENGTHADJUST_SPACING },
  { &nsGkAtoms::spacingAndGlyphs, SVG_LENGTHADJUST_SPACINGANDGLYPHS },
  { nullptr, 0 }
};

nsSVGElement::EnumInfo SVGTextContentElement::sEnumInfo[1] =
{
  { &nsGkAtoms::lengthAdjust, sLengthAdjustMap, SVG_LENGTHADJUST_SPACING }
};

nsSVGElement::LengthInfo SVGTextContentElement::sLengthInfo[1] =
{
  { &nsGkAtoms::textLength, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::XY }
};

SVGTextFrame*
SVGTextContentElement::GetSVGTextFrame()
{
  nsIFrame* frame = GetPrimaryFrame(Flush_Layout);
  while (frame) {
    SVGTextFrame* textFrame = do_QueryFrame(frame);
    if (textFrame) {
      return textFrame;
    }
    frame = frame->GetParent();
  }
  return nullptr;
}

already_AddRefed<SVGAnimatedLength>
SVGTextContentElement::TextLength()
{
  return LengthAttributes()[TEXTLENGTH].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedEnumeration>
SVGTextContentElement::LengthAdjust()
{
  return EnumAttributes()[LENGTHADJUST].ToDOMAnimatedEnum(this);
}

//----------------------------------------------------------------------

int32_t
SVGTextContentElement::GetNumberOfChars()
{
  SVGTextFrame* textFrame = GetSVGTextFrame();
  return textFrame ? textFrame->GetNumberOfChars(this) : 0;
}

float
SVGTextContentElement::GetComputedTextLength()
{
  SVGTextFrame* textFrame = GetSVGTextFrame();
  return textFrame ? textFrame->GetComputedTextLength(this) : 0.0f;
}

void
SVGTextContentElement::SelectSubString(uint32_t charnum, uint32_t nchars, ErrorResult& rv)
{
  SVGTextFrame* textFrame = GetSVGTextFrame();
  if (!textFrame)
    return;

  rv = textFrame->SelectSubString(this, charnum, nchars);
}

float
SVGTextContentElement::GetSubStringLength(uint32_t charnum, uint32_t nchars, ErrorResult& rv)
{
  SVGTextFrame* textFrame = GetSVGTextFrame();
  if (!textFrame)
    return 0.0f;

  float length = 0.0f;
  rv = textFrame->GetSubStringLength(this, charnum, nchars, &length);
  return length;
}

already_AddRefed<nsISVGPoint>
SVGTextContentElement::GetStartPositionOfChar(uint32_t charnum, ErrorResult& rv)
{
  SVGTextFrame* textFrame = GetSVGTextFrame();
  if (!textFrame) {
    rv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsISVGPoint> point;
  rv = textFrame->GetStartPositionOfChar(this, charnum, getter_AddRefs(point));
  return point.forget();
}

already_AddRefed<nsISVGPoint>
SVGTextContentElement::GetEndPositionOfChar(uint32_t charnum, ErrorResult& rv)
{
  SVGTextFrame* textFrame = GetSVGTextFrame();
  if (!textFrame) {
    rv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsISVGPoint> point;
  rv = textFrame->GetEndPositionOfChar(this, charnum, getter_AddRefs(point));
  return point.forget();
}

already_AddRefed<SVGIRect>
SVGTextContentElement::GetExtentOfChar(uint32_t charnum, ErrorResult& rv)
{
  SVGTextFrame* textFrame = GetSVGTextFrame();

  if (!textFrame) {
    rv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsRefPtr<SVGIRect> rect;
  rv = textFrame->GetExtentOfChar(this, charnum, getter_AddRefs(rect));
  return rect.forget();
}

float
SVGTextContentElement::GetRotationOfChar(uint32_t charnum, ErrorResult& rv)
{
  SVGTextFrame* textFrame = GetSVGTextFrame();

  if (!textFrame) {
    rv.Throw(NS_ERROR_FAILURE);
    return 0.0f;
  }

  float rotation;
  rv = textFrame->GetRotationOfChar(this, charnum, &rotation);
  return rotation;
}

int32_t
SVGTextContentElement::GetCharNumAtPosition(nsISVGPoint& aPoint)
{
  SVGTextFrame* textFrame = GetSVGTextFrame();
  return textFrame ? textFrame->GetCharNumAtPosition(this, &aPoint) : -1;
}

} // namespace dom
} // namespace mozilla
