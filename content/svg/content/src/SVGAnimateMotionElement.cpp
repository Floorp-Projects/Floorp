/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SVGAnimateMotionElement.h"
#include "mozilla/dom/SVGAnimateMotionElementBinding.h"

NS_IMPL_NS_NEW_NAMESPACED_SVG_ELEMENT(AnimateMotion)

namespace mozilla {
namespace dom {

JSObject*
SVGAnimateMotionElement::WrapNode(JSContext *aCx, JSObject *aScope)
{
  return SVGAnimateMotionElementBinding::Wrap(aCx, aScope, this);
}

//----------------------------------------------------------------------
// nsISupports methods
NS_IMPL_ISUPPORTS_INHERITED3(SVGAnimateMotionElement, SVGAnimationElement,
                             nsIDOMNode,
                             nsIDOMElement, nsIDOMSVGElement)

//----------------------------------------------------------------------
// Implementation

SVGAnimateMotionElement::SVGAnimateMotionElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : SVGAnimationElement(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(SVGAnimateMotionElement)

//----------------------------------------------------------------------

nsSMILAnimationFunction&
SVGAnimateMotionElement::AnimationFunction()
{
  return mAnimationFunction;
}

bool
SVGAnimateMotionElement::GetTargetAttributeName(int32_t *aNamespaceID,
                                                nsIAtom **aLocalName) const
{
  // <animateMotion> doesn't take an attributeName, since it doesn't target an
  // 'attribute' per se.  We'll use a unique dummy attribute-name so that our
  // nsSMILTargetIdentifier logic (which requires a attribute name) still works.
  *aNamespaceID = kNameSpaceID_None;
  *aLocalName = nsGkAtoms::mozAnimateMotionDummyAttr;
  return true;
}

nsSMILTargetAttrType
SVGAnimateMotionElement::GetTargetAttributeType() const
{
  // <animateMotion> doesn't take an attributeType, since it doesn't target an
  // 'attribute' per se.  We'll just return 'XML' for simplicity.  (This just
  // needs to match what we expect in nsSVGElement::GetAnimAttr.)
  return eSMILTargetAttrType_XML;
}

} // namespace dom
} // namespace mozilla

