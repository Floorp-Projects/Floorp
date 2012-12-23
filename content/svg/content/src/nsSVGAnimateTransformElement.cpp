/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGAnimationElement.h"
#include "nsIDOMSVGAnimateTransformElement.h"
#include "nsSVGEnum.h"
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

  NS_FORWARD_NSIDOMNODE_TO_NSINODE
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGAnimateTransformElementBase::)
  NS_FORWARD_NSIDOMSVGANIMATIONELEMENT(nsSVGAnimateTransformElementBase::)

  // nsIDOMNode specializations
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // Element specializations
  bool ParseAttribute(int32_t aNamespaceID,
                        nsIAtom* aAttribute,
                        const nsAString& aValue,
                        nsAttrValue& aResult);

  // nsISMILAnimationElement
  virtual nsSMILAnimationFunction& AnimationFunction();
  virtual bool GetTargetAttributeName(int32_t *aNamespaceID,
                                      nsIAtom **aLocalName) const;

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
nsSVGAnimateTransformElement::ParseAttribute(int32_t aNamespaceID,
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

bool
nsSVGAnimateTransformElement::GetTargetAttributeName(int32_t *aNamespaceID,
                                                     nsIAtom **aLocalName) const
{
  if (nsSVGAnimateTransformElementBase::GetTargetAttributeName(aNamespaceID,
                                                               aLocalName)) {
    return *aNamespaceID == kNameSpaceID_None &&
           (*aLocalName == nsGkAtoms::transform ||
            *aLocalName == nsGkAtoms::patternTransform ||
            *aLocalName == nsGkAtoms::gradientTransform);
  }
  return false;
}
