/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerInfo.h"

#include "ServiceWorkerUtils.h"
#include "ServiceWorkerPrivate.h"
#include "ServiceWorkerScriptCache.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/ClientState.h"
#include "mozilla/dom/RemoteWorkerTypes.h"
#include "mozilla/dom/WorkerPrivate.h"

namespace mozilla {
namespace dom {

using mozilla::ipc::PrincipalInfo;

static_assert(nsIServiceWorkerInfo::STATE_PARSED ==
                  static_cast<uint16_t>(ServiceWorkerState::Parsed),
              "ServiceWorkerState enumeration value should match state values "
              "from nsIServiceWorkerInfo.");
static_assert(nsIServiceWorkerInfo::STATE_INSTALLING ==
                  static_cast<uint16_t>(ServiceWorkerState::Installing),
              "ServiceWorkerState enumeration value should match state values "
              "from nsIServiceWorkerInfo.");
static_assert(nsIServiceWorkerInfo::STATE_INSTALLED ==
                  static_cast<uint16_t>(ServiceWorkerState::Installed),
              "ServiceWorkerState enumeration value should match state values "
              "from nsIServiceWorkerInfo.");
static_assert(nsIServiceWorkerInfo::STATE_ACTIVATING ==
                  static_cast<uint16_t>(ServiceWorkerState::Activating),
              "ServiceWorkerState enumeration value should match state values "
              "from nsIServiceWorkerInfo.");
static_assert(nsIServiceWorkerInfo::STATE_ACTIVATED ==
                  static_cast<uint16_t>(ServiceWorkerState::Activated),
              "ServiceWorkerState enumeration value should match state values "
              "from nsIServiceWorkerInfo.");
static_assert(nsIServiceWorkerInfo::STATE_REDUNDANT ==
                  static_cast<uint16_t>(ServiceWorkerState::Redundant),
              "ServiceWorkerState enumeration value should match state values "
              "from nsIServiceWorkerInfo.");
static_assert(nsIServiceWorkerInfo::STATE_UNKNOWN ==
                  ServiceWorkerStateValues::Count,
              "ServiceWorkerState enumeration value should match state values "
              "from nsIServiceWorkerInfo.");

NS_IMPL_ISUPPORTS(ServiceWorkerInfo, nsIServiceWorkerInfo)

NS_IMETHODIMP
ServiceWorkerInfo::GetId(nsAString& aId) {
  MOZ_ASSERT(NS_IsMainThread());
  aId = mWorkerPrivateId;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInfo::GetScriptSpec(nsAString& aScriptSpec) {
  MOZ_ASSERT(NS_IsMainThread());
  CopyUTF8toUTF16(mDescriptor.ScriptURL(), aScriptSpec);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInfo::GetCacheName(nsAString& aCacheName) {
  MOZ_ASSERT(NS_IsMainThread());
  aCacheName = mCacheName;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInfo::GetState(uint16_t* aState) {
  MOZ_ASSERT(aState);
  MOZ_ASSERT(NS_IsMainThread());
  *aState = static_cast<uint16_t>(State());
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInfo::GetDebugger(nsIWorkerDebugger** aResult) {
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_FAILURE;
  }

  return mServiceWorkerPrivate->GetDebugger(aResult);
}

NS_IMETHODIMP
ServiceWorkerInfo::GetHandlesFetchEvents(bool* aValue) {
  MOZ_ASSERT(aValue);
  MOZ_ASSERT(NS_IsMainThread());

  if (mHandlesFetch == Unknown) {
    return NS_ERROR_FAILURE;
  }

  *aValue = HandlesFetch();
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInfo::GetInstalledTime(PRTime* _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(_retval);
  *_retval = mInstalledTime;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInfo::GetActivatedTime(PRTime* _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(_retval);
  *_retval = mActivatedTime;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInfo::GetRedundantTime(PRTime* _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(_retval);
  *_retval = mRedundantTime;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInfo::GetNavigationFaultCount(uint32_t* aNavigationFaultCount) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aNavigationFaultCount);
  *aNavigationFaultCount = mNavigationFaultCount;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInfo::GetTestingInjectCancellation(
    nsresult* aTestingInjectCancellation) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTestingInjectCancellation);
  *aTestingInjectCancellation = mTestingInjectCancellation;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInfo::SetTestingInjectCancellation(
    nsresult aTestingInjectCancellation) {
  MOZ_ASSERT(NS_IsMainThread());
  mTestingInjectCancellation = aTestingInjectCancellation;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerInfo::AttachDebugger() {
  return mServiceWorkerPrivate->AttachDebugger();
}

NS_IMETHODIMP
ServiceWorkerInfo::DetachDebugger() {
  return mServiceWorkerPrivate->DetachDebugger();
}

void ServiceWorkerInfo::UpdateState(ServiceWorkerState aState) {
  MOZ_ASSERT(NS_IsMainThread());
#ifdef DEBUG
  // Any state can directly transition to redundant, but everything else is
  // ordered.
  if (aState != ServiceWorkerState::Redundant) {
    MOZ_ASSERT_IF(State() == ServiceWorkerState::EndGuard_,
                  aState == ServiceWorkerState::Installing);
    MOZ_ASSERT_IF(State() == ServiceWorkerState::Installing,
                  aState == ServiceWorkerState::Installed);
    MOZ_ASSERT_IF(State() == ServiceWorkerState::Installed,
                  aState == ServiceWorkerState::Activating);
    MOZ_ASSERT_IF(State() == ServiceWorkerState::Activating,
                  aState == ServiceWorkerState::Activated);
  }
  // Activated can only go to redundant.
  MOZ_ASSERT_IF(State() == ServiceWorkerState::Activated,
                aState == ServiceWorkerState::Redundant);
#endif
  // Flush any pending functional events to the worker when it transitions to
  // the activated state.
  // TODO: Do we care that these events will race with the propagation of the
  //       state change?
  if (State() != aState) {
    mServiceWorkerPrivate->UpdateState(aState);
  }
  mDescriptor.SetState(aState);
  if (State() == ServiceWorkerState::Redundant) {
    serviceWorkerScriptCache::PurgeCache(mPrincipal, mCacheName);
    mServiceWorkerPrivate->NoteDeadServiceWorkerInfo();
  }
}

ServiceWorkerInfo::ServiceWorkerInfo(nsIPrincipal* aPrincipal,
                                     const nsACString& aScope,
                                     uint64_t aRegistrationId,
                                     uint64_t aRegistrationVersion,
                                     const nsACString& aScriptSpec,
                                     const nsAString& aCacheName,
                                     nsLoadFlags aImportsLoadFlags)
    : mPrincipal(aPrincipal),
      mDescriptor(GetNextID(), aRegistrationId, aRegistrationVersion,
                  aPrincipal, aScope, aScriptSpec, ServiceWorkerState::Parsed),
      mCacheName(aCacheName),
      mWorkerPrivateId(ComputeWorkerPrivateId()),
      mImportsLoadFlags(aImportsLoadFlags),
      mCreationTime(PR_Now()),
      mCreationTimeStamp(TimeStamp::Now()),
      mInstalledTime(0),
      mActivatedTime(0),
      mRedundantTime(0),
      mServiceWorkerPrivate(new ServiceWorkerPrivate(this)),
      mSkipWaitingFlag(false),
      mHandlesFetch(Unknown),
      mNavigationFaultCount(0),
      mTestingInjectCancellation(NS_OK) {
  MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default);
  MOZ_ASSERT(mPrincipal);
  // cache origin attributes so we can use them off main thread
  mOriginAttributes = mPrincipal->OriginAttributesRef();
  MOZ_ASSERT(!mDescriptor.ScriptURL().IsEmpty());
  MOZ_ASSERT(!mCacheName.IsEmpty());
  MOZ_ASSERT(!mWorkerPrivateId.IsEmpty());

  // Scripts of a service worker should always be loaded bypass service workers.
  // Otherwise, we might not be able to update a service worker correctly, if
  // there is a service worker generating the script.
  MOZ_DIAGNOSTIC_ASSERT(mImportsLoadFlags &
                        nsIChannel::LOAD_BYPASS_SERVICE_WORKER);
}

ServiceWorkerInfo::~ServiceWorkerInfo() {
  MOZ_ASSERT(mServiceWorkerPrivate);
  mServiceWorkerPrivate->NoteDeadServiceWorkerInfo();
}

static uint64_t gServiceWorkerInfoCurrentID = 0;

uint64_t ServiceWorkerInfo::GetNextID() const {
  return ++gServiceWorkerInfoCurrentID;
}

void ServiceWorkerInfo::PostMessage(RefPtr<ServiceWorkerCloneData>&& aData,
                                    const ClientInfo& aClientInfo,
                                    const ClientState& aClientState) {
  mServiceWorkerPrivate->SendMessageEvent(
      std::move(aData),
      ClientInfoAndState(aClientInfo.ToIPC(), aClientState.ToIPC()));
}

void ServiceWorkerInfo::UpdateInstalledTime() {
  MOZ_ASSERT(State() == ServiceWorkerState::Installed);
  MOZ_ASSERT(mInstalledTime == 0);

  mInstalledTime =
      mCreationTime +
      static_cast<PRTime>(
          (TimeStamp::Now() - mCreationTimeStamp).ToMicroseconds());
}

void ServiceWorkerInfo::UpdateActivatedTime() {
  MOZ_ASSERT(State() == ServiceWorkerState::Activated);
  MOZ_ASSERT(mActivatedTime == 0);

  mActivatedTime =
      mCreationTime +
      static_cast<PRTime>(
          (TimeStamp::Now() - mCreationTimeStamp).ToMicroseconds());
}

void ServiceWorkerInfo::UpdateRedundantTime() {
  MOZ_ASSERT(State() == ServiceWorkerState::Redundant);
  MOZ_ASSERT(mRedundantTime == 0);

  mRedundantTime =
      mCreationTime +
      static_cast<PRTime>(
          (TimeStamp::Now() - mCreationTimeStamp).ToMicroseconds());
}

void ServiceWorkerInfo::SetRegistrationVersion(uint64_t aVersion) {
  mDescriptor.SetRegistrationVersion(aVersion);
}

}  // namespace dom
}  // namespace mozilla
