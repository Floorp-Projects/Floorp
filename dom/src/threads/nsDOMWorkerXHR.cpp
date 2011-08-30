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
#include "nsAXPCNativeCallContext.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsJSUtils.h"
#include "nsThreadUtils.h"

// DOMWorker includes
#include "nsDOMThreadService.h"
#include "nsDOMWorkerEvents.h"
#include "nsDOMWorkerPool.h"
#include "nsDOMWorkerXHRProxy.h"

using namespace mozilla;

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
  "readystatechange",                   /* LISTENER_TYPE_READYSTATECHANGE */
  "loadend"
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

NS_IMPL_ISUPPORTS_INHERITED1(nsDOMWorkerXHREventTarget,
                             nsDOMWorkerMessageHandler,
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
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_ARG_POINTER(aOnabort);

  nsAutoString type;
  type.AssignASCII(sListenerTypes[LISTENER_TYPE_ABORT]);

  nsCOMPtr<nsIDOMEventListener> listener = GetOnXListener(type);
  listener.forget(aOnabort);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::SetOnabort(nsIDOMEventListener* aOnabort)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsAutoString type;
  type.AssignASCII(sListenerTypes[LISTENER_TYPE_ABORT]);

  return SetOnXListener(type, aOnabort);
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::GetOnerror(nsIDOMEventListener** aOnerror)
{
  NS_ENSURE_ARG_POINTER(aOnerror);
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsAutoString type;
  type.AssignASCII(sListenerTypes[LISTENER_TYPE_ERROR]);

  nsCOMPtr<nsIDOMEventListener> listener = GetOnXListener(type);
  listener.forget(aOnerror);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::SetOnerror(nsIDOMEventListener* aOnerror)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsAutoString type;
  type.AssignASCII(sListenerTypes[LISTENER_TYPE_ERROR]);

  return SetOnXListener(type, aOnerror);
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::GetOnload(nsIDOMEventListener** aOnload)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_ARG_POINTER(aOnload);

  nsAutoString type;
  type.AssignASCII(sListenerTypes[LISTENER_TYPE_LOAD]);

  nsCOMPtr<nsIDOMEventListener> listener = GetOnXListener(type);
  listener.forget(aOnload);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::SetOnload(nsIDOMEventListener* aOnload)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsAutoString type;
  type.AssignASCII(sListenerTypes[LISTENER_TYPE_LOAD]);

  return SetOnXListener(type, aOnload);
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::GetOnloadstart(nsIDOMEventListener** aOnloadstart)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_ARG_POINTER(aOnloadstart);

  nsAutoString type;
  type.AssignASCII(sListenerTypes[LISTENER_TYPE_LOADSTART]);

  nsCOMPtr<nsIDOMEventListener> listener = GetOnXListener(type);
  listener.forget(aOnloadstart);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::SetOnloadstart(nsIDOMEventListener* aOnloadstart)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsAutoString type;
  type.AssignASCII(sListenerTypes[LISTENER_TYPE_LOADSTART]);

  return SetOnXListener(type, aOnloadstart);
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::GetOnprogress(nsIDOMEventListener** aOnprogress)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_ARG_POINTER(aOnprogress);

  nsAutoString type;
  type.AssignASCII(sListenerTypes[LISTENER_TYPE_PROGRESS]);

  nsCOMPtr<nsIDOMEventListener> listener = GetOnXListener(type);
  listener.forget(aOnprogress);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::SetOnprogress(nsIDOMEventListener* aOnprogress)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsAutoString type;
  type.AssignASCII(sListenerTypes[LISTENER_TYPE_PROGRESS]);

  return SetOnXListener(type, aOnprogress);
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::GetOnloadend(nsIDOMEventListener** aOnloadend)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_ARG_POINTER(aOnloadend);

  nsAutoString type;
  type.AssignASCII(sListenerTypes[LISTENER_TYPE_LOADEND]);

  nsCOMPtr<nsIDOMEventListener> listener = GetOnXListener(type);
  listener.forget(aOnloadend);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHREventTarget::SetOnloadend(nsIDOMEventListener* aOnloadend)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsAutoString type;
  type.AssignASCII(sListenerTypes[LISTENER_TYPE_LOADEND]);

  return SetOnXListener(type, aOnloadend);
}

nsDOMWorkerXHRUpload::nsDOMWorkerXHRUpload(nsDOMWorkerXHR* aWorkerXHR)
: mWorkerXHR(aWorkerXHR)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aWorkerXHR, "Null pointer!");
}

NS_IMPL_ISUPPORTS_INHERITED1(nsDOMWorkerXHRUpload, nsDOMWorkerXHREventTarget,
                                                   nsIXMLHttpRequestUpload)

NS_IMPL_CI_INTERFACE_GETTER3(nsDOMWorkerXHRUpload, nsIDOMEventTarget,
                                                   nsIXMLHttpRequestEventTarget,
                                                   nsIXMLHttpRequestUpload)

NS_IMPL_THREADSAFE_DOM_CI_GETINTERFACES(nsDOMWorkerXHRUpload)

NS_IMETHODIMP
nsDOMWorkerXHRUpload::RemoveEventListener(const nsAString& aType,
                                          nsIDOMEventListener* aListener,
                                          PRBool aUseCapture)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_ARG_POINTER(aListener);

  if (mWorkerXHR->mWorker->IsCanceled()) {
    return NS_ERROR_ABORT;
  }

  return nsDOMWorkerXHREventTarget::RemoveEventListener(aType, aListener,
                                                        aUseCapture);
}

NS_IMETHODIMP
nsDOMWorkerXHRUpload::DispatchEvent(nsIDOMEvent* aEvent,
                                    PRBool* _retval)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_ARG_POINTER(aEvent);

  if (mWorkerXHR->mWorker->IsCanceled()) {
    return NS_ERROR_ABORT;
  }

  return nsDOMWorkerXHREventTarget::DispatchEvent(aEvent, _retval);
}

NS_IMETHODIMP
nsDOMWorkerXHRUpload::AddEventListener(const nsAString& aType,
                                       nsIDOMEventListener* aListener,
                                       PRBool aUseCapture,
                                       PRBool aWantsUntrusted,
                                       PRUint8 optional_argc)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_ARG_POINTER(aListener);

  if (mWorkerXHR->mWorker->IsCanceled()) {
    return NS_ERROR_ABORT;
  }

  nsresult rv = nsDOMWorkerXHREventTarget::AddEventListener(aType, aListener,
                                                            aUseCapture,
                                                            aWantsUntrusted,
                                                            optional_argc);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mWorkerXHR->mXHRProxy->UploadEventListenerAdded();
  if (NS_FAILED(rv)) {
    NS_WARNING("UploadEventListenerAdded failed!");
    RemoveEventListener(aType, aListener, aUseCapture);
    return rv;
  }

  return NS_OK;
}

nsresult
nsDOMWorkerXHRUpload::SetOnXListener(const nsAString& aType,
                                     nsIDOMEventListener* aListener)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mWorkerXHR->mCanceled) {
    return NS_ERROR_ABORT;
  }

  PRUint32 type = GetListenerTypeFromString(aType);
  if (type > sMaxUploadEventTypes) {
    // Silently ignore junk events.
    return NS_OK;
  }

  return nsDOMWorkerXHREventTarget::SetOnXListener(aType, aListener);
}

nsDOMWorkerXHR::nsDOMWorkerXHR(nsDOMWorker* aWorker)
: nsDOMWorkerFeature(aWorker),
  mWrappedNative(nsnull),
  mCanceled(PR_FALSE)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aWorker, "Must have a worker!");
}

nsDOMWorkerXHR::~nsDOMWorkerXHR()
{
  if (mXHRProxy) {
    if (!NS_IsMainThread()) {
      nsCOMPtr<nsIRunnable> runnable =
        NS_NewRunnableMethod(mXHRProxy, &nsDOMWorkerXHRProxy::Destroy);

      if (runnable) {
        mXHRProxy = nsnull;
        NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL);
      }
    }
    else {
      mXHRProxy->Destroy();
    }
  }
}

// Tricky! We use the AddRef/Release method of nsDOMWorkerFeature (to make sure
// we properly remove ourselves from the worker array) but inherit the QI of
// nsDOMWorkerXHREventTarget.
NS_IMPL_ADDREF_INHERITED(nsDOMWorkerXHR, nsDOMWorkerFeature)
NS_IMPL_RELEASE_INHERITED(nsDOMWorkerXHR, nsDOMWorkerFeature)

NS_IMPL_QUERY_INTERFACE_INHERITED2(nsDOMWorkerXHR, nsDOMWorkerXHREventTarget,
                                                   nsIXMLHttpRequest,
                                                   nsIXPCScriptable)

NS_IMPL_CI_INTERFACE_GETTER3(nsDOMWorkerXHR, nsIDOMEventTarget,
                                             nsIXMLHttpRequestEventTarget,
                                             nsIXMLHttpRequest)

NS_IMPL_THREADSAFE_DOM_CI_GETINTERFACES(nsDOMWorkerXHR)

#define XPC_MAP_CLASSNAME nsDOMWorkerXHR
#define XPC_MAP_QUOTED_CLASSNAME "XMLHttpRequest"
#define XPC_MAP_WANT_POSTCREATE
#define XPC_MAP_WANT_TRACE
#define XPC_MAP_WANT_FINALIZE

#define XPC_MAP_FLAGS                                  \
  nsIXPCScriptable::DONT_ENUM_QUERY_INTERFACE        | \
  nsIXPCScriptable::CLASSINFO_INTERFACES_ONLY        | \
  nsIXPCScriptable::DONT_REFLECT_INTERFACE_NAMES

#include "xpc_map_end.h"

NS_IMETHODIMP
nsDOMWorkerXHR::Trace(nsIXPConnectWrappedNative* /* aWrapper */,
                      JSTracer* aTracer,
                      JSObject* /*aObj */)
{
  if (!mCanceled) {
    nsDOMWorkerMessageHandler::Trace(aTracer);
    if (mUpload) {
      mUpload->Trace(aTracer);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::Finalize(nsIXPConnectWrappedNative* /* aWrapper */,
                         JSContext* /* aCx */,
                         JSObject* /* aObj */)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsDOMWorkerMessageHandler::ClearAllListeners();

  if (mUpload) {
    mUpload->ClearAllListeners();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::PostCreate(nsIXPConnectWrappedNative* aWrapper,
                           JSContext* /* aCx */,
                           JSObject* /* aObj */)
{
  mWrappedNative = aWrapper;
  return NS_OK;
}

nsresult
nsDOMWorkerXHR::Init()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

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
    MutexAutoLock lock(mWorker->GetLock());

    mCanceled = PR_TRUE;
    mUpload = nsnull;
  }

  if (mXHRProxy) {
    mXHRProxy->Destroy();
    mXHRProxy = nsnull;
  }

  mWorker = nsnull;
}

nsresult
nsDOMWorkerXHR::SetOnXListener(const nsAString& aType,
                               nsIDOMEventListener* aListener)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  PRUint32 type = GetListenerTypeFromString(aType);
  if (type > sMaxXHREventTypes) {
    // Silently ignore junk events.
    return NS_OK;
  }

  return nsDOMWorkerXHREventTarget::SetOnXListener(aType, aListener);
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetChannel(nsIChannel** aChannel)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  NS_ENSURE_ARG_POINTER(aChannel);
  *aChannel = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetResponseXML(nsIDOMDocument** aResponseXML)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

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
nsDOMWorkerXHR::Open(const nsACString& aMethod, const nsACString& aUrl,
                     PRBool aAsync, const nsAString& aUser,
                     const nsAString& aPassword, PRUint8 optional_argc)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  if (!optional_argc) {
      aAsync = PR_TRUE;
  }

  nsresult rv = mXHRProxy->Open(aMethod, aUrl, aAsync, aUser, aPassword);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::Send(nsIVariant* aBody)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (mCanceled) {
    return NS_ERROR_ABORT;
  }

  if (mWorker->IsClosing() && !mXHRProxy->mSyncRequest) {
    // Cheat and don't start this request since we know we'll never be able to
    // use the data.
    return NS_OK;
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

  if (mWorker->IsClosing() && !mXHRProxy->mSyncRequest) {
    // Cheat and don't start this request since we know we'll never be able to
    // use the data.
    return NS_OK;
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
nsDOMWorkerXHR::GetReadyState(PRUint16* aReadyState)
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
                     nsPIDOMWindow* aOwnerWindow,
                     nsIURI* aBaseURI)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_NOTREACHED("No one should be calling this!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMWorkerXHR::GetUpload(nsIXMLHttpRequestUpload** aUpload)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<nsDOMWorker> worker = mWorker;
  if (!worker) {
    return NS_ERROR_ABORT;
  }

  MutexAutoLock lock(worker->GetLock());

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
  NS_ENSURE_ARG_POINTER(aOnreadystatechange);

  nsAutoString type;
  type.AssignASCII(sListenerTypes[LISTENER_TYPE_READYSTATECHANGE]);

  nsCOMPtr<nsIDOMEventListener> listener = GetOnXListener(type);
  listener.forget(aOnreadystatechange);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMWorkerXHR::SetOnreadystatechange(nsIDOMEventListener* aOnreadystatechange)
{
  nsAutoString type;
  type.AssignASCII(sListenerTypes[LISTENER_TYPE_READYSTATECHANGE]);

  return SetOnXListener(type, aOnreadystatechange);
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

NS_IMETHODIMP
nsDOMWorkerXHR::GetResponseType(nsAString& aResponseText)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMWorkerXHR::SetResponseType(const nsAString& aResponseText)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute jsval response; */
NS_IMETHODIMP
nsDOMWorkerXHR::GetResponse(JSContext *aCx, jsval *aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
