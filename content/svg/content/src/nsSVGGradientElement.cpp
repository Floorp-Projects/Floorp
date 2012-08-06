/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "DOMSVGAnimatedTransformList.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsIDOMSVGURIReference.h"
#include "nsIDOMSVGGradientElement.h"
#include "nsIDOMMutationEvent.h"
#include "nsCOMPtr.h"
#include "nsSVGStylableElement.h"
#include "nsGkAtoms.h"
#include "nsSVGGradientElement.h"

using namespace mozilla;

//--------------------- Gradients------------------------

nsSVGEnumMapping nsSVGGradientElement::sSpreadMethodMap[] = {
  {&nsGkAtoms::pad, nsIDOMSVGGradientElement::SVG_SPREADMETHOD_PAD},
  {&nsGkAtoms::reflect, nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REFLECT},
  {&nsGkAtoms::repeat, nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REPEAT},
  {nullptr, 0}
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
  { &nsGkAtoms::href, kNameSpaceID_XLink, true }
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
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGGradientElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
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
  // We're creating a DOM wrapper, so we must tell GetAnimatedTransformList
  // to allocate the SVGAnimatedTransformList if it hasn't already done so:
  *aGradientTransform = DOMSVGAnimatedTransformList::GetDOMWrapper(
                          GetAnimatedTransformList(DO_ALLOCATE), this).get();
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
  
  return FindAttributeDependence(name, map) ||
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
  NS_NODE_INTERFACE_TABLE6(nsSVGLinearGradientElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGTests,
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
nsSVGGradientElement::GetAnimatedTransformList(PRUint32 aFlags)
{
  if (!mGradientTransform && (aFlags & DO_ALLOCATE)) {
    mGradientTransform = new SVGAnimatedTransformList();
  }
  return mGradientTransform;
}

nsSVGElement::LengthAttributesInfo
nsSVGLinearGradientElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
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
  NS_NODE_INTERFACE_TABLE6(nsSVGRadialGradientElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGTests,
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
                              ArrayLength(sLengthInfo));
}
