/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkerinfo_h
#define mozilla_dom_serviceworkerinfo_h

#include "MainThreadUtils.h"
#include "mozilla/dom/ServiceWorkerBinding.h" // For ServiceWorkerState
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/OriginAttributes.h"
#include "nsIServiceWorkerManager.h"

namespace mozilla {
namespace dom {

class ClientInfoAndState;
class ClientState;
class ServiceWorkerCloneData;
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
  ServiceWorkerDescriptor mDescriptor;
  const nsString mCacheName;
  OriginAttributes mOriginAttributes;

  // This LoadFlags is only applied to imported scripts, since the main script
  // has already been downloaded when performing the bytecheck. This LoadFlag is
  // composed of three parts:
  //   1. nsIChannel::LOAD_BYPASS_SERVICE_WORKER
  //   2. (Optional) nsIRequest::VALIDATE_ALWAYS
  //      depends on ServiceWorkerUpdateViaCache of its registration.
  //   3. (optional) nsIRequest::LOAD_BYPASS_CACHE
  //      depends on whether the update timer is expired.
  const nsLoadFlags mImportsLoadFlags;

  // Timestamp to track SW's state
  PRTime mCreationTime;
  TimeStamp mCreationTimeStamp;

  // The time of states are 0, if SW has not reached that state yet. Besides, we
  // update each of them after UpdateState() is called in SWRegistrationInfo.
  PRTime mInstalledTime;
  PRTime mActivatedTime;
  PRTime mRedundantTime;

  RefPtr<ServiceWorkerPrivate> mServiceWorkerPrivate;
  bool mSkipWaitingFlag;

  enum {
    Unknown,
    Enabled,
    Disabled
  } mHandlesFetch;

  ~ServiceWorkerInfo();

  // Generates a unique id for the service worker, with zero being treated as
  // invalid.
  uint64_t
  GetNextID() const;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVICEWORKERINFO

  void
  PostMessage(RefPtr<ServiceWorkerCloneData>&& aData,
              const ClientInfo& aClientInfo,
              const ClientState& aClientState);

  class ServiceWorkerPrivate*
  WorkerPrivate() const
  {
    MOZ_ASSERT(mServiceWorkerPrivate);
    return mServiceWorkerPrivate;
  }

  nsIPrincipal*
  Principal() const
  {
    return mPrincipal;
  }

  const nsCString&
  ScriptSpec() const
  {
    return mDescriptor.ScriptURL();
  }

  const nsCString&
  Scope() const
  {
    return mDescriptor.Scope();
  }

  bool SkipWaitingFlag() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    return mSkipWaitingFlag;
  }

  void SetSkipWaitingFlag()
  {
    MOZ_ASSERT(NS_IsMainThread());
    mSkipWaitingFlag = true;
  }

  ServiceWorkerInfo(nsIPrincipal* aPrincipal,
                    const nsACString& aScope,
                    uint64_t aRegistrationId,
                    const nsACString& aScriptSpec,
                    const nsAString& aCacheName,
                    nsLoadFlags aLoadFlags);

  ServiceWorkerState
  State() const
  {
    return mDescriptor.State();
  }

  const OriginAttributes&
  GetOriginAttributes() const
  {
    return mOriginAttributes;
  }

  const nsString&
  CacheName() const
  {
    return mCacheName;
  }

  nsLoadFlags
  GetImportsLoadFlags() const
  {
    return mImportsLoadFlags;
  }

  uint64_t
  ID() const
  {
    return mDescriptor.Id();
  }

  const ServiceWorkerDescriptor&
  Descriptor() const
  {
    return mDescriptor;
  }

  void
  UpdateState(ServiceWorkerState aState);

  // Only used to set initial state when loading from disk!
  void
  SetActivateStateUncheckedWithoutEvent(ServiceWorkerState aState)
  {
    MOZ_ASSERT(NS_IsMainThread());
    mDescriptor.SetState(aState);
  }

  void
  SetHandlesFetch(bool aHandlesFetch)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_DIAGNOSTIC_ASSERT(mHandlesFetch == Unknown);
    mHandlesFetch = aHandlesFetch ? Enabled : Disabled;
  }

  bool
  HandlesFetch() const
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_DIAGNOSTIC_ASSERT(mHandlesFetch != Unknown);
    return mHandlesFetch != Disabled;
  }

  void
  UpdateInstalledTime();

  void
  UpdateActivatedTime();

  void
  UpdateRedundantTime();

  int64_t
  GetInstalledTime() const
  {
    return mInstalledTime;
  }

  void
  SetInstalledTime(const int64_t aTime)
  {
    if (aTime == 0) {
      return;
    }

    mInstalledTime = aTime;
  }

  int64_t
  GetActivatedTime() const
  {
    return mActivatedTime;
  }

  void
  SetActivatedTime(const int64_t aTime)
  {
    if (aTime == 0) {
      return;
    }

    mActivatedTime = aTime;
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_serviceworkerinfo_h
