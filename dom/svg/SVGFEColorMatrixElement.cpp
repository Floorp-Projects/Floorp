/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSVGAnimatedNumberList.h"
#include "mozilla/dom/SVGFEColorMatrixElement.h"
#include "mozilla/dom/SVGFEColorMatrixElementBinding.h"
#include "nsSVGUtils.h"

#define NUM_ENTRIES_IN_4x5_MATRIX 20

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEColorMatrix)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject*
SVGFEColorMatrixElement::WrapNode(JSContext* aCx)
{
  return SVGFEColorMatrixElementBinding::Wrap(aCx, this);
}

nsSVGEnumMapping SVGFEColorMatrixElement::sTypeMap[] = {
  {&nsGkAtoms::matrix, SVG_FECOLORMATRIX_TYPE_MATRIX},
  {&nsGkAtoms::saturate, SVG_FECOLORMATRIX_TYPE_SATURATE},
  {&nsGkAtoms::hueRotate, SVG_FECOLORMATRIX_TYPE_HUE_ROTATE},
  {&nsGkAtoms::luminanceToAlpha, SVG_FECOLORMATRIX_TYPE_LUMINANCE_TO_ALPHA},
  {nullptr, 0}
};

nsSVGElement::EnumInfo SVGFEColorMatrixElement::sEnumInfo[1] =
{
  { &nsGkAtoms::type,
    sTypeMap,
    SVG_FECOLORMATRIX_TYPE_MATRIX
  }
};

nsSVGElement::StringInfo SVGFEColorMatrixElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

nsSVGElement::NumberListInfo SVGFEColorMatrixElement::sNumberListInfo[1] =
{
  { &nsGkAtoms::values }
};

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEColorMatrixElement)


//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedString>
SVGFEColorMatrixElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<SVGAnimatedEnumeration>
SVGFEColorMatrixElement::Type()
{
  return mEnumAttributes[TYPE].ToDOMAnimatedEnum(this);
}

already_AddRefed<DOMSVGAnimatedNumberList>
SVGFEColorMatrixElement::Values()
{
  return DOMSVGAnimatedNumberList::GetDOMWrapper(&mNumberListAttributes[VALUES],
                                                 this, VALUES);
}

void
SVGFEColorMatrixElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

FilterPrimitiveDescription
SVGFEColorMatrixElement::GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                                                 const IntRect& aFilterSubregion,
                                                 const nsTArray<bool>& aInputsAreTainted,
                                                 nsTArray<RefPtr<SourceSurface>>& aInputImages)
{
  uint32_t type = mEnumAttributes[TYPE].GetAnimValue();
  const SVGNumberList &values = mNumberListAttributes[VALUES].GetAnimValue();

  FilterPrimitiveDescription descr(PrimitiveType::ColorMatrix);
  if (!mNumberListAttributes[VALUES].IsExplicitlySet() &&
      (type == SVG_FECOLORMATRIX_TYPE_MATRIX ||
       type == SVG_FECOLORMATRIX_TYPE_SATURATE ||
       type == SVG_FECOLORMATRIX_TYPE_HUE_ROTATE)) {
    descr.Attributes().Set(eColorMatrixType, (uint32_t)SVG_FECOLORMATRIX_TYPE_MATRIX);
    static const float identityMatrix[] = 
      { 1, 0, 0, 0, 0,
        0, 1, 0, 0, 0,
        0, 0, 1, 0, 0,
        0, 0, 0, 1, 0 };
    descr.Attributes().Set(eColorMatrixValues, identityMatrix, 20);
  } else {
    descr.Attributes().Set(eColorMatrixType, type);
    if (values.Length()) {
      descr.Attributes().Set(eColorMatrixValues, &values[0], values.Length());
    } else {
      descr.Attributes().Set(eColorMatrixValues, nullptr, 0);
    }
  }
  return descr;
}

bool
SVGFEColorMatrixElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                   nsIAtom* aAttribute) const
{
  return SVGFEColorMatrixElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::type ||
           aAttribute == nsGkAtoms::values));
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::EnumAttributesInfo
SVGFEColorMatrixElement::GetEnumInfo()
{
  return EnumAttributesInfo(mEnumAttributes, sEnumInfo,
                            ArrayLength(sEnumInfo));
}

nsSVGElement::StringAttributesInfo
SVGFEColorMatrixElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

nsSVGElement::NumberListAttributesInfo
SVGFEColorMatrixElement::GetNumberListInfo()
{
  return NumberListAttributesInfo(mNumberListAttributes, sNumberListInfo,
                                  ArrayLength(sNumberListInfo));
}

} // namespace dom
} // namespace mozilla
