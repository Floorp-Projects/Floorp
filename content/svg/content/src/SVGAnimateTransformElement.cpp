/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGAnimateTransformElement.h"
#include "mozilla/dom/SVGAnimateTransformElementBinding.h"

DOMCI_NODE_DATA(SVGAnimateTransformElement, mozilla::dom::SVGAnimateTransformElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(AnimateTransform)

namespace mozilla {
namespace dom {

JSObject*
SVGAnimateTransformElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGAnimateTransformElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGAnimateTransformElement, SVGAnimationElement)
NS_IMPL_RELEASE_INHERITED(SVGAnimateTransformElement, SVGAnimationElement)

NS_INTERFACE_TABLE_HEAD(SVGAnimateTransformElement)
  NS_NODE_INTERFACE_TABLE5(SVGAnimateTransformElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGAnimationElement,
                           nsIDOMSVGAnimateTransformElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimateTransformElement)
NS_INTERFACE_MAP_END_INHERITING(SVGAnimationElement)

//----------------------------------------------------------------------
// Implementation

SVGAnimateTransformElement::SVGAnimateTransformElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGAnimationElement(aNodeInfo)
{
}

bool
SVGAnimateTransformElement::ParseAttribute(int32_t aNamespaceID,
                                           nsIAtom* aAttribute,
                                           const nsAString& aValue,
                                           nsAttrValue& aResult)
{
  // 'type' is an <animateTransform>-specific attribute, and we'll handle it
  // specially.
  if (aNamespaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::type) {
    aResult.ParseAtom(aValue);
    nsIAtom* atom = aResult.GetAtomValue();
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
                                             aResult);
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGAnimateTransformElement)

//----------------------------------------------------------------------
// nsISMILAnimationElement methods

nsSMILAnimationFunction&
SVGAnimateTransformElement::AnimationFunction()
{
  return mAnimationFunction;
}

bool
SVGAnimateTransformElement::GetTargetAttributeName(int32_t *aNamespaceID,
                                                   nsIAtom **aLocalName) const
{
  if (SVGAnimationElement::GetTargetAttributeName(aNamespaceID,
                                                  aLocalName)) {
    return *aNamespaceID == kNameSpaceID_None &&
           (*aLocalName == nsGkAtoms::transform ||
            *aLocalName == nsGkAtoms::patternTransform ||
            *aLocalName == nsGkAtoms::gradientTransform);
  }
  return false;
}

} // namespace dom
} // namespace mozilla

