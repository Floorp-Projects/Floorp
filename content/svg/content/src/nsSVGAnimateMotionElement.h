/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SVGANIMATEMOTIONELEMENT_H_
#define NS_SVGANIMATEMOTIONELEMENT_H_

#include "nsIDOMSVGAnimateMotionElement.h"
#include "nsSVGAnimationElement.h"
#include "nsSVGEnum.h"
#include "SVGMotionSMILAnimationFunction.h"

typedef nsSVGAnimationElement nsSVGAnimateMotionElementBase;

class nsSVGAnimateMotionElement : public nsSVGAnimateMotionElementBase,
                                  public nsIDOMSVGAnimateMotionElement
{
protected:
  friend nsresult NS_NewSVGAnimateMotionElement(nsIContent **aResult,
                                                already_AddRefed<nsINodeInfo> aNodeInfo);
  nsSVGAnimateMotionElement(already_AddRefed<nsINodeInfo> aNodeInfo);

  mozilla::SVGMotionSMILAnimationFunction mAnimationFunction;

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGANIMATEMOTIONELEMENT

  NS_FORWARD_NSIDOMNODE(nsSVGAnimateMotionElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGAnimateMotionElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGAnimateMotionElementBase::)
  NS_FORWARD_NSIDOMSVGANIMATIONELEMENT(nsSVGAnimateMotionElementBase::)

  // nsIDOMNode specializations
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  // nsISMILAnimationElement
  virtual nsSMILAnimationFunction& AnimationFunction();
  virtual bool GetTargetAttributeName(PRInt32 *aNamespaceID,
                                        nsIAtom **aLocalName) const;
  virtual nsSMILTargetAttrType GetTargetAttributeType() const;

  // nsSVGElement
  virtual nsIAtom* GetPathDataAttrName() const {
    return nsGkAtoms::path;
  }

  // Utility method to let our <mpath> children tell us when they've changed,
  // so we can make sure our mAnimationFunction is marked as having changed.
  void MpathChanged() { mAnimationFunction.MpathChanged(); }

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
};

#endif // NS_SVGANIMATEMOTIONELEMENT_H_
