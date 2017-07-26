/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionStorageManager.h"

#include "SessionStorage.h"
#include "SessionStorageCache.h"
#include "StorageUtils.h"

namespace mozilla {
namespace dom {

using namespace StorageUtils;

NS_IMPL_ISUPPORTS(SessionStorageManager, nsIDOMStorageManager)

SessionStorageManager::SessionStorageManager()
{
  StorageObserver* observer = StorageObserver::Self();
  NS_ASSERTION(observer, "No StorageObserver, cannot observe private data delete notifications!");

  if (observer) {
    observer->AddSink(this);
  }
}

SessionStorageManager::~SessionStorageManager()
{
  StorageObserver* observer = StorageObserver::Self();
  if (observer) {
    observer->RemoveSink(this);
  }
}

NS_IMETHODIMP
SessionStorageManager::PrecacheStorage(nsIPrincipal* aPrincipal,
                                       nsIDOMStorage** aRetval)
{
  // Nothing to preload.
  return NS_OK;
}

NS_IMETHODIMP
SessionStorageManager::CreateStorage(mozIDOMWindow* aWindow,
                                     nsIPrincipal* aPrincipal,
                                     const nsAString& aDocumentURI,
                                     bool aPrivate,
                                     nsIDOMStorage** aRetval)
{
  nsAutoCString originKey;
  nsAutoCString originAttributes;
  nsresult rv = GenerateOriginKey(aPrincipal, originAttributes, originKey);
  if (NS_FAILED(rv)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  OriginKeyHashTable* table;
  if (!mOATable.Get(originAttributes, &table)) {
    table = new OriginKeyHashTable();
    mOATable.Put(originAttributes, table);
  }

  RefPtr<SessionStorageCache> cache;
  if (!table->Get(originKey, getter_AddRefs(cache))) {
    cache = new SessionStorageCache();
    table->Put(originKey, cache);
  }

  nsCOMPtr<nsPIDOMWindowInner> inner = nsPIDOMWindowInner::From(aWindow);

  RefPtr<SessionStorage> storage =
    new SessionStorage(inner, aPrincipal, cache, this, aDocumentURI, aPrivate);

  storage.forget(aRetval);
  return NS_OK;
}

NS_IMETHODIMP
SessionStorageManager::GetStorage(mozIDOMWindow* aWindow,
                                  nsIPrincipal* aPrincipal,
                                  bool aPrivate,
                                  nsIDOMStorage** aRetval)
{
  *aRetval = nullptr;

  nsAutoCString originKey;
  nsAutoCString originAttributes;
  nsresult rv = GenerateOriginKey(aPrincipal, originAttributes, originKey);
  if (NS_FAILED(rv)) {
    return rv;
  }

  OriginKeyHashTable* table;
  if (!mOATable.Get(originAttributes, &table)) {
    return NS_OK;
  }

  RefPtr<SessionStorageCache> cache;
  if (!table->Get(originKey, getter_AddRefs(cache))) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowInner> inner = nsPIDOMWindowInner::From(aWindow);

  RefPtr<SessionStorage> storage =
    new SessionStorage(inner, aPrincipal, cache, this, EmptyString(), aPrivate);

  storage.forget(aRetval);
  return NS_OK;
}

NS_IMETHODIMP
SessionStorageManager::CloneStorage(nsIDOMStorage* aStorage)
{
  if (NS_WARN_IF(!aStorage)) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<Storage> storage = static_cast<Storage*>(aStorage);
  if (storage->Type() != Storage::eSessionStorage) {
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoCString originKey;
  nsAutoCString originAttributes;
  nsresult rv = GenerateOriginKey(storage->Principal(), originAttributes,
                                  originKey);
  if (NS_FAILED(rv)) {
    return rv;
  }

  OriginKeyHashTable* table;
  if (!mOATable.Get(originAttributes, &table)) {
    table = new OriginKeyHashTable();
    mOATable.Put(originAttributes, table);
  }

  RefPtr<SessionStorageCache> cache;
  if (table->Get(originKey, getter_AddRefs(cache))) {
    // Do not replace an existing sessionStorage.
    return NS_OK;
  }

  cache = static_cast<SessionStorage*>(aStorage)->Cache()->Clone();
  MOZ_ASSERT(cache);

  table->Put(originKey, cache);
  return NS_OK;
}

NS_IMETHODIMP
SessionStorageManager::CheckStorage(nsIPrincipal* aPrincipal,
                                    nsIDOMStorage* aStorage,
                                    bool* aRetval)
{
  if (NS_WARN_IF(!aStorage)) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!aPrincipal) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoCString originKey;
  nsAutoCString originAttributes;
  nsresult rv = GenerateOriginKey(aPrincipal, originAttributes, originKey);
  if (NS_FAILED(rv)) {
    return rv;
  }

  *aRetval = false;

  OriginKeyHashTable* table;
  if (!mOATable.Get(originAttributes, &table)) {
    return NS_OK;
  }

  RefPtr<SessionStorageCache> cache;
  if (!table->Get(originKey, getter_AddRefs(cache))) {
    return NS_OK;
  }

  RefPtr<Storage> storage = static_cast<Storage*>(aStorage);
  if (storage->Type() != Storage::eSessionStorage) {
    return NS_OK;
  }

  RefPtr<SessionStorage> sessionStorage =
    static_cast<SessionStorage*>(aStorage);
  if (sessionStorage->Cache() != cache) {
    return NS_OK;
  }

  if (!StorageUtils::PrincipalsEqual(storage->Principal(), aPrincipal)) {
    return NS_OK;
  }

  *aRetval = true;
  return NS_OK;
}

NS_IMETHODIMP
SessionStorageManager::GetLocalStorageForPrincipal(nsIPrincipal* aPrincipal,
                                                   const nsAString& aDocumentURI,
                                                   bool aPrivate,
                                                   nsIDOMStorage** aRetval)
{
  return NS_ERROR_UNEXPECTED;
}

void
SessionStorageManager::ClearStorages(ClearStorageType aType,
                                     const OriginAttributesPattern& aPattern,
                                     const nsACString& aOriginScope)
{
  for (auto iter1 = mOATable.Iter(); !iter1.Done(); iter1.Next()) {
    OriginAttributes oa;
    DebugOnly<bool> ok = oa.PopulateFromSuffix(iter1.Key());
    MOZ_ASSERT(ok);
    if (!aPattern.Matches(oa)) {
      // This table doesn't match the given origin attributes pattern
      continue;
    }

    OriginKeyHashTable* table = iter1.Data();
    for (auto iter2 = table->Iter(); !iter2.Done(); iter2.Next()) {
      if (aOriginScope.IsEmpty() ||
          StringBeginsWith(iter2.Key(), aOriginScope)) {
        if (aType == eAll) {
          iter2.Data()->Clear(SessionStorageCache::eDefaultSetType, false);
          iter2.Data()->Clear(SessionStorageCache::eSessionSetType, false);
        } else {
          MOZ_ASSERT(aType == eSessionOnly);
          iter2.Data()->Clear(SessionStorageCache::eSessionSetType, false);
        }
      }
    }
  }
}

nsresult
SessionStorageManager::Observe(const char* aTopic,
                               const nsAString& aOriginAttributesPattern,
                               const nsACString& aOriginScope)
{
  OriginAttributesPattern pattern;
  if (!pattern.Init(aOriginAttributesPattern)) {
    NS_ERROR("Cannot parse origin attributes pattern");
    return NS_ERROR_FAILURE;
  }

  // Clear everything, caches + database
  if (!strcmp(aTopic, "cookie-cleared")) {
    ClearStorages(eAll, pattern, EmptyCString());
    return NS_OK;
  }

  // Clear from caches everything that has been stored
  // while in session-only mode
  if (!strcmp(aTopic, "session-only-cleared")) {
    ClearStorages(eSessionOnly, pattern, aOriginScope);
    return NS_OK;
  }

  // Clear everything (including so and pb data) from caches and database
  // for the gived domain and subdomains.
  if (!strcmp(aTopic, "domain-data-cleared")) {
    ClearStorages(eAll, pattern, aOriginScope);
    return NS_OK;
  }

  if (!strcmp(aTopic, "profile-change")) {
    // For case caches are still referenced - clear them completely
    ClearStorages(eAll, pattern, EmptyCString());
    mOATable.Clear();
    return NS_OK;
  }

  return NS_OK;
}

} // dom namespace
} // mozilla namespace
