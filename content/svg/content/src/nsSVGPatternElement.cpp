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
 * The Initial Developer of the Original Code is
 * Scooter Morris.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scooter Morris <scootermorris@comcast.net>
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
#include "nsSVGTransformList.h"
#include "nsSVGAnimatedTransformList.h"
#include "nsSVGEnum.h"
#include "nsSVGAnimatedEnumeration.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsSVGAnimatedString.h"
#include "nsIDOMSVGURIReference.h"
#include "nsIDOMSVGPatternElement.h"
#include "nsCOMPtr.h"
#include "nsISVGSVGElement.h"
#include "nsSVGStylableElement.h"
#include "nsSVGAtoms.h"
#include "nsSVGAnimatedRect.h"
#include "nsSVGRect.h"
#include "nsIDOMSVGFitToViewBox.h"
#include "nsSVGMatrix.h"
#include "nsSVGAnimatedPreserveAspectRatio.h"
#include "nsSVGPreserveAspectRatio.h"

//--------------------- Patterns ------------------------

typedef nsSVGStylableElement nsSVGPatternElementBase;

class nsSVGPatternElement : public nsSVGPatternElementBase,
                            public nsIDOMSVGURIReference,
                            public nsIDOMSVGFitToViewBox,
                            public nsIDOMSVGPatternElement
{
protected:
  friend nsresult NS_NewSVGPatternElement(nsIContent **aResult,
                                         nsINodeInfo *aNodeInfo);
  nsSVGPatternElement(nsINodeInfo* aNodeInfo);
  virtual ~nsSVGPatternElement();
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // Pattern Element
  NS_DECL_NSIDOMSVGPATTERNELEMENT

  // URI Reference
  NS_DECL_NSIDOMSVGURIREFERENCE

  // FitToViewbox
  NS_DECL_NSIDOMSVGFITTOVIEWBOX

  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGElement::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGElement::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGElement::)

  // nsISVGContent specializations:
  virtual void ParentChainChanged();

protected:
  
  // nsIDOMSVGPatternElement values
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mPatternUnits;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mPatternContentUnits;
  nsCOMPtr<nsIDOMSVGAnimatedTransformList> mPatternTransform;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mX;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mY;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mWidth;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mHeight;

  // nsIDOMSVGURIReference properties
  nsCOMPtr<nsIDOMSVGAnimatedString> mHref;

  // nsIDOMSVGFitToViewbox properties
  nsCOMPtr<nsIDOMSVGAnimatedRect> mViewBox;
  nsCOMPtr<nsIDOMSVGAnimatedPreserveAspectRatio> mPreserveAspectRatio;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Pattern)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGPatternElement,nsSVGPatternElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGPatternElement,nsSVGPatternElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGPatternElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGFitToViewBox)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGURIReference)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPatternElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGPatternElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGPatternElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGPatternElement::nsSVGPatternElement(nsINodeInfo* aNodeInfo)
  : nsSVGPatternElementBase(aNodeInfo)
{

}

nsSVGPatternElement::~nsSVGPatternElement()
{
}


nsresult
nsSVGPatternElement::Init()
{
  nsresult rv = nsSVGPatternElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Define enumeration mappings
  static struct nsSVGEnumMapping pUnitMap[] = {
        {&nsSVGAtoms::objectBoundingBox, nsIDOMSVGPatternElement::SVG_PUNITS_OBJECTBOUNDINGBOX},
        {&nsSVGAtoms::userSpaceOnUse, nsIDOMSVGPatternElement::SVG_PUNITS_USERSPACEONUSE},
        {nsnull, 0}
  };

  // Create mapped attributes

  // DOM property: patternUnits ,  #IMPLIED attrib: patternUnits
  {
    nsCOMPtr<nsISVGEnum> units;
    rv = NS_NewSVGEnum(getter_AddRefs(units),
                       nsIDOMSVGPatternElement::SVG_PUNITS_OBJECTBOUNDINGBOX, pUnitMap);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mPatternUnits), units);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::patternUnits, mPatternUnits);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: patternContentUnits ,  #IMPLIED attrib: patternContentUnits
  {
    nsCOMPtr<nsISVGEnum> units;
    rv = NS_NewSVGEnum(getter_AddRefs(units),
                       nsIDOMSVGPatternElement::SVG_PUNITS_USERSPACEONUSE, pUnitMap);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mPatternContentUnits), units);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::patternContentUnits, mPatternContentUnits);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: patternTransform ,  #IMPLIED attrib: patternTransform
  {
    nsCOMPtr<nsIDOMSVGTransformList> transformList;
    rv = nsSVGTransformList::Create(getter_AddRefs(transformList));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedTransformList(getter_AddRefs(mPatternTransform),
                                        transformList);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::patternTransform, mPatternTransform);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: x ,  #IMPLIED attrib: x
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
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
                         0.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
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
                         100.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
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
                         100.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mHeight), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::height, mHeight);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // nsIDOMSVGURIReference properties

  // DOM property: href , #IMPLIED attrib: xlink:href
  {
    rv = NS_NewSVGAnimatedString(getter_AddRefs(mHref));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::href, mHref, kNameSpaceID_XLink);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // nsIDOMSVGFitToViewBox properties

  // DOM property: viewBox
  {
    nsCOMPtr<nsIDOMSVGRect> viewbox;
    rv = NS_NewSVGRect(getter_AddRefs(viewbox));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedRect(getter_AddRefs(mViewBox), viewbox);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::viewBox, mViewBox);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: preserveAspectRatio
  {
    nsCOMPtr<nsIDOMSVGPreserveAspectRatio> preserveAspectRatio;
    rv = NS_NewSVGPreserveAspectRatio(getter_AddRefs(preserveAspectRatio));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedPreserveAspectRatio(
                                          getter_AddRefs(mPreserveAspectRatio),
                                          preserveAspectRatio);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::preserveAspectRatio,
                           mPreserveAspectRatio);
    NS_ENSURE_SUCCESS(rv,rv);
  }


  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMNode method

NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGPatternElement)

//----------------------------------------------------------------------
// nsIDOMSVGFitToViewBox methods

/* readonly attribute nsIDOMSVGAnimatedRect viewBox; */
NS_IMETHODIMP nsSVGPatternElement::GetViewBox(nsIDOMSVGAnimatedRect * *aViewBox)
{
  *aViewBox = mViewBox;
  NS_ADDREF(*aViewBox);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedPreserveAspectRatio preserveAspectRatio; */
NS_IMETHODIMP
nsSVGPatternElement::GetPreserveAspectRatio(nsIDOMSVGAnimatedPreserveAspectRatio * *aPreserveAspectRatio)
{
  *aPreserveAspectRatio = mPreserveAspectRatio;
  NS_ADDREF(*aPreserveAspectRatio);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGPatternElement methods

/* readonly attribute nsIDOMSVGAnimatedEnumeration patternUnits; */
NS_IMETHODIMP nsSVGPatternElement::GetPatternUnits(nsIDOMSVGAnimatedEnumeration * *aPatternUnits)
{
  *aPatternUnits = mPatternUnits;
  NS_IF_ADDREF(*aPatternUnits);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration patternContentUnits; */
NS_IMETHODIMP nsSVGPatternElement::GetPatternContentUnits(nsIDOMSVGAnimatedEnumeration * *aPatternUnits)
{
  *aPatternUnits = mPatternContentUnits;
  NS_IF_ADDREF(*aPatternUnits);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedTransformList patternTransform; */
NS_IMETHODIMP nsSVGPatternElement::GetPatternTransform(nsIDOMSVGAnimatedTransformList * *aPatternTransform)
{
  *aPatternTransform = mPatternTransform;
  NS_IF_ADDREF(*aPatternTransform);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength x; */
NS_IMETHODIMP nsSVGPatternElement::GetX(nsIDOMSVGAnimatedLength * *aX)
{
  *aX = mX;
  NS_IF_ADDREF(*aX);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength y; */
NS_IMETHODIMP nsSVGPatternElement::GetY(nsIDOMSVGAnimatedLength * *aY)
{
  *aY = mY;
  NS_IF_ADDREF(*aY);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength width; */
NS_IMETHODIMP nsSVGPatternElement::GetWidth(nsIDOMSVGAnimatedLength * *aWidth)
{
  *aWidth = mWidth;
  NS_IF_ADDREF(*aWidth);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength height; */
NS_IMETHODIMP nsSVGPatternElement::GetHeight(nsIDOMSVGAnimatedLength * *aHeight)
{
  *aHeight = mHeight;
  NS_IF_ADDREF(*aHeight);
  return NS_OK;
}


//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods:

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP
nsSVGPatternElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  *aHref = mHref;
  NS_IF_ADDREF(*aHref);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGContent methods

void nsSVGPatternElement::ParentChainChanged()
{
  // Pattern length properties are relative to the target element
  // (the one calling the pattern), so we don't set their
  // contexts here.

  // Do we need to recurse to child elements??
}  
