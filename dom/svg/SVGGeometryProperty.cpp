/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGGeometryProperty.h"
#include "SVGCircleElement.h"
#include "SVGEllipseElement.h"
#include "SVGForeignObjectElement.h"
#include "SVGRectElement.h"

namespace mozilla {
namespace dom {
namespace SVGGeometryProperty {

nsCSSUnit SpecifiedUnitTypeToCSSUnit(uint8_t aSpecifiedUnit) {
  switch (aSpecifiedUnit) {
    case SVGLength_Binding::SVG_LENGTHTYPE_NUMBER:
    case SVGLength_Binding::SVG_LENGTHTYPE_PX:
      return nsCSSUnit::eCSSUnit_Pixel;

    case SVGLength_Binding::SVG_LENGTHTYPE_MM:
      return nsCSSUnit::eCSSUnit_Millimeter;

    case SVGLength_Binding::SVG_LENGTHTYPE_CM:
      return nsCSSUnit::eCSSUnit_Centimeter;

    case SVGLength_Binding::SVG_LENGTHTYPE_IN:
      return nsCSSUnit::eCSSUnit_Inch;

    case SVGLength_Binding::SVG_LENGTHTYPE_PT:
      return nsCSSUnit::eCSSUnit_Point;

    case SVGLength_Binding::SVG_LENGTHTYPE_PC:
      return nsCSSUnit::eCSSUnit_Pica;

    case SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE:
      return nsCSSUnit::eCSSUnit_Percent;

    case SVGLength_Binding::SVG_LENGTHTYPE_EMS:
      return nsCSSUnit::eCSSUnit_EM;

    case SVGLength_Binding::SVG_LENGTHTYPE_EXS:
      return nsCSSUnit::eCSSUnit_XHeight;

    default:
      MOZ_ASSERT_UNREACHABLE("Unknown unit type");
      return nsCSSUnit::eCSSUnit_Pixel;
  }
}

nsCSSPropertyID AttrEnumToCSSPropId(const SVGElement* aElement,
                                    uint8_t aAttrEnum) {
  // This is a very trivial function only applied to a few elements,
  // so we want to avoid making it virtual.
  if (aElement->IsSVGElement(nsGkAtoms::rect)) {
    return SVGRectElement::GetCSSPropertyIdForAttrEnum(aAttrEnum);
  }
  if (aElement->IsSVGElement(nsGkAtoms::circle)) {
    return SVGCircleElement::GetCSSPropertyIdForAttrEnum(aAttrEnum);
  }
  if (aElement->IsSVGElement(nsGkAtoms::ellipse)) {
    return SVGEllipseElement::GetCSSPropertyIdForAttrEnum(aAttrEnum);
  }
  if (aElement->IsSVGElement(nsGkAtoms::foreignObject)) {
    return SVGForeignObjectElement::GetCSSPropertyIdForAttrEnum(aAttrEnum);
  }
  return eCSSProperty_UNKNOWN;
}

bool IsNonNegativeGeometryProperty(nsCSSPropertyID aProp) {
  return aProp == eCSSProperty_r || aProp == eCSSProperty_rx ||
         aProp == eCSSProperty_ry || aProp == eCSSProperty_width ||
         aProp == eCSSProperty_height;
}

bool ElementMapsLengthsToStyle(SVGElement const* aElement) {
  return aElement->IsSVGElement(nsGkAtoms::rect) ||
         aElement->IsSVGElement(nsGkAtoms::circle) ||
         aElement->IsSVGElement(nsGkAtoms::ellipse) ||
         aElement->IsSVGElement(nsGkAtoms::foreignObject);
}

}  // namespace SVGGeometryProperty
}  // namespace dom
}  // namespace mozilla
