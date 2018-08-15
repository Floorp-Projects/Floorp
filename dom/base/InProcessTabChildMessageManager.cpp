/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InProcessTabChildMessageManager.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsComponentManagerUtils.h"
#include "nsFrameLoader.h"
#include "xpcpublic.h"
#include "nsIMozBrowserFrame.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/ChromeMessageSender.h"
#include "mozilla/dom/MessageManagerBinding.h"
#include "mozilla/dom/SameProcessMessageQueue.h"
#include "mozilla/dom/ScriptLoader.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;

bool
InProcessTabChildMessageManager::DoSendBlockingMessage(JSContext* aCx,
                                                       const nsAString& aMessage,
                                                       StructuredCloneData& aData,
                                                       JS::Handle<JSObject *> aCpows,
                                                       nsIPrincipal* aPrincipal,
                                                       nsTArray<StructuredCloneData>* aRetVal,
                                                       bool aIsSync)
{
  SameProcessMessageQueue* queue = SameProcessMessageQueue::Get();
  queue->Flush();

  if (mChromeMessageManager) {
    SameProcessCpowHolder cpows(JS::RootingContext::get(aCx), aCpows);
    RefPtr<nsFrameMessageManager> mm = mChromeMessageManager;
    RefPtr<nsFrameLoader> fl = GetFrameLoader();
    mm->ReceiveMessage(mOwner, fl, aMessage, true, &aData, &cpows, aPrincipal,
                       aRetVal, IgnoreErrors());
  }
  return true;
}

class nsAsyncMessageToParent : public nsSameProcessAsyncMessageBase,
                               public SameProcessMessageQueue::Runnable
{
public:
  nsAsyncMessageToParent(JS::RootingContext* aRootingCx,
                         JS::Handle<JSObject*> aCpows,
                         InProcessTabChildMessageManager* aTabChild)
    : nsSameProcessAsyncMessageBase(aRootingCx, aCpows)
    , mTabChild(aTabChild)
  { }

  virtual nsresult HandleMessage() override
  {
    RefPtr<nsFrameLoader> fl = mTabChild->GetFrameLoader();
    ReceiveMessage(mTabChild->mOwner, fl, mTabChild->mChromeMessageManager);
    return NS_OK;
  }
  RefPtr<InProcessTabChildMessageManager> mTabChild;
};

nsresult
InProcessTabChildMessageManager::DoSendAsyncMessage(JSContext* aCx,
                                                    const nsAString& aMessage,
                                                    StructuredCloneData& aData,
                                                    JS::Handle<JSObject *> aCpows,
                                                    nsIPrincipal* aPrincipal)
{
  SameProcessMessageQueue* queue = SameProcessMessageQueue::Get();
  JS::RootingContext* rcx = JS::RootingContext::get(aCx);
  RefPtr<nsAsyncMessageToParent> ev =
    new nsAsyncMessageToParent(rcx, aCpows, this);

  nsresult rv = ev->Init(aMessage, aData, aPrincipal);
  if (NS_FAILED(rv)) {
    return rv;
  }

  queue->Push(ev);
  return NS_OK;
}

InProcessTabChildMessageManager::InProcessTabChildMessageManager(nsIDocShell* aShell,
                                                                 nsIContent* aOwner,
                                                                 nsFrameMessageManager* aChrome)
: ContentFrameMessageManager(new nsFrameMessageManager(this)),
  mDocShell(aShell), mLoadingScript(false),
  mPreventEventsEscaping(false),
  mOwner(aOwner), mChromeMessageManager(aChrome)
{
  mozilla::HoldJSObjects(this);

  // If owner corresponds to an <iframe mozbrowser>, we'll have to tweak our
  // GetEventTargetParent implementation.
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwner);
  if (browserFrame) {
    mIsBrowserFrame = browserFrame->GetReallyIsBrowser();
  }
  else {
    mIsBrowserFrame = false;
  }
}

InProcessTabChildMessageManager::~InProcessTabChildMessageManager()
{
  mAnonymousGlobalScopes.Clear();
  mozilla::DropJSObjects(this);
}

// This method isn't automatically forwarded safely because it's notxpcom, so
// the IDL binding doesn't know what value to return.
void
InProcessTabChildMessageManager::MarkForCC()
{
  MarkScopesForCC();
  MessageManagerGlobal::MarkForCC();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(InProcessTabChildMessageManager)


NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(InProcessTabChildMessageManager,
                                                  DOMEventTargetHelper)
   NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageManager)
   NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocShell)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(InProcessTabChildMessageManager,
                                               DOMEventTargetHelper)
  tmp->nsMessageManagerScriptExecutor::Trace(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(InProcessTabChildMessageManager,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocShell)
  tmp->nsMessageManagerScriptExecutor::Unlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(InProcessTabChildMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsIInProcessContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(InProcessTabChildMessageManager, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(InProcessTabChildMessageManager, DOMEventTargetHelper)

JSObject*
InProcessTabChildMessageManager::WrapObject(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto)
{
  return ContentFrameMessageManager_Binding::Wrap(aCx, this, aGivenProto);
}

void
InProcessTabChildMessageManager::CacheFrameLoader(nsFrameLoader* aFrameLoader)
{
  mFrameLoader = aFrameLoader;
}

already_AddRefed<nsPIDOMWindowOuter>
InProcessTabChildMessageManager::GetContent(ErrorResult& aError)
{
  nsCOMPtr<nsPIDOMWindowOuter> content;
  if (mDocShell) {
    content = mDocShell->GetWindow();
  }
  return content.forget();
}

already_AddRefed<nsIEventTarget>
InProcessTabChildMessageManager::GetTabEventTarget()
{
  nsCOMPtr<nsIEventTarget> target = GetMainThreadEventTarget();
  return target.forget();
}

uint64_t
InProcessTabChildMessageManager::ChromeOuterWindowID()
{
  if (!mDocShell) {
    return 0;
  }

  nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(mDocShell);
  nsCOMPtr<nsIDocShellTreeItem> root;
  nsresult rv = item->GetRootTreeItem(getter_AddRefs(root));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return 0;
  }

  nsPIDOMWindowOuter* topWin = root->GetWindow();
  if (!topWin) {
    return 0;
  }

  return topWin->WindowID();
}

void
InProcessTabChildMessageManager::FireUnloadEvent()
{
  // We're called from nsDocument::MaybeInitializeFinalizeFrameLoaders, so it
  // should be safe to run script.
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript());

  // Don't let the unload event propagate to chrome event handlers.
  mPreventEventsEscaping = true;
  DOMEventTargetHelper::DispatchTrustedEvent(NS_LITERAL_STRING("unload"));

  // Allow events fired during docshell destruction (pagehide, unload) to
  // propagate to the <browser> element since chrome code depends on this.
  mPreventEventsEscaping = false;
}

void
InProcessTabChildMessageManager::DisconnectEventListeners()
{
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

void
InProcessTabChildMessageManager::Disconnect()
{
  mChromeMessageManager = nullptr;
  mOwner = nullptr;
  if (mMessageManager) {
    static_cast<nsFrameMessageManager*>(mMessageManager.get())->Disconnect();
    mMessageManager = nullptr;
  }
}

NS_IMETHODIMP_(nsIContent *)
InProcessTabChildMessageManager::GetOwnerContent()
{
  return mOwner;
}

void
InProcessTabChildMessageManager::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  aVisitor.mForceContentDispatch = true;
  aVisitor.mCanHandle = true;

#ifdef DEBUG
  if (mOwner) {
    nsCOMPtr<nsIFrameLoaderOwner> owner = do_QueryInterface(mOwner);
    RefPtr<nsFrameLoader> fl = owner->GetFrameLoader();
    if (fl) {
      NS_ASSERTION(this == fl->GetTabChildMessageManager(),
                   "Wrong event target!");
      NS_ASSERTION(fl->mMessageManager == mChromeMessageManager,
                   "Wrong message manager!");
    }
  }
#endif

  if (mPreventEventsEscaping) {
    aVisitor.SetParentTarget(nullptr, false);
    return;
  }

  if (mIsBrowserFrame &&
      (!mOwner || !nsContentUtils::IsInChromeDocshell(mOwner->OwnerDoc()))) {
    if (mOwner) {
      if (nsPIDOMWindowInner* innerWindow = mOwner->OwnerDoc()->GetInnerWindow()) {
        // 'this' is already a "chrome handler", so we consider window's
        // parent target to be part of that same part of the event path.
        aVisitor.SetParentTarget(innerWindow->GetParentTarget(), false);
      }
    }
  } else {
    aVisitor.SetParentTarget(mOwner, false);
  }
}

class nsAsyncScriptLoad : public Runnable
{
public:
  nsAsyncScriptLoad(InProcessTabChildMessageManager* aTabChild,
                    const nsAString& aURL,
                    bool aRunInGlobalScope)
    : mozilla::Runnable("nsAsyncScriptLoad")
    , mTabChild(aTabChild)
    , mURL(aURL)
    , mRunInGlobalScope(aRunInGlobalScope)
  {
  }

  NS_IMETHOD Run() override
  {
    mTabChild->LoadFrameScript(mURL, mRunInGlobalScope);
    return NS_OK;
  }
  RefPtr<InProcessTabChildMessageManager> mTabChild;
  nsString mURL;
  bool mRunInGlobalScope;
};

void
InProcessTabChildMessageManager::LoadFrameScript(const nsAString& aURL, bool aRunInGlobalScope)
{
  if (!nsContentUtils::IsSafeToRunScript()) {
    nsContentUtils::AddScriptRunner(new nsAsyncScriptLoad(this, aURL, aRunInGlobalScope));
    return;
  }
  bool tmp = mLoadingScript;
  mLoadingScript = true;
  JS::Rooted<JSObject*> mm(mozilla::dom::RootingCx(), GetOrCreateWrapper());
  LoadScriptInternal(mm, aURL, !aRunInGlobalScope);
  mLoadingScript = tmp;
}

already_AddRefed<nsFrameLoader>
InProcessTabChildMessageManager::GetFrameLoader()
{
  nsCOMPtr<nsIFrameLoaderOwner> owner = do_QueryInterface(mOwner);
  RefPtr<nsFrameLoader> fl = owner ? owner->GetFrameLoader() : nullptr;
  if (!fl) {
    fl = mFrameLoader;
  }
  return fl.forget();
}
