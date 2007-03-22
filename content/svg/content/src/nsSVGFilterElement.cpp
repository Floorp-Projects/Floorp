/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsGkAtoms.h"
#include "nsSVGLength.h"
#include "nsCOMPtr.h"
#include "nsISVGSVGElement.h"
#include "nsSVGAnimatedEnumeration.h"
#include "nsSVGAnimatedInteger.h"
#include "nsSVGAnimatedString.h"
#include "nsSVGEnum.h"
#include "nsSVGFilterElement.h"

nsSVGElement::LengthInfo nsSVGFilterElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x, -10, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::X },
  { &nsGkAtoms::y, -10, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::Y },
  { &nsGkAtoms::width, 120, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::X },
  { &nsGkAtoms::height, 120, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::Y },
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Filter)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFilterElement,nsSVGFilterElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFilterElement,nsSVGFilterElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFilterElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFilterElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGURIReference)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGFilterElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFilterElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGFilterElement::nsSVGFilterElement(nsINodeInfo *aNodeInfo)
  : nsSVGFilterElementBase(aNodeInfo)
{
}

nsresult
nsSVGFilterElement::Init()
{
  nsresult rv = nsSVGFilterElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Define enumeration mappings
  static struct nsSVGEnumMapping gUnitMap[] = {
        {&nsGkAtoms::objectBoundingBox, nsIDOMSVGFilterElement::SVG_FUNITS_OBJECTBOUNDINGBOX},
        {&nsGkAtoms::userSpaceOnUse, nsIDOMSVGFilterElement::SVG_FUNITS_USERSPACEONUSE},
        {nsnull, 0}
  };

  // Create mapped properties:

  // DOM property: filterUnits ,  #IMPLIED attrib: filterUnits
  {
    nsCOMPtr<nsISVGEnum> units;
    rv = NS_NewSVGEnum(getter_AddRefs(units),
                       nsIDOMSVGFilterElement::SVG_FUNITS_OBJECTBOUNDINGBOX, gUnitMap);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mFilterUnits), units);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::filterUnits, mFilterUnits);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: primitiveUnits ,  #IMPLIED attrib: primitiveUnits
  {
    nsCOMPtr<nsISVGEnum> units;
    rv = NS_NewSVGEnum(getter_AddRefs(units),
                       nsIDOMSVGFilterElement::SVG_FUNITS_USERSPACEONUSE, gUnitMap);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mPrimitiveUnits), units);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::primitiveUnits, mPrimitiveUnits);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: filterResX , #IMPLIED attrib: filterRes
  {
    rv = NS_NewSVGAnimatedInteger(getter_AddRefs(mFilterResX), 0);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: filterResY , #IMPLIED attrib: filterRes
  {
    rv = NS_NewSVGAnimatedInteger(getter_AddRefs(mFilterResY), 0);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // nsIDOMSVGURIReference properties

  // DOM property: href , #REQUIRED attrib: xlink:href
  // XXX: enforce requiredness
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mHref));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsGkAtoms::href, mHref, kNameSpaceID_XLink);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return rv;
}

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFilterElement)


//----------------------------------------------------------------------
// nsIDOMSVGFilterElement methods

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP nsSVGFilterElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  return mLengthAttributes[X].ToDOMAnimatedLength(aX, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP nsSVGFilterElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  return mLengthAttributes[Y].ToDOMAnimatedLength(aY, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP nsSVGFilterElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  return mLengthAttributes[WIDTH].ToDOMAnimatedLength(aWidth, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP nsSVGFilterElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  return mLengthAttributes[HEIGHT].ToDOMAnimatedLength(aHeight, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration filterUnits; */
NS_IMETHODIMP nsSVGFilterElement::GetFilterUnits(nsIDOMSVGAnimatedEnumeration * *aUnits)
{
  *aUnits = mFilterUnits;
  NS_IF_ADDREF(*aUnits);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration primitiveUnits; */
NS_IMETHODIMP nsSVGFilterElement::GetPrimitiveUnits(nsIDOMSVGAnimatedEnumeration * *aUnits)
{
  *aUnits = mPrimitiveUnits;
  NS_IF_ADDREF(*aUnits);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration filterResY; */
NS_IMETHODIMP nsSVGFilterElement::GetFilterResX(nsIDOMSVGAnimatedInteger * *aFilterResX)
{
  *aFilterResX = mFilterResX;
  NS_IF_ADDREF(*aFilterResX);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration filterResY; */
NS_IMETHODIMP nsSVGFilterElement::GetFilterResY(nsIDOMSVGAnimatedInteger * *aFilterResY)
{
  *aFilterResY = mFilterResY;
  NS_IF_ADDREF(*aFilterResY);
  return NS_OK;
}

/* void setFilterRes (in unsigned long filterResX, in unsigned long filterResY);
 */
NS_IMETHODIMP
nsSVGFilterElement::SetFilterRes(PRUint32 filterResX, PRUint32 filterResY)
{
  mFilterResX->SetBaseVal(filterResX);
  mFilterResY->SetBaseVal(filterResY);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP 
nsSVGFilterElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  *aHref = mHref;
  NS_IF_ADDREF(*aHref);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIContent methods

nsresult
nsSVGFilterElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                            nsIAtom* aPrefix, const nsAString& aValue,
                            PRBool aNotify)
{
  nsresult rv = nsSVGFilterElementBase::SetAttr(aNameSpaceID, aName, aPrefix,
                                                aValue, aNotify);

  if (aName == nsGkAtoms::filterRes && aNameSpaceID == kNameSpaceID_None) {
    PRUint32 resX, resY;
    char *str;
    str = ToNewCString(aValue);
    int num = sscanf(str, "%d %d\n", &resX, &resY);
    switch (num) {
    case 2:
      mFilterResX->SetBaseVal(resX);
      mFilterResY->SetBaseVal(resY);
      break;
    case 1:
      mFilterResX->SetBaseVal(resX);
      mFilterResY->SetBaseVal(resX);
      break;
    default:
      break;
    }
    nsMemory::Free(str);
  }

  return rv;
}

NS_IMETHODIMP_(PRBool)
nsSVGFilterElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFEFloodMap,
    sFiltersMap,
    sFontSpecificationMap,
    sGradientStopMap,
    sMarkersMap,
    sTextContentElementsMap,
    sViewportsMap
  };
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGGraphicElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthAttributesInfo
nsSVGFilterElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              NS_ARRAY_LENGTH(sLengthInfo));
}
