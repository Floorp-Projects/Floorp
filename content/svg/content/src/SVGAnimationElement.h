/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGAnimationElement_h
#define mozilla_dom_SVGAnimationElement_h

#include "DOMSVGTests.h"
#include "nsISMILAnimationElement.h"
#include "nsReferencedElement.h"
#include "nsSMILTimedElement.h"
#include "nsSVGElement.h"

typedef nsSVGElement SVGAnimationElementBase;

namespace mozilla {
namespace dom {

class SVGAnimationElement : public SVGAnimationElementBase,
                            public DOMSVGTests,
                            public nsISMILAnimationElement
{
protected:
  SVGAnimationElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  nsresult Init();

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGAnimationElement,
                                           SVGAnimationElementBase)

  // nsIContent specializations
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep, bool aNullParent);

  virtual nsresult UnsetAttr(int32_t aNamespaceID, nsIAtom* aAttribute,
                             bool aNotify);

  virtual bool IsNodeOfType(uint32_t aFlags) const;

  // Element specializations
  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                                const nsAttrValue* aValue, bool aNotify);

  // nsISMILAnimationElement interface
  virtual const Element& AsElement() const;
  virtual Element& AsElement();
  virtual bool PassesConditionalProcessingTests();
  virtual const nsAttrValue* GetAnimAttr(nsIAtom* aName) const;
  virtual bool GetAnimAttr(nsIAtom* aAttName, nsAString& aResult) const;
  virtual bool HasAnimAttr(nsIAtom* aAttName) const;
  virtual Element* GetTargetElementContent();
  virtual bool GetTargetAttributeName(int32_t* aNamespaceID,
                                        nsIAtom** aLocalName) const;
  virtual nsSMILTargetAttrType GetTargetAttributeType() const;
  virtual nsSMILTimedElement& TimedElement();
  virtual nsSMILTimeContainer* GetTimeContainer();

  virtual bool IsEventAttributeName(nsIAtom* aName) MOZ_OVERRIDE;

  // Utility methods for within SVG
  void ActivateByHyperlink();

  // WebIDL
  nsSVGElement* GetTargetElement();
  float GetStartTime(ErrorResult& rv);
  float GetCurrentTime();
  float GetSimpleDuration(ErrorResult& rv);
  void BeginElement(ErrorResult& rv) { BeginElementAt(0.f, rv); }
  void BeginElementAt(float offset, ErrorResult& rv);
  void EndElement(ErrorResult& rv) { EndElementAt(0.f, rv); }
  void EndElementAt(float offset, ErrorResult& rv);

 protected:
  // nsSVGElement overrides

  void UpdateHrefTarget(nsIContent* aNodeForContext,
                        const nsAString& aHrefStr);
  void AnimationTargetChanged();

  class TargetReference : public nsReferencedElement {
  public:
    TargetReference(SVGAnimationElement* aAnimationElement) :
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
    SVGAnimationElement* const mAnimationElement;
  };

  TargetReference      mHrefTarget;
  nsSMILTimedElement   mTimedElement;
};

} // namespace mozilla
} // namespace dom

#endif // mozilla_dom_SVGAnimationElement_h
