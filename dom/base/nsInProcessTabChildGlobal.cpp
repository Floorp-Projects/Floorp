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
                         nsInProcessTabChildGlobal* aTabChild)
    : nsSameProcessAsyncMessageBase(aRootingCx, aCpows)
    , mTabChild(aTabChild)
  { }

  virtual nsresult HandleMessage() override
  {
    RefPtr<nsFrameLoader> fl = mTabChild->GetFrameLoader();
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

nsInProcessTabChildGlobal::~nsInProcessTabChildGlobal()
{
  mAnonymousGlobalScopes.Clear();
  mozilla::DropJSObjects(this);
}

// This method isn't automatically forwarded safely because it's notxpcom, so
// the IDL binding doesn't know what value to return.
void
nsInProcessTabChildGlobal::MarkForCC()
{
  MarkScopesForCC();
  MessageManagerGlobal::MarkForCC();
}

bool
nsInProcessTabChildGlobal::Init()
{
  // If you change this, please change GetCompartmentName() in XPCJSContext.cpp
  // accordingly.
  nsAutoCString id;
  id.AssignLiteral("inProcessTabChildGlobal");
  nsIURI* uri = mOwner->OwnerDoc()->GetDocumentURI();
  if (uri) {
    nsAutoCString u;
    nsresult rv = uri->GetSpec(u);
    NS_ENSURE_SUCCESS(rv, false);
    id.AppendLiteral("?ownedBy=");
    id.Append(u);
  }
  return InitChildGlobalInternal(id);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsInProcessTabChildGlobal)


NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsInProcessTabChildGlobal,
                                                  DOMEventTargetHelper)
   NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageManager)
   NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocShell)
   tmp->TraverseHostObjectURIs(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(nsInProcessTabChildGlobal,
                                               DOMEventTargetHelper)
  tmp->nsMessageManagerScriptExecutor::Trace(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsInProcessTabChildGlobal,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocShell)
  tmp->nsMessageManagerScriptExecutor::Unlink();
  tmp->UnlinkHostObjectURIs();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsInProcessTabChildGlobal)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsIContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIInProcessContentFrameMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsInProcessTabChildGlobal, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsInProcessTabChildGlobal, DOMEventTargetHelper)

bool
nsInProcessTabChildGlobal::WrapGlobalObject(JSContext* aCx,
                                            JS::RealmOptions& aOptions,
                                            JS::MutableHandle<JSObject*> aReflector)
{
  bool ok = ContentFrameMessageManager_Binding::Wrap(aCx, this, this, aOptions,
                                                       nsJSPrincipals::get(mPrincipal),
                                                       true, aReflector);
  if (ok) {
    // Since we can't rewrap we have to preserve the global's wrapper here.
    PreserveWrapper(ToSupports(this));
  }
  return ok;
}

void
nsInProcessTabChildGlobal::CacheFrameLoader(nsFrameLoader* aFrameLoader)
{
  mFrameLoader = aFrameLoader;
}

already_AddRefed<nsPIDOMWindowOuter>
nsInProcessTabChildGlobal::GetContent(ErrorResult& aError)
{
  nsCOMPtr<nsPIDOMWindowOuter> content;
  if (mDocShell) {
    content = mDocShell->GetWindow();
  }
  return content.forget();
}

already_AddRefed<nsIEventTarget>
nsInProcessTabChildGlobal::GetTabEventTarget()
{
  nsCOMPtr<nsIEventTarget> target = GetMainThreadEventTarget();
  return target.forget();
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

void
nsInProcessTabChildGlobal::GetEventTargetParent(EventChainPreVisitor& aVisitor)
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
  nsAsyncScriptLoad(nsInProcessTabChildGlobal* aTabChild,
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
  bool tmp = mLoadingScript;
  mLoadingScript = true;
  JS::Rooted<JSObject*> global(mozilla::dom::RootingCx(), GetWrapper());
  LoadScriptInternal(global, aURL, aRunInGlobalScope);
  mLoadingScript = tmp;
}

already_AddRefed<nsFrameLoader>
nsInProcessTabChildGlobal::GetFrameLoader()
{
  nsCOMPtr<nsIFrameLoaderOwner> owner = do_QueryInterface(mOwner);
  RefPtr<nsFrameLoader> fl = owner ? owner->GetFrameLoader() : nullptr;
  if (!fl) {
    fl = mFrameLoader;
  }
  return fl.forget();
}
