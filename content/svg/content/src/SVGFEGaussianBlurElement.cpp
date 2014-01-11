/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEGaussianBlurElement.h"
#include "mozilla/dom/SVGFEGaussianBlurElementBinding.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGUtils.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEGaussianBlur)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGFEGaussianBlurElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEGaussianBlurElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::NumberPairInfo SVGFEGaussianBlurElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::stdDeviation, 0, 0 }
};

nsSVGElement::StringInfo SVGFEGaussianBlurElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEGaussianBlurElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedString>
SVGFEGaussianBlurElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEGaussianBlurElement::StdDeviationX()
{
  return mNumberPairAttributes[STD_DEV].ToDOMAnimatedNumber(nsSVGNumberPair::eFirst, this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEGaussianBlurElement::StdDeviationY()
{
  return mNumberPairAttributes[STD_DEV].ToDOMAnimatedNumber(nsSVGNumberPair::eSecond, this);
}

void
SVGFEGaussianBlurElement::SetStdDeviation(float stdDeviationX, float stdDeviationY)
{
  mNumberPairAttributes[STD_DEV].SetBaseValues(stdDeviationX, stdDeviationY, this);
}

static const float kMaxStdDeviation = 500;

FilterPrimitiveDescription
SVGFEGaussianBlurElement::GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                                                  const IntRect& aFilterSubregion,
                                                  nsTArray<RefPtr<SourceSurface>>& aInputImages)
{
  float stdX = aInstance->GetPrimitiveNumber(SVGContentUtils::X,
                                             &mNumberPairAttributes[STD_DEV],
                                             nsSVGNumberPair::eFirst);
  float stdY = aInstance->GetPrimitiveNumber(SVGContentUtils::Y,
                                             &mNumberPairAttributes[STD_DEV],
                                             nsSVGNumberPair::eSecond);
  if (stdX < 0 || stdY < 0) {
    return FilterPrimitiveDescription(FilterPrimitiveDescription::eNone);
  }

  stdX = std::min(stdX, kMaxStdDeviation);
  stdY = std::min(stdY, kMaxStdDeviation);
  FilterPrimitiveDescription descr(FilterPrimitiveDescription::eGaussianBlur);
  descr.Attributes().Set(eGaussianBlurStdDeviation, Size(stdX, stdY));
  return descr;
}

bool
SVGFEGaussianBlurElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                    nsIAtom* aAttribute) const
{
  return SVGFEGaussianBlurElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::stdDeviation));
}

void
SVGFEGaussianBlurElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberPairAttributesInfo
SVGFEGaussianBlurElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  ArrayLength(sNumberPairInfo));
}

nsSVGElement::StringAttributesInfo
SVGFEGaussianBlurElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
