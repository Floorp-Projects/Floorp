/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEComponentTransferElement.h"

#include "mozilla/dom/SVGComponentTransferFunctionElement.h"
#include "mozilla/dom/SVGFEComponentTransferElementBinding.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/BindContext.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(FEComponentTransfer)

using namespace mozilla::gfx;
;

namespace mozilla::dom {

JSObject* SVGFEComponentTransferElement::WrapNode(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return SVGFEComponentTransferElement_Binding::Wrap(aCx, this, aGivenProto);
}

SVGElement::StringInfo SVGFEComponentTransferElement::sStringInfo[2] = {
    {nsGkAtoms::result, kNameSpaceID_None, true},
    {nsGkAtoms::in, kNameSpaceID_None, true}};

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEComponentTransferElement)

already_AddRefed<DOMSVGAnimatedString> SVGFEComponentTransferElement::In1() {
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------
// SVGElement methods

SVGElement::StringAttributesInfo
SVGFEComponentTransferElement::GetStringInfo() {
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

//--------------------------------------------

FilterPrimitiveDescription
SVGFEComponentTransferElement::GetPrimitiveDescription(
    SVGFilterInstance* aInstance, const IntRect& aFilterSubregion,
    const nsTArray<bool>& aInputsAreTainted,
    nsTArray<RefPtr<SourceSurface>>& aInputImages) {
  RefPtr<SVGComponentTransferFunctionElement> childForChannel[4];

  for (nsIContent* childContent = nsINode::GetFirstChild(); childContent;
       childContent = childContent->GetNextSibling()) {
    if (auto* child =
            SVGComponentTransferFunctionElement::FromNode(childContent)) {
      childForChannel[child->GetChannel()] = child;
    }
  }

  ComponentTransferAttributes atts;
  for (int32_t i = 0; i < 4; i++) {
    if (childForChannel[i]) {
      childForChannel[i]->ComputeAttributes(i, atts);
    } else {
      atts.mTypes[i] = (uint8_t)SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY;
    }
  }
  return FilterPrimitiveDescription(AsVariant(std::move(atts)));
}

bool SVGFEComponentTransferElement::AttributeAffectsRendering(
    int32_t aNameSpaceID, nsAtom* aAttribute) const {
  return SVGFEComponentTransferElementBase::AttributeAffectsRendering(
             aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::in);
}

void SVGFEComponentTransferElement::GetSourceImageNames(
    nsTArray<SVGStringInfo>& aSources) {
  aSources.AppendElement(SVGStringInfo(&mStringAttributes[IN1], this));
}

nsresult SVGFEComponentTransferElement::BindToTree(BindContext& aCtx,
                                                   nsINode& aParent) {
  if (aCtx.InComposedDoc()) {
    aCtx.OwnerDoc().SetUseCounter(eUseCounter_custom_feComponentTransfer);
  }

  return SVGFEComponentTransferElementBase::BindToTree(aCtx, aParent);
}

}  // namespace mozilla::dom
