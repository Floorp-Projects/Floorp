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

#ifndef mozilla_dom_telephony_telephony_h__
#define mozilla_dom_telephony_telephony_h__

#include "TelephonyCommon.h"

#include "nsIDOMTelephony.h"
#include "nsIDOMTelephonyCall.h"
#include "nsIRadioInterfaceLayer.h"

class nsIScriptContext;
class nsPIDOMWindow;

BEGIN_TELEPHONY_NAMESPACE

class Telephony : public nsDOMEventTargetHelper,
                  public nsIDOMTelephony
{
  nsCOMPtr<nsIRILContentHelper> mRIL;
  nsCOMPtr<nsIRILTelephonyCallback> mRILTelephonyCallback;

  NS_DECL_EVENT_HANDLER(incoming)
  NS_DECL_EVENT_HANDLER(callschanged)

  TelephonyCall* mActiveCall;
  nsTArray<nsRefPtr<TelephonyCall> > mCalls;

  // Cached calls array object. Cleared whenever mCalls changes and then rebuilt
  // once a page looks for the liveCalls attribute.
  JSObject* mCallsArray;

  bool mRooted;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMTELEPHONY
  NS_DECL_NSIRILTELEPHONYCALLBACK
  NS_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper::)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
                                                   Telephony,
                                                   nsDOMEventTargetHelper)

  static already_AddRefed<Telephony>
  Create(nsPIDOMWindow* aOwner, nsIRILContentHelper* aRIL);

  nsIDOMEventTarget*
  ToIDOMEventTarget() const
  {
    return static_cast<nsDOMEventTargetHelper*>(
             const_cast<Telephony*>(this));
  }

  nsISupports*
  ToISupports() const
  {
    return ToIDOMEventTarget();
  }

  void
  AddCall(TelephonyCall* aCall)
  {
    NS_ASSERTION(!mCalls.Contains(aCall), "Already know about this one!");
    mCalls.AppendElement(aCall);
    mCallsArray = nsnull;
    NotifyCallsChanged(aCall);
  }

  void
  RemoveCall(TelephonyCall* aCall)
  {
    NS_ASSERTION(mCalls.Contains(aCall), "Didn't know about this one!");
    mCalls.RemoveElement(aCall);
    mCallsArray = nsnull;
    NotifyCallsChanged(aCall);
  }

  nsIRILContentHelper*
  RIL() const
  {
    return mRIL;
  }

private:
  Telephony();
  ~Telephony();

  already_AddRefed<TelephonyCall>
  CreateNewDialingCall(const nsAString& aNumber);

  void
  NoteDialedCallFromOtherInstance(const nsAString& aNumber);

  nsresult
  NotifyCallsChanged(TelephonyCall* aCall);

  class RILTelephonyCallback : public nsIRILTelephonyCallback
  {
    Telephony* mTelephony;

  public:
    NS_DECL_ISUPPORTS
    NS_FORWARD_NSIRILTELEPHONYCALLBACK(mTelephony->)

    RILTelephonyCallback(Telephony* aTelephony)
    : mTelephony(aTelephony)
    {
      NS_ASSERTION(mTelephony, "Null pointer!");
    }
  };
};

END_TELEPHONY_NAMESPACE

#endif // mozilla_dom_telephony_telephony_h__
