/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGAltGlyphElement.h"
#include "mozilla/dom/SVGAltGlyphElementBinding.h"
#include "nsContentUtils.h"

DOMCI_NODE_DATA(SVGAltGlyphElement, mozilla::dom::SVGAltGlyphElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(AltGlyph)

namespace mozilla {
namespace dom {

JSObject*
SVGAltGlyphElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGAltGlyphElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

nsSVGElement::StringInfo SVGAltGlyphElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, false }
};


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGAltGlyphElement,SVGAltGlyphElementBase)
NS_IMPL_RELEASE_INHERITED(SVGAltGlyphElement,SVGAltGlyphElementBase)

NS_INTERFACE_TABLE_HEAD(SVGAltGlyphElement)
  NS_NODE_INTERFACE_TABLE7(SVGAltGlyphElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGAltGlyphElement,
                           nsIDOMSVGTextPositioningElement, nsIDOMSVGTextContentElement,
                           nsIDOMSVGURIReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAltGlyphElement)
NS_INTERFACE_MAP_END_INHERITING(SVGAltGlyphElementBase)

//----------------------------------------------------------------------
// Implementation

SVGAltGlyphElement::SVGAltGlyphElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGAltGlyphElementBase(aNodeInfo)
{
  SetIsDOMBinding();
}


//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGAltGlyphElement)

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP SVGAltGlyphElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  *aHref = Href().get();
  return NS_OK;
}

already_AddRefed<nsIDOMSVGAnimatedString>
SVGAltGlyphElement::Href()
{
  nsCOMPtr<nsIDOMSVGAnimatedString> href;
  mStringAttributes[HREF].ToDOMAnimatedString(getter_AddRefs(href), this);
  return href.forget();
}

//----------------------------------------------------------------------
// nsIDOMSVGAltGlyphElement methods

/* attribute DOMString glyphRef; */
NS_IMETHODIMP SVGAltGlyphElement::GetGlyphRef(nsAString & aGlyphRef)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::glyphRef, aGlyphRef);

  return NS_OK;
}

NS_IMETHODIMP SVGAltGlyphElement::SetGlyphRef(const nsAString & aGlyphRef)
{
  return SetAttr(kNameSpaceID_None, nsGkAtoms::glyphRef, aGlyphRef, true);
}

/* attribute DOMString format; */
NS_IMETHODIMP SVGAltGlyphElement::GetFormat(nsAString & aFormat)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::format, aFormat);

  return NS_OK;
}

NS_IMETHODIMP SVGAltGlyphElement::SetFormat(const nsAString & aFormat)
{
  return SetAttr(kNameSpaceID_None, nsGkAtoms::format, aFormat, true);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGAltGlyphElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFillStrokeMap,
    sFontSpecificationMap,
    sGraphicsMap,
    sTextContentElementsMap
  };

  return FindAttributeDependence(name, map) ||
    SVGAltGlyphElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement overrides

bool
SVGAltGlyphElement::IsEventName(nsIAtom* aName)
{
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_SVGGraphic);
}

nsSVGElement::StringAttributesInfo
SVGAltGlyphElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
