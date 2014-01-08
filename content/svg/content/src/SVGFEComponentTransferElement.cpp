/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGComponentTransferFunctionElement.h"
#include "mozilla/dom/SVGFEComponentTransferElement.h"
#include "mozilla/dom/SVGFEComponentTransferElementBinding.h"
#include "nsSVGUtils.h"
#include "mozilla/gfx/2D.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEComponentTransfer)

using namespace mozilla::gfx;;

namespace mozilla {
namespace dom {

JSObject*
SVGFEComponentTransferElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEComponentTransferElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::StringInfo SVGFEComponentTransferElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEComponentTransferElement)

already_AddRefed<SVGAnimatedString>
SVGFEComponentTransferElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
SVGFEComponentTransferElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

//--------------------------------------------

FilterPrimitiveDescription
SVGFEComponentTransferElement::GetPrimitiveDescription(nsSVGFilterInstance* aInstance,
                                                       const IntRect& aFilterSubregion,
                                                       const nsTArray<bool>& aInputsAreTainted,
                                                       nsTArray<RefPtr<SourceSurface>>& aInputImages)
{
  nsRefPtr<SVGComponentTransferFunctionElement> childForChannel[4];

  for (nsIContent* childContent = nsINode::GetFirstChild();
       childContent;
       childContent = childContent->GetNextSibling()) {

    nsRefPtr<SVGComponentTransferFunctionElement> child;
    CallQueryInterface(childContent,
            (SVGComponentTransferFunctionElement**)getter_AddRefs(child));
    if (child) {
      childForChannel[child->GetChannel()] = child;
    }
  }

  static const AttributeName attributeNames[4] = {
    eComponentTransferFunctionR,
    eComponentTransferFunctionG,
    eComponentTransferFunctionB,
    eComponentTransferFunctionA
  };

  FilterPrimitiveDescription descr(FilterPrimitiveDescription::eComponentTransfer);
  for (int32_t i = 0; i < 4; i++) {
    if (childForChannel[i]) {
      descr.Attributes().Set(attributeNames[i], childForChannel[i]->ComputeAttributes());
    } else {
      AttributeMap functionAttributes;
      functionAttributes.Set(eComponentTransferFunctionType,
                             (uint32_t)SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY);
      descr.Attributes().Set(attributeNames[i], functionAttributes);
    }
  }
  return descr;
}

bool
SVGFEComponentTransferElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                         nsIAtom* aAttribute) const
{
  return SVGFEComponentTransferElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          aAttribute == nsGkAtoms::in);
}

void
SVGFEComponentTransferElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

} // namespace dom
} // namespace mozilla
