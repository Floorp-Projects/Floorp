/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TrustedTypePolicyFactory.h"

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/TrustedTypePolicy.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TrustedTypePolicyFactory, mGlobalObject)

JSObject* TrustedTypePolicyFactory::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return TrustedTypePolicyFactory_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<TrustedTypePolicy> TrustedTypePolicyFactory::CreatePolicy(
    const nsAString& aPolicyName,
    const TrustedTypePolicyOptions& aPolicyOptions) {
  // TODO: add CSP support.

  // TODO: add default policy support; this requires accessing the default
  //       policy on the C++ side, hence already now ref-counting policy
  //       objects.

  TrustedTypePolicy::Options options;

  if (aPolicyOptions.mCreateHTML.WasPassed()) {
    options.mCreateHTMLCallback = &aPolicyOptions.mCreateHTML.Value();
  }

  if (aPolicyOptions.mCreateScript.WasPassed()) {
    options.mCreateScriptCallback = &aPolicyOptions.mCreateScript.Value();
  }

  if (aPolicyOptions.mCreateScriptURL.WasPassed()) {
    options.mCreateScriptURLCallback = &aPolicyOptions.mCreateScriptURL.Value();
  }

  RefPtr<TrustedTypePolicy> policy =
      MakeRefPtr<TrustedTypePolicy>(this, aPolicyName, std::move(options));

  mCreatedPolicyNames.AppendElement(aPolicyName);

  return policy.forget();
}

#define IS_TRUSTED_TYPE_IMPL(_trustedTypeSuffix)                                                                                                                         \
  bool TrustedTypePolicyFactory::Is##_trustedTypeSuffix(                                                                                                                 \
      JSContext*, const JS::Handle<JS::Value>& aValue) const {                                                                                                           \
    /**                                                                                                                                                                  \
     * No need to check the internal slot.                                                                                                                               \
     * Ensured by the corresponding test:                                                                                                                                \
     * <https://searchfox.org/mozilla-central/rev/b60cb73160843adb5a5a3ec8058e75a69b46acf7/testing/web-platform/tests/trusted-types/TrustedTypePolicyFactory-isXXX.html> \
     */                                                                                                                                                                  \
    return aValue.isObject() &&                                                                                                                                          \
           IS_INSTANCE_OF(Trusted##_trustedTypeSuffix, &aValue.toObject());                                                                                              \
  }

IS_TRUSTED_TYPE_IMPL(HTML);
IS_TRUSTED_TYPE_IMPL(Script);
IS_TRUSTED_TYPE_IMPL(ScriptURL);

UniquePtr<TrustedHTML> TrustedTypePolicyFactory::EmptyHTML() {
  // Preserving the wrapper ensures:
  // ```
  //  const e = trustedTypes.emptyHTML;
  //  e === trustedTypes.emptyHTML;
  // ```
  // which comes with the cost of keeping the factory, one per global, alive.
  // An additional benefit is it saves the cost of re-instantiating potentially
  // multiple emptyHML objects. Both, the JS- and the C++-objects.
  dom::PreserveWrapper(this);

  return MakeUnique<TrustedHTML>(EmptyString());
}

UniquePtr<TrustedScript> TrustedTypePolicyFactory::EmptyScript() {
  // See the explanation in `EmptyHTML()`.
  dom::PreserveWrapper(this);

  return MakeUnique<TrustedScript>(EmptyString());
}

}  // namespace mozilla::dom
