/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/OfflineStorage.h"

#include "mozilla/dom/cache/Context.h"
#include "mozilla/dom/cache/QuotaClient.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {
namespace cache {

using mozilla::dom::quota::Client;
using mozilla::dom::quota::PERSISTENCE_TYPE_DEFAULT;
using mozilla::dom::quota::QuotaManager;

NS_IMPL_ISUPPORTS(OfflineStorage, nsIOfflineStorage);

// static
already_AddRefed<OfflineStorage>
OfflineStorage::Register(Context::ThreadsafeHandle* aContext,
                         const QuotaInfo& aQuotaInfo)
{
  MOZ_ASSERT(NS_IsMainThread());

  QuotaManager* qm = QuotaManager::Get();
  if (NS_WARN_IF(!qm)) {
    return nullptr;
  }

  nsRefPtr<Client> client = qm->GetClient(Client::DOMCACHE);

  nsRefPtr<OfflineStorage> storage =
    new OfflineStorage(aContext, aQuotaInfo, client);

  if (NS_WARN_IF(!qm->RegisterStorage(storage))) {
    return nullptr;
  }

  return storage.forget();
}

void
OfflineStorage::AddDestroyCallback(nsIRunnable* aCallback)
{
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!mDestroyCallbacks.Contains(aCallback));
  mDestroyCallbacks.AppendElement(aCallback);
}

OfflineStorage::OfflineStorage(Context::ThreadsafeHandle* aContext,
                               const QuotaInfo& aQuotaInfo,
                               Client* aClient)
  : mContext(aContext)
  , mQuotaInfo(aQuotaInfo)
  , mClient(aClient)
{
  MOZ_ASSERT(mContext);
  MOZ_ASSERT(mClient);

  mPersistenceType = PERSISTENCE_TYPE_DEFAULT;
  mGroup = mQuotaInfo.mGroup;
}

OfflineStorage::~OfflineStorage()
{
  MOZ_ASSERT(NS_IsMainThread());
  QuotaManager* qm = QuotaManager::Get();
  MOZ_ASSERT(qm);
  qm->UnregisterStorage(this);
  for (uint32_t i = 0; i < mDestroyCallbacks.Length(); ++i) {
    mDestroyCallbacks[i]->Run();
  }
}

NS_IMETHODIMP_(const nsACString&)
OfflineStorage::Id()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mQuotaInfo.mStorageId;
}

NS_IMETHODIMP_(Client*)
OfflineStorage::GetClient()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mClient;
}

NS_IMETHODIMP_(bool)
OfflineStorage::IsOwnedByProcess(ContentParent* aOwner)
{
  MOZ_ASSERT(NS_IsMainThread());
  // The Cache and Context can be shared by multiple client processes.  They
  // are not exclusively owned by a single process.
  //
  // As far as I can tell this is used by QuotaManager to shutdown storages
  // when a particular process goes away.  We definitely don't want this
  // since we are shared.  Also, the Cache actor code already properly
  // handles asynchronous actor destruction when the child process dies.
  //
  // Therefore, always return false here.
  return false;
}

NS_IMETHODIMP_(const nsACString&)
OfflineStorage::Origin()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mQuotaInfo.mOrigin;
}

NS_IMETHODIMP_(nsresult)
OfflineStorage::Close()
{
  MOZ_ASSERT(NS_IsMainThread());
  mContext->AllowToClose();
  return NS_OK;
}

NS_IMETHODIMP_(void)
OfflineStorage::Invalidate()
{
  MOZ_ASSERT(NS_IsMainThread());
  mContext->InvalidateAndAllowToClose();
}

} // namespace cache
} // namespace dom
} // namespace mozilla
