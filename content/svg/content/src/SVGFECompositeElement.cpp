/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFECompositeElement.h"
#include "mozilla/dom/SVGFECompositeElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEComposite)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGFECompositeElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFECompositeElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::NumberInfo SVGFECompositeElement::sNumberInfo[4] =
{
  { &nsGkAtoms::k1, 0, false },
  { &nsGkAtoms::k2, 0, false },
  { &nsGkAtoms::k3, 0, false },
  { &nsGkAtoms::k4, 0, false }
};

nsSVGEnumMapping SVGFECompositeElement::sOperatorMap[] = {
  {&nsGkAtoms::over, SVG_FECOMPOSITE_OPERATOR_OVER},
  {&nsGkAtoms::in, SVG_FECOMPOSITE_OPERATOR_IN},
  {&nsGkAtoms::out, SVG_FECOMPOSITE_OPERATOR_OUT},
  {&nsGkAtoms::atop, SVG_FECOMPOSITE_OPERATOR_ATOP},
  {&nsGkAtoms::xor_, SVG_FECOMPOSITE_OPERATOR_XOR},
  {&nsGkAtoms::arithmetic, SVG_FECOMPOSITE_OPERATOR_ARITHMETIC},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGFECompositeElement::sEnumInfo[1] =
{
  { &nsGkAtoms::_operator,
    sOperatorMap,
    SVG_FECOMPOSITE_OPERATOR_OVER
  }
};

nsSVGElement::StringInfo SVGFECompositeElement::sStringInfo[3] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true },
  { &nsGkAtoms::in2, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFECompositeElement)

already_AddRefed<SVGAnimatedString>
SVGFECompositeElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<SVGAnimatedString>
SVGFECompositeElement::In2()
{
  return mStringAttributes[IN2].ToDOMAnimatedString(this);
}

already_AddRefed<SVGAnimatedEnumeration>
SVGFECompositeElement::Operator()
{
  return mEnumAttributes[OPERATOR].ToDOMAnimatedEnum(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFECompositeElement::K1()
{
  return mNumberAttributes[ATTR_K1].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFECompositeElement::K2()
{
  return mNumberAttributes[ATTR_K2].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFECompositeElement::K3()
{
  return mNumberAttributes[ATTR_K3].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFECompositeElement::K4()
{
  return mNumberAttributes[ATTR_K4].ToDOMAnimatedNumber(this);
}

void
SVGFECompositeElement::SetK(float k1, float k2, float k3, float k4)
{
  mNumberAttributes[ATTR_K1].SetBaseValue(k1, this);
  mNumberAttributes[ATTR_K2].SetBaseValue(k2, this);
  mNumberAttributes[ATTR_K3].SetBaseValue(k3, this);
  mNumberAttributes[ATTR_K4].SetBaseValue(k4, this);
}

FilterPrimitiveDescription
SVGFECompositeElement::GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                                               const IntRect& aFilterSubregion,
                                               const nsTArray<bool>& aInputsAreTainted,
                                               nsTArray<RefPtr<SourceSurface>>& aInputImages)
{
  FilterPrimitiveDescription descr(PrimitiveType::Composite);
  uint32_t op = mEnumAttributes[OPERATOR].GetAnimValue();
  descr.Attributes().Set(eCompositeOperator, op);

  if (op == SVG_FECOMPOSITE_OPERATOR_ARITHMETIC) {
    float k[4];
    GetAnimatedNumberValues(k, k+1, k+2, k+3, nullptr);
    descr.Attributes().Set(eCompositeCoefficients, k, 4);
  }

  return descr;
}

bool
SVGFECompositeElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                 nsIAtom* aAttribute) const
{
  return SVGFECompositeElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::in2 ||
           aAttribute == nsGkAtoms::k1 ||
           aAttribute == nsGkAtoms::k2 ||
           aAttribute == nsGkAtoms::k3 ||
           aAttribute == nsGkAtoms::k4 ||
           aAttribute == nsGkAtoms::_operator));
}

void
SVGFECompositeElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN2], this));
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
SVGFECompositeElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

nsSVGElement::EnumAttributesInfo
SVGFECompositeElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
SVGFECompositeElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
