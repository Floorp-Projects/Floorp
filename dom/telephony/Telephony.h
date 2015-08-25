/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_telephony_h__
#define mozilla_dom_telephony_telephony_h__

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/telephony/TelephonyCommon.h"

#include "nsITelephonyCallInfo.h"
#include "nsITelephonyService.h"

// Need to include TelephonyCall.h because we have inline methods that
// assume they see the definition of TelephonyCall.
#include "TelephonyCall.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {
namespace telephony {

class TelephonyDialCallback;

} // namespace telephony

class OwningTelephonyCallOrTelephonyCallGroup;

class Telephony final : public DOMEventTargetHelper,
                        private nsITelephonyListener
{
  /**
   * Class Telephony doesn't actually expose nsITelephonyListener.
   * Instead, it owns an nsITelephonyListener derived instance mListener
   * and passes it to nsITelephonyService. The onreceived events are first
   * delivered to mListener and then forwarded to its owner, Telephony. See
   * also bug 775997 comment #51.
   */
  class Listener;

  friend class telephony::TelephonyDialCallback;

  nsCOMPtr<nsITelephonyService> mService;
  nsRefPtr<Listener> mListener;

  nsTArray<nsRefPtr<TelephonyCall> > mCalls;
  nsRefPtr<CallsList> mCallsList;

  nsRefPtr<TelephonyCallGroup> mGroup;

  nsRefPtr<Promise> mReadyPromise;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSITELEPHONYLISTENER
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(DOMEventTargetHelper)
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Telephony,
                                           DOMEventTargetHelper)

  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }

  // WrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  already_AddRefed<Promise>
  Dial(const nsAString& aNumber, const Optional<uint32_t>& aServiceId,
       ErrorResult& aRv);

  already_AddRefed<Promise>
  DialEmergency(const nsAString& aNumber, const Optional<uint32_t>& aServiceId,
                ErrorResult& aRv);

  already_AddRefed<Promise>
  SendTones(const nsAString& aDTMFChars,
            uint32_t aPauseDuration,
            uint32_t aToneDuration,
            const Optional<uint32_t>& aServiceId,
            ErrorResult& aRv);

  void
  StartTone(const nsAString& aDTMFChar, const Optional<uint32_t>& aServiceId,
            ErrorResult& aRv);

  void
  StopTone(const Optional<uint32_t>& aServiceId, ErrorResult& aRv);

  bool
  GetMuted(ErrorResult& aRv) const;

  void
  SetMuted(bool aMuted, ErrorResult& aRv);

  bool
  GetSpeakerEnabled(ErrorResult& aRv) const;

  void
  SetSpeakerEnabled(bool aEnabled, ErrorResult& aRv);

  void
  GetActive(Nullable<OwningTelephonyCallOrTelephonyCallGroup>& aValue);

  already_AddRefed<CallsList>
  Calls() const;

  already_AddRefed<TelephonyCallGroup>
  ConferenceGroup() const;

  already_AddRefed<Promise>
  GetReady(ErrorResult& aRv) const;

  IMPL_EVENT_HANDLER(incoming)
  IMPL_EVENT_HANDLER(callschanged)
  IMPL_EVENT_HANDLER(remoteheld)
  IMPL_EVENT_HANDLER(remoteresumed)

  static already_AddRefed<Telephony>
  Create(nsPIDOMWindow* aOwner, ErrorResult& aRv);

  void
  AddCall(TelephonyCall* aCall)
  {
    NS_ASSERTION(!mCalls.Contains(aCall), "Already know about this one!");
    mCalls.AppendElement(aCall);
    NotifyCallsChanged(aCall);
  }

  void
  RemoveCall(TelephonyCall* aCall)
  {
    NS_ASSERTION(mCalls.Contains(aCall), "Didn't know about this one!");
    mCalls.RemoveElement(aCall);
    NotifyCallsChanged(aCall);
  }

  nsITelephonyService*
  Service() const
  {
    return mService;
  }

  const nsTArray<nsRefPtr<TelephonyCall> >&
  CallsArray() const
  {
    return mCalls;
  }

private:
  explicit Telephony(nsPIDOMWindow* aOwner);
  ~Telephony();

  void
  Shutdown();

  static bool
  IsValidNumber(const nsAString& aNumber);

  static uint32_t
  GetNumServices();

  static bool
  IsValidServiceId(uint32_t aServiceId);

  static bool
  IsActiveState(uint16_t aCallState);

  uint32_t
  GetServiceId(const Optional<uint32_t>& aServiceId,
               bool aGetIfActiveCall = false);

  already_AddRefed<Promise>
  DialInternal(uint32_t aServiceId, const nsAString& aNumber, bool aEmergency,
               ErrorResult& aRv);

  already_AddRefed<TelephonyCallId>
  CreateCallId(nsITelephonyCallInfo *aInfo);

  already_AddRefed<TelephonyCallId>
  CreateCallId(const nsAString& aNumber,
               uint16_t aNumberPresentation = nsITelephonyService::CALL_PRESENTATION_ALLOWED,
               const nsAString& aName = EmptyString(),
               uint16_t aNamePresentation = nsITelephonyService::CALL_PRESENTATION_ALLOWED);

  already_AddRefed<TelephonyCall>
  CreateCall(TelephonyCallId* aId,
             uint32_t aServiceId, uint32_t aCallIndex, uint16_t aCallState,
             bool aEmergency = false, bool aConference = false,
             bool aSwitchable = true, bool aMergeable = true);

  nsresult
  NotifyEvent(const nsAString& aType);

  nsresult
  NotifyCallsChanged(TelephonyCall* aCall);

  nsresult
  DispatchCallEvent(const nsAString& aType, TelephonyCall* aCall);

  already_AddRefed<TelephonyCall>
  GetCall(uint32_t aServiceId, uint32_t aCallIndex);

  already_AddRefed<TelephonyCall>
  GetCallFromEverywhere(uint32_t aServiceId, uint32_t aCallIndex);

  nsresult
  HandleCallInfo(nsITelephonyCallInfo* aInfo);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_telephony_telephony_h__
