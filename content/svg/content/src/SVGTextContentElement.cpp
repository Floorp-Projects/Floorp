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
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGTextContentElement, SVGTextContentElementBase)
NS_IMPL_RELEASE_INHERITED(SVGTextContentElement, SVGTextContentElementBase)

NS_INTERFACE_MAP_BEGIN(SVGTextContentElement)
NS_INTERFACE_MAP_END_INHERITING(SVGTextContentElementBase)

/* long getNumberOfChars (); */
NS_IMETHODIMP SVGTextContentElement::GetNumberOfChars(int32_t *_retval)
{
  *_retval = GetNumberOfChars();
  return NS_OK;
}

int32_t
SVGTextContentElement::GetNumberOfChars()
{
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  return metrics ? metrics->GetNumberOfChars() : 0;
}

/* float getComputedTextLength (); */
NS_IMETHODIMP SVGTextContentElement::GetComputedTextLength(float *_retval)
{
  *_retval = GetComputedTextLength();
  return NS_OK;
}

float
SVGTextContentElement::GetComputedTextLength()
{
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  return metrics ? metrics->GetComputedTextLength() : 0.0f;
}

/* float getSubStringLength (in unsigned long charnum, in unsigned long nchars); */
NS_IMETHODIMP SVGTextContentElement::GetSubStringLength(uint32_t charnum, uint32_t nchars, float *_retval)
{
  ErrorResult rv;
  *_retval = GetSubStringLength(charnum, nchars, rv);
  return rv.ErrorCode();
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

/* DOMSVGPoint getStartPositionOfChar (in unsigned long charnum); */
NS_IMETHODIMP SVGTextContentElement::GetStartPositionOfChar(uint32_t charnum, nsISupports **_retval)
{
  ErrorResult rv;
  *_retval = GetStartPositionOfChar(charnum, rv).get();
  return rv.ErrorCode();
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

/* DOMSVGPoint getEndPositionOfChar (in unsigned long charnum); */
NS_IMETHODIMP SVGTextContentElement::GetEndPositionOfChar(uint32_t charnum, nsISupports **_retval)
{
  ErrorResult rv;
  *_retval = GetEndPositionOfChar(charnum, rv).get();
  return rv.ErrorCode();
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

/* nsIDOMSVGRect getExtentOfChar (in unsigned long charnum); */
NS_IMETHODIMP SVGTextContentElement::GetExtentOfChar(uint32_t charnum, nsIDOMSVGRect **_retval)
{
  ErrorResult rv;
  *_retval = GetExtentOfChar(charnum, rv).get();
  return rv.ErrorCode();
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

/* float getRotationOfChar (in unsigned long charnum); */
NS_IMETHODIMP SVGTextContentElement::GetRotationOfChar(uint32_t charnum, float *_retval)
{
  ErrorResult rv;
  *_retval = GetRotationOfChar(charnum, rv);
  return rv.ErrorCode();
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

/* long getCharNumAtPosition (in DOMSVGPoint point); */
NS_IMETHODIMP SVGTextContentElement::GetCharNumAtPosition(nsISupports *point, int32_t *_retval)
{
  nsCOMPtr<nsISVGPoint> domPoint = do_QueryInterface(point);
  if (!domPoint) {
    *_retval = -1;
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }

  *_retval = GetCharNumAtPosition(*domPoint);
  return NS_OK;
}

int32_t
SVGTextContentElement::GetCharNumAtPosition(nsISVGPoint& aPoint)
{
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  return metrics ? metrics->GetCharNumAtPosition(&aPoint) : -1;
}

} // namespace dom
} // namespace mozilla
