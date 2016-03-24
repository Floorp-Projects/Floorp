/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RTCIdentityProviderRegistrar.h"
#include "mozilla/Attributes.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RTCIdentityProviderRegistrar)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(RTCIdentityProviderRegistrar)
NS_IMPL_CYCLE_COLLECTING_RELEASE(RTCIdentityProviderRegistrar)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(RTCIdentityProviderRegistrar,
                                      mGlobal,
                                      mGenerateAssertionCallback,
                                      mValidateAssertionCallback)

RTCIdentityProviderRegistrar::RTCIdentityProviderRegistrar(
    nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal)
    , mGenerateAssertionCallback(nullptr)
    , mValidateAssertionCallback(nullptr)
{
  MOZ_COUNT_CTOR(RTCIdentityProviderRegistrar);
}

RTCIdentityProviderRegistrar::~RTCIdentityProviderRegistrar()
{
  MOZ_COUNT_DTOR(RTCIdentityProviderRegistrar);
}

nsIGlobalObject*
RTCIdentityProviderRegistrar::GetParentObject() const
{
  return mGlobal;
}

JSObject*
RTCIdentityProviderRegistrar::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return RTCIdentityProviderRegistrarBinding::Wrap(aCx, this, aGivenProto);
}

void
RTCIdentityProviderRegistrar::Register(const RTCIdentityProvider& aIdp)
{
  mGenerateAssertionCallback = aIdp.mGenerateAssertion;
  mValidateAssertionCallback = aIdp.mValidateAssertion;
}

bool
RTCIdentityProviderRegistrar::HasIdp() const
{
  return mGenerateAssertionCallback && mValidateAssertionCallback;
}

already_AddRefed<Promise>
RTCIdentityProviderRegistrar::GenerateAssertion(
  const nsAString& aContents, const nsAString& aOrigin,
  const Optional<nsAString>& aUsernameHint, ErrorResult& aRv)
{
  if (!mGenerateAssertionCallback) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return nullptr;
  }
  return mGenerateAssertionCallback->Call(aContents, aOrigin, aUsernameHint, aRv);
}
already_AddRefed<Promise>
RTCIdentityProviderRegistrar::ValidateAssertion(
  const nsAString& aAssertion, const nsAString& aOrigin, ErrorResult& aRv)
{
  if (!mValidateAssertionCallback) {
    aRv.Throw(NS_ERROR_NOT_INITIALIZED);
    return nullptr;
  }
  return mValidateAssertionCallback->Call(aAssertion, aOrigin, aRv);
}



} // namespace dom
} // namespace mozilla
