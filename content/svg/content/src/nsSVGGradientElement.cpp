/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 * Scooter Morris
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Scooter Morris <scootermorris@comcast.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsSVGLength.h"
#include "nsSVGAnimatedLength.h"
#include "nsSVGTransformList.h"
#include "nsSVGAnimatedTransformList.h"
#include "nsSVGEnum.h"
#include "nsSVGAnimatedEnumeration.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsIDOMSVGURIReference.h"
#include "nsIDOMSVGGradientElement.h"
#include "nsSVGAnimatedString.h"
#include "nsCOMPtr.h"
#include "nsISVGSVGElement.h"
#include "nsSVGStylableElement.h"
#include "nsSVGAtoms.h"

//--------------------- Gradients------------------------

typedef nsSVGStylableElement nsSVGGradientElementBase;

class nsSVGGradientElement : public nsSVGGradientElementBase,
                             public nsIDOMSVGURIReference
{
protected:
  nsSVGGradientElement(nsINodeInfo* aNodeInfo);
  virtual ~nsSVGGradientElement();
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // Gradient Element
  NS_DECL_NSIDOMSVGGRADIENTELEMENT

  // URI Reference
  NS_DECL_NSIDOMSVGURIREFERENCE

  // nsISVGContent specializations:
  virtual void ParentChainChanged();

protected:
  
  // nsIDOMSVGGradientElement values
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mGradientUnits;
  nsCOMPtr<nsIDOMSVGAnimatedTransformList> mGradientTransform;
  nsCOMPtr<nsIDOMSVGAnimatedEnumeration> mSpreadMethod;

  // nsIDOMSVGURIReference properties
  nsCOMPtr<nsIDOMSVGAnimatedString> mHref;
};


//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGGradientElement,nsSVGGradientElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGGradientElement,nsSVGGradientElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGGradientElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGURIReference)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGradientElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGGradientElement::nsSVGGradientElement(nsINodeInfo* aNodeInfo)
  : nsSVGGradientElementBase(aNodeInfo)
{

}

nsSVGGradientElement::~nsSVGGradientElement()
{
}


nsresult
nsSVGGradientElement::Init()
{
  nsresult rv = nsSVGGradientElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Define enumeration mappings
  static struct nsSVGEnumMapping gUnitMap[] = {
        {&nsSVGAtoms::objectBoundingBox, nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX},
        {&nsSVGAtoms::userSpaceOnUse, nsIDOMSVGGradientElement::SVG_GRUNITS_USERSPACEONUSE},
        {nsnull, 0}
  };

  static struct nsSVGEnumMapping gSpreadMap[] = {
        {&nsSVGAtoms::pad, nsIDOMSVGGradientElement::SVG_SPREADMETHOD_PAD},
        {&nsSVGAtoms::reflect, nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REFLECT},
        {&nsSVGAtoms::repeat, nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REPEAT},
        {nsnull, 0}
  };

  // Create mapped attributes

  // DOM property: gradientUnits ,  #IMPLIED attrib: gradientUnits
  {
    nsCOMPtr<nsISVGEnum> units;
    rv = NS_NewSVGEnum(getter_AddRefs(units),
                       nsIDOMSVGGradientElement::SVG_GRUNITS_OBJECTBOUNDINGBOX, gUnitMap);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mGradientUnits), units);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::gradientUnits, mGradientUnits);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: gradientTransform ,  #IMPLIED attrib: gradientTransform
  {
    nsCOMPtr<nsIDOMSVGTransformList> transformList;
    rv = nsSVGTransformList::Create(getter_AddRefs(transformList));
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedTransformList(getter_AddRefs(mGradientTransform),
                                        transformList);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::gradientTransform, mGradientTransform);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: spreadMethod ,  #IMPLIED  attrib: spreadMethod
  {
    nsCOMPtr<nsISVGEnum> spread;
    rv = NS_NewSVGEnum(getter_AddRefs(spread), 
                       nsIDOMSVGGradientElement::SVG_SPREADMETHOD_PAD, gSpreadMap );
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedEnumeration(getter_AddRefs(mSpreadMethod), spread);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::spreadMethod, mSpreadMethod);
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

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGGradientElement methods

/* readonly attribute nsIDOMSVGAnimatedEnumeration gradientUnits; */
NS_IMETHODIMP nsSVGGradientElement::GetGradientUnits(nsIDOMSVGAnimatedEnumeration * *aGradientUnits)
{
  *aGradientUnits = mGradientUnits;
  NS_IF_ADDREF(*aGradientUnits);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedTransformList gradientTransform; */
NS_IMETHODIMP nsSVGGradientElement::GetGradientTransform(nsIDOMSVGAnimatedTransformList * *aGradientTransform)
{
  *aGradientTransform = mGradientTransform;
  NS_IF_ADDREF(*aGradientTransform);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration spreadMethod; */
NS_IMETHODIMP nsSVGGradientElement::GetSpreadMethod(nsIDOMSVGAnimatedEnumeration * *aSpreadMethod)
{
  *aSpreadMethod = mSpreadMethod;
  NS_IF_ADDREF(*aSpreadMethod);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods:

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP
nsSVGGradientElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  *aHref = mHref;
  NS_IF_ADDREF(*aHref);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGContent methods

void nsSVGGradientElement::ParentChainChanged()
{
  // Gradient length properties are relative to the target element
  // (the one calling the gradient), so we don't set their
  // contexts here.
  // Also, gradient child elements (stops) don't have any length
  // properties, so no need to recurse into children here.
}  

//---------------------Linear Gradients------------------------

typedef nsSVGGradientElement nsSVGLinearGradientElementBase;

class nsSVGLinearGradientElement : public nsSVGLinearGradientElementBase,
                                   public nsIDOMSVGLinearGradientElement
{
protected:
  friend nsresult NS_NewSVGLinearGradientElement(nsIContent **aResult,
                                                 nsINodeInfo *aNodeInfo);
  nsSVGLinearGradientElement(nsINodeInfo* aNodeInfo);
  virtual ~nsSVGLinearGradientElement();
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  // Gradient Element
  NS_FORWARD_NSIDOMSVGGRADIENTELEMENT(nsSVGLinearGradientElementBase::)

  // Linear Gradient
  NS_DECL_NSIDOMSVGLINEARGRADIENTELEMENT

  // The Gradient Element base class implements these
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGLinearGradientElementBase::)

  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGLinearGradientElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGLinearGradientElementBase::)

protected:
  
  // nsIDOMSVGLinearGradientElement values
  nsCOMPtr<nsIDOMSVGAnimatedLength> mX1;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mY1;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mX2;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mY2;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(LinearGradient)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGLinearGradientElement,nsSVGLinearGradientElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGLinearGradientElement,nsSVGLinearGradientElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGLinearGradientElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGGradientElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGLinearGradientElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGLinearGradientElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGLinearGradientElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGLinearGradientElement::nsSVGLinearGradientElement(nsINodeInfo* aNodeInfo)
  : nsSVGLinearGradientElementBase(aNodeInfo)
{

}

nsSVGLinearGradientElement::~nsSVGLinearGradientElement()
{
}


nsresult
nsSVGLinearGradientElement::Init()
{
  nsresult rv = nsSVGLinearGradientElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // DOM property: x1 ,  #IMPLIED attrib: x1
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mX1), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::x1, mX1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: y1 ,  #IMPLIED attrib: y1
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mY1), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::y1, mY1);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: x2 ,  #IMPLIED  attrib: x2
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         100.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mX2), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::x2, mX2);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: y2 ,  #IMPLIED  attrib: height
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         0.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mY2), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::y2, mY2);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGLinearGradientElement)

//----------------------------------------------------------------------
// nsIDOMSVGLinearGradientElement methods

/* readonly attribute nsIDOMSVGAnimatedLength x1; */
NS_IMETHODIMP nsSVGLinearGradientElement::GetX1(nsIDOMSVGAnimatedLength * *aX1)
{
  *aX1 = mX1;
  NS_IF_ADDREF(*aX1);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength y1; */
NS_IMETHODIMP nsSVGLinearGradientElement::GetY1(nsIDOMSVGAnimatedLength * *aY1)
{
  *aY1 = mY1;
  NS_IF_ADDREF(*aY1);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength x2; */
NS_IMETHODIMP nsSVGLinearGradientElement::GetX2(nsIDOMSVGAnimatedLength * *aX2)
{
  *aX2 = mX2;
  NS_IF_ADDREF(*aX2);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength y2; */
NS_IMETHODIMP nsSVGLinearGradientElement::GetY2(nsIDOMSVGAnimatedLength * *aY2)
{
  *aY2 = mY2;
  NS_IF_ADDREF(*aY2);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGContent methods

//-------------------------- Radial Gradients ----------------------------

typedef nsSVGGradientElement nsSVGRadialGradientElementBase;

class nsSVGRadialGradientElement : public nsSVGRadialGradientElementBase,
                                   public nsIDOMSVGRadialGradientElement
{
protected:
  friend nsresult NS_NewSVGRadialGradientElement(nsIContent **aResult,
                                                 nsINodeInfo *aNodeInfo);
  nsSVGRadialGradientElement(nsINodeInfo* aNodeInfo);
  virtual ~nsSVGRadialGradientElement();
  nsresult Init();

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED

  // Gradient Element
  NS_FORWARD_NSIDOMSVGGRADIENTELEMENT(nsSVGRadialGradientElementBase::)

  // Radial Gradient
  NS_DECL_NSIDOMSVGRADIALGRADIENTELEMENT

  // xxx I wish we could use virtual inheritance
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsSVGRadialGradientElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGRadialGradientElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGRadialGradientElementBase::)

protected:
  
  // nsIDOMSVGRadialGradientElement values
  nsCOMPtr<nsIDOMSVGAnimatedLength> mCx;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mCy;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mR;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mFx;
  nsCOMPtr<nsIDOMSVGAnimatedLength> mFy;
};

NS_IMPL_NS_NEW_SVG_ELEMENT(RadialGradient)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGRadialGradientElement,nsSVGRadialGradientElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGRadialGradientElement,nsSVGRadialGradientElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGRadialGradientElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGGradientElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGRadialGradientElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGRadialGradientElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGRadialGradientElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGRadialGradientElement::nsSVGRadialGradientElement(nsINodeInfo* aNodeInfo)
  : nsSVGRadialGradientElementBase(aNodeInfo)
{

}

nsSVGRadialGradientElement::~nsSVGRadialGradientElement()
{
}


nsresult
nsSVGRadialGradientElement::Init()
{
  nsresult rv = nsSVGRadialGradientElementBase::Init();
  NS_ENSURE_SUCCESS(rv,rv);

  // Create mapped properties:

  // DOM property: cx ,  #IMPLIED attrib: cx
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         50.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mCx), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::cx, mCx);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: cy ,  #IMPLIED attrib: cy
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         50.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mCy), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::cy, mCy);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: r ,  #IMPLIED  attrib: r
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         50.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mR), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::r, mR);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: fx ,  #IMPLIED  attrib: fx
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         50.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mFx), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::fx, mFx);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  // DOM property: fy ,  #IMPLIED  attrib: fy
  {
    nsCOMPtr<nsISVGLength> length;
    rv = NS_NewSVGLength(getter_AddRefs(length),
                         50.0,nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = NS_NewSVGAnimatedLength(getter_AddRefs(mFy), length);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = AddMappedSVGValue(nsSVGAtoms::fy, mFy);
    NS_ENSURE_SUCCESS(rv,rv);
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_DOM_CLONENODE_WITH_INIT(nsSVGRadialGradientElement)

//----------------------------------------------------------------------
// nsIDOMSVGRadialGradientElement methods

/* readonly attribute nsIDOMSVGAnimatedLength cx; */
NS_IMETHODIMP nsSVGRadialGradientElement::GetCx(nsIDOMSVGAnimatedLength * *aCx)
{
  *aCx = mCx;
  NS_IF_ADDREF(*aCx);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength cy; */
NS_IMETHODIMP nsSVGRadialGradientElement::GetCy(nsIDOMSVGAnimatedLength * *aCy)
{
  *aCy = mCy;
  NS_IF_ADDREF(*aCy);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength R; */
NS_IMETHODIMP nsSVGRadialGradientElement::GetR(nsIDOMSVGAnimatedLength * *aR)
{
  *aR = mR;
  NS_IF_ADDREF(*aR);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength fx; */
NS_IMETHODIMP nsSVGRadialGradientElement::GetFx(nsIDOMSVGAnimatedLength * *aFx)
{
  *aFx = mFx;
  NS_IF_ADDREF(*aFx);
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedLength fy; */
NS_IMETHODIMP nsSVGRadialGradientElement::GetFy(nsIDOMSVGAnimatedLength * *aFy)
{
  *aFy = mFy;
  NS_IF_ADDREF(*aFy);
  return NS_OK;
}


