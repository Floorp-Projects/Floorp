/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGDescElement.h"
#include "mozilla/dom/SVGDescElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Desc)

namespace mozilla {
namespace dom {

JSObject*
SVGDescElement::WrapNode(JSContext* aCx, JSObject* aScope)
{
  return SVGDescElementBinding::Wrap(aCx, aScope, this);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS_INHERITED3(SVGDescElement, SVGDescElementBase,
                             nsIDOMNode, nsIDOMElement,
                             nsIDOMSVGElement)

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

