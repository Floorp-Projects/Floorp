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
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsSVGLength.h"
#include "nsSVGAnimatedLength.h"
#include "nsSVGEnum.h"
#include "nsSVGAnimatedEnumeration.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsIDOMSVGMaskElement.h"
#include "nsCOMPtr.h"
#include "nsISVGSVGElement.h"
#include "nsSVGStylableElement.h"
#include "nsSVGAtoms.h"

//--------------------- Masks ------------------------

typedef nsSVGStylableElement nsSVGMaskElementBase;

class nsSVGMaskElement : public nsSVGMaskElementBase,
                         public nsIDOMSVGMaskElement
{
protected:
  friend nsresult NS_NewSVGMaskElement(nsIContent **aResult,
                                         nsINodeInfo *aNodeInfo);
  nsSVGMaskElement(nsINodeInfo* aNodeInfo);
  virtual ~nsSVGMaskElement();
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // Mask Element
  NS_DECL_NSIDOMSVGMASKELEMENT

  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGElement::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGElement::)

protected:
  
  // nsIDOMSVGMaskElement values
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mMaskUnits;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mMaskContentUnits;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mX;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mY;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mWidth;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mHeight;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Mask)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGMaskElement,nsSVGMaskElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGMaskElement,nsSVGMaskElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGMaskElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGMaskElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGMaskElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGMaskElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGMaskElement::nsSVGMaskElement(nsINodeInfo* aNodeInfo)
  : nsSVGMaskElementBase(aNodeInfo)
{

}

nsSVGMaskElement::~nsSVGMaskElement()
{
}


nsresult
nsSVGMaskElement::Init()
{
  nsresult rv = nsSVGMaskElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Define enumeration mappings
  static struct nsSVGEnumMapping pUnitMap[] = {
        {&nsSVGAtoms::objectBoundingBox, nsIDOMSVGMaskElement::SVG_MUNITS_OBJECTBOUNDINGBOX},
        {&nsSVGAtoms::userSpaceOnUse, nsIDOMSVGMaskElement::SVG_MUNITS_USERSPACEONUSE},
        {nsnull, 0}
  };

  // Create mapped attributes

  // DOM property: maskUnits ,  #IMPLIED attrib: maskUnits
  {
    nsCOMPtr<nsISVGEnum> units;
    rv = NS_NewSVGEnum(getter_AddRefs(units),
                       nsIDOMSVGMaskElement::SVG_MUNITS_OBJECTBOUNDINGBOX, pUnitMap);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mMaskUnits), units);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::maskUnits, mMaskUnits);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: maskContentUnits ,  #IMPLIED attrib: maskContentUnits
  {
    nsCOMPtr<nsISVGEnum> units;
    rv = NS_NewSVGEnum(getter_AddRefs(units),
                       nsIDOMSVGMaskElement::SVG_MUNITS_USERSPACEONUSE, pUnitMap);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mMaskContentUnits), units);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::maskContentUnits, mMaskContentUnits);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: x ,  #IMPLIED attrib: x
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         -10.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mX), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::x, mX);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: y ,  #IMPLIED attrib: y
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         -10.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mY), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::y, mY);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: width ,  #IMPLIED attrib: width
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         120.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mWidth), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::width, mWidth);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: height ,  #IMPLIED attrib: height
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         120.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mHeight), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::height, mHeight);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMNode method

NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGMaskElement)

//----------------------------------------------------------------------
// nsIDOMSVGMaskElement methods

/* readonly attribute nsIDOMSVGAnimatedEnumeration maskUnits; */
NS_IMETHODIMP nsSVGMaskElement::GetMaskUnits(nsIDOMSVGAnimatedEnumeration * *aMaskUnits)
{
  *aMaskUnits = mMaskUnits;
  NS_IF_ADDREF(*aMaskUnits);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration maskContentUnits; */
NS_IMETHODIMP nsSVGMaskElement::GetMaskContentUnits(nsIDOMSVGAnimatedEnumeration * *aMaskUnits)
{
  *aMaskUnits = mMaskContentUnits;
  NS_IF_ADDREF(*aMaskUnits);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP nsSVGMaskElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  *aX = mX;
  NS_IF_ADDREF(*aX);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP nsSVGMaskElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  *aY = mY;
  NS_IF_ADDREF(*aY);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP nsSVGMaskElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  *aWidth = mWidth;
  NS_IF_ADDREF(*aWidth);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP nsSVGMaskElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  *aHeight = mHeight;
  NS_IF_ADDREF(*aHeight);
  return NS_OK;
}
