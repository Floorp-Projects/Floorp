/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TVParent.h"

#include "mozilla/unused.h"
#include "TVTypes.h"
#include "TVIPCHelper.h"

namespace mozilla {
namespace dom {

/*
 * Implementation of TVParent
 */

NS_IMPL_ISUPPORTS(TVParent, nsITVSourceListener)

TVParent::TVParent() {}

TVParent::~TVParent() {}

bool
TVParent::Init()
{
  MOZ_ASSERT(!mService);
  mService = do_GetService(TV_SERVICE_CONTRACTID);
  return NS_WARN_IF(!mService) ? false : true;
}

/* virtual */ void
TVParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mService = nullptr;
}

/* virtual */ bool
TVParent::RecvPTVRequestConstructor(PTVRequestParent* aActor,
                                    const TVIPCRequest& aRequest)
{
  TVRequestParent* actor = static_cast<TVRequestParent*>(aActor);

  switch (aRequest.type()) {
    case TVIPCRequest::TTVGetTunersRequest:
      actor->DoRequest(aRequest.get_TVGetTunersRequest());
      break;
    case TVIPCRequest::TTVSetSourceRequest:
      actor->DoRequest(aRequest.get_TVSetSourceRequest());
      break;
    case TVIPCRequest::TTVStartScanningChannelsRequest:
      actor->DoRequest(aRequest.get_TVStartScanningChannelsRequest());
      break;
    case TVIPCRequest::TTVStopScanningChannelsRequest:
      actor->DoRequest(aRequest.get_TVStopScanningChannelsRequest());
      break;
    case TVIPCRequest::TTVClearScannedChannelsCacheRequest:
      actor->DoRequest(aRequest.get_TVClearScannedChannelsCacheRequest());
      break;
    case TVIPCRequest::TTVSetChannelRequest:
      actor->DoRequest(aRequest.get_TVSetChannelRequest());
      break;
    case TVIPCRequest::TTVGetChannelsRequest:
      actor->DoRequest(aRequest.get_TVGetChannelsRequest());
      break;
    case TVIPCRequest::TTVGetProgramsRequest:
      actor->DoRequest(aRequest.get_TVGetProgramsRequest());
      break;
    default:
      MOZ_CRASH("Unknown TVIPCRequest type");
  }

  return true;
}

/* virtual */ PTVRequestParent*
TVParent::AllocPTVRequestParent(const TVIPCRequest& aRequest)
{
  MOZ_ASSERT(mService);
  RefPtr<TVRequestParent> actor = new TVRequestParent(mService);
  return actor.forget().take();
}

/* virtual */ bool
TVParent::DeallocPTVRequestParent(PTVRequestParent* aActor)
{
  RefPtr<TVRequestParent> actor =
    dont_AddRef(static_cast<TVRequestParent*>(aActor));
  return true;
}

/* virtual */ bool
TVParent::RecvRegisterSourceHandler(const nsString& aTunerId,
                                    const nsString& aSourceType)
{
  MOZ_ASSERT(mService);
  NS_WARN_IF(
    NS_FAILED(mService->RegisterSourceListener(aTunerId, aSourceType, this)));
  return true;
}

/* virtual */ bool
TVParent::RecvUnregisterSourceHandler(const nsString& aTunerId,
                                      const nsString& aSourceType)
{
  MOZ_ASSERT(mService);
  NS_WARN_IF(
    NS_FAILED(mService->UnregisterSourceListener(aTunerId, aSourceType, this)));
  return true;
}

/* virtual */ NS_IMETHODIMP
TVParent::NotifyChannelScanned(const nsAString& aTunerId,
                               const nsAString& aSourceType,
                               nsITVChannelData* aChannelData)
{
  TVIPCChannelData channelData;
  PackTVIPCChannelData(aChannelData, channelData);
  if (NS_WARN_IF(!SendNotifyChannelScanned(nsAutoString(aTunerId),
                                           nsAutoString(aSourceType),
                                           channelData))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVParent::NotifyChannelScanComplete(const nsAString& aTunerId,
                                    const nsAString& aSourceType)
{
  if (NS_WARN_IF(!SendNotifyChannelScanComplete(nsAutoString(aTunerId),
                                                nsAutoString(aSourceType)))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVParent::NotifyChannelScanStopped(const nsAString& aTunerId,
                                   const nsAString& aSourceType)
{
  if (NS_WARN_IF(!SendNotifyChannelScanStopped(nsAutoString(aTunerId),
                                               nsAutoString(aSourceType)))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

/* virtual */ NS_IMETHODIMP
TVParent::NotifyEITBroadcasted(const nsAString& aTunerId,
                               const nsAString& aSourceType,
                               nsITVChannelData* aChannelData,
                               nsITVProgramData** aProgramDataList,
                               uint32_t aCount)
{
  TVIPCChannelData ipcChannelData;
  nsTArray<TVIPCProgramData> programDataList;

  PackTVIPCChannelData(aChannelData, ipcChannelData);
  for (uint32_t idx = 0; idx < aCount; idx++) {
    TVIPCProgramData ipcProgramData;
    PackTVIPCProgramData(aProgramDataList[idx], ipcProgramData);
    programDataList.AppendElement(ipcProgramData);
  }

  if (NS_WARN_IF(!SendNotifyEITBroadcasted(nsAutoString(aTunerId),
                                           nsAutoString(aSourceType),
                                           ipcChannelData, programDataList))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

/*
 * Implementation of TVRequestBaseCallback
 */

class TVRequestBaseCallback : public nsITVServiceCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITVSERVICECALLBACK

  explicit TVRequestBaseCallback(TVRequestParent* aRequestParent)
    : mRequestParent(aRequestParent)
  {
    MOZ_ASSERT(aRequestParent);
  }

protected:
  virtual ~TVRequestBaseCallback() {}

  RefPtr<TVRequestParent> mRequestParent;
};

NS_IMPL_ISUPPORTS(TVRequestBaseCallback, nsITVServiceCallback)

/* virtual */ NS_IMETHODIMP
TVRequestBaseCallback::NotifySuccess(nsIArray* aDataList)
{
  if (NS_WARN_IF(aDataList)) {
    return NS_ERROR_INVALID_ARG;
  }

  MOZ_ASSERT(mRequestParent);

  nsresult rv;
  rv = mRequestParent->SendResponse(TVSuccessResponse());
  mRequestParent = nullptr;

  return rv;
}

/* virtual */ NS_IMETHODIMP
TVRequestBaseCallback::NotifyError(uint16_t aErrorCode)
{
  MOZ_ASSERT(mRequestParent);

  nsresult rv;
  rv = mRequestParent->SendResponse(TVErrorResponse(aErrorCode));
  mRequestParent = nullptr;

  return rv;
}

/*
 * Implementation of TVRequestTunerGetterCallback
 */

class TVRequestTunerGetterCallback final : public TVRequestBaseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  explicit TVRequestTunerGetterCallback(TVRequestParent* aRequestParent)
    : TVRequestBaseCallback(aRequestParent)
  {
  }

  NS_IMETHOD NotifySuccess(nsIArray* aDataList) override;

private:
  virtual ~TVRequestTunerGetterCallback() {}
};

NS_IMPL_ISUPPORTS_INHERITED0(TVRequestTunerGetterCallback,
                             TVRequestBaseCallback)

/* virtual */ NS_IMETHODIMP
TVRequestTunerGetterCallback::NotifySuccess(nsIArray* aDataList)
{
  if (NS_WARN_IF(!aDataList)) {
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t length;
  nsresult rv = aDataList->GetLength(&length);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsTArray<TVIPCTunerData> tuners(length);
  for (uint32_t i = 0; i < length; i++) {
    nsCOMPtr<nsITVTunerData> tunerData = do_QueryElementAt(aDataList, i);
    if (NS_WARN_IF(!tunerData)) {
      return NS_ERROR_INVALID_ARG;
    }
    TVIPCTunerData ipcTunerData;
    PackTVIPCTunerData(tunerData, ipcTunerData);

    tuners.AppendElement(ipcTunerData);
  }

  MOZ_ASSERT(mRequestParent);

  rv = mRequestParent->SendResponse(TVGetTunersResponse(tuners));
  mRequestParent = nullptr;

  return rv;
}

/*
 * Implementation of TVRequestSourceSetterCallback
 */

class TVRequestSourceSetterCallback final : public TVRequestBaseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  explicit TVRequestSourceSetterCallback(TVRequestParent* aRequestParent)
    : TVRequestBaseCallback(aRequestParent)
  {
  }

  NS_IMETHOD NotifySuccess(nsIArray* aDataList) override;

private:
  virtual ~TVRequestSourceSetterCallback() {}
};

NS_IMPL_ISUPPORTS_INHERITED0(TVRequestSourceSetterCallback,
                             TVRequestBaseCallback)

/* virtual */ NS_IMETHODIMP
TVRequestSourceSetterCallback::NotifySuccess(nsIArray* aDataList)
{
  if (NS_WARN_IF(!aDataList)) {
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t length;
  nsresult rv = aDataList->GetLength(&length);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(length != 1)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsITVGonkNativeHandleData> handleData =
    do_QueryElementAt(aDataList, 0);
  if (NS_WARN_IF(!handleData)) {
    return NS_ERROR_INVALID_ARG;
  }
  TVIPCGonkNativeHandleData ipcHandleData;
  PackTVIPCGonkNativeHandleData(handleData, ipcHandleData);

  MOZ_ASSERT(mRequestParent);

  rv = mRequestParent->SendResponse(TVSetSourceResponse(ipcHandleData));
  mRequestParent = nullptr;

  return rv;
}

/*
 * Implementation of TVRequestChannelSetterCallback
 */

class TVRequestChannelSetterCallback final : public TVRequestBaseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  explicit TVRequestChannelSetterCallback(TVRequestParent* aRequestParent)
    : TVRequestBaseCallback(aRequestParent)
  {
  }

  NS_IMETHOD NotifySuccess(nsIArray* aDataList) override;

private:
  virtual ~TVRequestChannelSetterCallback() {}
};

NS_IMPL_ISUPPORTS_INHERITED0(TVRequestChannelSetterCallback,
                             TVRequestBaseCallback)

/* virtual */ NS_IMETHODIMP
TVRequestChannelSetterCallback::NotifySuccess(nsIArray* aDataList)
{
  if (NS_WARN_IF(!aDataList)) {
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t length;
  nsresult rv = aDataList->GetLength(&length);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(length != 1)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsITVChannelData> channelData = do_QueryElementAt(aDataList, 0);
  if (NS_WARN_IF(!channelData)) {
    return NS_ERROR_INVALID_ARG;
  }

  TVIPCChannelData ipcChannelData;
  PackTVIPCChannelData(channelData, ipcChannelData);

  MOZ_ASSERT(mRequestParent);

  rv = mRequestParent->SendResponse(TVSetChannelResponse(ipcChannelData));
  mRequestParent = nullptr;

  return rv;
}

/*
 * Implementation of TVRequestChannelGetterCallback
 */

class TVRequestChannelGetterCallback final : public TVRequestBaseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  explicit TVRequestChannelGetterCallback(TVRequestParent* aRequestParent)
    : TVRequestBaseCallback(aRequestParent)
  {
  }

  NS_IMETHOD NotifySuccess(nsIArray* aDataList) override;

private:
  virtual ~TVRequestChannelGetterCallback() {}
};

NS_IMPL_ISUPPORTS_INHERITED0(TVRequestChannelGetterCallback,
                             TVRequestBaseCallback)

/* virtual */ NS_IMETHODIMP
TVRequestChannelGetterCallback::NotifySuccess(nsIArray* aDataList)
{
  if (NS_WARN_IF(!aDataList)) {
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t length;
  nsresult rv = aDataList->GetLength(&length);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsTArray<TVIPCChannelData> channels(length);
  for (uint32_t i = 0; i < length; i++) {
    nsCOMPtr<nsITVChannelData> channelData = do_QueryElementAt(aDataList, i);
    TVIPCChannelData ipcChannelData;

    if (NS_WARN_IF(!channelData)) {
      return NS_ERROR_INVALID_ARG;
    }

    PackTVIPCChannelData(channelData, ipcChannelData);
    channels.AppendElement(ipcChannelData);
  }

  MOZ_ASSERT(mRequestParent);

  rv = mRequestParent->SendResponse(TVGetChannelsResponse(channels));
  mRequestParent = nullptr;

  return rv;
}

/*
 * Implementation of TVRequestProgramGetterCallback
 */

class TVRequestProgramGetterCallback final : public TVRequestBaseCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  explicit TVRequestProgramGetterCallback(TVRequestParent* aRequestParent)
    : TVRequestBaseCallback(aRequestParent)
  {
  }

  NS_IMETHOD NotifySuccess(nsIArray* aDataList) override;

private:
  virtual ~TVRequestProgramGetterCallback() {}
};

NS_IMPL_ISUPPORTS_INHERITED0(TVRequestProgramGetterCallback,
                             TVRequestBaseCallback)

/* virtual */ NS_IMETHODIMP
TVRequestProgramGetterCallback::NotifySuccess(nsIArray* aDataList)
{
  if (NS_WARN_IF(!aDataList)) {
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t length;
  nsresult rv = aDataList->GetLength(&length);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsTArray<TVIPCProgramData> programs(length);
  for (uint32_t i = 0; i < length; i++) {
    nsCOMPtr<nsITVProgramData> programData = do_QueryElementAt(aDataList, i);
    if (NS_WARN_IF(!programData)) {
      return NS_ERROR_INVALID_ARG;
    }
    TVIPCProgramData ipcProgramData;
    PackTVIPCProgramData(programData, ipcProgramData);
    programs.AppendElement(ipcProgramData);
  }

  MOZ_ASSERT(mRequestParent);

  rv = mRequestParent->SendResponse(TVGetProgramsResponse(programs));
  mRequestParent = nullptr;

  return rv;
}

/*
 * Implementation of TVRequestParent
 */

NS_IMPL_ISUPPORTS0(TVRequestParent)

TVRequestParent::TVRequestParent(nsITVService* aService) : mService(aService)
{
  MOZ_COUNT_CTOR(TVRequestParent);
}

TVRequestParent::~TVRequestParent()
{
  MOZ_COUNT_DTOR(TVRequestParent);

  mService = nullptr;
}

/* virtual */ void
TVRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mService = nullptr;
}

void
TVRequestParent::DoRequest(const TVGetTunersRequest& aRequest)
{
  MOZ_ASSERT(mService);
  nsresult rv = mService->GetTuners(new TVRequestTunerGetterCallback(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Unused << SendResponse(
      TVErrorResponse(nsITVServiceCallback::TV_ERROR_FAILURE));
  }
}

void
TVRequestParent::DoRequest(const TVSetSourceRequest& aRequest)
{
  MOZ_ASSERT(mService);

  nsresult rv = mService->SetSource(aRequest.tunerId(), aRequest.sourceType(),
                                    new TVRequestSourceSetterCallback(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Unused << SendResponse(
      TVErrorResponse(nsITVServiceCallback::TV_ERROR_FAILURE));
  }
}

void
TVRequestParent::DoRequest(const TVStartScanningChannelsRequest& aRequest)
{
  MOZ_ASSERT(mService);

  nsresult rv = mService->StartScanningChannels(
    aRequest.tunerId(), aRequest.sourceType(), new TVRequestBaseCallback(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Unused << SendResponse(
      TVErrorResponse(nsITVServiceCallback::TV_ERROR_FAILURE));
  }

}

void
TVRequestParent::DoRequest(const TVStopScanningChannelsRequest& aRequest)
{
  MOZ_ASSERT(mService);

  nsresult rv = mService->StopScanningChannels(
    aRequest.tunerId(), aRequest.sourceType(), new TVRequestBaseCallback(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Unused << SendResponse(
      TVErrorResponse(nsITVServiceCallback::TV_ERROR_FAILURE));
  }
}

void
TVRequestParent::DoRequest(const TVClearScannedChannelsCacheRequest& aRequest)
{
  MOZ_ASSERT(mService);

  nsresult rv = mService->ClearScannedChannelsCache();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Unused << SendResponse(
      TVErrorResponse(nsITVServiceCallback::TV_ERROR_FAILURE));
  }
  Unused << SendResponse(TVSuccessResponse());
}

void
TVRequestParent::DoRequest(const TVSetChannelRequest& aRequest)
{
  MOZ_ASSERT(mService);

  nsresult rv = mService->SetChannel(aRequest.tunerId(), aRequest.sourceType(),
                                     aRequest.channelNumber(),
                                     new TVRequestChannelSetterCallback(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Unused << SendResponse(
      TVErrorResponse(nsITVServiceCallback::TV_ERROR_FAILURE));
  }

}

void
TVRequestParent::DoRequest(const TVGetChannelsRequest& aRequest)
{
  MOZ_ASSERT(mService);

  nsresult rv = mService->GetChannels(aRequest.tunerId(), aRequest.sourceType(),
                                      new TVRequestChannelGetterCallback(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Unused << SendResponse(
      TVErrorResponse(nsITVServiceCallback::TV_ERROR_FAILURE));
  }
}

void
TVRequestParent::DoRequest(const TVGetProgramsRequest& aRequest)
{
  MOZ_ASSERT(mService);

  nsresult rv = mService->GetPrograms(aRequest.tunerId(),
                                      aRequest.sourceType(),
                                      aRequest.channelNumber(),
                                      aRequest.startTime(),
                                      aRequest.endTime(),
                                      new TVRequestProgramGetterCallback(this));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Unused << SendResponse(
      TVErrorResponse(nsITVServiceCallback::TV_ERROR_FAILURE));
  }
}

nsresult
TVRequestParent::SendResponse(const TVIPCResponse& aResponse)
{
  if (NS_WARN_IF(!Send__delete__(this, aResponse))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
