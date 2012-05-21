/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsGkAtoms.h"
#include "nsIDOMSVGAltGlyphElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsSVGString.h"
#include "nsSVGTextPositioningElement.h"
#include "nsContentUtils.h"

using namespace mozilla;

typedef nsSVGTextPositioningElement nsSVGAltGlyphElementBase;

class nsSVGAltGlyphElement : public nsSVGAltGlyphElementBase, // = nsIDOMSVGTextPositioningElement
                             public nsIDOMSVGAltGlyphElement,
                             public nsIDOMSVGURIReference
{
protected:
  friend nsresult NS_NewSVGAltGlyphElement(nsIContent **aResult,
                                           already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGAltGlyphElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  
public:
  // interfaces:
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGALTGLYPHELEMENT
  NS_DECL_NSIDOMSVGURIREFERENCE

  // xxx If xpcom allowed virtual inheritance we wouldn't need to
  // forward here :-(
  NS_FORWARD_NSIDOMNODE(nsSVGAltGlyphElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGAltGlyphElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGAltGlyphElementBase::)
  NS_FORWARD_NSIDOMSVGTEXTCONTENTELEMENT(nsSVGAltGlyphElementBase::)
  NS_FORWARD_NSIDOMSVGTEXTPOSITIONINGELEMENT(nsSVGAltGlyphElementBase::)

  // nsIContent interface
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom* aAttribute) const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
protected:

  // nsSVGElement overrides
  virtual StringAttributesInfo GetStringInfo();

  virtual bool IsEventName(nsIAtom* aName);

  enum { HREF };
  nsSVGString mStringAttributes[1];
  static StringInfo sStringInfo[1];

};

nsSVGElement::StringInfo nsSVGAltGlyphElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, false }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(AltGlyph)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGAltGlyphElement,nsSVGAltGlyphElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGAltGlyphElement,nsSVGAltGlyphElementBase)

DOMCI_NODE_DATA(SVGAltGlyphElement, nsSVGAltGlyphElement)

NS_INTERFACE_TABLE_HEAD(nsSVGAltGlyphElement)
  NS_NODE_INTERFACE_TABLE8(nsSVGAltGlyphElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGAltGlyphElement,
                           nsIDOMSVGTextPositioningElement, nsIDOMSVGTextContentElement,
                           nsIDOMSVGTests,
                           nsIDOMSVGURIReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAltGlyphElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGAltGlyphElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGAltGlyphElement::nsSVGAltGlyphElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGAltGlyphElementBase(aNodeInfo)
{
}


//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGAltGlyphElement)

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP nsSVGAltGlyphElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  return mStringAttributes[HREF].ToDOMAnimatedString(aHref, this);
}

//----------------------------------------------------------------------
// nsIDOMSVGAltGlyphElement methods

/* attribute DOMString glyphRef; */
NS_IMETHODIMP nsSVGAltGlyphElement::GetGlyphRef(nsAString & aGlyphRef)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::glyphRef, aGlyphRef);

  return NS_OK;
}

NS_IMETHODIMP nsSVGAltGlyphElement::SetGlyphRef(const nsAString & aGlyphRef)
{
  return SetAttr(kNameSpaceID_None, nsGkAtoms::glyphRef, aGlyphRef, true);
}

/* attribute DOMString format; */
NS_IMETHODIMP nsSVGAltGlyphElement::GetFormat(nsAString & aFormat)
{
  GetAttr(kNameSpaceID_None, nsGkAtoms::format, aFormat);

  return NS_OK;
}

NS_IMETHODIMP nsSVGAltGlyphElement::SetFormat(const nsAString & aFormat)
{
  return SetAttr(kNameSpaceID_None, nsGkAtoms::format, aFormat, true);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGAltGlyphElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFillStrokeMap,
    sFontSpecificationMap,
    sGraphicsMap,
    sTextContentElementsMap
  };
  
  return FindAttributeDependence(name, map) ||
    nsSVGAltGlyphElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement overrides

bool
nsSVGAltGlyphElement::IsEventName(nsIAtom* aName)
{
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_SVGGraphic);
}

nsSVGElement::StringAttributesInfo
nsSVGAltGlyphElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}
