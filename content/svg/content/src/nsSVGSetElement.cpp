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
#include "nsIDOMSVGSetElement.h"
#include "nsSMILSetAnimationFunction.h"

typedef nsSVGAnimationElement nsSVGSetElementBase;

class nsSVGSetElement : public nsSVGSetElementBase,
                        public nsIDOMSVGSetElement
{
protected:
  friend nsresult NS_NewSVGSetElement(nsIContent **aResult,
                                      already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGSetElement(already_AddRefed<nsINodeInfo> aNodeInfo);

  nsSMILSetAnimationFunction mAnimationFunction;

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGSETELEMENT

  NS_FORWARD_NSIDOMNODE(nsSVGSetElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGSetElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGSetElementBase::)
  NS_FORWARD_NSIDOMSVGANIMATIONELEMENT(nsSVGSetElementBase::)
  
  // nsIDOMNode
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // nsISMILAnimationElement
  virtual nsSMILAnimationFunction& AnimationFunction();

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(Set)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGSetElement,nsSVGSetElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGSetElement,nsSVGSetElementBase)

DOMCI_NODE_DATA(SVGSetElement, nsSVGSetElement)

NS_INTERFACE_TABLE_HEAD(nsSVGSetElement)
  NS_NODE_INTERFACE_TABLE6(nsSVGSetElement, nsIDOMNode, nsIDOMElement,
                           nsIDOMSVGElement, nsIDOMSVGAnimationElement,
                           nsIDOMSVGTests, nsIDOMSVGSetElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGSetElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGSetElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGSetElement::nsSVGSetElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGSetElementBase(aNodeInfo)
{
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGSetElement)

//----------------------------------------------------------------------
// nsISMILAnimationElement methods

nsSMILAnimationFunction&
nsSVGSetElement::AnimationFunction()
{
  return mAnimationFunction;
}
