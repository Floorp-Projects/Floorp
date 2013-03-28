/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGTSpanElement.h"
#include "mozilla/dom/SVGTSpanElementBinding.h"
#include "nsContentUtils.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(TSpan)

namespace mozilla {
namespace dom {

JSObject*
SVGTSpanElement::WrapNode(JSContext *aCx, JSObject *aScope)
{
  return SVGTSpanElementBinding::Wrap(aCx, aScope, this);
}


//----------------------------------------------------------------------
// Implementation

SVGTSpanElement::SVGTSpanElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGTSpanElementBase(aNodeInfo)
{
}


//----------------------------------------------------------------------
// nsIDOMNode methods


NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGTSpanElement)

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGTSpanElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sColorMap,
    sFillStrokeMap,
    sFontSpecificationMap,
    sGraphicsMap,
    sTextContentElementsMap
  };

  return FindAttributeDependence(name, map) ||
    SVGTSpanElementBase::IsAttributeMapped(name);
}

//----------------------------------------------------------------------
// nsSVGElement overrides

bool
SVGTSpanElement::IsEventName(nsIAtom* aName)
{
  return nsContentUtils::IsEventAttributeName(aName, EventNameType_SVGGraphic);
}

} // namespace dom
} // namespace mozilla
