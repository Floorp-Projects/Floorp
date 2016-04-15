/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TVSource.h"
#include "mozilla/dom/TVTuner.h"
#include "mozilla/dom/TVUtils.h"
#include "TVListeners.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(TVSourceListener)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(TVSourceListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSource)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(TVSourceListener)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSource)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(TVSourceListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TVSourceListener)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TVSourceListener)
  NS_INTERFACE_MAP_ENTRY(nsITVSourceListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */ already_AddRefed<TVSourceListener>
TVSourceListener::Create(TVSource* aSource)
{
  RefPtr<TVSourceListener> listener = new TVSourceListener(aSource);
  return (listener->Init()) ? listener.forget() : nullptr;
}

TVSourceListener::TVSourceListener(TVSource* aSource) : mSource(aSource)
{
  MOZ_ASSERT(mSource);
}

TVSourceListener::~TVSourceListener()
{
  Shutdown();
}

bool
TVSourceListener::Init()
{
  RefPtr<TVTuner> tuner = mSource->Tuner();
  tuner->GetId(mTunerId);

  nsCOMPtr<nsITVService> service = do_GetService(TV_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return false;
  }

  nsresult rv = service->RegisterSourceListener(
    mTunerId, ToTVSourceTypeStr(mSource->Type()), this);
  return NS_WARN_IF(NS_FAILED(rv)) ? false : true;
}

void
TVSourceListener::Shutdown()
{
  nsCOMPtr<nsITVService> service = do_GetService(TV_SERVICE_CONTRACTID);
  if (NS_WARN_IF(!service)) {
    return;
  }

  nsresult rv = service->UnregisterSourceListener(
    mTunerId, ToTVSourceTypeStr(mSource->Type()), this);
  NS_WARN_IF(NS_FAILED(rv));
}

/* virtual */ NS_IMETHODIMP
TVSourceListener::NotifyChannelScanned(const nsAString& aTunerId,
                                       const nsAString& aSourceType,
                                       nsITVChannelData* aChannelData)
{
  if (NS_WARN_IF(!IsMatched(aTunerId, aSourceType))) {
    return NS_ERROR_INVALID_ARG;
  }

  return mSource->NotifyChannelScanned(aChannelData);
}

/* virtual */ NS_IMETHODIMP
TVSourceListener::NotifyChannelScanComplete(const nsAString& aTunerId,
                                            const nsAString& aSourceType)
{
  if (NS_WARN_IF(!IsMatched(aTunerId, aSourceType))) {
    return NS_ERROR_INVALID_ARG;
  }

  return mSource->NotifyChannelScanComplete();
}

/* virtual */ NS_IMETHODIMP
TVSourceListener::NotifyChannelScanStopped(const nsAString& aTunerId,
                                           const nsAString& aSourceType)
{
  if (NS_WARN_IF(!IsMatched(aTunerId, aSourceType))) {
    return NS_ERROR_INVALID_ARG;
  }

  return mSource->NotifyChannelScanStopped();
}

/* virtual */ NS_IMETHODIMP
TVSourceListener::NotifyEITBroadcasted(const nsAString& aTunerId,
                                       const nsAString& aSourceType,
                                       nsITVChannelData* aChannelData,
                                       nsITVProgramData** aProgramDataList,
                                       const uint32_t aCount)
{
  if (NS_WARN_IF(!IsMatched(aTunerId, aSourceType))) {
    return NS_ERROR_INVALID_ARG;
  }

  return mSource->NotifyEITBroadcasted(aChannelData, aProgramDataList, aCount);
}

bool
TVSourceListener::IsMatched(const nsAString& aTunerId,
                            const nsAString& aSourceType)
{
  return aTunerId.Equals(mTunerId) &&
         ToTVSourceType(aSourceType) == mSource->Type();
}

} // namespace dom
} // namespace mozilla
