/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SVGANIMATIONELEMENT_H_
#define NS_SVGANIMATIONELEMENT_H_

#include "DOMSVGTests.h"
#include "nsIDOMElementTimeControl.h"
#include "nsIDOMSVGAnimationElement.h"
#include "nsISMILAnimationElement.h"
#include "nsReferencedElement.h"
#include "nsSMILTimedElement.h"
#include "nsSVGElement.h"

typedef nsSVGElement nsSVGAnimationElementBase;

class nsSVGAnimationElement : public nsSVGAnimationElementBase,
                              public DOMSVGTests,
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
                                const nsAttrValue* aValue, bool aNotify);

  // nsISMILAnimationElement interface
  virtual const Element& AsElement() const;
  virtual Element& AsElement();
  virtual bool PassesConditionalProcessingTests();
  virtual const nsAttrValue* GetAnimAttr(nsIAtom* aName) const;
  virtual bool GetAnimAttr(nsIAtom* aAttName, nsAString& aResult) const;
  virtual bool HasAnimAttr(nsIAtom* aAttName) const;
  virtual Element* GetTargetElementContent();
  virtual bool GetTargetAttributeName(PRInt32* aNamespaceID,
                                        nsIAtom** aLocalName) const;
  virtual nsSMILTargetAttrType GetTargetAttributeType() const;
  virtual nsSMILTimedElement& TimedElement();
  virtual nsSMILTimeContainer* GetTimeContainer();

  // Utility methods for within SVG
  void ActivateByHyperlink();

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
