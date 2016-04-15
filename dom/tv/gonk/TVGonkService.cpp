/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TVGonkService.h"

#include "mozilla/dom/TVServiceRunnables.h"
#include "nsCOMPtr.h"
#include "nsIMutableArray.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(TVGonkService, nsITVService)

TVGonkService::TVGonkService() {}

TVGonkService::~TVGonkService() {}

NS_IMETHODIMP
TVGonkService::RegisterSourceListener(const nsAString& aTunerId,
                                      const nsAString& aSourceType,
                                      nsITVSourceListener* aListener)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aTunerId.IsEmpty());
  MOZ_ASSERT(!aSourceType.IsEmpty());
  MOZ_ASSERT(aListener);

  mSourceListenerTuples.AppendElement(new TVSourceListenerTuple(
    nsString(aTunerId), nsString(aSourceType), aListener));
  return NS_OK;
}

NS_IMETHODIMP
TVGonkService::UnregisterSourceListener(const nsAString& aTunerId,
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

  return NS_OK;
}

void
TVGonkService::GetSourceListeners(
  const nsAString& aTunerId, const nsAString& aSourceType,
  nsTArray<nsCOMPtr<nsITVSourceListener> >& aListeners) const
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
TVGonkService::GetTuners(nsITVServiceCallback* aCallback)
{
  MOZ_ASSERT(aCallback);

  // TODO Bug 1229308 - Communicate with TV daemon process.

  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVGonkService::SetSource(const nsAString& aTunerId,
                         const nsAString& aSourceType,
                         nsITVServiceCallback* aCallback)
{
  MOZ_ASSERT(!aTunerId.IsEmpty());
  MOZ_ASSERT(!aSourceType.IsEmpty());
  MOZ_ASSERT(aCallback);

  // TODO Bug 1229308 - Communicate with TV daemon process.

  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVGonkService::StartScanningChannels(const nsAString& aTunerId,
                                     const nsAString& aSourceType,
                                     nsITVServiceCallback* aCallback)
{
  MOZ_ASSERT(!aTunerId.IsEmpty());
  MOZ_ASSERT(!aSourceType.IsEmpty());
  MOZ_ASSERT(aCallback);

  // TODO Bug 1229308 - Communicate with TV daemon process.

  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVGonkService::StopScanningChannels(const nsAString& aTunerId,
                                    const nsAString& aSourceType,
                                    nsITVServiceCallback* aCallback)
{
  MOZ_ASSERT(!aTunerId.IsEmpty());
  MOZ_ASSERT(!aSourceType.IsEmpty());
  MOZ_ASSERT(aCallback);

  // TODO Bug 1229308 - Communicate with TV daemon process.

  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVGonkService::ClearScannedChannelsCache()
{
  // TODO Bug 1229308 - Communicate with TV daemon process.

  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVGonkService::SetChannel(const nsAString& aTunerId,
                          const nsAString& aSourceType,
                          const nsAString& aChannelNumber,
                          nsITVServiceCallback* aCallback)
{
  MOZ_ASSERT(!aTunerId.IsEmpty());
  MOZ_ASSERT(!aSourceType.IsEmpty());
  MOZ_ASSERT(aCallback);

  // TODO Bug 1229308 - Communicate with TV daemon process.

  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVGonkService::GetChannels(const nsAString& aTunerId,
                           const nsAString& aSourceType,
                           nsITVServiceCallback* aCallback)
{
  MOZ_ASSERT(!aTunerId.IsEmpty());
  MOZ_ASSERT(!aSourceType.IsEmpty());
  MOZ_ASSERT(aCallback);

  // TODO Bug 1229308 - Communicate with TV daemon process.

  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVGonkService::GetPrograms(const nsAString& aTunerId,
                           const nsAString& aSourceType,
                           const nsAString& aChannelNumber, uint64_t startTime,
                           uint64_t endTime, nsITVServiceCallback* aCallback)
{
  MOZ_ASSERT(!aTunerId.IsEmpty());
  MOZ_ASSERT(!aSourceType.IsEmpty());
  MOZ_ASSERT(!aChannelNumber.IsEmpty());
  MOZ_ASSERT(aCallback);

  // TODO Bug 1229308 - Communicate with TV daemon process.

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
