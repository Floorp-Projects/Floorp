/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGAnimateElement.h"
#include "mozilla/dom/SVGAnimateElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Animate)

namespace mozilla {
namespace dom {

JSObject*
SVGAnimateElement::WrapNode(JSContext *aCx)
{
  return SVGAnimateElementBinding::Wrap(aCx, this);
}

//----------------------------------------------------------------------
// Implementation

SVGAnimateElement::SVGAnimateElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGAnimationElement(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGAnimateElement)

//----------------------------------------------------------------------

nsSMILAnimationFunction&
SVGAnimateElement::AnimationFunction()
{
  return mAnimationFunction;
}

} // namespace mozilla
} // namespace dom

