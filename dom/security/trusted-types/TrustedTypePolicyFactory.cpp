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
  // TODO: implement the spec.
  return MakeRefPtr<TrustedTypePolicy>(this).forget();
}

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

  return MakeUnique<TrustedHTML>();
}

UniquePtr<TrustedScript> TrustedTypePolicyFactory::EmptyScript() {
  // See the explanation in `EmptyHTML()`.
  dom::PreserveWrapper(this);

  return MakeUnique<TrustedScript>();
}

}  // namespace mozilla::dom
