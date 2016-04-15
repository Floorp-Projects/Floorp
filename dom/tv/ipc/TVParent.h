/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TVParent_h__
#define mozilla_dom_TVParent_h__

#include "mozilla/dom/PTVParent.h"
#include "mozilla/dom/PTVRequestParent.h"
#include "mozilla/dom/PTVTypes.h"
#include "nsITVService.h"

namespace mozilla {
namespace dom {

class TVParent final : public PTVParent
                     , public nsITVSourceListener
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITVSOURCELISTENER

  TVParent();

  bool Init();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool RecvPTVRequestConstructor(PTVRequestParent* aActor,
                                         const TVIPCRequest& aRequest) override;

  virtual PTVRequestParent*
  AllocPTVRequestParent(const TVIPCRequest& aRequest) override;

  virtual bool DeallocPTVRequestParent(PTVRequestParent* aActor) override;

  virtual bool RecvRegisterSourceHandler(const nsString& aTunerId,
                                         const nsString& aSourceType) override;

  virtual bool RecvUnregisterSourceHandler(
    const nsString& aTunerId, const nsString& aSourceType) override;

private:
  virtual ~TVParent();

  nsCOMPtr<nsITVService> mService;
};

class TVRequestParent final : public PTVRequestParent,
                              public nsISupports
{
  friend class TVParent;

public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit TVRequestParent(nsITVService* aService);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  nsresult SendResponse(const TVIPCResponse& aResponse);

private:
  virtual ~TVRequestParent();

  void DoRequest(const TVGetTunersRequest& aRequest);

  void DoRequest(const TVSetSourceRequest& aRequest);

  void DoRequest(const TVStartScanningChannelsRequest& aRequest);

  void DoRequest(const TVStopScanningChannelsRequest& aRequest);

  void DoRequest(const TVClearScannedChannelsCacheRequest& aRequest);

  void DoRequest(const TVSetChannelRequest& aRequest);

  void DoRequest(const TVGetChannelsRequest& aRequest);

  void DoRequest(const TVGetProgramsRequest& aRequest);

  nsCOMPtr<nsITVService> mService;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TVParent_h__
