/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEDropShadowElement.h"
#include "mozilla/dom/SVGFEDropShadowElementBinding.h"
#include "nsIFrame.h"
#include "nsSVGFilterInstance.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEDropShadow)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGFEDropShadowElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGFEDropShadowElementBinding::Wrap(aCx, this, aGivenProto);
}

nsSVGElement::NumberInfo SVGFEDropShadowElement::sNumberInfo[2] =
{
  { &nsGkAtoms::dx, 2, false },
  { &nsGkAtoms::dy, 2, false }
};

nsSVGElement::NumberPairInfo SVGFEDropShadowElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::stdDeviation, 2, 2 }
};

nsSVGElement::StringInfo SVGFEDropShadowElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEDropShadowElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedString>
SVGFEDropShadowElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEDropShadowElement::Dx()
{
  return mNumberAttributes[DX].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEDropShadowElement::Dy()
{
  return mNumberAttributes[DY].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEDropShadowElement::StdDeviationX()
{
  return mNumberPairAttributes[STD_DEV].ToDOMAnimatedNumber(nsSVGNumberPair::eFirst, this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEDropShadowElement::StdDeviationY()
{
  return mNumberPairAttributes[STD_DEV].ToDOMAnimatedNumber(nsSVGNumberPair::eSecond, this);
}

void
SVGFEDropShadowElement::SetStdDeviation(float stdDeviationX, float stdDeviationY)
{
  mNumberPairAttributes[STD_DEV].SetBaseValues(stdDeviationX, stdDeviationY, this);
}

FilterPrimitiveDescription
SVGFEDropShadowElement::GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                                                const IntRect& aFilterSubregion,
                                                const nsTArray<bool>& aInputsAreTainted,
                                                nsTArray<RefPtr<SourceSurface>>& aInputImages)
{
  float stdX = aInstance->GetPrimitiveNumber(SVGContentUtils::X,
                                             &mNumberPairAttributes[STD_DEV],
                                             nsSVGNumberPair::eFirst);
  float stdY = aInstance->GetPrimitiveNumber(SVGContentUtils::Y,
                                             &mNumberPairAttributes[STD_DEV],
                                             nsSVGNumberPair::eSecond);
  if (stdX < 0 || stdY < 0) {
    return FilterPrimitiveDescription(PrimitiveType::Empty);
  }

  IntPoint offset(int32_t(aInstance->GetPrimitiveNumber(
                            SVGContentUtils::X, &mNumberAttributes[DX])),
                  int32_t(aInstance->GetPrimitiveNumber(
                            SVGContentUtils::Y, &mNumberAttributes[DY])));

  FilterPrimitiveDescription descr(PrimitiveType::DropShadow);
  descr.Attributes().Set(eDropShadowStdDeviation, Size(stdX, stdY));
  descr.Attributes().Set(eDropShadowOffset, offset);

  nsIFrame* frame = GetPrimaryFrame();
  if (frame) {
    const nsStyleSVGReset* styleSVGReset = frame->Style()->StyleSVGReset();
    Color color(Color::FromABGR(styleSVGReset->mFloodColor.CalcColor(frame)));
    color.a *= styleSVGReset->mFloodOpacity;
    descr.Attributes().Set(eDropShadowColor, color);
  } else {
    descr.Attributes().Set(eDropShadowColor, Color());
  }
  return descr;
}

bool
SVGFEDropShadowElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                  nsAtom* aAttribute) const
{
  return SVGFEDropShadowElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::stdDeviation ||
           aAttribute == nsGkAtoms::dx ||
           aAttribute == nsGkAtoms::dy));
}

void
SVGFEDropShadowElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGFEDropShadowElement::IsAttributeMapped(const nsAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFEFloodMap
  };

  return FindAttributeDependence(name, map) ||
    SVGFEDropShadowElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
SVGFEDropShadowElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

nsSVGElement::NumberPairAttributesInfo
SVGFEDropShadowElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  ArrayLength(sNumberPairInfo));
}

nsSVGElement::StringAttributesInfo
SVGFEDropShadowElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
