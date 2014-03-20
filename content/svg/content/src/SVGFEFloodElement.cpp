/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEFloodElement.h"

#include "FilterSupport.h"
#include "mozilla/dom/SVGFEFloodElementBinding.h"
#include "nsColor.h"
#include "nsIFrame.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEFlood)

using namespace mozilla::gfx;

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

FilterPrimitiveDescription
SVGFEFloodElement::GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                                           const IntRect& aFilterSubregion,
                                           const nsTArray<bool>& aInputsAreTainted,
                                           nsTArray<RefPtr<SourceSurface>>& aInputImages)
{
  FilterPrimitiveDescription descr(PrimitiveType::Flood);
  nsIFrame* frame = GetPrimaryFrame();
  if (frame) {
    nsStyleContext* style = frame->StyleContext();
    nscolor floodColor = style->StyleSVGReset()->mFloodColor;
    float floodOpacity = style->StyleSVGReset()->mFloodOpacity;
    Color color(NS_GET_R(floodColor) / 255.0,
                NS_GET_G(floodColor) / 255.0,
                NS_GET_B(floodColor) / 255.0,
                NS_GET_A(floodColor) / 255.0 * floodOpacity);
    descr.Attributes().Set(eFloodColor, color);
  } else {
    descr.Attributes().Set(eFloodColor, Color());
  }
  return descr;
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
