/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CredentialsContainer.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WebAuthnManager.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsIDocShell.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CredentialsContainer, mParent, mManager)
NS_IMPL_CYCLE_COLLECTING_ADDREF(CredentialsContainer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CredentialsContainer)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CredentialsContainer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

already_AddRefed<Promise>
CreateAndReject(nsPIDOMWindowInner* aParent, ErrorResult& aRv)
{
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

  promise->MaybeReject(NS_ERROR_DOM_NOT_ALLOWED_ERR);
  return promise.forget();
}

static bool
IsInActiveTab(nsPIDOMWindowInner* aParent)
{
  // Returns whether aParent is an inner window somewhere in the active tab.
  // The active tab is the selected (i.e. visible) tab in the focused window.
  MOZ_ASSERT(aParent);

  nsCOMPtr<nsIDocument> doc(aParent->GetExtantDoc());
  if (NS_WARN_IF(!doc)) {
    return false;
  }

  nsCOMPtr<nsIDocShell> docShell = doc->GetDocShell();
  if (!docShell) {
    return false;
  }

  bool isActive = false;
  docShell->GetIsActive(&isActive);
  if (!isActive) {
    return false;
  }

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  docShell->GetRootTreeItem(getter_AddRefs(rootItem));
  if (!rootItem) {
    return false;
  }
  nsCOMPtr<nsPIDOMWindowOuter> rootWin = rootItem->GetWindow();
  if (!rootWin) {
    return false;
  }

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return false;
  }

  nsCOMPtr<mozIDOMWindowProxy> activeWindow;
  fm->GetActiveWindow(getter_AddRefs(activeWindow));
  return activeWindow == rootWin;
}

static bool
IsSameOriginWithAncestors(nsPIDOMWindowInner* aParent)
{
  // This method returns true if aParent is either not in a frame / iframe, or
  // is in a frame or iframe and all ancestors for aParent are the same origin.
  // This is useful for Credential Management because we need to prohibit
  // iframes, but not break mochitests (which use iframes to embed the tests).
  MOZ_ASSERT(aParent);

  if (aParent->IsTopInnerWindow()) {
    // Not in a frame or iframe
    return true;
  }

  // We're in some kind of frame, so let's get the parent and start checking
  // the same origin policy
  nsINode* node = nsContentUtils::GetCrossDocParentNode(aParent->GetExtantDoc());
  if (NS_WARN_IF(!node)) {
    // This is a sanity check, since there has to be a parent. Fail safe.
    return false;
  }

  // Check that all ancestors are the same origin, repeating until we find a
  // null parent
  do {
    nsresult rv = nsContentUtils::CheckSameOrigin(aParent->GetExtantDoc(), node);
    if (NS_FAILED(rv)) {
      // same-origin policy is violated
      return false;
    }

    node = nsContentUtils::GetCrossDocParentNode(node);
  } while (node);

  return true;
}

CredentialsContainer::CredentialsContainer(nsPIDOMWindowInner* aParent) :
  mParent(aParent)
{
  MOZ_ASSERT(aParent);
}

CredentialsContainer::~CredentialsContainer()
{}

void
CredentialsContainer::EnsureWebAuthnManager()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mManager) {
    mManager = new WebAuthnManager(mParent);
  }
}

JSObject*
CredentialsContainer::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CredentialsContainer_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
CredentialsContainer::Get(const CredentialRequestOptions& aOptions,
                          ErrorResult& aRv)
{
  if (!IsSameOriginWithAncestors(mParent) || !IsInActiveTab(mParent)) {
    return CreateAndReject(mParent, aRv);
  }

  EnsureWebAuthnManager();
  return mManager->GetAssertion(aOptions.mPublicKey, aOptions.mSignal);
}

already_AddRefed<Promise>
CredentialsContainer::Create(const CredentialCreationOptions& aOptions,
                             ErrorResult& aRv)
{
  if (!IsSameOriginWithAncestors(mParent) || !IsInActiveTab(mParent)) {
    return CreateAndReject(mParent, aRv);
  }

  EnsureWebAuthnManager();
  return mManager->MakeCredential(aOptions.mPublicKey, aOptions.mSignal);
}

already_AddRefed<Promise>
CredentialsContainer::Store(const Credential& aCredential, ErrorResult& aRv)
{
  if (!IsSameOriginWithAncestors(mParent) || !IsInActiveTab(mParent)) {
    return CreateAndReject(mParent, aRv);
  }

  EnsureWebAuthnManager();
  return mManager->Store(aCredential);
}

already_AddRefed<Promise>
CredentialsContainer::PreventSilentAccess(ErrorResult& aRv)
{
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

} // namespace dom
} // namespace mozilla
