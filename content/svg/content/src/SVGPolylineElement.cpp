/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGPolylineElement.h"
#include "mozilla/dom/SVGPolylineElementBinding.h"

DOMCI_NODE_DATA(SVGPolylineElement, mozilla::dom::SVGPolylineElement)

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Polyline)

namespace mozilla {
namespace dom {

JSObject*
SVGPolylineElement::WrapNode(JSContext *aCx, JSObject *aScope, bool *aTriedToWrap)
{
  return SVGPolylineElementBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(SVGPolylineElement,SVGPolylineElementBase)
NS_IMPL_RELEASE_INHERITED(SVGPolylineElement,SVGPolylineElementBase)

NS_INTERFACE_TABLE_HEAD(SVGPolylineElement)
  NS_NODE_INTERFACE_TABLE4(SVGPolylineElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement,
                           nsIDOMSVGPolylineElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGPolylineElement)
NS_INTERFACE_MAP_END_INHERITING(SVGPolylineElementBase)

//----------------------------------------------------------------------
// Implementation

SVGPolylineElement::SVGPolylineElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGPolylineElementBase(aNodeInfo)
{

}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGPolylineElement)

} // namespace dom
} // namespace mozilla
