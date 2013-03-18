/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEColorMatrixElement.h"

namespace mozilla {
namespace dom {

nsSVGEnumMapping nsSVGFEColorMatrixElement::sTypeMap[] = {
  {&nsGkAtoms::matrix, nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_MATRIX},
  {&nsGkAtoms::saturate, nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_SATURATE},
  {&nsGkAtoms::hueRotate, nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_HUE_ROTATE},
  {&nsGkAtoms::luminanceToAlpha, nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_LUMINANCE_TO_ALPHA},
  {nullptr, 0}
};

nsSVGElement::EnumInfo nsSVGFEColorMatrixElement::sEnumInfo[1] =
{
  { &nsGkAtoms::type,
    sTypeMap,
    nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_MATRIX
  }
};

nsSVGElement::StringInfo nsSVGFEColorMatrixElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

nsSVGElement::NumberListInfo nsSVGFEColorMatrixElement::sNumberListInfo[1] =
{
  { &nsGkAtoms::values }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(FEColorMatrix)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEColorMatrixElement,nsSVGFEColorMatrixElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEColorMatrixElement,nsSVGFEColorMatrixElementBase)

DOMCI_NODE_DATA(SVGFEColorMatrixElement, nsSVGFEColorMatrixElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEColorMatrixElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEColorMatrixElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEColorMatrixElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEColorMatrixElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEColorMatrixElementBase)


//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEColorMatrixElement)


//----------------------------------------------------------------------
// nsSVGFEColorMatrixElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEColorMatrixElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

/* readonly attribute nsIDOMSVGAnimatedEnumeration type; */
NS_IMETHODIMP nsSVGFEColorMatrixElement::GetType(nsIDOMSVGAnimatedEnumeration * *aType)
{
  return mEnumAttributes[TYPE].ToDOMAnimatedEnum(aType, this);
}

/* readonly attribute DOMSVGAnimatedNumberList values; */
NS_IMETHODIMP nsSVGFEColorMatrixElement::GetValues(nsISupports * *aValues)
{
  *aValues = DOMSVGAnimatedNumberList::GetDOMWrapper(&mNumberListAttributes[VALUES],
                                                     this, VALUES).get();
  return NS_OK;
}

void
nsSVGFEColorMatrixElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

nsresult
nsSVGFEColorMatrixElement::Filter(nsSVGFilterInstance *instance,
                                  const nsTArray<const Image*>& aSources,
                                  const Image* aTarget,
                                  const nsIntRect& rect)
{
  uint8_t* sourceData = aSources[0]->mImage->Data();
  uint8_t* targetData = aTarget->mImage->Data();
  uint32_t stride = aTarget->mImage->Stride();

  uint16_t type = mEnumAttributes[TYPE].GetAnimValue();
  const SVGNumberList &values = mNumberListAttributes[VALUES].GetAnimValue();

  if (!mNumberListAttributes[VALUES].IsExplicitlySet() &&
      (type == nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_MATRIX ||
       type == nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_SATURATE ||
       type == nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_HUE_ROTATE)) {
    // identity matrix filter
    CopyRect(aTarget, aSources[0], rect);
    return NS_OK;
  }

  static const float identityMatrix[] = 
    { 1, 0, 0, 0, 0,
      0, 1, 0, 0, 0,
      0, 0, 1, 0, 0,
      0, 0, 0, 1, 0 };

  static const float luminanceToAlphaMatrix[] = 
    { 0,       0,       0,       0, 0,
      0,       0,       0,       0, 0,
      0,       0,       0,       0, 0,
      0.2125f, 0.7154f, 0.0721f, 0, 0 };

  float colorMatrix[NUM_ENTRIES_IN_4x5_MATRIX];
  float s, c;

  switch (type) {
  case nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_MATRIX:

    if (values.Length() != NUM_ENTRIES_IN_4x5_MATRIX)
      return NS_ERROR_FAILURE;

    for(uint32_t j = 0; j < values.Length(); j++) {
      colorMatrix[j] = values[j];
    }
    break;
  case nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_SATURATE:

    if (values.Length() != 1)
      return NS_ERROR_FAILURE;

    s = values[0];

    if (s > 1 || s < 0)
      return NS_ERROR_FAILURE;

    memcpy(colorMatrix, identityMatrix, sizeof(colorMatrix));

    colorMatrix[0] = 0.213f + 0.787f * s;
    colorMatrix[1] = 0.715f - 0.715f * s;
    colorMatrix[2] = 0.072f - 0.072f * s;

    colorMatrix[5] = 0.213f - 0.213f * s;
    colorMatrix[6] = 0.715f + 0.285f * s;
    colorMatrix[7] = 0.072f - 0.072f * s;

    colorMatrix[10] = 0.213f - 0.213f * s;
    colorMatrix[11] = 0.715f - 0.715f * s;
    colorMatrix[12] = 0.072f + 0.928f * s;

    break;

  case nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_HUE_ROTATE:
  {
    memcpy(colorMatrix, identityMatrix, sizeof(colorMatrix));

    if (values.Length() != 1)
      return NS_ERROR_FAILURE;

    float hueRotateValue = values[0];

    c = static_cast<float>(cos(hueRotateValue * M_PI / 180));
    s = static_cast<float>(sin(hueRotateValue * M_PI / 180));

    memcpy(colorMatrix, identityMatrix, sizeof(colorMatrix));

    colorMatrix[0] = 0.213f + 0.787f * c - 0.213f * s;
    colorMatrix[1] = 0.715f - 0.715f * c - 0.715f * s;
    colorMatrix[2] = 0.072f - 0.072f * c + 0.928f * s;

    colorMatrix[5] = 0.213f - 0.213f * c + 0.143f * s;
    colorMatrix[6] = 0.715f + 0.285f * c + 0.140f * s;
    colorMatrix[7] = 0.072f - 0.072f * c - 0.283f * s;

    colorMatrix[10] = 0.213f - 0.213f * c - 0.787f * s;
    colorMatrix[11] = 0.715f - 0.715f * c + 0.715f * s;
    colorMatrix[12] = 0.072f + 0.928f * c + 0.072f * s;

    break;
  }

  case nsSVGFEColorMatrixElement::SVG_FECOLORMATRIX_TYPE_LUMINANCE_TO_ALPHA:

    memcpy(colorMatrix, luminanceToAlphaMatrix, sizeof(colorMatrix));
    break;

  default:
    return NS_ERROR_FAILURE;
  }

  for (int32_t x = rect.x; x < rect.XMost(); x++) {
    for (int32_t y = rect.y; y < rect.YMost(); y++) {
      uint32_t targIndex = y * stride + 4 * x;

      float col[4];
      for (int i = 0, row = 0; i < 4; i++, row += 5) {
        col[i] =
          sourceData[targIndex + GFX_ARGB32_OFFSET_R] * colorMatrix[row + 0] +
          sourceData[targIndex + GFX_ARGB32_OFFSET_G] * colorMatrix[row + 1] +
          sourceData[targIndex + GFX_ARGB32_OFFSET_B] * colorMatrix[row + 2] +
          sourceData[targIndex + GFX_ARGB32_OFFSET_A] * colorMatrix[row + 3] +
          255 *                                         colorMatrix[row + 4];
        col[i] = clamped(col[i], 0.f, 255.f);
      }
      targetData[targIndex + GFX_ARGB32_OFFSET_R] =
        static_cast<uint8_t>(col[0]);
      targetData[targIndex + GFX_ARGB32_OFFSET_G] =
        static_cast<uint8_t>(col[1]);
      targetData[targIndex + GFX_ARGB32_OFFSET_B] =
        static_cast<uint8_t>(col[2]);
      targetData[targIndex + GFX_ARGB32_OFFSET_A] =
        static_cast<uint8_t>(col[3]);
    }
  }
  return NS_OK;
}

bool
nsSVGFEColorMatrixElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                     nsIAtom* aAttribute) const
{
  return nsSVGFEColorMatrixElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::type ||
           aAttribute == nsGkAtoms::values));
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::EnumAttributesInfo
nsSVGFEColorMatrixElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGFEColorMatrixElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

nsSVGElement::NumberListAttributesInfo
nsSVGFEColorMatrixElement::GetNumberListInfo()
{
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  ArrayLength(sNumberListInfo));
}

} // namespace dom
} // namespace mozilla
