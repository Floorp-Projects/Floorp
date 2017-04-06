/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthenticationBinding.h"
#include "mozilla/dom/WebAuthnAssertion.h"

namespace mozilla {
namespace dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebAuthnAssertion, mParent)
NS_IMPL_CYCLE_COLLECTING_ADDREF(WebAuthnAssertion)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebAuthnAssertion)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebAuthnAssertion)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

WebAuthnAssertion::WebAuthnAssertion(nsPIDOMWindowInner* aParent)
  : mParent(aParent)
{}

WebAuthnAssertion::~WebAuthnAssertion()
{}

JSObject*
WebAuthnAssertion::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return WebAuthnAssertionBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<ScopedCredential>
WebAuthnAssertion::Credential() const
{
  RefPtr<ScopedCredential> temp(mCredential);
  return temp.forget();
}

void
WebAuthnAssertion::GetClientData(JSContext* aCx,
                                 JS::MutableHandle<JSObject*> aRetVal) const
{
  aRetVal.set(mClientData.ToUint8Array(aCx));
}

void
WebAuthnAssertion::GetAuthenticatorData(JSContext* aCx,
                                        JS::MutableHandle<JSObject*> aRetVal) const
{
  aRetVal.set(mAuthenticatorData.ToUint8Array(aCx));
}

void
WebAuthnAssertion::GetSignature(JSContext* aCx,
                                JS::MutableHandle<JSObject*> aRetVal) const
{
  aRetVal.set(mSignature.ToUint8Array(aCx));
}

nsresult
WebAuthnAssertion::SetCredential(RefPtr<ScopedCredential> aCredential)
{
  mCredential = aCredential;
  return NS_OK;
}

nsresult
WebAuthnAssertion::SetClientData(CryptoBuffer& aBuffer)
{
  if (!mClientData.Assign(aBuffer)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
WebAuthnAssertion::SetAuthenticatorData(CryptoBuffer& aBuffer)
{
  if (!mAuthenticatorData.Assign(aBuffer)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
WebAuthnAssertion::SetSignature(CryptoBuffer& aBuffer)
{
  if (!mSignature.Assign(aBuffer)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
