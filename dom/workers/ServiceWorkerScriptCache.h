/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_ServiceWorkerScriptCache_h
#define mozilla_dom_workers_ServiceWorkerScriptCache_h

#include "nsString.h"

class nsIPrincipal;

namespace mozilla {
namespace dom {
namespace workers {

namespace serviceWorkerScriptCache {

nsresult
PurgeCache(nsIPrincipal* aPrincipal, const nsAString& aCacheName);

nsresult
GenerateCacheName(nsAString& aName);

class CompareCallback
{
public:
  virtual void ComparisonResult(nsresult aStatus, bool aInCacheAndEqual) = 0;

  NS_IMETHOD_(MozExternalRefCountType) AddRef() = 0;
  NS_IMETHOD_(MozExternalRefCountType) Release() = 0;
};

nsresult
Compare(nsIPrincipal* aPrincipal, const nsAString& aCacheName,
        const nsAString& aURL, CompareCallback* aCallback);

} // serviceWorkerScriptCache namespace

} // workers namespace
} // dom namespace
} // mozilla namespace

#endif // mozilla_dom_workers_ServiceWorkerScriptCache_h
