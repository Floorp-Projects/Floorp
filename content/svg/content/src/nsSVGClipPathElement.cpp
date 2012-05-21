/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsSVGClipPathElement.h"
#include "nsGkAtoms.h"

using namespace mozilla;

nsSVGElement::EnumInfo nsSVGClipPathElement::sEnumInfo[1] =
{
  { &nsGkAtoms::clipPathUnits,
    sSVGUnitTypesMap,
    nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE
  }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(ClipPath)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGClipPathElement,nsSVGClipPathElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGClipPathElement,nsSVGClipPathElementBase)

DOMCI_NODE_DATA(SVGClipPathElement, nsSVGClipPathElement)

NS_INTERFACE_TABLE_HEAD(nsSVGClipPathElement)
  NS_NODE_INTERFACE_TABLE6(nsSVGClipPathElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGTests,
                           nsIDOMSVGClipPathElement,
                           nsIDOMSVGUnitTypes)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGClipPathElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGClipPathElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGClipPathElement::nsSVGClipPathElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGClipPathElementBase(aNodeInfo)
{
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration clipPathUnits; */
NS_IMETHODIMP nsSVGClipPathElement::GetClipPathUnits(nsIDOMSVGAnimatedEnumeration * *aClipPathUnits)
{
  return mEnumAttributes[CLIPPATHUNITS].ToDOMAnimatedEnum(aClipPathUnits, this);
}

nsSVGElement::EnumAttributesInfo
nsSVGClipPathElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGClipPathElement)

