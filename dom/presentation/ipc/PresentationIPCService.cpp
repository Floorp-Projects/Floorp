/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/PPresentation.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsGlobalWindow.h"
#include "nsIPresentationListener.h"
#include "PresentationCallbacks.h"
#include "PresentationChild.h"
#include "PresentationContentSessionInfo.h"
#include "PresentationIPCService.h"
#include "PresentationLog.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

namespace {

PresentationChild* sPresentationChild;

} // anonymous

NS_IMPL_ISUPPORTS(PresentationIPCService,
                  nsIPresentationService,
                  nsIPresentationAvailabilityListener)

PresentationIPCService::PresentationIPCService()
{
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (NS_WARN_IF(!contentChild || contentChild->IsShuttingDown())) {
    return;
  }
  sPresentationChild = new PresentationChild(this);
  Unused <<
    NS_WARN_IF(!contentChild->SendPPresentationConstructor(sPresentationChild));
}

/* virtual */
PresentationIPCService::~PresentationIPCService()
{
  Shutdown();

  mSessionListeners.Clear();
  mSessionInfoAtController.Clear();
  mSessionInfoAtReceiver.Clear();
  sPresentationChild = nullptr;
}

NS_IMETHODIMP
PresentationIPCService::StartSession(
               const nsTArray<nsString>& aUrls,
               const nsAString& aSessionId,
               const nsAString& aOrigin,
               const nsAString& aDeviceId,
               uint64_t aWindowId,
               nsIDOMEventTarget* aEventTarget,
               nsIPrincipal* aPrincipal,
               nsIPresentationServiceCallback* aCallback,
               nsIPresentationTransportBuilderConstructor* aBuilderConstructor)
{
  if (aWindowId != 0) {
    AddRespondingSessionId(aWindowId,
                           aSessionId,
                           nsIPresentationService::ROLE_CONTROLLER);
  }

  nsPIDOMWindowInner* window =
    nsGlobalWindow::GetInnerWindowWithId(aWindowId)->AsInner();
  TabId tabId = TabParent::GetTabIdFrom(window->GetDocShell());

  return SendRequest(aCallback, StartSessionRequest(aUrls,
                                                    nsString(aSessionId),
                                                    nsString(aOrigin),
                                                    nsString(aDeviceId),
                                                    aWindowId,
                                                    tabId,
                                                    IPC::Principal(aPrincipal)));
}

NS_IMETHODIMP
PresentationIPCService::SendSessionMessage(const nsAString& aSessionId,
                                           uint8_t aRole,
                                           const nsAString& aData)
{
  MOZ_ASSERT(!aSessionId.IsEmpty());
  MOZ_ASSERT(!aData.IsEmpty());

  RefPtr<PresentationContentSessionInfo> info =
    GetSessionInfo(aSessionId, aRole);
  // data channel session transport is maintained by content process
  if (info) {
    return info->Send(aData);
  }

  return SendRequest(nullptr, SendSessionMessageRequest(nsString(aSessionId),
                                                        aRole,
                                                        nsString(aData)));
}

NS_IMETHODIMP
PresentationIPCService::SendSessionBinaryMsg(const nsAString& aSessionId,
                                             uint8_t aRole,
                                             const nsACString &aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aData.IsEmpty());
  MOZ_ASSERT(!aSessionId.IsEmpty());
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);

  RefPtr<PresentationContentSessionInfo> info =
    GetSessionInfo(aSessionId, aRole);
  // data channel session transport is maintained by content process
  if (info) {
    return info->SendBinaryMsg(aData);
  }

  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
PresentationIPCService::SendSessionBlob(const nsAString& aSessionId,
                                        uint8_t aRole,
                                        nsIDOMBlob* aBlob)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aSessionId.IsEmpty());
  MOZ_ASSERT(aRole == nsIPresentationService::ROLE_CONTROLLER ||
             aRole == nsIPresentationService::ROLE_RECEIVER);
  MOZ_ASSERT(aBlob);

  RefPtr<PresentationContentSessionInfo> info =
    GetSessionInfo(aSessionId, aRole);
  // data channel session transport is maintained by content process
  if (info) {
    return info->SendBlob(aBlob);
  }

  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
PresentationIPCService::CloseSession(const nsAString& aSessionId,
                                     uint8_t aRole,
                                     uint8_t aClosedReason)
{
  MOZ_ASSERT(!aSessionId.IsEmpty());

  nsresult rv = SendRequest(nullptr, CloseSessionRequest(nsString(aSessionId),
                                                         aRole,
                                                         aClosedReason));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<PresentationContentSessionInfo> info =
    GetSessionInfo(aSessionId, aRole);
  if (info) {
    return info->Close(NS_OK);
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationIPCService::TerminateSession(const nsAString& aSessionId,
                                         uint8_t aRole)
{
  MOZ_ASSERT(!aSessionId.IsEmpty());

  nsresult rv = SendRequest(nullptr, TerminateSessionRequest(nsString(aSessionId), aRole));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<PresentationContentSessionInfo> info =
    GetSessionInfo(aSessionId, aRole);
  if (info) {
    return info->Close(NS_OK);
  }

  return NS_OK;
}

NS_IMETHODIMP
PresentationIPCService::ReconnectSession(const nsTArray<nsString>& aUrls,
                                         const nsAString& aSessionId,
                                         uint8_t aRole,
                                         nsIPresentationServiceCallback* aCallback)
{
  MOZ_ASSERT(!aSessionId.IsEmpty());

  if (aRole != nsIPresentationService::ROLE_CONTROLLER) {
    MOZ_ASSERT(false, "Only controller can call ReconnectSession.");
    return NS_ERROR_INVALID_ARG;
  }

  return SendRequest(aCallback, ReconnectSessionRequest(aUrls,
                                                        nsString(aSessionId),
                                                        aRole));
}

NS_IMETHODIMP
PresentationIPCService::BuildTransport(const nsAString& aSessionId,
                                       uint8_t aRole)
{
  MOZ_ASSERT(!aSessionId.IsEmpty());

  if (aRole != nsIPresentationService::ROLE_CONTROLLER) {
    MOZ_ASSERT(false, "Only controller can call ReconnectSession.");
    return NS_ERROR_INVALID_ARG;
  }

  return SendRequest(nullptr, BuildTransportRequest(nsString(aSessionId),
                                                    aRole));
}

nsresult
PresentationIPCService::SendRequest(nsIPresentationServiceCallback* aCallback,
                                    const PresentationIPCRequest& aRequest)
{
  if (sPresentationChild) {
    PresentationRequestChild* actor = new PresentationRequestChild(aCallback);
    Unused << NS_WARN_IF(!sPresentationChild->SendPPresentationRequestConstructor(actor, aRequest));
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationIPCService::RegisterAvailabilityListener(
                                const nsTArray<nsString>& aAvailabilityUrls,
                                nsIPresentationAvailabilityListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aAvailabilityUrls.IsEmpty());
  MOZ_ASSERT(aListener);

  nsTArray<nsString> addedUrls;
  mAvailabilityManager.AddAvailabilityListener(aAvailabilityUrls,
                                               aListener,
                                               addedUrls);

  if (sPresentationChild && !addedUrls.IsEmpty()) {
    Unused <<
      NS_WARN_IF(
        !sPresentationChild->SendRegisterAvailabilityHandler(addedUrls));
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationIPCService::UnregisterAvailabilityListener(
                                const nsTArray<nsString>& aAvailabilityUrls,
                                nsIPresentationAvailabilityListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<nsString> removedUrls;
  mAvailabilityManager.RemoveAvailabilityListener(aAvailabilityUrls,
                                                  aListener,
                                                  removedUrls);

  if (sPresentationChild && !removedUrls.IsEmpty()) {
    Unused <<
      NS_WARN_IF(
        !sPresentationChild->SendUnregisterAvailabilityHandler(removedUrls));
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

  nsCOMPtr<nsIPresentationSessionListener> listener;
  if (mSessionListeners.Get(aSessionId, getter_AddRefs(listener))) {
    mSessionListeners.Put(aSessionId, aListener);
    return NS_OK;
  }

  mSessionListeners.Put(aSessionId, aListener);
  if (sPresentationChild) {
    Unused <<
      NS_WARN_IF(!sPresentationChild->SendRegisterSessionHandler(
        nsString(aSessionId), aRole));
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
    Unused <<
      NS_WARN_IF(!sPresentationChild->SendUnregisterSessionHandler(
        nsString(aSessionId), aRole));
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
    Unused <<
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
    Unused <<
      NS_WARN_IF(!sPresentationChild->SendUnregisterRespondingHandler(
        aWindowId));
  }
  return NS_OK;
}

nsresult
PresentationIPCService::NotifySessionTransport(const nsString& aSessionId,
                                               const uint8_t& aRole,
                                               nsIPresentationSessionTransport* aTransport)
{
  RefPtr<PresentationContentSessionInfo> info =
    new PresentationContentSessionInfo(aSessionId, aRole, aTransport);

  if (NS_WARN_IF(NS_FAILED(info->Init()))) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (aRole == nsIPresentationService::ROLE_CONTROLLER) {
    mSessionInfoAtController.Put(aSessionId, info);
  } else {
    mSessionInfoAtReceiver.Put(aSessionId, info);
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationIPCService::GetWindowIdBySessionId(const nsAString& aSessionId,
                                               uint8_t aRole,
                                               uint64_t* aWindowId)
{
  return GetWindowIdBySessionIdInternal(aSessionId, aRole, aWindowId);
}

NS_IMETHODIMP
PresentationIPCService::UpdateWindowIdBySessionId(const nsAString& aSessionId,
                                                  uint8_t aRole,
                                                  const uint64_t aWindowId)
{
  return UpdateWindowIdBySessionIdInternal(aSessionId, aRole, aWindowId);
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

// Only used for OOP RTCDataChannel session transport case.
nsresult
PresentationIPCService::NotifyMessage(const nsAString& aSessionId,
                                      const nsACString& aData,
                                      const bool& aIsBinary)
{
  nsCOMPtr<nsIPresentationSessionListener> listener;
  if (NS_WARN_IF(!mSessionListeners.Get(aSessionId, getter_AddRefs(listener)))) {
    return NS_OK;
  }

  return listener->NotifyMessage(aSessionId, aData, aIsBinary);
}

// Only used for OOP RTCDataChannel session transport case.
nsresult
PresentationIPCService::NotifyTransportClosed(const nsAString& aSessionId,
                                              uint8_t aRole,
                                              nsresult aReason)
{
  RefPtr<PresentationContentSessionInfo> info =
    GetSessionInfo(aSessionId, aRole);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  Unused << NS_WARN_IF(!sPresentationChild->SendNotifyTransportClosed(nsString(aSessionId), aRole, aReason));
  return NS_OK;
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

NS_IMETHODIMP
PresentationIPCService::NotifyAvailableChange(
                                   const nsTArray<nsString>& aAvailabilityUrls,
                                   bool aAvailable)
{
  return mAvailabilityManager.DoNotifyAvailableChange(aAvailabilityUrls,
                                                      aAvailable);
}

NS_IMETHODIMP
PresentationIPCService::NotifyReceiverReady(
               const nsAString& aSessionId,
               uint64_t aWindowId,
               bool aIsLoading,
               nsIPresentationTransportBuilderConstructor* aBuilderConstructor)
{
  MOZ_ASSERT(NS_IsMainThread());

  // No actual window uses 0 as its ID.
  if (NS_WARN_IF(aWindowId == 0)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // Track the responding info for an OOP receiver page.
  AddRespondingSessionId(aWindowId,
                         aSessionId,
                         nsIPresentationService::ROLE_RECEIVER);

  Unused << NS_WARN_IF(!sPresentationChild->SendNotifyReceiverReady(nsString(aSessionId),
                                                                    aWindowId,
                                                                    aIsLoading));

  // Release mCallback after using aSessionId
  // because aSessionId is held by mCallback.
  mCallback = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
PresentationIPCService::UntrackSessionInfo(const nsAString& aSessionId,
                                           uint8_t aRole)
{
  PRES_DEBUG("content %s:id[%s], role[%d]\n", __func__,
             NS_ConvertUTF16toUTF8(aSessionId).get(), aRole);

  if (nsIPresentationService::ROLE_RECEIVER == aRole) {
    // Terminate receiver page.
    uint64_t windowId;
    if (NS_SUCCEEDED(GetWindowIdBySessionIdInternal(aSessionId,
                                                    aRole,
                                                    &windowId))) {
      NS_DispatchToMainThread(NS_NewRunnableFunction(
        "dom::PresentationIPCService::UntrackSessionInfo",
        [windowId]() -> void {
          PRES_DEBUG("Attempt to close window[%" PRIu64 "]\n", windowId);

          if (auto* window = nsGlobalWindow::GetInnerWindowWithId(windowId)) {
            window->Close();
          }
        }));
    }
  }

  // Remove the OOP responding info (if it has never been used).
  RemoveRespondingSessionId(aSessionId, aRole);

  if (nsIPresentationService::ROLE_CONTROLLER == aRole) {
    mSessionInfoAtController.Remove(aSessionId);
  } else {
    mSessionInfoAtReceiver.Remove(aSessionId);
  }

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

nsresult
PresentationIPCService::CloseContentSessionTransport(const nsString& aSessionId,
                                                     uint8_t aRole,
                                                     nsresult aReason)
{
  RefPtr<PresentationContentSessionInfo> info =
    GetSessionInfo(aSessionId, aRole);
  if (NS_WARN_IF(!info)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return info->Close(aReason);
}
