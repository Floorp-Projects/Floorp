/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CanonicalBrowsingContext.h"

#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/MediaController.h"
#include "mozilla/dom/MediaControlService.h"
#include "mozilla/dom/PlaybackController.h"
#include "mozilla/net/DocumentLoadListener.h"
#include "mozilla/net/BrowsingContextDocumentChannel.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/net/DocumentLoadListener.h"
#include "nsIWebNavigation.h"

#include "nsGlobalWindowOuter.h"

using namespace mozilla::ipc;

extern mozilla::LazyLogModule gAutoplayPermissionLog;

#define AUTOPLAY_LOG(msg, ...) \
  MOZ_LOG(gAutoplayPermissionLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

namespace mozilla {
namespace dom {

extern mozilla::LazyLogModule gUserInteractionPRLog;

#define USER_ACTIVATION_LOG(msg, ...) \
  MOZ_LOG(gUserInteractionPRLog, LogLevel::Debug, (msg, ##__VA_ARGS__))

CanonicalBrowsingContext::CanonicalBrowsingContext(BrowsingContext* aParent,
                                                   BrowsingContextGroup* aGroup,
                                                   uint64_t aBrowsingContextId,
                                                   uint64_t aOwnerProcessId,
                                                   uint64_t aEmbedderProcessId,
                                                   BrowsingContext::Type aType,
                                                   FieldTuple&& aFields)
    : BrowsingContext(aParent, aGroup, aBrowsingContextId, aType,
                      std::move(aFields)),
      mProcessId(aOwnerProcessId),
      mEmbedderProcessId(aEmbedderProcessId) {
  // You are only ever allowed to create CanonicalBrowsingContexts in the
  // parent process.
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
}

/* static */
already_AddRefed<CanonicalBrowsingContext> CanonicalBrowsingContext::Get(
    uint64_t aId) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return BrowsingContext::Get(aId).downcast<CanonicalBrowsingContext>();
}

/* static */
CanonicalBrowsingContext* CanonicalBrowsingContext::Cast(
    BrowsingContext* aContext) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return static_cast<CanonicalBrowsingContext*>(aContext);
}

/* static */
const CanonicalBrowsingContext* CanonicalBrowsingContext::Cast(
    const BrowsingContext* aContext) {
  MOZ_RELEASE_ASSERT(XRE_IsParentProcess());
  return static_cast<const CanonicalBrowsingContext*>(aContext);
}

ContentParent* CanonicalBrowsingContext::GetContentParent() const {
  if (mProcessId == 0) {
    return nullptr;
  }

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  return cpm->GetContentProcessById(ContentParentId(mProcessId));
}

void CanonicalBrowsingContext::GetCurrentRemoteType(nsAString& aRemoteType,
                                                    ErrorResult& aRv) const {
  // If we're in the parent process, dump out the void string.
  if (mProcessId == 0) {
    aRemoteType.Assign(VoidString());
    return;
  }

  ContentParent* cp = GetContentParent();
  if (!cp) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  aRemoteType.Assign(cp->GetRemoteType());
}

void CanonicalBrowsingContext::SetOwnerProcessId(uint64_t aProcessId) {
  MOZ_LOG(GetLog(), LogLevel::Debug,
          ("SetOwnerProcessId for 0x%08" PRIx64 " (0x%08" PRIx64
           " -> 0x%08" PRIx64 ")",
           Id(), mProcessId, aProcessId));

  mProcessId = aProcessId;
}

void CanonicalBrowsingContext::SetInFlightProcessId(uint64_t aProcessId) {
  // We can't handle more than one in-flight process change at a time.
  MOZ_ASSERT_IF(aProcessId, mInFlightProcessId == 0);

  mInFlightProcessId = aProcessId;
}

void CanonicalBrowsingContext::GetWindowGlobals(
    nsTArray<RefPtr<WindowGlobalParent>>& aWindows) {
  aWindows.SetCapacity(GetWindowContexts().Length());
  for (auto& window : GetWindowContexts()) {
    aWindows.AppendElement(static_cast<WindowGlobalParent*>(window.get()));
  }
}

WindowGlobalParent* CanonicalBrowsingContext::GetCurrentWindowGlobal() const {
  return static_cast<WindowGlobalParent*>(GetCurrentWindowContext());
}

already_AddRefed<nsIWidget>
CanonicalBrowsingContext::GetParentProcessWidgetContaining() {
  // If our document is loaded in-process, such as chrome documents, get the
  // widget directly from our outer window. Otherwise, try to get the widget
  // from the toplevel content's browser's element.
  nsCOMPtr<nsIWidget> widget;
  if (nsGlobalWindowOuter* window = nsGlobalWindowOuter::Cast(GetDOMWindow())) {
    widget = window->GetNearestWidget();
  } else if (Element* topEmbedder = Top()->GetEmbedderElement()) {
    widget = nsContentUtils::WidgetForContent(topEmbedder);
    if (!widget) {
      widget = nsContentUtils::WidgetForDocument(topEmbedder->OwnerDoc());
    }
  }

  if (widget) {
    widget = widget->GetTopLevelWidget();
  }

  return widget.forget();
}

already_AddRefed<WindowGlobalParent>
CanonicalBrowsingContext::GetEmbedderWindowGlobal() const {
  uint64_t windowId = GetEmbedderInnerWindowId();
  if (windowId == 0) {
    return nullptr;
  }

  return WindowGlobalParent::GetByInnerWindowId(windowId);
}

already_AddRefed<WindowGlobalParent>
CanonicalBrowsingContext::GetParentWindowGlobal() const {
  if (GetParent()) {
    return GetEmbedderWindowGlobal();
  }
  return nullptr;
}

nsISHistory* CanonicalBrowsingContext::GetSessionHistory() {
  if (mSessionHistory) {
    return mSessionHistory;
  }

  nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(GetDocShell());
  if (webNav) {
    RefPtr<ChildSHistory> shistory = webNav->GetSessionHistory();
    if (shistory) {
      return shistory->LegacySHistory();
    }
  }

  return nullptr;
}

JSObject* CanonicalBrowsingContext::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return CanonicalBrowsingContext_Binding::Wrap(aCx, this, aGivenProto);
}

void CanonicalBrowsingContext::CanonicalDiscard() {
  if (mTabMediaController) {
    mTabMediaController->Shutdown();
    mTabMediaController = nullptr;
  }
}

void CanonicalBrowsingContext::NotifyStartDelayedAutoplayMedia() {
  if (!GetCurrentWindowGlobal()) {
    return;
  }

  // As this function would only be called when user click the play icon on the
  // tab bar. That's clear user intent to play, so gesture activate the browsing
  // context so that the block-autoplay logic allows the media to autoplay.
  NotifyUserGestureActivation();
  AUTOPLAY_LOG("NotifyStartDelayedAutoplayMedia for chrome bc 0x%08" PRIx64,
               Id());
  StartDelayedAutoplayMediaComponents();
  // Notfiy all content browsing contexts which are related with the canonical
  // browsing content tree to start delayed autoplay media.

  Group()->EachParent([&](ContentParent* aParent) {
    Unused << aParent->SendStartDelayedAutoplayMediaComponents(this);
  });
}

void CanonicalBrowsingContext::NotifyMediaMutedChanged(bool aMuted) {
  MOZ_ASSERT(!GetParent(),
             "Notify media mute change on non top-level context!");
  SetMuted(aMuted);
}

uint32_t CanonicalBrowsingContext::CountSiteOrigins(
    GlobalObject& aGlobal,
    const Sequence<OwningNonNull<BrowsingContext>>& aRoots) {
  nsTHashtable<nsCStringHashKey> uniqueSiteOrigins;

  for (const auto& root : aRoots) {
    root->PreOrderWalk([&](BrowsingContext* aContext) {
      WindowGlobalParent* windowGlobalParent =
          aContext->Canonical()->GetCurrentWindowGlobal();
      if (windowGlobalParent) {
        nsIPrincipal* documentPrincipal =
            windowGlobalParent->DocumentPrincipal();

        bool isContentPrincipal = documentPrincipal->GetIsContentPrincipal();
        if (isContentPrincipal) {
          nsCString siteOrigin;
          documentPrincipal->GetSiteOrigin(siteOrigin);
          uniqueSiteOrigins.PutEntry(siteOrigin);
        }
      }
    });
  }

  return uniqueSiteOrigins.Count();
}

void CanonicalBrowsingContext::UpdateMediaControlKeysEvent(
    MediaControlKeysEvent aEvent) {
  MediaActionHandler::HandleMediaControlKeysEvent(this, aEvent);
  Group()->EachParent([&](ContentParent* aParent) {
    Unused << aParent->SendUpdateMediaControlKeysEvent(this, aEvent);
  });
}

void CanonicalBrowsingContext::LoadURI(const nsAString& aURI,
                                       const LoadURIOptions& aOptions,
                                       ErrorResult& aError) {
  nsCOMPtr<nsISupports> consumer = GetDocShell();
  if (!consumer) {
    consumer = GetEmbedderElement();
  }
  if (!consumer) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  RefPtr<nsDocShellLoadState> loadState;
  nsresult rv = nsDocShellLoadState::CreateFromLoadURIOptions(
      consumer, aURI, aOptions, getter_AddRefs(loadState));

  if (rv == NS_ERROR_MALFORMED_URI) {
    DisplayLoadError(aURI);
    return;
  }

  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }

  LoadURI(loadState, true);
}

void CanonicalBrowsingContext::Stop(uint32_t aStopFlags) {
  if (IsDiscarded()) {
    return;
  }

  // Stop any known network loads if necessary.
  if (mCurrentLoad && (aStopFlags & nsIWebNavigation::STOP_NETWORK)) {
    mCurrentLoad->Cancel(NS_BINDING_ABORTED);
  }

  // Ask the docshell to stop to handle loads that haven't
  // yet reached here, as well as non-network activity.
  if (GetDocShell()) {
    nsDocShell::Cast(GetDocShell())->Stop(aStopFlags);
  } else if (ContentParent* cp = GetContentParent()) {
    Unused << cp->SendStopLoad(this, aStopFlags);
  }
}

void CanonicalBrowsingContext::PendingRemotenessChange::Complete(
    ContentParent* aContentParent) {
  if (!mPromise) {
    return;
  }

  RefPtr<CanonicalBrowsingContext> target(mTarget);
  if (target->IsDiscarded()) {
    Cancel(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<WindowGlobalParent> embedderWindow = target->GetEmbedderWindowGlobal();
  if (NS_WARN_IF(!embedderWindow) || NS_WARN_IF(!embedderWindow->CanSend())) {
    Cancel(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<BrowserParent> embedderBrowser = embedderWindow->GetBrowserParent();
  if (NS_WARN_IF(!embedderBrowser)) {
    Cancel(NS_ERROR_FAILURE);
    return;
  }

  // Pull load flags from our embedder browser.
  nsCOMPtr<nsILoadContext> loadContext = embedderBrowser->GetLoadContext();
  MOZ_DIAGNOSTIC_ASSERT(
      loadContext->UseRemoteTabs() && loadContext->UseRemoteSubframes(),
      "Not supported without fission");

  // NOTE: These are the only flags we actually care about
  uint32_t chromeFlags = nsIWebBrowserChrome::CHROME_REMOTE_WINDOW |
                         nsIWebBrowserChrome::CHROME_FISSION_WINDOW;
  if (loadContext->UsePrivateBrowsing()) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;
  }

  TabId tabId(nsContentUtils::GenerateTabId());
  RefPtr<BrowserBridgeParent> bridge = new BrowserBridgeParent();
  ManagedEndpoint<PBrowserBridgeChild> endpoint =
      embedderBrowser->OpenPBrowserBridgeEndpoint(bridge);
  if (NS_WARN_IF(!endpoint.IsValid())) {
    Cancel(NS_ERROR_UNEXPECTED);
    return;
  }

  RefPtr<WindowGlobalParent> oldWindow = target->GetCurrentWindowGlobal();
  RefPtr<BrowserParent> oldBrowser =
      oldWindow ? oldWindow->GetBrowserParent() : nullptr;
  bool wasRemote = oldWindow && oldWindow->IsProcessRoot();

  // Update which process is considered the current owner
  uint64_t inFlightProcessId = target->OwnerProcessId();
  target->SetInFlightProcessId(inFlightProcessId);
  target->SetOwnerProcessId(aContentParent->ChildID());

  auto resetInFlightId = [target, inFlightProcessId] {
    if (target->GetInFlightProcessId() == inFlightProcessId) {
      target->SetInFlightProcessId(0);
    } else {
      MOZ_DIAGNOSTIC_ASSERT(false, "Unexpected InFlightProcessId");
    }
  };

  // If we were in a remote frame, trigger unloading of the remote window. When
  // the original remote window acknowledges, we can clear the in-flight ID.
  if (wasRemote) {
    MOZ_DIAGNOSTIC_ASSERT(oldBrowser);
    MOZ_DIAGNOSTIC_ASSERT(oldBrowser != embedderBrowser);
    MOZ_DIAGNOSTIC_ASSERT(oldBrowser->GetBrowserBridgeParent());

    auto callback = [resetInFlightId](auto) { resetInFlightId(); };
    oldBrowser->SendWillChangeProcess(callback, callback);
    oldBrowser->Destroy();
  }

  // Tell the embedder process a remoteness change is in-process. When this is
  // acknowledged, reset the in-flight ID if it used to be an in-process load.
  {
    auto callback = [wasRemote, resetInFlightId](auto) {
      if (!wasRemote) {
        resetInFlightId();
      }
    };
    embedderWindow->SendMakeFrameRemote(target, std::move(endpoint), tabId,
                                        callback, callback);
  }

  // FIXME: We should get the correct principal for the to-be-created window so
  // we can avoid creating unnecessary extra windows in the new process.
  OriginAttributes attrs = embedderBrowser->OriginAttributesRef();
  RefPtr<nsIPrincipal> principal = embedderBrowser->GetContentPrincipal();
  if (principal) {
    attrs.SetFirstPartyDomain(
        true, principal->OriginAttributesRef().mFirstPartyDomain);
  }

  nsCOMPtr<nsIPrincipal> initialPrincipal =
      NullPrincipal::CreateWithInheritedAttributes(attrs,
                                                   /* isFirstParty */ false);
  WindowGlobalInit windowInit =
      WindowGlobalActor::AboutBlankInitializer(target, initialPrincipal);

  // Actually create the new BrowserParent actor and finish initialization of
  // our new BrowserBridgeParent.
  nsresult rv = bridge->InitWithProcess(aContentParent, EmptyString(),
                                        windowInit, chromeFlags, tabId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Cancel(rv);
    return;
  }

  RefPtr<BrowserParent> newBrowser = bridge->GetBrowserParent();
  newBrowser->ResumeLoad(mPendingSwitchId);

  // We did it! The process switch is complete.
  mPromise->Resolve(newBrowser, __func__);
  Clear();
}

void CanonicalBrowsingContext::PendingRemotenessChange::Cancel(nsresult aRv) {
  if (!mPromise) {
    return;
  }

  mPromise->Reject(aRv, __func__);
  Clear();
}

void CanonicalBrowsingContext::PendingRemotenessChange::Clear() {
  // Make sure we don't die while we're doing cleanup.
  RefPtr<PendingRemotenessChange> kungFuDeathGrip(this);
  if (mTarget) {
    MOZ_DIAGNOSTIC_ASSERT(mTarget->mPendingRemotenessChange == this);
    mTarget->mPendingRemotenessChange = nullptr;
  }

  mPromise = nullptr;
  mTarget = nullptr;
}

CanonicalBrowsingContext::PendingRemotenessChange::~PendingRemotenessChange() {
  MOZ_ASSERT(!mPromise && !mTarget,
             "should've already been Cancel() or Complete()-ed");
}

RefPtr<CanonicalBrowsingContext::RemotenessPromise>
CanonicalBrowsingContext::ChangeFrameRemoteness(const nsAString& aRemoteType,
                                                uint64_t aPendingSwitchId) {
  // Ensure our embedder hasn't been destroyed already.
  RefPtr<WindowGlobalParent> embedderWindowGlobal = GetEmbedderWindowGlobal();
  if (!embedderWindowGlobal) {
    NS_WARNING("Non-embedded BrowsingContext");
    return RemotenessPromise::CreateAndReject(NS_ERROR_UNEXPECTED, __func__);
  }

  if (!embedderWindowGlobal->CanSend()) {
    NS_WARNING("Embedder already been destroyed.");
    return RemotenessPromise::CreateAndReject(NS_ERROR_NOT_AVAILABLE, __func__);
  }

  RefPtr<ContentParent> oldContent = GetContentParent();
  if (!oldContent || aRemoteType.IsEmpty()) {
    NS_WARNING("Cannot switch to or from non-remote frame");
    return RemotenessPromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED,
                                              __func__);
  }

  if (aRemoteType.Equals(oldContent->GetRemoteType())) {
    NS_WARNING("Already in the correct process");
    return RemotenessPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // Cancel ongoing remoteness changes.
  if (mPendingRemotenessChange) {
    mPendingRemotenessChange->Cancel(NS_ERROR_ABORT);
    MOZ_ASSERT(!mPendingRemotenessChange, "Should have cleared");
  }

  RefPtr<BrowserParent> embedderBrowser =
      embedderWindowGlobal->GetBrowserParent();
  MOZ_ASSERT(embedderBrowser);

  // Switching to local. No new process, so perform switch sync.
  if (aRemoteType.Equals(embedderBrowser->Manager()->GetRemoteType())) {
    if (GetCurrentWindowGlobal()) {
      MOZ_DIAGNOSTIC_ASSERT(GetCurrentWindowGlobal()->IsProcessRoot());
      RefPtr<BrowserParent> oldBrowser =
          GetCurrentWindowGlobal()->GetBrowserParent();

      RefPtr<CanonicalBrowsingContext> target(this);
      SetInFlightProcessId(OwnerProcessId());
      oldBrowser->SendWillChangeProcess(
          [target](bool aSuccess) { target->SetInFlightProcessId(0); },
          [target](mozilla::ipc::ResponseRejectReason aReason) {
            target->SetInFlightProcessId(0);
          });
      oldBrowser->Destroy();
    }

    SetOwnerProcessId(embedderBrowser->Manager()->ChildID());
    Unused << embedderWindowGlobal->SendMakeFrameLocal(this, aPendingSwitchId);
    return RemotenessPromise::CreateAndResolve(embedderBrowser, __func__);
  }

  // Switching to remote. Wait for new process to launch before switch.
  auto promise = MakeRefPtr<RemotenessPromise::Private>(__func__);
  RefPtr<PendingRemotenessChange> change =
      new PendingRemotenessChange(this, promise, aPendingSwitchId);
  mPendingRemotenessChange = change;

  ContentParent::GetNewOrUsedBrowserProcessAsync(
      /* aFrameElement = */ nullptr,
      /* aRemoteType = */ aRemoteType,
      /* aPriority = */ hal::PROCESS_PRIORITY_FOREGROUND,
      /* aOpener = */ nullptr,
      /* aPreferUsed = */ false)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [change](ContentParent* aContentParent) {
            change->Complete(aContentParent);
          },
          [change](LaunchError aError) { change->Cancel(NS_ERROR_FAILURE); });
  return promise.forget();
}

already_AddRefed<Promise> CanonicalBrowsingContext::ChangeFrameRemoteness(
    const nsAString& aRemoteType, uint64_t aPendingSwitchId, ErrorResult& aRv) {
  nsIGlobalObject* global = xpc::NativeGlobal(xpc::PrivilegedJunkScope());

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  ChangeFrameRemoteness(aRemoteType, aPendingSwitchId)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [promise](BrowserParent* aBrowserParent) {
            promise->MaybeResolve(aBrowserParent->Manager()->ChildID());
          },
          [promise](nsresult aRv) { promise->MaybeReject(aRv); });
  return promise.forget();
}

MediaController* CanonicalBrowsingContext::GetMediaController() {
  // We would only create one media controller per tab, so accessing the
  // controller via the top-level browsing context.
  if (GetParent()) {
    return Cast(Top())->GetMediaController();
  }

  MOZ_ASSERT(!GetParent(),
             "Must access the controller from the top-level browsing context!");
  // Only content browsing context can create media controller, we won't create
  // controller for chrome document, such as the browser UI.
  if (!mTabMediaController && !IsDiscarded() && IsContent()) {
    mTabMediaController = new MediaController(Id());
  }
  return mTabMediaController;
}

bool CanonicalBrowsingContext::AttemptLoadURIInParent(
    nsDocShellLoadState* aLoadState, bool aSetNavigating) {
  // We currently only support starting loads directly from the
  // CanonicalBrowsingContext for top-level BCs.
  if (!IsTopContent() || !StaticPrefs::browser_tabs_documentchannel() ||
      !StaticPrefs::browser_tabs_documentchannel_parent_initiated()) {
    return false;
  }

  // We currently don't support initiating loads in the parent when they are
  // watched by devtools. This is because devtools tracks loads using content
  // process notifications, which happens after the load is initiated in this
  // case. Devtools clears all prior requests when it detects a new navigation,
  // so it drops the main document load that happened here.
  if (GetWatchedByDevtools()) {
    return false;
  }

  // DocumentChannel currently only supports connecting channels into the
  // content process, so we can only support schemes that will always be loaded
  // there for now. Restrict to just http(s) for simplicity.
  if (!aLoadState->URI()->SchemeIs("http") &&
      !aLoadState->URI()->SchemeIs("https")) {
    return false;
  }

  uint64_t outerWindowId = 0;
  if (WindowGlobalParent* global = GetCurrentWindowGlobal()) {
    nsCOMPtr<nsIURI> currentURI = global->GetDocumentURI();
    if (currentURI) {
      bool newURIHasRef = false;
      aLoadState->URI()->GetHasRef(&newURIHasRef);
      bool equalsExceptRef = false;
      aLoadState->URI()->EqualsExceptRef(currentURI, &equalsExceptRef);

      if (equalsExceptRef && newURIHasRef) {
        // This navigation is same-doc WRT the current one, we should pass it
        // down to the docshell to be handled.
        return false;
      }
    }
    // If the current document has a beforeunload listener, then we need to
    // start the load in that process after we fire the event.
    if (global->HasBeforeUnload()) {
      return false;
    }

    outerWindowId = global->OuterWindowId();
  }

  RefPtr<net::BrowsingContextDocumentChannel> docChannel =
      new net::BrowsingContextDocumentChannel(this);

  // If we successfully open the DocumentChannel, then it'll register
  // itself in mCurrentLoad and be kept alive until it completes
  // loading.
  return docChannel->Open(aLoadState, outerWindowId, aSetNavigating);
}

bool CanonicalBrowsingContext::StartDocumentLoad(
    net::DocumentLoadListener* aLoad) {
  if (mCurrentLoad) {
    // If the new load is originates from a content process,
    // and the current load is from the parent, then we consider
    // the existing load higher precendence, and reject the new
    // one.
    if (aLoad->OtherPid() && !mCurrentLoad->OtherPid()) {
      return false;
    }
    mCurrentLoad->Cancel(NS_BINDING_ABORTED);
  }
  mCurrentLoad = aLoad;
  return true;
}
void CanonicalBrowsingContext::EndDocumentLoad(
    net::DocumentLoadListener* aLoad) {
  if (mCurrentLoad == aLoad) {
    mCurrentLoad = nullptr;
  }
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(CanonicalBrowsingContext, BrowsingContext,
                                   mSessionHistory)

NS_IMPL_ADDREF_INHERITED(CanonicalBrowsingContext, BrowsingContext)
NS_IMPL_RELEASE_INHERITED(CanonicalBrowsingContext, BrowsingContext)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CanonicalBrowsingContext)
NS_INTERFACE_MAP_END_INHERITING(BrowsingContext)

}  // namespace dom
}  // namespace mozilla
