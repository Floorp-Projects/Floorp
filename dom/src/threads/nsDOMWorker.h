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
#include "nsIXPCScriptable.h"

#include "jsapi.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsTPtrArray.h"
#include "prlock.h"

#include "nsDOMWorkerMessageHandler.h"

class nsDOMWorkerMessageHandler;
class nsDOMWorkerPool;
class nsDOMWorkerScope;
class nsDOMWorkerTimeout;
class nsICancelable;
class nsIDOMEventListener;
class nsIEventTarget;
class nsIScriptGlobalObject;
class nsIXPConnectWrappedNative;

class nsDOMWorkerFeature;

class nsDOMWorker : public nsIWorker,
                    public nsIJSNativeInitializer,
                    public nsIXPCScriptable
{
  friend class nsDOMWorkerFeature;
  friend class nsDOMWorkerFunctions;
  friend class nsDOMWorkerRefPtr;
  friend class nsDOMWorkerScope;
  friend class nsDOMWorkerScriptLoader;
  friend class nsDOMWorkerTimeout;
  friend class nsDOMWorkerXHR;

  friend JSBool DOMWorkerOperationCallback(JSContext* aCx);
  friend void DOMWorkerErrorReporter(JSContext* aCx,
                                     const char* aMessage,
                                     JSErrorReport* aReport);

#ifdef DEBUG
  // For fun assertions.
  friend class nsDOMFireEventRunnable;
#endif

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIABSTRACTWORKER
  NS_DECL_NSIWORKER
  NS_FORWARD_SAFE_NSIDOMEVENTTARGET(mOuterHandler)
  NS_DECL_NSIXPCSCRIPTABLE

  static nsresult NewWorker(nsISupports** aNewObject);

  nsDOMWorker(nsDOMWorker* aParent,
              nsIXPConnectWrappedNative* aParentWN);

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
  void Suspend();
  void Resume();

  PRBool IsCanceled() {
    return mCanceled;
  }

  PRBool IsSuspended() {
    return mSuspended;
  }

  PRBool SetGlobalForContext(JSContext* aCx);

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

private:
  ~nsDOMWorker();

  nsresult PostMessageInternal(const nsAString& aMessage,
                               PRBool aToInner);

  PRBool CompileGlobalObject(JSContext* aCx);

  PRUint32 NextTimeoutId() {
    return mNextTimeoutId++;
  }

  nsresult AddFeature(nsDOMWorkerFeature* aFeature,
                      JSContext* aCx);
  void RemoveFeature(nsDOMWorkerFeature* aFeature,
                     JSContext* aCx);
  void CancelTimeoutWithId(PRUint32 aId);
  void SuspendFeatures();
  void ResumeFeatures();
  void CancelFeatures();

private:

  // mParent will live as long as mParentWN but only mParentWN will keep the JS
  // reflection alive, so we only hold one strong reference to mParentWN.
  nsDOMWorker* mParent;
  nsCOMPtr<nsIXPConnectWrappedNative> mParentWN;

  PRUint32 mCallbackCount;

  PRLock* mLock;

  nsRefPtr<nsDOMWorkerMessageHandler> mInnerHandler;
  nsRefPtr<nsDOMWorkerMessageHandler> mOuterHandler;

  nsRefPtr<nsDOMWorkerPool> mPool;

  nsDOMWorkerScope* mInnerScope;
  JSObject* mGlobal;

  PRUint32 mNextTimeoutId;

  nsTArray<nsDOMWorkerFeature*> mFeatures;
  PRUint32 mFeatureSuspendDepth;

  nsString mScriptURL;

  nsIXPConnectWrappedNative* mWrappedNative;

  PRPackedBool mCanceled;
  PRPackedBool mSuspended;
  PRPackedBool mCompileAttempted;
  PRPackedBool mTerminated;
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
