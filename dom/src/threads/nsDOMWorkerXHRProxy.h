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
#include "nsIDOMEventTarget.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsStringGlue.h"
#include "nsTArray.h"

class nsIJSXMLHttpRequest;
class nsIThread;
class nsIVariant;
class nsIXMLHttpRequest;
class nsIXMLHttpRequestUpload;
class nsIXPConnectWrappedNative;
class nsDOMWorkerXHR;
class nsDOMWorkerXHREvent;
class nsDOMWorkerXHRFinishSyncXHRRunnable;
class nsDOMWorkerXHRState;
class nsDOMWorkerXHRWrappedListener;
class nsXMLHttpRequest;

class nsDOMWorkerXHRProxy : public nsIRunnable,
                            public nsIDOMEventListener,
                            public nsIRequestObserver
{
  friend class nsDOMWorkerXHRAttachUploadListenersRunnable;
  friend class nsDOMWorkerXHREvent;
  friend class nsDOMWorkerXHRFinishSyncXHRRunnable;
  friend class nsDOMWorkerXHRLastProgressOrLoadEvent;
  friend class nsDOMWorkerXHR;
  friend class nsDOMWorkerXHRUpload;

public:
  typedef nsAutoTArray<nsCOMPtr<nsIRunnable>, 5> SyncEventQueue;

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIREQUESTOBSERVER

  nsDOMWorkerXHRProxy(nsDOMWorkerXHR* aWorkerXHR);
  virtual ~nsDOMWorkerXHRProxy();

  nsresult Init();

  nsIXMLHttpRequest* GetXMLHttpRequest();

  nsresult Open(const nsACString& aMethod,
                const nsACString& aUrl,
                PRBool aAsync,
                const nsAString& aUser,
                const nsAString& aPassword);

  nsresult Abort();

  SyncEventQueue* SetSyncEventQueue(SyncEventQueue* aQueue);

  PRInt32 ChannelID() {
    return mChannelID;
  }

protected:
  struct ProgressInfo {
    ProgressInfo() : computable(PR_FALSE), loaded(0), total(0) { }

    PRBool computable;
    PRUint64 loaded;
    PRUint64 total;
  };

  nsresult InitInternal();
  void DestroyInternal();

  nsresult Destroy();

  void AddRemoveXHRListeners(PRBool aAdd);
  void FlipOwnership();

  nsresult UploadEventListenerAdded();

  nsresult HandleWorkerEvent(nsDOMWorkerXHREvent* aEvent,
                             PRBool aUploadEvent);

  nsresult HandleEventRunnable(nsIRunnable* aRunnable);

  // Methods of nsIXMLHttpRequest that we implement
  nsresult GetAllResponseHeaders(char** _retval);
  nsresult GetResponseHeader(const nsACString& aHeader,
                             nsACString& _retval);
  nsresult Send(nsIVariant* aBody);
  nsresult SendAsBinary(const nsAString& aBody);
  nsresult GetResponseText(nsAString& _retval);
  nsresult GetStatusText(nsACString& _retval);
  nsresult GetStatus(nsresult* _retval);
  nsresult GetReadyState(PRUint16* _retval);
  nsresult SetRequestHeader(const nsACString& aHeader,
                            const nsACString& aValue);
  nsresult OverrideMimeType(const nsACString& aMimetype);
  nsresult GetMultipart(PRBool* aMultipart);
  nsresult SetMultipart(PRBool aMultipart);
  nsresult GetWithCredentials(PRBool* aWithCredentials);
  nsresult SetWithCredentials(PRBool aWithCredentials);

  nsresult RunSyncEventLoop();

  PRBool IsUploadEvent(nsIDOMEvent* aEvent);

  nsresult DispatchPrematureAbortEvents(PRUint32 aType,
                                        nsIDOMEventTarget* aTarget,
                                        ProgressInfo* aProgressInfo);

  nsresult MaybeDispatchPrematureAbortEvents(PRBool aFromOpenRequest);

  // May be weak or strong, check mOwnedByXHR.
  nsDOMWorkerXHR* mWorkerXHR;
  nsCOMPtr<nsIXPConnectWrappedNative> mWorkerXHRWN;

  // May be weak or strong, check mOwnedByXHR.
  nsIXMLHttpRequest* mXHR;

  // Always weak!
  nsXMLHttpRequest* mConcreteXHR;
  nsIXMLHttpRequestUpload* mUpload;

  nsCOMPtr<nsIThread> mMainThread;

  nsRefPtr<nsDOMWorkerXHRState> mLastXHRState;
  nsRefPtr<nsDOMWorkerXHREvent> mLastProgressOrLoadEvent;

  SyncEventQueue* mSyncEventQueue;

  PRInt32 mChannelID;

  // Only touched on the worker thread!
  nsCOMPtr<nsIThread> mSyncXHRThread;

  // Touched on more than one thread, protected by the worker's lock.
  nsRefPtr<nsDOMWorkerXHRFinishSyncXHRRunnable> mSyncFinishedRunnable;

  nsAutoPtr<ProgressInfo> mDownloadProgressInfo;
  nsAutoPtr<ProgressInfo> mUploadProgressInfo;

  // Whether or not this object is owned by the real XHR object.
  PRPackedBool mOwnedByXHR;

  PRPackedBool mWantUploadListeners;
  PRPackedBool mCanceled;

  PRPackedBool mSyncRequest;

};

#endif /* __NSDOMWORKERXHRPROXY_H__ */
