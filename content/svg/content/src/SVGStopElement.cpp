/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGStopElement.h"
#include "mozilla/dom/SVGStopElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Stop)

namespace mozilla {
namespace dom {

JSObject*
SVGStopElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return SVGStopElementBinding::Wrap(aCx, aScope, this);
}

nsSVGElement::NumberInfo SVGStopElement::sNumberInfo =
{ &nsGkAtoms::offset, 0, true };

//----------------------------------------------------------------------
// Implementation

SVGStopElement::SVGStopElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGStopElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGStopElement)

//----------------------------------------------------------------------

already_AddRefed<SVGAnimatedNumber>
SVGStopElement::Offset()
{
  return mOffset.ToDOMAnimatedNumber(this);
}

//----------------------------------------------------------------------
// sSVGElement methods

nsSVGElement::NumberAttributesInfo
SVGStopElement::GetNumberInfo()
{
  return NumberAttributesInfo(&mOffset, &sNumberInfo, 1);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(bool)
SVGStopElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sGradientStopMap
  };

  return FindAttributeDependence(name, map) ||
    SVGStopElementBase::IsAttributeMapped(name);
}

} // namespace dom
} // namespace mozilla

