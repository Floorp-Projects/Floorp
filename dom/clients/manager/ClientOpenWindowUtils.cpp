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
#include "nsOpenWindowInfo.h"

#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/nsCSPContext.h"
#include "mozilla/dom/WindowGlobalParent.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/java/GeckoResultWrappers.h"
#  include "mozilla/java/GeckoRuntimeWrappers.h"
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
        mBaseURI(aBaseURI),
        mBrowserId(aBrowsingContext->GetBrowserId()) {
    MOZ_ASSERT(mBrowserId != 0);
    MOZ_ASSERT(aBaseURI);
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD
  OnStateChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                uint32_t aStateFlags, nsresult aStatus) override {
    if (!(aStateFlags & STATE_IS_WINDOW) ||
        !(aStateFlags & (STATE_STOP | STATE_TRANSFERRING))) {
      return NS_OK;
    }

    // Our browsing context may have been discarded before finishing the load,
    // this is a navigation error.
    RefPtr<CanonicalBrowsingContext> browsingContext =
        CanonicalBrowsingContext::Cast(
            BrowsingContext::GetCurrentTopByBrowserId(mBrowserId));
    if (!browsingContext || browsingContext->IsDiscarded()) {
      CopyableErrorResult rv;
      rv.ThrowInvalidStateError("Unable to open window");
      mPromise->Reject(rv, __func__);
      mPromise = nullptr;
      return NS_OK;
    }

    // Our caller keeps a strong reference, so it is safe to remove the listener
    // from the BrowsingContext's nsIWebProgress.
    nsCOMPtr<nsIWebProgress> webProgress = browsingContext->GetWebProgress();
    webProgress->RemoveProgressListener(this);

    RefPtr<dom::WindowGlobalParent> wgp =
        browsingContext->GetCurrentWindowGlobal();
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
                                   GetCurrentSerialEventTarget())
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
  nsCOMPtr<nsIURI> mBaseURI;
  uint64_t mBrowserId;
};

NS_IMPL_ISUPPORTS(WebProgressListener, nsIWebProgressListener,
                  nsISupportsWeakReference);

struct ClientOpenWindowArgsParsed {
  nsCOMPtr<nsIURI> uri;
  nsCOMPtr<nsIURI> baseURI;
  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
};

void OpenWindow(const ClientOpenWindowArgsParsed& aArgsValidated,
                nsOpenWindowInfo* aOpenInfo, BrowsingContext** aBC,
                ErrorResult& aRv) {
  MOZ_DIAGNOSTIC_ASSERT(aBC);

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
  nsresult rv = bwin->CreateContentWindow(
      nullptr, aOpenInfo, nsIBrowserDOMWindow::OPEN_DEFAULTWINDOW,
      nsIBrowserDOMWindow::OPEN_NEW, aArgsValidated.principal,
      aArgsValidated.csp, aBC);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowTypeError("Unable to open window");
    return;
  }
}

void WaitForLoad(const ClientOpenWindowArgsParsed& aArgsValidated,
                 BrowsingContext* aBrowsingContext,
                 ClientOpPromise::Private* aPromise) {
  MOZ_DIAGNOSTIC_ASSERT(aBrowsingContext);

  RefPtr<ClientOpPromise::Private> promise = aPromise;
  // We can get a WebProgress off of
  // the BrowsingContext for the <xul:browser> to listen for content
  // events. Note that this WebProgress filters out events which don't have
  // STATE_IS_NETWORK or STATE_IS_REDIRECTED_DOCUMENT set on them, and so this
  // listener will only see some web progress events.
  nsCOMPtr<nsIWebProgress> webProgress =
      aBrowsingContext->Canonical()->GetWebProgress();
  if (NS_WARN_IF(!webProgress)) {
    CopyableErrorResult result;
    result.ThrowInvalidStateError("Unable to watch window for navigation");
    promise->Reject(result, __func__);
    return;
  }

  // Add a progress listener before we start the load of the service worker URI
  RefPtr<WebProgressListener> listener = new WebProgressListener(
      aBrowsingContext, aArgsValidated.baseURI, do_AddRef(promise));

  nsresult rv = webProgress->AddProgressListener(
      listener, nsIWebProgress::NOTIFY_STATE_WINDOW);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    CopyableErrorResult result;
    // XXXbz Can we throw something better here?
    result.Throw(rv);
    promise->Reject(result, __func__);
    return;
  }

  // Load the service worker URI
  RefPtr<nsDocShellLoadState> loadState =
      new nsDocShellLoadState(aArgsValidated.uri);
  loadState->SetTriggeringPrincipal(aArgsValidated.principal);
  loadState->SetFirstParty(true);
  loadState->SetLoadFlags(
      nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL);

  rv = aBrowsingContext->LoadURI(loadState, true);
  if (NS_FAILED(rv)) {
    CopyableErrorResult result;
    result.ThrowInvalidStateError("Unable to start the load of the actual URI");
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

void GeckoViewOpenWindow(const ClientOpenWindowArgsParsed& aArgsValidated,
                         ClientOpPromise::Private* aPromise) {
  RefPtr<ClientOpPromise::Private> promise = aPromise;

  // passes the request to open a new window to GeckoView. Allowing the
  // application to decide how to hand the open window request.
  nsAutoCString uri;
  MOZ_ALWAYS_SUCCEEDS(aArgsValidated.uri->GetSpec(uri));
  auto genericResult = java::GeckoRuntime::ServiceWorkerOpenWindow(uri);
  auto typedResult = java::GeckoResult::LocalRef(std::move(genericResult));

  // MozPromise containing the ID for the handling GeckoSession
  auto promiseResult =
      mozilla::MozPromise<nsString, nsString, false>::FromGeckoResult(
          typedResult);

  promiseResult->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [aArgsValidated, promise](nsString sessionId) {
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

        WaitForLoad(aArgsValidated, browsingContext, promise);
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

  // [[1. Let url be the result of parsing url with entry settings object's API
  //   base URL.]]
  nsCOMPtr<nsIURI> baseURI;
  nsresult rv = NS_NewURI(getter_AddRefs(baseURI), aArgs.baseURL());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    nsPrintfCString err("Invalid base URL \"%s\"", aArgs.baseURL().get());
    CopyableErrorResult errResult;
    errResult.ThrowTypeError(err);
    promise->Reject(errResult, __func__);
    return promise;
  }

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), aArgs.url(), nullptr, baseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    nsPrintfCString err("Invalid URL \"%s\"", aArgs.url().get());
    CopyableErrorResult errResult;
    errResult.ThrowTypeError(err);
    promise->Reject(errResult, __func__);
    return promise;
  }

  auto principalOrErr = PrincipalInfoToPrincipal(aArgs.principalInfo());
  if (NS_WARN_IF(principalOrErr.isErr())) {
    CopyableErrorResult errResult;
    errResult.ThrowTypeError("Failed to obtain principal");
    promise->Reject(errResult, __func__);
    return promise;
  }
  nsCOMPtr<nsIPrincipal> principal = principalOrErr.unwrap();
  MOZ_DIAGNOSTIC_ASSERT(principal);

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  if (aArgs.cspInfo().isSome()) {
    csp = CSPInfoToCSP(aArgs.cspInfo().ref(), nullptr);
  }
  ClientOpenWindowArgsParsed argsValidated;
  argsValidated.uri = uri;
  argsValidated.baseURI = baseURI;
  argsValidated.principal = principal;
  argsValidated.csp = csp;

#ifdef MOZ_WIDGET_ANDROID
  // If we are on Android we are GeckoView.
  GeckoViewOpenWindow(argsValidated, promise);
  return promise.forget();
#endif  // MOZ_WIDGET_ANDROID

  RefPtr<BrowsingContextCallbackReceivedPromise::Private>
      browsingContextReadyPromise =
          new BrowsingContextCallbackReceivedPromise::Private(__func__);
  RefPtr<nsIBrowsingContextReadyCallback> callback =
      new nsBrowsingContextReadyCallback(browsingContextReadyPromise);

  RefPtr<nsOpenWindowInfo> openInfo = new nsOpenWindowInfo();
  openInfo->mBrowsingContextReadyCallback = callback;
  openInfo->mOriginAttributes = principal->OriginAttributesRef();
  openInfo->mIsRemote = true;

  RefPtr<BrowsingContext> bc;
  ErrorResult errResult;
  OpenWindow(argsValidated, openInfo, getter_AddRefs(bc), errResult);
  if (NS_WARN_IF(errResult.Failed())) {
    promise->Reject(errResult, __func__);
    return promise;
  }

  browsingContextReadyPromise->Then(
      GetCurrentSerialEventTarget(), __func__,
      [argsValidated, promise](const RefPtr<BrowsingContext>& aBC) {
        WaitForLoad(argsValidated, aBC, promise);
      },
      [promise]() {
        // in case of failure, reject the original promise
        CopyableErrorResult result;
        result.ThrowTypeError("Unable to open window");
        promise->Reject(result, __func__);
      });
  if (bc) {
    browsingContextReadyPromise->Resolve(bc, __func__);
  }
  return promise;
}

}  // namespace dom
}  // namespace mozilla
