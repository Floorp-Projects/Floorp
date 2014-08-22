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

NS_IMPL_ISUPPORTS(TVSourceListener, nsITVSourceListener)

void
TVSourceListener::RegisterSource(TVSource* aSource)
{
  nsString tunerId;
  nsRefPtr<TVTuner> tuner = aSource->Tuner();
  tuner->GetId(tunerId);

  nsRefPtrHashtable<nsStringHashKey, TVSource>* tunerSources = nullptr;
  if (!mSources.Get(tunerId, &tunerSources)) {
    tunerSources = new nsRefPtrHashtable<nsStringHashKey, TVSource>();
    mSources.Put(tunerId, tunerSources);
  }

  nsString sourceType = ToTVSourceTypeStr(aSource->Type());
  tunerSources->Put(sourceType, aSource);
}

void
TVSourceListener::UnregisterSource(TVSource* aSource)
{
  nsString tunerId;
  nsRefPtr<TVTuner> tuner = aSource->Tuner();
  tuner->GetId(tunerId);

  nsRefPtrHashtable<nsStringHashKey, TVSource>* tunerSources = nullptr;
  if (!mSources.Get(tunerId, &tunerSources)) {
    return;
  }

  nsString sourceType = ToTVSourceTypeStr(aSource->Type());
  tunerSources->Remove(sourceType);
}

/* virtual */ NS_IMETHODIMP
TVSourceListener::NotifyChannelScanned(const nsAString& aTunerId,
                                       const nsAString& aSourceType,
                                       nsITVChannelData* aChannelData)
{
  nsRefPtr<TVSource> source = GetSource(aTunerId, aSourceType);
  source->NotifyChannelScanned(aChannelData);
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVSourceListener::NotifyChannelScanComplete(const nsAString& aTunerId,
                                            const nsAString& aSourceType)
{
  nsRefPtr<TVSource> source = GetSource(aTunerId, aSourceType);
  source->NotifyChannelScanComplete();
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVSourceListener::NotifyChannelScanStopped(const nsAString& aTunerId,
                                           const nsAString& aSourceType)
{
  nsRefPtr<TVSource> source = GetSource(aTunerId, aSourceType);
  source->NotifyChannelScanStopped();
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVSourceListener::NotifyEITBroadcasted(const nsAString& aTunerId,
                                       const nsAString& aSourceType,
                                       nsITVChannelData* aChannelData,
                                       nsITVProgramData** aProgramDataList,
                                       const uint32_t aCount)
{
  nsRefPtr<TVSource> source = GetSource(aTunerId, aSourceType);
  source->NotifyEITBroadcasted(aChannelData, aProgramDataList, aCount);
  return NS_OK;
}

already_AddRefed<TVSource>
TVSourceListener::GetSource(const nsAString& aTunerId,
                            const nsAString& aSourceType)
{
  nsRefPtrHashtable<nsStringHashKey, TVSource>* tunerSources = nullptr;
  if (!mSources.Get(aTunerId, &tunerSources)) {
    return nullptr;
  }

  nsRefPtr<TVSource> source;
  tunerSources->Get(aSourceType, getter_AddRefs(source));
  return source.forget();
}

} // namespace dom
} // namespace mozilla
