/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "DOMSVGAnimatedTransformList.h"
#include "nsIDOMMutationEvent.h"
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/SVGPatternElement.h"
#include "mozilla/dom/SVGPatternElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Pattern)

DOMCI_NODE_DATA(SVGPatternElement, mozilla::dom::SVGPatternElement)

namespace mozilla {
namespace dom {

JSObject*
SVGPatternElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGPatternElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

//--------------------- Patterns ------------------------

nsSVGElement::LengthInfo SVGPatternElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
  { &nsGkAtoms::width, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::height, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
};

nsSVGElement::EnumInfo SVGPatternElement::sEnumInfo[2] =
{
  { &nsGkAtoms::patternUnits,
    sSVGUnitTypesMap,
    nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX
  },
  { &nsGkAtoms::patternContentUnits,
    sSVGUnitTypesMap,
    nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE
  }
};

nsSVGElement::StringInfo SVGPatternElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, true }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGPatternElement,SVGPatternElementBase)
NS_IMPL_RELEASE_INHERITED(SVGPatternElement,SVGPatternElementBase)

NS_INTERFACE_TABLE_HEAD(SVGPatternElement)
  NS_NODE_INTERFACE_TABLE7(SVGPatternElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGFitToViewBox, nsIDOMSVGURIReference,
                           nsIDOMSVGPatternElement, nsIDOMSVGUnitTypes)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGPatternElement)
NS_INTERFACE_MAP_END_INHERITING(SVGPatternElementBase)

//----------------------------------------------------------------------
// Implementation

SVGPatternElement::SVGPatternElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGPatternElementBase(aNodeInfo)
{
  SetIsDOMBinding();
}

//----------------------------------------------------------------------
// nsIDOMNode method

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGPatternElement)

//----------------------------------------------------------------------
// nsIDOMSVGFitToViewBox methods

/* readonly attribute nsIDOMSVGAnimatedRect viewBox; */
NS_IMETHODIMP SVGPatternElement::GetViewBox(nsIDOMSVGAnimatedRect * *aViewBox)
{
  *aViewBox = ViewBox().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedRect>
SVGPatternElement::ViewBox()
{
  nsCOMPtr<nsIDOMSVGAnimatedRect> rect;
  mViewBox.ToDOMAnimatedRect(getter_AddRefs(rect), this);
  return rect.forget();
}

/* readonly attribute SVGPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
SVGPatternElement::GetPreserveAspectRatio(nsISupports **aPreserveAspectRatio)
{
  *aPreserveAspectRatio = PreserveAspectRatio().get();
  return NS_OK;
}

already_AddRefed<DOMSVGAnimatedPreserveAspectRatio>
SVGPatternElement::PreserveAspectRatio()
{
  nsRefPtr<DOMSVGAnimatedPreserveAspectRatio> ratio;
  mPreserveAspectRatio.ToDOMAnimatedPreserveAspectRatio(getter_AddRefs(ratio), this);
  return ratio.forget();
}

//----------------------------------------------------------------------
// nsIDOMSVGPatternElement methods

/* readonly attribute nsIDOMSVGAnimatedEnumeration patternUnits; */
NS_IMETHODIMP SVGPatternElement::GetPatternUnits(nsIDOMSVGAnimatedEnumeration * *aPatternUnits)
{
  *aPatternUnits = PatternUnits().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedEnumeration>
SVGPatternElement::PatternUnits()
{
  return mEnumAttributes[PATTERNUNITS].ToDOMAnimatedEnum(this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration patternContentUnits; */
NS_IMETHODIMP SVGPatternElement::GetPatternContentUnits(nsIDOMSVGAnimatedEnumeration * *aPatternUnits)
{
  *aPatternUnits = PatternContentUnits().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedEnumeration>
SVGPatternElement::PatternContentUnits()
{
  return mEnumAttributes[PATTERNCONTENTUNITS].ToDOMAnimatedEnum(this);
}

/* readonly attribute nsISupports patternTransform; */
NS_IMETHODIMP SVGPatternElement::GetPatternTransform(nsISupports * *aPatternTransform)
{
  *aPatternTransform = PatternTransform().get();
  return NS_OK;
}

already_AddRefed<DOMSVGAnimatedTransformList>
SVGPatternElement::PatternTransform()
{
  // We're creating a DOM wrapper, so we must tell GetAnimatedTransformList
  // to allocate the SVGAnimatedTransformList if it hasn't already done so:
  return DOMSVGAnimatedTransformList::GetDOMWrapper(
           GetAnimatedTransformList(DO_ALLOCATE), this);
}

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP SVGPatternElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  *aX = X().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGPatternElement::X()
{
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP SVGPatternElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  *aY = Y().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGPatternElement::Y()
{
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP SVGPatternElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  *aWidth = Width().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGPatternElement::Width()
{
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP SVGPatternElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  *aHeight = Height().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGPatternElement::Height()
{
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods:

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP
SVGPatternElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  *aHref = Href().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedString>
SVGPatternElement::Href()
{
  nsCOMPtr<nsIDOMSVGAnimatedString> href;
  mStringAttributes[HREF].ToDOMAnimatedString(getter_AddRefs(href), this);
  return href.forget();
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGPatternElement::IsAttributeMapped(const nsIAtom* name) const
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
    SVGPatternElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

SVGAnimatedTransformList*
SVGPatternElement::GetAnimatedTransformList(uint32_t aFlags)
{
  if (!mPatternTransform && (aFlags & DO_ALLOCATE)) {
    mPatternTransform = new SVGAnimatedTransformList();
  }
  return mPatternTransform;
}

/* virtual */ bool
SVGPatternElement::HasValidDimensions() const
{
  return mLengthAttributes[ATTR_WIDTH].IsExplicitlySet() &&
         mLengthAttributes[ATTR_WIDTH].GetAnimValInSpecifiedUnits() > 0 &&
         mLengthAttributes[ATTR_HEIGHT].IsExplicitlySet() &&
         mLengthAttributes[ATTR_HEIGHT].GetAnimValInSpecifiedUnits() > 0;
}

nsSVGElement::LengthAttributesInfo
SVGPatternElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

nsSVGElement::EnumAttributesInfo
SVGPatternElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGViewBox *
SVGPatternElement::GetViewBox()
{
  return &mViewBox;
}

SVGAnimatedPreserveAspectRatio *
SVGPatternElement::GetPreserveAspectRatio()
{
  return &mPreserveAspectRatio;
}

nsSVGElement::StringAttributesInfo
SVGPatternElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
