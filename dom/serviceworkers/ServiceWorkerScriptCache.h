/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ServiceWorkerScriptCache_h
#define mozilla_dom_ServiceWorkerScriptCache_h

#include "nsIRequest.h"
#include "nsString.h"

class nsILoadGroup;
class nsIPrincipal;

namespace mozilla {
namespace dom {

class ServiceWorkerRegistrationInfo;

namespace serviceWorkerScriptCache {

nsresult
PurgeCache(nsIPrincipal* aPrincipal, const nsAString& aCacheName);

nsresult
GenerateCacheName(nsAString& aName);

enum class OnFailure : uint8_t {
    DoNothing,
    Uninstall
};

class CompareCallback
{
public:

  /*
   * If there is an error, ignore aInCacheAndEqual and aNewCacheName.
   * On success, if the cached result and network result matched,
   * aInCacheAndEqual will be true and no new cache name is passed, otherwise
   * use the new cache name to load the ServiceWorker.
   */
  virtual void
  ComparisonResult(nsresult aStatus,
                   bool aInCacheAndEqual,
                   OnFailure aOnFailure,
                   const nsAString& aNewCacheName,
                   const nsACString& aMaxScope,
                   nsLoadFlags aLoadFlags) = 0;

  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING
};

nsresult
Compare(ServiceWorkerRegistrationInfo* aRegistration,
        nsIPrincipal* aPrincipal, const nsAString& aCacheName,
        const nsAString& aURL, CompareCallback* aCallback, nsILoadGroup* aLoadGroup);

} // namespace serviceWorkerScriptCache

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ServiceWorkerScriptCache_h
