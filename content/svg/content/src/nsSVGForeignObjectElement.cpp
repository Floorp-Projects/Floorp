/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsCOMPtr.h"
#include "nsSVGForeignObjectElement.h"

using namespace mozilla;

nsSVGElement::LengthInfo nsSVGForeignObjectElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
  { &nsGkAtoms::width, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
  { &nsGkAtoms::height, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::Y },
};

NS_IMPL_NS_NEW_SVG_ELEMENT(ForeignObject)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGForeignObjectElement,nsSVGForeignObjectElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGForeignObjectElement,nsSVGForeignObjectElementBase)

DOMCI_NODE_DATA(SVGForeignObjectElement, nsSVGForeignObjectElement)

NS_INTERFACE_TABLE_HEAD(nsSVGForeignObjectElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGForeignObjectElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTests,
                           nsIDOMSVGForeignObjectElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGForeignObjectElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGForeignObjectElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGForeignObjectElement::nsSVGForeignObjectElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGForeignObjectElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGForeignObjectElement)

//----------------------------------------------------------------------
// nsIDOMSVGForeignObjectElement methods:

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP nsSVGForeignObjectElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  return mLengthAttributes[X].ToDOMAnimatedLength(aX, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP nsSVGForeignObjectElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  return mLengthAttributes[Y].ToDOMAnimatedLength(aY, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP nsSVGForeignObjectElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  return mLengthAttributes[WIDTH].ToDOMAnimatedLength(aWidth, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP nsSVGForeignObjectElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  return mLengthAttributes[HEIGHT].ToDOMAnimatedLength(aHeight, this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ gfxMatrix
nsSVGForeignObjectElement::PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                                                    TransformTypes aWhich) const
{
  NS_ABORT_IF_FALSE(aWhich != eChildToUserSpace || aMatrix.IsIdentity(),
                    "Skipping eUserSpaceToParent transforms makes no sense");

  // 'transform' attribute:
  gfxMatrix fromUserSpace =
    nsSVGForeignObjectElementBase::PrependLocalTransformsTo(aMatrix, aWhich);
  if (aWhich == eUserSpaceToParent) {
    return fromUserSpace;
  }
  // our 'x' and 'y' attributes:
  float x, y;
  const_cast<nsSVGForeignObjectElement*>(this)->
    GetAnimatedLengthValues(&x, &y, nsnull);
  gfxMatrix toUserSpace = gfxMatrix().Translate(gfxPoint(x, y));
  if (aWhich == eChildToUserSpace) {
    return toUserSpace;
  }
  NS_ABORT_IF_FALSE(aWhich == eAllTransforms, "Unknown TransformTypes");
  return toUserSpace * fromUserSpace;
}

/* virtual */ bool
nsSVGForeignObjectElement::HasValidDimensions() const
{
  return mLengthAttributes[WIDTH].IsExplicitlySet() &&
         mLengthAttributes[WIDTH].GetAnimValInSpecifiedUnits() > 0 &&
         mLengthAttributes[HEIGHT].IsExplicitlySet() &&
         mLengthAttributes[HEIGHT].GetAnimValInSpecifiedUnits() > 0;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGForeignObjectElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFEFloodMap,
    sFiltersMap,
    sFontSpecificationMap,
    sGradientStopMap,
    sLightingEffectsMap,
    sMarkersMap,
    sTextContentElementsMap,
    sViewportsMap
  };

  return FindAttributeDependence(name, map) ||
    nsSVGForeignObjectElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthAttributesInfo
nsSVGForeignObjectElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}
