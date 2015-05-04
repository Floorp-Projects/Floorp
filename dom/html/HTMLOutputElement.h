/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLOutputElement_h
#define mozilla_dom_HTMLOutputElement_h

#include "mozilla/Attributes.h"
#include "nsGenericHTMLElement.h"
#include "nsStubMutationObserver.h"
#include "nsIConstraintValidation.h"

namespace mozilla {
namespace dom {

class HTMLOutputElement final : public nsGenericHTMLFormElement,
                                public nsStubMutationObserver,
                                public nsIConstraintValidation
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  explicit HTMLOutputElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                             FromParser aFromParser = NOT_FROM_PARSER);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIFormControl
  NS_IMETHOD_(uint32_t) GetType() const override { return NS_FORM_OUTPUT; }
  NS_IMETHOD Reset() override;
  NS_IMETHOD SubmitNamesValues(nsFormSubmission* aFormSubmission) override;

  virtual bool IsDisabled() const override { return false; }

  nsresult Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult) const override;

  bool ParseAttribute(int32_t aNamespaceID, nsIAtom* aAttribute,
                        const nsAString& aValue, nsAttrValue& aResult) override;

  virtual void DoneAddingChildren(bool aHaveNotified) override;

  EventStates IntrinsicState() const override;

  virtual nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               bool aCompileEventHandlers) override;

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

  virtual JSObject* WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  nsDOMSettableTokenList* HtmlFor();
  // nsGenericHTMLFormElement::GetForm is fine.
  void GetName(nsAString& aName)
  {
    GetHTMLAttr(nsGkAtoms::name, aName);
  }

  void SetName(const nsAString& aName, ErrorResult& aRv)
  {
    SetHTMLAttr(nsGkAtoms::name, aName, aRv);
  }

  void GetType(nsAString& aType)
  {
    aType.AssignLiteral("output");
  }

  void GetDefaultValue(nsAString& aDefaultValue)
  {
    aDefaultValue = mDefaultValue;
  }

  void SetDefaultValue(const nsAString& aDefaultValue, ErrorResult& aRv);

  void GetValue(nsAString& aValue);
  void SetValue(const nsAString& aValue, ErrorResult& aRv);

  // nsIConstraintValidation::WillValidate is fine.
  // nsIConstraintValidation::Validity() is fine.
  // nsIConstraintValidation::GetValidationMessage() is fine.
  // nsIConstraintValidation::CheckValidity() is fine.
  void SetCustomValidity(const nsAString& aError);

protected:
  virtual ~HTMLOutputElement();

  enum ValueModeFlag {
    eModeDefault,
    eModeValue
  };

  ValueModeFlag                     mValueModeFlag;
  bool                              mIsDoneAddingChildren;
  nsString                          mDefaultValue;
  nsRefPtr<nsDOMSettableTokenList>  mTokenList;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HTMLOutputElement_h
