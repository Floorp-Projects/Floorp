/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InProcessBrowserChildMessageManager.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsFrameLoader.h"
#include "nsFrameLoaderOwner.h"
#include "nsQueryObject.h"
#include "xpcpublic.h"
#include "nsIMozBrowserFrame.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/ChromeMessageSender.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/MessageManagerBinding.h"
#include "mozilla/dom/SameProcessMessageQueue.h"
#include "mozilla/dom/ScriptLoader.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/dom/JSActorService.h"
#include "mozilla/HoldDropJSObjects.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;

/* static */
already_AddRefed<InProcessBrowserChildMessageManager>
InProcessBrowserChildMessageManager::Create(nsDocShell* aShell,
                                            nsIContent* aOwner,
                                            nsFrameMessageManager* aChrome) {
  RefPtr<InProcessBrowserChildMessageManager> mm =
      new InProcessBrowserChildMessageManager(aShell, aOwner, aChrome);

  NS_ENSURE_TRUE(mm->Init(), nullptr);

  if (XRE_IsParentProcess()) {
    RefPtr<JSActorService> wasvc = JSActorService::GetSingleton();
    wasvc->RegisterChromeEventTarget(mm);
  }

  return mm.forget();
}

bool InProcessBrowserChildMessageManager::DoSendBlockingMessage(
    const nsAString& aMessage, StructuredCloneData& aData,
    nsTArray<StructuredCloneData>* aRetVal) {
  SameProcessMessageQueue* queue = SameProcessMessageQueue::Get();
  queue->Flush();

  if (mChromeMessageManager) {
    RefPtr<nsFrameMessageManager> mm = mChromeMessageManager;
    RefPtr<nsFrameLoader> fl = GetFrameLoader();
    mm->ReceiveMessage(mOwner, fl, aMessage, true, &aData, aRetVal,
                       IgnoreErrors());
  }
  return true;
}

class nsAsyncMessageToParent : public nsSameProcessAsyncMessageBase,
                               public SameProcessMessageQueue::Runnable {
 public:
  explicit nsAsyncMessageToParent(
      InProcessBrowserChildMessageManager* aBrowserChild)
      : mBrowserChild(aBrowserChild) {}

  virtual nsresult HandleMessage() override {
    RefPtr<nsFrameLoader> fl = mBrowserChild->GetFrameLoader();
    ReceiveMessage(mBrowserChild->mOwner, fl,
                   mBrowserChild->mChromeMessageManager);
    return NS_OK;
  }
  RefPtr<InProcessBrowserChildMessageManager> mBrowserChild;
};

nsresult InProcessBrowserChildMessageManager::DoSendAsyncMessage(
    const nsAString& aMessage, StructuredCloneData& aData) {
  SameProcessMessageQueue* queue = SameProcessMessageQueue::Get();
  RefPtr<nsAsyncMessageToParent> ev = new nsAsyncMessageToParent(this);

  nsresult rv = ev->Init(aMessage, aData);
  if (NS_FAILED(rv)) {
    return rv;
  }

  queue->Push(ev);
  return NS_OK;
}

InProcessBrowserChildMessageManager::InProcessBrowserChildMessageManager(
    nsDocShell* aShell, nsIContent* aOwner, nsFrameMessageManager* aChrome)
    : ContentFrameMessageManager(new nsFrameMessageManager(this)),
      mDocShell(aShell),
      mLoadingScript(false),
      mPreventEventsEscaping(false),
      mOwner(aOwner),
      mChromeMessageManager(aChrome) {
  mozilla::HoldJSObjects(this);

  // If owner corresponds to an <iframe mozbrowser>, we'll have to tweak our
  // GetEventTargetParent implementation.
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwner);
  if (browserFrame) {
    mIsBrowserFrame = browserFrame->GetReallyIsBrowser();
  } else {
    mIsBrowserFrame = false;
  }
}

InProcessBrowserChildMessageManager::~InProcessBrowserChildMessageManager() {
  if (XRE_IsParentProcess()) {
    JSActorService::UnregisterChromeEventTarget(this);
  }

  mozilla::DropJSObjects(this);
}

// This method isn't automatically forwarded safely because it's notxpcom, so
// the IDL binding doesn't know what value to return.
void InProcessBrowserChildMessageManager::MarkForCC() {
  MarkScopesForCC();
  MessageManagerGlobal::MarkForCC();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(InProcessBrowserChildMessageManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    InProcessBrowserChildMessageManager, DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocShell)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(
    InProcessBrowserChildMessageManager, DOMEventTargetHelper)
  tmp->nsMessageManagerScriptExecutor::Trace(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(
    InProcessBrowserChildMessageManager, DOMEventTargetHelper)
  if (XRE_IsParentProcess()) {
    JSActorService::UnregisterChromeEventTarget(tmp);
  }

  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocShell)
  tmp->nsMessageManagerScriptExecutor::Unlink();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InProcessBrowserChildMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsIInProcessContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(ContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(InProcessBrowserChildMessageManager,
                         DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(InProcessBrowserChildMessageManager,
                          DOMEventTargetHelper)

JSObject* InProcessBrowserChildMessageManager::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return ContentFrameMessageManager_Binding::Wrap(aCx, this, aGivenProto);
}

void InProcessBrowserChildMessageManager::CacheFrameLoader(
    nsFrameLoader* aFrameLoader) {
  mFrameLoader = aFrameLoader;
}

Nullable<WindowProxyHolder> InProcessBrowserChildMessageManager::GetContent(
    ErrorResult& aError) {
  if (!mDocShell) {
    return nullptr;
  }
  return WindowProxyHolder(mDocShell->GetBrowsingContext());
}

already_AddRefed<nsIEventTarget>
InProcessBrowserChildMessageManager::GetTabEventTarget() {
  nsCOMPtr<nsIEventTarget> target = GetMainThreadSerialEventTarget();
  return target.forget();
}

void InProcessBrowserChildMessageManager::FireUnloadEvent() {
  // We're called from Document::MaybeInitializeFinalizeFrameLoaders, so it
  // should be safe to run script.
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());

  // Don't let the unload event propagate to chrome event handlers.
  mPreventEventsEscaping = true;
  DOMEventTargetHelper::DispatchTrustedEvent(u"unload"_ns);

  // Allow events fired during docshell destruction (pagehide, unload) to
  // propagate to the <browser> element since chrome code depends on this.
  mPreventEventsEscaping = false;
}

void InProcessBrowserChildMessageManager::DisconnectEventListeners() {
  if (mDocShell) {
    if (nsCOMPtr<nsPIDOMWindowOuter> win = mDocShell->GetWindow()) {
      win->SetChromeEventHandler(win->GetChromeEventHandler());
    }
  }
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }

  mDocShell = nullptr;
}

void InProcessBrowserChildMessageManager::Disconnect() {
  mChromeMessageManager = nullptr;
  mOwner = nullptr;
  if (mMessageManager) {
    static_cast<nsFrameMessageManager*>(mMessageManager.get())->Disconnect();
    mMessageManager = nullptr;
  }
}

NS_IMETHODIMP_(nsIContent*)
InProcessBrowserChildMessageManager::GetOwnerContent() { return mOwner; }

void InProcessBrowserChildMessageManager::GetEventTargetParent(
    EventChainPreVisitor& aVisitor) {
  aVisitor.mForceContentDispatch = true;
  aVisitor.mCanHandle = true;

  if (mPreventEventsEscaping) {
    aVisitor.SetParentTarget(nullptr, false);
    return;
  }

  if (mIsBrowserFrame &&
      (!mOwner || !nsContentUtils::IsInChromeDocshell(mOwner->OwnerDoc()))) {
    if (mOwner) {
      if (nsPIDOMWindowInner* innerWindow =
              mOwner->OwnerDoc()->GetInnerWindow()) {
        // 'this' is already a "chrome handler", so we consider window's
        // parent target to be part of that same part of the event path.
        aVisitor.SetParentTarget(innerWindow->GetParentTarget(), false);
      }
    }
  } else {
    aVisitor.SetParentTarget(mOwner, false);
  }
}

class nsAsyncScriptLoad : public Runnable {
 public:
  nsAsyncScriptLoad(InProcessBrowserChildMessageManager* aBrowserChild,
                    const nsAString& aURL, bool aRunInGlobalScope)
      : mozilla::Runnable("nsAsyncScriptLoad"),
        mBrowserChild(aBrowserChild),
        mURL(aURL),
        mRunInGlobalScope(aRunInGlobalScope) {}

  NS_IMETHOD Run() override {
    mBrowserChild->LoadFrameScript(mURL, mRunInGlobalScope);
    return NS_OK;
  }
  RefPtr<InProcessBrowserChildMessageManager> mBrowserChild;
  nsString mURL;
  bool mRunInGlobalScope;
};

void InProcessBrowserChildMessageManager::LoadFrameScript(
    const nsAString& aURL, bool aRunInGlobalScope) {
  if (!nsContentUtils::IsSafeToRunScript()) {
    nsContentUtils::AddScriptRunner(
        new nsAsyncScriptLoad(this, aURL, aRunInGlobalScope));
    return;
  }
  bool tmp = mLoadingScript;
  mLoadingScript = true;
  JS::Rooted<JSObject*> mm(mozilla::dom::RootingCx(), GetOrCreateWrapper());
  LoadScriptInternal(mm, aURL, !aRunInGlobalScope);
  mLoadingScript = tmp;
}

already_AddRefed<nsFrameLoader>
InProcessBrowserChildMessageManager::GetFrameLoader() {
  RefPtr<nsFrameLoaderOwner> owner = do_QueryObject(mOwner);
  RefPtr<nsFrameLoader> fl = owner ? owner->GetFrameLoader() : nullptr;
  if (!fl) {
    fl = mFrameLoader;
  }
  return fl.forget();
}
