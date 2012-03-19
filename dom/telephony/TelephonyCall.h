/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Telephony.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_dom_telephony_telephonycall_h__
#define mozilla_dom_telephony_telephonycall_h__

#include "TelephonyCommon.h"

#include "nsIDOMTelephonyCall.h"
#include "nsIRadioInterfaceLayer.h"

class nsPIDOMWindow;

BEGIN_TELEPHONY_NAMESPACE

class TelephonyCall : public nsDOMEventTargetHelper,
                      public nsIDOMTelephonyCall
{
  NS_DECL_EVENT_HANDLER(statechange)
  NS_DECL_EVENT_HANDLER(dialing)
  NS_DECL_EVENT_HANDLER(alerting)
  NS_DECL_EVENT_HANDLER(busy)
  NS_DECL_EVENT_HANDLER(connecting)
  NS_DECL_EVENT_HANDLER(connected)
  NS_DECL_EVENT_HANDLER(disconnecting)
  NS_DECL_EVENT_HANDLER(disconnected)
  NS_DECL_EVENT_HANDLER(incoming)

  nsRefPtr<Telephony> mTelephony;

  nsString mNumber;
  nsString mState;

  PRUint32 mCallIndex;
  PRUint16 mCallState;
  bool mLive;
  bool mOutgoing;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMTELEPHONYCALL
  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TelephonyCall,
                                           nsDOMEventTargetHelper)

  static already_AddRefed<TelephonyCall>
  Create(Telephony* aTelephony, const nsAString& aNumber, PRUint16 aCallState,
         PRUint32 aCallIndex = kOutgoingPlaceholderCallIndex);

  nsIDOMEventTarget*
  ToIDOMEventTarget() const
  {
    return static_cast<nsDOMEventTargetHelper*>(
             const_cast<TelephonyCall*>(this));
  }

  nsISupports*
  ToISupports() const
  {
    return ToIDOMEventTarget();
  }

  void
  ChangeState(PRUint16 aCallState)
  {
    ChangeStateInternal(aCallState, true);
  }

  PRUint32
  CallIndex() const
  {
    return mCallIndex;
  }

  void
  UpdateCallIndex(PRUint32 aCallIndex)
  {
    NS_ASSERTION(mCallIndex == kOutgoingPlaceholderCallIndex,
                 "Call index should not be set!");
    mCallIndex = aCallIndex;
  }

  PRUint16
  CallState() const
  {
    return mCallState;
  }

  bool
  IsOutgoing() const
  {
    return mOutgoing;
  }

private:
  TelephonyCall()
  : mCallIndex(kOutgoingPlaceholderCallIndex),
    mCallState(nsIRadioInterfaceLayer::CALL_STATE_UNKNOWN), mLive(false), mOutgoing(false)
  { }

  ~TelephonyCall()
  { }

  void
  ChangeStateInternal(PRUint16 aCallState, bool aFireEvents);
};

END_TELEPHONY_NAMESPACE

#endif // mozilla_dom_telephony_telephonycall_h__
