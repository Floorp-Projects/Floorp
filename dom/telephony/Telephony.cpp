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

#include "Telephony.h"

#include "nsIDocument.h"
#include "nsIURI.h"
#include "nsPIDOMWindow.h"

#include "jsapi.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsServiceManagerUtils.h"
#include "SystemWorkerManager.h"

#include "CallEvent.h"
#include "TelephonyCall.h"

USING_TELEPHONY_NAMESPACE
using mozilla::Preferences;

#define DOM_TELEPHONY_APP_PHONE_URL_PREF "dom.telephony.app.phone.url"

namespace {

template <class T>
inline nsresult
nsTArrayToJSArray(JSContext* aCx, JSObject* aGlobal,
                  const nsTArray<nsRefPtr<T> >& aSourceArray,
                  JSObject** aResultArray)
{
  NS_ASSERTION(aCx, "Null context!");
  NS_ASSERTION(aGlobal, "Null global!");

  JSAutoRequest ar(aCx);
  JSAutoEnterCompartment ac;
  if (!ac.enter(aCx, aGlobal)) {
    NS_WARNING("Failed to enter compartment!");
    return NS_ERROR_FAILURE;
  }

  JSObject* arrayObj;

  if (aSourceArray.IsEmpty()) {
    arrayObj = JS_NewArrayObject(aCx, 0, nsnull);
  } else {
    nsTArray<jsval> valArray;
    valArray.SetLength(aSourceArray.Length());

    for (PRUint32 index = 0; index < valArray.Length(); index++) {
      nsISupports* obj = aSourceArray[index]->ToISupports();
      nsresult rv =
        nsContentUtils::WrapNative(aCx, aGlobal, obj, &valArray[index]);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    arrayObj = JS_NewArrayObject(aCx, valArray.Length(), valArray.Elements());
  }

  if (!arrayObj) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // XXX This is not what Jonas wants. He wants it to be live.
  if (!JS_FreezeObject(aCx, arrayObj)) {
    return NS_ERROR_FAILURE;
  }

  *aResultArray = arrayObj;
  return NS_OK;
}

} // anonymous namespace

Telephony::~Telephony()
{
  if (mTelephone && mTelephoneCallback) {
    mTelephone->UnregisterCallback(mTelephoneCallback);
  }

  if (mRooted) {
    NS_DROP_JS_OBJECTS(this, Telephony);
  }
}

// static
already_AddRefed<Telephony>
Telephony::Create(nsPIDOMWindow* aOwner, nsITelephone* aTelephone)
{
  NS_ASSERTION(aOwner, "Null owner!");
  NS_ASSERTION(aTelephone, "Null telephone!");

  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aOwner);
  NS_ENSURE_TRUE(sgo, nsnull);

  nsCOMPtr<nsIScriptContext> scriptContext = sgo->GetContext();
  NS_ENSURE_TRUE(scriptContext, nsnull);

  nsRefPtr<Telephony> telephony = new Telephony();

  telephony->mOwner = aOwner;
  telephony->mScriptContext.swap(scriptContext);
  telephony->mTelephone = aTelephone;
  telephony->mTelephoneCallback = new TelephoneCallback(telephony);

  nsresult rv = aTelephone->EnumerateCalls(telephony->mTelephoneCallback);
  NS_ENSURE_SUCCESS(rv, nsnull);

  rv = aTelephone->RegisterCallback(telephony->mTelephoneCallback);
  NS_ENSURE_SUCCESS(rv, nsnull);

  return telephony.forget();
}

void
Telephony::SwitchActiveCall(TelephonyCall* aCall)
{
  if (mActiveCall) {
    // Put the call on hold?
    NS_NOTYETIMPLEMENTED("Implement me!");
  }
  mActiveCall = aCall;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Telephony)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Telephony,
                                                  nsDOMEventTargetWrapperCache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(incoming)
  for (PRUint32 index = 0; index < tmp->mCalls.Length(); index++) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mCalls[i]");
    cb.NoteXPCOMChild(tmp->mCalls[index]->ToISupports());
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(Telephony,
                                               nsDOMEventTargetWrapperCache)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(tmp->mCallsArray, "mCallsArray")
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Telephony,
                                                nsDOMEventTargetWrapperCache)
  NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(incoming)
  tmp->mCalls.Clear();
  tmp->mActiveCall = nsnull;
  tmp->mCallsArray = nsnull;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(Telephony)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTelephony)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Telephony)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetWrapperCache)

NS_IMPL_ADDREF_INHERITED(Telephony, nsDOMEventTargetWrapperCache)
NS_IMPL_RELEASE_INHERITED(Telephony, nsDOMEventTargetWrapperCache)

DOMCI_DATA(Telephony, Telephony)

NS_IMPL_ISUPPORTS1(Telephony::TelephoneCallback, nsITelephoneCallback)

NS_IMETHODIMP
Telephony::Dial(const nsAString& aNumber, nsIDOMTelephonyCall** aResult)
{
  NS_ENSURE_ARG(!aNumber.IsEmpty());

  for (PRUint32 index = 0; index < mCalls.Length(); index++) {
    const nsRefPtr<TelephonyCall>& tempCall = mCalls[index];
    if (tempCall->IsOutgoing() &&
        tempCall->CallState() < nsITelephone::CALL_STATE_CONNECTED) {
      // One call has been dialed already and we only support one outgoing call
      // at a time.
      NS_WARNING("Only permitted to dial one call at a time!");
      return NS_ERROR_NOT_AVAILABLE;
    }
  }

  nsresult rv = mTelephone->Dial(aNumber);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<TelephonyCall> call =
    TelephonyCall::Create(this, aNumber, nsITelephone::CALL_STATE_DIALING);
  NS_ASSERTION(call, "This should never fail!");

  NS_ASSERTION(mCalls.Contains(call), "Should have auto-added new call!");

  call.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
Telephony::GetMuted(bool* aMuted)
{
  nsresult rv = mTelephone->GetMicrophoneMuted(aMuted);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
Telephony::SetMuted(bool aMuted)
{
  nsresult rv = mTelephone->SetMicrophoneMuted(aMuted);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
Telephony::GetSpeakerEnabled(bool* aSpeakerEnabled)
{
  nsresult rv = mTelephone->GetSpeakerEnabled(aSpeakerEnabled);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
Telephony::SetSpeakerEnabled(bool aSpeakerEnabled)
{
  nsresult rv = mTelephone->SetSpeakerEnabled(aSpeakerEnabled);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
Telephony::GetActive(jsval* aActive)
{
  if (!mActiveCall) {
    aActive->setNull();
    return NS_OK;
  }

  nsresult rv =
    nsContentUtils::WrapNative(mScriptContext->GetNativeContext(),
                               mScriptContext->GetNativeGlobal(),
                               mActiveCall->ToISupports(), aActive);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
Telephony::SetActive(const jsval& aActive)
{
  if (aActive.isObject()) {
    nsIXPConnect* xpc = nsContentUtils::XPConnect();
    NS_ASSERTION(xpc, "This should never be null!");

    nsISupports* native =
      xpc->GetNativeOfWrapper(mScriptContext->GetNativeContext(),
                              &aActive.toObject());

    nsCOMPtr<nsIDOMTelephonyCall> call = do_QueryInterface(native);
    if (call) {
      // See if this call has the same telephony object. Otherwise we can't use
      // it.
      TelephonyCall* concreteCall = static_cast<TelephonyCall*>(call.get());
      if (this == concreteCall->mTelephony) {
        SwitchActiveCall(concreteCall);
        return NS_OK;
      }
    }
  }

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
Telephony::GetCalls(jsval* aCalls)
{
  JSObject* calls = mCallsArray;
  if (!calls) {
    nsresult rv =
      nsTArrayToJSArray(mScriptContext->GetNativeContext(),
                        mScriptContext->GetNativeGlobal(), mCalls, &calls);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!mRooted) {
      NS_HOLD_JS_OBJECTS(this, Telephony);
      mRooted = true;
    }

    mCallsArray = calls;
  }

  aCalls->setObject(*calls);
  return NS_OK;
}

NS_IMETHODIMP
Telephony::StartTone(const nsAString& aDTMFChar)
{
  if (aDTMFChar.IsEmpty()) {
    NS_WARNING("Empty tone string will be ignored");
    return NS_OK;
  }

  if (aDTMFChar.Length() > 1) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv = mTelephone->StartTone(aDTMFChar);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
Telephony::StopTone()
{
  nsresult rv = mTelephone->StopTone();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
Telephony::SendTones(const nsAString& aTones, PRUint32 aToneDuration,
                     PRUint32 aIntervalDuration)
{
  NS_NOTYETIMPLEMENTED("Implement me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMPL_EVENT_HANDLER(Telephony, incoming)

NS_IMETHODIMP
Telephony::CallStateChanged(PRUint32 aCallIndex, PRUint16 aCallState,
                            const nsAString& aNumber)
{
  NS_ASSERTION(aCallIndex != kOutgoingPlaceholderCallIndex,
               "This should never happen!");

  nsRefPtr<TelephonyCall> modifiedCall;
  nsRefPtr<TelephonyCall> outgoingCall;

  for (PRUint32 index = 0; index < mCalls.Length(); index++) {
    nsRefPtr<TelephonyCall>& tempCall = mCalls[index];
    if (tempCall->CallIndex() == kOutgoingPlaceholderCallIndex) {
      NS_ASSERTION(!outgoingCall, "More than one outgoing call not supported!");
      NS_ASSERTION(tempCall->CallState() == nsITelephone::CALL_STATE_DIALING,
                   "Something really wrong here!");
      // Stash this for later, we may need it if aCallIndex doesn't match one of
      // our other calls.
      outgoingCall = tempCall;
    } else if (tempCall->CallIndex() == aCallIndex) {
      // We already know about this call so just update its state.
      modifiedCall = tempCall;
      outgoingCall = nsnull;
      break;
    }
  }

  // If nothing matched above and the call state isn't incoming but we do have
  // an outgoing call then we must be seeing a status update for our outgoing
  // call.
  if (!modifiedCall &&
      aCallState != nsITelephone::CALL_STATE_INCOMING &&
      outgoingCall) {
    outgoingCall->UpdateCallIndex(aCallIndex);
    modifiedCall.swap(outgoingCall);
  }

  if (modifiedCall) {
    // Change state.
    modifiedCall->ChangeState(aCallState);

    // See if this should replace our current active call.
    if (aCallState == nsITelephone::CALL_STATE_CONNECTED) {
      SwitchActiveCall(modifiedCall);
    }

    return NS_OK;
  }

  // Didn't know anything about this call before now, must be incoming.
  NS_ASSERTION(aCallState == nsITelephone::CALL_STATE_INCOMING,
               "Serious logic problem here!");

  nsRefPtr<TelephonyCall> call =
    TelephonyCall::Create(this, aNumber, aCallState, aCallIndex);
  NS_ASSERTION(call, "This should never fail!");

  NS_ASSERTION(mCalls.Contains(call), "Should have auto-added new call!");

  // Dispatch incoming event.
  nsRefPtr<CallEvent> event = CallEvent::Create(call);
  NS_ASSERTION(event, "This should never fail!");

  nsresult rv =
    event->Dispatch(ToIDOMEventTarget(), NS_LITERAL_STRING("incoming"));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
Telephony::EnumerateCallState(PRUint32 aCallIndex, PRUint16 aCallState,
                              const nsAString& aNumber, bool aIsActive,
                              bool* aContinue)
{
#ifdef DEBUG
  // Make sure we don't somehow add duplicates.
  for (PRUint32 index = 0; index < mCalls.Length(); index++) {
    NS_ASSERTION(mCalls[index]->CallIndex() != aCallIndex,
                 "Something is really wrong here!");
  }
#endif
  nsRefPtr<TelephonyCall> call =
    TelephonyCall::Create(this, aNumber, aCallState, aCallIndex);
  NS_ASSERTION(call, "This should never fail!");

  NS_ASSERTION(mCalls.Contains(call), "Should have auto-added new call!");

  if (aIsActive) {
    NS_ASSERTION(!mActiveCall, "Already have an active call!");
    mActiveCall = call;
  }

  *aContinue = true;
  return NS_OK;
}

nsresult
NS_NewTelephony(nsPIDOMWindow* aWindow, nsIDOMTelephony** aTelephony)
{
  NS_ASSERTION(aWindow, "Null pointer!");

  // Make sure we're dealing with an inner window.
  nsPIDOMWindow* innerWindow = aWindow->IsInnerWindow() ?
                               aWindow :
                               aWindow->GetCurrentInnerWindow();
  NS_ENSURE_TRUE(innerWindow, NS_ERROR_FAILURE);

  // Make sure we're being called from a window that we have permission to
  // access.
  if (!nsContentUtils::CanCallerAccess(innerWindow)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // Need the document in order to make security decisions.
  nsCOMPtr<nsIDocument> document =
    do_QueryInterface(innerWindow->GetExtantDocument());
  NS_ENSURE_TRUE(document, NS_NOINTERFACE);

  // Do security checks. We assume that chrome is always allowed and we also
  // allow a single page specified by preferences.
  if (!nsContentUtils::IsSystemPrincipal(document->NodePrincipal())) {
    nsCOMPtr<nsIURI> documentURI;
    nsresult rv =
      document->NodePrincipal()->GetURI(getter_AddRefs(documentURI));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCString documentURL;
    rv = documentURI->GetSpec(documentURL);
    NS_ENSURE_SUCCESS(rv, rv);

    // The pref may not exist but in that case we deny access just as we do if
    // the url doesn't match.
    nsCString phoneAppURL;
    if (NS_FAILED(Preferences::GetCString(DOM_TELEPHONY_APP_PHONE_URL_PREF,
                                          &phoneAppURL)) ||
        !phoneAppURL.Equals(documentURL,
                            nsCaseInsensitiveCStringComparator())) {
      *aTelephony = nsnull;
      return NS_OK;
    }
  }

  // Security checks passed, make a telephony object.
  nsIInterfaceRequestor* ireq = SystemWorkerManager::GetInterfaceRequestor();
  NS_ENSURE_TRUE(ireq, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsITelephone> telephone = do_GetInterface(ireq);
  NS_ENSURE_TRUE(telephone, NS_ERROR_UNEXPECTED);

  nsRefPtr<Telephony> telephony = Telephony::Create(innerWindow, telephone);
  NS_ENSURE_TRUE(telephony, NS_ERROR_UNEXPECTED);

  telephony.forget(aTelephony);
  return NS_OK;
}
