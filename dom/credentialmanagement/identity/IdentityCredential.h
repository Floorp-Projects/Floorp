/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IdentityCredential_h
#define mozilla_dom_IdentityCredential_h

#include "mozilla/dom/Credential.h"
#include "mozilla/dom/IPCIdentityCredential.h"
#include "mozilla/MozPromise.h"
#include "mozilla/Tuple.h"

namespace mozilla::dom {

class IdentityCredential final : public Credential {
 public:
  typedef MozPromise<RefPtr<IdentityCredential>, nsresult, true>
      GetIdentityCredentialPromise;
  typedef MozPromise<IPCIdentityCredential, nsresult, true>
      GetIPCIdentityCredentialPromise;
  typedef MozPromise<bool, nsresult, true> ValidationPromise;
  typedef MozPromise<IdentityInternalManifest, nsresult, true>
      GetManifestPromise;
  typedef MozPromise<Tuple<IdentityInternalManifest, IdentityAccountList>,
                     nsresult, true>
      GetAccountListPromise;
  typedef MozPromise<Tuple<IdentityToken, IdentityAccount>, nsresult, true>
      GetTokenPromise;

  explicit IdentityCredential(nsPIDOMWindowInner* aParent);

 protected:
  ~IdentityCredential() override;

 public:
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void CopyValuesFrom(const IPCIdentityCredential& aOther);

  IPCIdentityCredential MakeIPCIdentityCredential();

  void GetToken(nsAString& aToken) const;
  void SetToken(const nsAString& aToken);

  static RefPtr<GetIdentityCredentialPromise> DiscoverFromExternalSource(
      nsPIDOMWindowInner* aParent, const CredentialRequestOptions& aOptions,
      bool aSameOriginWithAncestors);

  static RefPtr<GetIPCIdentityCredentialPromise>
  DiscoverFromExternalSourceInMainProcess(
      nsIPrincipal* aPrincipal,
      const IdentityCredentialRequestOptions& aOptions);

  // Create an IPC credential that can be passed back to the content process.
  // This calls a lot of helpers to do the logic of going from a single provider
  // to a bearer token for an account at that provider.
  //
  //  Arguments:
  //    aPrincipal: the caller of navigator.credentials.get()'s principal
  //    aProvider: the provider to validate the root manifest of
  //  Return value:
  //    a promise resolving to an IPC credential with type "identity", id
  //    constructed to identify it, and token corresponding to the token
  //    fetched in FetchToken. This promise may reject with nsresult errors.
  //  Side effects:
  //    Will send network requests to the IDP. The details of which are in the
  //    other static methods here.
  static RefPtr<GetIPCIdentityCredentialPromise> CreateCredential(
      nsIPrincipal* aPrincipal, const IdentityProvider& aProvider);

  // Performs a Fetch for the root manifest of the provided identity provider
  // and validates it as correct. The returned promise resolves with a bool
  // that is true if everything is valid.
  //
  //  Arguments:
  //    aPrincipal: the caller of navigator.credentials.get()'s principal
  //    aProvider: the provider to validate the root manifest of
  //  Return value:
  //    promise that resolves to a bool that indicates success. Will reject
  //    when there are network or other errors.
  //  Side effects:
  //    Network request to the IDP's well-known from inside a NullPrincipal
  //    sandbox
  //
  static RefPtr<ValidationPromise> CheckRootManifest(
      nsIPrincipal* aPrincipal, const IdentityProvider& aProvider);

  // Performs a Fetch for the internal manifest of the provided identity
  // provider. The returned promise resolves with the manifest retrieved.
  //
  //  Arguments:
  //    aPrincipal: the caller of navigator.credentials.get()'s principal
  //    aProvider: the provider to fetch the root manifest
  //  Return value:
  //    promise that resolves to the internal manifest. Will reject
  //    when there are network or other errors.
  //  Side effects:
  //    Network request to the URL in aProvider as the manifest from inside a
  //    NullPrincipal sandbox
  //
  static RefPtr<GetManifestPromise> FetchInternalManifest(
      nsIPrincipal* aPrincipal, const IdentityProvider& aProvider);

  // Performs a Fetch for the account list from the provided identity
  // provider. The returned promise resolves with the manifest and the fetched
  // account list in a tuple of objects. We put the argument manifest in the
  // tuple to facilitate clean promise chaining.
  //
  //  Arguments:
  //    aPrincipal: the caller of navigator.credentials.get()'s principal
  //    aProvider: the provider to get account lists from
  //    aManifest: the provider's internal manifest
  //  Return value:
  //    promise that resolves to a Tuple of the passed manifest and the fetched
  //    account list. Will reject when there are network or other errors.
  //  Side effects:
  //    Network request to the provider supplied account endpoint with
  //    credentials but without any indication of aPrincipal.
  //
  static RefPtr<GetAccountListPromise> FetchAccountList(
      nsIPrincipal* aPrincipal, const IdentityProvider& aProvider,
      const IdentityInternalManifest& aManifest);

  // Performs a Fetch for a bearer token to the provided identity
  // provider for a given account. The returned promise resolves with the
  // account argument and the fetched token in a tuple of objects.
  // We put the argument account in the
  // tuple to facilitate clean promise chaining.
  //
  //  Arguments:
  //    aPrincipal: the caller of navigator.credentials.get()'s principal
  //    aProvider: the provider to get account lists from
  //    aManifest: the provider's internal manifest
  //    aAccount: the account to request
  //  Return value:
  //    promise that resolves to a Tuple of the passed account and the fetched
  //    token. Will reject when there are network or other errors.
  //  Side effects:
  //    Network request to the provider supplied token endpoint with
  //    credentials and including information about the requesting principal.
  //
  static RefPtr<GetTokenPromise> FetchToken(
      nsIPrincipal* aPrincipal, const IdentityProvider& aProvider,
      const IdentityInternalManifest& aManifest,
      const IdentityAccount& aAccount);

 private:
  nsAutoString mToken;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_IdentityCredential_h
