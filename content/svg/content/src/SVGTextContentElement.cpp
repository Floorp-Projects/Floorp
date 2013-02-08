/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGTextContentElement.h"
#include "nsISVGPoint.h"
#include "nsSVGTextContainerFrame.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGRect.h"
#include "nsIDOMSVGAnimatedEnum.h"

namespace mozilla {
namespace dom {

nsSVGTextContainerFrame*
SVGTextContentElement::GetTextContainerFrame()
{
  return do_QueryFrame(GetPrimaryFrame(Flush_Layout));
}

//----------------------------------------------------------------------

int32_t
SVGTextContentElement::GetNumberOfChars()
{
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  return metrics ? metrics->GetNumberOfChars() : 0;
}

float
SVGTextContentElement::GetComputedTextLength()
{
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  return metrics ? metrics->GetComputedTextLength() : 0.0f;
}

float
SVGTextContentElement::GetSubStringLength(uint32_t charnum, uint32_t nchars, ErrorResult& rv)
{
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

already_AddRefed<nsISVGPoint>
SVGTextContentElement::GetStartPositionOfChar(uint32_t charnum, ErrorResult& rv)
{
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) {
    rv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsISVGPoint> point;
  rv = metrics->GetStartPositionOfChar(charnum, getter_AddRefs(point));
  return point.forget();
}

already_AddRefed<nsISVGPoint>
SVGTextContentElement::GetEndPositionOfChar(uint32_t charnum, ErrorResult& rv)
{
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) {
    rv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsISVGPoint> point;
  rv = metrics->GetEndPositionOfChar(charnum, getter_AddRefs(point));
  return point.forget();
}

already_AddRefed<nsIDOMSVGRect>
SVGTextContentElement::GetExtentOfChar(uint32_t charnum, ErrorResult& rv)
{
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) {
    rv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIDOMSVGRect> rect;
  rv = metrics->GetExtentOfChar(charnum, getter_AddRefs(rect));
  return rect.forget();
}

float
SVGTextContentElement::GetRotationOfChar(uint32_t charnum, ErrorResult& rv)
{
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) {
    rv.Throw(NS_ERROR_FAILURE);
    return 0.0f;
  }

  float _retval;
  rv = metrics->GetRotationOfChar(charnum, &_retval);
  return _retval;
}

int32_t
SVGTextContentElement::GetCharNumAtPosition(nsISVGPoint& aPoint)
{
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  return metrics ? metrics->GetCharNumAtPosition(&aPoint) : -1;
}

} // namespace dom
} // namespace mozilla
