/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "mozilla/dom/SVGTextPositioningElement.h"
#include "SVGAnimatedLengthList.h"
#include "DOMSVGAnimatedLengthList.h"
#include "DOMSVGAnimatedNumberList.h"
#include "SVGContentUtils.h"

namespace mozilla {
namespace dom {

nsSVGElement::LengthListInfo SVGTextPositioningElement::sLengthListInfo[4] =
{
  { &nsGkAtoms::x,  SVGContentUtils::X, false },
  { &nsGkAtoms::y,  SVGContentUtils::Y, false },
  { &nsGkAtoms::dx, SVGContentUtils::X, true },
  { &nsGkAtoms::dy, SVGContentUtils::Y, true }
};

nsSVGElement::LengthListAttributesInfo
SVGTextPositioningElement::GetLengthListInfo()
{
  return LengthListAttributesInfo(mLengthListAttributes, sLengthListInfo,
                                  ArrayLength(sLengthListInfo));
}


nsSVGElement::NumberListInfo SVGTextPositioningElement::sNumberListInfo[1] =
{
  { &nsGkAtoms::rotate }
};

nsSVGElement::NumberListAttributesInfo
SVGTextPositioningElement::GetNumberListInfo()
{
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  ArrayLength(sNumberListInfo));
}

//----------------------------------------------------------------------

already_AddRefed<DOMSVGAnimatedLengthList>
SVGTextPositioningElement::X()
{
  return DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[ATTR_X],
                                                 this, ATTR_X, SVGContentUtils::X);
}

already_AddRefed<DOMSVGAnimatedLengthList>
SVGTextPositioningElement::Y()
{
  return DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[ATTR_Y],
                                                 this, ATTR_Y, SVGContentUtils::Y);
}

already_AddRefed<DOMSVGAnimatedLengthList>
SVGTextPositioningElement::Dx()
{
  return DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[ATTR_DX],
                                                 this, ATTR_DX, SVGContentUtils::X);
}

already_AddRefed<DOMSVGAnimatedLengthList>
SVGTextPositioningElement::Dy()
{
  return DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[ATTR_DY],
                                                 this, ATTR_DY, SVGContentUtils::Y);
}

already_AddRefed<DOMSVGAnimatedNumberList>
SVGTextPositioningElement::Rotate()
{
  return DOMSVGAnimatedNumberList::GetDOMWrapper(&mNumberListAttributes[ROTATE],
                                                 this, ROTATE);
}

} // namespace dom
} // namespace mozilla
