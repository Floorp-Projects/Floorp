/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthentication_h
#define mozilla_dom_WebAuthentication_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/WebAuthenticationBinding.h"
#include "mozilla/dom/WebCryptoCommon.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/MozPromise.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/SharedThreadPool.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

#include "U2FAuthenticator.h"
#include "WebAuthnRequest.h"

namespace mozilla {
namespace dom {

struct Account;
class ArrayBufferViewOrArrayBuffer;
struct AssertionOptions;
class OwningArrayBufferViewOrArrayBuffer;
struct ScopedCredentialOptions;
struct ScopedCredentialParameters;

} // namespace dom
} // namespace mozilla

namespace mozilla {
namespace dom {

typedef RefPtr<ScopedCredentialInfo> CredentialPtr;
typedef RefPtr<WebAuthnAssertion> AssertionPtr;
typedef WebAuthnRequest<CredentialPtr> CredentialRequest;
typedef WebAuthnRequest<AssertionPtr> AssertionRequest;
typedef MozPromise<CredentialPtr, nsresult, false> CredentialPromise;
typedef MozPromise<AssertionPtr, nsresult, false> AssertionPromise;

class WebAuthentication final : public nsISupports
                              , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(WebAuthentication)

public:
  explicit WebAuthentication(nsPIDOMWindowInner* aParent);

protected:
  ~WebAuthentication();

public:
  nsPIDOMWindowInner*
  GetParentObject() const
  {
    return mParent;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise>
  MakeCredential(JSContext* aCx, const Account& accountInformation,
                 const Sequence<ScopedCredentialParameters>& cryptoParameters,
                 const ArrayBufferViewOrArrayBuffer& attestationChallenge,
                 const ScopedCredentialOptions& options);

  already_AddRefed<Promise>
  GetAssertion(const ArrayBufferViewOrArrayBuffer& assertionChallenge,
               const AssertionOptions& options);

private:
  nsresult
  InitLazily();

  void
  U2FAuthMakeCredential(const RefPtr<CredentialRequest>& aRequest,
             const Authenticator& aToken, CryptoBuffer& aRpIdHash,
             const nsACString& aClientData, CryptoBuffer& aClientDataHash,
             const Account& aAccount,
             const nsTArray<ScopedCredentialParameters>& aNormalizedParams,
             const Optional<Sequence<ScopedCredentialDescriptor>>& aExcludeList,
             const WebAuthnExtensions& aExtensions);
  void
  U2FAuthGetAssertion(const RefPtr<AssertionRequest>& aRequest,
                   const Authenticator& aToken, CryptoBuffer& aRpIdHash,
                   const nsACString& aClientData, CryptoBuffer& aClientDataHash,
                   nsTArray<CryptoBuffer>& aAllowList,
                   const WebAuthnExtensions& aExtensions);

  nsresult
  RelaxSameOrigin(const nsAString& aInputRpId, nsACString& aRelaxedRpId);

  nsCOMPtr<nsPIDOMWindowInner> mParent;
  nsString mOrigin;
  Sequence<Authenticator> mAuthenticators;
  bool mInitialized;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_WebAuthentication_h
