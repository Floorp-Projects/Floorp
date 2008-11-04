/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 * The Original Code is Web Workers.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
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

#include "nsDOMWorkerEvents.h"

#include "nsIXMLHttpRequest.h"

#include "nsThreadUtils.h"

#include "nsDOMWorkerMessageHandler.h"
#include "nsDOMWorkerXHR.h"
#include "nsDOMWorkerXHRProxy.h"

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDOMWorkerPrivateEvent,
                              NS_IDOMWORKERPRIVATEEVENT_IID)

nsDOMWorkerPrivateEvent::nsDOMWorkerPrivateEvent(nsIDOMEvent* aEvent)
: mEvent(aEvent),
  mPreventDefaultCalled(PR_FALSE)
{
  NS_ASSERTION(aEvent, "Null pointer!");
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsDOMWorkerPrivateEvent, nsIDOMEvent,
                                                       nsIDOMWorkerPrivateEvent,
                                                       nsIClassInfo)

NS_IMPL_CI_INTERFACE_GETTER2(nsDOMWorkerPrivateEvent, nsIDOMEvent,
                                                      nsIDOMWorkerPrivateEvent)

NS_IMPL_THREADSAFE_DOM_CI(nsDOMWorkerPrivateEvent)

NS_IMETHODIMP
nsDOMWorkerPrivateEvent::PreventDefault()
{
  mPreventDefaultCalled = PR_TRUE;
  return mEvent->PreventDefault();
}

NS_IMETHODIMP
nsDOMWorkerPrivateEvent::InitEvent(const nsAString& aEventType,
                                   PRBool aCanBubble,
                                   PRBool aCancelable)
{
  mPreventDefaultCalled = PR_FALSE;
  return mEvent->InitEvent(aEventType, aCanBubble, aCancelable);
}

PRBool
nsDOMWorkerPrivateEvent::PreventDefaultCalled()
{
  return mPreventDefaultCalled;
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsDOMWorkerEvent, nsIDOMEvent,
                                                nsIClassInfo)

NS_IMPL_CI_INTERFACE_GETTER1(nsDOMWorkerEvent, nsIDOMEvent)

NS_IMPL_THREADSAFE_DOM_CI(nsDOMWorkerEvent)

NS_IMETHODIMP
nsDOMWorkerEvent::GetType(nsAString& aType)
{
  aType.Assign(mType);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerEvent::GetTarget(nsIDOMEventTarget** aTarget)
{
  NS_ENSURE_ARG_POINTER(aTarget);
  NS_IF_ADDREF(*aTarget = mTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerEvent::GetCurrentTarget(nsIDOMEventTarget** aCurrentTarget)
{
  NS_ENSURE_ARG_POINTER(aCurrentTarget);
  NS_IF_ADDREF(*aCurrentTarget = mTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerEvent::GetEventPhase(PRUint16* aEventPhase)
{
  NS_ENSURE_ARG_POINTER(aEventPhase);
  *aEventPhase = mEventPhase;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerEvent::GetBubbles(PRBool* aBubbles)
{
  NS_ENSURE_ARG_POINTER(aBubbles);
  *aBubbles = mBubbles;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerEvent::GetCancelable(PRBool* aCancelable)
{
  NS_ENSURE_ARG_POINTER(aCancelable);
  *aCancelable = mCancelable;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerEvent::GetTimeStamp(DOMTimeStamp* aTimeStamp)
{
  NS_ENSURE_ARG_POINTER(aTimeStamp);
  *aTimeStamp = mTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerEvent::StopPropagation()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMWorkerEvent::PreventDefault()
{
  mPreventDefaultCalled = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerEvent::InitEvent(const nsAString& aEventTypeArg,
                            PRBool aCanBubbleArg,
                            PRBool aCancelableArg)
{
  NS_ENSURE_FALSE(aEventTypeArg.IsEmpty(), NS_ERROR_INVALID_ARG);

  mType.Assign(aEventTypeArg);
  mBubbles = aCanBubbleArg;
  mCancelable = aCancelableArg;
  mPreventDefaultCalled = PR_FALSE;
  mTimeStamp = PR_Now();
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED1(nsDOMWorkerMessageEvent, nsDOMWorkerEvent,
                                                      nsIWorkerMessageEvent)

NS_IMPL_CI_INTERFACE_GETTER2(nsDOMWorkerMessageEvent, nsIDOMEvent,
                                                      nsIWorkerMessageEvent)

NS_IMPL_THREADSAFE_DOM_CI_GETINTERFACES(nsDOMWorkerMessageEvent)

NS_IMETHODIMP
nsDOMWorkerMessageEvent::GetData(nsAString& aData)
{
  aData.Assign(mData);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerMessageEvent::GetOrigin(nsAString& aOrigin)
{
  aOrigin.Assign(mOrigin);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerMessageEvent::GetSource(nsISupports** aSource)
{
  NS_ENSURE_ARG_POINTER(aSource);
  NS_IF_ADDREF(*aSource = mSource);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerMessageEvent::InitMessageEvent(const nsAString& aTypeArg,
                                          PRBool aCanBubbleArg,
                                          PRBool aCancelableArg,
                                          const nsAString& aDataArg,
                                          const nsAString& aOriginArg,
                                          nsISupports* aSourceArg)
{
  mData.Assign(aDataArg);
  mOrigin.Assign(aOriginArg);
  mSource = aSourceArg;
  return nsDOMWorkerEvent::InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
}

nsDOMWorkerXHREvent::nsDOMWorkerXHREvent(nsDOMWorkerXHRProxy* aXHRProxy)
: mXHRProxy(aXHRProxy),
  mXHREventType(PR_UINT32_MAX),
  mStatus(NS_OK),
  mReadyState(0),
  mLoaded(0),
  mTotal(0),
  mChannelID(-1),
  mUploadEvent(PR_FALSE),
  mProgressEvent(PR_FALSE),
  mLengthComputable(PR_FALSE)
{
  NS_ASSERTION(aXHRProxy, "Can't be null!");
}

NS_IMPL_ADDREF_INHERITED(nsDOMWorkerXHREvent, nsDOMWorkerEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMWorkerXHREvent, nsDOMWorkerEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMWorkerXHREvent)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIDOMProgressEvent, mProgressEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMWorkerEvent)

NS_IMETHODIMP
nsDOMWorkerXHREvent::GetInterfaces(PRUint32* aCount,
                                   nsIID*** aArray)
{
  PRUint32 count = *aCount = mProgressEvent ? 2 : 1;

  *aArray = (nsIID**)nsMemory::Alloc(sizeof(nsIID*) * count);

  if (mProgressEvent) {
    (*aArray)[--count] =
      (nsIID*)nsMemory::Clone(&NS_GET_IID(nsIDOMProgressEvent), sizeof(nsIID));
  }

  (*aArray)[--count] =
    (nsIID *)nsMemory::Clone(&NS_GET_IID(nsIDOMEvent), sizeof(nsIID));

  NS_ASSERTION(!count, "Bad math!");
  return NS_OK;
}

nsresult
nsDOMWorkerXHREvent::Init(PRUint32 aXHREventType,
                          const nsAString& aType,
                          nsIDOMEvent* aEvent)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aEvent, "Don't pass null here!");

  mXHREventType = aXHREventType;
  mChannelID = mXHRProxy->ChannelID();

  mTarget = static_cast<nsDOMWorkerMessageHandler*>(mXHRProxy->mWorkerXHR);
  NS_ENSURE_TRUE(mTarget, NS_ERROR_UNEXPECTED);

  mWorkerWN = mXHRProxy->mWorkerXHR->mWorker->GetWrappedNative();
  NS_ENSURE_STATE(mWorkerWN);

  nsCOMPtr<nsIDOMEventTarget> mainThreadTarget;
  nsresult rv = aEvent->GetTarget(getter_AddRefs(mainThreadTarget));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_STATE(mainThreadTarget);

  nsCOMPtr<nsIXMLHttpRequestUpload> upload(do_QueryInterface(mainThreadTarget));
  if (upload) {
    mUploadEvent = PR_TRUE;
    mTarget =
      static_cast<nsDOMWorkerMessageHandler*>(mXHRProxy->mWorkerXHR->mUpload);
  }
  else {
    mUploadEvent = PR_FALSE;
    mTarget = static_cast<nsDOMWorkerMessageHandler*>(mXHRProxy->mWorkerXHR);
  }
  NS_ASSERTION(mTarget, "Null target!");

  PRBool bubbles;
  rv = aEvent->GetBubbles(&bubbles);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool cancelable;
  rv = aEvent->GetCancelable(&cancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aEvent->GetTimeStamp(&mTimeStamp);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aEvent->GetEventPhase(&mEventPhase);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(mEventPhase == nsIDOMEvent::AT_TARGET, "Unsupported phase!");

  nsCOMPtr<nsIDOMProgressEvent> progressEvent(do_QueryInterface(aEvent));
  if (progressEvent) {
    mProgressEvent = PR_TRUE;

    PRBool lengthComputable;
    rv = progressEvent->GetLengthComputable(&lengthComputable);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint64 loaded;
    rv = progressEvent->GetLoaded(&loaded);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint64 total;
    rv = progressEvent->GetTotal(&total);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = InitProgressEvent(aType, bubbles, cancelable, lengthComputable, loaded,
                           total);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    mProgressEvent = PR_FALSE;

    rv = nsDOMWorkerEvent::InitEvent(aType, bubbles, cancelable);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = SnapshotXHRState(mXHRProxy->mXHR);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsDOMWorkerXHREvent::SnapshotXHRState(nsIXMLHttpRequest* aXHR)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aXHR, "Don't pass null here!");

  nsresult rv = aXHR->GetResponseText(mResponseText);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aXHR->GetStatusText(mStatusText);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aXHR->GetStatus(&mStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aXHR->GetReadyState(&mReadyState);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
nsDOMWorkerXHREvent::EventHandled()
{
  // Prevent reference cycles by releasing these here.
  mXHRProxy = nsnull;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::Run()
{
  nsresult rv = mXHRProxy->HandleWorkerEvent(this, mUploadEvent);
  EventHandled();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::GetLengthComputable(PRBool* aLengthComputable)
{
  NS_ENSURE_ARG_POINTER(aLengthComputable);
  *aLengthComputable = mLengthComputable;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::GetLoaded(PRUint64* aLoaded)
{
  NS_ENSURE_ARG_POINTER(aLoaded);
  *aLoaded = mLoaded;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::GetTotal(PRUint64* aTotal)
{
  NS_ENSURE_ARG_POINTER(aTotal);
  *aTotal = mTotal;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREvent::InitProgressEvent(const nsAString_internal& aTypeArg,
                                       PRBool aCanBubbleArg,
                                       PRBool aCancelableArg,
                                       PRBool aLengthComputableArg,
                                       PRUint64 aLoadedArg,
                                       PRUint64 aTotalArg)
{
  mLengthComputable = aLengthComputableArg;
  mLoaded = aLoadedArg;
  mTotal = aTotalArg;
  return nsDOMWorkerEvent::InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
}
