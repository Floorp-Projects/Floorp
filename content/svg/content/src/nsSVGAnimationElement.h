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

#ifndef NS_SVGANIMATIONELEMENT_H_
#define NS_SVGANIMATIONELEMENT_H_

#include "nsSVGElement.h"
#include "nsAutoPtr.h"
#include "nsReferencedElement.h"
#include "nsIDOMSVGAnimationElement.h"
#include "nsIDOMElementTimeControl.h"
#include "nsISMILAnimationElement.h"
#include "nsSMILTimedElement.h"

typedef nsSVGElement nsSVGAnimationElementBase;

class nsSVGAnimationElement : public nsSVGAnimationElementBase,
                              public nsISMILAnimationElement,
                              public nsIDOMElementTimeControl
{
protected:
  nsSVGAnimationElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsSVGAnimationElement,
                                           nsSVGAnimationElementBase)
  NS_DECL_NSIDOMSVGANIMATIONELEMENT
  NS_DECL_NSIDOMELEMENTTIMECONTROL

  // nsIContent specializations
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep, bool aNullParent);

  virtual nsresult UnsetAttr(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                             bool aNotify);

  virtual bool IsNodeOfType(PRUint32 aFlags) const;

  // nsGenericElement specializations
  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsresult AfterSetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                                const nsAString* aValue, bool aNotify);

  // nsISMILAnimationElement interface
  virtual const Element& AsElement() const;
  virtual Element& AsElement();
  virtual const nsAttrValue* GetAnimAttr(nsIAtom* aName) const;
  virtual bool GetAnimAttr(nsIAtom* aAttName, nsAString& aResult) const;
  virtual bool HasAnimAttr(nsIAtom* aAttName) const;
  virtual Element* GetTargetElementContent();
  virtual bool GetTargetAttributeName(PRInt32* aNamespaceID,
                                        nsIAtom** aLocalName) const;
  virtual nsSMILTargetAttrType GetTargetAttributeType() const;
  virtual nsSMILTimedElement& TimedElement();
  virtual nsSMILTimeContainer* GetTimeContainer();

protected:
  // nsSVGElement overrides
  bool IsEventName(nsIAtom* aName);

  void UpdateHrefTarget(nsIContent* aNodeForContext,
                        const nsAString& aHrefStr);
  void AnimationTargetChanged();

  class TargetReference : public nsReferencedElement {
  public:
    TargetReference(nsSVGAnimationElement* aAnimationElement) :
      mAnimationElement(aAnimationElement) {}
  protected:
    // We need to be notified when target changes, in order to request a
    // sample (which will clear animation effects from old target and apply
    // them to the new target) and update any event registrations.
    virtual void ElementChanged(Element* aFrom, Element* aTo) {
      nsReferencedElement::ElementChanged(aFrom, aTo);
      mAnimationElement->AnimationTargetChanged();
    }

    // We need to override IsPersistent to get persistent tracking (beyond the
    // first time the target changes)
    virtual bool IsPersistent() { return true; }
  private:
    nsSVGAnimationElement* const mAnimationElement;
  };

  TargetReference      mHrefTarget;
  nsSMILTimedElement   mTimedElement;
};

#endif // NS_SVGANIMATIONELEMENT_H_
