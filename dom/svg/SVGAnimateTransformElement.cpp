/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGAnimateTransformElement.h"
#include "mozilla/dom/SVGAnimateTransformElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(AnimateTransform)

namespace mozilla {
namespace dom {

JSObject*
SVGAnimateTransformElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGAnimateTransformElementBinding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
// Implementation

SVGAnimateTransformElement::SVGAnimateTransformElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGAnimationElement(aNodeInfo)
{
}

bool
SVGAnimateTransformElement::ParseAttribute(int32_t aNamespaceID,
                                           nsAtom* aAttribute,
                                           const nsAString& aValue,
                                           nsIPrincipal* aMaybeScriptedPrincipal,
                                           nsAttrValue& aResult)
{
  // 'type' is an <animateTransform>-specific attribute, and we'll handle it
  // specially.
  if (aNamespaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::type) {
    aResult.ParseAtom(aValue);
    nsAtom* atom = aResult.GetAtomValue();
    if (atom != nsGkAtoms::translate &&
        atom != nsGkAtoms::scale &&
        atom != nsGkAtoms::rotate &&
        atom != nsGkAtoms::skewX &&
        atom != nsGkAtoms::skewY) {
      ReportAttributeParseFailure(OwnerDoc(), aAttribute, aValue);
    }
    return true;
  }

  return SVGAnimationElement::ParseAttribute(aNamespaceID,
                                             aAttribute, aValue,
                                             aMaybeScriptedPrincipal,
                                             aResult);
}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGAnimateTransformElement)

//----------------------------------------------------------------------

nsSMILAnimationFunction&
SVGAnimateTransformElement::AnimationFunction()
{
  return mAnimationFunction;
}

} // namespace dom
} // namespace mozilla

