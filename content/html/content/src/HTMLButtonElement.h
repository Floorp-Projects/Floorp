/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLButtonElement_h
#define mozilla_dom_HTMLButtonElement_h

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIConstraintValidation.h"

namespace mozilla {
namespace dom {

class HTMLButtonElement : public nsGenericHTMLFormElement,
                          public nsIDOMHTMLButtonElement,
                          public nsIConstraintValidation
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  HTMLButtonElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                      FromParser aFromParser = NOT_FROM_PARSER);
  virtual ~HTMLButtonElement();

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLButtonElement,
                                           nsGenericHTMLFormElement)

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  virtual int32_t TabIndexDefault() MOZ_OVERRIDE;

  // nsIDOMHTMLButtonElement
  NS_DECL_NSIDOMHTMLBUTTONELEMENT

  // overriden nsIFormControl methods
  NS_IMETHOD_(uint32_t) GetType() const { return mType; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);
  NS_IMETHOD SaveState();
  bool RestoreState(nsPresState* aState);
  virtual bool IsDisabledForEvents(uint32_t aMessage);

  // nsIDOMEventTarget
  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor);
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor);

  // nsINode
  virtual nsresult Clone(nsINodeInfo* aNodeInfo, nsINode** aResult) const;
  virtual nsIDOMNode* AsDOMNode() { return this; }
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // nsIContent
  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                              nsIContent* aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  virtual void DoneCreatingElement();

  // Element
  nsEventStates IntrinsicState() const;
  /**
   * Called when an attribute is about to be changed
   */
  virtual nsresult BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                 const nsAttrValueOrString* aValue,
                                 bool aNotify);
  /**
   * Called when an attribute has just been changed
   */
  nsresult AfterSetAttr(int32_t aNamespaceID, nsIAtom* aName,
                        const nsAttrValue* aValue, bool aNotify);
  virtual bool ParseAttribute(int32_t aNamespaceID,
                              nsIAtom* aAttribute,
                              const nsAString& aValue,
                              nsAttrValue& aResult);

  // nsGenericHTMLElement
  virtual bool IsHTMLFocusable(bool aWithMouse,
                               bool* aIsFocusable,
                               int32_t* aTabIndex);

  // WebIDL
  bool Autofocus() const
  {
    return GetBoolAttr(nsGkAtoms::autofocus);
  }
  void SetAutofocus(bool aAutofocus, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::autofocus, aAutofocus, aError);
  }
  bool Disabled() const
  {
    return GetBoolAttr(nsGkAtoms::disabled);
  }
  void SetDisabled(bool aDisabled, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::disabled, aDisabled, aError);
  }
  // nsGenericHTMLFormElement::GetForm is fine.
  using nsGenericHTMLFormElement::GetForm;
  // XPCOM GetFormAction is fine.
  void SetFormAction(const nsAString& aFormAction, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::formaction, aFormAction, aRv);
  }
  // XPCOM GetFormEnctype is fine.
  void SetFormEnctype(const nsAString& aFormEnctype, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::formenctype, aFormEnctype, aRv);
  }
  // XPCOM GetFormMethod is fine.
  void SetFormMethod(const nsAString& aFormMethod, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::formmethod, aFormMethod, aRv);
  }
  bool FormNoValidate() const
  {
    return GetBoolAttr(nsGkAtoms::formnovalidate);
  }
  void SetFormNoValidate(bool aFormNoValidate, ErrorResult& aError)
  {
    SetHTMLBoolAttr(nsGkAtoms::formnovalidate, aFormNoValidate, aError);
  }
  // XPCOM GetFormTarget is fine.
  void SetFormTarget(const nsAString& aFormTarget, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::formtarget, aFormTarget, aRv);
  }
  // XPCOM GetName is fine.
  void SetName(const nsAString& aName, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::name, aName, aRv);
  }
  // XPCOM GetType is fine.
  void SetType(const nsAString& aType, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::type, aType, aRv);
  }
  // XPCOM GetValue is fine.
  void SetValue(const nsAString& aValue, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::value, aValue, aRv);
  }

  // nsIConstraintValidation::WillValidate is fine.
  // nsIConstraintValidation::Validity() is fine.
  // nsIConstraintValidation::GetValidationMessage() is fine.
  // nsIConstraintValidation::CheckValidity() is fine.
  using nsIConstraintValidation::CheckValidity;
  // nsIConstraintValidation::SetCustomValidity() is fine.

protected:
  uint8_t mType;
  bool mDisabledChanged;
  bool mInInternalActivate;
  bool mInhibitStateRestoration;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLButtonElement_h
