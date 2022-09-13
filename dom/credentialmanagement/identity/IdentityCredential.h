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

  void GetToken(nsAString& aToken) const;
  void SetToken(const nsAString& aToken);

  static RefPtr<GetIdentityCredentialPromise> DiscoverFromExternalSource(
      nsPIDOMWindowInner* aParent, const CredentialRequestOptions& aOptions,
      bool aSameOriginWithAncestors);

 private:
  nsAutoString mToken;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_IdentityCredential_h
