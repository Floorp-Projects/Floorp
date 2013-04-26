/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEMergeElement.h"
#include "mozilla/dom/SVGFEMergeElementBinding.h"
#include "mozilla/dom/SVGFEMergeNodeElement.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEMerge)

namespace mozilla {
namespace dom {

JSObject*
SVGFEMergeElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return SVGFEMergeElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::StringInfo SVGFEMergeElement::sStringInfo[1] =
{
  { &nsGkAtoms::result, kNameSpaceID_None, true }
};

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEMergeElement)

nsresult
SVGFEMergeElement::Filter(nsSVGFilterInstance *instance,
                          const nsTArray<const Image*>& aSources,
                          const Image* aTarget,
                          const nsIntRect& rect)
{
  gfxContext ctx(aTarget->mImage);
  ctx.Clip(aTarget->mFilterPrimitiveSubregion);

  for (uint32_t i = 0; i < aSources.Length(); i++) {
    ctx.SetSource(aSources[i]->mImage);
    ctx.Paint();
  }
  return NS_OK;
}

void
SVGFEMergeElement::GetSourceImageNames(nsTArray<nsSVGStringInfo>& aSources)
{
  for (nsIContent* child = nsINode::GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->IsSVG(nsGkAtoms::feMergeNode)) {
      SVGFEMergeNodeElement* node = static_cast<SVGFEMergeNodeElement*>(child);
      aSources.AppendElement(nsSVGStringInfo(node->GetIn1(), node));
    }
  }
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
SVGFEMergeElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
