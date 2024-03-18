/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TrustedTypePolicy.h"

#include "mozilla/dom/DOMString.h"
#include "mozilla/dom/TrustedTypePolicyFactory.h"
#include "mozilla/dom/TrustedTypesBinding.h"

#include <utility>

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TrustedTypePolicy, mParentObject,
                                      mOptions.mCreateHTMLCallback,
                                      mOptions.mCreateScriptCallback,
                                      mOptions.mCreateScriptURLCallback)

TrustedTypePolicy::TrustedTypePolicy(TrustedTypePolicyFactory* aParentObject,
                                     const nsAString& aName, Options&& aOptions)
    : mParentObject{aParentObject},
      mName{aName},
      mOptions{std::move(aOptions)} {}

JSObject* TrustedTypePolicy::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return TrustedTypePolicy_Binding::Wrap(aCx, this, aGivenProto);
}

void TrustedTypePolicy::GetName(DOMString& aResult) const {
  aResult.SetKnownLiveString(mName);
}

#define IMPL_CREATE_TRUSTED_TYPE(_trustedTypeSuffix)                         \
  UniquePtr<Trusted##_trustedTypeSuffix>                                     \
      TrustedTypePolicy::Create##_trustedTypeSuffix(                         \
          JSContext* aJSContext, const nsAString& aInput,                    \
          const Sequence<JS::Value>& aArguments, ErrorResult& aErrorResult)  \
          const {                                                            \
    /* Invoking the callback could delete the policy and hence the callback. \
     * Hence keep a strong reference to it on the stack.                     \
     */                                                                      \
    RefPtr<Create##_trustedTypeSuffix##Callback> callbackObject =            \
        mOptions.mCreate##_trustedTypeSuffix##Callback;                      \
                                                                             \
    return CreateTrustedType<Trusted##_trustedTypeSuffix>(                   \
        callbackObject, aInput, aArguments, aErrorResult);                   \
  }

IMPL_CREATE_TRUSTED_TYPE(HTML)
IMPL_CREATE_TRUSTED_TYPE(Script)
IMPL_CREATE_TRUSTED_TYPE(ScriptURL)

template <typename T, typename CallbackObject>
UniquePtr<T> TrustedTypePolicy::CreateTrustedType(
    const RefPtr<CallbackObject>& aCallbackObject, const nsAString& aValue,
    const Sequence<JS::Value>& aArguments, ErrorResult& aErrorResult) const {
  nsString policyValue;
  DetermineTrustedPolicyValue(aCallbackObject, aValue, aArguments, true,
                              aErrorResult, policyValue);

  if (aErrorResult.Failed()) {
    return nullptr;
  }

  UniquePtr<T> trustedObject = MakeUnique<T>(std::move(policyValue));

  // TODO: add special handling for `TrustedScript` when default policy support
  // is added.

  return trustedObject;
}

template <typename CallbackObject>
void TrustedTypePolicy::DetermineTrustedPolicyValue(
    const RefPtr<CallbackObject>& aCallbackObject, const nsAString& aValue,
    const Sequence<JS::Value>& aArguments, bool aThrowIfMissing,
    ErrorResult& aErrorResult, nsAString& aResult) const {
  if (!aCallbackObject) {
    // The spec lacks a definition for stringifying null, see
    // <https://github.com/w3c/trusted-types/issues/469>.
    aResult = EmptyString();

    if (aThrowIfMissing) {
      aErrorResult.ThrowTypeError("Function missing.");
    }

    return;
  }

  nsString callbackResult;
  aCallbackObject->Call(aValue, aArguments, callbackResult, aErrorResult,
                        nullptr, CallbackObject::eRethrowExceptions);

  if (!aErrorResult.Failed()) {
    aResult = std::move(callbackResult);
  }
}

}  // namespace mozilla::dom
