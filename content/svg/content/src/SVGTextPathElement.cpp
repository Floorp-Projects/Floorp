/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGTextPathElement.h"
#include "mozilla/dom/SVGTextPathElementBinding.h"
#include "nsSVGElement.h"
#include "nsGkAtoms.h"
#include "nsIDOMSVGTextPathElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsIFrame.h"
#include "nsError.h"
#include "nsContentUtils.h"

DOMCI_NODE_DATA(SVGTextPathElement, mozilla::dom::SVGTextPathElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(TextPath)

namespace mozilla {
namespace dom {

JSObject*
SVGTextPathElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGTextPathElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

nsSVGElement::LengthInfo SVGTextPathElement::sLengthInfo[1] =
{
  { &nsGkAtoms::startOffset, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, SVGContentUtils::X },
};

nsSVGEnumMapping SVGTextPathElement::sMethodMap[] = {
  {&nsGkAtoms::align, nsIDOMSVGTextPathElement::TEXTPATH_METHODTYPE_ALIGN},
  {&nsGkAtoms::stretch, nsIDOMSVGTextPathElement::TEXTPATH_METHODTYPE_STRETCH},
  {nullptr, 0}
};

nsSVGEnumMapping SVGTextPathElement::sSpacingMap[] = {
  {&nsGkAtoms::_auto, nsIDOMSVGTextPathElement::TEXTPATH_SPACINGTYPE_AUTO},
  {&nsGkAtoms::exact, nsIDOMSVGTextPathElement::TEXTPATH_SPACINGTYPE_EXACT},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGTextPathElement::sEnumInfo[2] =
{
  { &nsGkAtoms::method,
    sMethodMap,
    nsIDOMSVGTextPathElement::TEXTPATH_METHODTYPE_ALIGN
  },
  { &nsGkAtoms::spacing,
    sSpacingMap,
    nsIDOMSVGTextPathElement::TEXTPATH_SPACINGTYPE_EXACT
  }
};

nsSVGElement::StringInfo SVGTextPathElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, true }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGTextPathElement,SVGTextPathElementBase)
NS_IMPL_RELEASE_INHERITED(SVGTextPathElement,SVGTextPathElementBase)

NS_INTERFACE_TABLE_HEAD(SVGTextPathElement)
  NS_NODE_INTERFACE_TABLE6(SVGTextPathElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTextPathElement,
                           nsIDOMSVGTextContentElement,
                           nsIDOMSVGURIReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGTextPathElement)
NS_INTERFACE_MAP_END_INHERITING(SVGTextPathElementBase)

//----------------------------------------------------------------------
// Implementation

SVGTextPathElement::SVGTextPathElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGTextPathElementBase(aNodeInfo)
{
  SetIsDOMBinding();
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGTextPathElement)

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP SVGTextPathElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  *aHref = Href().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedString>
SVGTextPathElement::Href()
{
  nsCOMPtr<nsIDOMSVGAnimatedString> href;
  mStringAttributes[HREF].ToDOMAnimatedString(getter_AddRefs(href), this);
  return href.forget();
}

//----------------------------------------------------------------------
// nsIDOMSVGTextPathElement methods

NS_IMETHODIMP SVGTextPathElement::GetStartOffset(nsIDOMSVGAnimatedLength * *aStartOffset)
{
  *aStartOffset = StartOffset().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedLength>
SVGTextPathElement::StartOffset()
{
  return mLengthAttributes[STARTOFFSET].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration method; */
NS_IMETHODIMP SVGTextPathElement::GetMethod(nsIDOMSVGAnimatedEnumeration * *aMethod)
{
  *aMethod = Method().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedEnumeration>
SVGTextPathElement::Method()
{
  return mEnumAttributes[METHOD].ToDOMAnimatedEnum(this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration spacing; */
NS_IMETHODIMP SVGTextPathElement::GetSpacing(nsIDOMSVGAnimatedEnumeration * *aSpacing)
{
  *aSpacing = Spacing().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedEnumeration>
SVGTextPathElement::Spacing()
{
  return mEnumAttributes[SPACING].ToDOMAnimatedEnum(this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGTextPathElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFillStrokeMap,
    sFontSpecificationMap,
    sGraphicsMap,
    sTextContentElementsMap
  };

  return FindAttributeDependence(name, map) ||
    SVGTextPathElementBase::IsAttributeMapped(name);
}


bool
SVGTextPathElement::IsEventAttributeName(nsIAtom* aName)
{
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_SVGGraphic);
}

//----------------------------------------------------------------------
// nsSVGElement overrides

nsSVGElement::LengthAttributesInfo
SVGTextPathElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

nsSVGElement::EnumAttributesInfo
SVGTextPathElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
SVGTextPathElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
