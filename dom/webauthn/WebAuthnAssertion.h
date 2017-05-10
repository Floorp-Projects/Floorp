/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnAssertion_h
#define mozilla_dom_WebAuthnAssertion_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class ScopedCredential;

} // namespace dom
} // namespace mozilla

namespace mozilla {
namespace dom {

class WebAuthnAssertion final : public nsISupports
                              , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WebAuthnAssertion)

public:
  explicit WebAuthnAssertion(nsPIDOMWindowInner* aParent);

protected:
  ~WebAuthnAssertion();

public:
  nsPIDOMWindowInner*
  GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<ScopedCredential>
  Credential() const;

  void
  GetClientData(JSContext* cx, JS::MutableHandle<JSObject*> aRetVal) const;

  void
  GetAuthenticatorData(JSContext* cx, JS::MutableHandle<JSObject*> aRetVal) const;

  void
  GetSignature(JSContext* cx, JS::MutableHandle<JSObject*> aRetVal) const;

  nsresult
  SetCredential(RefPtr<ScopedCredential> aCredential);

  nsresult
  SetClientData(CryptoBuffer& aBuffer);

  nsresult
  SetAuthenticatorData(CryptoBuffer& aBuffer);

  nsresult
  SetSignature(CryptoBuffer& aBuffer);

private:
  nsCOMPtr<nsPIDOMWindowInner> mParent;
  RefPtr<ScopedCredential> mCredential;
  CryptoBuffer mAuthenticatorData;
  CryptoBuffer mClientData;
  CryptoBuffer mSignature;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WebAuthnAssertion_h
