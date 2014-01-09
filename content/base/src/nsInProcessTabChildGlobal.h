/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsInProcessTabChildGlobal_h
#define nsInProcessTabChildGlobal_h

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsFrameMessageManager.h"
#include "nsIScriptContext.h"
#include "nsDOMEventTargetHelper.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIClassInfo.h"
#include "nsIDocShell.h"
#include "nsIDOMElement.h"
#include "nsCOMArray.h"
#include "nsIRunnable.h"
#include "nsIGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsWeakReference.h"

class nsInProcessTabChildGlobal : public nsDOMEventTargetHelper,
                                  public nsFrameScriptExecutor,
                                  public nsIInProcessContentFrameMessageManager,
                                  public nsIGlobalObject,
                                  public nsIScriptObjectPrincipal,
                                  public nsSupportsWeakReference,
                                  public mozilla::dom::ipc::MessageManagerCallback
{
public:
  nsInProcessTabChildGlobal(nsIDocShell* aShell, nsIContent* aOwner,
                            nsFrameMessageManager* aChrome);
  virtual ~nsInProcessTabChildGlobal();
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsInProcessTabChildGlobal,
                                           nsDOMEventTargetHelper)
  NS_FORWARD_SAFE_NSIMESSAGELISTENERMANAGER(mMessageManager)
  NS_FORWARD_SAFE_NSIMESSAGESENDER(mMessageManager)
  NS_IMETHOD SendSyncMessage(const nsAString& aMessageName,
                             JS::Handle<JS::Value> aObject,
                             JS::Handle<JS::Value> aRemote,
                             nsIPrincipal* aPrincipal,
                             JSContext* aCx,
                             uint8_t aArgc,
                             JS::MutableHandle<JS::Value> aRetval)
  {
    return mMessageManager
      ? mMessageManager->SendSyncMessage(aMessageName, aObject, aRemote,
                                         aPrincipal, aCx, aArgc, aRetval)
      : NS_ERROR_NULL_POINTER;
  }
  NS_IMETHOD SendRpcMessage(const nsAString& aMessageName,
                            JS::Handle<JS::Value> aObject,
                            JS::Handle<JS::Value> aRemote,
                            nsIPrincipal* aPrincipal,
                            JSContext* aCx,
                            uint8_t aArgc,
                            JS::MutableHandle<JS::Value> aRetval)
  {
    return mMessageManager
      ? mMessageManager->SendRpcMessage(aMessageName, aObject, aRemote,
                                        aPrincipal, aCx, aArgc, aRetval)
      : NS_ERROR_NULL_POINTER;
  }
  NS_IMETHOD GetContent(nsIDOMWindow** aContent) MOZ_OVERRIDE;
  NS_IMETHOD GetDocShell(nsIDocShell** aDocShell) MOZ_OVERRIDE;
  NS_IMETHOD Dump(const nsAString& aStr) MOZ_OVERRIDE
  {
    return mMessageManager ? mMessageManager->Dump(aStr) : NS_OK;
  }
  NS_IMETHOD PrivateNoteIntentionalCrash() MOZ_OVERRIDE;
  NS_IMETHOD Btoa(const nsAString& aBinaryData,
                  nsAString& aAsciiBase64String) MOZ_OVERRIDE;
  NS_IMETHOD Atob(const nsAString& aAsciiString,
                  nsAString& aBinaryData) MOZ_OVERRIDE;

  NS_DECL_NSIINPROCESSCONTENTFRAMEMESSAGEMANAGER

  /**
   * MessageManagerCallback methods that we override.
   */
  virtual bool DoSendBlockingMessage(JSContext* aCx,
                                      const nsAString& aMessage,
                                      const mozilla::dom::StructuredCloneData& aData,
                                      JS::Handle<JSObject *> aCpows,
                                      nsIPrincipal* aPrincipal,
                                      InfallibleTArray<nsString>* aJSONRetVal,
                                      bool aIsSync) MOZ_OVERRIDE;
  virtual bool DoSendAsyncMessage(JSContext* aCx,
                                  const nsAString& aMessage,
                                  const mozilla::dom::StructuredCloneData& aData,
                                  JS::Handle<JSObject *> aCpows,
                                  nsIPrincipal* aPrincipal) MOZ_OVERRIDE;

  virtual nsresult PreHandleEvent(nsEventChainPreVisitor& aVisitor) MOZ_OVERRIDE;
  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener* aListener,
                              bool aUseCapture)
  {
    // By default add listeners only for trusted events!
    return nsDOMEventTargetHelper::AddEventListener(aType, aListener,
                                                    aUseCapture, false, 2);
  }
  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener* aListener,
                              bool aUseCapture, bool aWantsUntrusted,
                              uint8_t optional_argc) MOZ_OVERRIDE
  {
    return nsDOMEventTargetHelper::AddEventListener(aType, aListener,
                                                    aUseCapture,
                                                    aWantsUntrusted,
                                                    optional_argc);
  }
  using nsDOMEventTargetHelper::AddEventListener;

  virtual JSContext* GetJSContextForEventHandlers() MOZ_OVERRIDE { return nsContentUtils::GetSafeJSContext(); }
  virtual nsIPrincipal* GetPrincipal() MOZ_OVERRIDE { return mPrincipal; }
  void LoadFrameScript(const nsAString& aURL, bool aRunInGlobalScope);
  void Disconnect();
  void SendMessageToParent(const nsString& aMessage, bool aSync,
                           const nsString& aJSON,
                           nsTArray<nsString>* aJSONRetVal);
  nsFrameMessageManager* GetInnerManager()
  {
    return static_cast<nsFrameMessageManager*>(mMessageManager.get());
  }

  void SetOwner(nsIContent* aOwner) { mOwner = aOwner; }
  nsFrameMessageManager* GetChromeMessageManager()
  {
    return mChromeMessageManager;
  }
  void SetChromeMessageManager(nsFrameMessageManager* aParent)
  {
    mChromeMessageManager = aParent;
  }

  void DelayedDisconnect();

  virtual JSObject* GetGlobalJSObject() MOZ_OVERRIDE {
    if (!mGlobal) {
      return nullptr;
    }

    return mGlobal->GetJSObject();
  }
protected:
  nsresult Init();
  nsresult InitTabChildGlobal();
  nsCOMPtr<nsIContentFrameMessageManager> mMessageManager;
  nsCOMPtr<nsIDocShell> mDocShell;
  bool mInitialized;
  bool mLoadingScript;
  bool mDelayedDisconnect;

  // Is this the message manager for an in-process <iframe mozbrowser> or
  // <iframe mozapp>?  This affects where events get sent, so it affects
  // PreHandleEvent.
  bool mIsBrowserOrAppFrame;
public:
  nsIContent* mOwner;
  nsFrameMessageManager* mChromeMessageManager;
  nsTArray<nsCOMPtr<nsIRunnable> > mASyncMessages;
};

#endif
