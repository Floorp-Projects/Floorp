/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGAnimateElement.h"
#include "mozilla/dom/SVGAnimateElementBinding.h"

DOMCI_NODE_DATA(SVGAnimateElement, mozilla::dom::SVGAnimateElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Animate)

namespace mozilla {
namespace dom {

JSObject*
SVGAnimateElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGAnimateElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

//----------------------------------------------------------------------
// nsISupports methods


NS_IMPL_ADDREF_INHERITED(SVGAnimateElement, SVGAnimationElement)
NS_IMPL_RELEASE_INHERITED(SVGAnimateElement, SVGAnimationElement)

NS_INTERFACE_TABLE_HEAD(SVGAnimateElement)
  NS_NODE_INTERFACE_TABLE5(SVGAnimateElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGAnimationElement,
                           nsIDOMSVGAnimateElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimateElement)
NS_INTERFACE_MAP_END_INHERITING(SVGAnimationElement)

//----------------------------------------------------------------------
// Implementation

SVGAnimateElement::SVGAnimateElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGAnimationElement(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGAnimateElement)

//----------------------------------------------------------------------
// nsISMILAnimationElement methods

nsSMILAnimationFunction&
SVGAnimateElement::AnimationFunction()
{
  return mAnimationFunction;
}

} // namespace mozilla
} // namespace dom

