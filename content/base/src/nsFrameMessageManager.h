/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
  nsFrameMessageManager(PRBool aChrome,
                        nsSyncMessageCallback aSyncCallback,
                        nsAsyncMessageCallback aAsyncCallback,
                        nsLoadScriptCallback aLoadScriptCallback,
                        void* aCallbackData,
                        nsFrameMessageManager* aParentManager,
                        JSContext* aContext,
                        PRBool aGlobal = PR_FALSE,
                        PRBool aProcessManager = PR_FALSE)
  : mChrome(aChrome), mGlobal(aGlobal), mIsProcessManager(aProcessManager),
    mParentManager(aParentManager),
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
        Disconnect(PR_FALSE);
    }
    if (mIsProcessManager) {
      if (this == sParentProcessManager) {
        sParentProcessManager = nsnull;
      }
      if (this == sChildProcessManager) {
        sChildProcessManager = nsnull;
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

  nsresult ReceiveMessage(nsISupports* aTarget, const nsAString& aMessage,
                          PRBool aSync, const nsAString& aJSON,
                          JSObject* aObjectsArray,
                          InfallibleTArray<nsString>* aJSONRetVal,
                          JSContext* aContext = nsnull);
  void AddChildManager(nsFrameMessageManager* aManager,
                       PRBool aLoadScripts = PR_TRUE);
  void RemoveChildManager(nsFrameMessageManager* aManager)
  {
    mChildManagers.RemoveObject(aManager);
  }

  void Disconnect(PRBool aRemoveFromParent = PR_TRUE);
  void SetCallbackData(void* aData, PRBool aLoadScripts = PR_TRUE);
  nsresult GetParamsForMessage(nsAString& aMessageName, nsAString& aJSON);
  nsresult SendAsyncMessageInternal(const nsAString& aMessage,
                                    const nsAString& aJSON);
  JSContext* GetJSContext() { return mContext; }
  nsFrameMessageManager* GetParentManager() { return mParentManager; }
  void SetParentManager(nsFrameMessageManager* aParent)
  {
    NS_ASSERTION(!mParentManager, "We have parent manager already!");
    NS_ASSERTION(mChrome, "Should not set parent manager!");
    mParentManager = aParent;
  }
  PRBool IsGlobal() { return mGlobal; }
  PRBool IsWindowLevel() { return mParentManager && mParentManager->IsGlobal(); }

  static nsFrameMessageManager* GetParentProcessManager()
  {
    return sParentProcessManager;
  }
  static nsFrameMessageManager* GetChildProcessManager()
  {
    return sChildProcessManager;
  }
protected:
  nsTArray<nsMessageListenerInfo> mListeners;
  nsCOMArray<nsIContentFrameMessageManager> mChildManagers;
  PRPackedBool mChrome;
  PRPackedBool mGlobal;
  PRPackedBool mIsProcessManager;
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
};

class nsScriptCacheCleaner;

struct nsFrameScriptExecutorJSObjectHolder
{
  nsFrameScriptExecutorJSObjectHolder(JSObject* aObject) : mObject(aObject) {}
  JSObject* mObject;
};

class nsFrameScriptExecutor
{
public:
  static void Shutdown();
protected:
  nsFrameScriptExecutor() : mCx(nsnull) {}
  void DidCreateCx();
  // Call this when you want to destroy mCx.
  void DestroyCx();
  void LoadFrameScriptInternal(const nsAString& aURL);
  nsCOMPtr<nsIXPConnectJSObjectHolder> mGlobal;
  JSContext* mCx;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  static nsDataHashtable<nsStringHashKey, nsFrameScriptExecutorJSObjectHolder*>* sCachedScripts;
  static nsRefPtr<nsScriptCacheCleaner> sScriptCacheCleaner;
};

class nsScriptCacheCleaner : public nsIObserver
{
  NS_DECL_ISUPPORTS

  nsScriptCacheCleaner()
  {
    nsCOMPtr<nsIObserverService> obsSvc = mozilla::services::GetObserverService();
    if (obsSvc)
      obsSvc->AddObserver(this, "xpcom-shutdown", PR_FALSE);
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
