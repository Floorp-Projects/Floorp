/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEConvolveMatrixElement.h"
#include "mozilla/dom/SVGFEConvolveMatrixElementBinding.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "DOMSVGAnimatedNumberList.h"
#include "nsSVGUtils.h"
#include "nsSVGFilterInstance.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEConvolveMatrix)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGFEConvolveMatrixElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGFEConvolveMatrixElementBinding::Wrap(aCx, this, aGivenProto);
}

nsSVGElement::NumberInfo SVGFEConvolveMatrixElement::sNumberInfo[2] =
{
  { &nsGkAtoms::divisor, 1, false },
  { &nsGkAtoms::bias, 0, false }
};

nsSVGElement::NumberPairInfo SVGFEConvolveMatrixElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::kernelUnitLength, 0, 0 }
};

nsSVGElement::IntegerInfo SVGFEConvolveMatrixElement::sIntegerInfo[2] =
{
  { &nsGkAtoms::targetX, 0 },
  { &nsGkAtoms::targetY, 0 }
};

nsSVGElement::IntegerPairInfo SVGFEConvolveMatrixElement::sIntegerPairInfo[1] =
{
  { &nsGkAtoms::order, 3, 3 }
};

nsSVGElement::BooleanInfo SVGFEConvolveMatrixElement::sBooleanInfo[1] =
{
  { &nsGkAtoms::preserveAlpha, false }
};

nsSVGEnumMapping SVGFEConvolveMatrixElement::sEdgeModeMap[] = {
  {&nsGkAtoms::duplicate, SVG_EDGEMODE_DUPLICATE},
  {&nsGkAtoms::wrap, SVG_EDGEMODE_WRAP},
  {&nsGkAtoms::none, SVG_EDGEMODE_NONE},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGFEConvolveMatrixElement::sEnumInfo[1] =
{
  { &nsGkAtoms::edgeMode,
    sEdgeModeMap,
    SVG_EDGEMODE_DUPLICATE
  }
};

nsSVGElement::StringInfo SVGFEConvolveMatrixElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

nsSVGElement::NumberListInfo SVGFEConvolveMatrixElement::sNumberListInfo[1] =
{
  { &nsGkAtoms::kernelMatrix }
};

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEConvolveMatrixElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedString>
SVGFEConvolveMatrixElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<SVGAnimatedInteger>
SVGFEConvolveMatrixElement::OrderX()
{
  return mIntegerPairAttributes[ORDER].ToDOMAnimatedInteger(nsSVGIntegerPair::eFirst, this);
}

already_AddRefed<SVGAnimatedInteger>
SVGFEConvolveMatrixElement::OrderY()
{
  return mIntegerPairAttributes[ORDER].ToDOMAnimatedInteger(nsSVGIntegerPair::eSecond, this);
}

already_AddRefed<DOMSVGAnimatedNumberList>
SVGFEConvolveMatrixElement::KernelMatrix()
{
  return DOMSVGAnimatedNumberList::GetDOMWrapper(&mNumberListAttributes[KERNELMATRIX],
                                                 this, KERNELMATRIX);
}

already_AddRefed<SVGAnimatedInteger>
SVGFEConvolveMatrixElement::TargetX()
{
  return mIntegerAttributes[TARGET_X].ToDOMAnimatedInteger(this);
}

already_AddRefed<SVGAnimatedInteger>
SVGFEConvolveMatrixElement::TargetY()
{
  return mIntegerAttributes[TARGET_Y].ToDOMAnimatedInteger(this);
}

already_AddRefed<SVGAnimatedEnumeration>
SVGFEConvolveMatrixElement::EdgeMode()
{
  return mEnumAttributes[EDGEMODE].ToDOMAnimatedEnum(this);
}

already_AddRefed<SVGAnimatedBoolean>
SVGFEConvolveMatrixElement::PreserveAlpha()
{
  return mBooleanAttributes[PRESERVEALPHA].ToDOMAnimatedBoolean(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEConvolveMatrixElement::Divisor()
{
  return mNumberAttributes[DIVISOR].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEConvolveMatrixElement::Bias()
{
  return mNumberAttributes[BIAS].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEConvolveMatrixElement::KernelUnitLengthX()
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(nsSVGNumberPair::eFirst,
                                                                       this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEConvolveMatrixElement::KernelUnitLengthY()
{
  return mNumberPairAttributes[KERNEL_UNIT_LENGTH].ToDOMAnimatedNumber(nsSVGNumberPair::eSecond,
                                                                       this);
}

void
SVGFEConvolveMatrixElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

FilterPrimitiveDescription
SVGFEConvolveMatrixElement::GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                                                    const IntRect& aFilterSubregion,
                                                    const nsTArray<bool>& aInputsAreTainted,
                                                    nsTArray<RefPtr<SourceSurface>>& aInputImages)
{
  const FilterPrimitiveDescription failureDescription(PrimitiveType::Empty);

  const SVGNumberList &kernelMatrix =
    mNumberListAttributes[KERNELMATRIX].GetAnimValue();
  uint32_t kmLength = kernelMatrix.Length();

  int32_t orderX = mIntegerPairAttributes[ORDER].GetAnimValue(nsSVGIntegerPair::eFirst);
  int32_t orderY = mIntegerPairAttributes[ORDER].GetAnimValue(nsSVGIntegerPair::eSecond);

  if (orderX <= 0 || orderY <= 0 ||
      static_cast<uint32_t>(orderX * orderY) != kmLength) {
    return failureDescription;
  }

  int32_t targetX, targetY;
  GetAnimatedIntegerValues(&targetX, &targetY, nullptr);

  if (mIntegerAttributes[TARGET_X].IsExplicitlySet()) {
    if (targetX < 0 || targetX >= orderX)
      return failureDescription;
  } else {
    targetX = orderX / 2;
  }
  if (mIntegerAttributes[TARGET_Y].IsExplicitlySet()) {
    if (targetY < 0 || targetY >= orderY)
      return failureDescription;
  } else {
    targetY = orderY / 2;
  }

  if (orderX > NS_SVG_OFFSCREEN_MAX_DIMENSION ||
      orderY > NS_SVG_OFFSCREEN_MAX_DIMENSION)
    return failureDescription;
  UniquePtr<float[]> kernel = MakeUniqueFallible<float[]>(orderX * orderY);
  if (!kernel)
    return failureDescription;
  for (uint32_t i = 0; i < kmLength; i++) {
    kernel[kmLength - 1 - i] = kernelMatrix[i];
  }

  float divisor;
  if (mNumberAttributes[DIVISOR].IsExplicitlySet()) {
    divisor = mNumberAttributes[DIVISOR].GetAnimValue();
    if (divisor == 0)
      return failureDescription;
  } else {
    divisor = kernel[0];
    for (uint32_t i = 1; i < kmLength; i++)
      divisor += kernel[i];
    if (divisor == 0)
      divisor = 1;
  }

  uint32_t edgeMode = mEnumAttributes[EDGEMODE].GetAnimValue();
  bool preserveAlpha = mBooleanAttributes[PRESERVEALPHA].GetAnimValue();
  float bias = mNumberAttributes[BIAS].GetAnimValue();

  Size kernelUnitLength =
    GetKernelUnitLength(aInstance, &mNumberPairAttributes[KERNEL_UNIT_LENGTH]);

  FilterPrimitiveDescription descr(PrimitiveType::ConvolveMatrix);
  AttributeMap& atts = descr.Attributes();
  atts.Set(eConvolveMatrixKernelSize, IntSize(orderX, orderY));
  atts.Set(eConvolveMatrixKernelMatrix, &kernelMatrix[0], kmLength);
  atts.Set(eConvolveMatrixDivisor, divisor);
  atts.Set(eConvolveMatrixBias, bias);
  atts.Set(eConvolveMatrixTarget, IntPoint(targetX, targetY));
  atts.Set(eConvolveMatrixEdgeMode, edgeMode);
  atts.Set(eConvolveMatrixKernelUnitLength, kernelUnitLength);
  atts.Set(eConvolveMatrixPreserveAlpha, preserveAlpha);

  return descr;
}

bool
SVGFEConvolveMatrixElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                      nsIAtom* aAttribute) const
{
  return SVGFEConvolveMatrixElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::divisor ||
           aAttribute == nsGkAtoms::bias ||
           aAttribute == nsGkAtoms::kernelUnitLength ||
           aAttribute == nsGkAtoms::targetX ||
           aAttribute == nsGkAtoms::targetY ||
           aAttribute == nsGkAtoms::order ||
           aAttribute == nsGkAtoms::preserveAlpha||
           aAttribute == nsGkAtoms::edgeMode ||
           aAttribute == nsGkAtoms::kernelMatrix));
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
SVGFEConvolveMatrixElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

nsSVGElement::NumberPairAttributesInfo
SVGFEConvolveMatrixElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  ArrayLength(sNumberPairInfo));
}

nsSVGElement::IntegerAttributesInfo
SVGFEConvolveMatrixElement::GetIntegerInfo()
{
  return IntegerAttributesInfo(mIntegerAttributes, sIntegerInfo,
                               ArrayLength(sIntegerInfo));
}

nsSVGElement::IntegerPairAttributesInfo
SVGFEConvolveMatrixElement::GetIntegerPairInfo()
{
  return IntegerPairAttributesInfo(mIntegerPairAttributes, sIntegerPairInfo,
                                   ArrayLength(sIntegerPairInfo));
}

nsSVGElement::BooleanAttributesInfo
SVGFEConvolveMatrixElement::GetBooleanInfo()
{
  return BooleanAttributesInfo(mBooleanAttributes, sBooleanInfo,
                               ArrayLength(sBooleanInfo));
}

nsSVGElement::EnumAttributesInfo
SVGFEConvolveMatrixElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
SVGFEConvolveMatrixElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

nsSVGElement::NumberListAttributesInfo
SVGFEConvolveMatrixElement::GetNumberListInfo()
{
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  ArrayLength(sNumberListInfo));
}

} // namespace dom
} // namespace mozilla
