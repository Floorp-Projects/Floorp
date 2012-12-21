/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsSVGMaskElement.h"

using namespace mozilla;

//--------------------- Masks ------------------------

nsSVGElement::LengthInfo nsSVGMaskElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, -10, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::y, -10, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
  { &nsGkAtoms::width, 120, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::height, 120, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
};

nsSVGElement::EnumInfo nsSVGMaskElement::sEnumInfo[2] =
{
  { &nsGkAtoms::maskUnits,
    sSVGUnitTypesMap,
    nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX
  },
  { &nsGkAtoms::maskContentUnits,
    sSVGUnitTypesMap,
    nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE
  }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Mask)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGMaskElement,nsSVGMaskElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGMaskElement,nsSVGMaskElementBase)

DOMCI_NODE_DATA(SVGMaskElement, nsSVGMaskElement)

NS_INTERFACE_TABLE_HEAD(nsSVGMaskElement)
  NS_NODE_INTERFACE_TABLE6(nsSVGMaskElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTests,
                           nsIDOMSVGMaskElement, nsIDOMSVGUnitTypes)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGMaskElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGMaskElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGMaskElement::nsSVGMaskElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGMaskElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode method

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGMaskElement)

//----------------------------------------------------------------------
// nsIDOMSVGMaskElement methods

/* readonly attribute nsIDOMSVGAnimatedEnumeration maskUnits; */
NS_IMETHODIMP nsSVGMaskElement::GetMaskUnits(nsIDOMSVGAnimatedEnumeration * *aMaskUnits)
{
  return mEnumAttributes[MASKUNITS].ToDOMAnimatedEnum(aMaskUnits, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration maskContentUnits; */
NS_IMETHODIMP nsSVGMaskElement::GetMaskContentUnits(nsIDOMSVGAnimatedEnumeration * *aMaskUnits)
{
  return mEnumAttributes[MASKCONTENTUNITS].ToDOMAnimatedEnum(aMaskUnits, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP nsSVGMaskElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  return mLengthAttributes[X].ToDOMAnimatedLength(aX, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP nsSVGMaskElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  return mLengthAttributes[Y].ToDOMAnimatedLength(aY, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP nsSVGMaskElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  return mLengthAttributes[WIDTH].ToDOMAnimatedLength(aWidth, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP nsSVGMaskElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  return mLengthAttributes[HEIGHT].ToDOMAnimatedLength(aHeight, this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ bool
nsSVGMaskElement::HasValidDimensions() const
{
  return (!mLengthAttributes[WIDTH].IsExplicitlySet() ||
           mLengthAttributes[WIDTH].GetAnimValInSpecifiedUnits() > 0) &&
         (!mLengthAttributes[HEIGHT].IsExplicitlySet() || 
           mLengthAttributes[HEIGHT].GetAnimValInSpecifiedUnits() > 0);
}

nsSVGElement::LengthAttributesInfo
nsSVGMaskElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

nsSVGElement::EnumAttributesInfo
nsSVGMaskElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGMaskElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFEFloodMap,
    sFiltersMap,
    sFontSpecificationMap,
    sGradientStopMap,
    sGraphicsMap,
    sMarkersMap,
    sMaskMap,
    sTextContentElementsMap,
    sViewportsMap
  };

  return FindAttributeDependence(name, map) ||
    nsSVGMaskElementBase::IsAttributeMapped(name);
}
