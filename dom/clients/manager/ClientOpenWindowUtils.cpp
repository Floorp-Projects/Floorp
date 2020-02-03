/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientOpenWindowUtils.h"

#include "ClientInfo.h"
#include "ClientState.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/ResultExtensions.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIDOMChromeWindow.h"
#include "nsIURI.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIWindowWatcher.h"
#include "nsIXPConnect.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowWatcher.h"
#include "nsPrintfCString.h"

#include "mozilla/dom/nsCSPContext.h"

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

  WebProgressListener(nsPIDOMWindowOuter* aWindow, nsIURI* aBaseURI,
                      already_AddRefed<ClientOpPromise::Private> aPromise)
      : mPromise(aPromise), mWindow(aWindow), mBaseURI(aBaseURI) {
    MOZ_ASSERT(aWindow);
    MOZ_ASSERT(aBaseURI);
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD
  OnStateChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                uint32_t aStateFlags, nsresult aStatus) override {
    if (!(aStateFlags & STATE_IS_DOCUMENT) ||
        !(aStateFlags & (STATE_STOP | STATE_TRANSFERRING))) {
      return NS_OK;
    }

    // Our caller keeps a strong reference, so it is safe to remove the listener
    // from ServiceWorkerPrivate.
    aWebProgress->RemoveProgressListener(this);

    nsCOMPtr<Document> doc = mWindow->GetExtantDoc();
    if (NS_WARN_IF(!doc)) {
      CopyableErrorResult rv;
      rv.ThrowInvalidStateError("Document is discarded");
      mPromise->Reject(rv, __func__);
      mPromise = nullptr;
      return NS_OK;
    }

    // Check same origin.
    nsCOMPtr<nsIScriptSecurityManager> securityManager =
        nsContentUtils::GetSecurityManager();
    bool isPrivateWin =
        doc->NodePrincipal()->OriginAttributesRef().mPrivateBrowsingId > 0;
    nsresult rv = securityManager->CheckSameOriginURI(
        doc->GetOriginalURI(), mBaseURI, false, isPrivateWin);
    if (NS_FAILED(rv)) {
      mPromise->Resolve(CopyableErrorResult(), __func__);
      mPromise = nullptr;
      return NS_OK;
    }

    Maybe<ClientInfo> info(doc->GetClientInfo());
    Maybe<ClientState> state(doc->GetClientState());

    if (NS_WARN_IF(info.isNothing() || state.isNothing())) {
      // XXXbz Can we find a useful error message here?
      CopyableErrorResult rv;
      rv.Throw(NS_ERROR_FAILURE);
      mPromise->Reject(rv, __func__);
      mPromise = nullptr;
      return NS_OK;
    }

    mPromise->Resolve(
        ClientInfoAndState(info.ref().ToIPC(), state.ref().ToIPC()), __func__);
    mPromise = nullptr;

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
  // TODO: make window a weak ref and stop cycle collecting
  nsCOMPtr<nsPIDOMWindowOuter> mWindow;
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
    aRv.ThrowTypeError(NS_ConvertUTF8toUTF16(err));
    return;
  }

  rv = NS_NewURI(getter_AddRefs(uri), aArgs.url(), nullptr, baseURI);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    nsPrintfCString err("Invalid URL \"%s\"", aArgs.url().get());
    aRv.ThrowTypeError(NS_ConvertUTF8toUTF16(err));
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
  if (XRE_IsContentProcess()) {
    // Let's create a sandbox in order to have a valid JSContext and correctly
    // propagate the SubjectPrincipal.
    AutoJSAPI jsapi;
    jsapi.Init();

    JSContext* cx = jsapi.cx();

    nsIXPConnect* xpc = nsContentUtils::XPConnect();
    MOZ_DIAGNOSTIC_ASSERT(xpc);

    JS::Rooted<JSObject*> sandbox(cx);
    rv = xpc->CreateSandbox(cx, principal, sandbox.address());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.ThrowTypeError(u"Unable to open window");
      return;
    }

    JSAutoRealm ar(cx, sandbox);

    // ContentProcess
    nsCOMPtr<nsIWindowWatcher> wwatch =
        do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.ThrowTypeError(u"Unable to open window");
      return;
    }
    nsCOMPtr<nsPIWindowWatcher> pwwatch(do_QueryInterface(wwatch));
    if (NS_WARN_IF(!pwwatch)) {
      aRv.ThrowTypeError(u"Unable to open window");
      return;
    }

    nsCString spec;
    rv = uri->GetSpec(spec);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.ThrowTypeError(u"Unable to open window");
      return;
    }

    rv = pwwatch->OpenWindow2(
        nullptr, spec.get(), nullptr, nullptr, false, false, true, nullptr,
        // Not a spammy popup; we got permission, we swear!
        /* aIsPopupSpam = */ false,
        // Don't force noopener.  We're not passing in an
        // opener anyway, and we _do_ want the returned
        // window.
        /* aForceNoOpener = */ false,
        /* aForceNoReferrer = */ false,
        /* aLoadInfp = */ nullptr, aBC);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aRv.ThrowTypeError(u"Unable to open window");
      return;
    }

    MOZ_DIAGNOSTIC_ASSERT(*aBC);
    return;
  }

  // Find the most recent browser window and open a new tab in it.
  nsCOMPtr<nsPIDOMWindowOuter> browserWindow =
      nsContentUtils::GetMostRecentNonPBWindow();
  if (!browserWindow) {
    // It is possible to be running without a browser window on Mac OS, so
    // we need to open a new chrome window.
    // TODO(catalinb): open new chrome window. Bug 1218080
    aRv.ThrowTypeError(u"Unable to open window");
    return;
  }

  nsCOMPtr<nsIDOMChromeWindow> chromeWin = do_QueryInterface(browserWindow);
  if (NS_WARN_IF(!chromeWin)) {
    // XXXbz Can this actually happen?  Seems unlikely.
    aRv.ThrowTypeError(u"Unable to open window");
    return;
  }

  nsCOMPtr<nsIBrowserDOMWindow> bwin;
  chromeWin->GetBrowserDOMWindow(getter_AddRefs(bwin));

  if (NS_WARN_IF(!bwin)) {
    aRv.ThrowTypeError(u"Unable to open window");
    return;
  }

  rv = bwin->OpenURI(uri, nullptr, nsIBrowserDOMWindow::OPEN_DEFAULTWINDOW,
                     nsIBrowserDOMWindow::OPEN_NEW, principal, csp, aBC);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.ThrowTypeError(u"Unable to open window");
    return;
  }
}

void WaitForLoad(const ClientOpenWindowArgs& aArgs,
                 nsPIDOMWindowOuter* aOuterWindow,
                 ClientOpPromise::Private* aPromise) {
  MOZ_DIAGNOSTIC_ASSERT(aOuterWindow);

  RefPtr<ClientOpPromise::Private> promise = aPromise;

  nsFocusManager::FocusWindow(aOuterWindow, CallerType::NonSystem);

  nsCOMPtr<nsIURI> baseURI;
  nsresult rv = NS_NewURI(getter_AddRefs(baseURI), aArgs.baseURL());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // Shouldn't really happen, since we passed in the serialization of a URI.
    CopyableErrorResult result;
    result.ThrowSyntaxError("Bad URL");
    promise->Reject(result, __func__);
    return;
  }

  nsCOMPtr<nsIDocShell> docShell = aOuterWindow->GetDocShell();
  nsCOMPtr<nsIWebProgress> webProgress = do_GetInterface(docShell);

  if (NS_WARN_IF(!webProgress)) {
    CopyableErrorResult rv;
    rv.ThrowInvalidStateError("No browsing context");
    promise->Reject(rv, __func__);
    return;
  }

  RefPtr<ClientOpPromise> ref = promise;

  RefPtr<WebProgressListener> listener =
      new WebProgressListener(aOuterWindow, baseURI, promise.forget());

  rv = webProgress->AddProgressListener(listener,
                                        nsIWebProgress::NOTIFY_STATE_DOCUMENT);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    CopyableErrorResult result;
    // XXXbz Can we throw something better here?
    result.Throw(rv);
    promise->Reject(result, __func__);
    return;
  }

  // Hold the listener alive until the promise settles
  ref->Then(
      aOuterWindow->EventTargetFor(TaskCategory::Other), __func__,
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
      SystemGroup::EventTargetFor(TaskCategory::Other), __func__,
      [aArgs, promise](nsString sessionId) {
        nsresult rv;
        nsCOMPtr<nsIWindowWatcher> wwatch =
            do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          promise->Reject(rv, __func__);
          return rv;
        }

        // Retrieve the window by using the GeckoSession ID. The window is named
        // the same as the ID of the GeckoSession it is associated with.
        nsCOMPtr<mozIDOMWindowProxy> domWindow;
        wwatch->GetWindowByName(sessionId, nullptr, getter_AddRefs(domWindow));
        if (!domWindow) {
          promise->Reject(NS_ERROR_FAILURE, __func__);
          return NS_ERROR_FAILURE;
        }

        nsCOMPtr<nsPIDOMWindowOuter> outerWindow = do_QueryInterface(domWindow);
        if (NS_WARN_IF(!outerWindow)) {
          promise->Reject(NS_ERROR_FAILURE, __func__);
          return NS_ERROR_FAILURE;
        }

        WaitForLoad(aArgs, outerWindow, promise);
        return NS_OK;
      },
      [promise](nsString aResult) {
        promise->Reject(NS_ERROR_FAILURE, __func__);
      });
}

class LaunchObserver final : public nsIObserver {
  RefPtr<GenericPromise::Private> mPromise;

  LaunchObserver() : mPromise(new GenericPromise::Private(__func__)) {}

  ~LaunchObserver() = default;

  NS_IMETHOD
  Observe(nsISupports* aSubject, const char* aTopic,
          const char16_t* aData) override {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->RemoveObserver(this, "BrowserChrome:Ready");
    }
    mPromise->Resolve(true, __func__);
    return NS_OK;
  }

 public:
  static already_AddRefed<LaunchObserver> Create() {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (NS_WARN_IF(!os)) {
      return nullptr;
    }

    RefPtr<LaunchObserver> ref = new LaunchObserver();

    nsresult rv =
        os->AddObserver(ref, "BrowserChrome:Ready", /* weakRef */ false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    return ref.forget();
  }

  void Cancel() {
    nsCOMPtr<nsIObserverService> os = services::GetObserverService();
    if (os) {
      os->RemoveObserver(this, "BrowserChrome:Ready");
    }
    mPromise->Reject(NS_ERROR_ABORT, __func__);
  }

  GenericPromise* Promise() { return mPromise; }

  NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS(LaunchObserver, nsIObserver);

#endif  // MOZ_WIDGET_ANDROID

}  // anonymous namespace

RefPtr<ClientOpPromise> ClientOpenWindowInCurrentProcess(
    const ClientOpenWindowArgs& aArgs) {
  RefPtr<ClientOpPromise::Private> promise =
      new ClientOpPromise::Private(__func__);

#ifdef MOZ_WIDGET_ANDROID
  // If we are on Android we are GeckoView.
  GeckoViewOpenWindow(aArgs, promise);
  return promise.forget();
#endif  // MOZ_WIDGET_ANDROID

  RefPtr<BrowsingContext> bc;
  CopyableErrorResult rv;
  OpenWindow(aArgs, getter_AddRefs(bc), rv);

  nsCOMPtr<nsPIDOMWindowOuter> outerWindow(bc->GetDOMWindow());

  if (NS_WARN_IF(rv.Failed())) {
    promise->Reject(rv, __func__);
    return promise.forget();
  }

  MOZ_DIAGNOSTIC_ASSERT(outerWindow);
  WaitForLoad(aArgs, outerWindow, promise);

  return promise.forget();
}

}  // namespace dom
}  // namespace mozilla
