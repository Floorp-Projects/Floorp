/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientNavigateOpChild.h"

#include "ClientState.h"
#include "mozilla/Unused.h"
#include "nsIDocShell.h"
#include "nsDocShellLoadInfo.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

namespace {

class NavigateLoadListener final : public nsIWebProgressListener
                                 , public nsSupportsWeakReference
{
  RefPtr<ClientOpPromise::Private> mPromise;
  RefPtr<nsPIDOMWindowOuter> mOuterWindow;
  nsCOMPtr<nsIURI> mBaseURL;

  ~NavigateLoadListener() = default;

public:
  NavigateLoadListener(ClientOpPromise::Private* aPromise,
                       nsPIDOMWindowOuter* aOuterWindow,
                       nsIURI* aBaseURL)
    : mPromise(aPromise)
    , mOuterWindow(aOuterWindow)
    , mBaseURL(aBaseURL)
  {
    MOZ_DIAGNOSTIC_ASSERT(mPromise);
    MOZ_DIAGNOSTIC_ASSERT(mOuterWindow);
    MOZ_DIAGNOSTIC_ASSERT(mBaseURL);
  }

  NS_IMETHOD
  OnStateChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                uint32_t aStateFlags, nsresult aResult) override
  {
    if (!(aStateFlags & STATE_IS_DOCUMENT) ||
        !(aStateFlags & (STATE_STOP | STATE_TRANSFERRING))) {
      return NS_OK;
    }

    aWebProgress->RemoveProgressListener(this);

    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
    if (!channel) {
      mPromise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
      return NS_OK;
    }

    nsCOMPtr<nsIURI> channelURL;
    nsresult rv = NS_GetFinalChannelURI(channel, getter_AddRefs(channelURL));
    if (NS_FAILED(rv)) {
      mPromise->Reject(rv, __func__);
      return NS_OK;
    }

    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    MOZ_DIAGNOSTIC_ASSERT(ssm);

    // If the resulting window is not same origin, then resolve immediately
    // without returning any information about the new Client.  This is
    // step 6.10 in the Client.navigate(url) spec.
    rv = ssm->CheckSameOriginURI(mBaseURL, channelURL, false);
    if (NS_FAILED(rv)) {
      mPromise->Resolve(NS_OK, __func__);
      return NS_OK;
    }

    nsPIDOMWindowInner* innerWindow = mOuterWindow->GetCurrentInnerWindow();
    MOZ_DIAGNOSTIC_ASSERT(innerWindow);

    Maybe<ClientInfo> clientInfo = innerWindow->GetClientInfo();
    MOZ_DIAGNOSTIC_ASSERT(clientInfo.isSome());

    Maybe<ClientState> clientState = innerWindow->GetClientState();
    MOZ_DIAGNOSTIC_ASSERT(clientState.isSome());

    // Otherwise, if the new window is same-origin we want to return a
    // ClientInfoAndState object so we can provide a Client snapshot
    // to the caller.  This is step 6.11 and 6.12 in the Client.navigate(url)
    // spec.
    mPromise->Resolve(ClientInfoAndState(clientInfo.ref().ToIPC(),
                                         clientState.ref().ToIPC()), __func__);

    return NS_OK;
  }

  NS_IMETHOD
  OnProgressChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                   int32_t aCurSelfProgress, int32_t aMaxSelfProgress,
                   int32_t aCurTotalProgress, int32_t aMaxTotalProgress) override
  {
    MOZ_CRASH("Unexpected notification.");
    return NS_OK;
  }

  NS_IMETHOD
  OnLocationChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                   nsIURI* aLocation, uint32_t aFlags) override
  {
    MOZ_CRASH("Unexpected notification.");
    return NS_OK;
  }

  NS_IMETHOD
  OnStatusChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                 nsresult aStatus, const char16_t* aMessage) override
  {
    MOZ_CRASH("Unexpected notification.");
    return NS_OK;
  }

  NS_IMETHOD
  OnSecurityChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                   uint32_t aState) override
  {
    MOZ_CRASH("Unexpected notification.");
    return NS_OK;
  }

  NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS(NavigateLoadListener, nsIWebProgressListener,
                                        nsISupportsWeakReference);

} // anonymous namespace

already_AddRefed<ClientOpPromise>
ClientNavigateOpChild::DoNavigate(const ClientNavigateOpConstructorArgs& aArgs)
{
  RefPtr<ClientOpPromise> ref;
  nsCOMPtr<nsPIDOMWindowInner> window;

  // Navigating the target client window will result in the original
  // ClientSource being destroyed.  To avoid potential UAF mistakes
  // we use a small scope to access the ClientSource object.  Once
  // we have a strong reference to the window object we should not
  // access the ClientSource again.
  {
    ClientSourceChild* targetActor =
      static_cast<ClientSourceChild*>(aArgs.targetChild());
    MOZ_DIAGNOSTIC_ASSERT(targetActor);

    ClientSource* target = targetActor->GetSource();
    if (!target) {
      ref = ClientOpPromise::CreateAndReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                                             __func__);
      return ref.forget();
    }

    window = target->GetInnerWindow();
    if (!window) {
      ref = ClientOpPromise::CreateAndReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                                             __func__);
      return ref.forget();
    }
  }

  MOZ_ASSERT(NS_IsMainThread());

  mSerialEventTarget = window->EventTargetFor(TaskCategory::Other);

  // In theory we could do the URL work before paying the IPC overhead
  // cost, but in practice its easier to do it here.  The ClientHandle
  // may be off-main-thread while this method is guaranteed to always
  // be main thread.
  nsCOMPtr<nsIURI> baseURL;
  nsresult rv = NS_NewURI(getter_AddRefs(baseURL), aArgs.baseURL());
  if (NS_FAILED(rv)) {
    ref = ClientOpPromise::CreateAndReject(rv, __func__);
    return ref.forget();
  }

  nsCOMPtr<nsIURI> url;
  rv = NS_NewURI(getter_AddRefs(url), aArgs.url(), nullptr, baseURL);
  if (NS_FAILED(rv)) {
    ref = ClientOpPromise::CreateAndReject(rv, __func__);
    return ref.forget();
  }

  if (url->GetSpecOrDefault().EqualsLiteral("about:blank")) {
    ref = ClientOpPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    return ref.forget();
  }

  nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
  if (!doc || !doc->IsActive()) {
    ref = ClientOpPromise::CreateAndReject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
    return ref.forget();
  }

  nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
  if (!principal) {
    ref = ClientOpPromise::CreateAndReject(rv, __func__);
    return ref.forget();
  }

  nsCOMPtr<nsIDocShell> docShell = window->GetDocShell();
  nsCOMPtr<nsIWebProgress> webProgress = do_GetInterface(docShell);
  if (!docShell || !webProgress) {
    ref = ClientOpPromise::CreateAndReject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
    return ref.forget();
  }

  RefPtr<nsDocShellLoadInfo> loadInfo = new nsDocShellLoadInfo();

  loadInfo->SetTriggeringPrincipal(principal);
  loadInfo->SetReferrerPolicy(doc->GetReferrerPolicy());
  loadInfo->SetLoadType(LOAD_STOP_CONTENT);
  loadInfo->SetSourceDocShell(docShell);
  rv = docShell->LoadURI(url, loadInfo, nsIWebNavigation::LOAD_FLAGS_NONE, true);
  if (NS_FAILED(rv)) {
    ref = ClientOpPromise::CreateAndReject(rv, __func__);
    return ref.forget();
  }

  RefPtr<ClientOpPromise::Private> promise =
    new ClientOpPromise::Private(__func__);

  nsCOMPtr<nsIWebProgressListener> listener =
    new NavigateLoadListener(promise, window->GetOuterWindow(), baseURL);

  rv = webProgress->AddProgressListener(listener,
                                        nsIWebProgress::NOTIFY_STATE_DOCUMENT);
  if (NS_FAILED(rv)) {
    promise->Reject(rv, __func__);
    ref = promise;
    return ref.forget();
  }

  ref = promise.get();

  ref->Then(mSerialEventTarget, __func__,
    [listener] (const ClientOpResult& aResult) { },
    [listener] (nsresult aResult) { });

  return ref.forget();
}

void
ClientNavigateOpChild::ActorDestroy(ActorDestroyReason aReason)
{
  mPromiseRequestHolder.DisconnectIfExists();
}

void
ClientNavigateOpChild::Init(const ClientNavigateOpConstructorArgs& aArgs)
{
  RefPtr<ClientOpPromise> promise = DoNavigate(aArgs);

  // Normally we get the event target from the window in DoNavigate().  If a
  // failure occurred, though, we may need to fall back to the current thread
  // target.
  if (!mSerialEventTarget) {
    mSerialEventTarget = GetCurrentThreadSerialEventTarget();
  }

  // Capturing `this` is safe here since we clear the mPromiseRequestHolder in
  // ActorDestroy.
  promise->Then(mSerialEventTarget, __func__,
    [this] (const ClientOpResult& aResult) {
      mPromiseRequestHolder.Complete();
      PClientNavigateOpChild::Send__delete__(this, aResult);
    }, [this] (nsresult aResult) {
      mPromiseRequestHolder.Complete();
      PClientNavigateOpChild::Send__delete__(this, aResult);
  })->Track(mPromiseRequestHolder);
}

} // namespace dom
} // namespace mozilla
