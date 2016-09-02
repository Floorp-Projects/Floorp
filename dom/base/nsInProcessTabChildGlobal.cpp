/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsInProcessTabChildGlobal.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsComponentManagerUtils.h"
#include "nsScriptLoader.h"
#include "nsFrameLoader.h"
#include "xpcpublic.h"
#include "nsIMozBrowserFrame.h"
#include "nsDOMClassInfoID.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/SameProcessMessageQueue.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;

bool
nsInProcessTabChildGlobal::DoSendBlockingMessage(JSContext* aCx,
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
    nsCOMPtr<nsIFrameLoader> fl = GetFrameLoader();
    mm->ReceiveMessage(mOwner, fl, aMessage, true, &aData, &cpows, aPrincipal,
                       aRetVal);
  }
  return true;
}

class nsAsyncMessageToParent : public nsSameProcessAsyncMessageBase,
                               public SameProcessMessageQueue::Runnable
{
public:
  nsAsyncMessageToParent(JS::RootingContext* aRootingCx,
                         JS::Handle<JSObject*> aCpows,
                         nsInProcessTabChildGlobal* aTabChild)
    : nsSameProcessAsyncMessageBase(aRootingCx, aCpows)
    , mTabChild(aTabChild)
  { }

  virtual nsresult HandleMessage() override
  {
    nsCOMPtr<nsIFrameLoader> fl = mTabChild->GetFrameLoader();
    ReceiveMessage(mTabChild->mOwner, fl, mTabChild->mChromeMessageManager);
    return NS_OK;
  }
  RefPtr<nsInProcessTabChildGlobal> mTabChild;
};

nsresult
nsInProcessTabChildGlobal::DoSendAsyncMessage(JSContext* aCx,
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

nsInProcessTabChildGlobal::nsInProcessTabChildGlobal(nsIDocShell* aShell,
                                                     nsIContent* aOwner,
                                                     nsFrameMessageManager* aChrome)
: mDocShell(aShell), mInitialized(false), mLoadingScript(false),
  mPreventEventsEscaping(false),
  mOwner(aOwner), mChromeMessageManager(aChrome)
{
  SetIsNotDOMBinding();
  mozilla::HoldJSObjects(this);

  // If owner corresponds to an <iframe mozbrowser> or <iframe mozapp>, we'll
  // have to tweak our PreHandleEvent implementation.
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwner);
  if (browserFrame) {
    mIsBrowserOrAppFrame = browserFrame->GetReallyIsBrowserOrApp();
  }
  else {
    mIsBrowserOrAppFrame = false;
  }
}

nsInProcessTabChildGlobal::~nsInProcessTabChildGlobal()
{
  mAnonymousGlobalScopes.Clear();
  mozilla::DropJSObjects(this);
}

// This method isn't automatically forwarded safely because it's notxpcom, so
// the IDL binding doesn't know what value to return.
NS_IMETHODIMP_(bool)
nsInProcessTabChildGlobal::MarkForCC()
{
  MarkScopesForCC();
  return mMessageManager ? mMessageManager->MarkForCC() : false;
}

nsresult
nsInProcessTabChildGlobal::Init()
{
#ifdef DEBUG
  nsresult rv =
#endif
  InitTabChildGlobal();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv),
                       "Couldn't initialize nsInProcessTabChildGlobal");
  mMessageManager = new nsFrameMessageManager(this,
                                              nullptr,
                                              dom::ipc::MM_CHILD);
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsInProcessTabChildGlobal)


NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsInProcessTabChildGlobal,
                                                  DOMEventTargetHelper)
   NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageManager)
   NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
   tmp->TraverseHostObjectURIs(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(nsInProcessTabChildGlobal,
                                               DOMEventTargetHelper)
  tmp->nsMessageManagerScriptExecutor::Trace(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsInProcessTabChildGlobal,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAnonymousGlobalScopes)
   tmp->UnlinkHostObjectURIs();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsInProcessTabChildGlobal)
  NS_INTERFACE_MAP_ENTRY(nsIMessageListenerManager)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsISyncMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsIContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIInProcessContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ContentFrameMessageManager)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsInProcessTabChildGlobal, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsInProcessTabChildGlobal, DOMEventTargetHelper)

void
nsInProcessTabChildGlobal::CacheFrameLoader(nsIFrameLoader* aFrameLoader)
{
  mFrameLoader = aFrameLoader;
}

NS_IMETHODIMP
nsInProcessTabChildGlobal::GetContent(mozIDOMWindowProxy** aContent)
{
  *aContent = nullptr;
  if (!mDocShell) {
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = mDocShell->GetWindow();
  window.forget(aContent);
  return NS_OK;
}

NS_IMETHODIMP
nsInProcessTabChildGlobal::GetDocShell(nsIDocShell** aDocShell)
{
  NS_IF_ADDREF(*aDocShell = mDocShell);
  return NS_OK;
}

void
nsInProcessTabChildGlobal::FireUnloadEvent()
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
nsInProcessTabChildGlobal::DisconnectEventListeners()
{
  if (mDocShell) {
    if (nsCOMPtr<nsPIDOMWindowOuter> win = mDocShell->GetWindow()) {
      MOZ_ASSERT(win->IsOuterWindow());
      win->SetChromeEventHandler(win->GetChromeEventHandler());
    }
  }
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }

  mDocShell = nullptr;
}

void
nsInProcessTabChildGlobal::Disconnect()
{
  mChromeMessageManager = nullptr;
  mOwner = nullptr;
  if (mMessageManager) {
    static_cast<nsFrameMessageManager*>(mMessageManager.get())->Disconnect();
    mMessageManager = nullptr;
  }
}

NS_IMETHODIMP_(nsIContent *)
nsInProcessTabChildGlobal::GetOwnerContent()
{
  return mOwner;
}

nsresult
nsInProcessTabChildGlobal::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  aVisitor.mForceContentDispatch = true;
  aVisitor.mCanHandle = true;

#ifdef DEBUG
  if (mOwner) {
    nsCOMPtr<nsIFrameLoaderOwner> owner = do_QueryInterface(mOwner);
    RefPtr<nsFrameLoader> fl = owner->GetFrameLoader();
    if (fl) {
      NS_ASSERTION(this == fl->GetTabChildGlobalAsEventTarget(),
                   "Wrong event target!");
      NS_ASSERTION(fl->mMessageManager == mChromeMessageManager,
                   "Wrong message manager!");
    }
  }
#endif

  if (mPreventEventsEscaping) {
    aVisitor.mParentTarget = nullptr;
    return NS_OK;
  }

  if (mIsBrowserOrAppFrame &&
      (!mOwner || !nsContentUtils::IsInChromeDocshell(mOwner->OwnerDoc()))) {
    if (mOwner) {
      if (nsPIDOMWindowInner* innerWindow = mOwner->OwnerDoc()->GetInnerWindow()) {
        aVisitor.mParentTarget = innerWindow->GetParentTarget();
      }
    }
  } else {
    aVisitor.mParentTarget = mOwner;
  }

  return NS_OK;
}

nsresult
nsInProcessTabChildGlobal::InitTabChildGlobal()
{
  // If you change this, please change GetCompartmentName() in XPCJSRuntime.cpp
  // accordingly.
  nsAutoCString id;
  id.AssignLiteral("inProcessTabChildGlobal");
  nsIURI* uri = mOwner->OwnerDoc()->GetDocumentURI();
  if (uri) {
    nsAutoCString u;
    uri->GetSpec(u);
    id.AppendLiteral("?ownedBy=");
    id.Append(u);
  }
  nsISupports* scopeSupports = NS_ISUPPORTS_CAST(EventTarget*, this);
  NS_ENSURE_STATE(InitChildGlobalInternal(scopeSupports, id));
  return NS_OK;
}

class nsAsyncScriptLoad : public Runnable
{
public:
    nsAsyncScriptLoad(nsInProcessTabChildGlobal* aTabChild, const nsAString& aURL,
                      bool aRunInGlobalScope)
      : mTabChild(aTabChild), mURL(aURL), mRunInGlobalScope(aRunInGlobalScope) {}

  NS_IMETHOD Run() override
  {
    mTabChild->LoadFrameScript(mURL, mRunInGlobalScope);
    return NS_OK;
  }
  RefPtr<nsInProcessTabChildGlobal> mTabChild;
  nsString mURL;
  bool mRunInGlobalScope;
};

void
nsInProcessTabChildGlobal::LoadFrameScript(const nsAString& aURL, bool aRunInGlobalScope)
{
  if (!nsContentUtils::IsSafeToRunScript()) {
    nsContentUtils::AddScriptRunner(new nsAsyncScriptLoad(this, aURL, aRunInGlobalScope));
    return;
  }
  if (!mInitialized) {
    mInitialized = true;
    Init();
  }
  bool tmp = mLoadingScript;
  mLoadingScript = true;
  LoadScriptInternal(aURL, aRunInGlobalScope);
  mLoadingScript = tmp;
}

already_AddRefed<nsIFrameLoader>
nsInProcessTabChildGlobal::GetFrameLoader()
{
  nsCOMPtr<nsIFrameLoaderOwner> owner = do_QueryInterface(mOwner);
  nsCOMPtr<nsIFrameLoader> fl = owner ? owner->GetFrameLoader() : nullptr;
  if (!fl) {
    fl = mFrameLoader;
  }
  return fl.forget();
}
