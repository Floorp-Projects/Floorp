/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsInProcessTabChildGlobal.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsEventDispatcher.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIJSRuntimeService.h"
#include "nsComponentManagerUtils.h"
#include "nsNetUtil.h"
#include "nsScriptLoader.h"
#include "nsFrameLoader.h"
#include "xpcpublic.h"
#include "nsIMozBrowserFrame.h"
#include "nsDOMClassInfoID.h"
#include "mozilla/dom/StructuredCloneUtils.h"

using mozilla::dom::StructuredCloneData;
using mozilla::dom::StructuredCloneClosure;

bool
nsInProcessTabChildGlobal::DoSendSyncMessage(const nsAString& aMessage,
                                             const StructuredCloneData& aData,
                                             InfallibleTArray<nsString>* aJSONRetVal)
{
  nsTArray<nsCOMPtr<nsIRunnable> > asyncMessages;
  asyncMessages.SwapElements(mASyncMessages);
  uint32_t len = asyncMessages.Length();
  for (uint32_t i = 0; i < len; ++i) {
    nsCOMPtr<nsIRunnable> async = asyncMessages[i];
    async->Run();
  }
  if (mChromeMessageManager) {
    nsRefPtr<nsFrameMessageManager> mm = mChromeMessageManager;
    mm->ReceiveMessage(mOwner, aMessage, true, &aData, JS::NullPtr(),
                       aJSONRetVal);
  }
  return true;
}

class nsAsyncMessageToParent : public nsRunnable
{
public:
  nsAsyncMessageToParent(nsInProcessTabChildGlobal* aTabChild,
                         const nsAString& aMessage,
                         const StructuredCloneData& aData)
    : mTabChild(aTabChild), mMessage(aMessage), mRun(false)
  {
    if (aData.mDataLength && !mData.copy(aData.mData, aData.mDataLength)) {
      NS_RUNTIMEABORT("OOM");
    }
    mClosure = aData.mClosure;
  }

  NS_IMETHOD Run()
  {
    if (mRun) {
      return NS_OK;
    }

    mRun = true;
    mTabChild->mASyncMessages.RemoveElement(this);
    if (mTabChild->mChromeMessageManager) {
      StructuredCloneData data;
      data.mData = mData.data();
      data.mDataLength = mData.nbytes();
      data.mClosure = mClosure;

      nsRefPtr<nsFrameMessageManager> mm = mTabChild->mChromeMessageManager;
      mm->ReceiveMessage(mTabChild->mOwner, mMessage, false, &data,
                         JS::NullPtr(), nullptr, nullptr);
    }
    return NS_OK;
  }
  nsRefPtr<nsInProcessTabChildGlobal> mTabChild;
  nsString mMessage;
  JSAutoStructuredCloneBuffer mData;
  StructuredCloneClosure mClosure;
  // True if this runnable has already been called. This can happen if DoSendSyncMessage
  // is called while waiting for an asynchronous message send.
  bool mRun;
};

bool
nsInProcessTabChildGlobal::DoSendAsyncMessage(const nsAString& aMessage,
                                              const StructuredCloneData& aData)
{
  nsCOMPtr<nsIRunnable> ev =
    new nsAsyncMessageToParent(this, aMessage, aData);
  mASyncMessages.AppendElement(ev);
  NS_DispatchToCurrentThread(ev);
  return true;
}

nsInProcessTabChildGlobal::nsInProcessTabChildGlobal(nsIDocShell* aShell,
                                                     nsIContent* aOwner,
                                                     nsFrameMessageManager* aChrome)
: mDocShell(aShell), mInitialized(false), mLoadingScript(false),
  mDelayedDisconnect(false), mOwner(aOwner), mChromeMessageManager(aChrome)
{

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
  NS_ASSERTION(!mCx, "Couldn't release JSContext?!?");
}

/* [notxpcom] boolean markForCC (); */
// This method isn't automatically forwarded safely because it's notxpcom, so
// the IDL binding doesn't know what value to return.
NS_IMETHODIMP_(bool)
nsInProcessTabChildGlobal::MarkForCC()
{
  return mMessageManager ? mMessageManager->MarkForCC() : false;
}

nsresult
nsInProcessTabChildGlobal::Init()
{
#ifdef DEBUG
  nsresult rv =
#endif
  InitTabChildGlobal();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv),
                   "Couldn't initialize nsInProcessTabChildGlobal");
  mMessageManager = new nsFrameMessageManager(this,
                                              nullptr,
                                              mCx,
                                              mozilla::dom::ipc::MM_CHILD);
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsInProcessTabChildGlobal,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  nsFrameScriptExecutor::Unlink(tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsInProcessTabChildGlobal,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageManager)
  nsFrameScriptExecutor::Traverse(tmp, cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

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
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsInProcessTabChildGlobal, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsInProcessTabChildGlobal, nsDOMEventTargetHelper)

NS_IMETHODIMP
nsInProcessTabChildGlobal::GetContent(nsIDOMWindow** aContent)
{
  *aContent = nullptr;
  nsCOMPtr<nsIDOMWindow> window = do_GetInterface(mDocShell);
  window.swap(*aContent);
  return NS_OK;
}

NS_IMETHODIMP
nsInProcessTabChildGlobal::GetDocShell(nsIDocShell** aDocShell)
{
  NS_IF_ADDREF(*aDocShell = mDocShell);
  return NS_OK;
}

NS_IMETHODIMP
nsInProcessTabChildGlobal::Btoa(const nsAString& aBinaryData,
                            nsAString& aAsciiBase64String)
{
  return nsContentUtils::Btoa(aBinaryData, aAsciiBase64String);
}

NS_IMETHODIMP
nsInProcessTabChildGlobal::Atob(const nsAString& aAsciiString,
                            nsAString& aBinaryData)
{
  return nsContentUtils::Atob(aAsciiString, aBinaryData);
}


NS_IMETHODIMP
nsInProcessTabChildGlobal::PrivateNoteIntentionalCrash()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsInProcessTabChildGlobal::Disconnect()
{
  // Let the frame scripts know the child is being closed. We do any other
  // cleanup after the event has been fired. See DelayedDisconnect
  nsContentUtils::AddScriptRunner(
     NS_NewRunnableMethod(this, &nsInProcessTabChildGlobal::DelayedDisconnect)
  );
}

void
nsInProcessTabChildGlobal::DelayedDisconnect()
{
  // Don't let the event escape
  mOwner = nullptr;

  // Fire the "unload" event
  nsDOMEventTargetHelper::DispatchTrustedEvent(NS_LITERAL_STRING("unload"));

  // Continue with the Disconnect cleanup
  nsCOMPtr<nsIDOMWindow> win = do_GetInterface(mDocShell);
  nsCOMPtr<nsPIDOMWindow> pwin = do_QueryInterface(win);
  if (pwin) {
    pwin->SetChromeEventHandler(pwin->GetChromeEventHandler());
  }
  mDocShell = nullptr;
  mChromeMessageManager = nullptr;
  if (mMessageManager) {
    static_cast<nsFrameMessageManager*>(mMessageManager.get())->Disconnect();
    mMessageManager = nullptr;
  }
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }

  if (!mLoadingScript) {
    nsContentUtils::ReleaseWrapper(static_cast<EventTarget*>(this), this);
    if (mCx) {
      DestroyCx();
    }
  } else {
    mDelayedDisconnect = true;
  }
}

NS_IMETHODIMP_(nsIContent *)
nsInProcessTabChildGlobal::GetOwnerContent()
{
  return mOwner;
}

nsresult
nsInProcessTabChildGlobal::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;

  if (mIsBrowserOrAppFrame &&
      (!mOwner || !nsContentUtils::IsInChromeDocshell(mOwner->OwnerDoc()))) {
    if (mOwner) {
      nsPIDOMWindow* innerWindow = mOwner->OwnerDoc()->GetInnerWindow();
      if (innerWindow) {
        aVisitor.mParentTarget = innerWindow->GetParentTarget();
      }
    }
  } else {
    aVisitor.mParentTarget = mOwner;
  }

#ifdef DEBUG
  if (mOwner) {
    nsCOMPtr<nsIFrameLoaderOwner> owner = do_QueryInterface(mOwner);
    nsRefPtr<nsFrameLoader> fl = owner->GetFrameLoader();
    if (fl) {
      NS_ASSERTION(this == fl->GetTabChildGlobalAsEventTarget(),
                   "Wrong event target!");
      NS_ASSERTION(fl->mMessageManager == mChromeMessageManager,
                   "Wrong message manager!");
    }
  }
#endif

  return NS_OK;
}

nsresult
nsInProcessTabChildGlobal::InitTabChildGlobal()
{
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
  NS_ENSURE_STATE(InitTabChildGlobalInternal(scopeSupports, id));
  return NS_OK;
}

class nsAsyncScriptLoad : public nsRunnable
{
public:
  nsAsyncScriptLoad(nsInProcessTabChildGlobal* aTabChild, const nsAString& aURL)
  : mTabChild(aTabChild), mURL(aURL) {}

  NS_IMETHOD Run()
  {
    mTabChild->LoadFrameScript(mURL);
    return NS_OK;
  }
  nsRefPtr<nsInProcessTabChildGlobal> mTabChild;
  nsString mURL;
};

void
nsInProcessTabChildGlobal::LoadFrameScript(const nsAString& aURL)
{
  if (!nsContentUtils::IsSafeToRunScript()) {
    nsContentUtils::AddScriptRunner(new nsAsyncScriptLoad(this, aURL));
    return;
  }
  if (!mInitialized) {
    mInitialized = true;
    Init();
  }
  bool tmp = mLoadingScript;
  mLoadingScript = true;
  LoadFrameScriptInternal(aURL);
  mLoadingScript = tmp;
  if (!mLoadingScript && mDelayedDisconnect) {
    mDelayedDisconnect = false;
    Disconnect();
  }
}
