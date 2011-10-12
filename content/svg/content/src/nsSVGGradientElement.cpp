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
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#include "DOMSVGAnimatedTransformList.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsIDOMSVGURIReference.h"
#include "nsIDOMSVGGradientElement.h"
#include "nsIDOMMutationEvent.h"
#include "nsCOMPtr.h"
#include "nsSVGStylableElement.h"
#include "nsGkAtoms.h"
#include "nsSVGGradientElement.h"
#include "nsIFrame.h"

using namespace mozilla;

//--------------------- Gradients------------------------

nsSVGEnumMapping nsSVGGradientElement::sSpreadMethodMap[] = {
  {&nsGkAtoms::pad, nsIDOMSVGGradientElement::SVG_SPREADMETHOD_PAD},
  {&nsGkAtoms::reflect, nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REFLECT},
  {&nsGkAtoms::repeat, nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REPEAT},
  {nsnull, 0}
};

nsSVGElement::EnumInfo nsSVGGradientElement::sEnumInfo[2] =
{
  { &nsGkAtoms::gradientUnits,
    sSVGUnitTypesMap,
    nsIDOMSVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX
  },
  { &nsGkAtoms::spreadMethod,
    sSpreadMethodMap,
    nsIDOMSVGGradientElement::SVG_SPREADMETHOD_PAD
  }
};

nsSVGElement::StringInfo nsSVGGradientElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, PR_TRUE }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGGradientElement,nsSVGGradientElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGGradientElement,nsSVGGradientElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGGradientElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGURIReference)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGUnitTypes)
NS_INTERFACE_MAP_END_INHERITING(nsSVGGradientElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGGradientElement::nsSVGGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGGradientElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::EnumAttributesInfo
nsSVGGradientElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            NS_ARRAY_LENGTH(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGGradientElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              NS_ARRAY_LENGTH(sStringInfo));
}

//----------------------------------------------------------------------
// nsIDOMSVGGradientElement methods

/* readonly attribute nsIDOMSVGAnimatedEnumeration gradientUnits; */
NS_IMETHODIMP nsSVGGradientElement::GetGradientUnits(nsIDOMSVGAnimatedEnumeration * *aGradientUnits)
{
  return mEnumAttributes[GRADIENTUNITS].ToDOMAnimatedEnum(aGradientUnits, this);
}

/* readonly attribute nsIDOMSVGAnimatedTransformList gradientTransform; */
NS_IMETHODIMP nsSVGGradientElement::GetGradientTransform(nsIDOMSVGAnimatedTransformList * *aGradientTransform)
{
  *aGradientTransform =
    DOMSVGAnimatedTransformList::GetDOMWrapper(GetAnimatedTransformList(), this)
    .get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration spreadMethod; */
NS_IMETHODIMP nsSVGGradientElement::GetSpreadMethod(nsIDOMSVGAnimatedEnumeration * *aSpreadMethod)
{
  return mEnumAttributes[SPREADMETHOD].ToDOMAnimatedEnum(aSpreadMethod, this);
}

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods:

/* readonly attribute nsIDOMSVGAnimatedString href; */
NS_IMETHODIMP
nsSVGGradientElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  return mStringAttributes[HREF].ToDOMAnimatedString(aHref, this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGGradientElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sGradientStopMap
  };
  
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGGradientElementBase::IsAttributeMapped(name);
}

//---------------------Linear Gradients------------------------

nsSVGElement::LengthInfo nsSVGLinearGradientElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x1, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::X },
  { &nsGkAtoms::y1, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::Y },
  { &nsGkAtoms::x2, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::X },
  { &nsGkAtoms::y2, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::Y },
};

NS_IMPL_NS_NEW_SVG_ELEMENT(LinearGradient)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGLinearGradientElement,nsSVGLinearGradientElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGLinearGradientElement,nsSVGLinearGradientElementBase)

DOMCI_NODE_DATA(SVGLinearGradientElement, nsSVGLinearGradientElement)

NS_INTERFACE_TABLE_HEAD(nsSVGLinearGradientElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGLinearGradientElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGGradientElement,
                           nsIDOMSVGLinearGradientElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGLinearGradientElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGLinearGradientElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGLinearGradientElement::nsSVGLinearGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGLinearGradientElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGLinearGradientElement)

//----------------------------------------------------------------------
// nsIDOMSVGLinearGradientElement methods

/* readonly attribute nsIDOMSVGAnimatedLength x1; */
NS_IMETHODIMP nsSVGLinearGradientElement::GetX1(nsIDOMSVGAnimatedLength * *aX1)
{
  return mLengthAttributes[X1].ToDOMAnimatedLength(aX1, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y1; */
NS_IMETHODIMP nsSVGLinearGradientElement::GetY1(nsIDOMSVGAnimatedLength * *aY1)
{
  return mLengthAttributes[Y1].ToDOMAnimatedLength(aY1, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength x2; */
NS_IMETHODIMP nsSVGLinearGradientElement::GetX2(nsIDOMSVGAnimatedLength * *aX2)
{
  return mLengthAttributes[X2].ToDOMAnimatedLength(aX2, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y2; */
NS_IMETHODIMP nsSVGLinearGradientElement::GetY2(nsIDOMSVGAnimatedLength * *aY2)
{
  return mLengthAttributes[Y2].ToDOMAnimatedLength(aY2, this);
}


//----------------------------------------------------------------------
// nsSVGElement methods

SVGAnimatedTransformList*
nsSVGGradientElement::GetAnimatedTransformList()
{
  if (!mGradientTransform) {
    mGradientTransform = new SVGAnimatedTransformList();
  }
  return mGradientTransform;
}

nsSVGElement::LengthAttributesInfo
nsSVGLinearGradientElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              NS_ARRAY_LENGTH(sLengthInfo));
}

//-------------------------- Radial Gradients ----------------------------

nsSVGElement::LengthInfo nsSVGRadialGradientElement::sLengthInfo[5] =
{
  { &nsGkAtoms::cx, 50, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::X },
  { &nsGkAtoms::cy, 50, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::Y },
  { &nsGkAtoms::r, 50, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::XY },
  { &nsGkAtoms::fx, 50, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::X },
  { &nsGkAtoms::fy, 50, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, nsSVGUtils::Y },
};

NS_IMPL_NS_NEW_SVG_ELEMENT(RadialGradient)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGRadialGradientElement,nsSVGRadialGradientElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGRadialGradientElement,nsSVGRadialGradientElementBase)

DOMCI_NODE_DATA(SVGRadialGradientElement, nsSVGRadialGradientElement)

NS_INTERFACE_TABLE_HEAD(nsSVGRadialGradientElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGRadialGradientElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGGradientElement,
                           nsIDOMSVGRadialGradientElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGRadialGradientElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGRadialGradientElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGRadialGradientElement::nsSVGRadialGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGRadialGradientElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGRadialGradientElement)

//----------------------------------------------------------------------
// nsIDOMSVGRadialGradientElement methods

/* readonly attribute nsIDOMSVGAnimatedLength cx; */
NS_IMETHODIMP nsSVGRadialGradientElement::GetCx(nsIDOMSVGAnimatedLength * *aCx)
{
  return mLengthAttributes[CX].ToDOMAnimatedLength(aCx, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength cy; */
NS_IMETHODIMP nsSVGRadialGradientElement::GetCy(nsIDOMSVGAnimatedLength * *aCy)
{
  return mLengthAttributes[CY].ToDOMAnimatedLength(aCy, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength R; */
NS_IMETHODIMP nsSVGRadialGradientElement::GetR(nsIDOMSVGAnimatedLength * *aR)
{
  return mLengthAttributes[R].ToDOMAnimatedLength(aR, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength fx; */
NS_IMETHODIMP nsSVGRadialGradientElement::GetFx(nsIDOMSVGAnimatedLength * *aFx)
{
  return mLengthAttributes[FX].ToDOMAnimatedLength(aFx, this);
}

/* readonly attribute nsIDOMSVGAnimatedLength fy; */
NS_IMETHODIMP nsSVGRadialGradientElement::GetFy(nsIDOMSVGAnimatedLength * *aFy)
{
  return mLengthAttributes[FY].ToDOMAnimatedLength(aFy, this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthAttributesInfo
nsSVGRadialGradientElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              NS_ARRAY_LENGTH(sLengthInfo));
}
