/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVChild_h
#define mozilla_dom_TVChild_h

#include "mozilla/dom/PTVChild.h"
#include "mozilla/dom/PTVRequestChild.h"
#include "mozilla/dom/PTVTypes.h"

class nsITVServiceCallback;

namespace mozilla {
namespace dom {

class TVIPCService;

class TVChild final : public PTVChild
{
public:
  explicit TVChild(TVIPCService* aService);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PTVRequestChild*
  AllocPTVRequestChild(const TVIPCRequest& aRequest) override;

  virtual bool DeallocPTVRequestChild(PTVRequestChild* aActor) override;

  virtual bool
  RecvNotifyChannelScanned(const nsString& aTunerId,
                           const nsString& aSourceType,
                           const TVIPCChannelData& aChannelData) override;

  virtual bool
  RecvNotifyChannelScanComplete(const nsString& aTunerId,
                                const nsString& aSourceType) override;

  virtual bool
  RecvNotifyChannelScanStopped(const nsString& aTunerId,
                               const nsString& aSourceType) override;

  virtual bool
  RecvNotifyEITBroadcasted(
                        const nsString& aTunerId,
                        const nsString& aSourceType,
                        const TVIPCChannelData& aChannelData,
                        nsTArray<TVIPCProgramData>&& aProgramDataList) override;

  virtual ~TVChild();

private:
  bool mActorDestroyed;
  RefPtr<TVIPCService> mService;
};

class TVRequestChild final : public PTVRequestChild
{
public:
  explicit TVRequestChild(nsITVServiceCallback* aCallback);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool Recv__delete__(const TVIPCResponse& aResponse) override;

private:
  virtual ~TVRequestChild();

  void DoResponse(const TVSuccessResponse& aResponse);

  void DoResponse(const TVErrorResponse& aResponse);

  void DoResponse(const TVGetTunersResponse& aResponse);

  void DoResponse(const TVSetSourceResponse& aResponse);

  void DoResponse(const TVSetChannelResponse& aResponse);

  void DoResponse(const TVGetChannelsResponse& aResponse);

  void DoResponse(const TVGetProgramsResponse& aResponse);

  bool mActorDestroyed;
  nsCOMPtr<nsITVServiceCallback> mCallback;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVChild_h
