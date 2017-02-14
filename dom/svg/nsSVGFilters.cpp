/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsSVGElement.h"
#include "nsGkAtoms.h"
#include "nsSVGNumber2.h"
#include "nsSVGNumberPair.h"
#include "nsSVGInteger.h"
#include "nsSVGIntegerPair.h"
#include "nsSVGBoolean.h"
#include "nsCOMPtr.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGEnum.h"
#include "SVGNumberList.h"
#include "SVGAnimatedNumberList.h"
#include "DOMSVGAnimatedNumberList.h"
#include "nsSVGFilters.h"
#include "nsLayoutUtils.h"
#include "nsSVGUtils.h"
#include "nsStyleContext.h"
#include "nsIFrame.h"
#include "imgIContainer.h"
#include "mozilla/dom/SVGFilterElement.h"
#include "nsSVGString.h"
#include "SVGContentUtils.h"
#include <algorithm>
#include "mozilla/dom/SVGAnimatedLength.h"
#include "mozilla/dom/SVGComponentTransferFunctionElement.h"
#include "mozilla/dom/SVGFEDistantLightElement.h"
#include "mozilla/dom/SVGFEFuncAElementBinding.h"
#include "mozilla/dom/SVGFEFuncBElementBinding.h"
#include "mozilla/dom/SVGFEFuncGElementBinding.h"
#include "mozilla/dom/SVGFEFuncRElementBinding.h"
#include "mozilla/dom/SVGFEPointLightElement.h"
#include "mozilla/dom/SVGFESpotLightElement.h"

#if defined(XP_WIN)
// Prevent Windows redefining LoadImage
#undef LoadImage
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

//--------------------Filter Element Base Class-----------------------

nsSVGElement::LengthInfo nsSVGFE::sLengthInfo[4] =
{
  { &nsGkAtoms::x, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::y, 0, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y },
  { &nsGkAtoms::width, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::X },
  { &nsGkAtoms::height, 100, nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE, SVGContentUtils::Y }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFE,nsSVGFEBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFE,nsSVGFEBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFE)
   // nsISupports is an ambiguous base of nsSVGFE so we have to work
   // around that
   if ( aIID.Equals(NS_GET_IID(nsSVGFE)) )
     foundInterface = static_cast<nsISupports*>(static_cast<void*>(this));
   else
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEBase)

//----------------------------------------------------------------------
// Implementation

void
nsSVGFE::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
}

bool
nsSVGFE::OutputIsTainted(const nsTArray<bool>& aInputsAreTainted,
                         nsIPrincipal* aReferencePrincipal)
{
  // This is the default implementation for OutputIsTainted.
  // Our output is tainted if we have at least one tainted input.
  for (uint32_t i = 0; i < aInputsAreTainted.Length(); i++) {
    if (aInputsAreTainted[i]) {
      return true;
    }
  }
  return false;
}

bool
nsSVGFE::AttributeAffectsRendering(int32_t aNameSpaceID,
                                   nsIAtom* aAttribute) const
{
  return aNameSpaceID == kNameSpaceID_None &&
         (aAttribute == nsGkAtoms::x ||
          aAttribute == nsGkAtoms::y ||
          aAttribute == nsGkAtoms::width ||
          aAttribute == nsGkAtoms::height ||
          aAttribute == nsGkAtoms::result);
}

already_AddRefed<SVGAnimatedLength>
nsSVGFE::X()
{
  return mLengthAttributes[ATTR_X].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
nsSVGFE::Y()
{
  return mLengthAttributes[ATTR_Y].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
nsSVGFE::Width()
{
  return mLengthAttributes[ATTR_WIDTH].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedLength>
nsSVGFE::Height()
{
  return mLengthAttributes[ATTR_HEIGHT].ToDOMAnimatedLength(this);
}

already_AddRefed<SVGAnimatedString>
nsSVGFE::Result()
{
  return GetResultImageName().ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
nsSVGFE::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFiltersMap
  };
  
  return FindAttributeDependence(name, map) ||
    nsSVGFEBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods


bool
nsSVGFE::StyleIsSetToSRGB()
{
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) return false;

  nsStyleContext* style = frame->StyleContext();
  return style->StyleSVG()->mColorInterpolationFilters ==
           NS_STYLE_COLOR_INTERPOLATION_SRGB;
}

/* virtual */ bool
nsSVGFE::HasValidDimensions() const
{
  return (!mLengthAttributes[ATTR_WIDTH].IsExplicitlySet() ||
           mLengthAttributes[ATTR_WIDTH].GetAnimValInSpecifiedUnits() > 0) &&
         (!mLengthAttributes[ATTR_HEIGHT].IsExplicitlySet() ||
           mLengthAttributes[ATTR_HEIGHT].GetAnimValInSpecifiedUnits() > 0);
}

Size
nsSVGFE::GetKernelUnitLength(nsSVGFilterInstance* aInstance,
                             nsSVGNumberPair *aKernelUnitLength)
{
  if (!aKernelUnitLength->IsExplicitlySet()) {
    return Size(1, 1);
  }

  float kernelX = aInstance->GetPrimitiveNumber(SVGContentUtils::X,
                                                aKernelUnitLength,
                                                nsSVGNumberPair::eFirst);
  float kernelY = aInstance->GetPrimitiveNumber(SVGContentUtils::Y,
                                                aKernelUnitLength,
                                                nsSVGNumberPair::eSecond);
  return Size(kernelX, kernelY);
}

nsSVGElement::LengthAttributesInfo
nsSVGFE::GetLengthInfo()
{
  return LengthAttributesInfo(mLengthAttributes, sLengthInfo,
                              ArrayLength(sLengthInfo));
}

namespace mozilla {
namespace dom {

nsSVGElement::NumberListInfo SVGComponentTransferFunctionElement::sNumberListInfo[1] =
{
  { &nsGkAtoms::tableValues }
};

nsSVGElement::NumberInfo SVGComponentTransferFunctionElement::sNumberInfo[5] =
{
  { &nsGkAtoms::slope,     1, false },
  { &nsGkAtoms::intercept, 0, false },
  { &nsGkAtoms::amplitude, 1, false },
  { &nsGkAtoms::exponent,  1, false },
  { &nsGkAtoms::offset,    0, false }
};

nsSVGEnumMapping SVGComponentTransferFunctionElement::sTypeMap[] = {
  {&nsGkAtoms::identity,
   SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY},
  {&nsGkAtoms::table,
   SVG_FECOMPONENTTRANSFER_TYPE_TABLE},
  {&nsGkAtoms::discrete,
   SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE},
  {&nsGkAtoms::linear,
   SVG_FECOMPONENTTRANSFER_TYPE_LINEAR},
  {&nsGkAtoms::gamma,
   SVG_FECOMPONENTTRANSFER_TYPE_GAMMA},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGComponentTransferFunctionElement::sEnumInfo[1] =
{
  { &nsGkAtoms::type,
    sTypeMap,
    SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY
  }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGComponentTransferFunctionElement,SVGComponentTransferFunctionElementBase)
NS_IMPL_RELEASE_INHERITED(SVGComponentTransferFunctionElement,SVGComponentTransferFunctionElementBase)

NS_INTERFACE_MAP_BEGIN(SVGComponentTransferFunctionElement)
   // nsISupports is an ambiguous base of nsSVGFE so we have to work
   // around that
   if ( aIID.Equals(NS_GET_IID(SVGComponentTransferFunctionElement)) )
     foundInterface = static_cast<nsISupports*>(static_cast<void*>(this));
   else
NS_INTERFACE_MAP_END_INHERITING(SVGComponentTransferFunctionElementBase)


//----------------------------------------------------------------------
// nsFEUnstyledElement methods

bool
SVGComponentTransferFunctionElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                               nsIAtom* aAttribute) const
{
  return aNameSpaceID == kNameSpaceID_None &&
         (aAttribute == nsGkAtoms::tableValues ||
          aAttribute == nsGkAtoms::slope ||
          aAttribute == nsGkAtoms::intercept ||
          aAttribute == nsGkAtoms::amplitude ||
          aAttribute == nsGkAtoms::exponent ||
          aAttribute == nsGkAtoms::offset ||
          aAttribute == nsGkAtoms::type);
}

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedEnumeration>
SVGComponentTransferFunctionElement::Type()
{
  return mEnumAttributes[TYPE].ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedNumberList>
SVGComponentTransferFunctionElement::TableValues()
{
  return DOMSVGAnimatedNumberList::GetDOMWrapper(
    &mNumberListAttributes[TABLEVALUES], this, TABLEVALUES);
}

already_AddRefed<SVGAnimatedNumber>
SVGComponentTransferFunctionElement::Slope()
{
  return mNumberAttributes[SLOPE].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGComponentTransferFunctionElement::Intercept()
{
  return mNumberAttributes[INTERCEPT].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGComponentTransferFunctionElement::Amplitude()
{
  return mNumberAttributes[AMPLITUDE].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGComponentTransferFunctionElement::Exponent()
{
  return mNumberAttributes[EXPONENT].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGComponentTransferFunctionElement::Offset()
{
  return mNumberAttributes[OFFSET].ToDOMAnimatedNumber(this);
}

AttributeMap
SVGComponentTransferFunctionElement::ComputeAttributes()
{
  uint32_t type = mEnumAttributes[TYPE].GetAnimValue();

  float slope, intercept, amplitude, exponent, offset;
  GetAnimatedNumberValues(&slope, &intercept, &amplitude, 
                          &exponent, &offset, nullptr);

  const SVGNumberList &tableValues =
    mNumberListAttributes[TABLEVALUES].GetAnimValue();

  AttributeMap map;
  map.Set(eComponentTransferFunctionType, type);
  map.Set(eComponentTransferFunctionSlope, slope);
  map.Set(eComponentTransferFunctionIntercept, intercept);
  map.Set(eComponentTransferFunctionAmplitude, amplitude);
  map.Set(eComponentTransferFunctionExponent, exponent);
  map.Set(eComponentTransferFunctionOffset, offset);
  if (tableValues.Length()) {
    map.Set(eComponentTransferFunctionTableValues, &tableValues[0], tableValues.Length());
  } else {
    map.Set(eComponentTransferFunctionTableValues, nullptr, 0);
  }
  return map;
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberListAttributesInfo
SVGComponentTransferFunctionElement::GetNumberListInfo()
{
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  ArrayLength(sNumberListInfo));
}

nsSVGElement::EnumAttributesInfo
SVGComponentTransferFunctionElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::NumberAttributesInfo
SVGComponentTransferFunctionElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

/* virtual */ JSObject*
SVGFEFuncRElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGFEFuncRElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEFuncR)

namespace mozilla {
namespace dom {

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEFuncRElement)

/* virtual */ JSObject*
SVGFEFuncGElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGFEFuncGElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEFuncG)

namespace mozilla {
namespace dom {

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEFuncGElement)

/* virtual */ JSObject*
SVGFEFuncBElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGFEFuncBElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEFuncB)

namespace mozilla {
namespace dom {

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEFuncBElement)

/* virtual */ JSObject*
SVGFEFuncAElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGFEFuncAElementBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEFuncA)

namespace mozilla {
namespace dom {

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEFuncAElement)

} // namespace dom
} // namespace mozilla

//--------------------------------------------------------------------
//
nsSVGElement::NumberInfo nsSVGFELightingElement::sNumberInfo[4] =
{
  { &nsGkAtoms::surfaceScale, 1, false },
  { &nsGkAtoms::diffuseConstant, 1, false },
  { &nsGkAtoms::specularConstant, 1, false },
  { &nsGkAtoms::specularExponent, 1, false }
};

nsSVGElement::NumberPairInfo nsSVGFELightingElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::kernelUnitLength, 0, 0 }
};

nsSVGElement::StringInfo nsSVGFELightingElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFELightingElement,nsSVGFELightingElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFELightingElement,nsSVGFELightingElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGFELightingElement) 
NS_INTERFACE_MAP_END_INHERITING(nsSVGFELightingElementBase)

//----------------------------------------------------------------------
// Implementation

NS_IMETHODIMP_(bool)
nsSVGFELightingElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sLightingEffectsMap
  };

  return FindAttributeDependence(name, map) ||
    nsSVGFELightingElementBase::IsAttributeMapped(name);
}

void
nsSVGFELightingElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

AttributeMap
nsSVGFELightingElement::ComputeLightAttributes(nsSVGFilterInstance* aInstance)
{
  // find specified light
  for (nsCOMPtr<nsIContent> child = nsINode::GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsAnyOfSVGElements(nsGkAtoms::feDistantLight,
                                  nsGkAtoms::fePointLight,
                                  nsGkAtoms::feSpotLight)) {
      return static_cast<SVGFELightElement*>(child.get())->ComputeLightAttributes(aInstance);
    }
  }

  AttributeMap map;
  map.Set(eLightType, (uint32_t)eLightTypeNone);
  return map;
}

FilterPrimitiveDescription
nsSVGFELightingElement::AddLightingAttributes(const FilterPrimitiveDescription& aDescription,
                                              nsSVGFilterInstance* aInstance)
{
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) {
    return FilterPrimitiveDescription(PrimitiveType::Empty);
  }

  nsStyleContext* style = frame->StyleContext();
  Color color(Color::FromABGR(style->StyleSVGReset()->mLightingColor));
  color.a = 1.f;
  float surfaceScale = mNumberAttributes[SURFACE_SCALE].GetAnimValue();
  Size kernelUnitLength =
    GetKernelUnitLength(aInstance, &mNumberPairAttributes[KERNEL_UNIT_LENGTH]);

  if (kernelUnitLength.width <= 0 || kernelUnitLength.height <= 0) {
    // According to spec, A negative or zero value is an error. See link below for details.
    // https://www.w3.org/TR/SVG/filters.html#feSpecularLightingKernelUnitLengthAttribute
    return FilterPrimitiveDescription(PrimitiveType::Empty);
  }

  FilterPrimitiveDescription descr = aDescription;
  descr.Attributes().Set(eLightingLight, ComputeLightAttributes(aInstance));
  descr.Attributes().Set(eLightingSurfaceScale, surfaceScale);
  descr.Attributes().Set(eLightingKernelUnitLength, kernelUnitLength);
  descr.Attributes().Set(eLightingColor, color);
  return descr;
}

bool
nsSVGFELightingElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                  nsIAtom* aAttribute) const
{
  return nsSVGFELightingElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::surfaceScale ||
           aAttribute == nsGkAtoms::kernelUnitLength));
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFELightingElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

nsSVGElement::NumberPairAttributesInfo
nsSVGFELightingElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  ArrayLength(sNumberPairInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGFELightingElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}
