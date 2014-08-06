/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEMorphologyElement.h"
#include "mozilla/dom/SVGFEMorphologyElementBinding.h"
#include "nsSVGFilterInstance.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEMorphology)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGFEMorphologyElement::WrapNode(JSContext* aCx)
{
  return SVGFEMorphologyElementBinding::Wrap(aCx, this);
}

nsSVGElement::NumberPairInfo SVGFEMorphologyElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::radius, 0, 0 }
};

nsSVGEnumMapping SVGFEMorphologyElement::sOperatorMap[] = {
  {&nsGkAtoms::erode, SVG_OPERATOR_ERODE},
  {&nsGkAtoms::dilate, SVG_OPERATOR_DILATE},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGFEMorphologyElement::sEnumInfo[1] =
{
  { &nsGkAtoms::_operator,
    sOperatorMap,
    SVG_OPERATOR_ERODE
  }
};

nsSVGElement::StringInfo SVGFEMorphologyElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEMorphologyElement)


//----------------------------------------------------------------------
// SVGFEMorphologyElement methods

already_AddRefed<SVGAnimatedString>
SVGFEMorphologyElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<SVGAnimatedEnumeration>
SVGFEMorphologyElement::Operator()
{
  return mEnumAttributes[OPERATOR].ToDOMAnimatedEnum(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEMorphologyElement::RadiusX()
{
  return mNumberPairAttributes[RADIUS].ToDOMAnimatedNumber(nsSVGNumberPair::eFirst, this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEMorphologyElement::RadiusY()
{
  return mNumberPairAttributes[RADIUS].ToDOMAnimatedNumber(nsSVGNumberPair::eSecond, this);
}

void
SVGFEMorphologyElement::SetRadius(float rx, float ry)
{
  mNumberPairAttributes[RADIUS].SetBaseValues(rx, ry, this);
}

void
SVGFEMorphologyElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

#define MORPHOLOGY_EPSILON 0.0001

void
SVGFEMorphologyElement::GetRXY(int32_t *aRX, int32_t *aRY,
                               const nsSVGFilterInstance& aInstance)
{
  // Subtract an epsilon here because we don't want a value that's just
  // slightly larger than an integer to round up to the next integer; it's
  // probably meant to be the integer it's close to, modulo machine precision
  // issues.
  *aRX = NSToIntCeil(aInstance.GetPrimitiveNumber(SVGContentUtils::X,
                                                  &mNumberPairAttributes[RADIUS],
                                                  nsSVGNumberPair::eFirst) -
                     MORPHOLOGY_EPSILON);
  *aRY = NSToIntCeil(aInstance.GetPrimitiveNumber(SVGContentUtils::Y,
                                                  &mNumberPairAttributes[RADIUS],
                                                  nsSVGNumberPair::eSecond) -
                     MORPHOLOGY_EPSILON);
}

FilterPrimitiveDescription
SVGFEMorphologyElement::GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                                                const IntRect& aFilterSubregion,
                                                const nsTArray<bool>& aInputsAreTainted,
                                                nsTArray<RefPtr<SourceSurface>>& aInputImages)
{
  int32_t rx, ry;
  GetRXY(&rx, &ry, *aInstance);
  FilterPrimitiveDescription descr(PrimitiveType::Morphology);
  descr.Attributes().Set(eMorphologyRadii, Size(rx, ry));
  descr.Attributes().Set(eMorphologyOperator,
                         (uint32_t)mEnumAttributes[OPERATOR].GetAnimValue());
  return descr;
}

bool
SVGFEMorphologyElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                  nsIAtom* aAttribute) const
{
  return SVGFEMorphologyElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::radius ||
           aAttribute == nsGkAtoms::_operator));
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberPairAttributesInfo
SVGFEMorphologyElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  ArrayLength(sNumberPairInfo));
}

nsSVGElement::EnumAttributesInfo
SVGFEMorphologyElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
SVGFEMorphologyElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
