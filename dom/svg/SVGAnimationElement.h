/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SVGAnimationElement_h
#define mozilla_dom_SVGAnimationElement_h

#include "mozilla/Attributes.h"
#include "mozilla/SMILTimedElement.h"
#include "mozilla/dom/IDTracker.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/dom/SVGTests.h"

namespace mozilla {
namespace dom {

typedef SVGElement SVGAnimationElementBase;

class SVGAnimationElement : public SVGAnimationElementBase, public SVGTests {
 protected:
  explicit SVGAnimationElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  nsresult Init();
  virtual ~SVGAnimationElement() = default;

 public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGAnimationElement,
                                           SVGAnimationElementBase)

  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override = 0;

  // nsIContent specializations
  virtual nsresult BindToTree(Document* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent) override;
  virtual void UnbindFromTree(bool aDeep, bool aNullParent) override;

  virtual bool IsNodeOfType(uint32_t aFlags) const override;

  // Element specializations
  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;

  Element* GetTargetElementContent();
  virtual bool GetTargetAttributeName(int32_t* aNamespaceID,
                                      nsAtom** aLocalName) const;
  mozilla::SMILTimedElement& TimedElement();
  mozilla::SMILTimeContainer* GetTimeContainer();
  virtual SMILAnimationFunction& AnimationFunction() = 0;

  virtual bool IsEventAttributeNameInternal(nsAtom* aName) override;

  // Utility methods for within SVG
  void ActivateByHyperlink();

  // WebIDL
  SVGElement* GetTargetElement();
  float GetStartTime(ErrorResult& rv);
  float GetCurrentTimeAsFloat();
  float GetSimpleDuration(ErrorResult& rv);
  void BeginElement(ErrorResult& rv) { BeginElementAt(0.f, rv); }
  void BeginElementAt(float offset, ErrorResult& rv);
  void EndElement(ErrorResult& rv) { EndElementAt(0.f, rv); }
  void EndElementAt(float offset, ErrorResult& rv);

  // SVGTests
  SVGElement* AsSVGElement() final { return this; }

 protected:
  // SVGElement overrides

  void UpdateHrefTarget(const nsAString& aHrefStr);
  void AnimationTargetChanged();

  /**
   * Helper that provides a reference to the element with the ID that is
   * referenced by the animation element's 'href' attribute, if any, and that
   * will notify the animation element if the element that that ID identifies
   * changes to a different element (or none).  (If the 'href' attribute is not
   * specified, then the animation target is the parent element and this helper
   * is not used.)
   */
  class HrefTargetTracker final : public IDTracker {
   public:
    explicit HrefTargetTracker(SVGAnimationElement* aAnimationElement)
        : mAnimationElement(aAnimationElement) {}

   protected:
    // We need to be notified when target changes, in order to request a
    // sample (which will clear animation effects from old target and apply
    // them to the new target) and update any event registrations.
    virtual void ElementChanged(Element* aFrom, Element* aTo) override {
      IDTracker::ElementChanged(aFrom, aTo);
      mAnimationElement->AnimationTargetChanged();
    }

    // We need to override IsPersistent to get persistent tracking (beyond the
    // first time the target changes)
    virtual bool IsPersistent() override { return true; }

   private:
    SVGAnimationElement* const mAnimationElement;
  };

  HrefTargetTracker mHrefTarget;
  mozilla::SMILTimedElement mTimedElement;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SVGAnimationElement_h
