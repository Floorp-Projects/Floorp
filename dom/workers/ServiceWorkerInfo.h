/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworkerinfo_h
#define mozilla_dom_workers_serviceworkerinfo_h

#include "mozilla/dom/ServiceWorkerBinding.h" // For ServiceWorkerState
#include "nsIServiceWorkerManager.h"

namespace mozilla {
namespace dom {
namespace workers {

class ServiceWorker;
class ServiceWorkerPrivate;

/*
 * Wherever the spec treats a worker instance and a description of said worker
 * as the same thing; i.e. "Resolve foo with
 * _GetNewestWorker(serviceWorkerRegistration)", we represent the description
 * by this class and spawn a ServiceWorker in the right global when required.
 */
class ServiceWorkerInfo final : public nsIServiceWorkerInfo
{
private:
  nsCOMPtr<nsIPrincipal> mPrincipal;
  const nsCString mScope;
  const nsCString mScriptSpec;
  const nsString mCacheName;
  ServiceWorkerState mState;
  PrincipalOriginAttributes mOriginAttributes;

  // This id is shared with WorkerPrivate to match requests issued by service
  // workers to their corresponding serviceWorkerInfo.
  uint64_t mServiceWorkerID;

  // We hold rawptrs since the ServiceWorker constructor and destructor ensure
  // addition and removal.
  // There is a high chance of there being at least one ServiceWorker
  // associated with this all the time.
  AutoTArray<ServiceWorker*, 1> mInstances;

  RefPtr<ServiceWorkerPrivate> mServiceWorkerPrivate;
  bool mSkipWaitingFlag;

  ~ServiceWorkerInfo();

  // Generates a unique id for the service worker, with zero being treated as
  // invalid.
  uint64_t
  GetNextID() const;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVICEWORKERINFO

  class ServiceWorkerPrivate*
  WorkerPrivate() const
  {
    MOZ_ASSERT(mServiceWorkerPrivate);
    return mServiceWorkerPrivate;
  }

  nsIPrincipal*
  GetPrincipal() const
  {
    return mPrincipal;
  }

  const nsCString&
  ScriptSpec() const
  {
    return mScriptSpec;
  }

  const nsCString&
  Scope() const
  {
    return mScope;
  }

  bool SkipWaitingFlag() const
  {
    AssertIsOnMainThread();
    return mSkipWaitingFlag;
  }

  void SetSkipWaitingFlag()
  {
    AssertIsOnMainThread();
    mSkipWaitingFlag = true;
  }

  ServiceWorkerInfo(nsIPrincipal* aPrincipal,
                    const nsACString& aScope,
                    const nsACString& aScriptSpec,
                    const nsAString& aCacheName);

  ServiceWorkerState
  State() const
  {
    return mState;
  }

  const PrincipalOriginAttributes&
  GetOriginAttributes() const
  {
    return mOriginAttributes;
  }

  const nsString&
  CacheName() const
  {
    return mCacheName;
  }

  uint64_t
  ID() const
  {
    return mServiceWorkerID;
  }

  void
  UpdateState(ServiceWorkerState aState);

  // Only used to set initial state when loading from disk!
  void
  SetActivateStateUncheckedWithoutEvent(ServiceWorkerState aState)
  {
    AssertIsOnMainThread();
    mState = aState;
  }

  void
  AppendWorker(ServiceWorker* aWorker);

  void
  RemoveWorker(ServiceWorker* aWorker);

  already_AddRefed<ServiceWorker>
  GetOrCreateInstance(nsPIDOMWindowInner* aWindow);
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkerinfo_h
