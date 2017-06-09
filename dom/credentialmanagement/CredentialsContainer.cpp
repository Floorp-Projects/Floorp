/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CredentialsContainer.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WebAuthnManager.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CredentialsContainer, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(CredentialsContainer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CredentialsContainer)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CredentialsContainer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

CredentialsContainer::CredentialsContainer(nsPIDOMWindowInner* aParent) :
  mParent(aParent)
{
  MOZ_ASSERT(aParent);
}

CredentialsContainer::~CredentialsContainer()
{}

JSObject*
CredentialsContainer::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CredentialsContainerBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
CredentialsContainer::Get(const CredentialRequestOptions& aOptions)
{
  RefPtr<WebAuthnManager> mgr = WebAuthnManager::GetOrCreate();
  MOZ_ASSERT(mgr);
  return mgr->GetAssertion(mParent, aOptions.mPublicKey);
}

already_AddRefed<Promise>
CredentialsContainer::Create(const CredentialCreationOptions& aOptions)
{
  RefPtr<WebAuthnManager> mgr = WebAuthnManager::GetOrCreate();
  MOZ_ASSERT(mgr);
  return mgr->MakeCredential(mParent, aOptions.mPublicKey);
}

} // namespace dom
} // namespace mozilla
