/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerInfo.h"

#include "ServiceWorkerScriptCache.h"

BEGIN_WORKERS_NAMESPACE

static_assert(nsIServiceWorkerInfo::STATE_INSTALLING == static_cast<uint16_t>(ServiceWorkerState::Installing),
              "ServiceWorkerState enumeration value should match state values from nsIServiceWorkerInfo.");
static_assert(nsIServiceWorkerInfo::STATE_INSTALLED == static_cast<uint16_t>(ServiceWorkerState::Installed),
              "ServiceWorkerState enumeration value should match state values from nsIServiceWorkerInfo.");
static_assert(nsIServiceWorkerInfo::STATE_ACTIVATING == static_cast<uint16_t>(ServiceWorkerState::Activating),
              "ServiceWorkerState enumeration value should match state values from nsIServiceWorkerInfo.");
static_assert(nsIServiceWorkerInfo::STATE_ACTIVATED == static_cast<uint16_t>(ServiceWorkerState::Activated),
              "ServiceWorkerState enumeration value should match state values from nsIServiceWorkerInfo.");
static_assert(nsIServiceWorkerInfo::STATE_REDUNDANT == static_cast<uint16_t>(ServiceWorkerState::Redundant),
              "ServiceWorkerState enumeration value should match state values from nsIServiceWorkerInfo.");
static_assert(nsIServiceWorkerInfo::STATE_UNKNOWN == static_cast<uint16_t>(ServiceWorkerState::EndGuard_),
              "ServiceWorkerState enumeration value should match state values from nsIServiceWorkerInfo.");

NS_IMPL_ISUPPORTS(ServiceWorkerInfo, nsIServiceWorkerInfo)

NS_IMETHODIMP
ServiceWorkerInfo::GetScriptSpec(nsAString& aScriptSpec)
{
  AssertIsOnMainThread();
  CopyUTF8toUTF16(mScriptSpec, aScriptSpec);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInfo::GetCacheName(nsAString& aCacheName)
{
  AssertIsOnMainThread();
  aCacheName = mCacheName;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInfo::GetState(uint16_t* aState)
{
  MOZ_ASSERT(aState);
  AssertIsOnMainThread();
  *aState = static_cast<uint16_t>(mState);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInfo::GetDebugger(nsIWorkerDebugger** aResult)
{
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_FAILURE;
  }

  return mServiceWorkerPrivate->GetDebugger(aResult);
}

NS_IMETHODIMP
ServiceWorkerInfo::AttachDebugger()
{
  return mServiceWorkerPrivate->AttachDebugger();
}

NS_IMETHODIMP
ServiceWorkerInfo::DetachDebugger()
{
  return mServiceWorkerPrivate->DetachDebugger();
}

void
ServiceWorkerInfo::AppendWorker(ServiceWorker* aWorker)
{
  MOZ_ASSERT(aWorker);
#ifdef DEBUG
  nsAutoString workerURL;
  aWorker->GetScriptURL(workerURL);
  MOZ_ASSERT(workerURL.Equals(NS_ConvertUTF8toUTF16(mScriptSpec)));
#endif
  MOZ_ASSERT(!mInstances.Contains(aWorker));

  mInstances.AppendElement(aWorker);
  aWorker->SetState(State());
}

void
ServiceWorkerInfo::RemoveWorker(ServiceWorker* aWorker)
{
  MOZ_ASSERT(aWorker);
#ifdef DEBUG
  nsAutoString workerURL;
  aWorker->GetScriptURL(workerURL);
  MOZ_ASSERT(workerURL.Equals(NS_ConvertUTF8toUTF16(mScriptSpec)));
#endif
  MOZ_ASSERT(mInstances.Contains(aWorker));

  mInstances.RemoveElement(aWorker);
}

namespace {

class ChangeStateUpdater final : public Runnable
{
public:
  ChangeStateUpdater(const nsTArray<ServiceWorker*>& aInstances,
                     ServiceWorkerState aState)
    : mState(aState)
  {
    for (size_t i = 0; i < aInstances.Length(); ++i) {
      mInstances.AppendElement(aInstances[i]);
    }
  }

  NS_IMETHOD Run() override
  {
    // We need to update the state of all instances atomically before notifying
    // them to make sure that the observed state for all instances inside
    // statechange event handlers is correct.
    for (size_t i = 0; i < mInstances.Length(); ++i) {
      mInstances[i]->SetState(mState);
    }
    for (size_t i = 0; i < mInstances.Length(); ++i) {
      mInstances[i]->DispatchStateChange(mState);
    }

    return NS_OK;
  }

private:
  AutoTArray<RefPtr<ServiceWorker>, 1> mInstances;
  ServiceWorkerState mState;
};

}

void
ServiceWorkerInfo::UpdateState(ServiceWorkerState aState)
{
  AssertIsOnMainThread();
#ifdef DEBUG
  // Any state can directly transition to redundant, but everything else is
  // ordered.
  if (aState != ServiceWorkerState::Redundant) {
    MOZ_ASSERT_IF(mState == ServiceWorkerState::EndGuard_, aState == ServiceWorkerState::Installing);
    MOZ_ASSERT_IF(mState == ServiceWorkerState::Installing, aState == ServiceWorkerState::Installed);
    MOZ_ASSERT_IF(mState == ServiceWorkerState::Installed, aState == ServiceWorkerState::Activating);
    MOZ_ASSERT_IF(mState == ServiceWorkerState::Activating, aState == ServiceWorkerState::Activated);
  }
  // Activated can only go to redundant.
  MOZ_ASSERT_IF(mState == ServiceWorkerState::Activated, aState == ServiceWorkerState::Redundant);
#endif
  // Flush any pending functional events to the worker when it transitions to the
  // activated state.
  // TODO: Do we care that these events will race with the propagation of the
  //       state change?
  if (aState == ServiceWorkerState::Activated && mState != aState) {
    mServiceWorkerPrivate->Activated();
  }
  mState = aState;
  nsCOMPtr<nsIRunnable> r = new ChangeStateUpdater(mInstances, mState);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(r.forget()));
  if (mState == ServiceWorkerState::Redundant) {
    serviceWorkerScriptCache::PurgeCache(mPrincipal, mCacheName);
  }
}

ServiceWorkerInfo::ServiceWorkerInfo(nsIPrincipal* aPrincipal,
                                     const nsACString& aScope,
                                     const nsACString& aScriptSpec,
                                     const nsAString& aCacheName)
  : mPrincipal(aPrincipal)
  , mScope(aScope)
  , mScriptSpec(aScriptSpec)
  , mCacheName(aCacheName)
  , mState(ServiceWorkerState::EndGuard_)
  , mServiceWorkerID(GetNextID())
  , mServiceWorkerPrivate(new ServiceWorkerPrivate(this))
  , mSkipWaitingFlag(false)
  , mHandlesFetch(Unknown)
{
  MOZ_ASSERT(mPrincipal);
  // cache origin attributes so we can use them off main thread
  mOriginAttributes = BasePrincipal::Cast(mPrincipal)->OriginAttributesRef();
  MOZ_ASSERT(!mScope.IsEmpty());
  MOZ_ASSERT(!mScriptSpec.IsEmpty());
  MOZ_ASSERT(!mCacheName.IsEmpty());
}

ServiceWorkerInfo::~ServiceWorkerInfo()
{
  MOZ_ASSERT(mServiceWorkerPrivate);
  mServiceWorkerPrivate->NoteDeadServiceWorkerInfo();
}

static uint64_t gServiceWorkerInfoCurrentID = 0;

uint64_t
ServiceWorkerInfo::GetNextID() const
{
  return ++gServiceWorkerInfoCurrentID;
}

already_AddRefed<ServiceWorker>
ServiceWorkerInfo::GetOrCreateInstance(nsPIDOMWindowInner* aWindow)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  RefPtr<ServiceWorker> ref;

  for (uint32_t i = 0; i < mInstances.Length(); ++i) {
    MOZ_ASSERT(mInstances[i]);
    if (mInstances[i]->GetOwner() == aWindow) {
      ref = mInstances[i];
      break;
    }
  }

  if (!ref) {
    ref = new ServiceWorker(aWindow, this);
  }

  return ref.forget();
}

END_WORKERS_NAMESPACE
