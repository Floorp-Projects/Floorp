/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGFEMergeNodeElement.h"
#include "mozilla/dom/SVGFEMergeNodeElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(FEMergeNode)

namespace mozilla {
namespace dom {

JSObject*
SVGFEMergeNodeElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGFEMergeNodeElement_Binding::Wrap(aCx, this, aGivenProto);
}

nsSVGElement::StringInfo SVGFEMergeNodeElement::sStringInfo[1] =
{
  { &nsGkAtoms::in, kNameSpaceID_None, true }
};

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGFEMergeNodeElement)

//----------------------------------------------------------------------
// nsFEUnstyledElement methods

bool
SVGFEMergeNodeElement::AttributeAffectsRendering(int32_t aNameSpaceID,
                                                 nsAtom* aAttribute) const
{
  return aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::in;
}

already_AddRefed<SVGAnimatedString>
SVGFEMergeNodeElement::In1()
{
  return mStringAttributes[IN1].ToDOMAnimatedString(this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::StringAttributesInfo
SVGFEMergeNodeElement::GetStringInfo()
{
  return StringAttributesInfo(mStringAttributes, sStringInfo,
                              ArrayLength(sStringInfo));
}

} // namespace dom
} // namespace mozilla
