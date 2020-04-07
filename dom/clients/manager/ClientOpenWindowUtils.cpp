/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientOpenWindowUtils.h"

#include "ClientInfo.h"
#include "ClientManager.h"
#include "ClientState.h"
#include "mozilla/ResultExtensions.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsFocusManager.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIDOMChromeWindow.h"
#include "nsIURI.h"
#include "nsIBrowser.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIWindowWatcher.h"
#include "nsIXPConnect.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowWatcher.h"
#include "nsPrintfCString.h"
#include "nsWindowWatcher.h"

#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/nsCSPContext.h"
#include "mozilla/dom/WindowGlobalParent.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "GeneratedJNIWrappers.h"
#endif

namespace mozilla {
namespace dom {

namespace {

class WebProgressListener final : public nsIWebProgressListener,
                                  public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS

  WebProgressListener(BrowsingContext* aBrowsingContext, nsIURI* aBaseURI,
                      already_AddRefed<ClientOpPromise::Private> aPromise)
      : mPromise(aPromise),
        mBrowsingContext(aBrowsingContext),
        mBaseURI(aBaseURI) {
    MOZ_ASSERT(aBrowsingContext);
    MOZ_ASSERT(aBaseURI);
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD
  OnStateChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                uint32_t aStateFlags, nsresult aStatus) override {
    MOZ_ASSERT(mBrowsingContext);

    if (!(aStateFlags & STATE_IS_WINDOW) ||
        !(aStateFlags & (STATE_STOP | STATE_TRANSFERRING))) {
      return NS_OK;
    }

    // Our caller keeps a strong reference, so it is safe to remove the listener
    // from ServiceWorkerPrivate.
    aWebProgress->RemoveProgressListener(this);

    // Our browsing context may have been discarded before finishing the load,
    // this is a navigation error.
    if (mBrowsingContext->IsDiscarded()) {
      CopyableErrorResult rv;
      rv.ThrowInvalidStateError("Unable to open window");
      mPromise->Reject(rv, __func__);
      mPromise = nullptr;
      return NS_OK;
    }

    RefPtr<dom::WindowGlobalParent> wgp =
        mBrowsingContext->Canonical()->GetCurrentWindowGlobal();
    if (NS_WARN_IF(!wgp)) {
      CopyableErrorResult rv;
      rv.ThrowInvalidStateError("Unable to open window");
      mPromise->Reject(rv, __func__);
      mPromise = nullptr;
      return NS_OK;
    }

    // Check same origin. If the origins do not match, resolve with null (per
    // step 7.2.7.1 of the openWindow spec).
    nsCOMPtr<nsIScriptSecurityManager> securityManager =
        nsContentUtils::GetSecurityManager();
    bool isPrivateWin =
        wgp->DocumentPrincipal()->OriginAttributesRef().mPrivateBrowsingId > 0;
    nsresult rv = securityManager->CheckSameOriginURI(
        wgp->GetDocumentURI(), mBaseURI, false, isPrivateWin);
    if (NS_FAILED(rv)) {
      mPromise->Resolve(CopyableErrorResult(), __func__);
      mPromise = nullptr;
      return NS_OK;
    }

    Maybe<ClientInfo> info = wgp->GetClientInfo();
    if (info.isNothing()) {
      CopyableErrorResult rv;
      rv.ThrowInvalidStateError("Unable to open window");
      mPromise->Reject(rv, __func__);
      mPromise = nullptr;
      return NS_OK;
    }

    const nsID& id = info.ref().Id();
    const mozilla::ipc::PrincipalInfo& principal = info.ref().PrincipalInfo();
    ClientManager::GetInfoAndState(ClientGetInfoAndStateArgs(id, principal),
                                   GetCurrentThreadSerialEventTarget())
        ->ChainTo(mPromise.forget(), __func__);

    return NS_OK;
  }

  NS_IMETHOD
  OnProgressChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                   int32_t aCurSelfProgress, int32_t aMaxSelfProgress,
                   int32_t aCurTotalProgress,
                   int32_t aMaxTotalProgress) override {
    MOZ_ASSERT(false, "Unexpected notification.");
    return NS_OK;
  }

  NS_IMETHOD
  OnLocationChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                   nsIURI* aLocation, uint32_t aFlags) override {
    MOZ_ASSERT(false, "Unexpected notification.");
    return NS_OK;
  }

  NS_IMETHOD
  OnStatusChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                 nsresult aStatus, const char16_t* aMessage) override {
    MOZ_ASSERT(false, "Unexpected notification.");
    return NS_OK;
  }

  NS_IMETHOD
  OnSecurityChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                   uint32_t aState) override {
    MOZ_ASSERT(false, "Unexpected notification.");
    return NS_OK;
  }

  NS_IMETHOD
  OnContentBlockingEvent(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                         uint32_t aEvent) override {
    MOZ_ASSERT(false, "Unexpected notification.");
    return NS_OK;
  }

 private:
  ~WebProgressListener() {
    if (mPromise) {
      CopyableErrorResult rv;
      rv.ThrowAbortError("openWindow aborted");
      mPromise->Reject(rv, __func__);
      mPromise = nullptr;
    }
  }

  RefPtr<ClientOpPromise::Private> mPromise;
  RefPtr<BrowsingContext> mBrowsingContext;
  nsCOMPtr<nsIURI> mBaseURI;
};

NS_IMPL_ISUPPORTS(WebProgressListener, nsIWebProgressListener,
                  nsISupportsWeakReference);

void OpenWindow(const ClientOpenWindowArgs& aArgs, BrowsingContext** aBC,
                ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(aBC);

  // [[1. Let url be the result of parsing url with entry settings object's API
  //   base URL.]]
  nsCOMPtr<nsIURI> uri;

  nsCOMPtr<nsIURI> baseURI;
  nsresult rv = NS_NewURI(getter_AddRefs(baseURI), aArgs.baseURL());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    nsPrintfCString err("Invalid base URL \"%s\"", aArgs.baseURL().get());
    aRv.ThrowTypeError(err);
    return;
  }

  rv = NS_NewURI(getter_AddRefs(uri), aArgs.url(), nullptr, baseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    nsPrintfCString err("Invalid URL \"%s\"", aArgs.url().get());
    aRv.ThrowTypeError(err);
    return;
  }

  nsCOMPtr<nsIPrincipal> principal =
      PrincipalInfoToPrincipal(aArgs.principalInfo());
  MOZ_DIAGNOSTIC_ASSERT(principal);

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  if (aArgs.cspInfo().isSome()) {
    csp = CSPInfoToCSP(aArgs.cspInfo().ref(), nullptr);
  }

  // [[6.1 Open Window]]

  // Find the most recent browser window and open a new tab in it.
  nsCOMPtr<nsPIDOMWindowOuter> browserWindow =
      nsContentUtils::GetMostRecentNonPBWindow();
  if (!browserWindow) {
    // It is possible to be running without a browser window on Mac OS, so
    // we need to open a new chrome window.
    // TODO(catalinb): open new chrome window. Bug 1218080
    aRv.ThrowTypeError("Unable to open window");
    return;
  }

  nsCOMPtr<nsIDOMChromeWindow> chromeWin = do_QueryInterface(browserWindow);
  if (NS_WARN_IF(!chromeWin)) {
    // XXXbz Can this actually happen?  Seems unlikely.
    aRv.ThrowTypeError("Unable to open window");
    return;
  }

  nsCOMPtr<nsIBrowserDOMWindow> bwin;
  chromeWin->GetBrowserDOMWindow(getter_AddRefs(bwin));

  if (NS_WARN_IF(!bwin)) {
    aRv.ThrowTypeError("Unable to open window");
    return;
  }

  rv = bwin->OpenURI(uri, nullptr, nsIBrowserDOMWindow::OPEN_DEFAULTWINDOW,
                     nsIBrowserDOMWindow::OPEN_NEW, principal, csp, aBC);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowTypeError("Unable to open window");
    return;
  }
}

void WaitForLoad(const ClientOpenWindowArgs& aArgs,
                 BrowsingContext* aBrowsingContext,
                 ClientOpPromise::Private* aPromise) {
  MOZ_DIAGNOSTIC_ASSERT(aBrowsingContext);

  RefPtr<ClientOpPromise::Private> promise = aPromise;

  nsCOMPtr<nsIURI> baseURI;
  nsresult rv = NS_NewURI(getter_AddRefs(baseURI), aArgs.baseURL());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Shouldn't really happen, since we passed in the serialization of a URI.
    CopyableErrorResult result;
    result.ThrowSyntaxError("Bad URL");
    promise->Reject(result, __func__);
    return;
  }

  nsCOMPtr<nsIWebProgress> webProgress;
  if (nsIDocShell* docShell = aBrowsingContext->GetDocShell()) {
    // We're dealing with a non-remote frame. We have access to an nsDocShell,
    // so we can just pull the nsIWebProgress off of that.
    webProgress = nsDocShell::Cast(docShell);
    nsFocusManager::FocusWindow(aBrowsingContext->GetDOMWindow(),
                                CallerType::NonSystem);
  } else {
    // We're dealing with a remote frame. We can get a RemoteWebProgress off of
    // the <xul:browser> that embeds |aBrowsingContext| to listen for content
    // events. Note that RemoteWebProgress filters out events which don't have
    // STATE_IS_NETWORK or STATE_IS_REDIRECTED_DOCUMENT set on them, and so this
    // listener will only see some web progress events.
    nsCOMPtr<Element> element = aBrowsingContext->GetEmbedderElement();
    if (NS_WARN_IF(!element)) {
      CopyableErrorResult result;
      result.ThrowInvalidStateError("Unable to watch window for navigation");
      promise->Reject(result, __func__);
      return;
    }

    nsCOMPtr<nsIBrowser> browser = element->AsBrowser();
    if (NS_WARN_IF(!browser)) {
      CopyableErrorResult result;
      result.ThrowInvalidStateError("Unable to watch window for navigation");
      promise->Reject(result, __func__);
      return;
    }

    rv = browser->GetRemoteWebProgressManager(getter_AddRefs(webProgress));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      CopyableErrorResult result;
      result.ThrowInvalidStateError("Unable to watch window for navigation");
      promise->Reject(result, __func__);
      return;
    }

    if (BrowserParent* browserParent = BrowserParent::GetFrom(element)) {
      browserParent->Activate();
    }
  }

  RefPtr<WebProgressListener> listener =
      new WebProgressListener(aBrowsingContext, baseURI, do_AddRef(promise));

  rv = webProgress->AddProgressListener(listener,
                                        nsIWebProgress::NOTIFY_STATE_WINDOW);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    CopyableErrorResult result;
    // XXXbz Can we throw something better here?
    result.Throw(rv);
    promise->Reject(result, __func__);
    return;
  }

  // Hold the listener alive until the promise settles.
  promise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [listener](const ClientOpResult& aResult) {},
      [listener](const CopyableErrorResult& aResult) {});
}

#ifdef MOZ_WIDGET_ANDROID

void GeckoViewOpenWindow(const ClientOpenWindowArgs& aArgs,
                         ClientOpPromise::Private* aPromise) {
  RefPtr<ClientOpPromise::Private> promise = aPromise;

  // passes the request to open a new window to GeckoView. Allowing the
  // application to decide how to hand the open window request.
  auto genericResult =
      java::GeckoRuntime::ServiceWorkerOpenWindow(aArgs.baseURL(), aArgs.url());
  auto typedResult = java::GeckoResult::LocalRef(std::move(genericResult));

  // MozPromise containing the ID for the handling GeckoSession
  auto promiseResult =
      mozilla::MozPromise<nsString, nsString, false>::FromGeckoResult(
          typedResult);

  promiseResult->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [aArgs, promise](nsString sessionId) {
        nsresult rv;
        nsCOMPtr<nsIWindowWatcher> wwatch =
            do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          promise->Reject(rv, __func__);
          return rv;
        }

        // Retrieve the browsing context by using the GeckoSession ID. The
        // window is named the same as the ID of the GeckoSession it is
        // associated with.
        RefPtr<BrowsingContext> browsingContext =
            static_cast<nsWindowWatcher*>(wwatch.get())
                ->GetBrowsingContextByName(sessionId, false, nullptr);
        if (NS_WARN_IF(!browsingContext)) {
          promise->Reject(NS_ERROR_FAILURE, __func__);
          return NS_ERROR_FAILURE;
        }

        WaitForLoad(aArgs, browsingContext, promise);
        return NS_OK;
      },
      [promise](nsString aResult) {
        promise->Reject(NS_ERROR_FAILURE, __func__);
      });
}

#endif  // MOZ_WIDGET_ANDROID

}  // anonymous namespace

RefPtr<ClientOpPromise> ClientOpenWindow(const ClientOpenWindowArgs& aArgs) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());

  RefPtr<ClientOpPromise::Private> promise =
      new ClientOpPromise::Private(__func__);

#ifdef MOZ_WIDGET_ANDROID
  // If we are on Android we are GeckoView.
  GeckoViewOpenWindow(aArgs, promise);
  return promise.forget();
#endif  // MOZ_WIDGET_ANDROID

  RefPtr<BrowsingContext> bc;
  ErrorResult rv;
  OpenWindow(aArgs, getter_AddRefs(bc), rv);
  if (NS_WARN_IF(rv.Failed())) {
    promise->Reject(rv, __func__);
    return promise;
  }

  MOZ_DIAGNOSTIC_ASSERT(bc);
  WaitForLoad(aArgs, bc, promise);

  return promise;
}

}  // namespace dom
}  // namespace mozilla
