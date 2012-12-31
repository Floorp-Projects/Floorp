/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsSVGTextPositioningElement.h"
#include "SVGAnimatedLengthList.h"
#include "DOMSVGAnimatedLengthList.h"
#include "DOMSVGAnimatedNumberList.h"
#include "SVGContentUtils.h"
#include "SVGLengthList.h"

using namespace mozilla;


nsSVGElement::LengthListInfo nsSVGTextPositioningElement::sLengthListInfo[4] =
{
  { &nsGkAtoms::x,  SVGContentUtils::X, false },
  { &nsGkAtoms::y,  SVGContentUtils::Y, false },
  { &nsGkAtoms::dx, SVGContentUtils::X, true },
  { &nsGkAtoms::dy, SVGContentUtils::Y, true }
};

nsSVGElement::LengthListAttributesInfo
nsSVGTextPositioningElement::GetLengthListInfo()
{
  return LengthListAttributesInfo(mLengthListAttributes, sLengthListInfo,
                                  ArrayLength(sLengthListInfo));
}


nsSVGElement::NumberListInfo nsSVGTextPositioningElement::sNumberListInfo[1] =
{
  { &nsGkAtoms::rotate }
};

nsSVGElement::NumberListAttributesInfo
nsSVGTextPositioningElement::GetNumberListInfo()
{
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  ArrayLength(sNumberListInfo));
}

//----------------------------------------------------------------------
// nsIDOMSVGTextPositioningElement methods

/* readonly attribute DOMSVGAnimatedLengthList x; */
NS_IMETHODIMP nsSVGTextPositioningElement::GetX(nsISupports * *aX)
{
  *aX = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[X],
                                                this, X, SVGContentUtils::X).get();
  return NS_OK;
}

/* readonly attribute DOMSVGAnimatedLengthList y; */
NS_IMETHODIMP nsSVGTextPositioningElement::GetY(nsISupports * *aY)
{
  *aY = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[Y],
                                                this, Y, SVGContentUtils::Y).get();
  return NS_OK;
}

/* readonly attribute DOMSVGAnimatedLengthList dx; */
NS_IMETHODIMP nsSVGTextPositioningElement::GetDx(nsISupports * *aDx)
{
  *aDx = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[DX],
                                                 this, DX, SVGContentUtils::X).get();
  return NS_OK;
}

/* readonly attribute DOMSVGAnimatedLengthList dy; */
NS_IMETHODIMP nsSVGTextPositioningElement::GetDy(nsISupports * *aDy)
{
  *aDy = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[DY],
                                                 this, DY, SVGContentUtils::Y).get();
  return NS_OK;
}

/* readonly attribute DOMSVGAnimatedNumberList rotate; */
NS_IMETHODIMP nsSVGTextPositioningElement::GetRotate(nsISupports * *aRotate)
{
  *aRotate = DOMSVGAnimatedNumberList::GetDOMWrapper(&mNumberListAttributes[ROTATE],
                                                     this, ROTATE).get();
  return NS_OK;
}
