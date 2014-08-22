/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TVServiceRunnables.h"
#include "nsCOMPtr.h"
#include "nsIMutableArray.h"
#include "nsServiceManagerUtils.h"
#include "FakeTVService.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(FakeTVService, nsITVService)

/* virtual */ NS_IMETHODIMP
FakeTVService::GetSourceListener(nsITVSourceListener** aSourceListener)
{
  *aSourceListener = mSourceListener;
  NS_ADDREF(*aSourceListener);
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
FakeTVService::SetSourceListener(nsITVSourceListener* aSourceListener)
{
  mSourceListener = aSourceListener;
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
FakeTVService::GetTuners(nsITVServiceCallback* aCallback)
{
  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIMutableArray> tunerDataList = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!tunerDataList) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // TODO Implement in follow-up patches.

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(aCallback, tunerDataList);
  return NS_DispatchToCurrentThread(runnable);
}

/* virtual */ NS_IMETHODIMP
FakeTVService::SetSource(const nsAString& aTunerId,
                         const nsAString& aSourceType,
                         nsITVServiceCallback* aCallback)
{
  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  // TODO Implement in follow-up patches.

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(aCallback, nullptr);
  return NS_DispatchToCurrentThread(runnable);
}

/* virtual */ NS_IMETHODIMP
FakeTVService::StartScanningChannels(const nsAString& aTunerId,
                                     const nsAString& aSourceType,
                                     nsITVServiceCallback* aCallback)
{
  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  // TODO Implement in follow-up patches.

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(aCallback, nullptr);
  return NS_DispatchToCurrentThread(runnable);
}

/* virtual */ NS_IMETHODIMP
FakeTVService::StopScanningChannels(const nsAString& aTunerId,
                                    const nsAString& aSourceType,
                                    nsITVServiceCallback* aCallback)
{
  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  // TODO Implement in follow-up patches.

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(aCallback, nullptr);
  return NS_DispatchToCurrentThread(runnable);
}

/* virtual */ NS_IMETHODIMP
FakeTVService::ClearScannedChannelsCache()
{
  // TODO Implement in follow-up patches.

  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
FakeTVService::SetChannel(const nsAString& aTunerId,
                          const nsAString& aSourceType,
                          const nsAString& aChannelNumber,
                          nsITVServiceCallback* aCallback)
{
  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIMutableArray> channelDataList = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!channelDataList) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // TODO Implement in follow-up patches.

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(aCallback, channelDataList);
  return NS_DispatchToCurrentThread(runnable);
}

/* virtual */ NS_IMETHODIMP
FakeTVService::GetChannels(const nsAString& aTunerId,
                           const nsAString& aSourceType,
                           nsITVServiceCallback* aCallback)
{
  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIMutableArray> channelDataList = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!channelDataList) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // TODO Implement in follow-up patches.

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(aCallback, channelDataList);
  return NS_DispatchToCurrentThread(runnable);
}

/* virtual */ NS_IMETHODIMP
FakeTVService::GetPrograms(const nsAString& aTunerId,
                           const nsAString& aSourceType,
                           const nsAString& aChannelNumber,
                           uint64_t startTime,
                           uint64_t endTime,
                           nsITVServiceCallback* aCallback)
{
  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIMutableArray> programDataList = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!programDataList) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // TODO Implement in follow-up patches.

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(aCallback, programDataList);
  return NS_DispatchToCurrentThread(runnable);
}

/* virtual */ NS_IMETHODIMP
FakeTVService::GetOverlayId(const nsAString& aTunerId,
                            nsITVServiceCallback* aCallback)
{
  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIMutableArray> overlayIds = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!overlayIds) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // TODO Implement in follow-up patches.

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(aCallback, overlayIds);
  return NS_DispatchToCurrentThread(runnable);
}

} // namespace dom
} // namespace mozilla
