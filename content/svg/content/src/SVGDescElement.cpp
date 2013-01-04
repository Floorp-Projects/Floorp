/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGDescElement.h"
#include "mozilla/dom/SVGDescElementBinding.h"

DOMCI_NODE_DATA(SVGDescElement, mozilla::dom::SVGDescElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Desc)

namespace mozilla {
namespace dom {

JSObject*
SVGDescElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGDescElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGDescElement, SVGDescElementBase)
NS_IMPL_RELEASE_INHERITED(SVGDescElement, SVGDescElementBase)

NS_INTERFACE_TABLE_HEAD(SVGDescElement)
  NS_NODE_INTERFACE_TABLE4(SVGDescElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGDescElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGDescElement)
NS_INTERFACE_MAP_END_INHERITING(SVGDescElementBase)

//----------------------------------------------------------------------
// Implementation

SVGDescElement::SVGDescElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGDescElementBase(aNodeInfo)
{
  SetIsDOMBinding();
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGDescElement)

} // namespace dom
} // namespace mozilla

