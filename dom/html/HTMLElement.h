/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLElement_h
#define mozilla_dom_HTMLElement_h

#include "nsGenericHTMLElement.h"

namespace mozilla::dom {

class HTMLElement final : public nsGenericHTMLFormElement {
 public:
  explicit HTMLElement(already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
                       FromParser aFromParser = NOT_FROM_PARSER);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLElement,
                                           nsGenericHTMLFormElement)

  // EventTarget
  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

  // nsINode
  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
  nsINode* GetScopeChainParent() const override;

  // nsIContent
  nsresult BindToTree(BindContext&, nsINode& aParent) override;
  void UnbindFromTree(bool aNullParent = true) override;
  void DoneCreatingElement() override;

  // Element
  void SetCustomElementDefinition(
      CustomElementDefinition* aDefinition) override;
  bool IsLabelable() const override { return IsFormAssociatedElement(); }

  // nsGenericHTMLElement
  // https://html.spec.whatwg.org/multipage/custom-elements.html#dom-attachinternals
  already_AddRefed<mozilla::dom::ElementInternals> AttachInternals(
      ErrorResult& aRv) override;
  bool IsDisabledForEvents(WidgetEvent* aEvent) override;

  // nsGenericHTMLFormElement
  bool IsFormAssociatedElement() const override;
  void AfterClearForm(bool aUnbindOrDelete) override;
  void FieldSetDisabledChanged(bool aNotify) override;
  void SaveState() override;
  void UpdateValidityElementStates(bool aNotify) final;

  void UpdateFormOwner();

  void MaybeRestoreFormAssociatedCustomElementState();

  void InhibitRestoration(bool aShouldInhibit);

 protected:
  virtual ~HTMLElement() = default;

  JSObject* WrapNode(JSContext* aCx,
                     JS::Handle<JSObject*> aGivenProto) override;

  // Element
  void AfterSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                    const nsAttrValue* aValue, const nsAttrValue* aOldValue,
                    nsIPrincipal* aMaybeScriptedPrincipal,
                    bool aNotify) override;

  // nsGenericHTMLFormElement
  void SetFormInternal(HTMLFormElement* aForm, bool aBindToTree) override;
  HTMLFormElement* GetFormInternal() const override;
  void SetFieldSetInternal(HTMLFieldSetElement* aFieldset) override;
  HTMLFieldSetElement* GetFieldSetInternal() const override;
  bool CanBeDisabled() const override;
  bool DoesReadOnlyApply() const override;
  void UpdateDisabledState(bool aNotify) override;
  void UpdateFormOwner(bool aBindToTree, Element* aFormIdElement) override;

  void UpdateBarredFromConstraintValidation();

  ElementInternals* GetElementInternals() const;

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void RestoreFormAssociatedCustomElementState();
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HTMLElement_h
