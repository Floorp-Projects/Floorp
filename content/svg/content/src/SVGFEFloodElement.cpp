/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEFloodElement.h"
#include "mozilla/dom/SVGFEFloodElementBinding.h"
#include "gfxContext.h"
#include "gfxColor.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEFlood)

namespace mozilla {
namespace dom {

JSObject*
SVGFEFloodElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEFloodElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::StringInfo SVGFEFloodElement::sStringInfo[1] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEFloodElement)

nsresult
SVGFEFloodElement::Filter(nsSVGFilterInstance *instance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& aDataRect)
{
  nsIFrame* frame = GetPrimaryFrame();
  if (!frame) return NS_ERROR_FAILURE;
  nsStyleContext* style = frame->StyleContext();

  nscolor floodColor = style->StyleSVGReset()->mFloodColor;
  float floodOpacity = style->StyleSVGReset()->mFloodOpacity;

  gfxContext ctx(aTarget->mImage);
  ctx.SetColor(gfxRGBA(NS_GET_R(floodColor) / 255.0,
                       NS_GET_G(floodColor) / 255.0,
                       NS_GET_B(floodColor) / 255.0,
                       NS_GET_A(floodColor) / 255.0 * floodOpacity));
  ctx.Rectangle(aTarget->mFilterPrimitiveSubregion);
  ctx.Fill();
  return NS_OK;
}

nsIntRect
SVGFEFloodElement::ComputeTargetBBox(const nsTArray<nsIntRect>& aSourceBBoxes,
                                     const nsSVGFilterInstance& aInstance)
{
  return GetMaxRect();
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGFEFloodElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sFEFloodMap
  };

  return FindAttributeDependence(name, map) ||
    SVGFEFloodElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
SVGFEFloodElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
