/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TVIPCService.h"

#include "mozilla/dom/ContentChild.h"
#include "TVChild.h"

namespace mozilla {
namespace dom {

static StaticAutoPtr<TVChild> sTVChild;

NS_IMPL_ISUPPORTS(TVIPCService, nsITVService)

TVIPCService::TVIPCService()
{
  ContentChild* contentChild = ContentChild::GetSingleton();
  if (NS_WARN_IF(!contentChild)) {
    return;
  }
  sTVChild = new TVChild(this);
  NS_WARN_IF(!contentChild->SendPTVConstructor(sTVChild));
}

/* virtual */
TVIPCService::~TVIPCService()
{
  sTVChild = nullptr;
}

NS_IMETHODIMP
TVIPCService::RegisterSourceListener(const nsAString& aTunerId,
                                     const nsAString& aSourceType,
                                     nsITVSourceListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aTunerId.IsEmpty());
  MOZ_ASSERT(!aSourceType.IsEmpty());
  MOZ_ASSERT(aListener);

  mSourceListenerTuples.AppendElement(new TVSourceListenerTuple(
    nsString(aTunerId), nsString(aSourceType), aListener));

  if (sTVChild) {
    NS_WARN_IF(!sTVChild->SendRegisterSourceHandler(nsAutoString(aTunerId),
                                                    nsAutoString(aSourceType)));
  }

  return NS_OK;
}

NS_IMETHODIMP
TVIPCService::UnregisterSourceListener(const nsAString& aTunerId,
                                       const nsAString& aSourceType,
                                       nsITVSourceListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aTunerId.IsEmpty());
  MOZ_ASSERT(!aSourceType.IsEmpty());
  MOZ_ASSERT(aListener);

  for (uint32_t i = 0; i < mSourceListenerTuples.Length(); i++) {
    const UniquePtr<TVSourceListenerTuple>& tuple = mSourceListenerTuples[i];
    if (aTunerId.Equals(Get<0>(*tuple)) && aSourceType.Equals(Get<1>(*tuple)) &&
        aListener == Get<2>(*tuple)) {
      mSourceListenerTuples.RemoveElementAt(i);
      break;
    }
  }

  if (sTVChild) {
    NS_WARN_IF(!sTVChild->SendUnregisterSourceHandler(
                  nsAutoString(aTunerId), nsAutoString(aSourceType)));
  }

  return NS_OK;
}

void
TVIPCService::GetSourceListeners(
  const nsAString& aTunerId,
  const nsAString& aSourceType,
  nsTArray<nsCOMPtr<nsITVSourceListener>>& aListeners) const
{
  aListeners.Clear();

  for (uint32_t i = 0; i < mSourceListenerTuples.Length(); i++) {
    const UniquePtr<TVSourceListenerTuple>& tuple = mSourceListenerTuples[i];
    nsCOMPtr<nsITVSourceListener> listener = Get<2>(*tuple);
    if (aTunerId.Equals(Get<0>(*tuple)) && aSourceType.Equals(Get<1>(*tuple))) {
      aListeners.AppendElement(listener);
      break;
    }
  }
}

/* virtual */ NS_IMETHODIMP
TVIPCService::GetTuners(nsITVServiceCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  return SendRequest(aCallback, TVGetTunersRequest());
}

/* virtual */ NS_IMETHODIMP
TVIPCService::SetSource(const nsAString& aTunerId,
                        const nsAString& aSourceType,
                        nsITVServiceCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  return SendRequest(aCallback, TVSetSourceRequest(nsAutoString(aTunerId),
                                                   nsAutoString(aSourceType)));
}

/* virtual */ NS_IMETHODIMP
TVIPCService::StartScanningChannels(const nsAString& aTunerId,
                                    const nsAString& aSourceType,
                                    nsITVServiceCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  return SendRequest(aCallback,
                     TVStartScanningChannelsRequest(nsAutoString(aTunerId),
                                                    nsAutoString(aSourceType)));
}

/* virtual */ NS_IMETHODIMP
TVIPCService::StopScanningChannels(const nsAString& aTunerId,
                                   const nsAString& aSourceType,
                                   nsITVServiceCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  return SendRequest(aCallback,
                     TVStopScanningChannelsRequest(nsAutoString(aTunerId),
                                                   nsAutoString(aSourceType)));
}

/* virtual */ NS_IMETHODIMP
TVIPCService::ClearScannedChannelsCache()
{
  MOZ_ASSERT(NS_IsMainThread());

  return SendRequest(nullptr, TVClearScannedChannelsCacheRequest());
}

/* virtual */ NS_IMETHODIMP
TVIPCService::SetChannel(const nsAString& aTunerId,
                         const nsAString& aSourceType,
                         const nsAString& aChannelNumber,
                         nsITVServiceCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  return SendRequest(aCallback,
                     TVSetChannelRequest(nsAutoString(aTunerId),
                                         nsAutoString(aSourceType),
                                         nsAutoString(aChannelNumber)));
}

/* virtual */ NS_IMETHODIMP
TVIPCService::GetChannels(const nsAString& aTunerId,
                          const nsAString& aSourceType,
                          nsITVServiceCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  return SendRequest(aCallback,
                     TVGetChannelsRequest(nsAutoString(aTunerId),
                                          nsAutoString(aSourceType)));
}

/* virtual */ NS_IMETHODIMP
TVIPCService::GetPrograms(const nsAString& aTunerId,
                          const nsAString& aSourceType,
                          const nsAString& aChannelNumber, uint64_t startTime,
                          uint64_t endTime, nsITVServiceCallback* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  return SendRequest(aCallback,
                     TVGetProgramsRequest(nsAutoString(aTunerId),
                                          nsAutoString(aSourceType),
                                          nsAutoString(aChannelNumber),
                                          startTime, endTime));
}

nsresult
TVIPCService::SendRequest(nsITVServiceCallback* aCallback,
                          const TVIPCRequest& aRequest)
{
  if (sTVChild) {
    TVRequestChild* actor = new TVRequestChild(aCallback);
    NS_WARN_IF(!sTVChild->SendPTVRequestConstructor(actor, aRequest));
  }
  return NS_OK;
}

nsresult
TVIPCService::NotifyChannelScanned(const nsAString& aTunerId,
                                   const nsAString& aSourceType,
                                   nsITVChannelData* aChannelData)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<nsCOMPtr<nsITVSourceListener>> listeners;
  GetSourceListeners(aTunerId, aSourceType, listeners);
  for (uint32_t i = 0; i < listeners.Length(); i++) {
    nsresult rv =
      listeners[i]->NotifyChannelScanned(aTunerId, aSourceType, aChannelData);
    NS_WARN_IF(NS_FAILED(rv));
  }

  return NS_OK;
}

nsresult
TVIPCService::NotifyChannelScanComplete(const nsAString& aTunerId,
                                        const nsAString& aSourceType)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<nsCOMPtr<nsITVSourceListener>> listeners;
  GetSourceListeners(aTunerId, aSourceType, listeners);
  for (uint32_t i = 0; i < listeners.Length(); i++) {
    nsresult rv =
      listeners[i]->NotifyChannelScanComplete(aTunerId, aSourceType);
    NS_WARN_IF(NS_FAILED(rv));
  }

  return NS_OK;
}

nsresult
TVIPCService::NotifyChannelScanStopped(const nsAString& aTunerId,
                                       const nsAString& aSourceType)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<nsCOMPtr<nsITVSourceListener>> listeners;
  GetSourceListeners(aTunerId, aSourceType, listeners);
  for (uint32_t i = 0; i < listeners.Length(); i++) {
    nsresult rv = listeners[i]->NotifyChannelScanStopped(aTunerId, aSourceType);
    NS_WARN_IF(NS_FAILED(rv));
  }

  return NS_OK;
}

nsresult
TVIPCService::NotifyEITBroadcasted(const nsAString& aTunerId,
                                   const nsAString& aSourceType,
                                   nsITVChannelData* aChannelData,
                                   nsITVProgramData** aProgramDataList,
                                   uint32_t aCount)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsTArray<nsCOMPtr<nsITVSourceListener>> listeners;
  GetSourceListeners(aTunerId, aSourceType, listeners);
  for (uint32_t i = 0; i < listeners.Length(); i++) {
    nsresult rv = listeners[i]->NotifyEITBroadcasted(
      aTunerId, aSourceType, aChannelData, aProgramDataList, aCount);
    NS_WARN_IF(NS_FAILED(rv));
  }

  return NS_OK;
}

} // dom
} // mozilla
