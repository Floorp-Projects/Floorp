/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TrustedTypePolicy.h"

#include "mozilla/AlreadyAddRefed.h"
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

UniquePtr<TrustedHTML> TrustedTypePolicy::CreateHTML(
    JSContext* aJSContext, const nsAString& aInput,
    const Sequence<JS::Value>& aArguments, ErrorResult& aErrorResult) const {
  // TODO: implement the spec.
  return MakeUnique<TrustedHTML>();
}

UniquePtr<TrustedScript> TrustedTypePolicy::CreateScript(
    JSContext* aJSContext, const nsAString& aInput,
    const Sequence<JS::Value>& aArguments, ErrorResult& aErrorResult) const {
  // TODO: implement the spec.
  return MakeUnique<TrustedScript>();
}

UniquePtr<TrustedScriptURL> TrustedTypePolicy::CreateScriptURL(
    JSContext* aJSContext, const nsAString& aInput,
    const Sequence<JS::Value>& aArguments, ErrorResult& aErrorResult) const {
  // TODO: implement the spec.
  return MakeUnique<TrustedScriptURL>();
}

}  // namespace mozilla::dom
