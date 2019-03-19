/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFETileElement.h"
#include "mozilla/dom/SVGFETileElementBinding.h"
#include "nsSVGFilterInstance.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FETile)

using namespace mozilla::gfx;

namespace mozilla {
namespace dom {

JSObject* SVGFETileElement::WrapNode(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return SVGFETileElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::StringInfo SVGFETileElement::sStringInfo[2] = {
    {nsGkAtoms::result, kNameSpaceID_None, true},
    {nsGkAtoms::in, kNameSpaceID_None, true}};

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFETileElement)

already_AddRefed<DOMSVGAnimatedString> SVGFETileElement::In1() {
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

void SVGFETileElement::GetSourceImageNames(nsTArray<SVGStringInfo>& aSources) {
  aSources.AppendElement(SVGStringInfo(&mStringAttributes[IN1], this));
}

//----------------------------------------------------------------------
// SVGElement methods

FilterPrimitiveDescription SVGFETileElement::GetPrimitiveDescription(
    nsSVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
    const nsTArray<bool>& aInputsAreTainted,
    nsTArray<RefPtr<SourceSurface>>& aInputImages) {
  return FilterPrimitiveDescription(AsVariant(TileAttributes()));
}

bool SVGFETileElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                 nsAtom* aAttribute) const {
  return SVGFETileElementBase::AttributeAffectsRendering(aNameSpaceID,
                                                         aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::in);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::StringAttributesInfo SVGFETileElement::GetStringInfo() {
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

}  // namespace dom
}  // namespace mozilla
