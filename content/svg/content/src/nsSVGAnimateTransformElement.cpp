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
#include "nsIDOMSVGAnimateTransformElement.h"
#include "nsSVGEnum.h"
#include "nsIDOMSVGTransform.h"
#include "nsIDOMSVGTransformable.h"
#include "nsSMILAnimationFunction.h"

typedef nsSVGAnimationElement nsSVGAnimateTransformElementBase;

class nsSVGAnimateTransformElement : public nsSVGAnimateTransformElementBase,
                                     public nsIDOMSVGAnimateTransformElement
{
protected:
  friend nsresult NS_NewSVGAnimateTransformElement(nsIContent **aResult,
                                                   already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGAnimateTransformElement(already_AddRefed<nsINodeInfo> aNodeInfo);

  nsSMILAnimationFunction mAnimationFunction;

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGANIMATETRANSFORMELEMENT

  NS_FORWARD_NSIDOMNODE(nsSVGAnimateTransformElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGAnimateTransformElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGAnimateTransformElementBase::)
  NS_FORWARD_NSIDOMSVGANIMATIONELEMENT(nsSVGAnimateTransformElementBase::)

  // nsIDOMNode specializations
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // nsGenericElement specializations
  bool ParseAttribute(PRInt32 aNamespaceID,
                        nsIAtom* aAttribute,
                        const nsAString& aValue,
                        nsAttrValue& aResult);

  // nsISMILAnimationElement
  virtual nsSMILAnimationFunction& AnimationFunction();

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

NS_IMPL_NS_NEW_SVG_ELEMENT(AnimateTransform)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGAnimateTransformElement,nsSVGAnimateTransformElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGAnimateTransformElement,nsSVGAnimateTransformElementBase)

DOMCI_NODE_DATA(SVGAnimateTransformElement, nsSVGAnimateTransformElement)

NS_INTERFACE_TABLE_HEAD(nsSVGAnimateTransformElement)
  NS_NODE_INTERFACE_TABLE6(nsSVGAnimateTransformElement, nsIDOMNode,
                           nsIDOMElement, nsIDOMSVGElement,
                           nsIDOMSVGAnimationElement,
                           nsIDOMSVGTests,
                           nsIDOMSVGAnimateTransformElement)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimateTransformElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGAnimateTransformElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGAnimateTransformElement::nsSVGAnimateTransformElement(already_AddRefed<nsINodeInfo> aNodeInfo)
  : nsSVGAnimateTransformElementBase(aNodeInfo)
{
}

bool
nsSVGAnimateTransformElement::ParseAttribute(PRInt32 aNamespaceID,
                                             nsIAtom* aAttribute,
                                             const nsAString& aValue,
                                             nsAttrValue& aResult)
{
  // 'type' is an <animateTransform>-specific attribute, and we'll handle it
  // specially.
  if (aNamespaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::type) {
    aResult.ParseAtom(aValue);
    nsIAtom* atom = aResult.GetAtomValue();
    if (atom != nsGkAtoms::translate &&
        atom != nsGkAtoms::scale &&
        atom != nsGkAtoms::rotate &&
        atom != nsGkAtoms::skewX &&
        atom != nsGkAtoms::skewY) {
      ReportAttributeParseFailure(OwnerDoc(), aAttribute, aValue);
    }
    return true;
  }

  return nsSVGAnimateTransformElementBase::ParseAttribute(aNamespaceID, 
                                                          aAttribute, aValue,
                                                          aResult);
}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGAnimateTransformElement)

//----------------------------------------------------------------------
// nsISMILAnimationElement methods

nsSMILAnimationFunction&
nsSVGAnimateTransformElement::AnimationFunction()
{
  return mAnimationFunction;
}
