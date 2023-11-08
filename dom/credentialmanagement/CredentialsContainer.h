/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CredentialsContainer_h
#define mozilla_dom_CredentialsContainer_h

#include "mozilla/dom/CredentialManagementBinding.h"

namespace mozilla::dom {

class WebAuthnManager;

class CredentialsContainer final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(CredentialsContainer)

  explicit CredentialsContainer(nsPIDOMWindowInner* aParent);

  nsPIDOMWindowInner* GetParentObject() const { return mParent; }

  already_AddRefed<WebAuthnManager> GetWebAuthnManager();

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  already_AddRefed<Promise> Get(const CredentialRequestOptions& aOptions,
                                ErrorResult& aRv);

  already_AddRefed<Promise> Create(const CredentialCreationOptions& aOptions,
                                   ErrorResult& aRv);

  already_AddRefed<Promise> Store(const Credential& aCredential,
                                  ErrorResult& aRv);

  already_AddRefed<Promise> PreventSilentAccess(ErrorResult& aRv);

 private:
  ~CredentialsContainer();

  void EnsureWebAuthnManager();

  nsCOMPtr<nsPIDOMWindowInner> mParent;
  RefPtr<WebAuthnManager> mManager;
  bool mActiveIdentityRequest;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CredentialsContainer_h
