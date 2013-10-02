/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEOffsetElement.h"
#include "mozilla/dom/SVGFEOffsetElementBinding.h"
#include "nsSVGFilterInstance.h"
#include "gfxContext.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEOffset)

namespace mozilla {
namespace dom {

JSObject*
SVGFEOffsetElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEOffsetElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::NumberInfo SVGFEOffsetElement::sNumberInfo[2] =
{
  { &nsGkAtoms::dx, 0, false },
  { &nsGkAtoms::dy, 0, false }
};

nsSVGElement::StringInfo SVGFEOffsetElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEOffsetElement)


//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedString>
SVGFEOffsetElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEOffsetElement::Dx()
{
  return mNumberAttributes[DX].ToDOMAnimatedNumber(this);
}

already_AddRefed<SVGAnimatedNumber>
SVGFEOffsetElement::Dy()
{
  return mNumberAttributes[DY].ToDOMAnimatedNumber(this);
}

nsIntPoint
SVGFEOffsetElement::GetOffset(const nsSVGFilterInstance& aInstance)
{
  return nsIntPoint(int32_t(aInstance.GetPrimitiveNumber(
                              SVGContentUtils::X, &mNumberAttributes[DX])),
                    int32_t(aInstance.GetPrimitiveNumber(
                              SVGContentUtils::Y, &mNumberAttributes[DY])));
}

nsresult
SVGFEOffsetElement::Filter(nsSVGFilterInstance* instance,
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
SVGFEOffsetElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                              nsIAtom* aAttribute) const
{
  return SVGFEOffsetElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          (aAttribute == nsGkAtoms::in ||
           aAttribute == nsGkAtoms::dx ||
           aAttribute == nsGkAtoms::dy));
}

void
SVGFEOffsetElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

nsIntRect
SVGFEOffsetElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
        const nsSVGFilterInstance& aInstance)
{
  return aSourceBBoxes[0] + GetOffset(aInstance);
}

nsIntRect
SVGFEOffsetElement::ComputeChangeBBox(const nsTArray<nsIntRect>& aSourceChangeBoxes,
                                      const nsSVGFilterInstance& aInstance)
{
  return aSourceChangeBoxes[0] + GetOffset(aInstance);
}

void
SVGFEOffsetElement::ComputeNeededSourceBBoxes(const nsIntRect& aTargetBBox,
          nsTArray<nsIntRect>& aSourceBBoxes, const nsSVGFilterInstance& aInstance)
{
  aSourceBBoxes[0] = aTargetBBox - GetOffset(aInstance);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
SVGFEOffsetElement::GetNumberInfo()
{
  return NumberAttributesInfo(mNumberAttributes, sNumberInfo,
                              ArrayLength(sNumberInfo));
}

nsSVGElement::StringAttributesInfo
SVGFEOffsetElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
