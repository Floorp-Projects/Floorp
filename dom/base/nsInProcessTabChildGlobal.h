/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsInProcessTabChildGlobal_h
#define nsInProcessTabChildGlobal_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/ContentFrameMessageManager.h"
#include "nsCOMPtr.h"
#include "nsFrameMessageManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIClassInfo.h"
#include "nsIDocShell.h"
#include "nsCOMArray.h"
#include "nsIRunnable.h"
#include "nsIGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsWeakReference.h"

namespace mozilla {
class EventChainPreVisitor;
} // namespace mozilla

class nsInProcessTabChildGlobal final : public mozilla::dom::ContentFrameMessageManager,
                                        public nsMessageManagerScriptExecutor,
                                        public nsIInProcessContentFrameMessageManager,
                                        public nsIGlobalObject,
                                        public nsIScriptObjectPrincipal,
                                        public nsSupportsWeakReference,
                                        public mozilla::dom::ipc::MessageManagerCallback
{
  typedef mozilla::dom::ipc::StructuredCloneData StructuredCloneData;

private:
  nsInProcessTabChildGlobal(nsIDocShell* aShell, nsIContent* aOwner,
                            nsFrameMessageManager* aChrome);

  bool Init();

public:
  static already_AddRefed<nsInProcessTabChildGlobal> Create(nsIDocShell* aShell,
                                                            nsIContent* aOwner,
                                                            nsFrameMessageManager* aChrome)
  {
    RefPtr<nsInProcessTabChildGlobal> global =
      new nsInProcessTabChildGlobal(aShell, aOwner, aChrome);

    NS_ENSURE_TRUE(global->Init(), nullptr);

    return global.forget();
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(nsInProcessTabChildGlobal,
                                                         mozilla::DOMEventTargetHelper)

  void MarkForCC();

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override
  {
    MOZ_CRASH("We should never get here!");
  }
  virtual bool WrapGlobalObject(JSContext* aCx,
                                JS::RealmOptions& aOptions,
                                JS::MutableHandle<JSObject*> aReflector) override;

  virtual already_AddRefed<nsPIDOMWindowOuter>
    GetContent(mozilla::ErrorResult& aError) override;
  virtual already_AddRefed<nsIDocShell>
    GetDocShell(mozilla::ErrorResult& aError) override
  {
    nsCOMPtr<nsIDocShell> docShell(mDocShell);
    return docShell.forget();
  }
  virtual already_AddRefed<nsIEventTarget> GetTabEventTarget() override;

  NS_FORWARD_SAFE_NSIMESSAGESENDER(mMessageManager)
  NS_DECL_NSICONTENTFRAMEMESSAGEMANAGER

  NS_DECL_NSIINPROCESSCONTENTFRAMEMESSAGEMANAGER

  void CacheFrameLoader(nsFrameLoader* aFrameLoader);

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

  void GetEventTargetParent(mozilla::EventChainPreVisitor& aVisitor) override;

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

  virtual JSObject* GetGlobalJSObject() override
  {
    return GetWrapper();
  }

  already_AddRefed<nsFrameLoader> GetFrameLoader();

protected:
  virtual ~nsInProcessTabChildGlobal();

  nsCOMPtr<nsIDocShell> mDocShell;
  bool mLoadingScript;

  // Is this the message manager for an in-process <iframe mozbrowser>? This
  // affects where events get sent, so it affects GetEventTargetParent.
  bool mIsBrowserFrame;
  bool mPreventEventsEscaping;

  // We keep a strong reference to the frameloader after we've started
  // teardown. This allows us to dispatch message manager messages during this
  // time.
  RefPtr<nsFrameLoader> mFrameLoader;
public:
  nsIContent* mOwner;
  nsFrameMessageManager* mChromeMessageManager;
};

#endif
