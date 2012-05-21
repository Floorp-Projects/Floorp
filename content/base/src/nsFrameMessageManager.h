/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsFrameMessageManager_h__
#define nsFrameMessageManager_h__

#include "nsIFrameMessageManager.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsIAtom.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "nsIPrincipal.h"
#include "nsIXPConnect.h"
#include "nsDataHashtable.h"
#include "mozilla/Services.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {
class ContentParent;
}
}

class nsAXPCNativeCallContext;
struct JSContext;
struct JSObject;

struct nsMessageListenerInfo
{
  nsCOMPtr<nsIFrameMessageListener> mListener;
  nsCOMPtr<nsIAtom> mMessage;
};

typedef bool (*nsLoadScriptCallback)(void* aCallbackData, const nsAString& aURL);
typedef bool (*nsSyncMessageCallback)(void* aCallbackData,
                                      const nsAString& aMessage,
                                      const nsAString& aJSON,
                                      InfallibleTArray<nsString>* aJSONRetVal);
typedef bool (*nsAsyncMessageCallback)(void* aCallbackData,
                                       const nsAString& aMessage,
                                       const nsAString& aJSON);

class nsFrameMessageManager : public nsIContentFrameMessageManager,
                              public nsIChromeFrameMessageManager
{
public:
  nsFrameMessageManager(bool aChrome,
                        nsSyncMessageCallback aSyncCallback,
                        nsAsyncMessageCallback aAsyncCallback,
                        nsLoadScriptCallback aLoadScriptCallback,
                        void* aCallbackData,
                        nsFrameMessageManager* aParentManager,
                        JSContext* aContext,
                        bool aGlobal = false,
                        bool aProcessManager = false)
  : mChrome(aChrome), mGlobal(aGlobal), mIsProcessManager(aProcessManager),
    mHandlingMessage(false), mDisconnected(false), mParentManager(aParentManager),
    mSyncCallback(aSyncCallback), mAsyncCallback(aAsyncCallback),
    mLoadScriptCallback(aLoadScriptCallback), mCallbackData(aCallbackData),
    mContext(aContext)
  {
    NS_ASSERTION(mContext || (aChrome && !aParentManager) || aProcessManager,
                 "Should have mContext in non-global/non-process manager!");
    NS_ASSERTION(aChrome || !aParentManager, "Should not set parent manager!");
    // This is a bit hackish. When parent manager is global, we want
    // to attach the window message manager to it immediately.
    // Is it just the frame message manager which waits until the
    // content process is running.
    if (mParentManager && (mCallbackData || IsWindowLevel())) {
      mParentManager->AddChildManager(this);
    }
  }

  ~nsFrameMessageManager()
  {
    for (PRInt32 i = mChildManagers.Count(); i > 0; --i) {
      static_cast<nsFrameMessageManager*>(mChildManagers[i - 1])->
        Disconnect(false);
    }
    if (mIsProcessManager) {
      if (this == sParentProcessManager) {
        sParentProcessManager = nsnull;
      }
      if (this == sChildProcessManager) {
        sChildProcessManager = nsnull;
        delete sPendingSameProcessAsyncMessages;
        sPendingSameProcessAsyncMessages = nsnull;
      }
      if (this == sSameProcessParentManager) {
        sSameProcessParentManager = nsnull;
      }
    }
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsFrameMessageManager,
                                           nsIContentFrameMessageManager)
  NS_DECL_NSIFRAMEMESSAGEMANAGER
  NS_DECL_NSISYNCMESSAGESENDER
  NS_DECL_NSICONTENTFRAMEMESSAGEMANAGER
  NS_DECL_NSICHROMEFRAMEMESSAGEMANAGER
  NS_DECL_NSITREEITEMFRAMEMESSAGEMANAGER

  static nsFrameMessageManager*
  NewProcessMessageManager(mozilla::dom::ContentParent* aProcess);

  nsresult ReceiveMessage(nsISupports* aTarget, const nsAString& aMessage,
                          bool aSync, const nsAString& aJSON,
                          JSObject* aObjectsArray,
                          InfallibleTArray<nsString>* aJSONRetVal,
                          JSContext* aContext = nsnull);
  void AddChildManager(nsFrameMessageManager* aManager,
                       bool aLoadScripts = true);
  void RemoveChildManager(nsFrameMessageManager* aManager)
  {
    mChildManagers.RemoveObject(aManager);
  }

  void Disconnect(bool aRemoveFromParent = true);
  void SetCallbackData(void* aData, bool aLoadScripts = true);
  void* GetCallbackData() { return mCallbackData; }
  void GetParamsForMessage(const jsval& aObject,
                           JSContext* aCx,
                           nsAString& aJSON);
  nsresult SendAsyncMessageInternal(const nsAString& aMessage,
                                    const nsAString& aJSON);
  JSContext* GetJSContext() { return mContext; }
  void SetJSContext(JSContext* aCx) { mContext = aCx; }
  void RemoveFromParent();
  nsFrameMessageManager* GetParentManager() { return mParentManager; }
  void SetParentManager(nsFrameMessageManager* aParent)
  {
    NS_ASSERTION(!mParentManager, "We have parent manager already!");
    NS_ASSERTION(mChrome, "Should not set parent manager!");
    mParentManager = aParent;
  }
  bool IsGlobal() { return mGlobal; }
  bool IsWindowLevel() { return mParentManager && mParentManager->IsGlobal(); }

  static nsFrameMessageManager* GetParentProcessManager()
  {
    return sParentProcessManager;
  }
  static nsFrameMessageManager* GetChildProcessManager()
  {
    return sChildProcessManager;
  }
protected:
  friend class MMListenerRemover;
  nsTArray<nsMessageListenerInfo> mListeners;
  nsCOMArray<nsIContentFrameMessageManager> mChildManagers;
  bool mChrome;
  bool mGlobal;
  bool mIsProcessManager;
  bool mHandlingMessage;
  bool mDisconnected;
  nsFrameMessageManager* mParentManager;
  nsSyncMessageCallback mSyncCallback;
  nsAsyncMessageCallback mAsyncCallback;
  nsLoadScriptCallback mLoadScriptCallback;
  void* mCallbackData;
  JSContext* mContext;
  nsTArray<nsString> mPendingScripts;
public:
  static nsFrameMessageManager* sParentProcessManager;
  static nsFrameMessageManager* sChildProcessManager;
  static nsFrameMessageManager* sSameProcessParentManager;
  static nsTArray<nsCOMPtr<nsIRunnable> >* sPendingSameProcessAsyncMessages;
};

void
ContentScriptErrorReporter(JSContext* aCx,
                           const char* aMessage,
                           JSErrorReport* aReport);

class nsScriptCacheCleaner;

struct nsFrameJSScriptExecutorHolder
{
  nsFrameJSScriptExecutorHolder(JSScript* aScript) : mScript(aScript)
  { MOZ_COUNT_CTOR(nsFrameJSScriptExecutorHolder); }
  ~nsFrameJSScriptExecutorHolder()
  { MOZ_COUNT_DTOR(nsFrameJSScriptExecutorHolder); }
  JSScript* mScript;
};

class nsFrameScriptExecutor
{
public:
  static void Shutdown();
protected:
  friend class nsFrameScriptCx;
  nsFrameScriptExecutor() : mCx(nsnull), mCxStackRefCnt(0),
                            mDelayedCxDestroy(false)
  { MOZ_COUNT_CTOR(nsFrameScriptExecutor); }
  ~nsFrameScriptExecutor()
  { MOZ_COUNT_DTOR(nsFrameScriptExecutor); }
  void DidCreateCx();
  // Call this when you want to destroy mCx.
  void DestroyCx();
  void LoadFrameScriptInternal(const nsAString& aURL);
  bool InitTabChildGlobalInternal(nsISupports* aScope);
  static void Traverse(nsFrameScriptExecutor *tmp,
                       nsCycleCollectionTraversalCallback &cb);
  nsCOMPtr<nsIXPConnectJSObjectHolder> mGlobal;
  JSContext* mCx;
  PRUint32 mCxStackRefCnt;
  bool mDelayedCxDestroy;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  static nsDataHashtable<nsStringHashKey, nsFrameJSScriptExecutorHolder*>* sCachedScripts;
  static nsRefPtr<nsScriptCacheCleaner> sScriptCacheCleaner;
};

class nsFrameScriptCx
{
public:
  nsFrameScriptCx(nsISupports* aOwner, nsFrameScriptExecutor* aExec)
  : mOwner(aOwner), mExec(aExec)
  {
    ++(mExec->mCxStackRefCnt);
  }
  ~nsFrameScriptCx()
  {
    if (--(mExec->mCxStackRefCnt) == 0 &&
        mExec->mDelayedCxDestroy) {
      mExec->DestroyCx();
    }
  }
  nsCOMPtr<nsISupports> mOwner;
  nsFrameScriptExecutor* mExec;
};

class nsScriptCacheCleaner : public nsIObserver
{
  NS_DECL_ISUPPORTS

  nsScriptCacheCleaner()
  {
    nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
    if (obsSvc)
      obsSvc->AddObserver(this, "xpcom-shutdown", false);
  }

  NS_IMETHODIMP Observe(nsISupports *aSubject,
                        const char *aTopic,
                        const PRUnichar *aData)
  {
    nsFrameScriptExecutor::Shutdown();
    return NS_OK;
  }
};

#endif
