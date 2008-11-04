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
 * The Original Code is worker threads.
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

#include "nsDOMWorkerXHR.h"

// Interfaces
#include "nsIDocument.h"
#include "nsIDOMEvent.h"
#include "nsIThread.h"
#include "nsIXPConnect.h"

// Other includes
#include "nsAutoLock.h"
#include "nsAXPCNativeCallContext.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsJSUtils.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

// DOMWorker includes
#include "nsDOMThreadService.h"
#include "nsDOMWorkerPool.h"
#include "nsDOMWorkerXHRProxy.h"

// The list of event types that we support. This list and the defines based on
// it determine the sizes of the listener arrays in nsDOMWorkerXHRProxy. Make
// sure that any event types shared by both the XHR and Upload objects are
// together at the beginning of the list. Any changes made to this list may
// affect sMaxUploadEventTypes, so make sure that it is adjusted accordingly or
// things will break!
const char* const nsDOMWorkerXHREventTarget::sListenerTypes[] = {
  // nsIXMLHttpRequestEventTarget listeners.
  "abort",                             /* LISTENER_TYPE_ABORT */
  "error",                             /* LISTENER_TYPE_ERROR */
  "load",                              /* LISTENER_TYPE_LOAD */
  "loadstart",                         /* LISTENER_TYPE_LOADSTART */
  "progress",                          /* LISTENER_TYPE_PROGRESS */

  // nsIXMLHttpRequest listeners.
  "readystatechange"                   /* LISTENER_TYPE_READYSTATECHANGE */
};

// This should always be set to the length of sListenerTypes.
const PRUint32 nsDOMWorkerXHREventTarget::sMaxXHREventTypes =
  NS_ARRAY_LENGTH(nsDOMWorkerXHREventTarget::sListenerTypes);

// This should be set to the index of the first event type that is *not*
// supported by the Upload object.
const PRUint32 nsDOMWorkerXHREventTarget::sMaxUploadEventTypes =
  LISTENER_TYPE_READYSTATECHANGE;

// Enforce the invariant that the upload object supports no more event types
// than the xhr object.
PR_STATIC_ASSERT(nsDOMWorkerXHREventTarget::sMaxXHREventTypes >=
                 nsDOMWorkerXHREventTarget::sMaxUploadEventTypes);

NS_IMPL_THREADSAFE_ISUPPORTS2(nsDOMWorkerXHREventTarget,
                              nsIDOMEventTarget,
                              nsIXMLHttpRequestEventTarget)

PRUint32
nsDOMWorkerXHREventTarget::GetListenerTypeFromString(const nsAString& aString)
{
  for (PRUint32 index = 0; index < sMaxXHREventTypes; index++) {
    if (aString.EqualsASCII(sListenerTypes[index])) {
      return index;
    }
  }
  return PR_UINT32_MAX;
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::GetOnabort(nsIDOMEventListener** aOnabort)
{
  NS_ENSURE_ARG_POINTER(aOnabort);

  nsCOMPtr<nsIDOMEventListener> listener = GetOnXListener(LISTENER_TYPE_ABORT);
  listener.forget(aOnabort);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::SetOnabort(nsIDOMEventListener* aOnabort)
{
  return SetEventListener(LISTENER_TYPE_ABORT, aOnabort, PR_TRUE);
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::GetOnerror(nsIDOMEventListener** aOnerror)
{
  NS_ENSURE_ARG_POINTER(aOnerror);

  nsCOMPtr<nsIDOMEventListener> listener = GetOnXListener(LISTENER_TYPE_ERROR);
  listener.forget(aOnerror);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::SetOnerror(nsIDOMEventListener* aOnerror)
{
  return SetEventListener(LISTENER_TYPE_ERROR, aOnerror, PR_TRUE);
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::GetOnload(nsIDOMEventListener** aOnload)
{
  NS_ENSURE_ARG_POINTER(aOnload);

  nsCOMPtr<nsIDOMEventListener> listener = GetOnXListener(LISTENER_TYPE_LOAD);
  listener.forget(aOnload);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::SetOnload(nsIDOMEventListener* aOnload)
{
  return SetEventListener(LISTENER_TYPE_LOAD, aOnload, PR_TRUE);
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::GetOnloadstart(nsIDOMEventListener** aOnloadstart)
{
  NS_ENSURE_ARG_POINTER(aOnloadstart);

  nsCOMPtr<nsIDOMEventListener> listener =
    GetOnXListener(LISTENER_TYPE_LOADSTART);
  listener.forget(aOnloadstart);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::SetOnloadstart(nsIDOMEventListener* aOnloadstart)
{
  return SetEventListener(LISTENER_TYPE_LOADSTART, aOnloadstart, PR_TRUE);
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::GetOnprogress(nsIDOMEventListener** aOnprogress)
{
  NS_ENSURE_ARG_POINTER(aOnprogress);

  nsCOMPtr<nsIDOMEventListener> listener =
    GetOnXListener(LISTENER_TYPE_PROGRESS);
  listener.forget(aOnprogress);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::SetOnprogress(nsIDOMEventListener* aOnprogress)
{
  return SetEventListener(LISTENER_TYPE_PROGRESS, aOnprogress, PR_TRUE);
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::AddEventListener(const nsAString& aType,
                                            nsIDOMEventListener* aListener,
                                            PRBool aUseCapture)
{
  NS_ENSURE_ARG_POINTER(aListener);

  PRUint32 type = GetListenerTypeFromString(aType);
  if (type > sMaxXHREventTypes) {
    // Silently ignore junk events.
    return NS_OK;
  }

  return SetEventListener(type, aListener, PR_FALSE);
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::RemoveEventListener(const nsAString& aType,
                                               nsIDOMEventListener* aListener,
                                               PRBool aUseCapture)
{
  NS_ENSURE_ARG_POINTER(aListener);

  PRUint32 type = GetListenerTypeFromString(aType);
  if (type > sMaxXHREventTypes) {
    // Silently ignore junk events.
    return NS_OK;
  }

  return UnsetEventListener(type, aListener);
}

/* ec702b78-c30f-439f-9a9b-a5dae17ee0fc */
#define NS_IPRIVATEWORKERXHREVENT_IID                      \
{                                                          \
  0xec702b78,                                              \
  0xc30f,                                                  \
  0x439f,                                                  \
  { 0x9a, 0x9b, 0xa5, 0xda, 0xe1, 0x7e, 0xe0, 0xfc }       \
}

class nsIPrivateWorkerXHREvent : public nsIDOMEvent
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPRIVATEWORKERXHREVENT_IID)
  virtual PRBool PreventDefaultCalled() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIPrivateWorkerXHREvent,
                              NS_IPRIVATEWORKERXHREVENT_IID)

#define NS_FORWARD_NSIDOMEVENT_SPECIAL \
  NS_IMETHOD GetType(nsAString& aType) \
    { return mEvent->GetType(aType); } \
  NS_IMETHOD GetTarget(nsIDOMEventTarget** aTarget) \
    { return mEvent->GetTarget(aTarget); } \
  NS_IMETHOD GetCurrentTarget(nsIDOMEventTarget** aCurrentTarget) \
    { return mEvent->GetCurrentTarget(aCurrentTarget); } \
  NS_IMETHOD GetEventPhase(PRUint16* aEventPhase) \
    { return mEvent->GetEventPhase(aEventPhase); } \
  NS_IMETHOD GetBubbles(PRBool* aBubbles) \
    { return mEvent->GetBubbles(aBubbles); } \
  NS_IMETHOD GetCancelable(PRBool* aCancelable) \
    { return mEvent->GetCancelable(aCancelable); } \
  NS_IMETHOD GetTimeStamp(DOMTimeStamp* aTimeStamp) \
    { return mEvent->GetTimeStamp(aTimeStamp); } \
  NS_IMETHOD StopPropagation() \
    { return mEvent->StopPropagation(); }

class nsDOMWorkerXHREventWrapper : public nsIPrivateWorkerXHREvent
{
public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_NSIDOMEVENT_SPECIAL

  nsDOMWorkerXHREventWrapper(nsIDOMEvent* aEvent)
  : mEvent(aEvent), mPreventDefaultCalled(PR_FALSE) {
    NS_ASSERTION(aEvent, "Null pointer!");
  }

  NS_IMETHOD PreventDefault() {
    mPreventDefaultCalled = PR_TRUE;
    return mEvent->PreventDefault();
  }

  NS_IMETHOD InitEvent(const nsAString& aEventType, PRBool aCanBubble,
                       PRBool aCancelable) {
    mPreventDefaultCalled = PR_FALSE;
    return mEvent->InitEvent(aEventType, aCanBubble, aCancelable);
  } 

  // nsIPrivateWorkerXHREvent
  virtual PRBool PreventDefaultCalled() {
    return mPreventDefaultCalled;
  }

private:
  nsCOMPtr<nsIDOMEvent> mEvent;
  PRBool mPreventDefaultCalled;
};

NS_IMPL_THREADSAFE_ISUPPORTS2(nsDOMWorkerXHREventWrapper,
                              nsIDOMEvent,
                              nsIPrivateWorkerXHREvent)

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::DispatchEvent(nsIDOMEvent* aEvent,
                                         PRBool* _retval)
{
  NS_ENSURE_ARG_POINTER(aEvent);
  NS_ENSURE_ARG_POINTER(_retval);

  nsCOMPtr<nsIPrivateWorkerXHREvent> wrapper(do_QueryInterface(aEvent));
  if (!wrapper) {
    wrapper = new nsDOMWorkerXHREventWrapper(aEvent);
    NS_ENSURE_TRUE(wrapper, NS_ERROR_OUT_OF_MEMORY);
  }

  nsresult rv = HandleWorkerEvent(wrapper);
  NS_ENSURE_SUCCESS(rv, rv);

  *_retval = wrapper->PreventDefaultCalled();
  return NS_OK;
}

nsDOMWorkerXHRUpload::nsDOMWorkerXHRUpload(nsDOMWorkerXHR* aWorkerXHR)
: mWorkerXHR(aWorkerXHR)
{
  NS_ASSERTION(aWorkerXHR, "Must have a worker XHR!");
}

NS_IMPL_ISUPPORTS_INHERITED2(nsDOMWorkerXHRUpload, nsDOMWorkerXHREventTarget,
                                                   nsIXMLHttpRequestUpload,
                                                   nsIClassInfo)

NS_IMPL_CI_INTERFACE_GETTER3(nsDOMWorkerXHRUpload, nsIDOMEventTarget,
                                                   nsIXMLHttpRequestEventTarget,
                                                   nsIXMLHttpRequestUpload)

NS_IMPL_THREADSAFE_CI(nsDOMWorkerXHRUpload)

nsresult
nsDOMWorkerXHRUpload::SetEventListener(PRUint32 aType,
                                       nsIDOMEventListener* aListener,
                                       PRBool aOnXListener)
{
  if (mWorkerXHR->mCanceled) {
    return NS_ERROR_ABORT;
  }

  return mWorkerXHR->mXHRProxy->AddEventListener(aType, aListener, aOnXListener,
                                                 PR_TRUE);
}

nsresult
nsDOMWorkerXHRUpload::UnsetEventListener(PRUint32 aType,
                                         nsIDOMEventListener* aListener)
{
  if (mWorkerXHR->mCanceled) {
    return NS_ERROR_ABORT;
  }

  return mWorkerXHR->mXHRProxy->RemoveEventListener(aType, aListener, PR_TRUE);
}

nsresult
nsDOMWorkerXHRUpload::HandleWorkerEvent(nsIDOMEvent* aEvent)
{
  if (mWorkerXHR->mCanceled) {
    return NS_ERROR_ABORT;
  }

  return mWorkerXHR->mXHRProxy->HandleWorkerEvent(aEvent, PR_TRUE);
}

already_AddRefed<nsIDOMEventListener>
nsDOMWorkerXHRUpload::GetOnXListener(PRUint32 aType)
{
  if (mWorkerXHR->mCanceled) {
    return nsnull;
  }

  return mWorkerXHR->mXHRProxy->GetOnXListener(aType, PR_TRUE);
}

nsDOMWorkerXHR::nsDOMWorkerXHR(nsDOMWorkerThread* aWorker)
: mWorker(aWorker),
  mCanceled(PR_TRUE)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aWorker, "Must have a worker!");
}

nsDOMWorkerXHR::~nsDOMWorkerXHR()
{
  if (!mCanceled) {
    mWorker->RemoveXHR(this);
  }
}

NS_IMPL_ISUPPORTS_INHERITED2(nsDOMWorkerXHR, nsDOMWorkerXHREventTarget,
                                             nsIXMLHttpRequest,
                                             nsIClassInfo)

NS_IMPL_CI_INTERFACE_GETTER3(nsDOMWorkerXHR, nsIDOMEventTarget,
                                             nsIXMLHttpRequestEventTarget,
                                             nsIXMLHttpRequest)

NS_IMPL_THREADSAFE_CI(nsDOMWorkerXHR)

nsresult
nsDOMWorkerXHR::Init()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (!mWorker->AddXHR(this)) {
    // Must have been canceled.
    return NS_ERROR_ABORT;
  }
  mCanceled = PR_FALSE;

  nsRefPtr<nsDOMWorkerXHRProxy> proxy = new nsDOMWorkerXHRProxy(this);
  NS_ENSURE_TRUE(proxy, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = proxy->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  proxy.swap(mXHRProxy);
  return NS_OK;
}

void
nsDOMWorkerXHR::Cancel()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Just in case mUpload holds the only ref to this object we make sure to stay
  // alive through this call.
  nsRefPtr<nsDOMWorkerXHR> kungFuDeathGrip(this);

  {
    // This lock is here to prevent a race between Cancel and GetUpload, not to
    // protect mCanceled.
    nsAutoLock lock(mWorker->Lock());

    mCanceled = PR_TRUE;
    mUpload = nsnull;
  }

  if (mXHRProxy) {
    mXHRProxy->Destroy();
  }

  mWorker->RemoveXHR(this);
  mWorker = nsnull;
}

nsresult
nsDOMWorkerXHR::SetEventListener(PRUint32 aType,
                                 nsIDOMEventListener* aListener,
                                 PRBool aOnXListener)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  return mXHRProxy->AddEventListener(aType, aListener, aOnXListener, PR_FALSE);
}

nsresult
nsDOMWorkerXHR::UnsetEventListener(PRUint32 aType,
                                   nsIDOMEventListener* aListener)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  return mXHRProxy->RemoveEventListener(aType, aListener, PR_FALSE);
}

nsresult
nsDOMWorkerXHR::HandleWorkerEvent(nsIDOMEvent* aEvent)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  return mXHRProxy->HandleWorkerEvent(aEvent, PR_FALSE);
}

already_AddRefed<nsIDOMEventListener>
nsDOMWorkerXHR::GetOnXListener(PRUint32 aType)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return nsnull;
  }

  return mXHRProxy->GetOnXListener(aType, PR_FALSE);
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetChannel(nsIChannel** aChannel)
{
  NS_ENSURE_ARG_POINTER(aChannel);
  *aChannel = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetResponseXML(nsIDOMDocument** aResponseXML)
{
  NS_ENSURE_ARG_POINTER(aResponseXML);
  *aResponseXML = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetResponseText(nsAString& aResponseText)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = mXHRProxy->GetResponseText(aResponseText);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetStatus(PRUint32* aStatus)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_ARG_POINTER(aStatus);

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = mXHRProxy->GetStatus(aStatus);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetStatusText(nsACString& aStatusText)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = mXHRProxy->GetStatusText(aStatusText);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::Abort()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = mXHRProxy->Abort();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetAllResponseHeaders(char** _retval)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_ARG_POINTER(_retval);

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = mXHRProxy->GetAllResponseHeaders(_retval);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetResponseHeader(const nsACString& aHeader,
                                  nsACString& _retval)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = mXHRProxy->GetResponseHeader(aHeader, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::OpenRequest(const nsACString& aMethod,
                            const nsACString& aUrl,
                            PRBool aAsync,
                            const nsAString& aUser,
                            const nsAString& aPassword)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = mXHRProxy->OpenRequest(aMethod, aUrl, aAsync, aUser, aPassword);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::Open(const nsACString& aMethod,
                     const nsACString& aUrl)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  PRBool async = PR_TRUE;
  nsAutoString user, password;

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  NS_ENSURE_TRUE(xpc, NS_ERROR_UNEXPECTED);

  nsAXPCNativeCallContext* cc;
  nsresult rv = xpc->GetCurrentNativeCallContext(&cc);

  do {
    if (NS_FAILED(rv) || !cc) {
      break;
    }

    PRUint32 argc;
    rv = cc->GetArgc(&argc);
    NS_ENSURE_SUCCESS(rv, rv);

    if (argc < 3) {
      break;
    }

    jsval* argv;
    rv = cc->GetArgvPtr(&argv);
    NS_ENSURE_SUCCESS(rv, rv);

    JSContext* cx;
    rv = cc->GetJSContext(&cx);
    NS_ENSURE_SUCCESS(rv, rv);

    JSAutoRequest ar(cx);

    JSBool asyncBool;
    JS_ValueToBoolean(cx, argv[2], &asyncBool);
    async = (PRBool)asyncBool;

    if (argc < 4) {
      break;
    }

    JSString* argStr;
    if (!JSVAL_IS_NULL(argv[3]) && !JSVAL_IS_VOID(argv[3])) {
      argStr = JS_ValueToString(cx, argv[3]);
      if (argStr) {
        user.Assign(nsDependentJSString(argStr));
      }
    }

    if (argc < 5) {
      break;
    }

    if (!JSVAL_IS_NULL(argv[4]) && !JSVAL_IS_VOID(argv[4])) {
      argStr = JS_ValueToString(cx, argv[4]);
      if (argStr) {
        password.Assign(nsDependentJSString(argStr));
      }
    }
  } while (PR_FALSE);

  return OpenRequest(aMethod, aUrl, async, user, password);
}

NS_IMETHODIMP
nsDOMWorkerXHR::Send(nsIVariant* aBody)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = mXHRProxy->Send(aBody);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::SendAsBinary(const nsAString& aBody)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = mXHRProxy->SendAsBinary(aBody);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::SetRequestHeader(const nsACString& aHeader,
                                 const nsACString& aValue)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = mXHRProxy->SetRequestHeader(aHeader, aValue);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetReadyState(PRInt32* aReadyState)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  NS_ENSURE_ARG_POINTER(aReadyState);

  nsresult rv = mXHRProxy->GetReadyState(aReadyState);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::OverrideMimeType(const nsACString& aMimetype)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = mXHRProxy->OverrideMimeType(aMimetype);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetMultipart(PRBool* aMultipart)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  NS_ENSURE_ARG_POINTER(aMultipart);

  nsresult rv = mXHRProxy->GetMultipart(aMultipart);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::SetMultipart(PRBool aMultipart)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = mXHRProxy->SetMultipart(aMultipart);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetMozBackgroundRequest(PRBool* aMozBackgroundRequest)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  NS_ENSURE_ARG_POINTER(aMozBackgroundRequest);

  *aMozBackgroundRequest = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::SetMozBackgroundRequest(PRBool aMozBackgroundRequest)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (aMozBackgroundRequest) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::Init(nsIPrincipal* aPrincipal,
                     nsIScriptContext* aScriptContext,
                     nsPIDOMWindow* aOwnerWindow)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_NOTREACHED("No one should be calling this!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetUpload(nsIXMLHttpRequestUpload** aUpload)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<nsDOMWorkerThread> worker = mWorker;
  if (!worker) {
    return NS_ERROR_ABORT;
  }

  nsAutoLock lock(worker->Lock());

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  NS_ENSURE_ARG_POINTER(aUpload);

  if (!mUpload) {
    mUpload = new nsDOMWorkerXHRUpload(this);
    NS_ENSURE_TRUE(mUpload, NS_ERROR_OUT_OF_MEMORY);
  }

  NS_ADDREF(*aUpload = mUpload);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetOnreadystatechange(nsIDOMEventListener** aOnreadystatechange)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  NS_ENSURE_ARG_POINTER(aOnreadystatechange);

  nsCOMPtr<nsIDOMEventListener> listener =
    mXHRProxy->GetOnXListener(LISTENER_TYPE_READYSTATECHANGE, PR_FALSE);

  listener.forget(aOnreadystatechange);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::SetOnreadystatechange(nsIDOMEventListener* aOnreadystatechange)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  return mXHRProxy->AddEventListener(LISTENER_TYPE_READYSTATECHANGE,
                                    aOnreadystatechange, PR_TRUE, PR_FALSE);
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetWithCredentials(PRBool* aWithCredentials)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  NS_ENSURE_ARG_POINTER(aWithCredentials);

  nsresult rv = mXHRProxy->GetWithCredentials(aWithCredentials);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::SetWithCredentials(PRBool aWithCredentials)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = mXHRProxy->SetWithCredentials(aWithCredentials);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}
