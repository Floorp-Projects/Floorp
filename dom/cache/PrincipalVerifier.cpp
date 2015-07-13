/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/PrincipalVerifier.h"

#include "mozilla/AppProcessChecker.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/cache/ManagerId.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::ipc::AssertIsOnBackgroundThread;
using mozilla::ipc::BackgroundParent;
using mozilla::ipc::PBackgroundParent;
using mozilla::ipc::PrincipalInfo;
using mozilla::ipc::PrincipalInfoToPrincipal;

// static
already_AddRefed<PrincipalVerifier>
PrincipalVerifier::CreateAndDispatch(Listener* aListener,
                                     PBackgroundParent* aActor,
                                     const PrincipalInfo& aPrincipalInfo)
{
  // We must get the ContentParent actor from the PBackgroundParent.  This
  // only works on the PBackground thread.
  AssertIsOnBackgroundThread();

  nsRefPtr<PrincipalVerifier> verifier = new PrincipalVerifier(aListener,
                                                               aActor,
                                                               aPrincipalInfo);

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(verifier)));

  return verifier.forget();
}

void
PrincipalVerifier::AddListener(Listener* aListener)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aListener);
  MOZ_ASSERT(!mListenerList.Contains(aListener));
  mListenerList.AppendElement(aListener);
}

void
PrincipalVerifier::RemoveListener(Listener* aListener)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aListener);
  MOZ_ALWAYS_TRUE(mListenerList.RemoveElement(aListener));
}

PrincipalVerifier::PrincipalVerifier(Listener* aListener,
                                     PBackgroundParent* aActor,
                                     const PrincipalInfo& aPrincipalInfo)
  : mActor(BackgroundParent::GetContentParent(aActor))
  , mPrincipalInfo(aPrincipalInfo)
  , mInitiatingThread(NS_GetCurrentThread())
  , mResult(NS_OK)
{
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(mInitiatingThread);
  MOZ_ASSERT(aListener);

  mListenerList.AppendElement(aListener);
}

PrincipalVerifier::~PrincipalVerifier()
{
  // Since the PrincipalVerifier is a Runnable that executes on multiple
  // threads, its a race to see which thread de-refs us last.  Therefore
  // we cannot guarantee which thread we destruct on.

  MOZ_ASSERT(mListenerList.IsEmpty());

  // We should always be able to explicitly release the actor on the main
  // thread.
  MOZ_ASSERT(!mActor);
}

NS_IMETHODIMP
PrincipalVerifier::Run()
{
  // Executed twice.  First, on the main thread and then back on the
  // originating thread.

  if (NS_IsMainThread()) {
    VerifyOnMainThread();
    return NS_OK;
  }

  CompleteOnInitiatingThread();
  return NS_OK;
}

void
PrincipalVerifier::VerifyOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread());

  // No matter what happens, we need to release the actor before leaving
  // this method.
  nsRefPtr<ContentParent> actor;
  actor.swap(mActor);

  nsresult rv;
  nsRefPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(mPrincipalInfo,
                                                              &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    DispatchToInitiatingThread(rv);
    return;
  }

  // We disallow null principal and unknown app IDs on the client side, but
  // double-check here.
  if (NS_WARN_IF(principal->GetIsNullPrincipal() ||
                 principal->GetUnknownAppId())) {
    DispatchToInitiatingThread(NS_ERROR_FAILURE);
    return;
  }

  // Verify that a child process claims to own the app for this principal
  if (NS_WARN_IF(actor && !AssertAppPrincipal(actor, principal))) {
    DispatchToInitiatingThread(NS_ERROR_FAILURE);
    return;
  }
  actor = nullptr;

  nsCOMPtr<nsIScriptSecurityManager> ssm = nsContentUtils::GetSecurityManager();
  if (NS_WARN_IF(!ssm)) {
    DispatchToInitiatingThread(NS_ERROR_ILLEGAL_DURING_SHUTDOWN);
    return;
  }

#ifdef DEBUG
  // Sanity check principal origin by using it to construct a URI and security
  // checking it.  Don't do this for the system principal, though, as its origin
  // is a synthetic [System Principal] string.
  if (!ssm->IsSystemPrincipal(principal)) {
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
    rv = principal->CheckMayLoad(uri, false, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      DispatchToInitiatingThread(rv);
      return;
    }
  }
#endif

  rv = ManagerId::Create(principal, getter_AddRefs(mManagerId));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    DispatchToInitiatingThread(rv);
    return;
  }

  DispatchToInitiatingThread(NS_OK);
}

void
PrincipalVerifier::CompleteOnInitiatingThread()
{
  AssertIsOnBackgroundThread();
  ListenerList::ForwardIterator iter(mListenerList);
  while (iter.HasMore()) {
    iter.GetNext()->OnPrincipalVerified(mResult, mManagerId);
  }

  // The listener must clear its reference in OnPrincipalVerified()
  MOZ_ASSERT(mListenerList.IsEmpty());
}

void
PrincipalVerifier::DispatchToInitiatingThread(nsresult aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  mResult = aRv;

  // The Cache ShutdownObserver does not track all principal verifiers, so we
  // cannot ensure this always succeeds.  Instead, simply warn on failures.
  // This will result in a new CacheStorage object delaying operations until
  // shutdown completes and the browser goes away.  This is as graceful as
  // we can get here.
  nsresult rv = mInitiatingThread->Dispatch(this, nsIThread::DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    NS_WARNING("Cache unable to complete principal verification due to shutdown.");
  }
}

} // namespace cache
} // namespace dom
} // namespace mozilla
