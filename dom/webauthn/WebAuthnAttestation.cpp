/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthenticationBinding.h"
#include "mozilla/dom/WebAuthnAttestation.h"

namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebAuthnAttestation, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(WebAuthnAttestation)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebAuthnAttestation)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebAuthnAttestation)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

WebAuthnAttestation::WebAuthnAttestation(nsPIDOMWindowInner* aParent)
  : mParent(aParent)
{}

WebAuthnAttestation::~WebAuthnAttestation()
{}

JSObject*
WebAuthnAttestation::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto)
{
  return WebAuthnAttestationBinding::Wrap(aCx, this, aGivenProto);
}

void
WebAuthnAttestation::GetFormat(nsString& aRetVal) const
{
  aRetVal = mFormat;
}

void
WebAuthnAttestation::GetClientData(JSContext* aCx,
                                   JS::MutableHandle<JSObject*> aRetVal) const
{
  aRetVal.set(mClientData.ToUint8Array(aCx));
}

void
WebAuthnAttestation::GetAuthenticatorData(JSContext* aCx,
                                          JS::MutableHandle<JSObject*> aRetVal) const
{
  aRetVal.set(mAuthenticatorData.ToUint8Array(aCx));
}

void
WebAuthnAttestation::GetAttestation(JSContext* aCx,
                                    JS::MutableHandle<JS::Value> aRetVal) const
{
  aRetVal.setObject(*mAttestation.ToUint8Array(aCx));
}

nsresult
WebAuthnAttestation::SetFormat(nsString aFormat)
{
  mFormat = aFormat;
  return NS_OK;
}

nsresult
WebAuthnAttestation::SetClientData(CryptoBuffer& aBuffer)
{
  if (!mClientData.Assign(aBuffer)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
WebAuthnAttestation::SetAuthenticatorData(CryptoBuffer& aBuffer)
{
  if (!mAuthenticatorData.Assign(aBuffer)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
WebAuthnAttestation::SetAttestation(CryptoBuffer& aBuffer)
{
  if (!mAttestation.Assign(aBuffer)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
