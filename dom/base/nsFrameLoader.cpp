/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Class for managing loading of a subframe (creation of the docshell,
 * handling of loads in it, recursion-checking).
 */

#include "base/basictypes.h"

#include "prenv.h"

#include "nsDocShell.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLFrameElement.h"
#include "nsIDOMMozBrowserFrame.h"
#include "nsIDOMWindow.h"
#include "nsIPresShell.h"
#include "nsIContentInlines.h"
#include "nsIContentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsPIDOMWindow.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIBaseWindow.h"
#include "nsIBrowser.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "nsUnicharUtils.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScrollable.h"
#include "nsFrameLoader.h"
#include "nsIDOMEventTarget.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsSubDocumentFrame.h"
#include "nsError.h"
#include "nsISHistory.h"
#include "nsISHistoryInternal.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIXULWindow.h"
#include "nsIEditor.h"
#include "nsIMozBrowserFrame.h"
#include "nsISHistory.h"
#include "NullPrincipal.h"
#include "nsIScriptError.h"
#include "nsGlobalWindow.h"
#include "nsPIWindowRoot.h"
#include "nsLayoutUtils.h"
#include "nsMappedAttributes.h"
#include "nsView.h"
#include "GroupedSHistory.h"
#include "PartialSHistory.h"

#include "nsIURI.h"
#include "nsIURL.h"
#include "nsNetUtil.h"

#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"

#include "nsThreadUtils.h"

#include "nsIDOMChromeWindow.h"
#include "nsInProcessTabChildGlobal.h"

#include "Layers.h"
#include "ClientLayerManager.h"

#include "ContentParent.h"
#include "TabParent.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Element.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "mozilla/layout/RenderFrameParent.h"
#include "GeckoProfiler.h"

#include "jsapi.h"
#include "mozilla/dom/HTMLIFrameElement.h"
#include "nsSandboxFlags.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/dom/CustomEvent.h"

#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/WebBrowserPersistLocalDocument.h"
#include "mozilla/dom/GroupedHistoryEvent.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"

#include "ContentPrincipal.h"

#ifdef XP_WIN
#include "mozilla/plugins/PPluginWidgetParent.h"
#include "../plugins/ipc/PluginWidgetParent.h"
#endif

#ifdef MOZ_XUL
#include "nsXULPopupManager.h"
#endif

#ifdef NS_PRINTING
#include "mozilla/embedding/printingui/PrintingParent.h"
#include "nsIWebBrowserPrint.h"
#endif

using namespace mozilla;
using namespace mozilla::hal;
using namespace mozilla::dom;
using namespace mozilla::dom::ipc;
using namespace mozilla::layers;
using namespace mozilla::layout;
typedef FrameMetrics::ViewID ViewID;

// Bug 136580: Limit to the number of nested content frames that can have the
//             same URL. This is to stop content that is recursively loading
//             itself.  Note that "#foo" on the end of URL doesn't affect
//             whether it's considered identical, but "?foo" or ";foo" are
//             considered and compared.
// Bug 228829: Limit this to 1, like IE does.
#define MAX_SAME_URL_CONTENT_FRAMES 1

// Bug 8065: Limit content frame depth to some reasonable level. This
// does not count chrome frames when determining depth, nor does it
// prevent chrome recursion.  Number is fairly arbitrary, but meant to
// keep number of shells to a reasonable number on accidental recursion with a
// small (but not 1) branching factor.  With large branching factors the number
// of shells can rapidly become huge and run us out of memory.  To solve that,
// we'd need to re-institute a fixed version of bug 98158.
#define MAX_DEPTH_CONTENT_FRAMES 10

NS_IMPL_CYCLE_COLLECTION(nsFrameLoader,
                         mDocShell,
                         mMessageManager,
                         mChildMessageManager,
                         mOpener,
                         mPartialSHistory)
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsFrameLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsFrameLoader)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsFrameLoader)
  NS_INTERFACE_MAP_ENTRY(nsIFrameLoader)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIFrameLoader)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserPersistable)
NS_INTERFACE_MAP_END

nsFrameLoader::nsFrameLoader(Element* aOwner, nsPIDOMWindowOuter* aOpener, bool aNetworkCreated)
  : mOwnerContent(aOwner)
  , mDetachedSubdocFrame(nullptr)
  , mOpener(aOpener)
  , mRemoteBrowser(nullptr)
  , mChildID(0)
  , mEventMode(EVENT_MODE_NORMAL_DISPATCH)
  , mBrowserChangingProcessBlockers(nullptr)
  , mIsPrerendered(false)
  , mDepthTooGreat(false)
  , mIsTopLevelContent(false)
  , mDestroyCalled(false)
  , mNeedsAsyncDestroy(false)
  , mInSwap(false)
  , mInShow(false)
  , mHideCalled(false)
  , mNetworkCreated(aNetworkCreated)
  , mRemoteBrowserShown(false)
  , mRemoteFrame(false)
  , mClipSubdocument(true)
  , mClampScrollPosition(true)
  , mObservingOwnerContent(false)
  , mVisible(true)
{
  mRemoteFrame = ShouldUseRemoteProcess();
  MOZ_ASSERT(!mRemoteFrame || !aOpener,
             "Cannot pass aOpener for a remote frame!");
}

nsFrameLoader::~nsFrameLoader()
{
  if (mMessageManager) {
    mMessageManager->Disconnect();
  }
  MOZ_RELEASE_ASSERT(mDestroyCalled);
}

nsFrameLoader*
nsFrameLoader::Create(Element* aOwner, nsPIDOMWindowOuter* aOpener, bool aNetworkCreated)
{
  NS_ENSURE_TRUE(aOwner, nullptr);
  nsIDocument* doc = aOwner->OwnerDoc();

  // We never create nsFrameLoaders for elements in resource documents.
  //
  // We never create nsFrameLoaders for elements in data documents, unless the
  // document is a static document.
  // Static documents are an exception because any sub-documents need an
  // nsFrameLoader to keep the relevant docShell alive, even though the
  // nsFrameLoader isn't used to load anything (the sub-document is created by
  // the static clone process).
  //
  // We never create nsFrameLoaders for elements that are not
  // in-composed-document, unless the element belongs to a static document.
  // Static documents are an exception because this method is called at a point
  // in the static clone process before aOwner has been inserted into its
  // document.  For other types of documents this wouldn't be a problem since
  // we'd create the nsFrameLoader as necessary after aOwner is inserted into a
  // document, but the mechanisms that take care of that don't apply for static
  // documents so we need to create the nsFrameLoader now. (This isn't wasteful
  // since for a static document we know aOwner will end up in a document and
  // the nsFrameLoader will be used for its docShell.)
  //
  NS_ENSURE_TRUE(!doc->IsResourceDoc() &&
                 ((!doc->IsLoadedAsData() && aOwner->IsInComposedDoc()) ||
                  doc->IsStaticDocument()),
                 nullptr);

  return new nsFrameLoader(aOwner, aOpener, aNetworkCreated);
}

NS_IMETHODIMP
nsFrameLoader::LoadFrame()
{
  NS_ENSURE_TRUE(mOwnerContent, NS_ERROR_NOT_INITIALIZED);

  nsAutoString src;

  bool isSrcdoc = mOwnerContent->IsHTMLElement(nsGkAtoms::iframe) &&
                  mOwnerContent->HasAttr(kNameSpaceID_None, nsGkAtoms::srcdoc);
  if (isSrcdoc) {
    src.AssignLiteral("about:srcdoc");
  }
  else {
    GetURL(src);

    src.Trim(" \t\n\r");

    if (src.IsEmpty()) {
      // If the frame is a XUL element and has the attribute 'nodefaultsrc=true'
      // then we will not use 'about:blank' as fallback but return early without
      // starting a load if no 'src' attribute is given (or it's empty).
      if (mOwnerContent->IsXULElement() &&
          mOwnerContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::nodefaultsrc,
                                     nsGkAtoms::_true, eCaseMatters)) {
        return NS_OK;
      }
      src.AssignLiteral("about:blank");
    }
  }

  nsIDocument* doc = mOwnerContent->OwnerDoc();
  if (doc->IsStaticDocument()) {
    return NS_OK;
  }

  if (doc->IsLoadedAsInteractiveData()) {
    // XBL bindings doc shouldn't load sub-documents.
    return NS_OK;
  }

  nsCOMPtr<nsIURI> base_uri = mOwnerContent->GetBaseURI();
  const nsAFlatCString &doc_charset = doc->GetDocumentCharacterSet();
  const char *charset = doc_charset.IsEmpty() ? nullptr : doc_charset.get();

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), src, charset, base_uri);

  // If the URI was malformed, try to recover by loading about:blank.
  if (rv == NS_ERROR_MALFORMED_URI) {
    rv = NS_NewURI(getter_AddRefs(uri), NS_LITERAL_STRING("about:blank"),
                   charset, base_uri);
  }

  if (NS_SUCCEEDED(rv)) {
    rv = LoadURI(uri);
  }

  if (NS_FAILED(rv)) {
    FireErrorEvent();

    return rv;
  }

  return NS_OK;
}

void
nsFrameLoader::FireErrorEvent()
{
  if (!mOwnerContent) {
    return;
  }
  RefPtr<AsyncEventDispatcher > loadBlockingAsyncDispatcher =
    new LoadBlockingAsyncEventDispatcher(mOwnerContent,
                                         NS_LITERAL_STRING("error"),
                                         false, false);
  loadBlockingAsyncDispatcher->PostDOMEvent();
}

NS_IMETHODIMP
nsFrameLoader::LoadURI(nsIURI* aURI)
{
  if (!aURI)
    return NS_ERROR_INVALID_POINTER;
  NS_ENSURE_STATE(!mDestroyCalled && mOwnerContent);

  nsCOMPtr<nsIDocument> doc = mOwnerContent->OwnerDoc();

  nsresult rv = CheckURILoad(aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  mURIToLoad = aURI;
  rv = doc->InitializeFrameLoader(this);
  if (NS_FAILED(rv)) {
    mURIToLoad = nullptr;
  }
  return rv;
}

NS_IMETHODIMP
nsFrameLoader::SetIsPrerendered()
{
  MOZ_ASSERT(!mDocShell, "Please call SetIsPrerendered before docShell is created");
  mIsPrerendered = true;

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::MakePrerenderedLoaderActive()
{
  MOZ_ASSERT(mIsPrerendered, "This frameloader was not in prerendered mode.");

  mIsPrerendered = false;
  if (IsRemoteFrame()) {
    if (!mRemoteBrowser) {
      NS_WARNING("Missing remote browser.");
      return NS_ERROR_FAILURE;
    }

    mRemoteBrowser->SetDocShellIsActive(true);
  } else {
    if (!mDocShell) {
      NS_WARNING("Missing docshell.");
      return NS_ERROR_FAILURE;
    }

    nsresult rv = mDocShell->SetIsActive(true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetPartialSHistory(nsIPartialSHistory** aResult)
{
  if (mRemoteBrowser && !mPartialSHistory) {
    // For remote case we can lazy initialize PartialSHistory since
    // it doens't need to be registered as a listener to nsISHistory directly.
    mPartialSHistory = new PartialSHistory(this);
  }

  nsCOMPtr<nsIPartialSHistory> partialHistory(mPartialSHistory);
  partialHistory.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::EnsureGroupedSHistory(nsIGroupedSHistory** aResult)
{
  nsCOMPtr<nsIPartialSHistory> partialHistory;
  GetPartialSHistory(getter_AddRefs(partialHistory));
  MOZ_ASSERT(partialHistory);

  nsCOMPtr<nsIGroupedSHistory> groupedHistory;
  partialHistory->GetGroupedSHistory(getter_AddRefs(groupedHistory));
  if (!groupedHistory) {
    groupedHistory = new GroupedSHistory();
    groupedHistory->AppendPartialSHistory(partialHistory);

#ifdef DEBUG
    nsCOMPtr<nsIGroupedSHistory> test;
    GetGroupedSHistory(getter_AddRefs(test));
    MOZ_ASSERT(test == groupedHistory, "GroupedHistory must match");
#endif
  }

  groupedHistory.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetGroupedSHistory(nsIGroupedSHistory** aResult)
{
  nsCOMPtr<nsIGroupedSHistory> groupedSHistory;
  if (mPartialSHistory) {
    mPartialSHistory->GetGroupedSHistory(getter_AddRefs(groupedSHistory));
  }
  groupedSHistory.forget(aResult);
  return NS_OK;
}

bool
nsFrameLoader::SwapBrowsersAndNotify(nsFrameLoader* aOther)
{
  // Cache the owner content before calling SwapBrowsers, which will change
  // these member variables.
  RefPtr<mozilla::dom::Element> primaryContent = mOwnerContent;
  RefPtr<mozilla::dom::Element> secondaryContent = aOther->mOwnerContent;

  // Swap loaders through our owner, so the owner's listeners will be correctly
  // setup.
  nsCOMPtr<nsIBrowser> ourBrowser = do_QueryInterface(primaryContent);
  nsCOMPtr<nsIBrowser> otherBrowser = do_QueryInterface(secondaryContent);
  if (NS_WARN_IF(!ourBrowser || !otherBrowser)) {
    return false;
  }
  nsresult rv = ourBrowser->SwapBrowsers(otherBrowser, nsIBrowser::SWAP_KEEP_PERMANENT_KEY);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  // Dispatch the BrowserChangedProcess event to tell JS that the process swap
  // has occurred.
  GroupedHistoryEventInit eventInit;
  eventInit.mBubbles = true;
  eventInit.mCancelable= false;
  eventInit.mOtherBrowser = secondaryContent;
  RefPtr<GroupedHistoryEvent> event =
    GroupedHistoryEvent::Constructor(primaryContent,
                                     NS_LITERAL_STRING("BrowserChangedProcess"),
                                     eventInit);
  event->SetTrusted(true);
  bool dummy;
  primaryContent->DispatchEvent(event, &dummy);

  return true;
}

class AppendPartialSHistoryAndSwapHelper : public PromiseNativeHandler
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(AppendPartialSHistoryAndSwapHelper)

  AppendPartialSHistoryAndSwapHelper(nsFrameLoader* aThis,
                                     nsFrameLoader* aOther,
                                     Promise* aPromise)
    : mThis(aThis), mOther(aOther), mPromise(aPromise) {}

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    nsCOMPtr<nsIGroupedSHistory> otherGroupedHistory;
    mOther->GetGroupedSHistory(getter_AddRefs(otherGroupedHistory));
    MOZ_ASSERT(!otherGroupedHistory,
               "Cannot append a GroupedSHistory owner to another.");
    if (otherGroupedHistory) {
      mPromise->MaybeRejectWithUndefined();
      return;
    }

    // Append ourselves.
    nsresult rv;
    nsCOMPtr<nsIGroupedSHistory> groupedSHistory;
    rv = mThis->EnsureGroupedSHistory(getter_AddRefs(groupedSHistory));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeRejectWithUndefined();
      return;
    }

    // Append the other.
    nsCOMPtr<nsIPartialSHistory> otherPartialSHistory;
    MOZ_ALWAYS_SUCCEEDS(mOther->GetPartialSHistory(getter_AddRefs(otherPartialSHistory)));
    rv = groupedSHistory->AppendPartialSHistory(otherPartialSHistory);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeRejectWithUndefined();
      return;
    }

    // Swap the browsers and fire the BrowserChangedProcess event.
    if (mThis->SwapBrowsersAndNotify(mOther)) {
      mPromise->MaybeResolveWithUndefined();
    } else {
      mPromise->MaybeRejectWithUndefined();
    }
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    mPromise->MaybeRejectWithUndefined();
  }

private:
  ~AppendPartialSHistoryAndSwapHelper() {}
  RefPtr<nsFrameLoader> mThis;
  RefPtr<nsFrameLoader> mOther;
  RefPtr<Promise> mPromise;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(AppendPartialSHistoryAndSwapHelper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AppendPartialSHistoryAndSwapHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AppendPartialSHistoryAndSwapHelper)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTION(AppendPartialSHistoryAndSwapHelper,
                         mThis, mPromise)

class RequestGroupedHistoryNavigationHelper : public PromiseNativeHandler
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(RequestGroupedHistoryNavigationHelper)

  RequestGroupedHistoryNavigationHelper(nsFrameLoader* aThis,
                                        uint32_t aGlobalIndex,
                                        Promise* aPromise)
    : mThis(aThis), mGlobalIndex(aGlobalIndex), mPromise(aPromise) {}

  void
  ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    if (NS_WARN_IF(!mThis->mOwnerContent)) {
      mPromise->MaybeRejectWithUndefined();
      return;
    }

    nsCOMPtr<nsIGroupedSHistory> groupedSHistory;
    mThis->GetGroupedSHistory(getter_AddRefs(groupedSHistory));
    if (NS_WARN_IF(!groupedSHistory)) {
      mPromise->MaybeRejectWithUndefined();
      return;
    }

    // Navigate the loader to the new index
    nsCOMPtr<nsIFrameLoader> otherLoader;
    nsresult rv = groupedSHistory->GotoIndex(mGlobalIndex, getter_AddRefs(otherLoader));

    // Check if the gotoIndex failed because the target frameloader is dead. We
    // need to perform a navigateAndRestoreByIndex and then return to recover.
    if (rv == NS_ERROR_NOT_AVAILABLE) {
      // Get the nsIXULBrowserWindow so that we can call NavigateAndRestoreByIndex on it.
      nsCOMPtr<nsIDocShell> docShell = mThis->mOwnerContent->OwnerDoc()->GetDocShell();
      if (NS_WARN_IF(!docShell)) {
        mPromise->MaybeRejectWithUndefined();
        return;
      }

      nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
      docShell->GetTreeOwner(getter_AddRefs(treeOwner));
      if (NS_WARN_IF(!treeOwner)) {
        mPromise->MaybeRejectWithUndefined();
        return;
      }

      nsCOMPtr<nsIXULWindow> window = do_GetInterface(treeOwner);
      if (NS_WARN_IF(!window)) {
        mPromise->MaybeRejectWithUndefined();
        return;
      }

      nsCOMPtr<nsIXULBrowserWindow> xbw;
      window->GetXULBrowserWindow(getter_AddRefs(xbw));
      if (NS_WARN_IF(!xbw)) {
        mPromise->MaybeRejectWithUndefined();
        return;
      }

      nsCOMPtr<nsIBrowser> ourBrowser = do_QueryInterface(mThis->mOwnerContent);
      if (NS_WARN_IF(!ourBrowser)) {
        mPromise->MaybeRejectWithUndefined();
        return;
      }

      rv = xbw->NavigateAndRestoreByIndex(ourBrowser, mGlobalIndex);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        mPromise->MaybeRejectWithUndefined();
        return;
      }
      mPromise->MaybeResolveWithUndefined();
      return;
    }

    // Check for any other type of failure
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mPromise->MaybeRejectWithUndefined();
      return;
    }

    // Perform the swap.
    nsFrameLoader* other = static_cast<nsFrameLoader*>(otherLoader.get());
    if (!other || other == mThis) {
      mPromise->MaybeRejectWithUndefined();
      return;
    }

    // Swap the browsers and fire the BrowserChangedProcess event.
    if (mThis->SwapBrowsersAndNotify(other)) {
      mPromise->MaybeResolveWithUndefined();
    } else {
      mPromise->MaybeRejectWithUndefined();
    }
  }

  void
  RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override
  {
    mPromise->MaybeRejectWithUndefined();
  }

private:
  ~RequestGroupedHistoryNavigationHelper() {}
  RefPtr<nsFrameLoader> mThis;
  uint32_t mGlobalIndex;
  RefPtr<Promise> mPromise;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(RequestGroupedHistoryNavigationHelper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(RequestGroupedHistoryNavigationHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RequestGroupedHistoryNavigationHelper)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTION(RequestGroupedHistoryNavigationHelper,
                         mThis, mPromise)

already_AddRefed<Promise>
nsFrameLoader::FireWillChangeProcessEvent()
{
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mOwnerContent->GetOwnerGlobal()))) {
    return nullptr;
  }
  JSContext* cx = jsapi.cx();
  GlobalObject global(cx, mOwnerContent->GetOwnerGlobal()->GetGlobalJSObject());
  MOZ_ASSERT(!global.Failed());

  // Set our mBrowserChangingProcessBlockers property to refer to the blockers
  // list. We will synchronously dispatch a DOM event to collect this list of
  // blockers.
  nsTArray<RefPtr<Promise>> blockers;
  mBrowserChangingProcessBlockers = &blockers;

  GroupedHistoryEventInit eventInit;
  eventInit.mBubbles = true;
  eventInit.mCancelable = false;
  eventInit.mOtherBrowser = nullptr;
  RefPtr<GroupedHistoryEvent> event =
    GroupedHistoryEvent::Constructor(mOwnerContent,
                                     NS_LITERAL_STRING("BrowserWillChangeProcess"),
                                     eventInit);
  event->SetTrusted(true);
  bool dummy;
  mOwnerContent->DispatchEvent(event, &dummy);

  mBrowserChangingProcessBlockers = nullptr;

  ErrorResult rv;
  RefPtr<Promise> allPromise = Promise::All(global, blockers, rv);
  return allPromise.forget();
}

NS_IMETHODIMP
nsFrameLoader::AppendPartialSHistoryAndSwap(nsIFrameLoader* aOther, nsISupports** aPromise)
{
  if (!aOther) {
    return NS_ERROR_INVALID_POINTER;
  }

  if (aOther == this) {
    return NS_OK;
  }

  RefPtr<nsFrameLoader> otherLoader = static_cast<nsFrameLoader*>(aOther);

  RefPtr<Promise> ready = FireWillChangeProcessEvent();
  if (NS_WARN_IF(!ready)) {
    return NS_ERROR_FAILURE;
  }

  // This promise will be resolved when the swap has finished, we return it now
  // and pass it to our helper so our helper can resolve it.
  ErrorResult rv;
  RefPtr<Promise> complete = Promise::Create(mOwnerContent->GetOwnerGlobal(), rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  // Attach our handler to the ready promise, and make it fulfil the complete
  // promise when we are done.
  RefPtr<AppendPartialSHistoryAndSwapHelper> helper =
    new AppendPartialSHistoryAndSwapHelper(this, otherLoader, complete);
  ready->AppendNativeHandler(helper);
  complete.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::RequestGroupedHistoryNavigation(uint32_t aGlobalIndex, nsISupports** aPromise)
{

  RefPtr<Promise> ready = FireWillChangeProcessEvent();
  if (NS_WARN_IF(!ready)) {
    return NS_ERROR_FAILURE;
  }

  // This promise will be resolved when the swap has finished, we return it now
  // and pass it to our helper so our helper can resolve it.
  ErrorResult rv;
  RefPtr<Promise> complete = Promise::Create(mOwnerContent->GetOwnerGlobal(), rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  // Attach our handler to the ready promise, and make it fulfil the complete
  // promise when we are done.
  RefPtr<RequestGroupedHistoryNavigationHelper> helper =
    new RequestGroupedHistoryNavigationHelper(this, aGlobalIndex, complete);
  ready->AppendNativeHandler(helper);
  complete.forget(aPromise);
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::AddProcessChangeBlockingPromise(js::Handle<js::Value> aPromise,
                                               JSContext* aCx)
{
  nsCOMPtr<nsIGlobalObject> go = xpc::NativeGlobal(JS::CurrentGlobalOrNull(aCx));
  if (!go) {
    return NS_ERROR_FAILURE;
  }
  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Resolve(go, aCx, aPromise, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  if (NS_WARN_IF(!mBrowserChangingProcessBlockers)) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  mBrowserChangingProcessBlockers->AppendElement(promise);
  return NS_OK;
}

nsresult
nsFrameLoader::ReallyStartLoading()
{
  nsresult rv = ReallyStartLoadingInternal();
  if (NS_FAILED(rv)) {
    FireErrorEvent();
  }

  return rv;
}

nsresult
nsFrameLoader::ReallyStartLoadingInternal()
{
  NS_ENSURE_STATE(mURIToLoad && mOwnerContent && mOwnerContent->IsInComposedDoc());

  PROFILER_LABEL("nsFrameLoader", "ReallyStartLoading",
    js::ProfileEntry::Category::OTHER);

  if (IsRemoteFrame()) {
    if (!mRemoteBrowser && !TryRemoteBrowser()) {
        NS_WARNING("Couldn't create child process for iframe.");
        return NS_ERROR_FAILURE;
    }

    // FIXME get error codes from child
    mRemoteBrowser->LoadURL(mURIToLoad);

    if (!mRemoteBrowserShown && !ShowRemoteFrame(ScreenIntSize(0, 0))) {
      NS_WARNING("[nsFrameLoader] ReallyStartLoadingInternal tried but couldn't show remote browser.\n");
    }

    return NS_OK;
  }

  nsresult rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return rv;
  }
  NS_ASSERTION(mDocShell,
               "MaybeCreateDocShell succeeded with a null mDocShell");

  // Just to be safe, recheck uri.
  rv = CheckURILoad(mURIToLoad);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
  mDocShell->CreateLoadInfo(getter_AddRefs(loadInfo));
  NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

  // If this frame is sandboxed with respect to origin we will set it up with
  // a null principal later in nsDocShell::DoURILoad.
  // We do it there to correctly sandbox content that was loaded into
  // the frame via other methods than the src attribute.
  // We'll use our principal, not that of the document loaded inside us.  This
  // is very important; needed to prevent XSS attacks on documents loaded in
  // subframes!
  loadInfo->SetTriggeringPrincipal(mOwnerContent->NodePrincipal());

  nsCOMPtr<nsIURI> referrer;

  nsAutoString srcdoc;
  bool isSrcdoc = mOwnerContent->IsHTMLElement(nsGkAtoms::iframe) &&
                  mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::srcdoc,
                                         srcdoc);

  if (isSrcdoc) {
    nsAutoString referrerStr;
    mOwnerContent->OwnerDoc()->GetReferrer(referrerStr);
    rv = NS_NewURI(getter_AddRefs(referrer), referrerStr);

    loadInfo->SetSrcdocData(srcdoc);
    nsCOMPtr<nsIURI> baseURI = mOwnerContent->GetBaseURI();
    loadInfo->SetBaseURI(baseURI);
  }
  else {
    rv = mOwnerContent->NodePrincipal()->GetURI(getter_AddRefs(referrer));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Use referrer as long as it is not an NullPrincipalURI.
  // We could add a method such as GetReferrerURI to principals to make this
  // cleaner, but given that we need to start using Source Browsing Context for
  // referrer (see Bug 960639) this may be wasted effort at this stage.
  if (referrer) {
    bool isNullPrincipalScheme;
    rv = referrer->SchemeIs(NS_NULLPRINCIPAL_SCHEME, &isNullPrincipalScheme);
    if (NS_SUCCEEDED(rv) && !isNullPrincipalScheme) {
      loadInfo->SetReferrer(referrer);
    }
  }

  // get referrer policy for this iframe:
  // first load document wide policy, then
  // load iframe referrer attribute if enabled in preferences
  // per element referrer overrules document wide referrer if enabled
  net::ReferrerPolicy referrerPolicy = mOwnerContent->OwnerDoc()->GetReferrerPolicy();
  HTMLIFrameElement* iframe = HTMLIFrameElement::FromContent(mOwnerContent);
  if (iframe) {
    net::ReferrerPolicy iframeReferrerPolicy = iframe->GetReferrerPolicyAsEnum();
    if (iframeReferrerPolicy != net::RP_Unset) {
      referrerPolicy = iframeReferrerPolicy;
    }
  }
  loadInfo->SetReferrerPolicy(referrerPolicy);

  // Default flags:
  int32_t flags = nsIWebNavigation::LOAD_FLAGS_NONE;

  // Flags for browser frame:
  if (OwnerIsMozBrowserFrame()) {
    flags = nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
            nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
  }

  // Kick off the load...
  bool tmpState = mNeedsAsyncDestroy;
  mNeedsAsyncDestroy = true;
  nsCOMPtr<nsIURI> uriToLoad = mURIToLoad;
  rv = mDocShell->LoadURI(uriToLoad, loadInfo, flags, false);
  mNeedsAsyncDestroy = tmpState;
  mURIToLoad = nullptr;
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsFrameLoader::CheckURILoad(nsIURI* aURI)
{
  // Check for security.  The fun part is trying to figure out what principals
  // to use.  The way I figure it, if we're doing a LoadFrame() accidentally
  // (eg someone created a frame/iframe node, we're being parsed, XUL iframes
  // are being reframed, etc.) then we definitely want to use the node
  // principal of mOwnerContent for security checks.  If, on the other hand,
  // someone's setting the src on our owner content, or created it via script,
  // or whatever, then they can clearly access it... and we should still use
  // the principal of mOwnerContent.  I don't think that leads to privilege
  // escalation, and it's reasonably guaranteed to not lead to XSS issues
  // (since caller can already access mOwnerContent in this case).  So just use
  // the principal of mOwnerContent no matter what.  If script wants to run
  // things with its own permissions, which differ from those of mOwnerContent
  // (which means the script is privileged in some way) it should set
  // window.location instead.
  nsIScriptSecurityManager *secMan = nsContentUtils::GetSecurityManager();

  // Get our principal
  nsIPrincipal* principal = mOwnerContent->NodePrincipal();

  // Check if we are allowed to load absURL
  nsresult rv =
    secMan->CheckLoadURIWithPrincipal(principal, aURI,
                                      nsIScriptSecurityManager::STANDARD);
  if (NS_FAILED(rv)) {
    return rv; // We're not
  }

  // Bail out if this is an infinite recursion scenario
  if (IsRemoteFrame()) {
    return NS_OK;
  }
  return CheckForRecursiveLoad(aURI);
}

NS_IMETHODIMP
nsFrameLoader::GetDocShell(nsIDocShell **aDocShell)
{
  *aDocShell = nullptr;
  nsresult rv = NS_OK;

  if (IsRemoteFrame()) {
    return rv;
  }

  // If we have an owner, make sure we have a docshell and return
  // that. If not, we're most likely in the middle of being torn down,
  // then we just return null.
  if (mOwnerContent) {
    nsresult rv = MaybeCreateDocShell();
    if (NS_FAILED(rv)) {
      return rv;
    }
    NS_ASSERTION(mDocShell,
                 "MaybeCreateDocShell succeeded, but null mDocShell");
  }

  *aDocShell = mDocShell;
  NS_IF_ADDREF(*aDocShell);

  return rv;
}

static void
SetTreeOwnerAndChromeEventHandlerOnDocshellTree(nsIDocShellTreeItem* aItem,
                                                nsIDocShellTreeOwner* aOwner,
                                                EventTarget* aHandler)
{
  NS_PRECONDITION(aItem, "Must have item");

  aItem->SetTreeOwner(aOwner);

  int32_t childCount = 0;
  aItem->GetChildCount(&childCount);
  for (int32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> item;
    aItem->GetChildAt(i, getter_AddRefs(item));
    if (aHandler) {
      nsCOMPtr<nsIDocShell> shell(do_QueryInterface(item));
      shell->SetChromeEventHandler(aHandler);
    }
    SetTreeOwnerAndChromeEventHandlerOnDocshellTree(item, aOwner, aHandler);
  }
}

/**
 * Set the type of the treeitem and hook it up to the treeowner.
 * @param aItem the treeitem we're working with
 * @param aTreeOwner the relevant treeowner; might be null
 * @param aParentType the nsIDocShellTreeItem::GetType of our parent docshell
 * @param aParentNode if non-null, the docshell we should be added as a child to
 *
 * @return whether aItem is top-level content
 */
bool
nsFrameLoader::AddTreeItemToTreeOwner(nsIDocShellTreeItem* aItem,
                                      nsIDocShellTreeOwner* aOwner,
                                      int32_t aParentType,
                                      nsIDocShell* aParentNode)
{
  NS_PRECONDITION(aItem, "Must have docshell treeitem");
  NS_PRECONDITION(mOwnerContent, "Must have owning content");

  nsAutoString value;
  bool isContent = false;
  mOwnerContent->GetAttr(kNameSpaceID_None, TypeAttrName(), value);

  // we accept "content" and "content-xxx" values.
  // We ignore anything that comes after 'content-'.
  isContent = value.LowerCaseEqualsLiteral("content") ||
    StringBeginsWith(value, NS_LITERAL_STRING("content-"),
                     nsCaseInsensitiveStringComparator());


  // Force mozbrowser frames to always be typeContent, even if the
  // mozbrowser interfaces are disabled.
  nsCOMPtr<nsIDOMMozBrowserFrame> mozbrowser =
    do_QueryInterface(mOwnerContent);
  if (mozbrowser) {
    bool isMozbrowser = false;
    mozbrowser->GetMozbrowser(&isMozbrowser);
    isContent |= isMozbrowser;
  }

  if (isContent) {
    // The web shell's type is content.

    aItem->SetItemType(nsIDocShellTreeItem::typeContent);
  } else {
    // Inherit our type from our parent docshell.  If it is
    // chrome, we'll be chrome.  If it is content, we'll be
    // content.

    aItem->SetItemType(aParentType);
  }

  // Now that we have our type set, add ourselves to the parent, as needed.
  if (aParentNode) {
    aParentNode->AddChild(aItem);
  }

  bool retval = false;
  if (aParentType == nsIDocShellTreeItem::typeChrome && isContent) {
    retval = true;

    bool is_primary =
      mOwnerContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::primary,
                                 nsGkAtoms::_true, eIgnoreCase);
    if (aOwner) {
      mOwnerContent->AddMutationObserver(this);
      mObservingOwnerContent = true;
      aOwner->ContentShellAdded(aItem, is_primary);
    }
  }

  return retval;
}

static bool
AllDescendantsOfType(nsIDocShellTreeItem* aParentItem, int32_t aType)
{
  int32_t childCount = 0;
  aParentItem->GetChildCount(&childCount);

  for (int32_t i = 0; i < childCount; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> kid;
    aParentItem->GetChildAt(i, getter_AddRefs(kid));

    if (kid->ItemType() != aType || !AllDescendantsOfType(kid, aType)) {
      return false;
    }
  }

  return true;
}

/**
 * A class that automatically sets mInShow to false when it goes
 * out of scope.
 */
class MOZ_RAII AutoResetInShow {
  private:
    nsFrameLoader* mFrameLoader;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  public:
    explicit AutoResetInShow(nsFrameLoader* aFrameLoader MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : mFrameLoader(aFrameLoader)
    {
      MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
    ~AutoResetInShow() { mFrameLoader->mInShow = false; }
};


bool
nsFrameLoader::Show(int32_t marginWidth, int32_t marginHeight,
                    int32_t scrollbarPrefX, int32_t scrollbarPrefY,
                    nsSubDocumentFrame* frame)
{
  if (mInShow) {
    return false;
  }
  // Reset mInShow if we exit early.
  AutoResetInShow resetInShow(this);
  mInShow = true;

  ScreenIntSize size = frame->GetSubdocumentSize();
  if (IsRemoteFrame()) {
    return ShowRemoteFrame(size, frame);
  }

  nsresult rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return false;
  }
  NS_ASSERTION(mDocShell,
               "MaybeCreateDocShell succeeded, but null mDocShell");
  if (!mDocShell) {
    return false;
  }

  mDocShell->SetMarginWidth(marginWidth);
  mDocShell->SetMarginHeight(marginHeight);

  nsCOMPtr<nsIScrollable> sc = do_QueryInterface(mDocShell);
  if (sc) {
    sc->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_X,
                                       scrollbarPrefX);
    sc->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_Y,
                                       scrollbarPrefY);
  }

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
  if (presShell) {
    // Ensure root scroll frame is reflowed in case scroll preferences or
    // margins have changed
    nsIFrame* rootScrollFrame = presShell->GetRootScrollFrame();
    if (rootScrollFrame) {
      presShell->FrameNeedsReflow(rootScrollFrame, nsIPresShell::eResize,
                                  NS_FRAME_IS_DIRTY);
    }
    return true;
  }

  nsView* view = frame->EnsureInnerView();
  if (!view)
    return false;

  nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(mDocShell);
  NS_ASSERTION(baseWindow, "Found a nsIDocShell that isn't a nsIBaseWindow.");
  baseWindow->InitWindow(nullptr, view->GetWidget(), 0, 0,
                         size.width, size.height);
  // This is kinda whacky, this "Create()" call doesn't really
  // create anything, one starts to wonder why this was named
  // "Create"...
  baseWindow->Create();
  baseWindow->SetVisibility(true);
  NS_ENSURE_TRUE(mDocShell, false);

  // Trigger editor re-initialization if midas is turned on in the
  // sub-document. This shouldn't be necessary, but given the way our
  // editor works, it is. See
  // https://bugzilla.mozilla.org/show_bug.cgi?id=284245
  presShell = mDocShell->GetPresShell();
  if (presShell) {
    nsCOMPtr<nsIDOMHTMLDocument> doc =
      do_QueryInterface(presShell->GetDocument());

    if (doc) {
      nsAutoString designMode;
      doc->GetDesignMode(designMode);

      if (designMode.EqualsLiteral("on")) {
        // Hold on to the editor object to let the document reattach to the
        // same editor object, instead of creating a new one.
        nsCOMPtr<nsIEditor> editor;
        nsresult rv = mDocShell->GetEditor(getter_AddRefs(editor));
        NS_ENSURE_SUCCESS(rv, false);

        doc->SetDesignMode(NS_LITERAL_STRING("off"));
        doc->SetDesignMode(NS_LITERAL_STRING("on"));
      } else {
        // Re-initialize the presentation for contenteditable documents
        bool editable = false,
             hasEditingSession = false;
        mDocShell->GetEditable(&editable);
        mDocShell->GetHasEditingSession(&hasEditingSession);
        nsCOMPtr<nsIEditor> editor;
        mDocShell->GetEditor(getter_AddRefs(editor));
        if (editable && hasEditingSession && editor) {
          editor->PostCreate();
        }
      }
    }
  }

  mInShow = false;
  if (mHideCalled) {
    mHideCalled = false;
    Hide();
    return false;
  }
  return true;
}

void
nsFrameLoader::MarginsChanged(uint32_t aMarginWidth,
                              uint32_t aMarginHeight)
{
  // We assume that the margins are always zero for remote frames.
  if (IsRemoteFrame())
    return;

  // If there's no docshell, we're probably not up and running yet.
  // nsFrameLoader::Show() will take care of setting the right
  // margins.
  if (!mDocShell)
    return;

  // Set the margins
  mDocShell->SetMarginWidth(aMarginWidth);
  mDocShell->SetMarginHeight(aMarginHeight);

  // There's a cached property declaration block
  // that needs to be updated
  if (nsIDocument* doc = mDocShell->GetDocument()) {
    // We don't need to do anything for Gecko here because
    // we plan to RebuildAllStyleData anyway.
    if (doc->GetStyleBackendType() == StyleBackendType::Servo) {
      for (nsINode* cur = doc; cur; cur = cur->GetNextNode()) {
        if (cur->IsHTMLElement(nsGkAtoms::body)) {
          static_cast<HTMLBodyElement*>(cur)->ClearMappedServoStyle();
        }
      }
    }
  }

  // Trigger a restyle if there's a prescontext
  // FIXME: This could do something much less expensive.
  RefPtr<nsPresContext> presContext;
  mDocShell->GetPresContext(getter_AddRefs(presContext));
  if (presContext)
    // rebuild, because now the same nsMappedAttributes* will produce
    // a different style
    presContext->RebuildAllStyleData(nsChangeHint(0), eRestyle_Subtree);
}

bool
nsFrameLoader::ShowRemoteFrame(const ScreenIntSize& size,
                               nsSubDocumentFrame *aFrame)
{
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::GRAPHICS);
  NS_ASSERTION(IsRemoteFrame(), "ShowRemote only makes sense on remote frames.");

  if (!mRemoteBrowser && !TryRemoteBrowser()) {
    NS_ERROR("Couldn't create child process.");
    return false;
  }

  // FIXME/bug 589337: Show()/Hide() is pretty expensive for
  // cross-process layers; need to figure out what behavior we really
  // want here.  For now, hack.
  if (!mRemoteBrowserShown) {
    if (!mOwnerContent ||
        !mOwnerContent->GetComposedDoc()) {
      return false;
    }

    RenderFrameParent* rfp = GetCurrentRenderFrame();
    if (!rfp) {
      return false;
    }

    if (!rfp->AttachLayerManager()) {
      // This is just not going to work.
      return false;
    }

    nsPIDOMWindowOuter* win = mOwnerContent->OwnerDoc()->GetWindow();
    bool parentIsActive = false;
    if (win) {
      nsCOMPtr<nsPIWindowRoot> windowRoot =
        nsGlobalWindow::Cast(win)->GetTopWindowRoot();
      if (windowRoot) {
        nsPIDOMWindowOuter* topWin = windowRoot->GetWindow();
        parentIsActive = topWin && topWin->IsActive();
      }
    }
    mRemoteBrowser->Show(size, parentIsActive);
    mRemoteBrowserShown = true;

    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->NotifyObservers(NS_ISUPPORTS_CAST(nsIFrameLoader*, this),
                          "remote-browser-shown", nullptr);
    }
  } else {
    nsIntRect dimensions;
    NS_ENSURE_SUCCESS(GetWindowDimensions(dimensions), false);

    // Don't show remote iframe if we are waiting for the completion of reflow.
    if (!aFrame || !(aFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {
      mRemoteBrowser->UpdateDimensions(dimensions, size);
    }
  }

  return true;
}

void
nsFrameLoader::Hide()
{
  if (mHideCalled) {
    return;
  }
  if (mInShow) {
    mHideCalled = true;
    return;
  }

  if (!mDocShell)
    return;

  nsCOMPtr<nsIContentViewer> contentViewer;
  mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
  if (contentViewer)
    contentViewer->SetSticky(false);

  nsCOMPtr<nsIBaseWindow> baseWin = do_QueryInterface(mDocShell);
  NS_ASSERTION(baseWin,
               "Found an nsIDocShell which doesn't implement nsIBaseWindow.");
  baseWin->SetVisibility(false);
  baseWin->SetParentWidget(nullptr);
}

nsresult
nsFrameLoader::SwapWithOtherRemoteLoader(nsFrameLoader* aOther,
                                         nsIFrameLoaderOwner* aThisOwner,
                                         nsIFrameLoaderOwner* aOtherOwner)
{
  MOZ_ASSERT(NS_IsMainThread());

#ifdef DEBUG
  RefPtr<nsFrameLoader> first = aThisOwner->GetFrameLoader();
  RefPtr<nsFrameLoader> second = aOtherOwner->GetFrameLoader();
  MOZ_ASSERT(first == this, "aThisOwner must own this");
  MOZ_ASSERT(second == aOther, "aOtherOwner must own aOther");
#endif

  Element* ourContent = mOwnerContent;
  Element* otherContent = aOther->mOwnerContent;

  if (!ourContent || !otherContent) {
    // Can't handle this
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Make sure there are no same-origin issues
  bool equal;
  nsresult rv =
    ourContent->NodePrincipal()->Equals(otherContent->NodePrincipal(), &equal);
  if (NS_FAILED(rv) || !equal) {
    // Security problems loom.  Just bail on it all
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsIDocument* ourDoc = ourContent->GetComposedDoc();
  nsIDocument* otherDoc = otherContent->GetComposedDoc();
  if (!ourDoc || !otherDoc) {
    // Again, how odd, given that we had docshells
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsIPresShell* ourShell = ourDoc->GetShell();
  nsIPresShell* otherShell = otherDoc->GetShell();
  if (!ourShell || !otherShell) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (!mRemoteBrowser || !aOther->mRemoteBrowser) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (mRemoteBrowser->IsIsolatedMozBrowserElement() !=
      aOther->mRemoteBrowser->IsIsolatedMozBrowserElement()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // When we swap docShells, maybe we have to deal with a new page created just
  // for this operation. In this case, the browser code should already have set
  // the correct userContextId attribute value in the owning XULElement, but our
  // docShell, that has been created way before) doesn't know that that
  // happened.
  // This is the reason why now we must retrieve the correct value from the
  // usercontextid attribute before comparing our originAttributes with the
  // other one.
  OriginAttributes ourOriginAttributes =
    mRemoteBrowser->OriginAttributesRef();
  rv = PopulateUserContextIdFromAttribute(ourOriginAttributes);
  NS_ENSURE_SUCCESS(rv,rv);

  OriginAttributes otherOriginAttributes =
    aOther->mRemoteBrowser->OriginAttributesRef();
  rv = aOther->PopulateUserContextIdFromAttribute(otherOriginAttributes);
  NS_ENSURE_SUCCESS(rv,rv);

  if (ourOriginAttributes != otherOriginAttributes) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  bool ourHasHistory =
    mIsTopLevelContent &&
    ourContent->IsXULElement(nsGkAtoms::browser) &&
    !ourContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disablehistory);
  bool otherHasHistory =
    aOther->mIsTopLevelContent &&
    otherContent->IsXULElement(nsGkAtoms::browser) &&
    !otherContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disablehistory);
  if (ourHasHistory != otherHasHistory) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (mInSwap || aOther->mInSwap) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  mInSwap = aOther->mInSwap = true;

  nsIFrame* ourFrame = ourContent->GetPrimaryFrame();
  nsIFrame* otherFrame = otherContent->GetPrimaryFrame();
  if (!ourFrame || !otherFrame) {
    mInSwap = aOther->mInSwap = false;
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsSubDocumentFrame* ourFrameFrame = do_QueryFrame(ourFrame);
  if (!ourFrameFrame) {
    mInSwap = aOther->mInSwap = false;
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  rv = ourFrameFrame->BeginSwapDocShells(otherFrame);
  if (NS_FAILED(rv)) {
    mInSwap = aOther->mInSwap = false;
    return rv;
  }

  nsCOMPtr<nsIBrowserDOMWindow> otherBrowserDOMWindow =
    aOther->mRemoteBrowser->GetBrowserDOMWindow();
  nsCOMPtr<nsIBrowserDOMWindow> browserDOMWindow =
    mRemoteBrowser->GetBrowserDOMWindow();

  if (!!otherBrowserDOMWindow != !!browserDOMWindow) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Destroy browser frame scripts for content leaving a frame with browser API
  if (OwnerIsMozBrowserFrame() && !aOther->OwnerIsMozBrowserFrame()) {
    DestroyBrowserFrameScripts();
  }
  if (!OwnerIsMozBrowserFrame() && aOther->OwnerIsMozBrowserFrame()) {
    aOther->DestroyBrowserFrameScripts();
  }

  aOther->mRemoteBrowser->SetBrowserDOMWindow(browserDOMWindow);
  mRemoteBrowser->SetBrowserDOMWindow(otherBrowserDOMWindow);

#ifdef XP_WIN
  // Native plugin windows used by this remote content need to be reparented.
  if (nsPIDOMWindowOuter* newWin = ourDoc->GetWindow()) {
    RefPtr<nsIWidget> newParent = nsGlobalWindow::Cast(newWin)->GetMainWidget();
    const ManagedContainer<mozilla::plugins::PPluginWidgetParent>& plugins =
      aOther->mRemoteBrowser->ManagedPPluginWidgetParent();
    for (auto iter = plugins.ConstIter(); !iter.Done(); iter.Next()) {
      static_cast<mozilla::plugins::PluginWidgetParent*>(iter.Get()->GetKey())->SetParent(newParent);
    }
  }
#endif // XP_WIN

  MaybeUpdatePrimaryTabParent(eTabParentRemoved);
  aOther->MaybeUpdatePrimaryTabParent(eTabParentRemoved);

  SetOwnerContent(otherContent);
  aOther->SetOwnerContent(ourContent);

  mRemoteBrowser->SetOwnerElement(otherContent);
  aOther->mRemoteBrowser->SetOwnerElement(ourContent);

  MaybeUpdatePrimaryTabParent(eTabParentChanged);
  aOther->MaybeUpdatePrimaryTabParent(eTabParentChanged);

  RefPtr<nsFrameMessageManager> ourMessageManager = mMessageManager;
  RefPtr<nsFrameMessageManager> otherMessageManager = aOther->mMessageManager;
  // Swap and setup things in parent message managers.
  if (ourMessageManager) {
    ourMessageManager->SetCallback(aOther);
  }
  if (otherMessageManager) {
    otherMessageManager->SetCallback(this);
  }
  mMessageManager.swap(aOther->mMessageManager);

  // Perform the actual swap of the internal refptrs. We keep a strong reference
  // to ourselves to make sure we don't die while we overwrite our reference to
  // ourself.
  nsCOMPtr<nsIFrameLoader> kungFuDeathGrip(this);
  aThisOwner->InternalSetFrameLoader(aOther);
  aOtherOwner->InternalSetFrameLoader(kungFuDeathGrip);

  ourFrameFrame->EndSwapDocShells(otherFrame);

  ourShell->BackingScaleFactorChanged();
  otherShell->BackingScaleFactorChanged();

  ourDoc->FlushPendingNotifications(FlushType::Layout);
  otherDoc->FlushPendingNotifications(FlushType::Layout);

  // Initialize browser API if needed now that owner content has changed.
  InitializeBrowserAPI();
  aOther->InitializeBrowserAPI();

  mInSwap = aOther->mInSwap = false;

  // Send an updated tab context since owner content type may have changed.
  MutableTabContext ourContext;
  rv = GetNewTabContext(&ourContext);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  MutableTabContext otherContext;
  rv = aOther->GetNewTabContext(&otherContext);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Swap the remoteType property as the frameloaders are being swapped
  nsAutoString ourRemoteType;
  if (!ourContent->GetAttr(kNameSpaceID_None, nsGkAtoms::RemoteType,
                           ourRemoteType)) {
    ourRemoteType.AssignLiteral(DEFAULT_REMOTE_TYPE);
  }
  nsAutoString otherRemoteType;
  if (!otherContent->GetAttr(kNameSpaceID_None, nsGkAtoms::RemoteType,
                             otherRemoteType)) {
    otherRemoteType.AssignLiteral(DEFAULT_REMOTE_TYPE);
  }
  ourContent->SetAttr(kNameSpaceID_None, nsGkAtoms::RemoteType, otherRemoteType, false);
  otherContent->SetAttr(kNameSpaceID_None, nsGkAtoms::RemoteType, ourRemoteType, false);

  Unused << mRemoteBrowser->SendSwappedWithOtherRemoteLoader(
    ourContext.AsIPCTabContext());
  Unused << aOther->mRemoteBrowser->SendSwappedWithOtherRemoteLoader(
    otherContext.AsIPCTabContext());
  return NS_OK;
}

class MOZ_RAII AutoResetInFrameSwap final
{
public:
  AutoResetInFrameSwap(nsFrameLoader* aThisFrameLoader,
                       nsFrameLoader* aOtherFrameLoader,
                       nsDocShell* aThisDocShell,
                       nsDocShell* aOtherDocShell,
                       EventTarget* aThisEventTarget,
                       EventTarget* aOtherEventTarget
                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mThisFrameLoader(aThisFrameLoader)
    , mOtherFrameLoader(aOtherFrameLoader)
    , mThisDocShell(aThisDocShell)
    , mOtherDocShell(aOtherDocShell)
    , mThisEventTarget(aThisEventTarget)
    , mOtherEventTarget(aOtherEventTarget)
  {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;

    mThisFrameLoader->mInSwap = true;
    mOtherFrameLoader->mInSwap = true;
    mThisDocShell->SetInFrameSwap(true);
    mOtherDocShell->SetInFrameSwap(true);

    // Fire pageshow events on still-loading pages, and then fire pagehide
    // events.  Note that we do NOT fire these in the normal way, but just fire
    // them on the chrome event handlers.
    nsContentUtils::FirePageShowEvent(mThisDocShell, mThisEventTarget, false);
    nsContentUtils::FirePageShowEvent(mOtherDocShell, mOtherEventTarget, false);
    nsContentUtils::FirePageHideEvent(mThisDocShell, mThisEventTarget);
    nsContentUtils::FirePageHideEvent(mOtherDocShell, mOtherEventTarget);
  }

  ~AutoResetInFrameSwap()
  {
    nsContentUtils::FirePageShowEvent(mThisDocShell, mThisEventTarget, true);
    nsContentUtils::FirePageShowEvent(mOtherDocShell, mOtherEventTarget, true);

    mThisFrameLoader->mInSwap = false;
    mOtherFrameLoader->mInSwap = false;
    mThisDocShell->SetInFrameSwap(false);
    mOtherDocShell->SetInFrameSwap(false);
  }

private:
  RefPtr<nsFrameLoader> mThisFrameLoader;
  RefPtr<nsFrameLoader> mOtherFrameLoader;
  RefPtr<nsDocShell> mThisDocShell;
  RefPtr<nsDocShell> mOtherDocShell;
  nsCOMPtr<EventTarget> mThisEventTarget;
  nsCOMPtr<EventTarget> mOtherEventTarget;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

nsresult
nsFrameLoader::SwapWithOtherLoader(nsFrameLoader* aOther,
                                   nsIFrameLoaderOwner* aThisOwner,
                                   nsIFrameLoaderOwner* aOtherOwner)
{
#ifdef DEBUG
  RefPtr<nsFrameLoader> first = aThisOwner->GetFrameLoader();
  RefPtr<nsFrameLoader> second = aOtherOwner->GetFrameLoader();
  MOZ_ASSERT(first == this, "aThisOwner must own this");
  MOZ_ASSERT(second == aOther, "aOtherOwner must own aOther");
#endif

  NS_ENSURE_STATE(!mInShow && !aOther->mInShow);

  if (IsRemoteFrame() != aOther->IsRemoteFrame()) {
    NS_WARNING("Swapping remote and non-remote frames is not currently supported");
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  Element* ourContent = mOwnerContent;
  Element* otherContent = aOther->mOwnerContent;

  if (!ourContent || !otherContent) {
    // Can't handle this
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  bool ourHasSrcdoc = ourContent->IsHTMLElement(nsGkAtoms::iframe) &&
                      ourContent->HasAttr(kNameSpaceID_None, nsGkAtoms::srcdoc);
  bool otherHasSrcdoc = otherContent->IsHTMLElement(nsGkAtoms::iframe) &&
                        otherContent->HasAttr(kNameSpaceID_None, nsGkAtoms::srcdoc);
  if (ourHasSrcdoc || otherHasSrcdoc) {
    // Ignore this case entirely for now, since we support XUL <-> HTML swapping
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  bool ourFullscreenAllowed =
    ourContent->IsXULElement() ||
    (OwnerIsMozBrowserFrame() &&
      (ourContent->HasAttr(kNameSpaceID_None, nsGkAtoms::allowfullscreen) ||
       ourContent->HasAttr(kNameSpaceID_None, nsGkAtoms::mozallowfullscreen)));
  bool otherFullscreenAllowed =
    otherContent->IsXULElement() ||
    (aOther->OwnerIsMozBrowserFrame() &&
      (otherContent->HasAttr(kNameSpaceID_None, nsGkAtoms::allowfullscreen) ||
       otherContent->HasAttr(kNameSpaceID_None, nsGkAtoms::mozallowfullscreen)));
  if (ourFullscreenAllowed != otherFullscreenAllowed) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Divert to a separate path for the remaining steps in the remote case
  if (IsRemoteFrame()) {
    MOZ_ASSERT(aOther->IsRemoteFrame());
    return SwapWithOtherRemoteLoader(aOther, aThisOwner, aOtherOwner);
  }

  // Make sure there are no same-origin issues
  bool equal;
  nsresult rv =
    ourContent->NodePrincipal()->Equals(otherContent->NodePrincipal(), &equal);
  if (NS_FAILED(rv) || !equal) {
    // Security problems loom.  Just bail on it all
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  RefPtr<nsDocShell> ourDocshell = static_cast<nsDocShell*>(GetExistingDocShell());
  RefPtr<nsDocShell> otherDocshell = static_cast<nsDocShell*>(aOther->GetExistingDocShell());
  if (!ourDocshell || !otherDocshell) {
    // How odd
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // To avoid having to mess with session history, avoid swapping
  // frameloaders that don't correspond to root same-type docshells,
  // unless both roots have session history disabled.
  nsCOMPtr<nsIDocShellTreeItem> ourRootTreeItem, otherRootTreeItem;
  ourDocshell->GetSameTypeRootTreeItem(getter_AddRefs(ourRootTreeItem));
  otherDocshell->GetSameTypeRootTreeItem(getter_AddRefs(otherRootTreeItem));
  nsCOMPtr<nsIWebNavigation> ourRootWebnav =
    do_QueryInterface(ourRootTreeItem);
  nsCOMPtr<nsIWebNavigation> otherRootWebnav =
    do_QueryInterface(otherRootTreeItem);

  if (!ourRootWebnav || !otherRootWebnav) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsISHistory> ourHistory;
  nsCOMPtr<nsISHistory> otherHistory;
  ourRootWebnav->GetSessionHistory(getter_AddRefs(ourHistory));
  otherRootWebnav->GetSessionHistory(getter_AddRefs(otherHistory));

  if ((ourRootTreeItem != ourDocshell || otherRootTreeItem != otherDocshell) &&
      (ourHistory || otherHistory)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Also make sure that the two docshells are the same type. Otherwise
  // swapping is certainly not safe. If this needs to be changed then
  // the code below needs to be audited as it assumes identical types.
  int32_t ourType = ourDocshell->ItemType();
  int32_t otherType = otherDocshell->ItemType();
  if (ourType != otherType) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // One more twist here.  Setting up the right treeowners in a heterogeneous
  // tree is a bit of a pain.  So make sure that if ourType is not
  // nsIDocShellTreeItem::typeContent then all of our descendants are the same
  // type as us.
  if (ourType != nsIDocShellTreeItem::typeContent &&
      (!AllDescendantsOfType(ourDocshell, ourType) ||
       !AllDescendantsOfType(otherDocshell, otherType))) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Save off the tree owners, frame elements, chrome event handlers, and
  // docshell and document parents before doing anything else.
  nsCOMPtr<nsIDocShellTreeOwner> ourOwner, otherOwner;
  ourDocshell->GetTreeOwner(getter_AddRefs(ourOwner));
  otherDocshell->GetTreeOwner(getter_AddRefs(otherOwner));
  // Note: it's OK to have null treeowners.

  nsCOMPtr<nsIDocShellTreeItem> ourParentItem, otherParentItem;
  ourDocshell->GetParent(getter_AddRefs(ourParentItem));
  otherDocshell->GetParent(getter_AddRefs(otherParentItem));
  if (!ourParentItem || !otherParentItem) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Make sure our parents are the same type too
  int32_t ourParentType = ourParentItem->ItemType();
  int32_t otherParentType = otherParentItem->ItemType();
  if (ourParentType != otherParentType) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsPIDOMWindowOuter> ourWindow = ourDocshell->GetWindow();
  nsCOMPtr<nsPIDOMWindowOuter> otherWindow = otherDocshell->GetWindow();

  nsCOMPtr<Element> ourFrameElement =
    ourWindow->GetFrameElementInternal();
  nsCOMPtr<Element> otherFrameElement =
    otherWindow->GetFrameElementInternal();

  nsCOMPtr<EventTarget> ourChromeEventHandler =
    do_QueryInterface(ourWindow->GetChromeEventHandler());
  nsCOMPtr<EventTarget> otherChromeEventHandler =
    do_QueryInterface(otherWindow->GetChromeEventHandler());

  nsCOMPtr<EventTarget> ourEventTarget = ourWindow->GetParentTarget();
  nsCOMPtr<EventTarget> otherEventTarget = otherWindow->GetParentTarget();

  NS_ASSERTION(SameCOMIdentity(ourFrameElement, ourContent) &&
               SameCOMIdentity(otherFrameElement, otherContent) &&
               SameCOMIdentity(ourChromeEventHandler, ourContent) &&
               SameCOMIdentity(otherChromeEventHandler, otherContent),
               "How did that happen, exactly?");

  nsCOMPtr<nsIDocument> ourChildDocument = ourWindow->GetExtantDoc();
  nsCOMPtr<nsIDocument> otherChildDocument = otherWindow ->GetExtantDoc();
  if (!ourChildDocument || !otherChildDocument) {
    // This shouldn't be happening
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsCOMPtr<nsIDocument> ourParentDocument =
    ourChildDocument->GetParentDocument();
  nsCOMPtr<nsIDocument> otherParentDocument =
    otherChildDocument->GetParentDocument();

  // Make sure to swap docshells between the two frames.
  nsIDocument* ourDoc = ourContent->GetComposedDoc();
  nsIDocument* otherDoc = otherContent->GetComposedDoc();
  if (!ourDoc || !otherDoc) {
    // Again, how odd, given that we had docshells
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_ASSERTION(ourDoc == ourParentDocument, "Unexpected parent document");
  NS_ASSERTION(otherDoc == otherParentDocument, "Unexpected parent document");

  nsIPresShell* ourShell = ourDoc->GetShell();
  nsIPresShell* otherShell = otherDoc->GetShell();
  if (!ourShell || !otherShell) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (ourDocshell->GetIsIsolatedMozBrowserElement() !=
      otherDocshell->GetIsIsolatedMozBrowserElement()) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // When we swap docShells, maybe we have to deal with a new page created just
  // for this operation. In this case, the browser code should already have set
  // the correct userContextId attribute value in the owning XULElement, but our
  // docShell, that has been created way before) doesn't know that that
  // happened.
  // This is the reason why now we must retrieve the correct value from the
  // usercontextid attribute before comparing our originAttributes with the
  // other one.
  OriginAttributes ourOriginAttributes =
    ourDocshell->GetOriginAttributes();
  rv = PopulateUserContextIdFromAttribute(ourOriginAttributes);
  NS_ENSURE_SUCCESS(rv,rv);

  OriginAttributes otherOriginAttributes =
    otherDocshell->GetOriginAttributes();
  rv = aOther->PopulateUserContextIdFromAttribute(otherOriginAttributes);
  NS_ENSURE_SUCCESS(rv,rv);

  if (ourOriginAttributes != otherOriginAttributes) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  if (mInSwap || aOther->mInSwap) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  AutoResetInFrameSwap autoFrameSwap(this, aOther, ourDocshell, otherDocshell,
                                     ourEventTarget, otherEventTarget);

  nsIFrame* ourFrame = ourContent->GetPrimaryFrame();
  nsIFrame* otherFrame = otherContent->GetPrimaryFrame();
  if (!ourFrame || !otherFrame) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  nsSubDocumentFrame* ourFrameFrame = do_QueryFrame(ourFrame);
  if (!ourFrameFrame) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // OK.  First begin to swap the docshells in the two nsIFrames
  rv = ourFrameFrame->BeginSwapDocShells(otherFrame);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Destroy browser frame scripts for content leaving a frame with browser API
  if (OwnerIsMozBrowserFrame() && !aOther->OwnerIsMozBrowserFrame()) {
    DestroyBrowserFrameScripts();
  }
  if (!OwnerIsMozBrowserFrame() && aOther->OwnerIsMozBrowserFrame()) {
    aOther->DestroyBrowserFrameScripts();
  }

  // Now move the docshells to the right docshell trees.  Note that this
  // resets their treeowners to null.
  ourParentItem->RemoveChild(ourDocshell);
  otherParentItem->RemoveChild(otherDocshell);
  if (ourType == nsIDocShellTreeItem::typeContent) {
    ourOwner->ContentShellRemoved(ourDocshell);
    otherOwner->ContentShellRemoved(otherDocshell);
  }

  ourParentItem->AddChild(otherDocshell);
  otherParentItem->AddChild(ourDocshell);

  // Restore the correct chrome event handlers.
  ourDocshell->SetChromeEventHandler(otherChromeEventHandler);
  otherDocshell->SetChromeEventHandler(ourChromeEventHandler);
  // Restore the correct treeowners
  // (and also chrome event handlers for content frames only).
  SetTreeOwnerAndChromeEventHandlerOnDocshellTree(ourDocshell, otherOwner,
    ourType == nsIDocShellTreeItem::typeContent ? otherChromeEventHandler.get() : nullptr);
  SetTreeOwnerAndChromeEventHandlerOnDocshellTree(otherDocshell, ourOwner,
    ourType == nsIDocShellTreeItem::typeContent ? ourChromeEventHandler.get() : nullptr);

  // Switch the owner content before we start calling AddTreeItemToTreeOwner.
  // Note that we rely on this to deal with setting mObservingOwnerContent to
  // false and calling RemoveMutationObserver as needed.
  SetOwnerContent(otherContent);
  aOther->SetOwnerContent(ourContent);

  AddTreeItemToTreeOwner(ourDocshell, otherOwner, otherParentType, nullptr);
  aOther->AddTreeItemToTreeOwner(otherDocshell, ourOwner, ourParentType,
                                 nullptr);

  // SetSubDocumentFor nulls out parent documents on the old child doc if a
  // new non-null document is passed in, so just go ahead and remove both
  // kids before reinserting in the parent subdoc maps, to avoid
  // complications.
  ourParentDocument->SetSubDocumentFor(ourContent, nullptr);
  otherParentDocument->SetSubDocumentFor(otherContent, nullptr);
  ourParentDocument->SetSubDocumentFor(ourContent, otherChildDocument);
  otherParentDocument->SetSubDocumentFor(otherContent, ourChildDocument);

  ourWindow->SetFrameElementInternal(otherFrameElement);
  otherWindow->SetFrameElementInternal(ourFrameElement);

  RefPtr<nsFrameMessageManager> ourMessageManager = mMessageManager;
  RefPtr<nsFrameMessageManager> otherMessageManager = aOther->mMessageManager;
  // Swap pointers in child message managers.
  if (mChildMessageManager) {
    nsInProcessTabChildGlobal* tabChild =
      static_cast<nsInProcessTabChildGlobal*>(mChildMessageManager.get());
    tabChild->SetOwner(otherContent);
    tabChild->SetChromeMessageManager(otherMessageManager);
  }
  if (aOther->mChildMessageManager) {
    nsInProcessTabChildGlobal* otherTabChild =
      static_cast<nsInProcessTabChildGlobal*>(aOther->mChildMessageManager.get());
    otherTabChild->SetOwner(ourContent);
    otherTabChild->SetChromeMessageManager(ourMessageManager);
  }
  // Swap and setup things in parent message managers.
  if (mMessageManager) {
    mMessageManager->SetCallback(aOther);
  }
  if (aOther->mMessageManager) {
    aOther->mMessageManager->SetCallback(this);
  }
  mMessageManager.swap(aOther->mMessageManager);

  // Perform the actual swap of the internal refptrs. We keep a strong reference
  // to ourselves to make sure we don't die while we overwrite our reference to
  // ourself.
  nsCOMPtr<nsIFrameLoader> kungFuDeathGrip(this);
  aThisOwner->InternalSetFrameLoader(aOther);
  aOtherOwner->InternalSetFrameLoader(kungFuDeathGrip);

  // Drop any cached content viewers in the two session histories.
  nsCOMPtr<nsISHistoryInternal> ourInternalHistory =
    do_QueryInterface(ourHistory);
  nsCOMPtr<nsISHistoryInternal> otherInternalHistory =
    do_QueryInterface(otherHistory);
  if (ourInternalHistory) {
    ourInternalHistory->EvictAllContentViewers();
  }
  if (otherInternalHistory) {
    otherInternalHistory->EvictAllContentViewers();
  }

  NS_ASSERTION(ourFrame == ourContent->GetPrimaryFrame() &&
               otherFrame == otherContent->GetPrimaryFrame(),
               "changed primary frame");

  ourFrameFrame->EndSwapDocShells(otherFrame);

  // If the content being swapped came from windows on two screens with
  // incompatible backing resolution (e.g. dragging a tab between windows on
  // hi-dpi and low-dpi screens), it will have style data that is based on
  // the wrong appUnitsPerDevPixel value. So we tell the PresShells that their
  // backing scale factor may have changed. (Bug 822266)
  ourShell->BackingScaleFactorChanged();
  otherShell->BackingScaleFactorChanged();

  ourParentDocument->FlushPendingNotifications(FlushType::Layout);
  otherParentDocument->FlushPendingNotifications(FlushType::Layout);

  // Initialize browser API if needed now that owner content has changed
  InitializeBrowserAPI();
  aOther->InitializeBrowserAPI();

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::Destroy()
{
  StartDestroy();
  return NS_OK;
}

class nsFrameLoaderDestroyRunnable : public Runnable
{
  enum DestroyPhase
  {
    // See the implementation of Run for an explanation of these phases.
    eDestroyDocShell,
    eWaitForUnloadMessage,
    eDestroyComplete
  };

  RefPtr<nsFrameLoader> mFrameLoader;
  DestroyPhase mPhase;

public:
  explicit nsFrameLoaderDestroyRunnable(nsFrameLoader* aFrameLoader)
   : mFrameLoader(aFrameLoader), mPhase(eDestroyDocShell) {}

  NS_IMETHOD Run() override;
};

void
nsFrameLoader::StartDestroy()
{
  // nsFrameLoader::StartDestroy is called just before the frameloader is
  // detached from the <browser> element. Destruction continues in phases via
  // the nsFrameLoaderDestroyRunnable.

  if (mDestroyCalled) {
    return;
  }
  mDestroyCalled = true;

  // After this point, we return an error when trying to send a message using
  // the message manager on the frame.
  if (mMessageManager) {
    mMessageManager->Close();
  }

  // Retain references to the <browser> element and the frameloader in case we
  // receive any messages from the message manager on the frame. These
  // references are dropped in DestroyComplete.
  if (mChildMessageManager || mRemoteBrowser) {
    mOwnerContentStrong = mOwnerContent;
    if (mRemoteBrowser) {
      mRemoteBrowser->CacheFrameLoader(this);
    }
    if (mChildMessageManager) {
      mChildMessageManager->CacheFrameLoader(this);
    }
  }

  // If the TabParent has installed any event listeners on the window, this is
  // its last chance to remove them while we're still in the document.
  if (mRemoteBrowser) {
    mRemoteBrowser->RemoveWindowListeners();
  }

  nsCOMPtr<nsIDocument> doc;
  bool dynamicSubframeRemoval = false;
  if (mOwnerContent) {
    doc = mOwnerContent->OwnerDoc();
    dynamicSubframeRemoval = !mIsTopLevelContent && !doc->InUnlinkOrDeletion();
    doc->SetSubDocumentFor(mOwnerContent, nullptr);
    MaybeUpdatePrimaryTabParent(eTabParentRemoved);
    SetOwnerContent(nullptr);
  }

  // Seems like this is a dynamic frame removal.
  if (dynamicSubframeRemoval) {
    if (mDocShell) {
      mDocShell->RemoveFromSessionHistory();
    }
  }

  // Let the tree owner know we're gone.
  if (mIsTopLevelContent) {
    if (mDocShell) {
      nsCOMPtr<nsIDocShellTreeItem> parentItem;
      mDocShell->GetParent(getter_AddRefs(parentItem));
      nsCOMPtr<nsIDocShellTreeOwner> owner = do_GetInterface(parentItem);
      if (owner) {
        owner->ContentShellRemoved(mDocShell);
      }
    }
  }

  // Let our window know that we are gone
  if (mDocShell) {
    nsCOMPtr<nsPIDOMWindowOuter> win_private(mDocShell->GetWindow());
    if (win_private) {
      win_private->SetFrameElementInternal(nullptr);
    }
  }

  // Destroy the other frame loader owners now that we are being destroyed.
  if (mPartialSHistory &&
      mPartialSHistory->GetActiveState() == nsIPartialSHistory::STATE_ACTIVE) {
    nsCOMPtr<nsIGroupedSHistory> groupedSHistory;
    GetGroupedSHistory(getter_AddRefs(groupedSHistory));
    if (groupedSHistory) {
      NS_DispatchToCurrentThread(NS_NewRunnableFunction([groupedSHistory] () {
        groupedSHistory->CloseInactiveFrameLoaderOwners();
      }));
    }
  }

  nsCOMPtr<nsIRunnable> destroyRunnable = new nsFrameLoaderDestroyRunnable(this);
  if (mNeedsAsyncDestroy || !doc ||
      NS_FAILED(doc->FinalizeFrameLoader(this, destroyRunnable))) {
    NS_DispatchToCurrentThread(destroyRunnable);
  }
}

nsresult
nsFrameLoaderDestroyRunnable::Run()
{
  switch (mPhase) {
  case eDestroyDocShell:
    mFrameLoader->DestroyDocShell();

    // In the out-of-process case, TabParent will eventually call
    // DestroyComplete once it receives a __delete__ message from the child. In
    // the in-process case, we dispatch a series of runnables to ensure that
    // DestroyComplete gets called at the right time. The frame loader is kept
    // alive by mFrameLoader during this time.
    if (mFrameLoader->mChildMessageManager) {
      // When the docshell is destroyed, NotifyWindowIDDestroyed is called to
      // asynchronously notify {outer,inner}-window-destroyed via a runnable. We
      // don't want DestroyComplete to run until after those runnables have
      // run. Since we're enqueueing ourselves after the window-destroyed
      // runnables are enqueued, we're guaranteed to run after.
      mPhase = eWaitForUnloadMessage;
      NS_DispatchToCurrentThread(this);
    }
    break;

   case eWaitForUnloadMessage:
     // The *-window-destroyed observers have finished running at this
     // point. However, it's possible that a *-window-destroyed observer might
     // have sent a message using the message manager. These messages might not
     // have been processed yet. So we enqueue ourselves again to ensure that
     // DestroyComplete runs after all messages sent by *-window-destroyed
     // observers have been processed.
     mPhase = eDestroyComplete;
     NS_DispatchToCurrentThread(this);
     break;

   case eDestroyComplete:
     // Now that all messages sent by unload listeners and window destroyed
     // observers have been processed, we disconnect the message manager and
     // finish destruction.
     mFrameLoader->DestroyComplete();
     break;
  }

  return NS_OK;
}

void
nsFrameLoader::DestroyDocShell()
{
  // This code runs after the frameloader has been detached from the <browser>
  // element. We postpone this work because we may not be allowed to run
  // script at that time.

  // Ask the TabChild to fire the frame script "unload" event, destroy its
  // docshell, and finally destroy the PBrowser actor. This eventually leads to
  // nsFrameLoader::DestroyComplete being called.
  if (mRemoteBrowser) {
    mRemoteBrowser->Destroy();
  }

  // Fire the "unload" event if we're in-process.
  if (mChildMessageManager) {
    static_cast<nsInProcessTabChildGlobal*>(mChildMessageManager.get())->FireUnloadEvent();
  }

  // Destroy the docshell.
  nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mDocShell));
  if (base_win) {
    base_win->Destroy();
  }
  mDocShell = nullptr;

  if (mChildMessageManager) {
    // Stop handling events in the in-process frame script.
    static_cast<nsInProcessTabChildGlobal*>(mChildMessageManager.get())->DisconnectEventListeners();
  }
}

void
nsFrameLoader::DestroyComplete()
{
  // We get here, as part of StartDestroy, after the docshell has been destroyed
  // and all message manager messages sent during docshell destruction have been
  // dispatched.  We also get here if the child process crashes. In the latter
  // case, StartDestroy might not have been called.

  // Drop the strong references created in StartDestroy.
  if (mChildMessageManager || mRemoteBrowser) {
    mOwnerContentStrong = nullptr;
    if (mRemoteBrowser) {
      mRemoteBrowser->CacheFrameLoader(nullptr);
    }
    if (mChildMessageManager) {
      mChildMessageManager->CacheFrameLoader(nullptr);
    }
  }

  // Call TabParent::Destroy if we haven't already (in case of a crash).
  if (mRemoteBrowser) {
    mRemoteBrowser->SetOwnerElement(nullptr);
    mRemoteBrowser->Destroy();
    mRemoteBrowser = nullptr;
  }

  if (mMessageManager) {
    mMessageManager->Disconnect();
  }

  if (mChildMessageManager) {
    static_cast<nsInProcessTabChildGlobal*>(mChildMessageManager.get())->Disconnect();
  }

  mMessageManager = nullptr;
  mChildMessageManager = nullptr;
}

NS_IMETHODIMP
nsFrameLoader::GetDepthTooGreat(bool* aDepthTooGreat)
{
  *aDepthTooGreat = mDepthTooGreat;
  return NS_OK;
}

void
nsFrameLoader::SetOwnerContent(Element* aContent)
{
  if (mObservingOwnerContent) {
    mObservingOwnerContent = false;
    mOwnerContent->RemoveMutationObserver(this);
  }
  mOwnerContent = aContent;
  if (RenderFrameParent* rfp = GetCurrentRenderFrame()) {
    rfp->OwnerContentChanged(aContent);
  }
}

bool
nsFrameLoader::OwnerIsMozBrowserFrame()
{
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwnerContent);
  return browserFrame ? browserFrame->GetReallyIsBrowser() : false;
}

// The xpcom getter version
NS_IMETHODIMP
nsFrameLoader::GetOwnerIsMozBrowserFrame(bool* aResult)
{
  *aResult = OwnerIsMozBrowserFrame();
  return NS_OK;
}

bool
nsFrameLoader::OwnerIsIsolatedMozBrowserFrame()
{
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwnerContent);
  if (!browserFrame) {
    return false;
  }

  if (!OwnerIsMozBrowserFrame()) {
    return false;
  }

  bool isolated = browserFrame->GetIsolated();
  if (isolated) {
    return true;
  }

  return false;
}

bool
nsFrameLoader::ShouldUseRemoteProcess()
{
  if (PR_GetEnv("MOZ_DISABLE_OOP_TABS") ||
      Preferences::GetBool("dom.ipc.tabs.disabled", false)) {
    return false;
  }

  // Don't try to launch nested children if we don't have OMTC.
  // They won't render!
  if (XRE_IsContentProcess() &&
      !CompositorBridgeChild::ChildProcessHasCompositorBridge()) {
    return false;
  }

  if (XRE_IsContentProcess() &&
      !(PR_GetEnv("MOZ_NESTED_OOP_TABS") ||
        Preferences::GetBool("dom.ipc.tabs.nested.enabled", false))) {
    return false;
  }

  // If we're an <iframe mozbrowser> and we don't have a "remote" attribute,
  // fall back to the default.
  if (OwnerIsMozBrowserFrame() &&
      !mOwnerContent->HasAttr(kNameSpaceID_None, nsGkAtoms::Remote)) {

    return Preferences::GetBool("dom.ipc.browser_frames.oop_by_default", false);
  }

  // Otherwise, we're remote if we have "remote=true" and we're either a
  // browser frame or a XUL element.
  return (OwnerIsMozBrowserFrame() ||
          mOwnerContent->GetNameSpaceID() == kNameSpaceID_XUL) &&
         mOwnerContent->AttrValueIs(kNameSpaceID_None,
                                    nsGkAtoms::Remote,
                                    nsGkAtoms::_true,
                                    eCaseMatters);
}

bool
nsFrameLoader::IsRemoteFrame()
{
  if (mRemoteFrame) {
    MOZ_ASSERT(!mDocShell, "Found a remote frame with a DocShell");
    return true;
  }
  return false;
}

nsresult
nsFrameLoader::MaybeCreateDocShell()
{
  if (mDocShell) {
    return NS_OK;
  }
  if (IsRemoteFrame()) {
    return NS_OK;
  }
  NS_ENSURE_STATE(!mDestroyCalled);

  // Get our parent docshell off the document of mOwnerContent
  // XXXbz this is such a total hack.... We really need to have a
  // better setup for doing this.
  nsIDocument* doc = mOwnerContent->OwnerDoc();

  MOZ_RELEASE_ASSERT(!doc->IsResourceDoc(), "We shouldn't even exist");

  if (!(doc->IsStaticDocument() || mOwnerContent->IsInComposedDoc())) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!doc->IsActive()) {
    // Don't allow subframe loads in non-active documents.
    // (See bug 610571 comment 5.)
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIDocShell> docShell = doc->GetDocShell();
  nsCOMPtr<nsIWebNavigation> parentAsWebNav = do_QueryInterface(docShell);
  NS_ENSURE_STATE(parentAsWebNav);

  // Create the docshell...
  mDocShell = do_CreateInstance("@mozilla.org/docshell;1");
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  if (mIsPrerendered) {
    nsresult rv = mDocShell->SetIsPrerendered();
    NS_ENSURE_SUCCESS(rv,rv);
  }

  if (!mNetworkCreated) {
    if (mDocShell) {
      mDocShell->SetCreatedDynamically(true);
    }
  }

  // Get the frame name and tell the docshell about it.
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);
  nsAutoString frameName;

  int32_t namespaceID = mOwnerContent->GetNameSpaceID();
  if (namespaceID == kNameSpaceID_XHTML && !mOwnerContent->IsInHTMLDocument()) {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::id, frameName);
  } else {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, frameName);
    // XXX if no NAME then use ID, after a transition period this will be
    // changed so that XUL only uses ID too (bug 254284).
    if (frameName.IsEmpty() && namespaceID == kNameSpaceID_XUL) {
      mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::id, frameName);
    }
  }

  if (!frameName.IsEmpty()) {
    mDocShell->SetName(frameName);
  }

  // Inform our docShell that it has a new child.
  // Note: This logic duplicates a lot of logic in
  // nsSubDocumentFrame::AttributeChanged.  We should fix that.

  int32_t parentType = docShell->ItemType();

  // XXXbz why is this in content code, exactly?  We should handle
  // this some other way.....  Not sure how yet.
  nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
  docShell->GetTreeOwner(getter_AddRefs(parentTreeOwner));
  NS_ENSURE_STATE(parentTreeOwner);
  mIsTopLevelContent =
    AddTreeItemToTreeOwner(mDocShell, parentTreeOwner, parentType, docShell);

  // Make sure all shells have links back to the content element
  // in the nearest enclosing chrome shell.
  nsCOMPtr<nsIDOMEventTarget> chromeEventHandler;

  if (parentType == nsIDocShellTreeItem::typeChrome) {
    // Our parent shell is a chrome shell. It is therefore our nearest
    // enclosing chrome shell.

    chromeEventHandler = do_QueryInterface(mOwnerContent);
    NS_ASSERTION(chromeEventHandler,
                 "This mContent should implement this.");
  } else {
    // Our parent shell is a content shell. Get the chrome event
    // handler from it and use that for our shell as well.

    docShell->GetChromeEventHandler(getter_AddRefs(chromeEventHandler));
  }

  mDocShell->SetChromeEventHandler(chromeEventHandler);

  // This is nasty, this code (the mDocShell->GetWindow() below)
  // *must* come *after* the above call to
  // mDocShell->SetChromeEventHandler() for the global window to get
  // the right chrome event handler.

  // Tell the window about the frame that hosts it.
  nsCOMPtr<Element> frame_element = mOwnerContent;
  NS_ASSERTION(frame_element, "frame loader owner element not a DOM element!");

  nsCOMPtr<nsPIDOMWindowOuter> win_private(mDocShell->GetWindow());
  nsCOMPtr<nsIBaseWindow> base_win(do_QueryInterface(mDocShell));
  if (win_private) {
    win_private->SetFrameElementInternal(frame_element);

    // Set the opener window if we have one provided here
    if (mOpener) {
      win_private->SetOpenerWindow(mOpener, true);
      mOpener = nullptr;
    }
  }

  // This is kinda whacky, this call doesn't really create anything,
  // but it must be called to make sure things are properly
  // initialized.
  if (NS_FAILED(base_win->Create()) || !win_private) {
    // Do not call Destroy() here. See bug 472312.
    NS_WARNING("Something wrong when creating the docshell for a frameloader!");
    return NS_ERROR_FAILURE;
  }

  if (mIsTopLevelContent &&
      mOwnerContent->IsXULElement(nsGkAtoms::browser) &&
      !mOwnerContent->HasAttr(kNameSpaceID_None, nsGkAtoms::disablehistory)) {
    nsresult rv;
    nsCOMPtr<nsISHistory> sessionHistory =
      do_CreateInstance(NS_SHISTORY_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
    webNav->SetSessionHistory(sessionHistory);


    if (GroupedSHistory::GroupedHistoryEnabled()) {
      mPartialSHistory = new PartialSHistory(this);
      nsCOMPtr<nsISHistoryListener> listener(do_QueryInterface(mPartialSHistory));
      nsCOMPtr<nsIPartialSHistoryListener> partialListener(do_QueryInterface(mPartialSHistory));
      sessionHistory->AddSHistoryListener(listener);
      sessionHistory->SetPartialSHistoryListener(partialListener);
    }
  }

  OriginAttributes attrs;
  if (docShell->ItemType() == mDocShell->ItemType()) {
    attrs = nsDocShell::Cast(docShell)->GetOriginAttributes();
  }

  // Inherit origin attributes from parent document if
  // 1. It's in a content docshell.
  // 2. its nodePrincipal is not a SystemPrincipal.
  // 3. It's not a mozbrowser frame.
  //
  // For example, firstPartyDomain is computed from top-level document, it
  // doesn't exist in the top-level docshell.
  if (parentType == nsIDocShellTreeItem::typeContent &&
      !nsContentUtils::IsSystemPrincipal(doc->NodePrincipal()) &&
      !OwnerIsMozBrowserFrame()) {
    OriginAttributes oa = doc->NodePrincipal()->OriginAttributesRef();

    // Assert on the firstPartyDomain from top-level docshell should be empty
    MOZ_ASSERT_IF(mIsTopLevelContent, attrs.mFirstPartyDomain.IsEmpty());

    // So far we want to make sure Inherit doesn't override any other origin
    // attribute than firstPartyDomain.
    MOZ_ASSERT(attrs.mAppId == oa.mAppId,
              "docshell and document should have the same appId attribute.");
    MOZ_ASSERT(attrs.mUserContextId == oa.mUserContextId,
              "docshell and document should have the same userContextId attribute.");
    MOZ_ASSERT(attrs.mInIsolatedMozBrowser == oa.mInIsolatedMozBrowser,
              "docshell and document should have the same inIsolatedMozBrowser attribute.");
    MOZ_ASSERT(attrs.mPrivateBrowsingId == oa.mPrivateBrowsingId,
              "docshell and document should have the same privateBrowsingId attribute.");

    attrs = oa;
  }

  if (OwnerIsMozBrowserFrame()) {
    attrs.mAppId = nsIScriptSecurityManager::NO_APP_ID;
    attrs.mInIsolatedMozBrowser = OwnerIsIsolatedMozBrowserFrame();
    mDocShell->SetFrameType(nsIDocShell::FRAME_TYPE_BROWSER);
  }

  // Apply sandbox flags even if our owner is not an iframe, as this copies
  // flags from our owning content's owning document.
  // Note: ApplySandboxFlags should be called after mDocShell->SetFrameType
  // because we need to get the correct presentation URL in ApplySandboxFlags.
  uint32_t sandboxFlags = 0;
  HTMLIFrameElement* iframe = HTMLIFrameElement::FromContent(mOwnerContent);
  if (iframe) {
    sandboxFlags = iframe->GetSandboxFlags();
  }
  ApplySandboxFlags(sandboxFlags);

  // Grab the userContextId from owner if XUL
  nsresult rv = PopulateUserContextIdFromAttribute(attrs);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  bool isPrivate = false;
  nsCOMPtr<nsILoadContext> parentContext = do_QueryInterface(docShell);
  NS_ENSURE_STATE(parentContext);

  rv = parentContext->GetUsePrivateBrowsing(&isPrivate);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  attrs.SyncAttributesWithPrivateBrowsing(isPrivate);

  if (OwnerIsMozBrowserFrame()) {
    // For inproc frames, set the docshell properties.
    nsAutoString name;
    if (mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, name)) {
      docShell->SetName(name);
    }
    mDocShell->SetFullscreenAllowed(
      mOwnerContent->HasAttr(kNameSpaceID_None, nsGkAtoms::allowfullscreen) ||
      mOwnerContent->HasAttr(kNameSpaceID_None, nsGkAtoms::mozallowfullscreen));
    bool isPrivate = mOwnerContent->HasAttr(kNameSpaceID_None, nsGkAtoms::mozprivatebrowsing);
    if (isPrivate) {
      if (mDocShell->GetHasLoadedNonBlankURI()) {
        nsContentUtils::ReportToConsoleNonLocalized(
          NS_LITERAL_STRING("We should not switch to Private Browsing after loading a document."),
          nsIScriptError::warningFlag,
          NS_LITERAL_CSTRING("mozprivatebrowsing"),
          nullptr);
      } else {
        // This handles the case where a frames private browsing is set by chrome flags
        // and not inherited by its parent.
        attrs.SyncAttributesWithPrivateBrowsing(isPrivate);
      }
    }
  }

  nsDocShell::Cast(mDocShell)->SetOriginAttributes(attrs);

  ReallyLoadFrameScripts();
  InitializeBrowserAPI();

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->NotifyObservers(NS_ISUPPORTS_CAST(nsIFrameLoader*, this),
                        "inprocess-browser-shown", nullptr);
  }

  return NS_OK;
}

void
nsFrameLoader::GetURL(nsString& aURI)
{
  aURI.Truncate();

  if (mOwnerContent->IsHTMLElement(nsGkAtoms::object)) {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::data, aURI);
  } else {
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::src, aURI);
  }
}

nsresult
nsFrameLoader::CheckForRecursiveLoad(nsIURI* aURI)
{
  nsresult rv;

  MOZ_ASSERT(!IsRemoteFrame(),
             "Shouldn't call CheckForRecursiveLoad on remote frames.");

  mDepthTooGreat = false;
  rv = MaybeCreateDocShell();
  if (NS_FAILED(rv)) {
    return rv;
  }
  NS_ASSERTION(mDocShell,
               "MaybeCreateDocShell succeeded, but null mDocShell");
  if (!mDocShell) {
    return NS_ERROR_FAILURE;
  }

  // Check that we're still in the docshell tree.
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  mDocShell->GetTreeOwner(getter_AddRefs(treeOwner));
  NS_WARNING_ASSERTION(treeOwner,
                       "Trying to load a new url to a docshell without owner!");
  NS_ENSURE_STATE(treeOwner);

  if (mDocShell->ItemType() != nsIDocShellTreeItem::typeContent) {
    // No need to do recursion-protection here XXXbz why not??  Do we really
    // trust people not to screw up with non-content docshells?
    return NS_OK;
  }

  // Bug 8065: Don't exceed some maximum depth in content frames
  // (MAX_DEPTH_CONTENT_FRAMES)
  nsCOMPtr<nsIDocShellTreeItem> parentAsItem;
  mDocShell->GetSameTypeParent(getter_AddRefs(parentAsItem));
  int32_t depth = 0;
  while (parentAsItem) {
    ++depth;

    if (depth >= MAX_DEPTH_CONTENT_FRAMES) {
      mDepthTooGreat = true;
      NS_WARNING("Too many nested content frames so giving up");

      return NS_ERROR_UNEXPECTED; // Too deep, give up!  (silently?)
    }

    nsCOMPtr<nsIDocShellTreeItem> temp;
    temp.swap(parentAsItem);
    temp->GetSameTypeParent(getter_AddRefs(parentAsItem));
  }

  // Bug 136580: Check for recursive frame loading excluding about:srcdoc URIs.
  // srcdoc URIs require their contents to be specified inline, so it isn't
  // possible for undesirable recursion to occur without the aid of a
  // non-srcdoc URI,  which this method will block normally.
  // Besides, URI is not enough to guarantee uniqueness of srcdoc documents.
  nsAutoCString buffer;
  rv = aURI->GetScheme(buffer);
  if (NS_SUCCEEDED(rv) && buffer.EqualsLiteral("about")) {
    rv = aURI->GetPath(buffer);
    if (NS_SUCCEEDED(rv) && buffer.EqualsLiteral("srcdoc")) {
      // Duplicates allowed up to depth limits
      return NS_OK;
    }
  }
  int32_t matchCount = 0;
  mDocShell->GetSameTypeParent(getter_AddRefs(parentAsItem));
  while (parentAsItem) {
    // Check the parent URI with the URI we're loading
    nsCOMPtr<nsIWebNavigation> parentAsNav(do_QueryInterface(parentAsItem));
    if (parentAsNav) {
      // Does the URI match the one we're about to load?
      nsCOMPtr<nsIURI> parentURI;
      parentAsNav->GetCurrentURI(getter_AddRefs(parentURI));
      if (parentURI) {
        // Bug 98158/193011: We need to ignore data after the #
        bool equal;
        rv = aURI->EqualsExceptRef(parentURI, &equal);
        NS_ENSURE_SUCCESS(rv, rv);

        if (equal) {
          matchCount++;
          if (matchCount >= MAX_SAME_URL_CONTENT_FRAMES) {
            NS_WARNING("Too many nested content frames have the same url (recursion?) so giving up");
            return NS_ERROR_UNEXPECTED;
          }
        }
      }
    }
    nsCOMPtr<nsIDocShellTreeItem> temp;
    temp.swap(parentAsItem);
    temp->GetSameTypeParent(getter_AddRefs(parentAsItem));
  }

  return NS_OK;
}

nsresult
nsFrameLoader::GetWindowDimensions(nsIntRect& aRect)
{
  // Need to get outer window position here
  nsIDocument* doc = mOwnerContent->GetComposedDoc();
  if (!doc) {
    return NS_ERROR_FAILURE;
  }

  MOZ_RELEASE_ASSERT(!doc->IsResourceDoc(), "We shouldn't even exist");

  nsCOMPtr<nsPIDOMWindowOuter> win = doc->GetWindow();
  if (!win) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShellTreeItem> parentAsItem(win->GetDocShell());
  if (!parentAsItem) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShellTreeOwner> parentOwner;
  if (NS_FAILED(parentAsItem->GetTreeOwner(getter_AddRefs(parentOwner))) ||
      !parentOwner) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin(do_GetInterface(parentOwner));
  treeOwnerAsWin->GetPosition(&aRect.x, &aRect.y);
  treeOwnerAsWin->GetSize(&aRect.width, &aRect.height);
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::UpdatePositionAndSize(nsSubDocumentFrame *aIFrame)
{
  if (IsRemoteFrame()) {
    if (mRemoteBrowser) {
      ScreenIntSize size = aIFrame->GetSubdocumentSize();
      nsIntRect dimensions;
      NS_ENSURE_SUCCESS(GetWindowDimensions(dimensions), NS_ERROR_FAILURE);
      mLazySize = size;
      mRemoteBrowser->UpdateDimensions(dimensions, size);
    }
    return NS_OK;
  }
  UpdateBaseWindowPositionAndSize(aIFrame);
  return NS_OK;
}

void
nsFrameLoader::UpdateBaseWindowPositionAndSize(nsSubDocumentFrame *aIFrame)
{
  nsCOMPtr<nsIDocShell> docShell;
  GetDocShell(getter_AddRefs(docShell));
  nsCOMPtr<nsIBaseWindow> baseWindow(do_QueryInterface(docShell));

  // resize the sub document
  if (baseWindow) {
    int32_t x = 0;
    int32_t y = 0;

    AutoWeakFrame weakFrame(aIFrame);

    baseWindow->GetPosition(&x, &y);

    if (!weakFrame.IsAlive()) {
      // GetPosition() killed us
      return;
    }

    ScreenIntSize size = aIFrame->GetSubdocumentSize();
    mLazySize = size;

    baseWindow->SetPositionAndSize(x, y, size.width, size.height,
                                   nsIBaseWindow::eDelayResize);
  }
}

NS_IMETHODIMP
nsFrameLoader::GetLazyWidth(uint32_t* aLazyWidth)
{
  *aLazyWidth = mLazySize.width;

  nsIFrame* frame = GetPrimaryFrameOfOwningContent();
  if (frame) {
    *aLazyWidth = frame->PresContext()->DevPixelsToIntCSSPixels(*aLazyWidth);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetLazyHeight(uint32_t* aLazyHeight)
{
  *aLazyHeight = mLazySize.height;

  nsIFrame* frame = GetPrimaryFrameOfOwningContent();
  if (frame) {
    *aLazyHeight = frame->PresContext()->DevPixelsToIntCSSPixels(*aLazyHeight);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetEventMode(uint32_t* aEventMode)
{
  *aEventMode = mEventMode;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::SetEventMode(uint32_t aEventMode)
{
  mEventMode = aEventMode;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetClipSubdocument(bool* aResult)
{
  *aResult = mClipSubdocument;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::SetClipSubdocument(bool aClip)
{
  mClipSubdocument = aClip;
  nsIFrame* frame = GetPrimaryFrameOfOwningContent();
  if (frame) {
    frame->InvalidateFrame();
    frame->PresContext()->PresShell()->
      FrameNeedsReflow(frame, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
    nsSubDocumentFrame* subdocFrame = do_QueryFrame(frame);
    if (subdocFrame) {
      nsIFrame* subdocRootFrame = subdocFrame->GetSubdocumentRootFrame();
      if (subdocRootFrame) {
        nsIFrame* subdocRootScrollFrame = subdocRootFrame->PresContext()->PresShell()->
          GetRootScrollFrame();
        if (subdocRootScrollFrame) {
          frame->PresContext()->PresShell()->
            FrameNeedsReflow(frame, nsIPresShell::eResize, NS_FRAME_IS_DIRTY);
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetClampScrollPosition(bool* aResult)
{
  *aResult = mClampScrollPosition;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::SetClampScrollPosition(bool aClamp)
{
  mClampScrollPosition = aClamp;

  // When turning clamping on, make sure the current position is clamped.
  if (aClamp) {
    nsIFrame* frame = GetPrimaryFrameOfOwningContent();
    nsSubDocumentFrame* subdocFrame = do_QueryFrame(frame);
    if (subdocFrame) {
      nsIFrame* subdocRootFrame = subdocFrame->GetSubdocumentRootFrame();
      if (subdocRootFrame) {
        nsIScrollableFrame* subdocRootScrollFrame = subdocRootFrame->PresContext()->PresShell()->
          GetRootScrollFrameAsScrollable();
        if (subdocRootScrollFrame) {
          subdocRootScrollFrame->ScrollTo(subdocRootScrollFrame->GetScrollPosition(), nsIScrollableFrame::INSTANT);
        }
      }
    }
  }
  return NS_OK;
}

static
Tuple<ContentParent*, TabParent*>
GetContentParent(Element* aBrowser)
{
  using ReturnTuple = Tuple<ContentParent*, TabParent*>;

  nsCOMPtr<nsIBrowser> browser = do_QueryInterface(aBrowser);
  if (!browser) {
    return ReturnTuple(nullptr, nullptr);
  }

  nsCOMPtr<nsIFrameLoader> otherLoader;
  browser->GetSameProcessAsFrameLoader(getter_AddRefs(otherLoader));
  if (!otherLoader) {
    return ReturnTuple(nullptr, nullptr);
  }

  TabParent* tabParent = TabParent::GetFrom(otherLoader);
  if (tabParent &&
      tabParent->Manager() &&
      tabParent->Manager()->IsContentParent()) {
    return MakeTuple(tabParent->Manager()->AsContentParent(), tabParent);
  }

  return ReturnTuple(nullptr, nullptr);
}

bool
nsFrameLoader::TryRemoteBrowser()
{
  NS_ASSERTION(!mRemoteBrowser, "TryRemoteBrowser called with a remote browser already?");

  //XXXsmaug Per spec (2014/08/21) frameloader should not work in case the
  //         element isn't in document, only in shadow dom, but that will change
  //         https://www.w3.org/Bugs/Public/show_bug.cgi?id=26365#c0
  nsIDocument* doc = mOwnerContent->GetComposedDoc();
  if (!doc) {
    return false;
  }

  MOZ_RELEASE_ASSERT(!doc->IsResourceDoc(), "We shouldn't even exist");

  if (!doc->IsActive()) {
    // Don't allow subframe loads in non-active documents.
    // (See bug 610571 comment 5.)
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> parentWin = doc->GetWindow();
  if (!parentWin) {
    return false;
  }

  nsCOMPtr<nsIDocShell> parentDocShell = parentWin->GetDocShell();
  if (!parentDocShell) {
    return false;
  }

  TabParent* openingTab = TabParent::GetFrom(parentDocShell->GetOpener());
  RefPtr<ContentParent> openerContentParent;
  RefPtr<TabParent> sameTabGroupAs;

  if (openingTab &&
      openingTab->Manager() &&
      openingTab->Manager()->IsContentParent()) {
    openerContentParent = openingTab->Manager()->AsContentParent();
  }

  // <iframe mozbrowser> gets to skip these checks.
  if (!OwnerIsMozBrowserFrame()) {
    if (parentDocShell->ItemType() != nsIDocShellTreeItem::typeChrome) {
      // Allow about:addon an exception to this rule so it can load remote
      // extension options pages.
      //
      // Note that the new frame's message manager will not be a child of the
      // chrome window message manager, and, the values of window.top and
      // window.parent will be different than they would be for a non-remote
      // frame.
      nsCOMPtr<nsIWebNavigation> parentWebNav;
      nsCOMPtr<nsIURI> aboutAddons;
      nsCOMPtr<nsIURI> parentURI;
      bool equals;
      if (!((parentWebNav = do_GetInterface(parentDocShell)) &&
            NS_SUCCEEDED(NS_NewURI(getter_AddRefs(aboutAddons), "about:addons")) &&
            NS_SUCCEEDED(parentWebNav->GetCurrentURI(getter_AddRefs(parentURI))) &&
            NS_SUCCEEDED(parentURI->EqualsExceptRef(aboutAddons, &equals)) && equals)) {
        return false;
      }
    }

    if (!mOwnerContent->IsXULElement()) {
      return false;
    }

    nsAutoString value;
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::type, value);

    if (!value.LowerCaseEqualsLiteral("content") &&
        !StringBeginsWith(value, NS_LITERAL_STRING("content-"),
                          nsCaseInsensitiveStringComparator())) {
      return false;
    }

    // Try to get the related content parent from our browser element.
    Tie(openerContentParent, sameTabGroupAs) = GetContentParent(mOwnerContent);
  }

  uint32_t chromeFlags = 0;
  nsCOMPtr<nsIDocShellTreeOwner> parentOwner;
  if (NS_FAILED(parentDocShell->GetTreeOwner(getter_AddRefs(parentOwner))) ||
      !parentOwner) {
    return false;
  }
  nsCOMPtr<nsIXULWindow> window(do_GetInterface(parentOwner));
  if (window && NS_FAILED(window->GetChromeFlags(&chromeFlags))) {
    return false;
  }

  PROFILER_LABEL("nsFrameLoader", "CreateRemoteBrowser",
    js::ProfileEntry::Category::OTHER);

  MutableTabContext context;
  nsresult rv = GetNewTabContext(&context);
  NS_ENSURE_SUCCESS(rv, false);

  uint64_t nextTabParentId = 0;
  if (mOwnerContent) {
    nsAutoString nextTabParentIdAttr;
    mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::nextTabParentId,
                           nextTabParentIdAttr);
    nextTabParentId = strtoull(NS_ConvertUTF16toUTF8(nextTabParentIdAttr).get(),
                               nullptr, 10);

    // We may be in a window that was just opened, so try the
    // nsIBrowserDOMWindow API as a backup.
    if (!nextTabParentId && window) {
      Unused << window->GetNextTabParentId(&nextTabParentId);
    }
  }

  nsCOMPtr<Element> ownerElement = mOwnerContent;
  mRemoteBrowser = ContentParent::CreateBrowser(context, ownerElement,
                                                openerContentParent,
                                                sameTabGroupAs,
                                                nextTabParentId);
  if (!mRemoteBrowser) {
    return false;
  }
  // Now that mRemoteBrowser is set, we can initialize the RenderFrameParent
  mRemoteBrowser->InitRenderFrame();

  MaybeUpdatePrimaryTabParent(eTabParentChanged);

  mChildID = mRemoteBrowser->Manager()->ChildID();

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  parentDocShell->GetRootTreeItem(getter_AddRefs(rootItem));
  nsCOMPtr<nsPIDOMWindowOuter> rootWin = rootItem->GetWindow();
  nsCOMPtr<nsIDOMChromeWindow> rootChromeWin = do_QueryInterface(rootWin);

  if (rootChromeWin) {
    nsCOMPtr<nsIBrowserDOMWindow> browserDOMWin;
    rootChromeWin->GetBrowserDOMWindow(getter_AddRefs(browserDOMWin));
    mRemoteBrowser->SetBrowserDOMWindow(browserDOMWin);
  }

  ReallyLoadFrameScripts();
  InitializeBrowserAPI();

 return true;
}

mozilla::dom::PBrowserParent*
nsFrameLoader::GetRemoteBrowser() const
{
  return mRemoteBrowser;
}

RenderFrameParent*
nsFrameLoader::GetCurrentRenderFrame() const
{
  if (mRemoteBrowser) {
    return mRemoteBrowser->GetRenderFrame();
  }
  return nullptr;
}

NS_IMETHODIMP
nsFrameLoader::ActivateRemoteFrame() {
  if (mRemoteBrowser) {
    mRemoteBrowser->Activate();
    return NS_OK;
  }
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrameLoader::DeactivateRemoteFrame() {
  if (mRemoteBrowser) {
    mRemoteBrowser->Deactivate();
    return NS_OK;
  }
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrameLoader::SendCrossProcessMouseEvent(const nsAString& aType,
                                          float aX,
                                          float aY,
                                          int32_t aButton,
                                          int32_t aClickCount,
                                          int32_t aModifiers,
                                          bool aIgnoreRootScrollFrame)
{
  if (mRemoteBrowser) {
    mRemoteBrowser->SendMouseEvent(aType, aX, aY, aButton,
                                   aClickCount, aModifiers,
                                   aIgnoreRootScrollFrame);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFrameLoader::ActivateFrameEvent(const nsAString& aType,
                                  bool aCapture)
{
  if (mRemoteBrowser) {
    return mRemoteBrowser->SendActivateFrameEvent(nsString(aType), aCapture) ?
      NS_OK : NS_ERROR_NOT_AVAILABLE;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsFrameLoader::SendCrossProcessKeyEvent(const nsAString& aType,
                                        int32_t aKeyCode,
                                        int32_t aCharCode,
                                        int32_t aModifiers,
                                        bool aPreventDefault)
{
  if (mRemoteBrowser) {
    mRemoteBrowser->SendKeyEvent(aType, aKeyCode, aCharCode, aModifiers,
                                 aPreventDefault);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsFrameLoader::CreateStaticClone(nsIFrameLoader* aDest)
{
  nsFrameLoader* dest = static_cast<nsFrameLoader*>(aDest);
  dest->MaybeCreateDocShell();
  NS_ENSURE_STATE(dest->mDocShell);

  nsCOMPtr<nsIDocument> kungFuDeathGrip = dest->mDocShell->GetDocument();
  Unused << kungFuDeathGrip;

  nsCOMPtr<nsIContentViewer> viewer;
  dest->mDocShell->GetContentViewer(getter_AddRefs(viewer));
  NS_ENSURE_STATE(viewer);

  nsCOMPtr<nsIDocShell> origDocShell;
  GetDocShell(getter_AddRefs(origDocShell));
  NS_ENSURE_STATE(origDocShell);

  nsCOMPtr<nsIDocument> doc = origDocShell->GetDocument();
  NS_ENSURE_STATE(doc);

  nsCOMPtr<nsIDocument> clonedDoc = doc->CreateStaticClone(dest->mDocShell);
  nsCOMPtr<nsIDOMDocument> clonedDOMDoc = do_QueryInterface(clonedDoc);

  viewer->SetDOMDocument(clonedDOMDoc);
  return NS_OK;
}

bool
nsFrameLoader::DoLoadMessageManagerScript(const nsAString& aURL, bool aRunInGlobalScope)
{
  auto* tabParent = TabParent::GetFrom(GetRemoteBrowser());
  if (tabParent) {
    return tabParent->SendLoadRemoteScript(nsString(aURL), aRunInGlobalScope);
  }
  RefPtr<nsInProcessTabChildGlobal> tabChild =
    static_cast<nsInProcessTabChildGlobal*>(GetTabChildGlobalAsEventTarget());
  if (tabChild) {
    tabChild->LoadFrameScript(aURL, aRunInGlobalScope);
  }
  return true;
}

class nsAsyncMessageToChild : public nsSameProcessAsyncMessageBase,
                              public Runnable
{
public:
  nsAsyncMessageToChild(JS::RootingContext* aRootingCx,
                        JS::Handle<JSObject*> aCpows,
                        nsFrameLoader* aFrameLoader)
    : nsSameProcessAsyncMessageBase(aRootingCx, aCpows)
    , mFrameLoader(aFrameLoader)
  {
  }

  NS_IMETHOD Run() override
  {
    nsInProcessTabChildGlobal* tabChild =
      static_cast<nsInProcessTabChildGlobal*>(mFrameLoader->mChildMessageManager.get());
    // Since bug 1126089, messages can arrive even when the docShell is destroyed.
    // Here we make sure that those messages are not delivered.
    if (tabChild && tabChild->GetInnerManager() && mFrameLoader->GetExistingDocShell()) {
      nsCOMPtr<nsIXPConnectJSObjectHolder> kungFuDeathGrip(tabChild->GetGlobal());
      ReceiveMessage(static_cast<EventTarget*>(tabChild), mFrameLoader,
                     tabChild->GetInnerManager());
    }
    return NS_OK;
  }
  RefPtr<nsFrameLoader> mFrameLoader;
};

nsresult
nsFrameLoader::DoSendAsyncMessage(JSContext* aCx,
                                  const nsAString& aMessage,
                                  StructuredCloneData& aData,
                                  JS::Handle<JSObject *> aCpows,
                                  nsIPrincipal* aPrincipal)
{
  TabParent* tabParent = mRemoteBrowser;
  if (tabParent) {
    ClonedMessageData data;
    nsIContentParent* cp = tabParent->Manager();
    if (!BuildClonedMessageDataForParent(cp, aData, data)) {
      MOZ_CRASH();
      return NS_ERROR_DOM_DATA_CLONE_ERR;
    }
    InfallibleTArray<mozilla::jsipc::CpowEntry> cpows;
    jsipc::CPOWManager* mgr = cp->GetCPOWManager();
    if (aCpows && (!mgr || !mgr->Wrap(aCx, aCpows, &cpows))) {
      return NS_ERROR_UNEXPECTED;
    }
    if (tabParent->SendAsyncMessage(nsString(aMessage), cpows,
                                    IPC::Principal(aPrincipal), data)) {
      return NS_OK;
    } else {
      return NS_ERROR_UNEXPECTED;
    }
  }

  if (mChildMessageManager) {
    JS::RootingContext* rcx = JS::RootingContext::get(aCx);
    RefPtr<nsAsyncMessageToChild> ev = new nsAsyncMessageToChild(rcx, aCpows, this);
    nsresult rv = ev->Init(aMessage, aData, aPrincipal);
    if (NS_FAILED(rv)) {
      return rv;
    }
    rv = NS_DispatchToCurrentThread(ev);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return rv;
  }

  // We don't have any targets to send our asynchronous message to.
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsFrameLoader::GetMessageManager(nsIMessageSender** aManager)
{
  EnsureMessageManager();
  if (mMessageManager) {
    RefPtr<nsFrameMessageManager> mm(mMessageManager);
    mm.forget(aManager);
    return NS_OK;
  }
  return NS_OK;
}

nsresult
nsFrameLoader::EnsureMessageManager()
{
  NS_ENSURE_STATE(mOwnerContent);

  if (mMessageManager) {
    return NS_OK;
  }

  if (!mIsTopLevelContent &&
      !OwnerIsMozBrowserFrame() &&
      !IsRemoteFrame() &&
      !(mOwnerContent->IsXULElement() &&
        mOwnerContent->AttrValueIs(kNameSpaceID_None,
                                   nsGkAtoms::forcemessagemanager,
                                   nsGkAtoms::_true, eCaseMatters))) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMChromeWindow> chromeWindow =
    do_QueryInterface(GetOwnerDoc()->GetWindow());
  nsCOMPtr<nsIMessageBroadcaster> parentManager;

  if (chromeWindow) {
    nsAutoString messagemanagergroup;
    if (mOwnerContent->IsXULElement() &&
        mOwnerContent->GetAttr(kNameSpaceID_None,
                               nsGkAtoms::messagemanagergroup,
                               messagemanagergroup)) {
      chromeWindow->GetGroupMessageManager(messagemanagergroup, getter_AddRefs(parentManager));
    }

    if (!parentManager) {
      chromeWindow->GetMessageManager(getter_AddRefs(parentManager));
    }
  } else {
    parentManager = do_GetService("@mozilla.org/globalmessagemanager;1");
  }

  mMessageManager = new nsFrameMessageManager(nullptr,
                                              static_cast<nsFrameMessageManager*>(parentManager.get()),
                                              MM_CHROME);
  if (!IsRemoteFrame()) {
    nsresult rv = MaybeCreateDocShell();
    if (NS_FAILED(rv)) {
      return rv;
    }
    NS_ASSERTION(mDocShell,
                 "MaybeCreateDocShell succeeded, but null mDocShell");
    if (!mDocShell) {
      return NS_ERROR_FAILURE;
    }
    mChildMessageManager =
      new nsInProcessTabChildGlobal(mDocShell, mOwnerContent, mMessageManager);
  }
  return NS_OK;
}

nsresult
nsFrameLoader::ReallyLoadFrameScripts()
{
  nsresult rv = EnsureMessageManager();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (mMessageManager) {
    mMessageManager->InitWithCallback(this);
  }
  return NS_OK;
}

EventTarget*
nsFrameLoader::GetTabChildGlobalAsEventTarget()
{
  return static_cast<nsInProcessTabChildGlobal*>(mChildMessageManager.get());
}

NS_IMETHODIMP
nsFrameLoader::GetOwnerElement(nsIDOMElement **aElement)
{
  nsCOMPtr<nsIDOMElement> ownerElement = do_QueryInterface(mOwnerContent);
  ownerElement.forget(aElement);
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetChildID(uint64_t* aChildID)
{
  *aChildID = mChildID;
  return NS_OK;
}

void
nsFrameLoader::SetRemoteBrowser(nsITabParent* aTabParent)
{
  MOZ_ASSERT(!mRemoteBrowser);
  mRemoteFrame = true;
  mRemoteBrowser = TabParent::GetFrom(aTabParent);
  mChildID = mRemoteBrowser ? mRemoteBrowser->Manager()->ChildID() : 0;
  MaybeUpdatePrimaryTabParent(eTabParentChanged);
  ReallyLoadFrameScripts();
  InitializeBrowserAPI();
  ShowRemoteFrame(ScreenIntSize(0, 0));
}

void
nsFrameLoader::SetDetachedSubdocFrame(nsIFrame* aDetachedFrame,
                                      nsIDocument* aContainerDoc)
{
  mDetachedSubdocFrame = aDetachedFrame;
  mContainerDocWhileDetached = aContainerDoc;
}

nsIFrame*
nsFrameLoader::GetDetachedSubdocFrame(nsIDocument** aContainerDoc) const
{
  NS_IF_ADDREF(*aContainerDoc = mContainerDocWhileDetached);
  return mDetachedSubdocFrame.GetFrame();
}

void
nsFrameLoader::ApplySandboxFlags(uint32_t sandboxFlags)
{
  if (mDocShell) {
    uint32_t parentSandboxFlags = mOwnerContent->OwnerDoc()->GetSandboxFlags();

    // The child can only add restrictions, never remove them.
    sandboxFlags |= parentSandboxFlags;

    // If this frame is a receiving browsing context, we should add
    // sandboxed auxiliary navigation flag to sandboxFlags. See
    // https://w3c.github.io/presentation-api/#creating-a-receiving-browsing-context
    nsAutoString presentationURL;
    nsContentUtils::GetPresentationURL(mDocShell, presentationURL);
    if (!presentationURL.IsEmpty()) {
      sandboxFlags |= SANDBOXED_AUXILIARY_NAVIGATION;
    }
    mDocShell->SetSandboxFlags(sandboxFlags);
  }
}

/* virtual */ void
nsFrameLoader::AttributeChanged(nsIDocument* aDocument,
                                mozilla::dom::Element* aElement,
                                int32_t      aNameSpaceID,
                                nsIAtom*     aAttribute,
                                int32_t      aModType,
                                const nsAttrValue* aOldValue)
{
  MOZ_ASSERT(mObservingOwnerContent);

  if (aNameSpaceID != kNameSpaceID_None ||
      (aAttribute != TypeAttrName() && aAttribute != nsGkAtoms::primary)) {
    return;
  }

  if (aElement != mOwnerContent) {
    return;
  }

  // Note: This logic duplicates a lot of logic in
  // MaybeCreateDocshell.  We should fix that.

  // Notify our enclosing chrome that our type has changed.  We only do this
  // if our parent is chrome, since in all other cases we're random content
  // subframes and the treeowner shouldn't worry about us.
  if (!mDocShell) {
    MaybeUpdatePrimaryTabParent(eTabParentChanged);
    return;
  }

  nsCOMPtr<nsIDocShellTreeItem> parentItem;
  mDocShell->GetParent(getter_AddRefs(parentItem));
  if (!parentItem) {
    return;
  }

  if (parentItem->ItemType() != nsIDocShellTreeItem::typeChrome) {
    return;
  }

  nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
  parentItem->GetTreeOwner(getter_AddRefs(parentTreeOwner));
  if (!parentTreeOwner) {
    return;
  }

  bool is_primary =
    aElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::primary, nsGkAtoms::_true, eIgnoreCase);

#ifdef MOZ_XUL
  // when a content panel is no longer primary, hide any open popups it may have
  if (!is_primary) {
    nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
    if (pm)
      pm->HidePopupsInDocShell(mDocShell);
  }
#endif

  parentTreeOwner->ContentShellRemoved(mDocShell);

  nsAutoString value;
  aElement->GetAttr(kNameSpaceID_None, TypeAttrName(), value);

  if (value.LowerCaseEqualsLiteral("content") ||
      StringBeginsWith(value, NS_LITERAL_STRING("content-"),
                       nsCaseInsensitiveStringComparator())) {
    parentTreeOwner->ContentShellAdded(mDocShell, is_primary);
  }
}

/**
 * Send the RequestNotifyAfterRemotePaint message to the current Tab.
 */
NS_IMETHODIMP
nsFrameLoader::RequestNotifyAfterRemotePaint()
{
  // If remote browsing (e10s), handle this with the TabParent.
  if (mRemoteBrowser) {
    Unused << mRemoteBrowser->SendRequestNotifyAfterRemotePaint();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::RequestFrameLoaderClose()
{
  nsCOMPtr<nsIBrowser> browser = do_QueryInterface(mOwnerContent);
  if (NS_WARN_IF(!browser)) {
    // OwnerElement other than nsIBrowser is not supported yet.
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return browser->CloseBrowser();
}

NS_IMETHODIMP
nsFrameLoader::Print(uint64_t aOuterWindowID,
                     nsIPrintSettings* aPrintSettings,
                     nsIWebProgressListener* aProgressListener)
{
#if defined(NS_PRINTING)
  if (mRemoteBrowser) {
    RefPtr<embedding::PrintingParent> printingParent =
      mRemoteBrowser->Manager()->AsContentParent()->GetPrintingParent();

    embedding::PrintData printData;
    nsresult rv = printingParent->SerializeAndEnsureRemotePrintJob(
      aPrintSettings, aProgressListener, nullptr, &printData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    bool success = mRemoteBrowser->SendPrint(aOuterWindowID, printData);
    return success ? NS_OK : NS_ERROR_FAILURE;
  }

  nsGlobalWindow* outerWindow =
    nsGlobalWindow::GetOuterWindowWithId(aOuterWindowID);
  if (NS_WARN_IF(!outerWindow)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWebBrowserPrint> webBrowserPrint =
    do_GetInterface(outerWindow->AsOuter());
  if (NS_WARN_IF(!webBrowserPrint)) {
    return NS_ERROR_FAILURE;
  }

  return webBrowserPrint->Print(aPrintSettings, aProgressListener);
#endif
  return NS_OK;
}

/* [infallible] */ NS_IMETHODIMP
nsFrameLoader::SetVisible(bool aVisible)
{
  if (mVisible == aVisible) {
    return NS_OK;
  }

  mVisible = aVisible;
  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (os) {
    os->NotifyObservers(NS_ISUPPORTS_CAST(nsIFrameLoader*, this),
                        "frameloader-visible-changed", nullptr);
  }
  return NS_OK;
}

/* [infallible] */ NS_IMETHODIMP
nsFrameLoader::GetVisible(bool* aVisible)
{
  *aVisible = mVisible;
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetTabParent(nsITabParent** aTabParent)
{
  nsCOMPtr<nsITabParent> tp = mRemoteBrowser;
  tp.forget(aTabParent);
  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetLoadContext(nsILoadContext** aLoadContext)
{
  nsCOMPtr<nsILoadContext> loadContext;
  if (IsRemoteFrame() &&
      (mRemoteBrowser || TryRemoteBrowser())) {
    loadContext = mRemoteBrowser->GetLoadContext();
  } else {
    nsCOMPtr<nsIDocShell> docShell;
    GetDocShell(getter_AddRefs(docShell));
    loadContext = do_GetInterface(docShell);
  }
  loadContext.forget(aLoadContext);
  return NS_OK;
}

void
nsFrameLoader::InitializeBrowserAPI()
{
  if (!OwnerIsMozBrowserFrame()) {
    return;
  }
  if (!IsRemoteFrame()) {
    nsresult rv = EnsureMessageManager();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
    if (mMessageManager) {
      mMessageManager->LoadFrameScript(
        NS_LITERAL_STRING("chrome://global/content/BrowserElementChild.js"),
        /* allowDelayedLoad = */ true,
        /* aRunInGlobalScope */ true);
    }
  }
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwnerContent);
  if (browserFrame) {
    browserFrame->InitializeBrowserAPI();
  }
}

void
nsFrameLoader::DestroyBrowserFrameScripts()
{
  if (!OwnerIsMozBrowserFrame()) {
    return;
  }
  nsCOMPtr<nsIMozBrowserFrame> browserFrame = do_QueryInterface(mOwnerContent);
  if (browserFrame) {
    browserFrame->DestroyBrowserFrameScripts();
  }
}

NS_IMETHODIMP
nsFrameLoader::StartPersistence(uint64_t aOuterWindowID,
                                nsIWebBrowserPersistDocumentReceiver* aRecv)
{
  if (!aRecv) {
    return NS_ERROR_INVALID_POINTER;
  }

  if (mRemoteBrowser) {
    return mRemoteBrowser->StartPersistence(aOuterWindowID, aRecv);
  }

  nsCOMPtr<nsIDocument> rootDoc =
    mDocShell ? mDocShell->GetDocument() : nullptr;
  nsCOMPtr<nsIDocument> foundDoc;
  if (aOuterWindowID) {
    foundDoc = nsContentUtils::GetSubdocumentWithOuterWindowId(rootDoc, aOuterWindowID);
  } else {
    foundDoc = rootDoc;
  }

  if (!foundDoc) {
    aRecv->OnError(NS_ERROR_NO_CONTENT);
  } else {
    nsCOMPtr<nsIWebBrowserPersistDocument> pdoc =
      new mozilla::WebBrowserPersistLocalDocument(foundDoc);
    aRecv->OnDocumentReady(pdoc);
  }
  return NS_OK;
}

void
nsFrameLoader::MaybeUpdatePrimaryTabParent(TabParentChange aChange)
{
  if (mRemoteBrowser && mOwnerContent) {
    nsCOMPtr<nsIDocShell> docShell = mOwnerContent->OwnerDoc()->GetDocShell();
    if (!docShell) {
      return;
    }

    int32_t parentType = docShell->ItemType();
    if (parentType != nsIDocShellTreeItem::typeChrome) {
      return;
    }

    nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner;
    docShell->GetTreeOwner(getter_AddRefs(parentTreeOwner));
    if (!parentTreeOwner) {
      return;
    }

    if (!mObservingOwnerContent) {
      mOwnerContent->AddMutationObserver(this);
      mObservingOwnerContent = true;
    }

    parentTreeOwner->TabParentRemoved(mRemoteBrowser);
    if (aChange == eTabParentChanged) {
      bool isPrimary =
        mOwnerContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::primary,
                                   nsGkAtoms::_true, eIgnoreCase);
      parentTreeOwner->TabParentAdded(mRemoteBrowser, isPrimary);
    }
  }
}

nsresult
nsFrameLoader::GetNewTabContext(MutableTabContext* aTabContext,
                                nsIURI* aURI)
{
  OriginAttributes attrs;
  attrs.mInIsolatedMozBrowser = OwnerIsIsolatedMozBrowserFrame();
  nsresult rv;

  attrs.mAppId = nsIScriptSecurityManager::NO_APP_ID;

  // set the userContextId on the attrs before we pass them into
  // the tab context
  rv = PopulateUserContextIdFromAttribute(attrs);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString presentationURLStr;
  mOwnerContent->GetAttr(kNameSpaceID_None,
                         nsGkAtoms::mozpresentation,
                         presentationURLStr);

  nsCOMPtr<nsIDocShell> docShell = mOwnerContent->OwnerDoc()->GetDocShell();
  nsCOMPtr<nsILoadContext> parentContext = do_QueryInterface(docShell);
  NS_ENSURE_STATE(parentContext);

  bool isPrivate = parentContext->UsePrivateBrowsing();
  attrs.SyncAttributesWithPrivateBrowsing(isPrivate);

  UIStateChangeType showAccelerators = UIStateChangeType_NoChange;
  UIStateChangeType showFocusRings = UIStateChangeType_NoChange;
  nsIDocument* doc = mOwnerContent->OwnerDoc();
  if (doc) {
    nsCOMPtr<nsPIWindowRoot> root = nsContentUtils::GetWindowRoot(doc);
    if (root) {
      showAccelerators =
        root->ShowAccelerators() ? UIStateChangeType_Set : UIStateChangeType_Clear;
      showFocusRings =
        root->ShowFocusRings() ? UIStateChangeType_Set : UIStateChangeType_Clear;
    }
  }

  bool tabContextUpdated =
    aTabContext->SetTabContext(OwnerIsMozBrowserFrame(),
                               mIsPrerendered,
                               showAccelerators,
                               showFocusRings,
                               attrs,
                               presentationURLStr);
  NS_ENSURE_STATE(tabContextUpdated);

  return NS_OK;
}

nsresult
nsFrameLoader::PopulateUserContextIdFromAttribute(OriginAttributes& aAttr)
{
  if (aAttr.mUserContextId ==
        nsIScriptSecurityManager::DEFAULT_USER_CONTEXT_ID)  {
    // Grab the userContextId from owner if XUL
    nsAutoString userContextIdStr;
    int32_t namespaceID = mOwnerContent->GetNameSpaceID();
    if ((namespaceID == kNameSpaceID_XUL) &&
        mOwnerContent->GetAttr(kNameSpaceID_None, nsGkAtoms::usercontextid,
                               userContextIdStr) &&
        !userContextIdStr.IsEmpty()) {
      nsresult rv;
      aAttr.mUserContextId = userContextIdStr.ToInteger(&rv);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFrameLoader::GetIsDead(bool* aIsDead)
{
  *aIsDead = mDestroyCalled;
  return NS_OK;
}

nsIMessageSender*
nsFrameLoader::GetProcessMessageManager() const
{
  return mRemoteBrowser ? mRemoteBrowser->Manager()->GetMessageManager()
                        : nullptr;
};
