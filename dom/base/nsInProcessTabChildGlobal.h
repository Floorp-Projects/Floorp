/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsInProcessTabChildGlobal_h
#define nsInProcessTabChildGlobal_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsCOMPtr.h"
#include "nsFrameMessageManager.h"
#include "nsIScriptContext.h"
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

namespace mozilla {
class EventChainPreVisitor;
} // namespace mozilla

class nsInProcessTabChildGlobal : public mozilla::DOMEventTargetHelper,
                                  public nsMessageManagerScriptExecutor,
                                  public nsIInProcessContentFrameMessageManager,
                                  public nsIGlobalObject,
                                  public nsIScriptObjectPrincipal,
                                  public nsSupportsWeakReference,
                                  public mozilla::dom::ipc::MessageManagerCallback
{
  typedef mozilla::dom::ipc::StructuredCloneData StructuredCloneData;

  using mozilla::dom::ipc::MessageManagerCallback::GetProcessMessageManager;

public:
  nsInProcessTabChildGlobal(nsIDocShell* aShell, nsIContent* aOwner,
                            nsFrameMessageManager* aChrome);
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(nsInProcessTabChildGlobal,
                                                         mozilla::DOMEventTargetHelper)

  NS_FORWARD_SAFE_NSIMESSAGELISTENERMANAGER(mMessageManager)
  NS_FORWARD_SAFE_NSIMESSAGESENDER(mMessageManager)
  NS_FORWARD_SAFE_NSIMESSAGEMANAGERGLOBAL(mMessageManager)
  NS_IMETHOD SendSyncMessage(const nsAString& aMessageName,
                             JS::Handle<JS::Value> aObject,
                             JS::Handle<JS::Value> aRemote,
                             nsIPrincipal* aPrincipal,
                             JSContext* aCx,
                             uint8_t aArgc,
                             JS::MutableHandle<JS::Value> aRetval) override
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
                            JS::MutableHandle<JS::Value> aRetval) override
  {
    return mMessageManager
      ? mMessageManager->SendRpcMessage(aMessageName, aObject, aRemote,
                                        aPrincipal, aCx, aArgc, aRetval)
      : NS_ERROR_NULL_POINTER;
  }
  NS_IMETHOD GetContent(mozIDOMWindowProxy** aContent) override;
  NS_IMETHOD GetDocShell(nsIDocShell** aDocShell) override;
  NS_IMETHOD GetTabEventTarget(nsIEventTarget** aTarget) override;

  NS_DECL_NSIINPROCESSCONTENTFRAMEMESSAGEMANAGER

  /**
   * MessageManagerCallback methods that we override.
   */
  virtual bool DoSendBlockingMessage(JSContext* aCx,
                                      const nsAString& aMessage,
                                      StructuredCloneData& aData,
                                      JS::Handle<JSObject *> aCpows,
                                      nsIPrincipal* aPrincipal,
                                      nsTArray<StructuredCloneData>* aRetVal,
                                      bool aIsSync) override;
  virtual nsresult DoSendAsyncMessage(JSContext* aCx,
                                      const nsAString& aMessage,
                                      StructuredCloneData& aData,
                                      JS::Handle<JSObject *> aCpows,
                                      nsIPrincipal* aPrincipal) override;

  virtual nsresult GetEventTargetParent(
                     mozilla::EventChainPreVisitor& aVisitor) override;
  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener* aListener,
                              bool aUseCapture)
  {
    // By default add listeners only for trusted events!
    return mozilla::DOMEventTargetHelper::AddEventListener(aType, aListener,
                                                           aUseCapture, false,
                                                           2);
  }
  NS_IMETHOD AddEventListener(const nsAString& aType,
                              nsIDOMEventListener* aListener,
                              bool aUseCapture, bool aWantsUntrusted,
                              uint8_t optional_argc) override
  {
    return mozilla::DOMEventTargetHelper::AddEventListener(aType, aListener,
                                                           aUseCapture,
                                                           aWantsUntrusted,
                                                           optional_argc);
  }
  using mozilla::DOMEventTargetHelper::AddEventListener;

  virtual nsIPrincipal* GetPrincipal() override { return mPrincipal; }
  void LoadFrameScript(const nsAString& aURL, bool aRunInGlobalScope);
  void FireUnloadEvent();
  void DisconnectEventListeners();
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

  virtual JSObject* GetGlobalJSObject() override {
    return mGlobal;
  }
  virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override
  {
    MOZ_CRASH("nsInProcessTabChildGlobal doesn't use DOM bindings!");
  }

  already_AddRefed<nsIFrameLoader> GetFrameLoader();

protected:
  virtual ~nsInProcessTabChildGlobal();

  nsresult Init();
  nsresult InitTabChildGlobal();
  nsCOMPtr<nsIContentFrameMessageManager> mMessageManager;
  nsCOMPtr<nsIDocShell> mDocShell;
  bool mInitialized;
  bool mLoadingScript;

  // Is this the message manager for an in-process <iframe mozbrowser>? This
  // affects where events get sent, so it affects GetEventTargetParent.
  bool mIsBrowserFrame;
  bool mPreventEventsEscaping;

  // We keep a strong reference to the frameloader after we've started
  // teardown. This allows us to dispatch message manager messages during this
  // time.
  nsCOMPtr<nsIFrameLoader> mFrameLoader;
public:
  nsIContent* mOwner;
  nsFrameMessageManager* mChromeMessageManager;
};

#endif
