/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerScriptCache.h"
#include "mozilla/dom/cache/CacheStorage.h"

#include "nsIPrincipal.h"
#include "Workers.h"

using mozilla::dom::cache::CacheStorage;

BEGIN_WORKERS_NAMESPACE

namespace serviceWorkerScriptCache {

nsresult
PurgeCache(nsIPrincipal* aPrincipal, const nsAString& aCacheName)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aPrincipal);

  if (aCacheName.IsEmpty()) {
    return NS_OK;
  }

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  MOZ_ASSERT(xpc, "This should never be null!");

  AutoJSAPI jsapi;
  jsapi.Init();
  nsCOMPtr<nsIXPConnectJSObjectHolder> sandbox;
  nsresult rv = xpc->CreateSandbox(jsapi.cx(), aPrincipal, getter_AddRefs(sandbox));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIGlobalObject> sandboxGlobalObject =
    xpc::NativeGlobal(sandbox->GetJSObject());
  if (!sandboxGlobalObject) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult error;
  nsRefPtr<CacheStorage> cacheStorage =
    CacheStorage::CreateOnMainThread(cache::CHROME_ONLY_NAMESPACE,
                                     sandboxGlobalObject,
                                     aPrincipal, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.ErrorCode();
  }

  // We use the ServiceWorker scope as key for the cacheStorage.
  nsRefPtr<Promise> promise =
    cacheStorage->Delete(aCacheName, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.ErrorCode();
  }

  // We don't actually care about the result of the delete operation.
  return NS_OK;
}

nsresult
GenerateCacheName(nsAString& aName)
{
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidGenerator =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsID id;
  rv = uuidGenerator->GenerateUUIDInPlace(&id);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  char chars[NSID_LENGTH];
  id.ToProvidedString(chars);
  aName.AssignASCII(chars, NSID_LENGTH);

  return NS_OK;
}

} // serviceWorkerScriptCache namespace

END_WORKERS_NAMESPACE
