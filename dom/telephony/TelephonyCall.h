/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_telephonycall_h__
#define mozilla_dom_telephony_telephonycall_h__

#include "TelephonyCommon.h"

#include "nsIDOMTelephonyCall.h"
#include "mozilla/dom/DOMError.h"

class nsPIDOMWindow;

BEGIN_TELEPHONY_NAMESPACE

class TelephonyCall : public nsDOMEventTargetHelper,
                      public nsIDOMTelephonyCall
{
  nsRefPtr<Telephony> mTelephony;

  nsString mNumber;
  nsString mState;
  bool mEmergency;
  nsRefPtr<mozilla::dom::DOMError> mError;

  uint32_t mCallIndex;
  uint16_t mCallState;
  bool mLive;
  bool mOutgoing;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMTELEPHONYCALL
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper)
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TelephonyCall,
                                           nsDOMEventTargetHelper)

  static already_AddRefed<TelephonyCall>
  Create(Telephony* aTelephony, const nsAString& aNumber, uint16_t aCallState,
         uint32_t aCallIndex = kOutgoingPlaceholderCallIndex,
         bool aEmergency = false);

  nsISupports*
  ToISupports()
  {
    return static_cast<EventTarget*>(this);
  }

  void
  ChangeState(uint16_t aCallState)
  {
    ChangeStateInternal(aCallState, true);
  }

  uint32_t
  CallIndex() const
  {
    return mCallIndex;
  }

  void
  UpdateCallIndex(uint32_t aCallIndex)
  {
    NS_ASSERTION(mCallIndex == kOutgoingPlaceholderCallIndex,
                 "Call index should not be set!");
    mCallIndex = aCallIndex;
  }

  uint16_t
  CallState() const
  {
    return mCallState;
  }

  void
  UpdateEmergency(bool aEmergency)
  {
    mEmergency = aEmergency;
  }

  bool
  IsOutgoing() const
  {
    return mOutgoing;
  }

  void
  NotifyError(const nsAString& aError);

private:
  TelephonyCall();

  ~TelephonyCall()
  { }

  void
  ChangeStateInternal(uint16_t aCallState, bool aFireEvents);

  nsresult
  DispatchCallEvent(const nsAString& aType,
                    nsIDOMTelephonyCall* aCall);
};

END_TELEPHONY_NAMESPACE

#endif // mozilla_dom_telephony_telephonycall_h__
