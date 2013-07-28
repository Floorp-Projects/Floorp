/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGTextContentElement.h"
#include "nsISVGPoint.h"
#include "nsSVGTextContainerFrame.h"
#include "nsSVGTextFrame2.h"
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

nsSVGTextContainerFrame*
SVGTextContentElement::GetTextContainerFrame()
{
  return do_QueryFrame(GetPrimaryFrame(Flush_Layout));
}

nsSVGTextFrame2*
SVGTextContentElement::GetSVGTextFrame()
{
  nsIFrame* frame = GetPrimaryFrame(Flush_Layout);
  while (frame) {
    nsSVGTextFrame2* textFrame = do_QueryFrame(frame);
    if (textFrame) {
      return textFrame;
    }
    frame = frame->GetParent();
  }
  return nullptr;
}

bool
SVGTextContentElement::FrameIsSVGText()
{
  nsIFrame* frame = GetPrimaryFrame(Flush_Layout);
  return frame && frame->IsSVGText();
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
  if (FrameIsSVGText()) {
    nsSVGTextFrame2* textFrame = GetSVGTextFrame();
    return textFrame ? textFrame->GetNumberOfChars(this) : 0;
  } else {
    nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
    return metrics ? metrics->GetNumberOfChars() : 0;
  }
}

float
SVGTextContentElement::GetComputedTextLength()
{
  if (FrameIsSVGText()) {
    nsSVGTextFrame2* textFrame = GetSVGTextFrame();
    return textFrame ? textFrame->GetComputedTextLength(this) : 0.0f;
  } else {
    nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
    return metrics ? metrics->GetComputedTextLength() : 0.0f;
  }
}

void
SVGTextContentElement::SelectSubString(uint32_t charnum, uint32_t nchars, ErrorResult& rv)
{
  if (FrameIsSVGText()) {
    nsSVGTextFrame2* textFrame = GetSVGTextFrame();
    if (!textFrame)
      return;

    rv = textFrame->SelectSubString(this, charnum, nchars);
  } else {
    rv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  }
}

float
SVGTextContentElement::GetSubStringLength(uint32_t charnum, uint32_t nchars, ErrorResult& rv)
{
  if (FrameIsSVGText()) {
    nsSVGTextFrame2* textFrame = GetSVGTextFrame();
    if (!textFrame)
      return 0.0f;

    float length = 0.0f;
    rv = textFrame->GetSubStringLength(this, charnum, nchars, &length);
    return length;
  } else {
    nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
    if (!metrics)
      return 0.0f;

    uint32_t charcount = metrics->GetNumberOfChars();
    if (charcount <= charnum || nchars > charcount - charnum) {
      rv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return 0.0f;
    }

    if (nchars == 0)
      return 0.0f;

    return metrics->GetSubStringLength(charnum, nchars);
  }
}

already_AddRefed<nsISVGPoint>
SVGTextContentElement::GetStartPositionOfChar(uint32_t charnum, ErrorResult& rv)
{
  nsCOMPtr<nsISVGPoint> point;
  if (FrameIsSVGText()) {
    nsSVGTextFrame2* textFrame = GetSVGTextFrame();
    if (!textFrame) {
      rv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    rv = textFrame->GetStartPositionOfChar(this, charnum, getter_AddRefs(point));
  } else {
    nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

    if (!metrics) {
      rv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    rv = metrics->GetStartPositionOfChar(charnum, getter_AddRefs(point));
  }
  return point.forget();
}

already_AddRefed<nsISVGPoint>
SVGTextContentElement::GetEndPositionOfChar(uint32_t charnum, ErrorResult& rv)
{
  nsCOMPtr<nsISVGPoint> point;
  if (FrameIsSVGText()) {
    nsSVGTextFrame2* textFrame = GetSVGTextFrame();
    if (!textFrame) {
      rv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    rv = textFrame->GetEndPositionOfChar(this, charnum, getter_AddRefs(point));
  } else {
    nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

    if (!metrics) {
      rv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    rv = metrics->GetEndPositionOfChar(charnum, getter_AddRefs(point));
  }
  return point.forget();
}

already_AddRefed<SVGIRect>
SVGTextContentElement::GetExtentOfChar(uint32_t charnum, ErrorResult& rv)
{
  nsRefPtr<SVGIRect> rect;
  if (FrameIsSVGText()) {
    nsSVGTextFrame2* textFrame = GetSVGTextFrame();

    if (!textFrame) {
      rv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    rv = textFrame->GetExtentOfChar(this, charnum, getter_AddRefs(rect));
  } else {
    nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

    if (!metrics) {
      rv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    rv = metrics->GetExtentOfChar(charnum, getter_AddRefs(rect));
  }
  return rect.forget();
}

float
SVGTextContentElement::GetRotationOfChar(uint32_t charnum, ErrorResult& rv)
{
  float rotation;
  if (FrameIsSVGText()) {
    nsSVGTextFrame2* textFrame = GetSVGTextFrame();

    if (!textFrame) {
      rv.Throw(NS_ERROR_FAILURE);
      return 0.0f;
    }

    rv = textFrame->GetRotationOfChar(this, charnum, &rotation);
  } else {
    nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

    if (!metrics) {
      rv.Throw(NS_ERROR_FAILURE);
      return 0.0f;
    }

    rv = metrics->GetRotationOfChar(charnum, &rotation);
  }
  return rotation;
}

int32_t
SVGTextContentElement::GetCharNumAtPosition(nsISVGPoint& aPoint)
{
  if (FrameIsSVGText()) {
    nsSVGTextFrame2* textFrame = GetSVGTextFrame();
    return textFrame ? textFrame->GetCharNumAtPosition(this, &aPoint) : -1;
  } else {
    nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
    return metrics ? metrics->GetCharNumAtPosition(&aPoint) : -1;
  }
}

} // namespace dom
} // namespace mozilla
