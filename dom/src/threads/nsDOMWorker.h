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

#ifndef __NSDOMWORKER_H__
#define __NSDOMWORKER_H__

#include "nsIDOMEventTarget.h"
#include "nsIDOMWorkers.h"
#include "nsIJSNativeInitializer.h"
#include "nsIPrincipal.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsIXPCScriptable.h"

#include "jsapi.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsTPtrArray.h"
#include "prlock.h"

#include "nsDOMWorkerMessageHandler.h"

class nsDOMWorker;
class nsDOMWorkerFeature;
class nsDOMWorkerMessageHandler;
class nsDOMWorkerNavigator;
class nsDOMWorkerPool;
class nsDOMWorkerTimeout;
class nsICancelable;
class nsIDOMEventListener;
class nsIEventTarget;
class nsIRunnable;
class nsIScriptGlobalObject;
class nsIXPConnectWrappedNative;

class nsDOMWorkerScope : public nsDOMWorkerMessageHandler,
                         public nsIWorkerScope,
                         public nsIXPCScriptable
{
  friend class nsDOMWorker;

  typedef nsresult (NS_STDCALL nsDOMWorkerScope::*SetListenerFunc)
    (nsIDOMEventListener*);

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMEVENTTARGET
  // nsIDOMNSEventTarget
  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener* aListener,
                              PRBool aUseCapture,
                              PRBool aWantsUntrusted,
                              PRUint8 optional_argc);
  NS_DECL_NSIWORKERGLOBALSCOPE
  NS_DECL_NSIWORKERSCOPE
  NS_DECL_NSIXPCSCRIPTABLE
  NS_DECL_NSICLASSINFO

  nsDOMWorkerScope(nsDOMWorker* aWorker);

protected:
  already_AddRefed<nsIXPConnectWrappedNative> GetWrappedNative();

private:
  nsDOMWorker* mWorker;
  nsIXPConnectWrappedNative* mWrappedNative;

  nsRefPtr<nsDOMWorkerNavigator> mNavigator;

  PRPackedBool mHasOnerror;
};

class nsLazyAutoRequest
{
public:
  nsLazyAutoRequest() : mCx(nsnull) {}

  ~nsLazyAutoRequest() {
    if (mCx)
      JS_EndRequest(mCx);
  }

  void enter(JSContext *aCx) {
    JS_BeginRequest(aCx);
    mCx = aCx;
  }

  bool entered() const { return mCx != nsnull; }

  void swap(nsLazyAutoRequest &other) {
    JSContext *tmp = mCx;
    mCx = other.mCx;
    other.mCx = tmp;
  }

private:
  JSContext *mCx;
};

class nsDOMWorker : public nsDOMWorkerMessageHandler,
                    public nsIChromeWorker,
                    public nsITimerCallback,
                    public nsIJSNativeInitializer,
                    public nsIXPCScriptable
{
  friend class nsDOMWorkerFeature;
  friend class nsDOMWorkerFunctions;
  friend class nsDOMWorkerScope;
  friend class nsDOMWorkerScriptLoader;
  friend class nsDOMWorkerTimeout;
  friend class nsDOMWorkerXHR;
  friend class nsDOMWorkerXHRProxy;
  friend class nsReportErrorRunnable;
  friend class nsDOMFireEventRunnable;

  friend JSBool DOMWorkerOperationCallback(JSContext* aCx);
  friend void DOMWorkerErrorReporter(JSContext* aCx,
                                     const char* aMessage,
                                     JSErrorReport* aReport);

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMEVENTTARGET
  // nsIDOMNSEventTarget
  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener* aListener,
                              PRBool aUseCapture,
                              PRBool aWantsUntrusted,
                              PRUint8 optional_argc);
  NS_DECL_NSIABSTRACTWORKER
  NS_DECL_NSIWORKER
  NS_DECL_NSICHROMEWORKER
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSICLASSINFO
  NS_DECL_NSIXPCSCRIPTABLE

  static nsresult NewWorker(nsISupports** aNewObject);
  static nsresult NewChromeWorker(nsISupports** aNewObject);

  enum WorkerPrivilegeModel { CONTENT, CHROME };

  nsDOMWorker(nsDOMWorker* aParent,
              nsIXPConnectWrappedNative* aParentWN,
              WorkerPrivilegeModel aModel);

  NS_IMETHOD Initialize(nsISupports* aOwner,
                        JSContext* aCx,
                        JSObject* aObj,
                        PRUint32 aArgc,
                        jsval* aArgv);

  nsresult InitializeInternal(nsIScriptGlobalObject* aOwner,
                              JSContext* aCx,
                              JSObject* aObj,
                              PRUint32 aArgc,
                              jsval* aArgv);

  void Cancel();
  void Kill();
  void Suspend();
  void Resume();

  // This just calls IsCanceledNoLock with an autolock around the call.
  PRBool IsCanceled();

  PRBool IsClosing();
  PRBool IsSuspended();

  PRBool SetGlobalForContext(JSContext* aCx, nsLazyAutoRequest *aRequest, JSAutoEnterCompartment *aComp);

  void SetPool(nsDOMWorkerPool* aPool);

  nsDOMWorkerPool* Pool() {
    return mPool;
  }

  PRLock* Lock() {
    return mLock;
  }

  already_AddRefed<nsIXPConnectWrappedNative> GetWrappedNative();
  already_AddRefed<nsDOMWorker> GetParent();

  nsDOMWorkerScope* GetInnerScope() {
    return mInnerScope;
  }

  void SetExpirationTime(PRIntervalTime aExpirationTime);
#ifdef DEBUG
  PRIntervalTime GetExpirationTime();
#endif

  PRBool IsPrivileged() {
    return mPrivilegeModel == CHROME;
  }

  /**
   * Use this chart to help figure out behavior during each of the closing
   * statuses. Details below.
   * 
   * +=============+=============+=================+=======================+
   * |   status    | clear queue | abort execution | close handler timeout |
   * +=============+=============+=================+=======================+
   * |   eClosed   |     yes     |       no        |          no           |
   * +-------------+-------------+-----------------+-----------------------+
   * | eTerminated |     yes     |       yes       |          no           |
   * +-------------+-------------+-----------------+-----------------------+
   * |  eCanceled  |     yes     |       yes       |          yes          |
   * +-------------+-------------+-----------------+-----------------------+
   * 
   */

  enum DOMWorkerStatus {
    // This status means that the close handler has not yet been scheduled.
    eRunning = 0,

    // Inner script called Close() on the worker global scope. Setting this
    // status causes the worker to clear its queue of events but does not abort
    // the currently running script. The close handler is also scheduled with
    // no expiration time. This status may be superseded by 'eTerminated' in
    // which case the currently running script will be aborted as detailed
    // below. It may also be superseded by 'eCanceled' at which point the close
    // handler will be assigned an expiration time. Once the close handler has
    // completed or timed out the status will be changed to 'eKilled'.
    eClosed,

    // Outer script called Terminate() on the worker or the worker object was
    // garbage collected in its outer script. Setting this status causes the
    // worker to abort immediately, clear its queue of events, and schedules the
    // close handler with no expiration time. This status may be superseded by
    // 'eCanceled' at which point the close handler will have an expiration time
    // assigned. Once the close handler has completed or timed out the status
    // will be changed to 'eKilled'.
    eTerminated,

    // Either the user navigated away from the owning page, the owning page fell
    // out of bfcache, or the user quit the application. Setting this status
    // causes the worker to abort immediately and schedules the close handler
    // with an expiration time. Since the page has gone away the worker may not
    // post any messages. Once the close handler has completed or timed out the
    // status will be changed to 'eKilled'.
    eCanceled,

    // The close handler has run and the worker is effectively dead.
    eKilled
  };

private:
  ~nsDOMWorker();

  nsresult PostMessageInternal(PRBool aToInner);

  PRBool CompileGlobalObject(JSContext* aCx, nsLazyAutoRequest *aRequest, JSAutoEnterCompartment *aComp);

  PRUint32 NextTimeoutId() {
    return ++mNextTimeoutId;
  }

  nsresult AddFeature(nsDOMWorkerFeature* aFeature,
                      JSContext* aCx);
  void RemoveFeature(nsDOMWorkerFeature* aFeature,
                     JSContext* aCx);
  void CancelTimeoutWithId(PRUint32 aId);
  void SuspendFeatures();
  void ResumeFeatures();

  nsIPrincipal* GetPrincipal() {
    return mPrincipal;
  }

  void SetPrincipal(nsIPrincipal* aPrincipal) {
    mPrincipal = aPrincipal;
  }

  nsIURI* GetURI() {
    return mURI;
  }

  nsresult SetURI(nsIURI* aURI);

  nsresult FireCloseRunnable(PRIntervalTime aTimeoutInterval,
                             PRBool aClearQueue,
                             PRBool aFromFinalize);
  nsresult Close();

  nsresult TerminateInternal(PRBool aFromFinalize);

  nsIWorkerLocation* GetLocation() {
    return mLocation;
  }

  PRBool QueueSuspendedRunnable(nsIRunnable* aRunnable);

  // Determines if the worker should be considered "canceled". See the large
  // comment in the implementation for more details.
  PRBool IsCanceledNoLock();

private:

  // mParent will live as long as mParentWN but only mParentWN will keep the JS
  // reflection alive, so we only hold one strong reference to mParentWN.
  nsDOMWorker* mParent;
  nsCOMPtr<nsIXPConnectWrappedNative> mParentWN;

  // Whether or not this worker has chrome privileges. Never changed after the
  // worker is created.
  WorkerPrivilegeModel mPrivilegeModel;

  PRLock* mLock;

  nsRefPtr<nsDOMWorkerPool> mPool;

  nsDOMWorkerScope* mInnerScope;
  nsCOMPtr<nsIXPConnectWrappedNative> mScopeWN;
  JSObject* mGlobal;

  PRUint32 mNextTimeoutId;

  nsTArray<nsDOMWorkerFeature*> mFeatures;
  PRUint32 mFeatureSuspendDepth;

  nsString mScriptURL;

  nsIXPConnectWrappedNative* mWrappedNative;

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIURI> mURI;

  PRInt32 mErrorHandlerRecursionCount;

  // Always protected by mLock
  DOMWorkerStatus mStatus;

  // Always protected by mLock
  PRIntervalTime mExpirationTime;

  nsCOMPtr<nsITimer> mKillTimer;

  nsCOMPtr<nsIWorkerLocation> mLocation;

  nsTArray<nsCOMPtr<nsIRunnable> > mQueuedRunnables;

  PRPackedBool mSuspended;
  PRPackedBool mCompileAttempted;
};

/**
 * A worker "feature" holds the worker alive yet can be canceled, paused, and
 * resumed by the worker. It is up to each derived class to implement these
 * methods. This class uses a custom implementation of Release in order to
 * ensure no races between Cancel and object destruction can occur, so derived
 * classes must use the ISUPPORTS_INHERITED macros.
 *
 * To use this class you should inherit it and use the ISUPPORTS_INHERITED
 * macros. Then add or remove an instance to the worker using the
 * AddFeature/RemoveFeature functions. 
 */
class nsDOMWorkerFeature : public nsISupports
{
  friend class nsDOMWorker;

public:
  NS_DECL_ISUPPORTS

  nsDOMWorkerFeature(nsDOMWorker* aWorker)
  : mWorker(aWorker), mWorkerWN(aWorker->GetWrappedNative()), mId(0),
    mHasId(PR_FALSE), mFreeToDie(PR_TRUE) { }

  nsDOMWorkerFeature(nsDOMWorker* aWorker, PRUint32 aId)
  : mWorker(aWorker), mWorkerWN(aWorker->GetWrappedNative()), mId(aId),
    mHasId(PR_TRUE), mFreeToDie(PR_TRUE) { }

  virtual void Cancel() = 0;
  virtual void Suspend() { }
  virtual void Resume() { }

  PRUint32 GetId() {
    return mId;
  }

  PRBool HasId() {
    return mHasId;
  }

protected:
  virtual ~nsDOMWorkerFeature() { }

private:
  void FreeToDie(PRBool aFreeToDie) {
    mFreeToDie = aFreeToDie;
  }

protected:
  nsRefPtr<nsDOMWorker> mWorker;
  nsCOMPtr<nsIXPConnectWrappedNative> mWorkerWN;
  PRUint32 mId;

private:
  PRPackedBool mHasId;
  PRPackedBool mFreeToDie;
};

#endif /* __NSDOMWORKER_H__ */
