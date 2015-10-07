/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TVServiceRunnables.h"
#include "mozilla/dom/TVTypes.h"
#include "nsCOMPtr.h"
#include "nsIMutableArray.h"
#include "nsITimer.h"
#include "nsServiceManagerUtils.h"
#include "prtime.h"
#include "FakeTVService.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(FakeTVService)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(FakeTVService)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSourceListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTuners)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mChannels)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPrograms)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEITBroadcastedTimer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mScanCompleteTimer)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(FakeTVService)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSourceListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTuners)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChannels)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPrograms)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEITBroadcastedTimer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mScanCompleteTimer)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(FakeTVService)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FakeTVService)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FakeTVService)
  NS_INTERFACE_MAP_ENTRY(nsITVService)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

FakeTVService::FakeTVService()
{
  Init();
}

FakeTVService::~FakeTVService()
{
  Shutdown();
}

void
FakeTVService::Init()
{
  const char* sourceTypes1[2] = {"dvb-t", "dvb-c"};
  nsCOMPtr<nsITVTunerData> tunerData1 = MockTuner(NS_LITERAL_STRING("1"), 2, sourceTypes1);
  mTuners.AppendElement(tunerData1);
  const char* sourceTypes2[1] = {"dvb-s"};
  nsCOMPtr<nsITVTunerData> tunerData2 = MockTuner(NS_LITERAL_STRING("2"), 1, sourceTypes2);
  mTuners.AppendElement(tunerData2);

  nsCOMPtr<nsITVChannelData> channelData1 =
    MockChannel(NS_LITERAL_STRING("networkId1"), NS_LITERAL_STRING("transportStreamId1"),
                NS_LITERAL_STRING("serviceId1"), NS_LITERAL_STRING("tv"),
                NS_LITERAL_STRING("1"), NS_LITERAL_STRING("name1"), true, true);
  mChannels.AppendElement(channelData1);
  nsCOMPtr<nsITVChannelData> channelData2 =
    MockChannel(NS_LITERAL_STRING("networkId2"), NS_LITERAL_STRING("transportStreamId2"),
                NS_LITERAL_STRING("serviceId2"), NS_LITERAL_STRING("radio"),
                NS_LITERAL_STRING("2"), NS_LITERAL_STRING("name2"), true, true);
  mChannels.AppendElement(channelData2);

  uint64_t now = PR_Now();
  const char* audioLanguages1[2] = {"eng", "jpn"};
  const char* subtitleLanguages1[2] = {"fre", "spa"};
  nsCOMPtr<nsITVProgramData> programData1 =
    MockProgram(NS_LITERAL_STRING("eventId1"), NS_LITERAL_STRING("title1"),
                now - 1, 3600000,
                NS_LITERAL_STRING("description1"), NS_LITERAL_STRING("rating1"),
                2, audioLanguages1, 2, subtitleLanguages1);
  mPrograms.AppendElement(programData1);
  nsCOMPtr<nsITVProgramData> programData2 =
      MockProgram(NS_LITERAL_STRING("eventId2"), NS_LITERAL_STRING("title2"),
                  now + 3600000 , 3600000,
                  NS_LITERAL_STRING(""), NS_LITERAL_STRING(""),
                  0, nullptr, 0, nullptr);
  mPrograms.AppendElement(programData2);
}

void
FakeTVService::Shutdown()
{
  if (mEITBroadcastedTimer) {
    mEITBroadcastedTimer->Cancel();
  }
  if (mScanCompleteTimer) {
    mScanCompleteTimer->Cancel();
  }
}

/* virtual */ NS_IMETHODIMP
FakeTVService::GetSourceListener(nsITVSourceListener** aSourceListener)
{
  if (!mSourceListener) {
    *aSourceListener = nullptr;
    return NS_OK;
  }

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

  for (uint32_t i = 0; i < mTuners.Length(); i++) {
    tunerDataList->AppendElement(mTuners[i], false);
  }

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

  for (uint32_t i = 0; i < mTuners.Length(); i++) {
    nsString tunerId;
    mTuners[i]->GetId(tunerId);
    if (aTunerId.Equals(tunerId)) {
      uint32_t sourceTypeCount;
      char** sourceTypes;
      mTuners[i]->GetSupportedSourceTypes(&sourceTypeCount, &sourceTypes);
      for (uint32_t j = 0; j < sourceTypeCount; j++) {
        nsString sourceType;
        sourceType.AssignASCII(sourceTypes[j]);
        if (aSourceType.Equals(sourceType)) {
          NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(sourceTypeCount, sourceTypes);
          nsCOMPtr<nsIRunnable> runnable =
            new TVServiceNotifyRunnable(aCallback, nullptr);
          return NS_DispatchToCurrentThread(runnable);
        }
      }
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(sourceTypeCount, sourceTypes);
    }
  }

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(aCallback, nullptr, nsITVServiceCallback::TV_ERROR_FAILURE);
  return NS_DispatchToCurrentThread(runnable);
}

class EITBroadcastedCallback final : public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS

  EITBroadcastedCallback(const nsAString& aTunerId,
                         const nsAString& aSourceType,
                         nsITVSourceListener* aSourceListener,
                         nsITVChannelData* aChannelData)
    : mTunerId(aTunerId)
    , mSourceType(aSourceType)
    , mSourceListener(aSourceListener)
    , mChannelData(aChannelData)
  {}

  NS_IMETHODIMP
  Notify(nsITimer* aTimer) override
  {
    // Notify mock EIT broadcasting.
    nsITVProgramData** programDataList =
      static_cast<nsITVProgramData **>(moz_xmalloc(1 * sizeof(nsITVProgramData*)));
    programDataList[0] = new TVProgramData();
    programDataList[0]->SetEventId(NS_LITERAL_STRING("eventId"));
    programDataList[0]->SetTitle(NS_LITERAL_STRING("title"));
    programDataList[0]->SetStartTime(PR_Now() + 3600000);
    programDataList[0]->SetDuration(3600000);
    programDataList[0]->SetDescription(NS_LITERAL_STRING("description"));
    programDataList[0]->SetRating(NS_LITERAL_STRING("rating"));
    programDataList[0]->SetAudioLanguages(0, nullptr);
    programDataList[0]->SetSubtitleLanguages(0, nullptr);
    nsresult rv = mSourceListener->NotifyEITBroadcasted(mTunerId, mSourceType,
                                                        mChannelData,
                                                        programDataList, 1);
    NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(1, programDataList);
    return rv;
  }

private:
  ~EITBroadcastedCallback() {}

  nsString mTunerId;
  nsString mSourceType;
  nsCOMPtr<nsITVSourceListener> mSourceListener;
  nsCOMPtr<nsITVChannelData> mChannelData;
};

NS_IMPL_ISUPPORTS(EITBroadcastedCallback, nsITimerCallback)

class ScanCompleteCallback final : public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS

  ScanCompleteCallback(const nsAString& aTunerId,
                       const nsAString& aSourceType,
                       nsITVSourceListener* aSourceListener)
    : mTunerId(aTunerId)
    , mSourceType(aSourceType)
    , mSourceListener(aSourceListener)
  {}

  NS_IMETHODIMP
  Notify(nsITimer* aTimer) override
  {
    return mSourceListener->NotifyChannelScanComplete(mTunerId, mSourceType);
  }

private:
  ~ScanCompleteCallback() {}

  nsString mTunerId;
  nsString mSourceType;
  nsCOMPtr<nsITVSourceListener> mSourceListener;
};

NS_IMPL_ISUPPORTS(ScanCompleteCallback, nsITimerCallback)

/* virtual */ NS_IMETHODIMP
FakeTVService::StartScanningChannels(const nsAString& aTunerId,
                                     const nsAString& aSourceType,
                                     nsITVServiceCallback* aCallback)
{
  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(aCallback, nullptr);
  nsresult rv = NS_DispatchToCurrentThread(runnable);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsAllowed(aTunerId, aSourceType)) {
    rv = mSourceListener->NotifyChannelScanned(aTunerId, aSourceType, mChannels[0]);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set a timer. |notifyEITBroadcasted| will be called after the timer
    // fires (10ms). (The timer could be canceled if |StopScanningChannels| gets
    // called before firing.)
    mEITBroadcastedTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    NS_ENSURE_TRUE(mEITBroadcastedTimer, NS_ERROR_OUT_OF_MEMORY);
    nsRefPtr<EITBroadcastedCallback> eitBroadcastedCb =
      new EITBroadcastedCallback(aTunerId, aSourceType, mSourceListener, mChannels[0]);
    rv = mEITBroadcastedTimer->InitWithCallback(eitBroadcastedCb, 10,
                                                nsITimer::TYPE_ONE_SHOT);
    NS_ENSURE_SUCCESS(rv, rv);

    // Set a timer. |notifyChannelScanComplete| will be called after the timer
    // fires (20ms). (The timer could be canceled if |StopScanningChannels| gets
    // called before firing.)
    mScanCompleteTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    NS_ENSURE_TRUE(mScanCompleteTimer, NS_ERROR_OUT_OF_MEMORY);
    nsRefPtr<ScanCompleteCallback> scanCompleteCb =
      new ScanCompleteCallback(aTunerId, aSourceType, mSourceListener);
    rv = mScanCompleteTimer->InitWithCallback(scanCompleteCb, 20,
                                              nsITimer::TYPE_ONE_SHOT);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
FakeTVService::StopScanningChannels(const nsAString& aTunerId,
                                    const nsAString& aSourceType,
                                    nsITVServiceCallback* aCallback)
{
  if (!aCallback) {
    return NS_ERROR_INVALID_ARG;
  }

  if (mEITBroadcastedTimer) {
    mEITBroadcastedTimer->Cancel();
    mEITBroadcastedTimer = nullptr;
  }
  if (mScanCompleteTimer) {
    mScanCompleteTimer->Cancel();
    mScanCompleteTimer = nullptr;
  }
  nsresult rv = mSourceListener->NotifyChannelScanStopped(aTunerId, aSourceType);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(aCallback, nullptr);
  return NS_DispatchToCurrentThread(runnable);
}

/* virtual */ NS_IMETHODIMP
FakeTVService::ClearScannedChannelsCache()
{
  // Fake service doesn't support channel cache, so there's nothing to do here.
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

  if (IsAllowed(aTunerId, aSourceType)) {
    for (uint32_t i = 0; i < mChannels.Length(); i++) {
      nsString channelNumber;
      mChannels[i]->GetNumber(channelNumber);
      if (aChannelNumber.Equals(channelNumber)) {
        channelDataList->AppendElement(mChannels[i], false);
        break;
      }
    }
  }

  uint32_t length;
  nsresult rv = channelDataList->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRunnable> runnable = new TVServiceNotifyRunnable(
    aCallback,
    (length == 1) ? channelDataList : nullptr,
    (length == 1) ? nsITVServiceCallback::TV_ERROR_OK : nsITVServiceCallback::TV_ERROR_FAILURE
  );
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

  if (IsAllowed(aTunerId, aSourceType)) {
    for (uint32_t i = 0; i < mChannels.Length(); i++) {
      channelDataList->AppendElement(mChannels[i], false);
    }
  }

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

  // Only return mock programs for the first channel.
  nsString channelNumber;
  mChannels[0]->GetNumber(channelNumber);
  if (IsAllowed(aTunerId, aSourceType) && aChannelNumber.Equals(channelNumber)) {
    for (uint32_t i = 0; i < mPrograms.Length(); i++) {
      programDataList->AppendElement(mPrograms[i], false);
    }
  }

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

bool
FakeTVService::IsAllowed(const nsAString& aTunerId,
                         const nsAString& aSourceType)
{
  // Only allow for the first source of the first tuner.
  nsString tunerId;
  mTuners[0]->GetId(tunerId);
  if (!aTunerId.Equals(tunerId)) {
    return false;
  }

  uint32_t sourceTypeCount;
  char** sourceTypes;
  mTuners[0]->GetSupportedSourceTypes(&sourceTypeCount, &sourceTypes);
  nsString sourceType;
  sourceType.AssignASCII(sourceTypes[0]);
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(sourceTypeCount, sourceTypes);
  if (!aSourceType.Equals(sourceType)) {
    return false;
  }

  return true;
}

already_AddRefed<nsITVTunerData>
FakeTVService::MockTuner(const nsAString& aId,
                         uint32_t aSupportedSourceTypeCount,
                         const char** aSupportedSourceTypes)
{
  nsCOMPtr<nsITVTunerData> tunerData = new TVTunerData();
  tunerData->SetId(aId);
  tunerData->SetSupportedSourceTypes(aSupportedSourceTypeCount, aSupportedSourceTypes);
  return tunerData.forget();
}

already_AddRefed<nsITVChannelData>
FakeTVService::MockChannel(const nsAString& aNetworkId,
                           const nsAString& aTransportStreamId,
                           const nsAString& aServiceId,
                           const nsAString& aType,
                           const nsAString& aNumber,
                           const nsAString& aName,
                           bool aIsEmergency,
                           bool aIsFree)
{
  nsCOMPtr<nsITVChannelData> channelData = new TVChannelData();
  channelData->SetNetworkId(aNetworkId);
  channelData->SetTransportStreamId(aTransportStreamId);
  channelData->SetServiceId(aServiceId);
  channelData->SetType(aType);
  channelData->SetNumber(aNumber);
  channelData->SetName(aName);
  channelData->SetIsEmergency(aIsEmergency);
  channelData->SetIsFree(aIsFree);
  return channelData.forget();
}

already_AddRefed<nsITVProgramData>
FakeTVService::MockProgram(const nsAString& aEventId,
                           const nsAString& aTitle,
                           uint64_t aStartTime,
                           uint64_t aDuration,
                           const nsAString& aDescription,
                           const nsAString& aRating,
                           uint32_t aAudioLanguageCount,
                           const char** aAudioLanguages,
                           uint32_t aSubtitleLanguageCount,
                           const char** aSubtitleLanguages)
{
  nsCOMPtr<nsITVProgramData> programData = new TVProgramData();
  programData->SetEventId(aEventId);
  programData->SetTitle(aTitle);
  programData->SetStartTime(aStartTime);
  programData->SetDuration(aDuration);
  programData->SetDescription(aDescription);
  programData->SetRating(aRating);
  programData->SetAudioLanguages(aAudioLanguageCount, aAudioLanguages);
  programData->SetSubtitleLanguages(aSubtitleLanguageCount, aSubtitleLanguages);
  return programData.forget();
}

} // namespace dom
} // namespace mozilla
