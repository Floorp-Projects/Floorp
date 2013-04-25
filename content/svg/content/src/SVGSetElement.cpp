/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGSetElement.h"
#include "mozilla/dom/SVGSetElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(Set)

namespace mozilla {
namespace dom {

JSObject*
SVGSetElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return SVGSetElementBinding::Wrap(aCx, aScope, this);
}

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

nsSMILAnimationFunction&
SVGSetElement::AnimationFunction()
{
  return mAnimationFunction;
}

} // namespace dom
} // namespace mozilla

