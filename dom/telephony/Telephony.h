/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_telephony_h__
#define mozilla_dom_telephony_telephony_h__

#include "TelephonyCommon.h"

#include "nsIDOMTelephony.h"
#include "nsIDOMTelephonyCall.h"
#include "nsITelephonyProvider.h"

class nsIScriptContext;
class nsPIDOMWindow;

BEGIN_TELEPHONY_NAMESPACE

class Telephony : public nsDOMEventTargetHelper,
                  public nsIDOMTelephony
{
  /**
   * Class Telephony doesn't actually inherit nsITelephonyListener.
   * Instead, it owns an nsITelephonyListener derived instance mListener
   * and passes it to nsITelephonyProvider. The onreceived events are first
   * delivered to mListener and then forwarded to its owner, Telephony. See
   * also bug 775997 comment #51.
   */
  class Listener;

  class EnumerationAck;
  friend class EnumerationAck;

  nsCOMPtr<nsITelephonyProvider> mProvider;
  nsRefPtr<Listener> mListener;

  TelephonyCall* mActiveCall;
  nsTArray<nsRefPtr<TelephonyCall> > mCalls;

  // Cached calls array object. Cleared whenever mCalls changes and then rebuilt
  // once a page looks for the liveCalls attribute.
  JSObject* mCallsArray;

  bool mRooted;
  bool mEnumerated;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMTELEPHONY
  NS_DECL_NSITELEPHONYLISTENER
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
                                                   Telephony,
                                                   nsDOMEventTargetHelper)

  static already_AddRefed<Telephony>
  Create(nsPIDOMWindow* aOwner, nsITelephonyProvider* aProvider);

  nsISupports*
  ToISupports()
  {
    return static_cast<EventTarget*>(this);
  }

  void
  AddCall(TelephonyCall* aCall)
  {
    NS_ASSERTION(!mCalls.Contains(aCall), "Already know about this one!");
    mCalls.AppendElement(aCall);
    mCallsArray = nullptr;
    NotifyCallsChanged(aCall);
  }

  void
  RemoveCall(TelephonyCall* aCall)
  {
    NS_ASSERTION(mCalls.Contains(aCall), "Didn't know about this one!");
    mCalls.RemoveElement(aCall);
    mCallsArray = nullptr;
    NotifyCallsChanged(aCall);
  }

  nsITelephonyProvider*
  Provider() const
  {
    return mProvider;
  }

  virtual void EventListenerAdded(nsIAtom* aType) MOZ_OVERRIDE;

private:
  Telephony();
  ~Telephony();

  already_AddRefed<TelephonyCall>
  CreateNewDialingCall(const nsAString& aNumber);

  void
  NoteDialedCallFromOtherInstance(const nsAString& aNumber);

  nsresult
  NotifyCallsChanged(TelephonyCall* aCall);

  nsresult
  DialInternal(bool isEmergency,
               const nsAString& aNumber,
               nsIDOMTelephonyCall** aResult);

  nsresult
  DispatchCallEvent(const nsAString& aType,
                    nsIDOMTelephonyCall* aCall);

  void
  EnqueueEnumerationAck();
};

END_TELEPHONY_NAMESPACE

#endif // mozilla_dom_telephony_telephony_h__
