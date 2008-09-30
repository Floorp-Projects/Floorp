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

#ifndef __NSDOMWORKERXHRPROXY_H__
#define __NSDOMWORKERXHRPROXY_H__

// Bases
#include "nsThreadUtils.h"
#include "nsIDOMEventListener.h"
#include "nsIRequestObserver.h"

// Other includes
#include "nsCOMPtr.h"
#include "nsStringGlue.h"
#include "nsTArray.h"

// DOMWorker includes
#include "nsDOMWorkerXHR.h"

class nsIJSXMLHttpRequest;
class nsIThread;
class nsIVariant;
class nsIXMLHttpRequest;
class nsDOMWorkerXHREvent;
class nsDOMWorkerXHRWrappedListener;
class nsXMLHttpRequest;

class nsDOMWorkerXHRProxy : public nsRunnable,
                            public nsIDOMEventListener,
                            public nsIRequestObserver
{

  friend class nsDOMWorkerXHREvent;
  friend class nsDOMWorkerXHR;
  friend class nsDOMWorkerXHRUpload;

  typedef nsCOMPtr<nsIDOMEventListener> Listener;
  typedef nsTArray<Listener> ListenerArray;

  typedef nsRefPtr<nsDOMWorkerXHRWrappedListener> WrappedListener;

  typedef nsresult (NS_STDCALL nsIDOMEventTarget::*EventListenerFunction)
    (const nsAString&, nsIDOMEventListener*, PRBool);

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIREQUESTOBSERVER

  nsDOMWorkerXHRProxy(nsDOMWorkerXHR* aWorkerXHR);
  virtual ~nsDOMWorkerXHRProxy();

  nsresult Init();

  nsIXMLHttpRequest* GetXMLHttpRequest();

  nsresult Abort();

protected:
  nsresult InitInternal();
  void DestroyInternal();

  nsresult Destroy();

  void FlipOwnership();

  nsresult AddEventListener(PRUint32 aType,
                            nsIDOMEventListener* aListener,
                            PRBool aOnXListener,
                            PRBool aUploadListener);

  nsresult RemoveEventListener(PRUint32 aType,
                               nsIDOMEventListener* aListener,
                               PRBool aUploadListener);

  already_AddRefed<nsIDOMEventListener> GetOnXListener(PRUint32 aType,
                                                       PRBool aUploadListener);

  nsresult HandleWorkerEvent(nsDOMWorkerXHREvent* aEvent, PRBool aUploadEvent);

  nsresult HandleWorkerEvent(nsIDOMEvent* aEvent, PRBool aUploadEvent);

  nsresult HandleEventInternal(PRUint32 aType,
                               nsIDOMEvent* aEvent,
                               PRBool aUploadEvent);

  void ClearEventListeners();

  // Methods of nsIXMLHttpRequest that we implement
  nsresult GetAllResponseHeaders(char** _retval);
  nsresult GetResponseHeader(const nsACString& aHeader,
                             nsACString& _retval);
  nsresult OpenRequest(const nsACString& aMethod,
                       const nsACString& aUrl,
                       PRBool aAsync,
                       const nsAString& aUser,
                       const nsAString& aPassword);
  nsresult Send(nsIVariant* aBody);
  nsresult SendAsBinary(const nsAString& aBody);
  nsresult GetResponseText(nsAString& _retval);
  nsresult GetStatusText(nsACString& _retval);
  nsresult GetStatus(nsresult* _retval);
  nsresult GetReadyState(PRInt32* _retval);
  nsresult SetRequestHeader(const nsACString& aHeader,
                            const nsACString& aValue);
  nsresult OverrideMimeType(const nsACString& aMimetype);
  nsresult GetMultipart(PRBool* aMultipart);
  nsresult SetMultipart(PRBool aMultipart);

  // May be weak or strong, check mOwnedByXHR.
  nsDOMWorkerXHR* mWorkerXHR;

  // May be weak or strong, check mOwnedByXHR.
  nsIXMLHttpRequest* mXHR;

  // Always weak!
  nsXMLHttpRequest* mConcreteXHR;
  nsIXMLHttpRequestUpload* mUpload;

  nsCOMPtr<nsIThread> mMainThread;

  nsRefPtr<nsDOMWorkerXHREvent> mLastXHREvent;

  nsTArray<ListenerArray> mXHRListeners;
  nsTArray<WrappedListener> mXHROnXListeners;

  nsTArray<ListenerArray> mUploadListeners;
  nsTArray<WrappedListener> mUploadOnXListeners;

  // Whether or not this object is owned by the real XHR object.
  PRPackedBool mOwnedByXHR;

  PRPackedBool mMultipart;
  PRPackedBool mCanceled;
};

#endif /* __NSDOMWORKERXHRPROXY_H__ */
