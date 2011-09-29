/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
 *   Chris Double  <chris.double@double.co.nz>
 *   Daniel Holbert <dholbert@mozilla.com>
 *   Eric Hedekar <afterthebeep@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSVGAnimateMotionElement.h"

NS_IMPL_NS_NEW_SVG_ELEMENT(AnimateMotion)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGAnimateMotionElement,nsSVGAnimateMotionElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGAnimateMotionElement,nsSVGAnimateMotionElementBase)

DOMCI_NODE_DATA(SVGAnimateMotionElement, nsSVGAnimateMotionElement)

NS_INTERFACE_TABLE_HEAD(nsSVGAnimateMotionElement)
  NS_NODE_INTERFACE_TABLE5(nsSVGAnimateMotionElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGAnimationElement,
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
  return PR_TRUE;
}

nsSMILTargetAttrType
nsSVGAnimateMotionElement::GetTargetAttributeType() const
{
  // <animateMotion> doesn't take an attributeType, since it doesn't target an
  // 'attribute' per se.  We'll just return 'XML' for simplicity.  (This just
  // needs to match what we expect in nsSVGElement::GetAnimAttr.)
  return eSMILTargetAttrType_XML;
}
