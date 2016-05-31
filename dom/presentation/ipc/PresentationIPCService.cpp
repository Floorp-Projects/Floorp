/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PPresentation.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsIPresentationListener.h"
#include "PresentationCallbacks.h"
#include "PresentationChild.h"
#include "PresentationIPCService.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace {

PresentationChild* sPresentationChild;

} // anonymous

NS_IMPL_ISUPPORTS_INHERITED(PresentationIPCService,
                            PresentationServiceBase,
                            nsIPresentationService)

PresentationIPCService::PresentationIPCService()
{
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (NS_WARN_IF(!contentChild)) {
    return;
  }
  sPresentationChild = new PresentationChild(this);
  NS_WARN_IF(!contentChild->SendPPresentationConstructor(sPresentationChild));
}

/* virtual */
PresentationIPCService::~PresentationIPCService()
{
  Shutdown();

  mAvailabilityListeners.Clear();
  mSessionListeners.Clear();
  sPresentationChild = nullptr;
}

NS_IMETHODIMP
PresentationIPCService::StartSession(const nsAString& aUrl,
                                     const nsAString& aSessionId,
                                     const nsAString& aOrigin,
                                     const nsAString& aDeviceId,
                                     uint64_t aWindowId,
                                     nsIPresentationServiceCallback* aCallback)
{
  if (aWindowId != 0) {
    AddRespondingSessionId(aWindowId, aSessionId);
  }

  return SendRequest(aCallback, StartSessionRequest(nsString(aUrl),
                                                    nsString(aSessionId),
                                                    nsString(aOrigin),
                                                    nsString(aDeviceId),
                                                    aWindowId));
}

NS_IMETHODIMP
PresentationIPCService::SendSessionMessage(const nsAString& aSessionId,
                                           uint8_t aRole,
                                           const nsAString& aData)
{
  MOZ_ASSERT(!aSessionId.IsEmpty());
  MOZ_ASSERT(!aData.IsEmpty());

  return SendRequest(nullptr, SendSessionMessageRequest(nsString(aSessionId),
                                                        aRole,
                                                        nsString(aData)));
}

NS_IMETHODIMP
PresentationIPCService::CloseSession(const nsAString& aSessionId,
                                     uint8_t aRole,
                                     uint8_t aClosedReason)
{
  MOZ_ASSERT(!aSessionId.IsEmpty());

  return SendRequest(nullptr, CloseSessionRequest(nsString(aSessionId),
                                                  aRole,
                                                  aClosedReason));
}

NS_IMETHODIMP
PresentationIPCService::TerminateSession(const nsAString& aSessionId,
                                         uint8_t aRole)
{
  MOZ_ASSERT(!aSessionId.IsEmpty());

  return SendRequest(nullptr, TerminateSessionRequest(nsString(aSessionId), aRole));
}

nsresult
PresentationIPCService::SendRequest(nsIPresentationServiceCallback* aCallback,
                                    const PresentationIPCRequest& aRequest)
{
  if (sPresentationChild) {
    PresentationRequestChild* actor = new PresentationRequestChild(aCallback);
    NS_WARN_IF(!sPresentationChild->SendPPresentationRequestConstructor(actor, aRequest));
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationIPCService::RegisterAvailabilityListener(nsIPresentationAvailabilityListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);

  mAvailabilityListeners.AppendElement(aListener);
  if (sPresentationChild) {
    NS_WARN_IF(!sPresentationChild->SendRegisterAvailabilityHandler());
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationIPCService::UnregisterAvailabilityListener(nsIPresentationAvailabilityListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);

  mAvailabilityListeners.RemoveElement(aListener);
  if (sPresentationChild) {
    NS_WARN_IF(!sPresentationChild->SendUnregisterAvailabilityHandler());
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationIPCService::RegisterSessionListener(const nsAString& aSessionId,
                                                uint8_t aRole,
                                                nsIPresentationSessionListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);

  mSessionListeners.Put(aSessionId, aListener);
  if (sPresentationChild) {
    NS_WARN_IF(!sPresentationChild->SendRegisterSessionHandler(nsString(aSessionId), aRole));
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationIPCService::UnregisterSessionListener(const nsAString& aSessionId,
                                                  uint8_t aRole)
{
  MOZ_ASSERT(NS_IsMainThread());

  UntrackSessionInfo(aSessionId, aRole);

  mSessionListeners.Remove(aSessionId);
  if (sPresentationChild) {
    NS_WARN_IF(!sPresentationChild->SendUnregisterSessionHandler(nsString(aSessionId), aRole));
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationIPCService::RegisterRespondingListener(uint64_t aWindowId,
                                                   nsIPresentationRespondingListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());

  mRespondingListeners.Put(aWindowId, aListener);
  if (sPresentationChild) {
    NS_WARN_IF(!sPresentationChild->SendRegisterRespondingHandler(aWindowId));
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationIPCService::UnregisterRespondingListener(uint64_t aWindowId)
{
  MOZ_ASSERT(NS_IsMainThread());

  mRespondingListeners.Remove(aWindowId);
  if (sPresentationChild) {
    NS_WARN_IF(!sPresentationChild->SendUnregisterRespondingHandler(aWindowId));
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationIPCService::GetWindowIdBySessionId(const nsAString& aSessionId,
                                               uint64_t* aWindowId)
{
  return GetWindowIdBySessionIdInternal(aSessionId, aWindowId);
}

nsresult
PresentationIPCService::NotifySessionStateChange(const nsAString& aSessionId,
                                                 uint16_t aState,
                                                 nsresult aReason)
{
  nsCOMPtr<nsIPresentationSessionListener> listener;
  if (NS_WARN_IF(!mSessionListeners.Get(aSessionId, getter_AddRefs(listener)))) {
    return NS_OK;
  }

  return listener->NotifyStateChange(aSessionId, aState, aReason);
}

nsresult
PresentationIPCService::NotifyMessage(const nsAString& aSessionId,
                                      const nsACString& aData)
{
  nsCOMPtr<nsIPresentationSessionListener> listener;
  if (NS_WARN_IF(!mSessionListeners.Get(aSessionId, getter_AddRefs(listener)))) {
    return NS_OK;
  }

  return listener->NotifyMessage(aSessionId, aData);
}

nsresult
PresentationIPCService::NotifySessionConnect(uint64_t aWindowId,
                                             const nsAString& aSessionId)
{
  nsCOMPtr<nsIPresentationRespondingListener> listener;
  if (NS_WARN_IF(!mRespondingListeners.Get(aWindowId, getter_AddRefs(listener)))) {
    return NS_OK;
  }

  return listener->NotifySessionConnect(aWindowId, aSessionId);
}

nsresult
PresentationIPCService::NotifyAvailableChange(bool aAvailable)
{
  nsTObserverArray<nsCOMPtr<nsIPresentationAvailabilityListener>>::ForwardIterator iter(mAvailabilityListeners);
  while (iter.HasMore()) {
    nsIPresentationAvailabilityListener* listener = iter.GetNext();
    NS_WARN_IF(NS_FAILED(listener->NotifyAvailableChange(aAvailable)));
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationIPCService::GetExistentSessionIdAtLaunch(uint64_t aWindowId,
                                                     nsAString& aSessionId)
{
  return GetExistentSessionIdAtLaunchInternal(aWindowId, aSessionId);;
}

NS_IMETHODIMP
PresentationIPCService::NotifyReceiverReady(const nsAString& aSessionId,
                                            uint64_t aWindowId)
{
  MOZ_ASSERT(NS_IsMainThread());

  // No actual window uses 0 as its ID.
  if (NS_WARN_IF(aWindowId == 0)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Track the responding info for an OOP receiver page.
  AddRespondingSessionId(aWindowId, aSessionId);

  NS_WARN_IF(!sPresentationChild->SendNotifyReceiverReady(nsString(aSessionId),
                                                          aWindowId));

  // Release mCallback after using aSessionId
  // because aSessionId is held by mCallback.
  mCallback = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
PresentationIPCService::UntrackSessionInfo(const nsAString& aSessionId,
                                           uint8_t aRole)
{
  // Remove the OOP responding info (if it has never been used).
  RemoveRespondingSessionId(aSessionId);
  return NS_OK;
}

void
PresentationIPCService::NotifyPresentationChildDestroyed()
{
  sPresentationChild = nullptr;
}

nsresult
PresentationIPCService::MonitorResponderLoading(const nsAString& aSessionId,
                                                nsIDocShell* aDocShell)
{
  MOZ_ASSERT(NS_IsMainThread());

  mCallback = new PresentationResponderLoadingCallback(aSessionId);
  return mCallback->Init(aDocShell);
}
