/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFECompositeElement.h"
#include "mozilla/dom/SVGFECompositeElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEComposite)

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

already_AddRefed<nsIDOMSVGAnimatedEnumeration>
SVGFECompositeElement::Operator()
{
  return mEnumAttributes[OPERATOR].ToDOMAnimatedEnum(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFECompositeElement::K1()
{
  return mNumberAttributes[ATTR_K1].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFECompositeElement::K2()
{
  return mNumberAttributes[ATTR_K2].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
SVGFECompositeElement::K3()
{
  return mNumberAttributes[ATTR_K3].ToDOMAnimatedNumber(this);
}

already_AddRefed<nsIDOMSVGAnimatedNumber>
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

nsresult
SVGFECompositeElement::Filter(nsSVGFilterInstance* instance,
                              const nsTArray<const Image*>& aSources,
                              const Image* aTarget,
                              const nsIntRect& rect)
{
  uint16_t op = mEnumAttributes[OPERATOR].GetAnimValue();

  // Cairo does not support arithmetic operator
  if (op == SVG_FECOMPOSITE_OPERATOR_ARITHMETIC) {
    float k1, k2, k3, k4;
    GetAnimatedNumberValues(&k1, &k2, &k3, &k4, nullptr);

    // Copy the first source image
    CopyRect(aTarget, aSources[0], rect);

    uint8_t* sourceData = aSources[1]->mImage->Data();
    uint8_t* targetData = aTarget->mImage->Data();
    uint32_t stride = aTarget->mImage->Stride();

    // Blend in the second source image
    float k1Scaled = k1 / 255.0f;
    float k4Scaled = k4*255.0f;
    for (int32_t x = rect.x; x < rect.XMost(); x++) {
      for (int32_t y = rect.y; y < rect.YMost(); y++) {
        uint32_t targIndex = y * stride + 4 * x;
        for (int32_t i = 0; i < 4; i++) {
          uint8_t i1 = targetData[targIndex + i];
          uint8_t i2 = sourceData[targIndex + i];
          float result = k1Scaled*i1*i2 + k2*i1 + k3*i2 + k4Scaled;
          targetData[targIndex + i] =
                       static_cast<uint8_t>(clamped(result, 0.f, 255.f));
        }
      }
    }
    return NS_OK;
  }

  // Cairo supports the operation we are trying to perform

  gfxContext ctx(aTarget->mImage);
  ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
  ctx.SetSource(aSources[1]->mImage);
  // Ensure rendering is limited to the filter primitive subregion
  ctx.Clip(aTarget->mFilterPrimitiveSubregion);
  ctx.Paint();

  if (op < SVG_FECOMPOSITE_OPERATOR_OVER || op > SVG_FECOMPOSITE_OPERATOR_XOR) {
    return NS_ERROR_FAILURE;
  }
  static const gfxContext::GraphicsOperator opMap[] = {
                                           gfxContext::OPERATOR_DEST,
                                           gfxContext::OPERATOR_OVER,
                                           gfxContext::OPERATOR_IN,
                                           gfxContext::OPERATOR_OUT,
                                           gfxContext::OPERATOR_ATOP,
                                           gfxContext::OPERATOR_XOR };
  ctx.SetOperator(opMap[op]);
  ctx.SetSource(aSources[0]->mImage);
  ctx.Paint();
  return NS_OK;
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

nsIntRect
SVGFECompositeElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  uint16_t op = mEnumAttributes[OPERATOR].GetAnimValue();

  if (op == SVG_FECOMPOSITE_OPERATOR_ARITHMETIC) {
    // "arithmetic" operator can produce non-zero alpha values even where
    // all input alphas are zero, so we can actually render outside the
    // union of the source bboxes.
    // XXX we could also check that k4 is nonzero and check for other
    // cases like k1/k2 or k1/k3 zero.
    return GetMaxRect();
  }

  if (op == SVG_FECOMPOSITE_OPERATOR_IN ||
      op == SVG_FECOMPOSITE_OPERATOR_ATOP) {
    // We will only draw where in2 has nonzero alpha, so it's a good
    // bounding box for us
    return aSourceBBoxes[1];
  }

  // The regular Porter-Duff operators always compute zero alpha values
  // where all sources have zero alpha, so the union of their bounding
  // boxes is also a bounding box for our rendering
  return SVGFECompositeElementBase::ComputeTargetBBox(aSourceBBoxes, aInstance);
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
