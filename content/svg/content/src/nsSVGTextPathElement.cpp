/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsSVGStylableElement.h"
#include "nsGkAtoms.h"
#include "nsIDOMSVGTextPathElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsIFrame.h"
#include "nsSVGTextPathElement.h"
#include "nsDOMError.h"
#include "nsContentUtils.h"

using namespace mozilla;

nsSVGElement::LengthInfo nsSVGTextPathElement::sLengthInfo[1] =
{
  { &nsGkAtoms::startOffset, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER, nsSVGUtils::X },
};

nsSVGEnumMapping nsSVGTextPathElement::sMethodMap[] = {
  {&nsGkAtoms::align, nsIDOMSVGTextPathElement::TEXTPATH_METHODTYPE_ALIGN},
  {&nsGkAtoms::stretch, nsIDOMSVGTextPathElement::TEXTPATH_METHODTYPE_STRETCH},
  {nsnull, 0}
};

nsSVGEnumMapping nsSVGTextPathElement::sSpacingMap[] = {
  {&nsGkAtoms::_auto, nsIDOMSVGTextPathElement::TEXTPATH_SPACINGTYPE_AUTO},
  {&nsGkAtoms::exact, nsIDOMSVGTextPathElement::TEXTPATH_SPACINGTYPE_EXACT},
  {nsnull, 0}
};

nsSVGElement::EnumInfo nsSVGTextPathElement::sEnumInfo[2] =
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

nsSVGElement::StringInfo nsSVGTextPathElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, true }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(TextPath)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGTextPathElement,nsSVGTextPathElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGTextPathElement,nsSVGTextPathElementBase)

DOMCI_NODE_DATA(SVGTextPathElement, nsSVGTextPathElement)

NS_INTERFACE_TABLE_HEAD(nsSVGTextPathElement)
  NS_NODE_INTERFACE_TABLE7(nsSVGTextPathElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTextPathElement,
                           nsIDOMSVGTextContentElement, nsIDOMSVGTests,
                           nsIDOMSVGURIReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGTextPathElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGTextPathElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGTextPathElement::nsSVGTextPathElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGTextPathElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGTextPathElement)

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP nsSVGTextPathElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  return mStringAttributes[HREF].ToDOMAnimatedString(aHref, this);
}

//----------------------------------------------------------------------
// nsIDOMSVGTextPathElement methods

NS_IMETHODIMP nsSVGTextPathElement::GetStartOffset(nsIDOMSVGAnimatedLength * *aStartOffset)
{
  return mLengthAttributes[STARTOFFSET].ToDOMAnimatedLength(aStartOffset, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration method; */
NS_IMETHODIMP nsSVGTextPathElement::GetMethod(nsIDOMSVGAnimatedEnumeration * *aMethod)
{
  return mEnumAttributes[METHOD].ToDOMAnimatedEnum(aMethod, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration spacing; */
NS_IMETHODIMP nsSVGTextPathElement::GetSpacing(nsIDOMSVGAnimatedEnumeration * *aSpacing)
{
  return mEnumAttributes[SPACING].ToDOMAnimatedEnum(aSpacing, this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGTextPathElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFillStrokeMap,
    sFontSpecificationMap,
    sGraphicsMap,
    sTextContentElementsMap
  };
  
  return FindAttributeDependence(name, map) ||
    nsSVGTextPathElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement overrides

bool
nsSVGTextPathElement::IsEventName(nsIAtom* aName)
{
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_SVGGraphic);
}

nsSVGElement::LengthAttributesInfo
nsSVGTextPathElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

nsSVGElement::EnumAttributesInfo
nsSVGTextPathElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGTextPathElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}
