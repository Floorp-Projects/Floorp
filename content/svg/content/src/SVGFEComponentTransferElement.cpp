/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGComponentTransferFunctionElement.h"
#include "mozilla/dom/SVGFEComponentTransferElement.h"
#include "mozilla/dom/SVGFEComponentTransferElementBinding.h"
#include "nsSVGUtils.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEComponentTransfer)

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

already_AddRefed<nsIDOMSVGAnimatedString>
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

nsresult
SVGFEComponentTransferElement::Filter(nsSVGFilterInstance *instance,
                                      const nsTArray<const Image*>& aSources,
                                      const Image* aTarget,
                                      const nsIntRect& rect)
{
  uint8_t* sourceData = aSources[0]->mImage->Data();
  uint8_t* targetData = aTarget->mImage->Data();
  uint32_t stride = aTarget->mImage->Stride();

  uint8_t tableR[256], tableG[256], tableB[256], tableA[256];
  for (int i=0; i<256; i++)
    tableR[i] = tableG[i] = tableB[i] = tableA[i] = i;
  uint8_t* tables[] = { tableR, tableG, tableB, tableA };
  for (nsIContent* childContent = nsINode::GetFirstChild();
       childContent;
       childContent = childContent->GetNextSibling()) {

    nsRefPtr<SVGComponentTransferFunctionElement> child;
    CallQueryInterface(childContent,
            (SVGComponentTransferFunctionElement**)getter_AddRefs(child));
    if (child) {
      if (!child->GenerateLookupTable(tables[child->GetChannel()])) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  for (int32_t y = rect.y; y < rect.YMost(); y++) {
    for (int32_t x = rect.x; x < rect.XMost(); x++) {
      int32_t targIndex = y * stride + x * 4;
      targetData[targIndex + GFX_ARGB32_OFFSET_B] =
        tableB[sourceData[targIndex + GFX_ARGB32_OFFSET_B]];
      targetData[targIndex + GFX_ARGB32_OFFSET_G] =
        tableG[sourceData[targIndex + GFX_ARGB32_OFFSET_G]];
      targetData[targIndex + GFX_ARGB32_OFFSET_R] =
        tableR[sourceData[targIndex + GFX_ARGB32_OFFSET_R]];
      targetData[targIndex + GFX_ARGB32_OFFSET_A] =
        tableA[sourceData[targIndex + GFX_ARGB32_OFFSET_A]];
    }
  }
  return NS_OK;
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
