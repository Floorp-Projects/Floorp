/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TrustedTypePolicy.h"

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/TrustedTypePolicyFactory.h"
#include "mozilla/dom/TrustedTypesBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TrustedTypePolicy, mParentObject)

JSObject* TrustedTypePolicy::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return TrustedTypePolicy_Binding::Wrap(aCx, this, aGivenProto);
}

UniquePtr<TrustedHTML> TrustedTypePolicy::CreateHTML(
    JSContext* aJSContext, const nsAString& aInput,
    const Sequence<JS::Value>& aArguments) const {
  // TODO: implement the spec.
  return MakeUnique<TrustedHTML>();
}

UniquePtr<TrustedScript> TrustedTypePolicy::CreateScript(
    JSContext* aJSContext, const nsAString& aInput,
    const Sequence<JS::Value>& aArguments) const {
  // TODO: implement the spec.
  return MakeUnique<TrustedScript>();
}

UniquePtr<TrustedScriptURL> TrustedTypePolicy::CreateScriptURL(
    JSContext* aJSContext, const nsAString& aInput,
    const Sequence<JS::Value>& aArguments) const {
  // TODO: implement the spec.
  return MakeUnique<TrustedScriptURL>();
}

}  // namespace mozilla::dom
