/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_telephonycallgroup_h__
#define mozilla_dom_telephony_telephonycallgroup_h__

#include "TelephonyCommon.h"

BEGIN_TELEPHONY_NAMESPACE

class TelephonyCallGroup MOZ_FINAL : public nsDOMEventTargetHelper
{
  nsRefPtr<Telephony> mTelephony;

  nsTArray<nsRefPtr<TelephonyCall> > mCalls;

  nsRefPtr<CallsList> mCallsList;

  nsString mState;

  uint16_t mCallState;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TelephonyCallGroup,
                                           nsDOMEventTargetHelper)

  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // WebIDL interface
  already_AddRefed<CallsList>
  Calls() const;

  void
  Add(TelephonyCall& aCall, ErrorResult& aRv);

  void
  Add(TelephonyCall& aCall, TelephonyCall& aSecondCall, ErrorResult& aRv);

  void
  Remove(TelephonyCall& aCall, ErrorResult& aRv);

  void
  Hold(ErrorResult& aRv);

  void
  Resume(ErrorResult& aRv);

  void
  GetState(nsString& aState) const
  {
    aState = mState;
  }

  IMPL_EVENT_HANDLER(statechange)
  IMPL_EVENT_HANDLER(connected)
  IMPL_EVENT_HANDLER(holding)
  IMPL_EVENT_HANDLER(held)
  IMPL_EVENT_HANDLER(resuming)
  IMPL_EVENT_HANDLER(callschanged)

  static already_AddRefed<TelephonyCallGroup>
  Create(Telephony* aTelephony);

  void
  AddCall(TelephonyCall* aCall);

  void
  RemoveCall(TelephonyCall* aCall);

  already_AddRefed<TelephonyCall>
  GetCall(uint32_t aCallIndex);

  const nsTArray<nsRefPtr<TelephonyCall> >&
  CallsArray() const
  {
    return mCalls;
  }

  void
  ChangeState(uint16_t aCallState);

  uint16_t
  CallState() const
  {
    return mCallState;
  }

private:
  TelephonyCallGroup();
  ~TelephonyCallGroup();

  nsresult
  NotifyCallsChanged(TelephonyCall* aCall);

  nsresult
  DispatchCallEvent(const nsAString& aType,
                    TelephonyCall* aCall);

  bool CanConference(const TelephonyCall& aCall, TelephonyCall* aSecondCall);
};

END_TELEPHONY_NAMESPACE

#endif // mozilla_dom_telephony_telephonycallgroup_h__
