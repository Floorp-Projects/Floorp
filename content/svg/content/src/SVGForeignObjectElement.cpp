/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsCOMPtr.h"
#include "mozilla/dom/SVGForeignObjectElement.h"
#include "mozilla/dom/SVGForeignObjectElementBinding.h"

DOMCI_NODE_DATA(SVGForeignObjectElement, mozilla::dom::SVGForeignObjectElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(ForeignObject)

namespace mozilla {
namespace dom {

JSObject*
SVGForeignObjectElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGForeignObjectElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

nsSVGElement::LengthInfo SVGForeignObjectElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
  { &nsGkAtoms::width, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
  { &nsGkAtoms::height, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::Y },
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGForeignObjectElement, SVGGraphicsElement)
NS_IMPL_RELEASE_INHERITED(SVGForeignObjectElement, SVGGraphicsElement)

NS_INTERFACE_TABLE_HEAD(SVGForeignObjectElement)
  NS_NODE_INTERFACE_TABLE4(SVGForeignObjectElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGForeignObjectElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGForeignObjectElement)
NS_INTERFACE_MAP_END_INHERITING(SVGGraphicsElement)

//----------------------------------------------------------------------
// Implementation

SVGForeignObjectElement::SVGForeignObjectElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGGraphicsElement(aNodeInfo)
{
  SetIsDOMBinding();
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGForeignObjectElement)

//----------------------------------------------------------------------
// nsIDOMSVGForeignObjectElement methods:

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP SVGForeignObjectElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  *aX = X().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGForeignObjectElement::X()
{
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP SVGForeignObjectElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  *aY = Y().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGForeignObjectElement::Y()
{
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}


/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP SVGForeignObjectElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  *aWidth = Width().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGForeignObjectElement::Width()
{
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}


/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP SVGForeignObjectElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  *aHeight = Height().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGForeignObjectElement::Height()
{
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

/* virtual */ gfxMatrix
SVGForeignObjectElement::PrependLocalTransformsTo(const gfxMatrix &aMatrix,
                                                  TransformTypes aWhich) const
{
  NS_ABORT_IF_FALSE(aWhich != eChildToUserSpace || aMatrix.IsIdentity(),
                    "Skipping eUserSpaceToParent transforms makes no sense");

  // 'transform' attribute:
  gfxMatrix fromUserSpace =
    SVGGraphicsElement::PrependLocalTransformsTo(aMatrix, aWhich);
  if (aWhich == eUserSpaceToParent) {
    return fromUserSpace;
  }
  // our 'x' and 'y' attributes:
  float x, y;
  const_cast<SVGForeignObjectElement*>(this)->
    GetAnimatedLengthValues(&x, &y, nullptr);
  gfxMatrix toUserSpace = gfxMatrix().Translate(gfxPoint(x, y));
  if (aWhich == eChildToUserSpace) {
    return toUserSpace;
  }
  NS_ABORT_IF_FALSE(aWhich == eAllTransforms, "Unknown TransformTypes");
  return toUserSpace * fromUserSpace;
}

/* virtual */ bool
SVGForeignObjectElement::HasValidDimensions() const
{
  return mLengthAttributes[ATTR_WIDTH].IsExplicitlySet() &&
         mLengthAttributes[ATTR_WIDTH].GetAnimValInSpecifiedUnits() > 0 &&
         mLengthAttributes[ATTR_HEIGHT].IsExplicitlySet() &&
         mLengthAttributes[ATTR_HEIGHT].GetAnimValInSpecifiedUnits() > 0;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGForeignObjectElement::IsAttributeMapped(const nsIAtom* name) const
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
    SVGGraphicsElement::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthAttributesInfo
SVGForeignObjectElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

} // namespace dom
} // namespace mozilla

