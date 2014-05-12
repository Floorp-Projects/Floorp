/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/SVGMaskElement.h"
#include "mozilla/dom/SVGMaskElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Mask)

namespace mozilla {
namespace dom {

JSObject*
SVGMaskElement::WrapNode(JSContext *aCx)
{
  return SVGMaskElementBinding::Wrap(aCx, this);
}

//--------------------- Masks ------------------------

nsSVGElement::LengthInfo SVGMaskElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, -10, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::y, -10, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
  { &nsGkAtoms::width, 120, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::height, 120, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
};

nsSVGElement::EnumInfo SVGMaskElement::sEnumInfo[2] =
{
  { &nsGkAtoms::maskUnits,
    sSVGUnitTypesMap,
    SVG_UNIT_TYPE_OBJECTBOUNDINGBOX
  },
  { &nsGkAtoms::maskContentUnits,
    sSVGUnitTypesMap,
    SVG_UNIT_TYPE_USERSPACEONUSE
  }
};

//----------------------------------------------------------------------
// Implementation

SVGMaskElement::SVGMaskElement(already_AddRefed<nsINodeInfo>& aNodeInfo)
  : SVGMaskElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode method

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGMaskElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedEnumeration>
SVGMaskElement::MaskUnits()
{
  return mEnumAttributes[MASKUNITS].ToDOMAnimatedEnum(this);
}

already_AddRefed<SVGAnimatedEnumeration>
SVGMaskElement::MaskContentUnits()
{
  return mEnumAttributes[MASKCONTENTUNITS].ToDOMAnimatedEnum(this);
}

already_AddRefed<SVGAnimatedLength>
SVGMaskElement::X()
{
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGMaskElement::Y()
{
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGMaskElement::Width()
{
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGMaskElement::Height()
{
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ bool
SVGMaskElement::HasValidDimensions() const
{
  return (!mLengthAttributes[ATTR_WIDTH].IsExplicitlySet() ||
           mLengthAttributes[ATTR_WIDTH].GetAnimValInSpecifiedUnits() > 0) &&
         (!mLengthAttributes[ATTR_HEIGHT].IsExplicitlySet() ||
           mLengthAttributes[ATTR_HEIGHT].GetAnimValInSpecifiedUnits() > 0);
}

nsSVGElement::LengthAttributesInfo
SVGMaskElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

nsSVGElement::EnumAttributesInfo
SVGMaskElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGMaskElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFEFloodMap,
    sFillStrokeMap,
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
    SVGMaskElementBase::IsAttributeMapped(name);
}

} // namespace dom
} // namespace mozilla
