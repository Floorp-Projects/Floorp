/* a*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEComponentTransferElement.h"

DOMCI_NODE_DATA(SVGFEComponentTransferElement, nsSVGFEComponentTransferElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEComponentTransfer)

namespace mozilla {
namespace dom {

nsSVGElement::StringInfo nsSVGFEComponentTransferElement::sStringInfo[2] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true },
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGFEComponentTransferElement,nsSVGFEComponentTransferElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGFEComponentTransferElement,nsSVGFEComponentTransferElementBase)

NS_INTERFACE_TABLE_HEAD(nsSVGFEComponentTransferElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGFEComponentTransferElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGFilterPrimitiveStandardAttributes,
                           nsIDOMSVGFEComponentTransferElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGFEComponentTransferElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGFEComponentTransferElementBase)

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGFEComponentTransferElement)

//----------------------------------------------------------------------
// nsIDOMSVGFEComponentTransferElement methods

/* readonly attribute nsIDOMSVGAnimatedString in1; */
NS_IMETHODIMP
nsSVGFEComponentTransferElement::GetIn1(nsIDOMSVGAnimatedString * *aIn)
{
  return mStringAttributes[IN1].ToDOMAnimatedString(aIn, this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
nsSVGFEComponentTransferElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

//--------------------------------------------

nsresult
nsSVGFEComponentTransferElement::Filter(nsSVGFilterInstance *instance,
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
nsSVGFEComponentTransferElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                           nsIAtom* aAttribute) const
{
  return nsSVGFEComponentTransferElementBase::AttributeAffectsRendering(aNameSpaceID, aAttribute) ||
         (aNameSpaceID == kNameSpaceID_None &&
          aAttribute == nsGkAtoms::in);
}

void
nsSVGFEComponentTransferElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  aSources.AppendElement(nsSVGStringInfo(&mStringAttributes[IN1], this));
}

} // namespace dom
} // namespace mozilla
