/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebCryptoThreadPool.h"

#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsXPCOMCIDInternal.h"
#include "nsXPCOMPrivate.h"
#include "nsIObserverService.h"
#include "nsIThreadPool.h"

namespace mozilla {
namespace dom {

StaticRefPtr<WebCryptoThreadPool> gInstance;

NS_IMPL_ISUPPORTS(WebCryptoThreadPool, nsIObserver)

/* static */ void
WebCryptoThreadPool::Initialize()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MOZ_ASSERT(!gInstance, "More than one instance!");

  gInstance = new WebCryptoThreadPool();
  NS_WARN_IF_FALSE(gInstance, "Failed create thread pool!");

  if (gInstance && NS_FAILED(gInstance->Init())) {
    NS_WARNING("Failed to initialize thread pool!");
    gInstance = nullptr;
  }
}

/* static */ nsresult
WebCryptoThreadPool::Dispatch(nsIRunnable* aRunnable)
{
  if (gInstance) {
    return gInstance->DispatchInternal(aRunnable);
  }

  // Fail if called on shutdown.
  return NS_ERROR_FAILURE;
}

nsresult
WebCryptoThreadPool::Init()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  NS_ENSURE_TRUE(obs, NS_ERROR_FAILURE);

  // Need this observer to know when to shut down the thread pool.
  return obs->AddObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false);
}

nsresult
WebCryptoThreadPool::DispatchInternal(nsIRunnable* aRunnable)
{
  MutexAutoLock lock(mMutex);

  if (!mPool) {
    nsCOMPtr<nsIThreadPool> pool(do_CreateInstance(NS_THREADPOOL_CONTRACTID));
    NS_ENSURE_TRUE(pool, NS_ERROR_FAILURE);

    nsresult rv = pool->SetName(NS_LITERAL_CSTRING("SubtleCrypto"));
    NS_ENSURE_SUCCESS(rv, rv);

    pool.swap(mPool);
  }

  return mPool->Dispatch(aRunnable, NS_DISPATCH_NORMAL);
}

void
WebCryptoThreadPool::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
  MutexAutoLock lock(mMutex);

  if (mPool) {
    mPool->Shutdown();
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  NS_WARN_IF_FALSE(obs, "Failed to retrieve observer service!");

  if (obs) {
    if (NS_FAILED(obs->RemoveObserver(this,
                                      NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID))) {
      NS_WARNING("Failed to remove shutdown observer!");
    }
  }
}

NS_IMETHODIMP
WebCryptoThreadPool::Observe(nsISupports* aSubject,
                             const char* aTopic,
                             const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  if (gInstance) {
    gInstance->Shutdown();
    gInstance = nullptr;
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
