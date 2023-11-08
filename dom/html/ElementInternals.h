/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ElementInternals_h
#define mozilla_dom_ElementInternals_h

#include "js/TypeDecls.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/ElementInternalsBinding.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/dom/CustomStateSet.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIConstraintValidation.h"
#include "nsIFormControl.h"
#include "nsWrapperCache.h"
#include "AttrArray.h"
#include "nsGkAtoms.h"

#define ARIA_REFLECT_ATTR(method, attr)                             \
  void Get##method(nsAString& aValue) const {                       \
    GetAttr(nsGkAtoms::attr, aValue);                               \
  }                                                                 \
  void Set##method(const nsAString& aValue, ErrorResult& aResult) { \
    aResult = ErrorResult(SetAttr(nsGkAtoms::attr, aValue));        \
  }

class nsINodeList;
class nsGenericHTMLElement;

namespace mozilla::dom {

class DocGroup;
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
  CustomStateSet* States();

  // nsIFormControl
  mozilla::dom::HTMLFieldSetElement* GetFieldSet() override {
    return mFieldSet;
  }
  mozilla::dom::HTMLFormElement* GetForm() const override { return mForm; }
  void SetForm(mozilla::dom::HTMLFormElement* aForm) override;
  void ClearForm(bool aRemoveFromForm, bool aUnbindOrDelete) override;
  NS_IMETHOD Reset() override;
  NS_IMETHOD SubmitNamesValues(mozilla::dom::FormData* aFormData) override;
  int32_t GetParserInsertedControlNumberForStateKey() const override {
    return mControlNumber;
  }

  void SetFieldSet(mozilla::dom::HTMLFieldSetElement* aFieldSet) {
    mFieldSet = aFieldSet;
  }

  const Nullable<OwningFileOrUSVStringOrFormData>& GetFormSubmissionValue()
      const {
    return mSubmissionValue;
  }

  const Nullable<OwningFileOrUSVStringOrFormData>& GetFormState() const {
    return mState;
  }

  void RestoreFormValue(Nullable<OwningFileOrUSVStringOrFormData>&& aValue,
                        Nullable<OwningFileOrUSVStringOrFormData>&& aState);

  const nsCString& GetStateKey() const { return mStateKey; }
  void SetStateKey(nsCString&& key) {
    MOZ_ASSERT(mStateKey.IsEmpty(), "FACE state key should only be set once!");
    mStateKey = key;
  }
  void InitializeControlNumber();

  void UpdateFormOwner();
  void UpdateBarredFromConstraintValidation();

  void Unlink();

  // AccessibilityRole
  ARIA_REFLECT_ATTR(Role, role)

  // AriaAttributes
  ARIA_REFLECT_ATTR(AriaAtomic, aria_atomic)
  ARIA_REFLECT_ATTR(AriaAutoComplete, aria_autocomplete)
  ARIA_REFLECT_ATTR(AriaBusy, aria_busy)
  ARIA_REFLECT_ATTR(AriaChecked, aria_checked)
  ARIA_REFLECT_ATTR(AriaColCount, aria_colcount)
  ARIA_REFLECT_ATTR(AriaColIndex, aria_colindex)
  ARIA_REFLECT_ATTR(AriaColIndexText, aria_colindextext)
  ARIA_REFLECT_ATTR(AriaColSpan, aria_colspan)
  ARIA_REFLECT_ATTR(AriaCurrent, aria_current)
  ARIA_REFLECT_ATTR(AriaDescription, aria_description)
  ARIA_REFLECT_ATTR(AriaDisabled, aria_disabled)
  ARIA_REFLECT_ATTR(AriaExpanded, aria_expanded)
  ARIA_REFLECT_ATTR(AriaHasPopup, aria_haspopup)
  ARIA_REFLECT_ATTR(AriaHidden, aria_hidden)
  ARIA_REFLECT_ATTR(AriaInvalid, aria_invalid)
  ARIA_REFLECT_ATTR(AriaKeyShortcuts, aria_keyshortcuts)
  ARIA_REFLECT_ATTR(AriaLabel, aria_label)
  ARIA_REFLECT_ATTR(AriaLevel, aria_level)
  ARIA_REFLECT_ATTR(AriaLive, aria_live)
  ARIA_REFLECT_ATTR(AriaModal, aria_modal)
  ARIA_REFLECT_ATTR(AriaMultiLine, aria_multiline)
  ARIA_REFLECT_ATTR(AriaMultiSelectable, aria_multiselectable)
  ARIA_REFLECT_ATTR(AriaOrientation, aria_orientation)
  ARIA_REFLECT_ATTR(AriaPlaceholder, aria_placeholder)
  ARIA_REFLECT_ATTR(AriaPosInSet, aria_posinset)
  ARIA_REFLECT_ATTR(AriaPressed, aria_pressed)
  ARIA_REFLECT_ATTR(AriaReadOnly, aria_readonly)
  ARIA_REFLECT_ATTR(AriaRelevant, aria_relevant)
  ARIA_REFLECT_ATTR(AriaRequired, aria_required)
  ARIA_REFLECT_ATTR(AriaRoleDescription, aria_roledescription)
  ARIA_REFLECT_ATTR(AriaRowCount, aria_rowcount)
  ARIA_REFLECT_ATTR(AriaRowIndex, aria_rowindex)
  ARIA_REFLECT_ATTR(AriaRowIndexText, aria_rowindextext)
  ARIA_REFLECT_ATTR(AriaRowSpan, aria_rowspan)
  ARIA_REFLECT_ATTR(AriaSelected, aria_selected)
  ARIA_REFLECT_ATTR(AriaSetSize, aria_setsize)
  ARIA_REFLECT_ATTR(AriaSort, aria_sort)
  ARIA_REFLECT_ATTR(AriaValueMax, aria_valuemax)
  ARIA_REFLECT_ATTR(AriaValueMin, aria_valuemin)
  ARIA_REFLECT_ATTR(AriaValueNow, aria_valuenow)
  ARIA_REFLECT_ATTR(AriaValueText, aria_valuetext)

  void GetAttr(const nsAtom* aName, nsAString& aResult) const;

  nsresult SetAttr(nsAtom* aName, const nsAString& aValue);

  const AttrArray& GetAttrs() const { return mAttrs; }

  DocGroup* GetDocGroup();

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
  // TODO: Bug 1734841 - Figure out how to support autocomplete for
  //       form-associated custom element.
  Nullable<OwningFileOrUSVStringOrFormData> mState;

  // https://html.spec.whatwg.org/#face-validation-message
  nsString mValidationMessage;

  // https://html.spec.whatwg.org/#face-validation-anchor
  RefPtr<nsGenericHTMLElement> mValidationAnchor;

  AttrArray mAttrs;

  // Used to store the key to a form-associated custom element in the current
  // session. Is empty until element has been upgraded.
  nsCString mStateKey;

  RefPtr<CustomStateSet> mCustomStateSet;

  // A number for a form-associated custom element that is unique within its
  // owner document. This is only set to a number for elements inserted into the
  // document by the parser from the network. Otherwise, it is -1.
  int32_t mControlNumber;
};

}  // namespace mozilla::dom

#undef ARIA_REFLECT_ATTR

#endif  // mozilla_dom_ElementInternals_h
