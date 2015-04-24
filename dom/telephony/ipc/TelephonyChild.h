/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_TelephonyChild_h
#define mozilla_dom_telephony_TelephonyChild_h

#include "mozilla/dom/telephony/PTelephonyChild.h"
#include "mozilla/dom/telephony/PTelephonyRequestChild.h"
#include "mozilla/dom/telephony/TelephonyCommon.h"
#include "nsITelephonyCallInfo.h"
#include "nsITelephonyService.h"

BEGIN_TELEPHONY_NAMESPACE

class TelephonyIPCService;

class TelephonyChild : public PTelephonyChild
{
public:
  explicit TelephonyChild(TelephonyIPCService* aService);

protected:
  virtual ~TelephonyChild();

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual PTelephonyRequestChild*
  AllocPTelephonyRequestChild(const IPCTelephonyRequest& aRequest) override;

  virtual bool
  DeallocPTelephonyRequestChild(PTelephonyRequestChild* aActor) override;

  virtual bool
  RecvNotifyCallError(const uint32_t& aClientId, const int32_t& aCallIndex,
                      const nsString& aError) override;

  virtual bool
  RecvNotifyCallStateChanged(nsTArray<nsITelephonyCallInfo*>&& aAllInfo) override;

  virtual bool
  RecvNotifyCdmaCallWaiting(const uint32_t& aClientId,
                            const IPCCdmaWaitingCallData& aData) override;

  virtual bool
  RecvNotifyConferenceCallStateChanged(const uint16_t& aCallState) override;

  virtual bool
  RecvNotifyConferenceError(const nsString& aName,
                            const nsString& aMessage) override;

  virtual bool
  RecvNotifySupplementaryService(const uint32_t& aClientId,
                                 const int32_t& aCallIndex,
                                 const uint16_t& aNotification) override;

private:
  nsRefPtr<TelephonyIPCService> mService;
};

class TelephonyRequestChild : public PTelephonyRequestChild
{
public:
  TelephonyRequestChild(nsITelephonyListener* aListener,
                        nsITelephonyCallback* aCallback);

protected:
  virtual ~TelephonyRequestChild() {}

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool
  Recv__delete__(const IPCTelephonyResponse& aResponse) override;

  virtual bool
  RecvNotifyEnumerateCallState(nsITelephonyCallInfo* const& aInfo) override;

  virtual bool
  RecvNotifyDialMMI(const nsString& aServiceCode) override;

private:
  bool
  DoResponse(const SuccessResponse& aResponse);

  bool
  DoResponse(const ErrorResponse& aResponse);

  bool
  DoResponse(const DialResponseCallSuccess& aResponse);

  bool
  DoResponse(const DialResponseMMISuccess& aResponse);

  bool
  DoResponse(const DialResponseMMIError& aResponse);

  nsCOMPtr<nsITelephonyListener> mListener;
  nsCOMPtr<nsITelephonyCallback> mCallback;
};

END_TELEPHONY_NAMESPACE

#endif // mozilla_dom_telephony_TelephonyChild_h
