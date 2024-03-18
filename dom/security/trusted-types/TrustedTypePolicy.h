/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SECURITY_TRUSTED_TYPES_TRUSTEDTYPEPOLICY_H_
#define DOM_SECURITY_TRUSTED_TYPES_TRUSTEDTYPEPOLICY_H_

#include "js/TypeDecls.h"
#include "js/Value.h"
#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/TrustedHTML.h"
#include "mozilla/dom/TrustedScript.h"
#include "mozilla/dom/TrustedScriptURL.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

class DOMString;
class TrustedTypePolicyFactory;

// https://w3c.github.io/trusted-types/dist/spec/#trusted-type-policy
class TrustedTypePolicy : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(TrustedTypePolicy)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(TrustedTypePolicy)

  struct Options {
    RefPtr<CreateHTMLCallback> mCreateHTMLCallback;
    RefPtr<CreateScriptCallback> mCreateScriptCallback;
    RefPtr<CreateScriptURLCallback> mCreateScriptURLCallback;
  };

  TrustedTypePolicy(TrustedTypePolicyFactory* aParentObject,
                    const nsAString& aName, Options&& aOptions);

  // Required for Web IDL binding.
  TrustedTypePolicyFactory* GetParentObject() const { return mParentObject; }

  // Required for Web IDL binding.
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // https://w3c.github.io/trusted-types/dist/spec/#trustedtypepolicy-name
  void GetName(DOMString& aResult) const;

  // https://w3c.github.io/trusted-types/dist/spec/#dom-trustedtypepolicy-createhtml
  MOZ_CAN_RUN_SCRIPT UniquePtr<TrustedHTML> CreateHTML(
      JSContext* aJSContext, const nsAString& aInput,
      const Sequence<JS::Value>& aArguments, ErrorResult& aErrorResult) const;

  // https://w3c.github.io/trusted-types/dist/spec/#dom-trustedtypepolicy-createscript
  MOZ_CAN_RUN_SCRIPT UniquePtr<TrustedScript> CreateScript(
      JSContext* aJSContext, const nsAString& aInput,
      const Sequence<JS::Value>& aArguments, ErrorResult& aErrorResult) const;

  // https://w3c.github.io/trusted-types/dist/spec/#dom-trustedtypepolicy-createscripturl
  MOZ_CAN_RUN_SCRIPT UniquePtr<TrustedScriptURL> CreateScriptURL(
      JSContext* aJSContext, const nsAString& aInput,
      const Sequence<JS::Value>& aArguments, ErrorResult& aErrorResult) const;

 private:
  // Required because this class is ref-counted.
  virtual ~TrustedTypePolicy() = default;

  // https://w3c.github.io/trusted-types/dist/spec/#abstract-opdef-create-a-trusted-type
  template <typename T, typename CallbackObject>
  MOZ_CAN_RUN_SCRIPT UniquePtr<T> CreateTrustedType(
      const RefPtr<CallbackObject>& aCallbackObject, const nsAString& aValue,
      const Sequence<JS::Value>& aArguments, ErrorResult& aErrorResult) const;

  // https://w3c.github.io/trusted-types/dist/spec/#abstract-opdef-get-trusted-type-policy-value
  //
  // @param aResult may become void.
  template <typename CallbackObject>
  MOZ_CAN_RUN_SCRIPT void DetermineTrustedPolicyValue(
      const RefPtr<CallbackObject>& aCallbackObject, const nsAString& aValue,
      const Sequence<JS::Value>& aArguments, bool aThrowIfMissing,
      ErrorResult& aErrorResult, nsAString& aResult) const;
  RefPtr<TrustedTypePolicyFactory> mParentObject;

  const nsString mName;

  Options mOptions;
};

}  // namespace mozilla::dom

#endif  // DOM_SECURITY_TRUSTED_TYPES_TRUSTEDTYPEPOLICY_H_
