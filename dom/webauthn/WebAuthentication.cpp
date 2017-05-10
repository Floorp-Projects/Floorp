/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthentication.h"
#include "mozilla/dom/WebAuthnManager.h"

namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebAuthentication, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(WebAuthentication)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebAuthentication)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebAuthentication)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

WebAuthentication::WebAuthentication(nsPIDOMWindowInner* aParent) :
  mParent(aParent)
{
  MOZ_ASSERT(aParent);
}

WebAuthentication::~WebAuthentication()
{}

JSObject*
WebAuthentication::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return WebAuthenticationBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
WebAuthentication::MakeCredential(JSContext* aCx, const Account& aAccount,
                                  const Sequence<ScopedCredentialParameters>& aCryptoParameters,
                                  const ArrayBufferViewOrArrayBuffer& aChallenge,
                                  const ScopedCredentialOptions& aOptions)
{
  RefPtr<WebAuthnManager> mgr = WebAuthnManager::GetOrCreate();
  MOZ_ASSERT(mgr);
  return mgr->MakeCredential(mParent, aCx, aAccount, aCryptoParameters, aChallenge, aOptions);
}

already_AddRefed<Promise>
WebAuthentication::GetAssertion(const ArrayBufferViewOrArrayBuffer& aChallenge,
                                const AssertionOptions& aOptions)
{
  RefPtr<WebAuthnManager> mgr = WebAuthnManager::GetOrCreate();
  MOZ_ASSERT(mgr);
  return mgr->GetAssertion(mParent, aChallenge, aOptions);
}

} // namespace dom
} // namespace mozilla
