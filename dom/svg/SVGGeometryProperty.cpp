/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGGeometryProperty.h"
#include "SVGCircleElement.h"
#include "SVGEllipseElement.h"
#include "SVGForeignObjectElement.h"
#include "SVGImageElement.h"
#include "SVGRectElement.h"
#include "SVGUseElement.h"

namespace mozilla::dom::SVGGeometryProperty {

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
  if (aElement->IsSVGElement(nsGkAtoms::image)) {
    return SVGImageElement::GetCSSPropertyIdForAttrEnum(aAttrEnum);
  }
  if (aElement->IsSVGElement(nsGkAtoms::foreignObject)) {
    return SVGForeignObjectElement::GetCSSPropertyIdForAttrEnum(aAttrEnum);
  }
  if (aElement->IsSVGElement(nsGkAtoms::use)) {
    return SVGUseElement::GetCSSPropertyIdForAttrEnum(aAttrEnum);
  }
  return eCSSProperty_UNKNOWN;
}

bool IsNonNegativeGeometryProperty(nsCSSPropertyID aProp) {
  return aProp == eCSSProperty_r || aProp == eCSSProperty_rx ||
         aProp == eCSSProperty_ry || aProp == eCSSProperty_width ||
         aProp == eCSSProperty_height;
}

bool ElementMapsLengthsToStyle(SVGElement const* aElement) {
  return aElement->IsAnyOfSVGElements(nsGkAtoms::rect, nsGkAtoms::circle,
                                      nsGkAtoms::ellipse, nsGkAtoms::image,
                                      nsGkAtoms::foreignObject, nsGkAtoms::use);
}

}  // namespace mozilla::dom::SVGGeometryProperty
