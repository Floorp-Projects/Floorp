/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
SVGFEFloodElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGFEFloodElement_Binding::Wrap(aCx, this, aGivenProto);
}

nsSVGElement::StringInfo SVGFEFloodElement::sStringInfo[1] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsINode methods

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
    const nsStyleSVGReset* styleSVGReset = frame->Style()->StyleSVGReset();
    Color color(Color::FromABGR(styleSVGReset->mFloodColor.CalcColor(frame)));
    color.a *= styleSVGReset->mFloodOpacity;
    descr.Attributes().Set(eFloodColor, color);
  } else {
    descr.Attributes().Set(eFloodColor, Color());
  }
  return descr;
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGFEFloodElement::IsAttributeMapped(const nsAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
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
