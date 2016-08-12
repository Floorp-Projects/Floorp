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

NS_IMPL_CYCLE_COLLECTION(TVSourceListener, mSources)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TVSourceListener)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TVSourceListener)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TVSourceListener)
  NS_INTERFACE_MAP_ENTRY(nsITVSourceListener)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

void
TVSourceListener::RegisterSource(TVSource* aSource)
{
  mSources.AppendElement(aSource);
}

void
TVSourceListener::UnregisterSource(TVSource* aSource)
{
  for (uint32_t i = 0; i < mSources.Length(); i++) {
    if (mSources[i] == aSource) {
      mSources.RemoveElementsAt(i, 1);
    }
  }
}

NS_IMETHODIMP
TVSourceListener::NotifyChannelScanned(const nsAString& aTunerId,
                                       const nsAString& aSourceType,
                                       nsITVChannelData* aChannelData)
{
  RefPtr<TVSource> source = GetSource(aTunerId, aSourceType);
  source->NotifyChannelScanned(aChannelData);
  return NS_OK;
}

NS_IMETHODIMP
TVSourceListener::NotifyChannelScanComplete(const nsAString& aTunerId,
                                            const nsAString& aSourceType)
{
  RefPtr<TVSource> source = GetSource(aTunerId, aSourceType);
  source->NotifyChannelScanComplete();
  return NS_OK;
}

NS_IMETHODIMP
TVSourceListener::NotifyChannelScanStopped(const nsAString& aTunerId,
                                           const nsAString& aSourceType)
{
  RefPtr<TVSource> source = GetSource(aTunerId, aSourceType);
  source->NotifyChannelScanStopped();
  return NS_OK;
}

NS_IMETHODIMP
TVSourceListener::NotifyEITBroadcasted(const nsAString& aTunerId,
                                       const nsAString& aSourceType,
                                       nsITVChannelData* aChannelData,
                                       nsITVProgramData** aProgramDataList,
                                       const uint32_t aCount)
{
  RefPtr<TVSource> source = GetSource(aTunerId, aSourceType);
  source->NotifyEITBroadcasted(aChannelData, aProgramDataList, aCount);
  return NS_OK;
}

already_AddRefed<TVSource>
TVSourceListener::GetSource(const nsAString& aTunerId,
                            const nsAString& aSourceType)
{
  for (uint32_t i = 0; i < mSources.Length(); i++) {
    nsString tunerId;
    RefPtr<TVTuner> tuner = mSources[i]->Tuner();
    tuner->GetId(tunerId);

    nsString sourceType = ToTVSourceTypeStr(mSources[i]->Type());

    if (aTunerId.Equals(tunerId) && aSourceType.Equals(sourceType)) {
      RefPtr<TVSource> source = mSources[i];
      return source.forget();
    }
  }

  return nullptr;
}

} // namespace dom
} // namespace mozilla
