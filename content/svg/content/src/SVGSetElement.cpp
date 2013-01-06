/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGSetElement.h"
#include "mozilla/dom/SVGSetElementBinding.h"

DOMCI_NODE_DATA(SVGSetElement, mozilla::dom::SVGSetElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Set)

namespace mozilla {
namespace dom {

JSObject*
SVGSetElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGSetElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGSetElement, SVGAnimationElement)
NS_IMPL_RELEASE_INHERITED(SVGSetElement, SVGAnimationElement)

NS_INTERFACE_TABLE_HEAD(SVGSetElement)
  NS_NODE_INTERFACE_TABLE5(SVGSetElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGAnimationElement,
                           nsIDOMSVGSetElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGSetElement)
NS_INTERFACE_MAP_END_INHERITING(SVGAnimationElement)

//----------------------------------------------------------------------
// Implementation

SVGSetElement::SVGSetElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGAnimationElement(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGSetElement)

//----------------------------------------------------------------------
// nsISMILAnimationElement methods

nsSMILAnimationFunction&
SVGSetElement::AnimationFunction()
{
  return mAnimationFunction;
}

} // namespace dom
} // namespace mozilla

