/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLButtonElement_h
#define mozilla_dom_HTMLButtonElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsIConstraintValidation.h"

namespace mozilla {
class EventChainPostVisitor;
class EventChainPreVisitor;
namespace dom {

class HTMLButtonElement final : public nsGenericHTMLFormElementWithState,
                                public nsIConstraintValidation {
 public:
  using nsIConstraintValidation::GetValidationMessage;

  explicit HTMLButtonElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
      FromParser aFromParser = NOT_FROM_PARSER);

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLButtonElement,
                                           nsGenericHTMLFormElementWithState)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  virtual int32_t TabIndexDefault() override;

  NS_IMPL_FROMNODE_HTML_WITH_TAG(HTMLButtonElement, button)

  // Element
  virtual bool IsInteractiveHTMLContent(bool aIgnoreTabindex) const override {
    return true;
  }

  // overriden nsIFormControl methods
  NS_IMETHOD Reset() override;
  NS_IMETHOD SubmitNamesValues(HTMLFormSubmission* aFormSubmission) override;
  NS_IMETHOD SaveState() override;
  bool RestoreState(PresState* aState) override;
  virtual bool IsDisabledForEvents(WidgetEvent* aEvent) override;

  virtual void FieldSetDisabledChanged(bool aNotify) override;

  // EventTarget
  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual nsresult PostHandleEvent(EventChainPostVisitor& aVisitor) override;

  // nsINode
  virtual nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) override;

  // nsIContent
  virtual nsresult BindToTree(Document* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent) override;
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true) override;
  virtual void DoneCreatingElement() override;

  void UpdateBarredFromConstraintValidation();
  // Element
  EventStates IntrinsicState() const override;
  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify) override;
  /**
   * Called when an attribute has just been changed
   */
  virtual nsresult AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify) override;
  virtual bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                              const nsAString& aValue,
                              nsIPrincipal* aMaybeScriptedPrincipal,
                              nsAttrValue& aResult) override;

  // nsGenericHTMLElement
  virtual bool IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                               int32_t* aTabIndex) override;

  // WebIDL
  bool Autofocus() const { return GetBoolAttr(nsGkAtoms::autofocus); }
  void SetAutofocus(bool aAutofocus, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::autofocus, aAutofocus, aError);
  }
  bool Disabled() const { return GetBoolAttr(nsGkAtoms::disabled); }
  void SetDisabled(bool aDisabled, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::disabled, aDisabled, aError);
  }
  // nsGenericHTMLFormElement::GetForm is fine.
  using nsGenericHTMLFormElement::GetForm;
  // GetFormAction implemented in superclass
  void SetFormAction(const nsAString& aFormAction, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::formaction, aFormAction, aRv);
  }
  void GetFormEnctype(nsAString& aFormEncType);
  void SetFormEnctype(const nsAString& aFormEnctype, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::formenctype, aFormEnctype, aRv);
  }
  void GetFormMethod(nsAString& aFormMethod);
  void SetFormMethod(const nsAString& aFormMethod, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::formmethod, aFormMethod, aRv);
  }
  bool FormNoValidate() const { return GetBoolAttr(nsGkAtoms::formnovalidate); }
  void SetFormNoValidate(bool aFormNoValidate, ErrorResult& aError) {
    SetHTMLBoolAttr(nsGkAtoms::formnovalidate, aFormNoValidate, aError);
  }
  void GetFormTarget(DOMString& aFormTarget) {
    GetHTMLAttr(nsGkAtoms::formtarget, aFormTarget);
  }
  void SetFormTarget(const nsAString& aFormTarget, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::formtarget, aFormTarget, aRv);
  }
  void GetName(DOMString& aName) { GetHTMLAttr(nsGkAtoms::name, aName); }
  void SetName(const nsAString& aName, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::name, aName, aRv);
  }
  void GetType(nsAString& aType);
  void SetType(const nsAString& aType, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::type, aType, aRv);
  }
  void GetValue(DOMString& aValue) { GetHTMLAttr(nsGkAtoms::value, aValue); }
  void SetValue(const nsAString& aValue, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::value, aValue, aRv);
  }

  // Override SetCustomValidity so we update our state properly when it's called
  // via bindings.
  void SetCustomValidity(const nsAString& aError);

 protected:
  virtual ~HTMLButtonElement();

  bool mDisabledChanged;
  bool mInInternalActivate;
  bool mInhibitStateRestoration;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_HTMLButtonElement_h
