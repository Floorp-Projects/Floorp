/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Robert Longson <longsonr@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/Util.h"

#include "nsSVGTextPositioningElement.h"
#include "SVGAnimatedLengthList.h"
#include "DOMSVGAnimatedLengthList.h"
#include "SVGLengthList.h"
#include "DOMSVGAnimatedNumberList.h"

using namespace mozilla;


nsSVGElement::LengthListInfo nsSVGTextPositioningElement::sLengthListInfo[4] =
{
  { &nsGkAtoms::x,  nsSVGUtils::X, PR_FALSE },
  { &nsGkAtoms::y,  nsSVGUtils::Y, PR_FALSE },
  { &nsGkAtoms::dx, nsSVGUtils::X, PR_TRUE },
  { &nsGkAtoms::dy, nsSVGUtils::Y, PR_TRUE }
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

/* readonly attribute nsIDOMSVGAnimatedLengthList x; */
NS_IMETHODIMP nsSVGTextPositioningElement::GetX(nsIDOMSVGAnimatedLengthList * *aX)
{
  *aX = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[X],
                                                this, X, nsSVGUtils::X).get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLengthList y; */
NS_IMETHODIMP nsSVGTextPositioningElement::GetY(nsIDOMSVGAnimatedLengthList * *aY)
{
  *aY = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[Y],
                                                this, Y, nsSVGUtils::Y).get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLengthList dx; */
NS_IMETHODIMP nsSVGTextPositioningElement::GetDx(nsIDOMSVGAnimatedLengthList * *aDx)
{
  *aDx = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[DX],
                                                 this, DX, nsSVGUtils::X).get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLengthList dy; */
NS_IMETHODIMP nsSVGTextPositioningElement::GetDy(nsIDOMSVGAnimatedLengthList * *aDy)
{
  *aDy = DOMSVGAnimatedLengthList::GetDOMWrapper(&mLengthListAttributes[DY],
                                                 this, DY, nsSVGUtils::Y).get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedNumberList rotate; */
NS_IMETHODIMP nsSVGTextPositioningElement::GetRotate(nsIDOMSVGAnimatedNumberList * *aRotate)
{
  *aRotate = DOMSVGAnimatedNumberList::GetDOMWrapper(&mNumberListAttributes[ROTATE],
                                                     this, ROTATE).get();
  return NS_OK;
}
