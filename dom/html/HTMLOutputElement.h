/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLOutputElement_h
#define mozilla_dom_HTMLOutputElement_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/ConstraintValidation.h"
#include "nsGenericHTMLElement.h"
#include "nsStubMutationObserver.h"

namespace mozilla::dom {

class FormData;

class HTMLOutputElement final : public nsGenericHTMLFormControlElement,
                                public nsStubMutationObserver,
                                public ConstraintValidation {
 public:
  using ConstraintValidation::GetValidationMessage;

  explicit HTMLOutputElement(
      already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
      FromParser aFromParser = NOT_FROM_PARSER);

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIFormControl
  NS_IMETHOD Reset() override;
  // The output element is not submittable.
  NS_IMETHOD SubmitNamesValues(FormData* aFormData) override { return NS_OK; }

  nsresult Clone(dom::NodeInfo*, nsINode** aResult) const override;

  bool ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                      const nsAString& aValue,
                      nsIPrincipal* aMaybeScriptedPrincipal,
                      nsAttrValue& aResult) override;

  void DoneAddingChildren(bool aHaveNotified) override;

  // This function is called when a callback function from nsIMutationObserver
  // has to be used to update the defaultValue attribute.
  void DescendantsChanged();

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(HTMLOutputElement,
                                           nsGenericHTMLFormControlElement)

  JSObject* WrapNode(JSContext*, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  nsDOMTokenList* HtmlFor();

  void GetName(nsAString& aName) { GetHTMLAttr(nsGkAtoms::name, aName); }

  void SetName(const nsAString& aName, ErrorResult& aRv) {
    SetHTMLAttr(nsGkAtoms::name, aName, aRv);
  }

  void GetType(nsAString& aType) { aType.AssignLiteral("output"); }

  void GetDefaultValue(nsAString& aDefaultValue) {
    aDefaultValue = mDefaultValue;
  }

  void SetDefaultValue(const nsAString& aDefaultValue, ErrorResult& aRv);

  void GetValue(nsAString& aValue) const;
  void SetValue(const nsAString& aValue, ErrorResult& aRv);

  // nsIConstraintValidation::WillValidate is fine.
  // nsIConstraintValidation::Validity() is fine.
  // nsIConstraintValidation::GetValidationMessage() is fine.
  // nsIConstraintValidation::CheckValidity() is fine.
  void SetCustomValidity(const nsAString& aError);

 protected:
  virtual ~HTMLOutputElement();

  enum ValueModeFlag { eModeDefault, eModeValue };

  ValueModeFlag mValueModeFlag;
  bool mIsDoneAddingChildren;
  nsString mDefaultValue;
  RefPtr<nsDOMTokenList> mTokenList;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_HTMLOutputElement_h
