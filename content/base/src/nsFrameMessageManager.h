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
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsIAtom.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"

class nsAXPCNativeCallContext;
struct JSContext;

struct nsMessageListenerInfo
{
  nsCOMPtr<nsIFrameMessageListener> mListener;
  nsCOMPtr<nsIAtom> mMessage;
};

typedef bool (*nsLoadScriptCallback)(void* aCallbackData, const nsAString& aURL);
typedef bool (*nsSyncMessageCallback)(void* aCallbackData,
                                      const nsAString& aMessage,
                                      const nsAString& aJSON,
                                      nsTArray<nsString>* aJSONRetVal);
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
                        JSContext* aContext)
  : mChrome(aChrome), mParentManager(aParentManager),
    mSyncCallback(aSyncCallback), mAsyncCallback(aAsyncCallback),
    mLoadScriptCallback(aLoadScriptCallback), mCallbackData(aCallbackData),
    mContext(aContext)
  {
    NS_ASSERTION(mContext, "Should have mContext!");
    if (mParentManager && mCallbackData) {
      mParentManager->AddChildManager(this);
    }
  }

  ~nsFrameMessageManager()
  {
    for (PRInt32 i = mChildManagers.Count(); i > 0; --i) {
      static_cast<nsFrameMessageManager*>(mChildManagers[i - 1])->
        Disconnect(PR_FALSE);
    }
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsFrameMessageManager,
                                           nsIContentFrameMessageManager)
  NS_DECL_NSIFRAMEMESSAGEMANAGER
  NS_DECL_NSICONTENTFRAMEMESSAGEMANAGER
  NS_DECL_NSICHROMEFRAMEMESSAGEMANAGER

  nsresult ReceiveMessage(nsISupports* aTarget, const nsAString& aMessage,
                          PRBool aSync, const nsAString& aJSON,
                          nsTArray<nsString>* aJSONRetVal);
  void AddChildManager(nsFrameMessageManager* aManager);
  void RemoveChildManager(nsFrameMessageManager* aManager)
  {
    mChildManagers.RemoveObject(aManager);
  }

  void Disconnect(PRBool aRemoveFromParent = PR_TRUE);
  void SetCallbackData(void* aData);
  nsresult GetParamsForMessage(nsAString& aMessageName, nsAString& aJSON);
  nsresult SendAsyncMessageInternal(const nsAString& aMessage,
                                    const nsAString& aJSON);
protected:
  nsTArray<nsMessageListenerInfo> mListeners;
  nsCOMArray<nsIContentFrameMessageManager> mChildManagers;
  PRBool mChrome;
  nsFrameMessageManager* mParentManager;
  nsSyncMessageCallback mSyncCallback;
  nsAsyncMessageCallback mAsyncCallback;
  nsLoadScriptCallback mLoadScriptCallback;
  void* mCallbackData;
  JSContext* mContext;
  nsTArray<nsString> mPendingScripts;
};

#endif
