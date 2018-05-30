/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGAnimateMotionElement.h"
#include "mozilla/dom/SVGAnimateMotionElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(AnimateMotion)

namespace mozilla {
namespace dom {

JSObject*
SVGAnimateMotionElement::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGAnimateMotionElementBinding::Wrap(aCx, this, aGivenProto);
}

//----------------------------------------------------------------------
// Implementation

SVGAnimateMotionElement::SVGAnimateMotionElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo)
  : SVGAnimationElement(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsINode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGAnimateMotionElement)

//----------------------------------------------------------------------

nsSMILAnimationFunction&
SVGAnimateMotionElement::AnimationFunction()
{
  return mAnimationFunction;
}

bool
SVGAnimateMotionElement::GetTargetAttributeName(int32_t *aNamespaceID,
                                                nsAtom **aLocalName) const
{
  // <animateMotion> doesn't take an attributeName, since it doesn't target an
  // 'attribute' per se.  We'll use a unique dummy attribute-name so that our
  // nsSMILTargetIdentifier logic (which requires a attribute name) still works.
  *aNamespaceID = kNameSpaceID_None;
  *aLocalName = nsGkAtoms::mozAnimateMotionDummyAttr;
  return true;
}

} // namespace dom
} // namespace mozilla

