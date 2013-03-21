/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEOffsetElement.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEOffset)

namespace mozilla {
namespace dom {

nsSVGElement::NumberInfo nsSVGFEOffsetElement::sNumberInfo[2] =
{
  { &nsGkAtoms::dx, 0, false },
  { &nsGkAtoms::dy, 0, false }
};

nsSVGElement::StringInfo nsSVGFEOffsetElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEOffsetElement,nsSVGFEOffsetElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEOffsetElement,nsSVGFEOffsetElementBase)

DOMCI_NODE_DATA(SVGFEOffsetElement, nsSVGFEOffsetElement)

NS_INTERFACE_TABLE_HEAD(nsSVGFEOffsetElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEOffsetElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEOffsetElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEOffsetElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEOffsetElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEOffsetElement)


//----------------------------------------------------------------------
// nsIDOMSVGFEOffsetElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP nsSVGFEOffsetElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber dx; */
NS_IMETHODIMP nsSVGFEOffsetElement::GetDx(nsIDOMSVGAnimatedNumber * *aDx)
{
  return mNumberAttributes[DX].ToDOMAnimatedNumber(aDx, this);
}

/* readonly attribute nsIDOMSVGAnimatedNumber dy; */
NS_IMETHODIMP nsSVGFEOffsetElement::GetDy(nsIDOMSVGAnimatedNumber * *aDy)
{
  return mNumberAttributes[DY].ToDOMAnimatedNumber(aDy, this);
}

nsIntPoint
nsSVGFEOffsetElement::GetOffset(const nsSVGFilterInstance& aInstance)
{
  return nsIntPoint(int32_t(aInstance.GetPrimitiveNumber(
                              SVGContentUtils::X, &mNumberAttributes[DX])),
                    int32_t(aInstance.GetPrimitiveNumber(
                              SVGContentUtils::Y, &mNumberAttributes[DY])));
}

nsresult
nsSVGFEOffsetElement::Filter(nsSVGFilterInstance *instance,
                             const nsTArray<const Image*>& aSources,
                             const Image* aTarget,
                             const nsIntRect& rect)
{
  nsIntPoint offset = GetOffset(*instance);

  gfxContext ctx(aTarget->mImage);
  ctx.SetOperator(gfxContext::OPERATOR_SOURCE);
  // Ensure rendering is limited to the filter primitive subregion
  ctx.Clip(aTarget->mFilterPrimitiveSubregion);
  ctx.Translate(gfxPoint(offset.x, offset.y));
  ctx.SetSource(aSources[0]->mImage);
  ctx.Paint();

  return NS_OK;
}

bool
nsSVGFEOffsetElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                nsIAtom* aAttribute) const
{
  return nsSVGFEOffsetElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::dx ||
           aAttribute == nsGkAtoms::dy));
}

void
nsSVGFEOffsetElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

nsIntRect
nsSVGFEOffsetElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  return aSourceBBoxes[0] + GetOffset(aInstance);
}

nsIntRect
nsSVGFEOffsetElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                        const nsSVGFilterInstance& aInstance)
{
  return aSourceChangeBoxes[0] + GetOffset(aInstance);
}

void
nsSVGFEOffsetElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  aSourceBBoxes[0] = aTargetBBox - GetOffset(aInstance);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGFEOffsetElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

nsSVGElement::StringAttributesInfo
nsSVGFEOffsetElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
