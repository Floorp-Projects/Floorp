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

#include "nsSVGAnimationElement.h"
#include "nsIDOMSVGAnimateElement.h"
#include "nsSMILAnimationFunction.h"

typedef nsSVGAnimationElement nsSVGAnimateElementBase;

class nsSVGAnimateElement : public nsSVGAnimateElementBase,
                            public nsIDOMSVGAnimateElement
{
protected:
  friend nsresult NS_NewSVGAnimateElement(nsIContent **aResult,
                                          already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGAnimateElement(already_AddRefed<nsINodeInfo> aNodeInfo);

  nsSMILAnimationFunction mAnimationFunction;

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGANIMATEELEMENT

  NS_FORWARD_NSIDOMNODE(nsSVGAnimateElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGAnimateElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGAnimateElementBase::)
  NS_FORWARD_NSIDOMSVGANIMATIONELEMENT(nsSVGAnimateElementBase::)
  
  // nsIDOMNode
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // nsISMILAnimationElement
  virtual nsSMILAnimationFunction& AnimationFunction();

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Animate)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGAnimateElement,nsSVGAnimateElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGAnimateElement,nsSVGAnimateElementBase)

DOMCI_NODE_DATA(SVGAnimateElement, nsSVGAnimateElement)

NS_INTERFACE_TABLE_HEAD(nsSVGAnimateElement)
  NS_NODE_INTERFACE_TABLE6(nsSVGAnimateElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGAnimationElement,
                           nsIDOMSVGTests, nsIDOMSVGAnimateElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimateElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGAnimateElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGAnimateElement::nsSVGAnimateElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGAnimateElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGAnimateElement)

//----------------------------------------------------------------------
// nsISMILAnimationElement methods

nsSMILAnimationFunction&
nsSVGAnimateElement::AnimationFunction()
{
  return mAnimationFunction;
}
