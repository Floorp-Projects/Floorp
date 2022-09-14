/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Credential.h"
#include "mozilla/dom/CredentialsContainer.h"
#include "mozilla/dom/IdentityCredential.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/dom/WebAuthnManager.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/WindowContext.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsIDocShell.h"
#include "nsGlobalWindowInner.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CredentialsContainer, mParent, mManager)
NS_IMPL_CYCLE_COLLECTING_ADDREF(CredentialsContainer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CredentialsContainer)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CredentialsContainer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

already_AddRefed<Promise> CreatePromise(nsPIDOMWindowInner* aParent,
                                        ErrorResult& aRv) {
  MOZ_ASSERT(aParent);
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aParent);
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  return promise.forget();
}

already_AddRefed<Promise> CreateAndRejectWithNotAllowed(
    nsPIDOMWindowInner* aParent, ErrorResult& aRv) {
  MOZ_ASSERT(aParent);
  RefPtr<Promise> promise = CreatePromise(aParent, aRv);
  if (!promise) {
    return nullptr;
  }
  promise->MaybeRejectWithNotAllowedError(
      "CredentialContainer request is not allowed."_ns);
  return promise.forget();
}

already_AddRefed<Promise> CreateAndRejectWithNotSupported(
    nsPIDOMWindowInner* aParent, ErrorResult& aRv) {
  MOZ_ASSERT(aParent);
  RefPtr<Promise> promise = CreatePromise(aParent, aRv);
  if (!promise) {
    return nullptr;
  }
  promise->MaybeRejectWithNotSupportedError(
      "CredentialContainer request is not supported."_ns);
  return promise.forget();
}

static bool IsInActiveTab(nsPIDOMWindowInner* aParent) {
  // Returns whether aParent is an inner window somewhere in the active tab.
  // The active tab is the selected (i.e. visible) tab in the focused window.
  MOZ_ASSERT(aParent);

  RefPtr<Document> doc = aParent->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return false;
  }

  return IsInActiveTab(doc);
}

static bool IsSameOriginWithAncestors(nsPIDOMWindowInner* aParent) {
  // This method returns true if aParent is either not in a frame / iframe, or
  // is in a frame or iframe and all ancestors for aParent are the same origin.
  // This is useful for Credential Management because we need to prohibit
  // iframes, but not break mochitests (which use iframes to embed the tests).
  MOZ_ASSERT(aParent);

  WindowGlobalChild* wgc = aParent->GetWindowGlobalChild();

  // If there's no WindowGlobalChild, the inner window has already been
  // destroyed, so fail safe and return false.
  if (!wgc) {
    return false;
  }

  // Check that all ancestors are the same origin, repeating until we find a
  // null parent
  for (WindowContext* parentContext =
           wgc->WindowContext()->GetParentWindowContext();
       parentContext; parentContext = parentContext->GetParentWindowContext()) {
    if (!wgc->IsSameOriginWith(parentContext)) {
      // same-origin policy is violated
      return false;
    }
  }

  return true;
}

CredentialsContainer::CredentialsContainer(nsPIDOMWindowInner* aParent)
    : mParent(aParent) {
  MOZ_ASSERT(aParent);
}

CredentialsContainer::~CredentialsContainer() = default;

void CredentialsContainer::EnsureWebAuthnManager() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mManager) {
    mManager = new WebAuthnManager(mParent);
  }
}

JSObject* CredentialsContainer::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return CredentialsContainer_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise> CredentialsContainer::Get(
    const CredentialRequestOptions& aOptions, ErrorResult& aRv) {
  uint64_t totalOptions = 0;
  if (aOptions.mPublicKey.WasPassed() &&
      StaticPrefs::security_webauth_webauthn()) {
    totalOptions += 1;
  }
  if (aOptions.mIdentity.WasPassed() &&
      StaticPrefs::dom_security_credentialmanagement_identity_enabled()) {
    totalOptions += 1;
  }
  if (totalOptions > 1) {
    return CreateAndRejectWithNotSupported(mParent, aRv);
  }

  if (aOptions.mPublicKey.WasPassed() &&
      StaticPrefs::security_webauth_webauthn()) {
    if (!IsSameOriginWithAncestors(mParent) || !IsInActiveTab(mParent)) {
      return CreateAndRejectWithNotAllowed(mParent, aRv);
    }

    EnsureWebAuthnManager();
    return mManager->GetAssertion(aOptions.mPublicKey.Value(), aOptions.mSignal,
                                  aRv);
  }

  if (aOptions.mIdentity.WasPassed() &&
      StaticPrefs::dom_security_credentialmanagement_identity_enabled()) {
    RefPtr<Promise> promise = CreatePromise(mParent, aRv);

    IdentityCredential::DiscoverFromExternalSource(
        mParent, aOptions, IsSameOriginWithAncestors(mParent))
        ->Then(
            GetCurrentSerialEventTarget(), __func__,
            [promise](RefPtr<IdentityCredential> credential) {
              promise->MaybeResolve(credential);
            },
            [promise](nsresult error) { promise->MaybeReject(error); });

    return promise.forget();
  }

  return CreateAndRejectWithNotSupported(mParent, aRv);
}

already_AddRefed<Promise> CredentialsContainer::Create(
    const CredentialCreationOptions& aOptions, ErrorResult& aRv) {
  // Count the types of options provided. Must not be >1.
  uint64_t totalOptions = 0;
  if (aOptions.mPublicKey.WasPassed() &&
      StaticPrefs::security_webauth_webauthn()) {
    totalOptions += 1;
  }
  if (totalOptions > 1) {
    return CreateAndRejectWithNotSupported(mParent, aRv);
  }

  if (aOptions.mPublicKey.WasPassed() &&
      StaticPrefs::security_webauth_webauthn()) {
    if (!IsSameOriginWithAncestors(mParent) || !IsInActiveTab(mParent)) {
      return CreateAndRejectWithNotAllowed(mParent, aRv);
    }

    EnsureWebAuthnManager();
    return mManager->MakeCredential(aOptions.mPublicKey.Value(),
                                    aOptions.mSignal, aRv);
  }

  return CreateAndRejectWithNotSupported(mParent, aRv);
}

already_AddRefed<Promise> CredentialsContainer::Store(
    const Credential& aCredential, ErrorResult& aRv) {
  nsString type;
  aCredential.GetType(type);
  if (type.EqualsLiteral("public-key") &&
      StaticPrefs::security_webauth_webauthn()) {
    if (!IsSameOriginWithAncestors(mParent) || !IsInActiveTab(mParent)) {
      return CreateAndRejectWithNotAllowed(mParent, aRv);
    }

    EnsureWebAuthnManager();
    return mManager->Store(aCredential, aRv);
  }

  if (type.EqualsLiteral("identity") &&
      StaticPrefs::dom_security_credentialmanagement_identity_enabled()) {
    return CreateAndRejectWithNotSupported(mParent, aRv);
  }

  return CreateAndRejectWithNotSupported(mParent, aRv);
}

already_AddRefed<Promise> CredentialsContainer::PreventSilentAccess(
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mParent);
  if (NS_WARN_IF(!global)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  promise->MaybeResolveWithUndefined();
  return promise.forget();
}

}  // namespace mozilla::dom
