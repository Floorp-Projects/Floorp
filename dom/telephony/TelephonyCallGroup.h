/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_telephonycallgroup_h__
#define mozilla_dom_telephony_telephonycallgroup_h__

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TelephonyCallGroupBinding.h"
#include "mozilla/dom/telephony/TelephonyCommon.h"

namespace mozilla {
namespace dom {

class TelephonyCallGroup final : public DOMEventTargetHelper
{
  nsRefPtr<Telephony> mTelephony;

  nsTArray<nsRefPtr<TelephonyCall> > mCalls;

  nsRefPtr<CallsList> mCallsList;

  TelephonyCallGroupState mState;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TelephonyCallGroup,
                                           DOMEventTargetHelper)

  friend class Telephony;

  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL interface
  already_AddRefed<CallsList>
  Calls() const;

  already_AddRefed<Promise>
  Add(TelephonyCall& aCall, ErrorResult& aRv);

  already_AddRefed<Promise>
  Add(TelephonyCall& aCall, TelephonyCall& aSecondCall, ErrorResult& aRv);

  already_AddRefed<Promise>
  Remove(TelephonyCall& aCall, ErrorResult& aRv);

  already_AddRefed<Promise>
  HangUp(ErrorResult& aRv);

  already_AddRefed<Promise>
  Hold(ErrorResult& aRv);

  already_AddRefed<Promise>
  Resume(ErrorResult& aRv);

  TelephonyCallGroupState
  State() const
  {
    return mState;
  }

  bool
  IsActive() {
    return mState == TelephonyCallGroupState::Connected;
  }

  IMPL_EVENT_HANDLER(statechange)
  IMPL_EVENT_HANDLER(connected)
  IMPL_EVENT_HANDLER(held)
  IMPL_EVENT_HANDLER(callschanged)
  IMPL_EVENT_HANDLER(error)

  static already_AddRefed<TelephonyCallGroup>
  Create(Telephony* aTelephony);

  void
  AddCall(TelephonyCall* aCall);

  void
  RemoveCall(TelephonyCall* aCall);

  already_AddRefed<TelephonyCall>
  GetCall(uint32_t aServiceId, uint32_t aCallIndex);

  const nsTArray<nsRefPtr<TelephonyCall> >&
  CallsArray() const
  {
    return mCalls;
  }

  // Update its call state according to the calls wihtin itself.
  void
  ChangeState();

  nsresult
  NotifyError(const nsAString& aName, const nsAString& aMessage);

private:
  explicit TelephonyCallGroup(nsPIDOMWindow* aOwner);
  ~TelephonyCallGroup();

  nsresult
  Hold(nsITelephonyCallback* aCallback);

  nsresult
  Resume(nsITelephonyCallback* aCallback);

  nsresult
  NotifyStateChanged();

  nsresult
  NotifyCallsChanged(TelephonyCall* aCall);

  void
  ChangeStateInternal(TelephonyCallGroupState aState);

  nsresult
  DispatchCallEvent(const nsAString& aType,
                    TelephonyCall* aCall);

  already_AddRefed<Promise>
  CreatePromise(ErrorResult& aRv);

  bool CanConference(const TelephonyCall& aCall, const TelephonyCall* aSecondCall);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_telephony_telephonycallgroup_h__
