/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionStorageManager.h"
#include "SessionStorage.h"
#include "StorageUtils.h"

namespace mozilla {
namespace dom {

using namespace StorageUtils;

NS_IMPL_ISUPPORTS(SessionStorageManager, nsIDOMStorageManager)

SessionStorageManager::SessionStorageManager()
{}

SessionStorageManager::~SessionStorageManager()
{}

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
  if (NS_WARN_IF(NS_FAILED(rv))) {
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
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
  if (NS_WARN_IF(NS_FAILED(rv))) {
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

} // dom namespace
} // mozilla namespace
