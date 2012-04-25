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
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
