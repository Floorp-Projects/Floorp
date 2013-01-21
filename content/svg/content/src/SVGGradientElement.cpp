/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGGradientElement.h"

#include "DOMSVGAnimatedTransformList.h"
#include "mozilla/dom/SVGAnimatedLength.h"
#include "mozilla/dom/SVGRadialGradientElementBinding.h"
#include "mozilla/dom/SVGLinearGradientElementBinding.h"
#include "mozilla/Util.h"
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsIDOMMutationEvent.h"
#include "nsIDOMSVGAnimatedEnum.h"
#include "nsIDOMSVGGradientElement.h"
#include "nsIDOMSVGURIReference.h"
#include "nsSVGElement.h"

DOMCI_NODE_DATA(SVGRadialGradientElement, mozilla::dom::SVGRadialGradientElement)
DOMCI_NODE_DATA(SVGLinearGradientElement, mozilla::dom::SVGLinearGradientElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(LinearGradient)
NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(RadialGradient)

namespace mozilla {
namespace dom {

//--------------------- Gradients------------------------

nsSVGEnumMapping SVGGradientElement::sSpreadMethodMap[] = {
  {&nsGkAtoms::pad, nsIDOMSVGGradientElement::SVG_SPREADMETHOD_PAD},
  {&nsGkAtoms::reflect, nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REFLECT},
  {&nsGkAtoms::repeat, nsIDOMSVGGradientElement::SVG_SPREADMETHOD_REPEAT},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGGradientElement::sEnumInfo[2] =
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

nsSVGElement::StringInfo SVGGradientElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, true }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGGradientElement, SVGGradientElementBase)
NS_IMPL_RELEASE_INHERITED(SVGGradientElement, SVGGradientElementBase)

NS_INTERFACE_MAP_BEGIN(SVGGradientElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGURIReference)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGUnitTypes)
NS_INTERFACE_MAP_END_INHERITING(SVGGradientElementBase)

//----------------------------------------------------------------------
// Implementation

SVGGradientElement::SVGGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGGradientElementBase(aNodeInfo)
{
  SetIsDOMBinding();
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::EnumAttributesInfo
SVGGradientElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
SVGGradientElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

//----------------------------------------------------------------------
// nsIDOMSVGGradientElement methods

/* readonly attribute nsIDOMSVGAnimatedEnumeration gradientUnits; */
already_AddRefed<nsIDOMSVGAnimatedEnumeration>
SVGGradientElement::GradientUnits()
{
  return mEnumAttributes[GRADIENTUNITS].ToDOMAnimatedEnum(this);
}

NS_IMETHODIMP SVGGradientElement::GetGradientUnits(nsIDOMSVGAnimatedEnumeration * *aGradientUnits)
{
  *aGradientUnits = GradientUnits().get();
  return NS_OK;
}

/* readonly attribute SVGAnimatedTransformList gradientTransform; */
already_AddRefed<DOMSVGAnimatedTransformList>
SVGGradientElement::GradientTransform()
{
  // We're creating a DOM wrapper, so we must tell GetAnimatedTransformList
  // to allocate the SVGAnimatedTransformList if it hasn't already done so:
  return DOMSVGAnimatedTransformList::GetDOMWrapper(
           GetAnimatedTransformList(DO_ALLOCATE), this);
}

NS_IMETHODIMP SVGGradientElement::GetGradientTransform(nsISupports * *aGradientTransform)
{
  *aGradientTransform = GradientTransform().get();
  return NS_OK;
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration spreadMethod; */
already_AddRefed<nsIDOMSVGAnimatedEnumeration>
SVGGradientElement::SpreadMethod()
{
  return mEnumAttributes[SPREADMETHOD].ToDOMAnimatedEnum(this);
}

NS_IMETHODIMP SVGGradientElement::GetSpreadMethod(nsIDOMSVGAnimatedEnumeration * *aSpreadMethod)
{
  *aSpreadMethod = SpreadMethod().get();
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIDOMSVGURIReference methods:

/* readonly attribute nsIDOMSVGAnimatedString href; */
already_AddRefed<nsIDOMSVGAnimatedString>
SVGGradientElement::Href()
{
  nsCOMPtr<nsIDOMSVGAnimatedString> href;
  mStringAttributes[HREF].ToDOMAnimatedString(getter_AddRefs(href), this);
  return href.forget();
}

NS_IMETHODIMP
SVGGradientElement::GetHref(nsIDOMSVGAnimatedString * *aHref)
{
  *aHref = Href().get();
  return NS_OK;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGGradientElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sGradientStopMap
  };
  
  return FindAttributeDependence(name, map) ||
    SVGGradientElementBase::IsAttributeMapped(name);
}

//---------------------Linear Gradients------------------------

JSObject*
SVGLinearGradientElement::WrapNode(JSContext* aCx, JSObject* aScope,
                                   bool* aTriedToWrap)
{
  return SVGLinearGradientElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

nsSVGElement::LengthInfo SVGLinearGradientElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x1, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::y1, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
  { &nsGkAtoms::x2, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::y2, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGLinearGradientElement,SVGLinearGradientElementBase)
NS_IMPL_RELEASE_INHERITED(SVGLinearGradientElement,SVGLinearGradientElementBase)

NS_INTERFACE_TABLE_HEAD(SVGLinearGradientElement)
  NS_NODE_INTERFACE_TABLE5(SVGLinearGradientElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGGradientElement,
                           nsIDOMSVGLinearGradientElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGLinearGradientElement)
NS_INTERFACE_MAP_END_INHERITING(SVGLinearGradientElementBase)

//----------------------------------------------------------------------
// Implementation

SVGLinearGradientElement::SVGLinearGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGLinearGradientElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGLinearGradientElement)

//----------------------------------------------------------------------
// nsIDOMSVGLinearGradientElement methods

/* readonly attribute nsIDOMSVGAnimatedLength x1; */
NS_IMETHODIMP SVGLinearGradientElement::GetX1(nsIDOMSVGAnimatedLength * *aX1)
{
  *aX1 = X1().get();
  return NS_OK;
}

already_AddRefed<SVGAnimatedLength>
SVGLinearGradientElement::X1()
{
  return mLengthAttributes[ATTR_X1].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y1; */
NS_IMETHODIMP SVGLinearGradientElement::GetY1(nsIDOMSVGAnimatedLength * *aY1)
{
  *aY1 = Y1().get();
  return NS_OK;
}

already_AddRefed<SVGAnimatedLength>
SVGLinearGradientElement::Y1()
{
  return mLengthAttributes[ATTR_Y1].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength x2; */
NS_IMETHODIMP SVGLinearGradientElement::GetX2(nsIDOMSVGAnimatedLength * *aX2)
{
  *aX2 = X2().get();
  return NS_OK;
}

already_AddRefed<SVGAnimatedLength>
SVGLinearGradientElement::X2()
{
  return mLengthAttributes[ATTR_X2].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength y2; */
NS_IMETHODIMP SVGLinearGradientElement::GetY2(nsIDOMSVGAnimatedLength * *aY2)
{
  *aY2 = Y2().get();
  return NS_OK;
}

already_AddRefed<SVGAnimatedLength>
SVGLinearGradientElement::Y2()
{
  return mLengthAttributes[ATTR_Y2].ToDOMAnimatedLength(this);
}


//----------------------------------------------------------------------
// nsSVGElement methods

SVGAnimatedTransformList*
SVGGradientElement::GetAnimatedTransformList(uint32_t aFlags)
{
  if (!mGradientTransform && (aFlags & DO_ALLOCATE)) {
    mGradientTransform = new SVGAnimatedTransformList();
  }
  return mGradientTransform;
}

nsSVGElement::LengthAttributesInfo
SVGLinearGradientElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

//-------------------------- Radial Gradients ----------------------------

JSObject*
SVGRadialGradientElement::WrapNode(JSContext* aCx, JSObject* aScope,
                                   bool* aTriedToWrap)
{
  return SVGRadialGradientElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

nsSVGElement::LengthInfo SVGRadialGradientElement::sLengthInfo[5] =
{
  { &nsGkAtoms::cx, 50, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::cy, 50, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
  { &nsGkAtoms::r, 50, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::XY },
  { &nsGkAtoms::fx, 50, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::fy, 50, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGRadialGradientElement, SVGRadialGradientElementBase)
NS_IMPL_RELEASE_INHERITED(SVGRadialGradientElement, SVGRadialGradientElementBase)

NS_INTERFACE_TABLE_HEAD(SVGRadialGradientElement)
  NS_NODE_INTERFACE_TABLE5(SVGRadialGradientElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGGradientElement,
                           nsIDOMSVGRadialGradientElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGRadialGradientElement)
NS_INTERFACE_MAP_END_INHERITING(SVGRadialGradientElementBase)

//----------------------------------------------------------------------
// Implementation

SVGRadialGradientElement::SVGRadialGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGRadialGradientElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGRadialGradientElement)

//----------------------------------------------------------------------
// nsIDOMSVGRadialGradientElement methods

/* readonly attribute nsIDOMSVGAnimatedLength cx; */
NS_IMETHODIMP SVGRadialGradientElement::GetCx(nsIDOMSVGAnimatedLength * *aCx)
{
  *aCx = Cx().get();
  return NS_OK;
}

already_AddRefed<SVGAnimatedLength>
SVGRadialGradientElement::Cx()
{
  return mLengthAttributes[ATTR_CX].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength cy; */
NS_IMETHODIMP SVGRadialGradientElement::GetCy(nsIDOMSVGAnimatedLength * *aCy)
{
  *aCy = Cy().get();
  return NS_OK;
}

already_AddRefed<SVGAnimatedLength>
SVGRadialGradientElement::Cy()
{
  return mLengthAttributes[ATTR_CY].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength r; */
NS_IMETHODIMP SVGRadialGradientElement::GetR(nsIDOMSVGAnimatedLength * *aR)
{
  *aR = R().get();
  return NS_OK;
}

already_AddRefed<SVGAnimatedLength>
SVGRadialGradientElement::R()
{
  return mLengthAttributes[ATTR_R].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength fx; */
NS_IMETHODIMP SVGRadialGradientElement::GetFx(nsIDOMSVGAnimatedLength * *aFx)
{
  *aFx = Fx().get();
  return NS_OK;
}

already_AddRefed<SVGAnimatedLength>
SVGRadialGradientElement::Fx()
{
  return mLengthAttributes[ATTR_FX].ToDOMAnimatedLength(this);
}

/* readonly attribute nsIDOMSVGAnimatedLength fy; */
NS_IMETHODIMP SVGRadialGradientElement::GetFy(nsIDOMSVGAnimatedLength * *aFy)
{
  *aFy = Fy().get();
  return NS_OK;
}

already_AddRefed<SVGAnimatedLength>
SVGRadialGradientElement::Fy()
{
  return mLengthAttributes[ATTR_FY].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::LengthAttributesInfo
SVGRadialGradientElement::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

} // namespace dom
} // namespace mozilla
