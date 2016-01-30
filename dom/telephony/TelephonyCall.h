/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_telephonycall_h__
#define mozilla_dom_telephony_telephonycall_h__

#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TelephonyCallBinding.h"
#include "mozilla/dom/TelephonyCallId.h"
#include "mozilla/dom/telephony/TelephonyCommon.h"

#include "nsITelephonyService.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class TelephonyCall final : public DOMEventTargetHelper
{
  RefPtr<Telephony> mTelephony;
  RefPtr<TelephonyCallGroup> mGroup;

  RefPtr<TelephonyCallId> mId;
  RefPtr<TelephonyCallId> mSecondId;

  uint32_t mServiceId;
  TelephonyCallState mState;
  bool mEmergency;
  RefPtr<DOMError> mError;
  Nullable<TelephonyCallDisconnectedReason> mDisconnectedReason;

  bool mSwitchable;
  bool mMergeable;

  uint32_t mCallIndex;
  bool mLive;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TelephonyCall,
                                           DOMEventTargetHelper)
  friend class Telephony;

  nsPIDOMWindowInner*
  GetParentObject() const
  {
    return GetOwner();
  }

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  already_AddRefed<TelephonyCallId>
  Id() const;

  already_AddRefed<TelephonyCallId>
  GetSecondId() const;

  TelephonyCallState
  State() const
  {
    return mState;
  }

  bool
  Emergency() const
  {
    return mEmergency;
  }

  bool
  Switchable() const
  {
    return mSwitchable;
  }

  bool
  Mergeable() const
  {
    return mMergeable;
  }

  bool
  IsActive() const
  {
    return mState == TelephonyCallState::Dialing ||
           mState == TelephonyCallState::Alerting ||
           mState == TelephonyCallState::Connected;
  }

  already_AddRefed<DOMError>
  GetError() const;

  Nullable<TelephonyCallDisconnectedReason>
  GetDisconnectedReason() const
  {
    return mDisconnectedReason;
  }

  already_AddRefed<TelephonyCallGroup>
  GetGroup() const;

  already_AddRefed<Promise>
  Answer(ErrorResult& aRv);

  already_AddRefed<Promise>
  HangUp(ErrorResult& aRv);

  already_AddRefed<Promise>
  Hold(ErrorResult& aRv);

  already_AddRefed<Promise>
  Resume(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(statechange)
  IMPL_EVENT_HANDLER(dialing)
  IMPL_EVENT_HANDLER(alerting)
  IMPL_EVENT_HANDLER(connected)
  IMPL_EVENT_HANDLER(disconnected)
  IMPL_EVENT_HANDLER(held)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(groupchange)

  static TelephonyCallState
  ConvertToTelephonyCallState(uint32_t aCallState);

  static already_AddRefed<TelephonyCall>
  Create(Telephony* aTelephony, TelephonyCallId* aId,
         uint32_t aServiceId, uint32_t aCallIndex, TelephonyCallState aState,
         bool aEmergency = false, bool aConference = false,
         bool aSwitchable = true, bool aMergeable = true);

  void
  ChangeState(TelephonyCallState aState)
  {
    ChangeStateInternal(aState, true);
  }

  nsresult
  NotifyStateChanged();

  uint32_t
  ServiceId() const
  {
    return mServiceId;
  }

  uint32_t
  CallIndex() const
  {
    return mCallIndex;
  }

  void
  UpdateEmergency(bool aEmergency)
  {
    mEmergency = aEmergency;
  }

  void
  UpdateSwitchable(bool aSwitchable) {
    mSwitchable = aSwitchable;
  }

  void
  UpdateMergeable(bool aMergeable) {
    mMergeable = aMergeable;
  }

  void
  UpdateSecondId(TelephonyCallId* aId) {
    mSecondId = aId;
  }

  void
  NotifyError(const nsAString& aError);

  void
  UpdateDisconnectedReason(const nsAString& aDisconnectedReason);

  void
  ChangeGroup(TelephonyCallGroup* aGroup);

private:
  explicit TelephonyCall(nsPIDOMWindowInner* aOwner);

  ~TelephonyCall();

  nsresult
  Hold(nsITelephonyCallback* aCallback);

  nsresult
  Resume(nsITelephonyCallback* aCallback);

  void
  ChangeStateInternal(TelephonyCallState aState, bool aFireEvents);

  nsresult
  DispatchCallEvent(const nsAString& aType,
                    TelephonyCall* aCall);

  already_AddRefed<Promise>
  CreatePromise(ErrorResult& aRv);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_telephony_telephonycall_h__
