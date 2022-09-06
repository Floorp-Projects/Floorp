/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ElementInternals_h
#define mozilla_dom_ElementInternals_h

#include "js/TypeDecls.h"
#include "mozilla/dom/ElementInternalsBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIConstraintValidation.h"
#include "nsIFormControl.h"
#include "nsWrapperCache.h"

class nsINodeList;
class nsGenericHTMLElement;

namespace mozilla {

class ErrorResult;

namespace dom {

class HTMLElement;
class HTMLFieldSetElement;
class HTMLFormElement;
class ShadowRoot;
class ValidityState;

class ElementInternals final : public nsIFormControl,
                               public nsIConstraintValidation,
                               public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS_AMBIGUOUS(ElementInternals,
                                                        nsIFormControl)

  explicit ElementInternals(HTMLElement* aTarget);

  nsISupports* GetParentObject();

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  ShadowRoot* GetShadowRoot() const;
  void SetFormValue(const Nullable<FileOrUSVStringOrFormData>& aValue,
                    const Optional<Nullable<FileOrUSVStringOrFormData>>& aState,
                    ErrorResult& aRv);
  mozilla::dom::HTMLFormElement* GetForm(ErrorResult& aRv) const;
  void SetValidity(const ValidityStateFlags& aFlags,
                   const Optional<nsAString>& aMessage,
                   const Optional<NonNull<nsGenericHTMLElement>>& aAnchor,
                   ErrorResult& aRv);
  bool GetWillValidate(ErrorResult& aRv) const;
  ValidityState* GetValidity(ErrorResult& aRv);
  void GetValidationMessage(nsAString& aValidationMessage,
                            ErrorResult& aRv) const;
  bool CheckValidity(ErrorResult& aRv);
  bool ReportValidity(ErrorResult& aRv);
  already_AddRefed<nsINodeList> GetLabels(ErrorResult& aRv) const;
  nsGenericHTMLElement* GetValidationAnchor(ErrorResult& aRv) const;

  // nsIFormControl
  mozilla::dom::HTMLFieldSetElement* GetFieldSet() override {
    return mFieldSet;
  }
  mozilla::dom::HTMLFormElement* GetForm() const override { return mForm; }
  void SetForm(mozilla::dom::HTMLFormElement* aForm) override;
  void ClearForm(bool aRemoveFromForm, bool aUnbindOrDelete) override;
  NS_IMETHOD Reset() override;
  NS_IMETHOD SubmitNamesValues(mozilla::dom::FormData* aFormData) override;
  bool AllowDrop() override { return true; }

  void SetFieldSet(mozilla::dom::HTMLFieldSetElement* aFieldSet) {
    mFieldSet = aFieldSet;
  }

  void UpdateFormOwner();
  void UpdateBarredFromConstraintValidation();

  void Unlink();

 private:
  ~ElementInternals() = default;

  // It's a target element which is a custom element.
  RefPtr<HTMLElement> mTarget;

  // The form that contains the target element.
  // It's safe to use raw pointer because it will be reset via
  // CustomElementData::Unlink when mTarget is released or unlinked.
  HTMLFormElement* mForm;

  // This is a pointer to the target element's closest fieldset parent if any.
  // It's safe to use raw pointer because it will be reset via
  // CustomElementData::Unlink when mTarget is released or unlinked.
  HTMLFieldSetElement* mFieldSet;

  // https://html.spec.whatwg.org/#face-submission-value
  Nullable<OwningFileOrUSVStringOrFormData> mSubmissionValue;

  // https://html.spec.whatwg.org/#face-state
  // TODO: Bug 1734841 - Figure out how to support form restoration or
  //       autocomplete for form-associated custom element
  Nullable<OwningFileOrUSVStringOrFormData> mState;

  // https://html.spec.whatwg.org/#face-validation-message
  nsString mValidationMessage;

  // https://html.spec.whatwg.org/#face-validation-anchor
  RefPtr<nsGenericHTMLElement> mValidationAnchor;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ElementInternals_h
