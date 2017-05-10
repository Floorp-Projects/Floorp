/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScopedCredentialInfo_h
#define mozilla_dom_ScopedCredentialInfo_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/ErrorResult.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class CryptoKey;
class ScopedCredential;
class WebAuthnAttestation;

} // namespace dom
} // namespace mozilla

namespace mozilla {
namespace dom {

class ScopedCredentialInfo final : public nsISupports
                                 , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ScopedCredentialInfo)

public:
  explicit ScopedCredentialInfo(nsPIDOMWindowInner* aParent);

protected:
  ~ScopedCredentialInfo();

public:
  nsISupports*
  GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<ScopedCredential>
  Credential() const;

  already_AddRefed<WebAuthnAttestation>
  Attestation() const;

  void
  SetCredential(RefPtr<ScopedCredential>);

  void
  SetAttestation(RefPtr<WebAuthnAttestation>);

private:
  nsCOMPtr<nsPIDOMWindowInner> mParent;
  RefPtr<WebAuthnAttestation> mAttestation;
  RefPtr<ScopedCredential> mCredential;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ScopedCredentialInfo_h
