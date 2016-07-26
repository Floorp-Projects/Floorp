/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFETurbulenceElement.h"
#include "mozilla/dom/SVGFETurbulenceElementBinding.h"
#include "nsSVGFilterInstance.h"
#include "nsSVGUtils.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FETurbulence)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

// Stitch Options
static const unsigned short SVG_STITCHTYPE_STITCH = 1;
static const unsigned short SVG_STITCHTYPE_NOSTITCH = 2;

static const int32_t MAX_OCTAVES = 10;

JSObject*
SVGFETurbulenceElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGFETurbulenceElementBinding::Wrap(aCx, this, aGivenProto);
}

nsSVGElement::NumberInfo SVGFETurbulenceElement::sNumberInfo[1] =
{
  { &nsGkAtoms::seed, 0, false }
};

nsSVGElement::NumberPairInfo SVGFETurbulenceElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::baseFrequency, 0, 0 }
};

nsSVGElement::IntegerInfo SVGFETurbulenceElement::sIntegerInfo[1] =
{
  { &nsGkAtoms::numOctaves, 1 }
};

nsSVGEnumMapping SVGFETurbulenceElement::sTypeMap[] = {
  {&nsGkAtoms::fractalNoise,
   SVG_TURBULENCE_TYPE_FRACTALNOISE},
  {&nsGkAtoms::turbulence,
   SVG_TURBULENCE_TYPE_TURBULENCE},
  {nullptr, 0}
};

nsSVGEnumMapping SVGFETurbulenceElement::sStitchTilesMap[] = {
  {&nsGkAtoms::stitch,
   SVG_STITCHTYPE_STITCH},
  {&nsGkAtoms::noStitch,
   SVG_STITCHTYPE_NOSTITCH},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGFETurbulenceElement::sEnumInfo[2] =
{
  { &nsGkAtoms::type,
    sTypeMap,
    SVG_TURBULENCE_TYPE_TURBULENCE
  },
  { &nsGkAtoms::stitchTiles,
    sStitchTilesMap,
    SVG_STITCHTYPE_NOSTITCH
  }
};

nsSVGElement::StringInfo SVGFETurbulenceElement::sStringInfo[1] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFETurbulenceElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedNumber>
SVGFETurbulenceElement::BaseFrequencyX()
{
  return mNumberPairAttributes[BASE_FREQ].ToDOMAnimatedNumber(nsSVGNumberPair::eFirst, this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFETurbulenceElement::BaseFrequencyY()
{
  return mNumberPairAttributes[BASE_FREQ].ToDOMAnimatedNumber(nsSVGNumberPair::eSecond, this);
}

already_AddRefed<SVGAnimatedInteger>
SVGFETurbulenceElement::NumOctaves()
{
  return mIntegerAttributes[OCTAVES].ToDOMAnimatedInteger(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFETurbulenceElement::Seed()
{
  return mNumberAttributes[SEED].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedEnumeration>
SVGFETurbulenceElement::StitchTiles()
{
  return mEnumAttributes[STITCHTILES].ToDOMAnimatedEnum(this);
}

already_AddRefed<SVGAnimatedEnumeration>
SVGFETurbulenceElement::Type()
{
  return mEnumAttributes[TYPE].ToDOMAnimatedEnum(this);
}

FilterPrimitiveDescription
SVGFETurbulenceElement::GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                                                const IntRect& aFilterSubregion,
                                                const nsTArray<bool>& aInputsAreTainted,
                                                nsTArray<RefPtr<SourceSurface>>& aInputImages)
{
  float fX = mNumberPairAttributes[BASE_FREQ].GetAnimValue(nsSVGNumberPair::eFirst);
  float fY = mNumberPairAttributes[BASE_FREQ].GetAnimValue(nsSVGNumberPair::eSecond);
  float seed = mNumberAttributes[OCTAVES].GetAnimValue();
  uint32_t octaves = clamped(mIntegerAttributes[OCTAVES].GetAnimValue(), 0, MAX_OCTAVES);
  uint32_t type = mEnumAttributes[TYPE].GetAnimValue();
  uint16_t stitch = mEnumAttributes[STITCHTILES].GetAnimValue();

  if (fX == 0 || fY == 0) {
    return FilterPrimitiveDescription(PrimitiveType::Empty);
  }

  // We interpret the base frequency as relative to user space units. In other
  // words, we consider one turbulence base period to be 1 / fX user space
  // units wide and 1 / fY user space units high. We do not scale the frequency
  // depending on the filter primitive region.
  gfxRect firstPeriodInUserSpace(0, 0, 1 / fX, 1 / fY);
  gfxRect firstPeriodInFilterSpace = aInstance->UserSpaceToFilterSpace(firstPeriodInUserSpace);
  Size frequencyInFilterSpace(1 / firstPeriodInFilterSpace.width,
                              1 / firstPeriodInFilterSpace.height);
  gfxPoint offset = firstPeriodInFilterSpace.TopLeft();

  FilterPrimitiveDescription descr(PrimitiveType::Turbulence);
  descr.Attributes().Set(eTurbulenceOffset, IntPoint::Truncate(offset.x, offset.y));
  descr.Attributes().Set(eTurbulenceBaseFrequency, frequencyInFilterSpace);
  descr.Attributes().Set(eTurbulenceSeed, seed);
  descr.Attributes().Set(eTurbulenceNumOctaves, octaves);
  descr.Attributes().Set(eTurbulenceStitchable, stitch == SVG_STITCHTYPE_STITCH);
  descr.Attributes().Set(eTurbulenceType, type);
  return descr;
}

bool
SVGFETurbulenceElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                    nsIAtom* aAttribute) const
{
  return SVGFETurbulenceElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::seed ||
           aAttribute == nsGkAtoms::baseFrequency ||
           aAttribute == nsGkAtoms::numOctaves ||
           aAttribute == nsGkAtoms::type ||
           aAttribute == nsGkAtoms::stitchTiles));
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
SVGFETurbulenceElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

nsSVGElement::NumberPairAttributesInfo
SVGFETurbulenceElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                 ArrayLength(sNumberPairInfo));
}

nsSVGElement::IntegerAttributesInfo
SVGFETurbulenceElement::GetIntegerInfo()
{
  return IntegerAttributesInfo(mIntegerAttributes, sIntegerInfo,
                               ArrayLength(sIntegerInfo));
}

nsSVGElement::EnumAttributesInfo
SVGFETurbulenceElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
SVGFETurbulenceElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
