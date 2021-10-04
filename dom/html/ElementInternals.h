/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ElementInternals_h
#define mozilla_dom_ElementInternals_h

#include "js/TypeDecls.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIFormControl.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class HTMLElement;
class HTMLFieldSetElement;
class HTMLFormElement;
class ShadowRoot;

class ElementInternals final : public nsIFormControl, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ElementInternals)

  explicit ElementInternals(HTMLElement* aTarget);

  nsISupports* GetParentObject();

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  ShadowRoot* GetShadowRoot() const;

  // nsIFormControl
  mozilla::dom::HTMLFieldSetElement* GetFieldSet() override {
    return mFieldSet;
  }
  mozilla::dom::HTMLFormElement* GetForm() const override { return mForm; }
  void SetForm(mozilla::dom::HTMLFormElement* aForm) override;
  void ClearForm(bool aRemoveFromForm, bool aUnbindOrDelete) override;
  NS_IMETHOD Reset() override { return NS_OK; }
  NS_IMETHOD SubmitNamesValues(mozilla::dom::FormData* aFormData) override;
  bool AllowDrop() override { return true; }

  void SetFieldSet(mozilla::dom::HTMLFieldSetElement* aFieldSet) {
    mFieldSet = aFieldSet;
  }

  void UpdateFormOwner();

  void Unlink();

 private:
  ~ElementInternals() = default;

  // It's a target element which is a custom element.
  // It's safe to use raw pointer because it will be reset via
  // CustomElementData::Unlink when mTarget is released or unlinked.
  HTMLElement* mTarget;

  // The form that contains the target element.
  // It's safe to use raw pointer because it will be reset via
  // CustomElementData::Unlink when mTarget is released or unlinked.
  HTMLFormElement* mForm;

  // This is a pointer to the target element's closest fieldset parent if any.
  // It's safe to use raw pointer because it will be reset via
  // CustomElementData::Unlink when mTarget is released or unlinked.
  HTMLFieldSetElement* mFieldSet;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ElementInternals_h
