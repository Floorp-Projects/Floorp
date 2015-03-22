/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/QuotaClient.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/dom/cache/Manager.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/UsageInfo.h"
#include "nsIFile.h"
#include "nsISimpleEnumerator.h"
#include "nsThreadUtils.h"

namespace {

using mozilla::DebugOnly;
using mozilla::dom::cache::Manager;
using mozilla::dom::quota::Client;
using mozilla::dom::quota::PersistenceType;
using mozilla::dom::quota::QuotaManager;
using mozilla::dom::quota::UsageInfo;

static nsresult
GetBodyUsage(nsIFile* aDir, UsageInfo* aUsageInfo)
{
  nsCOMPtr<nsISimpleEnumerator> entries;
  nsresult rv = aDir->GetDirectoryEntries(getter_AddRefs(entries));
  if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

  bool hasMore;
  while (NS_SUCCEEDED(rv = entries->HasMoreElements(&hasMore)) && hasMore &&
         !aUsageInfo->Canceled()) {
    nsCOMPtr<nsISupports> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    nsCOMPtr<nsIFile> file = do_QueryInterface(entry);

    bool isDir;
    rv = file->IsDirectory(&isDir);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    if (isDir) {
      rv = GetBodyUsage(file, aUsageInfo);
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
      continue;
    }

    int64_t fileSize;
    rv = file->GetFileSize(&fileSize);
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
    MOZ_ASSERT(fileSize >= 0);

    aUsageInfo->AppendToFileUsage(fileSize);
  }

  return NS_OK;
}

class CacheQuotaClient final : public Client
{
public:
  virtual Type
  GetType() override
  {
    return DOMCACHE;
  }

  virtual nsresult
  InitOrigin(PersistenceType aPersistenceType, const nsACString& aGroup,
             const nsACString& aOrigin, UsageInfo* aUsageInfo) override
  {
    return NS_OK;
  }

  virtual nsresult
  GetUsageForOrigin(PersistenceType aPersistenceType, const nsACString& aGroup,
                    const nsACString& aOrigin,
                    UsageInfo* aUsageInfo) override
  {
    QuotaManager* qm = QuotaManager::Get();
    MOZ_ASSERT(qm);

    nsCOMPtr<nsIFile> dir;
    nsresult rv = qm->GetDirectoryForOrigin(aPersistenceType, aOrigin,
                                            getter_AddRefs(dir));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    rv = dir->Append(NS_LITERAL_STRING(DOMCACHE_DIRECTORY_NAME));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = dir->GetDirectoryEntries(getter_AddRefs(entries));
    if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

    bool hasMore;
    while (NS_SUCCEEDED(rv = entries->HasMoreElements(&hasMore)) && hasMore &&
           !aUsageInfo->Canceled()) {
      nsCOMPtr<nsISupports> entry;
      rv = entries->GetNext(getter_AddRefs(entry));
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

      nsCOMPtr<nsIFile> file = do_QueryInterface(entry);

      nsAutoString leafName;
      rv = file->GetLeafName(leafName);
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

      bool isDir;
      rv = file->IsDirectory(&isDir);
      if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }

      if (isDir) {
        if (leafName.EqualsLiteral("morgue")) {
          rv = GetBodyUsage(file, aUsageInfo);
        } else {
          NS_WARNING("Unknown Cache directory found!");
        }

        continue;
      }

      // Ignore transient sqlite files
      if (leafName.EqualsLiteral("caches.sqlite-journal") ||
          leafName.EqualsLiteral("caches.sqlite-shm") ||
          leafName.Find(NS_LITERAL_CSTRING("caches.sqlite-mj"), false, 0, 0) == 0) {
        continue;
      }

      if (leafName.EqualsLiteral("caches.sqlite") ||
          leafName.EqualsLiteral("caches.sqlite-wal")) {
        int64_t fileSize;
        rv = file->GetFileSize(&fileSize);
        if (NS_WARN_IF(NS_FAILED(rv))) { return rv; }
        MOZ_ASSERT(fileSize >= 0);

        aUsageInfo->AppendToDatabaseUsage(fileSize);
        continue;
      }

      NS_WARNING("Unknown Cache file found!");
    }

    return NS_OK;
  }

  virtual void
  OnOriginClearCompleted(PersistenceType aPersistenceType,
                         const nsACString& aOrigin) override
  {
    // nothing to do
  }

  virtual void
  ReleaseIOThreadObjects() override
  {
    // nothing to do
  }

  virtual bool
  IsFileServiceUtilized() override
  {
    return false;
  }

  virtual bool
  IsTransactionServiceActivated() override
  {
    // TODO: implement nsIOfflineStorage interface (bug 1110487)
    return false;
  }

  virtual void
  WaitForStoragesToComplete(nsTArray<nsIOfflineStorage*>& aStorages,
                            nsIRunnable* aCallback) override
  {
    // TODO: implement nsIOfflineStorage interface (bug 1110487)
  }


  virtual void
  ShutdownTransactionService() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    // spins the event loop and synchronously shuts down all Managers
    Manager::ShutdownAllOnMainThread();
  }

private:
  ~CacheQuotaClient() { }

public:
  NS_INLINE_DECL_REFCOUNTING(CacheQuotaClient, override)
};

} // anonymous namespace;

namespace mozilla {
namespace dom {
namespace cache {

already_AddRefed<quota::Client> CreateQuotaClient()
{
  nsRefPtr<CacheQuotaClient> ref = new CacheQuotaClient();
  return ref.forget();
}

} // namespace cache
} // namespace dom
} // namespace mozilla
