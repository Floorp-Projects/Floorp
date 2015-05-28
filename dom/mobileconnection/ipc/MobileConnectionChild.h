/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobileconnection_MobileConnectionChild_h
#define mozilla_dom_mobileconnection_MobileConnectionChild_h

#include "mozilla/dom/MobileConnectionInfo.h"
#include "mozilla/dom/mobileconnection/PMobileConnectionChild.h"
#include "mozilla/dom/mobileconnection/PMobileConnectionRequestChild.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsIMobileConnectionService.h"
#include "nsIVariant.h"

class nsIMobileConnectionCallback;

namespace mozilla {
namespace dom {
namespace mobileconnection {

/**
 * Child actor of PMobileConnection. The object is created by
 * MobileConnectionIPCService and destroyed after MobileConnectionIPCService is
 * shutdown. For multi-sim device, more than one instance will
 * be created and each instance represents a sim slot.
 */
class MobileConnectionChild final : public PMobileConnectionChild
                                  , public nsIMobileConnection
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMOBILECONNECTION

  explicit MobileConnectionChild(uint32_t aServiceId);

  void
  Init();

  void
  Shutdown();

private:
  MobileConnectionChild() = delete;

  // final suppresses -Werror,-Wdelete-non-virtual-dtor
  ~MobileConnectionChild()
  {
    MOZ_COUNT_DTOR(MobileConnectionChild);
  }

protected:
  bool
  SendRequest(const MobileConnectionRequest& aRequest,
              nsIMobileConnectionCallback* aCallback);

  virtual void
  ActorDestroy(ActorDestroyReason why) override;

  virtual PMobileConnectionRequestChild*
  AllocPMobileConnectionRequestChild(const MobileConnectionRequest& request) override;

  virtual bool
  DeallocPMobileConnectionRequestChild(PMobileConnectionRequestChild* aActor) override;

  virtual bool
  RecvNotifyVoiceInfoChanged(nsIMobileConnectionInfo* const& aInfo) override;

  virtual bool
  RecvNotifyDataInfoChanged(nsIMobileConnectionInfo* const& aInfo) override;

  virtual bool
  RecvNotifyDataError(const nsString& aMessage) override;

  virtual bool
  RecvNotifyCFStateChanged(const uint16_t& aAction, const uint16_t& aReason,
                           const nsString& aNumber, const uint16_t& aTimeSeconds,
                           const uint16_t& aServiceClass) override;

  virtual bool
  RecvNotifyEmergencyCbModeChanged(const bool& aActive,
                                   const uint32_t& aTimeoutMs) override;

  virtual bool
  RecvNotifyOtaStatusChanged(const nsString& aStatus) override;

  virtual bool
  RecvNotifyRadioStateChanged(const int32_t& aRadioState) override;

  virtual bool
  RecvNotifyClirModeChanged(const uint32_t& aMode) override;

  virtual bool
  RecvNotifyLastNetworkChanged(const nsString& aNetwork) override;

  virtual bool
  RecvNotifyLastHomeNetworkChanged(const nsString& aNetwork) override;

  virtual bool
  RecvNotifyNetworkSelectionModeChanged(const int32_t& aMode) override;

private:
  uint32_t mServiceId;
  bool mLive;
  nsCOMArray<nsIMobileConnectionListener> mListeners;
  nsRefPtr<MobileConnectionInfo> mVoice;
  nsRefPtr<MobileConnectionInfo> mData;
  int32_t mRadioState;
  nsString mLastNetwork;
  nsString mLastHomeNetwork;
  int32_t mNetworkSelectionMode;
  nsTArray<int32_t> mSupportedNetworkTypes;
};

/******************************************************************************
 * PMobileConnectionRequestChild
 ******************************************************************************/

/**
 * Child actor of PMobileConnectionRequest. The object is created when an
 * asynchronous request is made and destroyed after receiving the response sent
 * by parent actor.
 */
class MobileConnectionRequestChild : public PMobileConnectionRequestChild
{
public:
  explicit MobileConnectionRequestChild(nsIMobileConnectionCallback* aRequestCallback)
    : mRequestCallback(aRequestCallback)
  {
    MOZ_COUNT_CTOR(MobileConnectionRequestChild);
    MOZ_ASSERT(mRequestCallback);
  }

  bool
  DoReply(const MobileConnectionReplySuccess& aReply);

  bool
  DoReply(const MobileConnectionReplySuccessBoolean& aReply);

  bool
  DoReply(const MobileConnectionReplySuccessNetworks& aReply);

  bool
  DoReply(const MobileConnectionReplySuccessCallForwarding& aReply);

  bool
  DoReply(const MobileConnectionReplySuccessCallBarring& aReply);

  bool
  DoReply(const MobileConnectionReplySuccessCallWaiting& aReply);

  bool
  DoReply(const MobileConnectionReplySuccessClirStatus& aReply);

  bool
  DoReply(const MobileConnectionReplySuccessPreferredNetworkType& aReply);

  bool
  DoReply(const MobileConnectionReplySuccessRoamingPreference& aMode);

  bool
  DoReply(const MobileConnectionReplyError& aReply);

protected:
  virtual
  ~MobileConnectionRequestChild()
  {
    MOZ_COUNT_DTOR(MobileConnectionRequestChild);
  }

  virtual void
  ActorDestroy(ActorDestroyReason why) override;

  virtual bool
  Recv__delete__(const MobileConnectionReply& aReply) override;

private:
  nsCOMPtr<nsIMobileConnectionCallback> mRequestCallback;
};

} // namespace mobileconnection
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobileconnection_MobileConnectionChild_h
