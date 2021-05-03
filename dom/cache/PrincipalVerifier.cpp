/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/PrincipalVerifier.h"

#include "ErrorList.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/cache/ManagerId.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/BasePrincipal.h"
#include "CacheCommon.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIPrincipal.h"
#include "nsNetUtil.h"

namespace mozilla::dom::cache {

using mozilla::ipc::AssertIsOnBackgroundThread;
using mozilla::ipc::BackgroundParent;
using mozilla::ipc::PBackgroundParent;
using mozilla::ipc::PrincipalInfo;
using mozilla::ipc::PrincipalInfoToPrincipal;

// static
already_AddRefed<PrincipalVerifier> PrincipalVerifier::CreateAndDispatch(
    Listener& aListener, PBackgroundParent* aActor,
    const PrincipalInfo& aPrincipalInfo) {
  // We must get the ContentParent actor from the PBackgroundParent.  This
  // only works on the PBackground thread.
  AssertIsOnBackgroundThread();

  RefPtr<PrincipalVerifier> verifier =
      new PrincipalVerifier(aListener, aActor, aPrincipalInfo);

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(verifier));

  return verifier.forget();
}

void PrincipalVerifier::AddListener(Listener& aListener) {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mListenerList.Contains(&aListener));
  mListenerList.AppendElement(WrapNotNullUnchecked(&aListener));
}

void PrincipalVerifier::RemoveListener(Listener& aListener) {
  AssertIsOnBackgroundThread();
  MOZ_ALWAYS_TRUE(mListenerList.RemoveElement(&aListener));
}

PrincipalVerifier::PrincipalVerifier(Listener& aListener,
                                     PBackgroundParent* aActor,
                                     const PrincipalInfo& aPrincipalInfo)
    : Runnable("dom::cache::PrincipalVerifier"),
      mActor(BackgroundParent::GetContentParent(aActor)),
      mPrincipalInfo(aPrincipalInfo),
      mInitiatingEventTarget(GetCurrentSerialEventTarget()),
      mResult(NS_OK) {
  AssertIsOnBackgroundThread();
  MOZ_DIAGNOSTIC_ASSERT(mInitiatingEventTarget);

  AddListener(aListener);
}

PrincipalVerifier::~PrincipalVerifier() {
  // Since the PrincipalVerifier is a Runnable that executes on multiple
  // threads, its a race to see which thread de-refs us last.  Therefore
  // we cannot guarantee which thread we destruct on.

  MOZ_DIAGNOSTIC_ASSERT(mListenerList.IsEmpty());

  // We should always be able to explicitly release the actor on the main
  // thread.
  MOZ_DIAGNOSTIC_ASSERT(!mActor);
}

NS_IMETHODIMP
PrincipalVerifier::Run() {
  // Executed twice.  First, on the main thread and then back on the
  // originating thread.

  if (NS_IsMainThread()) {
    VerifyOnMainThread();
    return NS_OK;
  }

  CompleteOnInitiatingThread();
  return NS_OK;
}

void PrincipalVerifier::VerifyOnMainThread() {
  MOZ_ASSERT(NS_IsMainThread());

  // No matter what happens, we need to release the actor before leaving
  // this method.
  RefPtr<ContentParent> actor = std::move(mActor);

  QM_TRY_INSPECT(
      const auto& principal, PrincipalInfoToPrincipal(mPrincipalInfo), QM_VOID,
      [this](const nsresult result) { DispatchToInitiatingThread(result); });

  // We disallow null principal on the client side, but double-check here.
  if (NS_WARN_IF(principal->GetIsNullPrincipal())) {
    DispatchToInitiatingThread(NS_ERROR_FAILURE);
    return;
  }

  // Verify if a child process uses system principal, which is not allowed
  // to prevent system principal is spoofed.
  if (NS_WARN_IF(actor && principal->IsSystemPrincipal())) {
    DispatchToInitiatingThread(NS_ERROR_FAILURE);
    return;
  }

  actor = nullptr;

#ifdef DEBUG
  nsresult rv = NS_OK;
  // Sanity check principal origin by using it to construct a URI and security
  // checking it.  Don't do this for the system principal, though, as its origin
  // is a synthetic [System Principal] string.
  if (!principal->IsSystemPrincipal()) {
    nsAutoCString origin;
    rv = principal->GetOriginNoSuffix(origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      DispatchToInitiatingThread(rv);
      return;
    }
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), origin);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      DispatchToInitiatingThread(rv);
      return;
    }
    rv = principal->CheckMayLoad(uri, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      DispatchToInitiatingThread(rv);
      return;
    }
  }
#endif

  auto managerIdOrErr = ManagerId::Create(principal);
  if (NS_WARN_IF(managerIdOrErr.isErr())) {
    DispatchToInitiatingThread(managerIdOrErr.unwrapErr());
    return;
  }
  mManagerId = managerIdOrErr.unwrap();

  DispatchToInitiatingThread(NS_OK);
}

void PrincipalVerifier::CompleteOnInitiatingThread() {
  AssertIsOnBackgroundThread();

  for (const auto& listener : mListenerList.ForwardRange()) {
    listener->OnPrincipalVerified(mResult, mManagerId);
  }

  // The listener must clear its reference in OnPrincipalVerified()
  MOZ_DIAGNOSTIC_ASSERT(mListenerList.IsEmpty());
}

void PrincipalVerifier::DispatchToInitiatingThread(nsresult aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  mResult = aRv;

  // The Cache ShutdownObserver does not track all principal verifiers, so we
  // cannot ensure this always succeeds.  Instead, simply warn on failures.
  // This will result in a new CacheStorage object delaying operations until
  // shutdown completes and the browser goes away.  This is as graceful as
  // we can get here.
  QM_WARNONLY_TRY(
      mInitiatingEventTarget->Dispatch(this, nsIThread::DISPATCH_NORMAL));
}

}  // namespace mozilla::dom::cache
