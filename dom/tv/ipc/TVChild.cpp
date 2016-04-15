/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TVChild.h"

#include "mozilla/unused.h"
#include "nsIMutableArray.h"
#include "TVIPCHelper.h"
#include "TVIPCService.h"
#include "TVServiceRunnables.h"
#include "TVTuner.h"

using namespace mozilla;

namespace mozilla {
namespace dom {

/*
 * Implementation of TVChild
 */

TVChild::TVChild(TVIPCService* aService)
  : mActorDestroyed(false), mService(aService)
{
  MOZ_ASSERT(mService);

  MOZ_COUNT_CTOR(TVChild);
}

TVChild::~TVChild()
{
  MOZ_COUNT_DTOR(TVChild);

  if (!mActorDestroyed) {
    Send__delete__(this);
  }
  mService = nullptr;
}

/* virtual */ void
TVChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorDestroyed = true;
  mService = nullptr;
}

/* virtual */ PTVRequestChild*
TVChild::AllocPTVRequestChild(const TVIPCRequest& aRequest)
{
  NS_NOTREACHED(
    "We should never be manually allocating PTVRequestChild actors");
  return nullptr;
}

/* virtual */ bool
TVChild::DeallocPTVRequestChild(PTVRequestChild* aActor)
{
  delete aActor;
  return true;
}

/* virtual */ bool
TVChild::RecvNotifyChannelScanned(const nsString& aTunerId,
                                  const nsString& aSourceType,
                                  const TVIPCChannelData& aChannelData)
{
  if (mService) {
    nsCOMPtr<nsITVChannelData> channelData;
    channelData = do_CreateInstance(TV_CHANNEL_DATA_CONTRACTID);
    UnpackTVIPCChannelData(channelData, aChannelData);
    NS_WARN_IF(NS_FAILED(
      mService->NotifyChannelScanned(aTunerId, aSourceType, channelData)));
  }
  return true;
}

/* virtual */ bool
TVChild::RecvNotifyChannelScanComplete(const nsString& aTunerId,
                                       const nsString& aSourceType)
{
  if (mService) {
    NS_WARN_IF(
      NS_FAILED(mService->NotifyChannelScanComplete(aTunerId, aSourceType)));
  }
  return true;
}

/* virtual */ bool
TVChild::RecvNotifyChannelScanStopped(const nsString& aTunerId,
                                      const nsString& aSourceType)
{
  if (mService) {
    NS_WARN_IF(
      NS_FAILED(mService->NotifyChannelScanStopped(aTunerId, aSourceType)));
  }
  return true;
}

/* virtual */ bool
TVChild::RecvNotifyEITBroadcasted(const nsString& aTunerId,
                                  const nsString& aSourceType,
                                  const TVIPCChannelData& aChannelData,
                                  nsTArray<TVIPCProgramData>&& aProgramDataList)
{
  if (mService) {
    nsCOMPtr<nsITVChannelData> channelData;
    channelData = do_CreateInstance(TV_CHANNEL_DATA_CONTRACTID);
    UnpackTVIPCChannelData(channelData, aChannelData);

    uint32_t count = aProgramDataList.Length();
    nsCOMArray<nsITVProgramData> programDataList(count);
    for (uint32_t i = 0; i < count; i++) {
      nsCOMPtr<nsITVProgramData> programData;
      programData = do_CreateInstance(TV_PROGRAM_DATA_CONTRACTID);
      UnpackTVIPCProgramData(programData, aProgramDataList[i]);
      programDataList.AppendElement(programData);
    }

    nsresult rv = mService->NotifyEITBroadcasted(
      aTunerId,
      aSourceType,
      channelData,
      const_cast<nsITVProgramData**>(programDataList.Elements()),
      count);

    NS_WARN_IF(NS_FAILED(rv));
  }
  return true;
}

/*
 * Implementation of TVRequestChild
 */

TVRequestChild::TVRequestChild(nsITVServiceCallback* aCallback)
  : mActorDestroyed(false), mCallback(aCallback)
{
  MOZ_COUNT_CTOR(TVRequestChild);
}

TVRequestChild::~TVRequestChild()
{
  MOZ_COUNT_DTOR(TVRequestChild);

  mCallback = nullptr;
}

/* virtual */ void
TVRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorDestroyed = true;
  mCallback = nullptr;
}

/* virtual */ bool
TVRequestChild::Recv__delete__(const TVIPCResponse& aResponse)
{
  if (mActorDestroyed) {
    return true;
  }

  if (!mCallback) {
    return true;
  }

  switch (aResponse.type()) {
    case TVIPCResponse::TTVSuccessResponse:
      DoResponse(aResponse.get_TVSuccessResponse());
      break;
    case TVIPCResponse::TTVErrorResponse:
      DoResponse(aResponse.get_TVErrorResponse());
      break;
    case TVIPCResponse::TTVGetTunersResponse:
      DoResponse(aResponse.get_TVGetTunersResponse());
      break;
    case TVIPCResponse::TTVSetSourceResponse:
      DoResponse(aResponse.get_TVSetSourceResponse());
      break;
    case TVIPCResponse::TTVSetChannelResponse:
      DoResponse(aResponse.get_TVSetChannelResponse());
      break;
    case TVIPCResponse::TTVGetChannelsResponse:
      DoResponse(aResponse.get_TVGetChannelsResponse());
      break;
    case TVIPCResponse::TTVGetProgramsResponse:
      DoResponse(aResponse.get_TVGetProgramsResponse());
      break;
    default:
      MOZ_CRASH("Unknown TVIPCResponse type");
  }

  return true;
}

void
TVRequestChild::DoResponse(const TVSuccessResponse& aResponse)
{
  if (mCallback) {
    Unused << mCallback->NotifySuccess(nullptr);
  }
}

void
TVRequestChild::DoResponse(const TVErrorResponse& aResponse)
{
  if (mCallback) {
    Unused << mCallback->NotifyError(aResponse.errorCode());
  }
}

void
TVRequestChild::DoResponse(const TVGetTunersResponse& aResponse)
{
  MOZ_ASSERT(mCallback);

  nsCOMPtr<nsIMutableArray> tunerDataList =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!tunerDataList) {
    NS_WARNING("OUT OF MEMORY");
    return;
  }

  const nsTArray<TVIPCTunerData>& tuners = aResponse.tuners();
  for (uint32_t i = 0; i < tuners.Length(); i++) {
    nsCOMPtr<nsITVTunerData> tunerData;
    tunerData = do_CreateInstance(TV_TUNER_DATA_CONTRACTID);
    UnpackTVIPCTunerData(tunerData, tuners[i]);
    tunerDataList->AppendElement(tunerData, false);
  }

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(mCallback, tunerDataList);
  Unused << NS_DispatchToCurrentThread(runnable);
}

void
TVRequestChild::DoResponse(const TVSetSourceResponse& aResponse)
{
  MOZ_ASSERT(mCallback);

  nsCOMPtr<nsIMutableArray> handleDataList =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!handleDataList) {
    NS_WARNING("OUT OF MEMORY");
    return;
  }

  nsCOMPtr<nsITVGonkNativeHandleData> handleData;
  handleData = do_CreateInstance(TV_GONK_NATIVE_HANDLE_DATA_CONTRACTID);
  UnpackTVIPCGonkNativeHandleData(handleData, aResponse.streamHandle());
  handleDataList->AppendElement(handleData, false);

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(mCallback, handleDataList);
  Unused << NS_DispatchToCurrentThread(runnable);
}

void
TVRequestChild::DoResponse(const TVSetChannelResponse& aResponse)
{
  MOZ_ASSERT(mCallback);

  nsCOMPtr<nsIMutableArray> channelDataList =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!channelDataList) {
    NS_WARNING("OUT OF MEMORY");
    return;
  }

  nsCOMPtr<nsITVChannelData> channelData;
  channelData = do_CreateInstance(TV_CHANNEL_DATA_CONTRACTID);
  UnpackTVIPCChannelData(channelData, aResponse.channel());
  channelDataList->AppendElement(channelData, false);

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(mCallback, channelDataList);
  Unused << NS_DispatchToCurrentThread(runnable);
}

void
TVRequestChild::DoResponse(const TVGetChannelsResponse& aResponse)
{
  MOZ_ASSERT(mCallback);

  nsCOMPtr<nsIMutableArray> channelDataList =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!channelDataList) {
    NS_WARNING("OUT OF MEMORY");
    return;
  }

  const nsTArray<TVIPCChannelData>& channels = aResponse.channels();
  for (uint32_t i = 0; i < channels.Length(); i++) {
    nsCOMPtr<nsITVChannelData> channelData;
    channelData = do_CreateInstance(TV_CHANNEL_DATA_CONTRACTID);
    UnpackTVIPCChannelData(channelData, channels[i]);

    channelDataList->AppendElement(channelData, false);
  }

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(mCallback, channelDataList);
  Unused << NS_DispatchToCurrentThread(runnable);
}

void
TVRequestChild::DoResponse(const TVGetProgramsResponse& aResponse)
{
  MOZ_ASSERT(mCallback);

  nsCOMPtr<nsIMutableArray> programDataList =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!programDataList) {
    NS_WARNING("OUT OF MEMORY");
    return;
  }

  const nsTArray<TVIPCProgramData>& programs = aResponse.programs();
  for (uint32_t i = 0; i < programs.Length(); i++) {
    nsCOMPtr<nsITVProgramData> programData;
    programData = do_CreateInstance(TV_PROGRAM_DATA_CONTRACTID);
    UnpackTVIPCProgramData(programData, programs[i]);
    programDataList->AppendElement(programData, false);
  }

  nsCOMPtr<nsIRunnable> runnable =
    new TVServiceNotifyRunnable(mCallback, programDataList);
  Unused << NS_DispatchToCurrentThread(runnable);
}

} // namespace dom
} // namespace mozilla
