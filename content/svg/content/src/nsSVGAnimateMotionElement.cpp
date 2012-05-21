/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGAnimateMotionElement.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(AnimateMotion)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGAnimateMotionElement,nsSVGAnimateMotionElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGAnimateMotionElement,nsSVGAnimateMotionElementBase)

DOMCI_NODE_DATA(SVGAnimateMotionElement, nsSVGAnimateMotionElement)

NS_INTERFACE_TABLE_HEAD(nsSVGAnimateMotionElement)
  NS_NODE_INTERFACE_TABLE6(nsSVGAnimateMotionElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGAnimationElement, nsIDOMSVGTests,
                           nsIDOMSVGAnimateMotionElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimateMotionElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGAnimateMotionElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGAnimateMotionElement::nsSVGAnimateMotionElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGAnimateMotionElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGAnimateMotionElement)

//----------------------------------------------------------------------
// nsISMILAnimationElement methods

nsSMILAnimationFunction&
nsSVGAnimateMotionElement::AnimationFunction()
{
  return mAnimationFunction;
}

bool
nsSVGAnimateMotionElement::GetTargetAttributeName(PRInt32 *aNamespaceID,
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
nsSVGAnimateMotionElement::GetTargetAttributeType() const
{
  // <animateMotion> doesn't take an attributeType, since it doesn't target an
  // 'attribute' per se.  We'll just return 'XML' for simplicity.  (This just
  // needs to match what we expect in nsSVGElement::GetAnimAttr.)
  return eSMILTargetAttrType_XML;
}
