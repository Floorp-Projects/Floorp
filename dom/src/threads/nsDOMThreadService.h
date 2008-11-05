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
 *   Vladimir Vukicevic <vladimir@pobox.com> (Original Author)
 *   Ben Turner <bent.mozilla@gmail.com>
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

#ifndef __NSDOMTHREADSERVICE_H__
#define __NSDOMTHREADSERVICE_H__

// Interfaces
#include "nsIEventTarget.h"
#include "nsIObserver.h"
#include "nsIThreadPool.h"
#include "nsIDOMThreads.h"

// Other includes
#include "jsapi.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsRefPtrHashtable.h"
#include "nsTPtrArray.h"
#include "prmon.h"

#include "prlog.h"
#ifdef PR_LOGGING
extern PRLogModuleInfo* gDOMThreadsLog;
#endif

class nsDOMWorkerPool;
class nsDOMWorkerRunnable;
class nsDOMWorkerThread;
class nsDOMWorkerTimeout;
class nsIJSRuntimeService;
class nsIScriptGlobalObject;
class nsIThreadJSContextStack;
class nsIXPConnect;
class nsIXPCSecurityManager;

class nsDOMThreadService : public nsIEventTarget,
                           public nsIObserver,
                           public nsIThreadPoolListener,
                           public nsIDOMThreadService
{
  friend class nsDOMWorkerPool;
  friend class nsDOMWorkerRunnable;
  friend class nsDOMWorkerThread;
  friend class nsDOMWorkerTimeout;
  friend class nsDOMWorkerXHR;
  friend class nsDOMWorkerXHRProxy;
  friend class nsLayoutStatics;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEVENTTARGET
  NS_DECL_NSIOBSERVER
  NS_DECL_NSITHREADPOOLLISTENER
  NS_DECL_NSIDOMTHREADSERVICE

  // Any DOM consumers that need access to this service should use this method.
  static already_AddRefed<nsIDOMThreadService> GetOrInitService();

  // Simple getter for this service. This does not create the service if it
  // hasn't been created already, and it never AddRef's!
  static nsDOMThreadService* get();

  // Easy access to the services we care about.
  static nsIJSRuntimeService* JSRuntimeService();
  static nsIThreadJSContextStack* ThreadJSContextStack();
  static nsIXPCSecurityManager* WorkerSecurityManager();

  void CancelWorkersForGlobal(nsIScriptGlobalObject* aGlobalObject);
  void SuspendWorkersForGlobal(nsIScriptGlobalObject* aGlobalObject);
  void ResumeWorkersForGlobal(nsIScriptGlobalObject* aGlobalObject);

  nsresult ChangeThreadPoolMaxThreads(PRInt16 aDelta);

private:
  nsDOMThreadService();
  ~nsDOMThreadService();

  nsresult Init();
  void Cleanup();

  static void Shutdown();

  nsresult Dispatch(nsDOMWorkerThread* aWorker,
                    nsIRunnable* aRunnable);

  void WorkerComplete(nsDOMWorkerRunnable* aRunnable);

  void WaitForCanceledWorker(nsDOMWorkerThread* aWorker);

  static JSContext* CreateJSContext();

  void NoteDyingPool(nsDOMWorkerPool* aPool);

  void TimeoutReady(nsDOMWorkerTimeout* aTimeout);

  // Our internal thread pool.
  nsCOMPtr<nsIThreadPool> mThreadPool;

  // Weak references, only ever touched on the main thread!
  nsTPtrArray<nsDOMWorkerPool> mPools;

  // mMonitor protects all access to mWorkersInProgress and
  // mCreationsInProgress.
  PRMonitor* mMonitor;

  // A map from nsDOMWorkerThread to nsDOMWorkerRunnable.
  nsRefPtrHashtable<nsVoidPtrHashKey, nsDOMWorkerRunnable> mWorkersInProgress;
};

#endif /* __NSDOMTHREADSERVICE_H__ */
