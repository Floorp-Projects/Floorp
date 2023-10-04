/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGANIMATIONELEMENT_H_
#define DOM_SVG_SVGANIMATIONELEMENT_H_

#include "mozilla/Attributes.h"
#include "mozilla/SMILTimedElement.h"
#include "mozilla/dom/IDTracker.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/dom/SVGTests.h"

namespace mozilla::dom {

using SVGAnimationElementBase = SVGElement;

class SVGAnimationElement : public SVGAnimationElementBase, public SVGTests {
 protected:
  explicit SVGAnimationElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo);
  nsresult Init();
  virtual ~SVGAnimationElement() = default;

 public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED

  NS_IMPL_FROMNODE_HELPER(SVGAnimationElement, IsSVGAnimationElement())

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SVGAnimationElement,
                                           SVGAnimationElementBase)

  bool IsSVGAnimationElement() const final { return true; }
  bool PassesConditionalProcessingTests() const final {
    return SVGTests::PassesConditionalProcessingTests();
  }
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override = 0;

  // nsIContent specializations
  nsresult BindToTree(BindContext&, nsINode& aParent) override;
  void UnbindFromTree(bool aNullParent) override;

  // Element specializations
  bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                      const nsAString& aValue,
                      nsIPrincipal* aMaybeScriptedPrincipal,
                      nsAttrValue& aResult) override;
  void AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                    const nsAttrValue* aValue, const nsAttrValue* aOldValue,
                    nsIPrincipal* aSubjectPrincipal, bool aNotify) override;

  Element* GetTargetElementContent();
  virtual bool GetTargetAttributeName(int32_t* aNamespaceID,
                                      nsAtom** aLocalName) const;
  mozilla::SMILTimedElement& TimedElement();
  mozilla::SMILTimeContainer* GetTimeContainer();
  virtual SMILAnimationFunction& AnimationFunction() = 0;

  bool IsEventAttributeNameInternal(nsAtom* aName) override;

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

  // Manually implement onbegin/onrepeat/onend IDL property getters/setters.
  // We don't autogenerate these methods because the property name differs
  // from the event type atom - the event type atom has an extra 'Event' tacked
  // on at the end. (i.e. 'onbegin' corresponds to an event whose name is
  // literally 'beginEvent' rather than 'begin')

  EventHandlerNonNull* GetOnbegin() {
    return GetEventHandler(nsGkAtoms::onbeginEvent);
  }
  void SetOnbegin(EventHandlerNonNull* handler) {
    EventTarget::SetEventHandler(nsGkAtoms::onbeginEvent, handler);
  }

  EventHandlerNonNull* GetOnrepeat() {
    return GetEventHandler(nsGkAtoms::onrepeatEvent);
  }
  void SetOnrepeat(EventHandlerNonNull* handler) {
    EventTarget::SetEventHandler(nsGkAtoms::onrepeatEvent, handler);
  }

  EventHandlerNonNull* GetOnend() {
    return GetEventHandler(nsGkAtoms::onendEvent);
  }
  void SetOnend(EventHandlerNonNull* handler) {
    EventTarget::SetEventHandler(nsGkAtoms::onendEvent, handler);
  }

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
    void ElementChanged(Element* aFrom, Element* aTo) override {
      IDTracker::ElementChanged(aFrom, aTo);
      mAnimationElement->AnimationTargetChanged();
    }

    // We need to override IsPersistent to get persistent tracking (beyond the
    // first time the target changes)
    bool IsPersistent() override { return true; }

   private:
    SVGAnimationElement* const mAnimationElement;
  };

  HrefTargetTracker mHrefTarget;
  mozilla::SMILTimedElement mTimedElement;
};

}  // namespace mozilla::dom

#endif  // DOM_SVG_SVGANIMATIONELEMENT_H_
