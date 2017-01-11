/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ScopedCredentialInfo.h"
#include "mozilla/dom/WebAuthenticationBinding.h"

namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(ScopedCredentialInfo, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(ScopedCredentialInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ScopedCredentialInfo)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ScopedCredentialInfo)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

ScopedCredentialInfo::ScopedCredentialInfo(WebAuthentication* aParent)
  : mParent(aParent)
{}

ScopedCredentialInfo::~ScopedCredentialInfo()
{}

JSObject*
ScopedCredentialInfo::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto)
{
  return ScopedCredentialInfoBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<ScopedCredential>
ScopedCredentialInfo::Credential() const
{
  RefPtr<ScopedCredential> temp(mCredential);
  return temp.forget();
}

already_AddRefed<WebAuthnAttestation>
ScopedCredentialInfo::Attestation() const
{
  RefPtr<WebAuthnAttestation> temp(mAttestation);
  return temp.forget();
}

void
ScopedCredentialInfo::SetCredential(RefPtr<ScopedCredential> aCredential)
{
  mCredential = aCredential;
}

void
ScopedCredentialInfo::SetAttestation(RefPtr<WebAuthnAttestation> aAttestation)
{
  mAttestation = aAttestation;
}


} // namespace dom
} // namespace mozilla
