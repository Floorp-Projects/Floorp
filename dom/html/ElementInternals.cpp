/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ElementInternals.h"

#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/ElementInternalsBinding.h"
#include "mozilla/dom/FormData.h"
#include "mozilla/dom/HTMLElement.h"
#include "mozilla/dom/HTMLFieldSetElement.h"
#include "mozilla/dom/ShadowRoot.h"
#include "nsContentUtils.h"
#include "nsGenericHTMLElement.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ElementInternals, mSubmissionValue,
                                      mState)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ElementInternals)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ElementInternals)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ElementInternals)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIFormControl)
NS_INTERFACE_MAP_END

ElementInternals::ElementInternals(HTMLElement* aTarget)
    : nsIFormControl(FormControlType::FormAssociatedCustomElement),
      mTarget(aTarget),
      mForm(nullptr),
      mFieldSet(nullptr) {}

nsISupports* ElementInternals::GetParentObject() { return ToSupports(mTarget); }

JSObject* ElementInternals::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return ElementInternals_Binding::Wrap(aCx, this, aGivenProto);
}

// https://html.spec.whatwg.org/#dom-elementinternals-shadowroot
ShadowRoot* ElementInternals::GetShadowRoot() const {
  MOZ_ASSERT(mTarget);

  ShadowRoot* shadowRoot = mTarget->GetShadowRoot();
  if (shadowRoot && !shadowRoot->IsAvailableToElementInternals()) {
    return nullptr;
  }

  return shadowRoot;
}

// https://html.spec.whatwg.org/commit-snapshots/912a3fe1f29649ccf8229de56f604b3c07ffd242/#dom-elementinternals-setformvalue
void ElementInternals::SetFormValue(
    const Nullable<FileOrUSVStringOrFormData>& aValue,
    const Optional<Nullable<FileOrUSVStringOrFormData>>& aState,
    ErrorResult& aRv) {
  /**
   * 1. Let element be this's target element.
   * 2. If element is not a form-associated custom element, then throw a
   *    "NotSupportedError" DOMException.
   */
  if (!mTarget || !mTarget->IsFormAssociatedElement()) {
    aRv.ThrowNotSupportedError(
        "Target element is not a form-associated custom element");
    return;
  }

  /**
   * 3. Set target element's submission value to value if value is not a
   *    FormData object, or to a clone of the entry list associated with value
   *    otherwise.
   */
  mSubmissionValue.SetNull();
  if (!aValue.IsNull()) {
    const FileOrUSVStringOrFormData& value = aValue.Value();
    OwningFileOrUSVStringOrFormData& owningValue = mSubmissionValue.SetValue();
    if (value.IsFormData()) {
      owningValue.SetAsFormData() = value.GetAsFormData().Clone();
    } else if (value.IsFile()) {
      owningValue.SetAsFile() = &value.GetAsFile();
    } else {
      owningValue.SetAsUSVString() = value.GetAsUSVString();
    }
  }

  /**
   * 4. If the state argument of the function is omitted, set element's state to
   *    its submission value.
   */
  if (!aState.WasPassed()) {
    mState = mSubmissionValue;
    return;
  }

  /**
   * 5. Otherwise, if state is a FormData object, set element's state to clone
   *    of the entry list associated with state.
   * 6. Otherwise, set element's state to state.
   */
  mState.SetNull();
  if (!aState.Value().IsNull()) {
    const FileOrUSVStringOrFormData& state = aState.Value().Value();
    OwningFileOrUSVStringOrFormData& owningState = mState.SetValue();
    if (state.IsFormData()) {
      owningState.SetAsFormData() = state.GetAsFormData().Clone();
    } else if (state.IsFile()) {
      owningState.SetAsFile() = &state.GetAsFile();
    } else {
      owningState.SetAsUSVString() = state.GetAsUSVString();
    }
  }
}

// https://html.spec.whatwg.org/#dom-elementinternals-form
HTMLFormElement* ElementInternals::GetForm(ErrorResult& aRv) const {
  if (!mTarget || !mTarget->IsFormAssociatedElement()) {
    aRv.ThrowNotSupportedError(
        "Target element is not a form-associated custom element");
    return nullptr;
  }
  return GetForm();
}

// https://html.spec.whatwg.org/#dom-elementinternals-labels
already_AddRefed<nsINodeList> ElementInternals::GetLabels(
    ErrorResult& aRv) const {
  if (!mTarget || !mTarget->IsFormAssociatedElement()) {
    aRv.ThrowNotSupportedError(
        "Target element is not a form-associated custom element");
    return nullptr;
  }
  return mTarget->Labels();
}

void ElementInternals::SetForm(HTMLFormElement* aForm) { mForm = aForm; }

void ElementInternals::ClearForm(bool aRemoveFromForm, bool aUnbindOrDelete) {
  if (mTarget) {
    mTarget->ClearForm(aRemoveFromForm, aUnbindOrDelete);
  }
}

NS_IMETHODIMP ElementInternals::Reset() {
  if (mTarget) {
    MOZ_ASSERT(mTarget->IsFormAssociatedElement());
    nsContentUtils::EnqueueLifecycleCallback(ElementCallbackType::eFormReset,
                                             mTarget, {});
  }
  return NS_OK;
}

NS_IMETHODIMP ElementInternals::SubmitNamesValues(FormData* aFormData) {
  if (!mTarget) {
    return NS_ERROR_UNEXPECTED;
  }

  MOZ_ASSERT(mTarget->IsFormAssociatedElement());

  // https://html.spec.whatwg.org/#face-entry-construction
  if (!mSubmissionValue.IsNull()) {
    if (mSubmissionValue.Value().IsFormData()) {
      aFormData->Append(mSubmissionValue.Value().GetAsFormData());
      return NS_OK;
    }

    // Get the name
    nsAutoString name;
    if (!mTarget->GetAttr(nsGkAtoms::name, name) || name.IsEmpty()) {
      return NS_OK;
    }

    if (mSubmissionValue.Value().IsUSVString()) {
      return aFormData->AddNameValuePair(
          name, mSubmissionValue.Value().GetAsUSVString());
    }

    return aFormData->AddNameBlobPair(name,
                                      mSubmissionValue.Value().GetAsFile());
  }
  return NS_OK;
}

void ElementInternals::UpdateFormOwner() {
  if (mTarget) {
    mTarget->UpdateFormOwner();
  }
}

void ElementInternals::Unlink() {
  if (mForm) {
    // Don't notify, since we're being destroyed in any case.
    ClearForm(true, true);
  }
  if (mFieldSet) {
    mFieldSet->RemoveElement(mTarget);
  }
  mTarget = nullptr;
}

}  // namespace mozilla::dom
