/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGSymbolElement.h"
#include "mozilla/dom/SVGSymbolElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Symbol)

namespace mozilla {
namespace dom {

JSObject*
SVGSymbolElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGSymbolElement_Binding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ISUPPORTS_INHERITED(SVGSymbolElement, SVGSymbolElementBase,
                            mozilla::dom::SVGTests)

//----------------------------------------------------------------------
// Implementation

SVGSymbolElement::SVGSymbolElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGSymbolElementBase(aNodeInfo)
{
}

SVGSymbolElement::~SVGSymbolElement()
{
}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGSymbolElement)

} // namespace dom
} // namespace mozilla
