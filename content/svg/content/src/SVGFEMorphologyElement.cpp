/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEMorphologyElement.h"

namespace mozilla {
namespace dom {

nsSVGElement::NumberPairInfo nsSVGFEMorphologyElement::sNumberPairInfo[1] =
{
  { &nsGkAtoms::radius, 0, 0 }
};

nsSVGEnumMapping nsSVGFEMorphologyElement::sOperatorMap[] = {
  {&nsGkAtoms::erode, nsSVGFEMorphologyElement::SVG_OPERATOR_ERODE},
  {&nsGkAtoms::dilate, nsSVGFEMorphologyElement::SVG_OPERATOR_DILATE},
  {nullptr, 0}
};

nsSVGElement::EnumInfo nsSVGFEMorphologyElement::sEnumInfo[1] =
{
  { &nsGkAtoms::_operator,
    sOperatorMap,
    nsSVGFEMorphologyElement::SVG_OPERATOR_ERODE
  }
};

nsSVGElement::StringInfo nsSVGFEMorphologyElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEMorphology)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEMorphologyElement,nsSVGFEMorphologyElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEMorphologyElement,nsSVGFEMorphologyElementBase)

DOMCI_NODE_DATA(SVGFEMorphologyElement, nsSVGFEMorphologyElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEMorphologyElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEMorphologyElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEMorphologyElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEMorphologyElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEMorphologyElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEMorphologyElement)


//----------------------------------------------------------------------
// nsSVGFEMorphologyElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEMorphologyElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration operator; */
NS_IMETHODIMP nsSVGFEMorphologyElement::GetOperator(nsIDOMSVGAnimatedEnumeration * *aOperator)
{
  return mEnumAttributes[OPERATOR].ToDOMAnimatedEnum(aOperator, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber radiusX; */
NS_IMETHODIMP nsSVGFEMorphologyElement::GetRadiusX(nsIDOMSVGAnimatedNumber * *aX)
{
  return mNumberPairAttributes[RADIUS].ToDOMAnimatedNumber(aX, nsSVGNumberPair::eFirst, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber radiusY; */
NS_IMETHODIMP nsSVGFEMorphologyElement::GetRadiusY(nsIDOMSVGAnimatedNumber * *aY)
{
  return mNumberPairAttributes[RADIUS].ToDOMAnimatedNumber(aY, nsSVGNumberPair::eSecond, this);
}

NS_IMETHODIMP
nsSVGFEMorphologyElement::SetRadius(float rx, float ry)
{
  NS_ENSURE_FINITE2(rx, ry, NS_ERROR_ILLEGAL_VALUE);
  mNumberPairAttributes[RADIUS].SetBaseValues(rx, ry, this);
  return NS_OK;
}

void
nsSVGFEMorphologyElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

nsIntRect
nsSVGFEMorphologyElement::InflateRect(const nsIntRect& aRect,
                                      const nsSVGFilterInstance& aInstance)
{
  int32_t rx, ry;
  GetRXY(&rx, &ry, aInstance);
  nsIntRect result = aRect;
  result.Inflate(std::max(0, rx), std::max(0, ry));
  return result;
}

nsIntRect
nsSVGFEMorphologyElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  return InflateRect(aSourceBBoxes[0], aInstance);
}

void
nsSVGFEMorphologyElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  aSourceBBoxes[0] = InflateRect(aTargetBBox, aInstance);
}

nsIntRect
nsSVGFEMorphologyElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                            const nsSVGFilterInstance& aInstance)
{
  return InflateRect(aSourceChangeBoxes[0], aInstance);
}

#define MORPHOLOGY_EPSILON 0.0001

void
nsSVGFEMorphologyElement::GetRXY(int32_t *aRX, int32_t *aRY,
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

nsresult
nsSVGFEMorphologyElement::Filter(nsSVGFilterInstance *instance,
                                 const nsTArray<const Image*>& aSources,
                                 const Image* aTarget,
                                 const nsIntRect& rect)
{
  int32_t rx, ry;
  GetRXY(&rx, &ry, *instance);

  if (rx < 0 || ry < 0) {
    // XXX SVGContentUtils::ReportToConsole()
    return NS_OK;
  }
  if (rx == 0 && ry == 0) {
    return NS_OK;
  }

  // Clamp radii to prevent completely insane values:
  rx = std::min(rx, 100000);
  ry = std::min(ry, 100000);

  uint8_t* sourceData = aSources[0]->mImage->Data();
  uint8_t* targetData = aTarget->mImage->Data();
  int32_t stride = aTarget->mImage->Stride();
  uint8_t extrema[4];         // RGBA magnitude of extrema
  uint16_t op = mEnumAttributes[OPERATOR].GetAnimValue();

  // Scan the kernel for each pixel to determine max/min RGBA values.
  for (int32_t y = rect.y; y < rect.YMost(); y++) {
    int32_t startY = std::max(0, y - ry);
    // We need to read pixels not just in 'rect', which is limited to
    // the dirty part of our filter primitive subregion, but all pixels in
    // the given radii from the source surface, so use the surface size here.
    int32_t endY = std::min(y + ry, instance->GetSurfaceHeight() - 1);
    for (int32_t x = rect.x; x < rect.XMost(); x++) {
      int32_t startX = std::max(0, x - rx);
      int32_t endX = std::min(x + rx, instance->GetSurfaceWidth() - 1);
      int32_t targIndex = y * stride + 4 * x;

      for (int32_t i = 0; i < 4; i++) {
        extrema[i] = sourceData[targIndex + i];
      }
      for (int32_t y1 = startY; y1 <= endY; y1++) {
        for (int32_t x1 = startX; x1 <= endX; x1++) {
          for (int32_t i = 0; i < 4; i++) {
            uint8_t pixel = sourceData[y1 * stride + 4 * x1 + i];
            if ((extrema[i] > pixel &&
                 op == nsSVGFEMorphologyElement::SVG_OPERATOR_ERODE) ||
                (extrema[i] < pixel &&
                 op == nsSVGFEMorphologyElement::SVG_OPERATOR_DILATE)) {
              extrema[i] = pixel;
            }
          }
        }
      }
      targetData[targIndex  ] = extrema[0];
      targetData[targIndex+1] = extrema[1];
      targetData[targIndex+2] = extrema[2];
      targetData[targIndex+3] = extrema[3];
    }
  }
  return NS_OK;
}

bool
nsSVGFEMorphologyElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                    nsIAtom* aAttribute) const
{
  return nsSVGFEMorphologyElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::radius ||
           aAttribute == nsGkAtoms::_operator));
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberPairAttributesInfo
nsSVGFEMorphologyElement::GetNumberPairInfo()
{
  return NumberPairAttributesInfo(mNumberPairAttributes, sNumberPairInfo,
                                  ArrayLength(sNumberPairInfo));
}

nsSVGElement::EnumAttributesInfo
nsSVGFEMorphologyElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGFEMorphologyElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
