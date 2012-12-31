/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGTextContentElement.h"
#include "DOMSVGPoint.h"
#include "nsCOMPtr.h"

using namespace mozilla;

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGTextContentElement, nsSVGTextContentElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGTextContentElement, nsSVGTextContentElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGTextContentElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGTests)
NS_INTERFACE_MAP_END_INHERITING(nsSVGTextContentElementBase)

/* readonly attribute nsIDOMSVGAnimatedLength textLength; */
NS_IMETHODIMP nsSVGTextContentElement::GetTextLength(nsIDOMSVGAnimatedLength * *aTextLength)
{
  NS_NOTYETIMPLEMENTED("nsSVGTextContentElement::GetTextLength");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration lengthAdjust; */
NS_IMETHODIMP nsSVGTextContentElement::GetLengthAdjust(nsIDOMSVGAnimatedEnumeration * *aLengthAdjust)
{
  NS_NOTYETIMPLEMENTED("nsSVGTextContentElement::GetLengthAdjust");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* long getNumberOfChars (); */
NS_IMETHODIMP nsSVGTextContentElement::GetNumberOfChars(int32_t *_retval)
{
  *_retval = 0;

  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  if (metrics)
    *_retval = metrics->GetNumberOfChars();

  return NS_OK;
}

/* float getComputedTextLength (); */
NS_IMETHODIMP nsSVGTextContentElement::GetComputedTextLength(float *_retval)
{
  *_retval = 0.0;

  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  if (metrics)
    *_retval = metrics->GetComputedTextLength();

  return NS_OK;
}

/* float getSubStringLength (in unsigned long charnum, in unsigned long nchars); */
NS_IMETHODIMP nsSVGTextContentElement::GetSubStringLength(uint32_t charnum, uint32_t nchars, float *_retval)
{
  *_retval = 0.0f;
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  if (!metrics)
    return NS_OK;

  uint32_t charcount = metrics->GetNumberOfChars();
  if (charcount <= charnum || nchars > charcount - charnum)
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  if (nchars == 0)
    return NS_OK;

  *_retval = metrics->GetSubStringLength(charnum, nchars);
  return NS_OK;
}

/* DOMSVGPoint getStartPositionOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTextContentElement::GetStartPositionOfChar(uint32_t charnum, nsISupports **_retval)
{
  *_retval = nullptr;
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetStartPositionOfChar(charnum, _retval);
}

/* DOMSVGPoint getEndPositionOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTextContentElement::GetEndPositionOfChar(uint32_t charnum, nsISupports **_retval)
{
  *_retval = nullptr;
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetEndPositionOfChar(charnum, _retval);
}

/* nsIDOMSVGRect getExtentOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTextContentElement::GetExtentOfChar(uint32_t charnum, nsIDOMSVGRect **_retval)
{
  *_retval = nullptr;
  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetExtentOfChar(charnum, _retval);
}

/* float getRotationOfChar (in unsigned long charnum); */
NS_IMETHODIMP nsSVGTextContentElement::GetRotationOfChar(uint32_t charnum, float *_retval)
{
  *_retval = 0.0;

  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();

  if (!metrics) return NS_ERROR_FAILURE;

  return metrics->GetRotationOfChar(charnum, _retval);
}

/* long getCharNumAtPosition (in DOMSVGPoint point); */
NS_IMETHODIMP nsSVGTextContentElement::GetCharNumAtPosition(nsISupports *point, int32_t *_retval)
{
  *_retval = -1;

  nsCOMPtr<DOMSVGPoint> domPoint = do_QueryInterface(point);
  if (!domPoint) {
    return NS_ERROR_DOM_SVG_WRONG_TYPE_ERR;
  }

  nsSVGTextContainerFrame* metrics = GetTextContainerFrame();
  if (metrics)
    *_retval = metrics->GetCharNumAtPosition(domPoint);

  return NS_OK;
}

/* void selectSubString (in unsigned long charnum, in unsigned long nchars); */
NS_IMETHODIMP nsSVGTextContentElement::SelectSubString(uint32_t charnum, uint32_t nchars)
{
  NS_NOTYETIMPLEMENTED("nsSVGTextContentElement::SelectSubString");
  return NS_ERROR_NOT_IMPLEMENTED;
}
