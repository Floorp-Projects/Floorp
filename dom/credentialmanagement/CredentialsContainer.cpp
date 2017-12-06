/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CredentialsContainer.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WebAuthnManager.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CredentialsContainer, mParent, mManager)
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

void
CredentialsContainer::EnsureWebAuthnManager()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mManager) {
    mManager = new WebAuthnManager(mParent);
  }
}

JSObject*
CredentialsContainer::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CredentialsContainerBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
CredentialsContainer::Get(const CredentialRequestOptions& aOptions)
{
  EnsureWebAuthnManager();
  return mManager->GetAssertion(aOptions.mPublicKey, aOptions.mSignal);
}

already_AddRefed<Promise>
CredentialsContainer::Create(const CredentialCreationOptions& aOptions)
{
  EnsureWebAuthnManager();
  return mManager->MakeCredential(aOptions.mPublicKey, aOptions.mSignal);
}

already_AddRefed<Promise>
CredentialsContainer::Store(const Credential& aCredential)
{
  EnsureWebAuthnManager();
  return mManager->Store(aCredential);
}

} // namespace dom
} // namespace mozilla
