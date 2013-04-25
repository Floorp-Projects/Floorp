/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLOutputElement_h
#define mozilla_dom_HTMLOutputElement_h

#include "nsGenericHTMLElement.h"
#include "nsIDOMHTMLOutputElement.h"
#include "nsStubMutationObserver.h"
#include "nsIConstraintValidation.h"

namespace mozilla {
namespace dom {

class HTMLOutputElement : public nsGenericHTMLFormElement,
                          public nsIDOMHTMLOutputElement,
                          public nsStubMutationObserver,
                          public nsIConstraintValidation
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  HTMLOutputElement(already_AddRefed<nsINodeInfo> aNodeInfo);
  virtual ~HTMLOutputElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  // nsIDOMHTMLOutputElement
  NS_DECL_NSIDOMHTMLOUTPUTELEMENT

  // nsIFormControl
  NS_IMETHOD_(uint32_t) GetType() const { return NS_FORM_OUTPUT; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission);

  virtual bool IsDisabled() const { return false; }

  nsresult Clone(nsINodeInfo* aNodeInfo, nsINode** aResult) const;

  bool ParseAttribute(int32_t aNamespaceID, nsIAtom* aAttribute,
                        const nsAString& aValue, nsAttrValue& aResult);

  nsEventStates IntrinsicState() const;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               bool aCompileEventHandlers);

  // This function is called when a callback function from nsIMutationObserver
  // has to be used to update the defaultValue attribute.
  void DescendantsChanged();

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLOutputElement,
                                           nsGenericHTMLFormElement)

  virtual nsIDOMNode* AsDOMNode() { return this; }
  virtual JSObject* WrapNode(JSContext* aCx,
                             JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // WebIDL
  nsDOMSettableTokenList* HtmlFor();
  // nsGenericHTMLFormElement::GetForm is fine.
  using nsGenericHTMLFormElement::GetForm;
  // XPCOM GetName is fine.
  void SetName(const nsAString& aName, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::name, aName, aRv);
  }

  // XPCOM GetType is fine.
  // XPCOM GetDefaultValue is fine.
  void SetDefaultValue(const nsAString& aDefaultValue, ErrorResult& aRv)
  {
    aRv = SetDefaultValue(aDefaultValue);
  }
  // XPCOM GetValue is fine.
  void SetValue(const nsAString& aValue, ErrorResult& aRv)
  {
    aRv = SetValue(aValue);
  }

  // nsIConstraintValidation::WillValidate is fine.
  // nsIConstraintValidation::Validity() is fine.
  // nsIConstraintValidation::GetValidationMessage() is fine.
  // nsIConstraintValidation::CheckValidity() is fine.
  using nsIConstraintValidation::CheckValidity;
  // nsIConstraintValidation::SetCustomValidity() is fine.

protected:
  enum ValueModeFlag {
    eModeDefault,
    eModeValue
  };

  ValueModeFlag                     mValueModeFlag;
  nsString                          mDefaultValue;
  nsRefPtr<nsDOMSettableTokenList>  mTokenList;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLOutputElement_h
