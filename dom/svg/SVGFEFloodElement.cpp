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
#include "mozilla/dom/Document.h"
#include "mozilla/dom/BindContext.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FEFlood)

using namespace mozilla::gfx;

namespace mozilla::dom {

JSObject* SVGFEFloodElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return SVGFEFloodElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::StringInfo SVGFEFloodElement::sStringInfo[1] = {
    {nsGkAtoms::result, kNameSpaceID_None, true}};

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEFloodElement)

FilterPrimitiveDescription SVGFEFloodElement::GetPrimitiveDescription(
    SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
    const nsTArray<bool>& aInputsAreTainted,
    nsTArray<RefPtr<SourceSurface>>& aInputImages) {
  FloodAttributes atts;
  if (const auto* frame = GetPrimaryFrame()) {
    const nsStyleSVGReset* styleSVGReset = frame->Style()->StyleSVGReset();
    sRGBColor color(
        sRGBColor::FromABGR(styleSVGReset->mFloodColor.CalcColor(frame)));
    color.a *= styleSVGReset->mFloodOpacity;
    atts.mColor = color;
  } else {
    atts.mColor = sRGBColor();
  }
  return FilterPrimitiveDescription(AsVariant(std::move(atts)));
}

bool SVGFEFloodElement::OutputIsTainted(const nsTArray<bool>& aInputsAreTainted,
                                        nsIPrincipal* aReferencePrincipal) {
  if (const auto* frame = GetPrimaryFrame()) {
    if (frame->Style()->StyleSVGReset()->mFloodColor.IsCurrentColor()) {
      return true;
    }
  }

  return SVGFEFloodElementBase::OutputIsTainted(aInputsAreTainted,
                                                aReferencePrincipal);
}

//----------------------------------------------------------------------
// nsIContent methods

nsresult SVGFEFloodElement::BindToTree(BindContext& aCtx, nsINode& aParent) {
  if (aCtx.InComposedDoc()) {
    aCtx.OwnerDoc().SetUseCounter(eUseCounter_custom_feFlood);
  }

  return SVGFEFloodElementBase::BindToTree(aCtx, aParent);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::StringAttributesInfo SVGFEFloodElement::GetStringInfo() {
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

}  // namespace mozilla::dom
