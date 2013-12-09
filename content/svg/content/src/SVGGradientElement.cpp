/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGGradientElement.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/dom/SVGAnimatedTransformList.h"
#include "mozilla/dom/SVGRadialGradientElementBinding.h"
#include "mozilla/dom/SVGLinearGradientElementBinding.h"
#include "nsCOMPtr.h"
#include "nsGkAtoms.h"
#include "nsSVGElement.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(LinearGradient)
NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(RadialGradient)

namespace mozilla {
namespace dom {

//--------------------- Gradients------------------------

nsSVGEnumMapping SVGGradientElement::sSpreadMethodMap[] = {
  {&nsGkAtoms::pad, SVG_SPREADMETHOD_PAD},
  {&nsGkAtoms::reflect, SVG_SPREADMETHOD_REFLECT},
  {&nsGkAtoms::repeat, SVG_SPREADMETHOD_REPEAT},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGGradientElement::sEnumInfo[2] =
{
  { &nsGkAtoms::gradientUnits,
    sSVGUnitTypesMap,
    SVG_UNIT_TYPE_OBJECTBOUNDINGBOX
  },
  { &nsGkAtoms::spreadMethod,
    sSpreadMethodMap,
    SVG_SPREADMETHOD_PAD
  }
};

nsSVGElement::StringInfo SVGGradientElement::sStringInfo[1] =
{
  { &nsGkAtoms::href, kNameSpaceID_XLink, true }
};

//----------------------------------------------------------------------
// Implementation

SVGGradientElement::SVGGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGGradientElementBase(aNodeInfo)
{
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

already_AddRefed<SVGAnimatedEnumeration>
SVGGradientElement::GradientUnits()
{
  return mEnumAttributes[GRADIENTUNITS].ToDOMAnimatedEnum(this);
}

/* readonly attribute SVGAnimatedTransformList gradientTransform; */
already_AddRefed<SVGAnimatedTransformList>
SVGGradientElement::GradientTransform()
{
  // We're creating a DOM wrapper, so we must tell GetAnimatedTransformList
  // to allocate the SVGAnimatedTransformList if it hasn't already done so:
  return SVGAnimatedTransformList::GetDOMWrapper(
           GetAnimatedTransformList(DO_ALLOCATE), this);
}

already_AddRefed<SVGAnimatedEnumeration>
SVGGradientElement::SpreadMethod()
{
  return mEnumAttributes[SPREADMETHOD].ToDOMAnimatedEnum(this);
}

already_AddRefed<SVGAnimatedString>
SVGGradientElement::Href()
{
  return mStringAttributes[HREF].ToDOMAnimatedString(this);
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
SVGLinearGradientElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGLinearGradientElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::LengthInfo SVGLinearGradientElement::sLengthInfo[4] =
{
  { &nsGkAtoms::x1, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::y1, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
  { &nsGkAtoms::x2, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::y2, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
};

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

already_AddRefed<SVGAnimatedLength>
SVGLinearGradientElement::X1()
{
  return mLengthAttributes[ATTR_X1].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGLinearGradientElement::Y1()
{
  return mLengthAttributes[ATTR_Y1].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGLinearGradientElement::X2()
{
  return mLengthAttributes[ATTR_X2].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGLinearGradientElement::Y2()
{
  return mLengthAttributes[ATTR_Y2].ToDOMAnimatedLength(this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGAnimatedTransformList*
SVGGradientElement::GetAnimatedTransformList(uint32_t aFlags)
{
  if (!mGradientTransform && (aFlags & DO_ALLOCATE)) {
    mGradientTransform = new nsSVGAnimatedTransformList();
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
SVGRadialGradientElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGRadialGradientElementBinding::Wrap(aCx, aScope, this);
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
// Implementation

SVGRadialGradientElement::SVGRadialGradientElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGRadialGradientElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGRadialGradientElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedLength>
SVGRadialGradientElement::Cx()
{
  return mLengthAttributes[ATTR_CX].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGRadialGradientElement::Cy()
{
  return mLengthAttributes[ATTR_CY].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGRadialGradientElement::R()
{
  return mLengthAttributes[ATTR_R].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
SVGRadialGradientElement::Fx()
{
  return mLengthAttributes[ATTR_FX].ToDOMAnimatedLength(this);
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
