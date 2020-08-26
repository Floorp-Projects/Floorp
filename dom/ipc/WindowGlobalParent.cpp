/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WindowGlobalParent.h"

#include <algorithm>

#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/InProcessParent.h"
#include "mozilla/dom/BrowserBridgeParent.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/ClientInfo.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/BrowserHost.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/RemoteWebProgress.h"
#include "mozilla/dom/WindowGlobalActorsBinding.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/ipc/IdType.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/ServoCSSParser.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Telemetry.h"
#include "mozJSComponentLoader.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsError.h"
#include "nsFrameLoader.h"
#include "nsFrameLoaderOwner.h"
#include "nsGlobalWindowInner.h"
#include "nsQueryObject.h"
#include "nsFrameLoaderOwner.h"
#include "nsSerializationHelper.h"
#include "nsIBrowser.h"
#include "nsITransportSecurityInfo.h"
#include "nsISharePicker.h"
#include "mozilla/Telemetry.h"

#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"

#include "mozilla/dom/JSActorService.h"
#include "mozilla/dom/JSWindowActorBinding.h"
#include "mozilla/dom/JSWindowActorParent.h"

using namespace mozilla::ipc;
using namespace mozilla::dom::ipc;

namespace mozilla {
namespace dom {

WindowGlobalParent::WindowGlobalParent(
    CanonicalBrowsingContext* aBrowsingContext, uint64_t aInnerWindowId,
    uint64_t aOuterWindowId, bool aInProcess, FieldValues&& aInit)
    : WindowContext(aBrowsingContext, aInnerWindowId, aOuterWindowId,
                    aInProcess, std::move(aInit)),
      mIsInitialDocument(false),
      mHasBeforeUnload(false),
      mSandboxFlags(0),
      mDocumentHasLoaded(false),
      mDocumentHasUserInteracted(false),
      mBlockAllMixedContent(false),
      mUpgradeInsecureRequests(false) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(), "Parent process only");
}

already_AddRefed<WindowGlobalParent> WindowGlobalParent::CreateDisconnected(
    const WindowGlobalInit& aInit, bool aInProcess) {
  RefPtr<CanonicalBrowsingContext> browsingContext =
      CanonicalBrowsingContext::Get(aInit.context().mBrowsingContextId);
  if (NS_WARN_IF(!browsingContext)) {
    return nullptr;
  }

  RefPtr<WindowGlobalParent> wgp =
      GetByInnerWindowId(aInit.context().mInnerWindowId);
  MOZ_RELEASE_ASSERT(!wgp, "Creating duplicate WindowGlobalParent");

  FieldValues fields(aInit.context().mFields);
  wgp = new WindowGlobalParent(browsingContext, aInit.context().mInnerWindowId,
                               aInit.context().mOuterWindowId, aInProcess,
                               std::move(fields));
  wgp->mDocumentPrincipal = aInit.principal();
  wgp->mDocumentURI = aInit.documentURI();
  wgp->mDocContentBlockingAllowListPrincipal =
      aInit.contentBlockingAllowListPrincipal();
  wgp->mBlockAllMixedContent = aInit.blockAllMixedContent();
  wgp->mUpgradeInsecureRequests = aInit.upgradeInsecureRequests();
  wgp->mSandboxFlags = aInit.sandboxFlags();
  wgp->mHttpsOnlyStatus = aInit.httpsOnlyStatus();
  wgp->mSecurityInfo = aInit.securityInfo();
  net::CookieJarSettings::Deserialize(aInit.cookieJarSettings(),
                                      getter_AddRefs(wgp->mCookieJarSettings));
  MOZ_RELEASE_ASSERT(wgp->mDocumentPrincipal, "Must have a valid principal");

  return wgp.forget();
}

void WindowGlobalParent::Init() {
  MOZ_ASSERT(Manager(), "Should have a manager!");

  // Invoke our base class' `Init` method. This will register us in
  // `gWindowContexts`.
  WindowContext::Init();

  // Determine which content process the window global is coming from.
  dom::ContentParentId processId(0);
  ContentParent* cp = nullptr;
  if (!IsInProcess()) {
    cp = static_cast<ContentParent*>(Manager()->Manager());
    processId = cp->ChildID();

    // Ensure the content process has permissions for this principal.
    cp->TransmitPermissionsForPrincipal(mDocumentPrincipal);
  }

  MOZ_DIAGNOSTIC_ASSERT(
      !BrowsingContext()->GetParent() ||
          BrowsingContext()->GetEmbedderInnerWindowId(),
      "When creating a non-root WindowGlobalParent, the WindowGlobalParent "
      "for our embedder should've already been created.");

  // Ensure we have a document URI
  if (!mDocumentURI) {
    NS_NewURI(getter_AddRefs(mDocumentURI), "about:blank");
  }

  // NOTE: `cp` may be nullptr, but that's OK, we need to tell every other
  // process in our group in that case.
  IPCInitializer ipcinit = GetIPCInitializer();
  Group()->EachOtherParent(cp, [&](ContentParent* otherContent) {
    Unused << otherContent->SendCreateWindowContext(ipcinit);
  });

  // If there is no current window global, assume we're about to become it
  // optimistically.
  if (!BrowsingContext()->IsDiscarded()) {
    MOZ_ALWAYS_SUCCEEDS(
        BrowsingContext()->SetCurrentInnerWindowId(InnerWindowId()));
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(ToSupports(this), "window-global-created", nullptr);
  }

  if (!BrowsingContext()->IsDiscarded() && ShouldTrackSiteOriginTelemetry()) {
    mOriginCounter.emplace();
    mOriginCounter->UpdateSiteOriginsFrom(this, /* aIncrease = */ true);
  }
}

void WindowGlobalParent::OriginCounter::UpdateSiteOriginsFrom(
    WindowGlobalParent* aParent, bool aIncrease) {
  MOZ_RELEASE_ASSERT(aParent);

  if (aParent->DocumentPrincipal()->GetIsContentPrincipal()) {
    nsAutoCString origin;
    aParent->DocumentPrincipal()->GetSiteOrigin(origin);

    if (aIncrease) {
      int32_t& count = mOriginMap.GetOrInsert(origin);
      count += 1;
      mMaxOrigins = std::max(mMaxOrigins, mOriginMap.Count());
    } else if (auto entry = mOriginMap.Lookup(origin)) {
      entry.Data() -= 1;

      if (entry.Data() == 0) {
        entry.Remove();
      }
    }
  }
}

void WindowGlobalParent::OriginCounter::Accumulate() {
  mozilla::Telemetry::Accumulate(
      mozilla::Telemetry::HistogramID::
          FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_PER_DOCUMENT,
      mMaxOrigins);

  mMaxOrigins = 0;
  mOriginMap.Clear();
}

/* static */
already_AddRefed<WindowGlobalParent> WindowGlobalParent::GetByInnerWindowId(
    uint64_t aInnerWindowId) {
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  return WindowContext::GetById(aInnerWindowId).downcast<WindowGlobalParent>();
}

already_AddRefed<WindowGlobalChild> WindowGlobalParent::GetChildActor() {
  if (!CanSend()) {
    return nullptr;
  }
  IProtocol* otherSide = InProcessParent::ChildActorFor(this);
  return do_AddRef(static_cast<WindowGlobalChild*>(otherSide));
}

already_AddRefed<BrowserParent> WindowGlobalParent::GetBrowserParent() {
  if (IsInProcess() || !CanSend()) {
    return nullptr;
  }
  return do_AddRef(static_cast<BrowserParent*>(Manager()));
}

ContentParent* WindowGlobalParent::GetContentParent() {
  if (IsInProcess() || !CanSend()) {
    return nullptr;
  }
  return static_cast<ContentParent*>(Manager()->Manager());
}

already_AddRefed<nsFrameLoader> WindowGlobalParent::GetRootFrameLoader() {
  dom::BrowsingContext* top = BrowsingContext()->Top();

  RefPtr<nsFrameLoaderOwner> frameLoaderOwner =
      do_QueryObject(top->GetEmbedderElement());
  if (frameLoaderOwner) {
    return frameLoaderOwner->GetFrameLoader();
  }
  return nullptr;
}

uint64_t WindowGlobalParent::ContentParentId() {
  RefPtr<BrowserParent> browserParent = GetBrowserParent();
  return browserParent ? browserParent->Manager()->ChildID() : 0;
}

int32_t WindowGlobalParent::OsPid() {
  RefPtr<BrowserParent> browserParent = GetBrowserParent();
  return browserParent ? browserParent->Manager()->Pid() : -1;
}

// A WindowGlobalPaernt is the root in its process if it has no parent, or its
// embedder is in a different process.
bool WindowGlobalParent::IsProcessRoot() {
  if (!BrowsingContext()->GetParent()) {
    return true;
  }

  RefPtr<WindowGlobalParent> embedder =
      BrowsingContext()->GetEmbedderWindowGlobal();
  if (NS_WARN_IF(!embedder)) {
    return false;
  }

  return ContentParentId() != embedder->ContentParentId();
}

uint32_t WindowGlobalParent::ContentBlockingEvents() {
  return GetContentBlockingLog()->GetContentBlockingEventsInLog();
}

void WindowGlobalParent::GetContentBlockingLog(nsAString& aLog) {
  NS_ConvertUTF8toUTF16 log(GetContentBlockingLog()->Stringify());
  aLog.Assign(std::move(log));
}

mozilla::ipc::IPCResult WindowGlobalParent::RecvLoadURI(
    const MaybeDiscarded<dom::BrowsingContext>& aTargetBC,
    nsDocShellLoadState* aLoadState, bool aSetNavigating) {
  if (aTargetBC.IsNullOrDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message with dead or detached context"));
    return IPC_OK();
  }
  CanonicalBrowsingContext* targetBC = aTargetBC.get_canonical();

  // FIXME: For cross-process loads, we should double check CanAccess() for the
  // source browsing context in the parent process.

  if (targetBC->Group() != BrowsingContext()->Group()) {
    return IPC_FAIL(this, "Illegal cross-group BrowsingContext load");
  }

  // FIXME: We should really initiate the load in the parent before bouncing
  // back down to the child.

  targetBC->LoadURI(aLoadState, aSetNavigating);
  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalParent::RecvInternalLoad(
    nsDocShellLoadState* aLoadState) {
  if (!aLoadState->Target().IsEmpty() ||
      aLoadState->TargetBrowsingContext().IsNull()) {
    return IPC_FAIL(this, "must already be retargeted");
  }
  if (aLoadState->TargetBrowsingContext().IsDiscarded()) {
    MOZ_LOG(
        BrowsingContext::GetLog(), LogLevel::Debug,
        ("ParentIPC: Trying to send a message with dead or detached context"));
    return IPC_OK();
  }
  CanonicalBrowsingContext* targetBC =
      aLoadState->TargetBrowsingContext().get_canonical();

  // FIXME: For cross-process loads, we should double check CanAccess() for the
  // source browsing context in the parent process.

  if (targetBC->Group() != BrowsingContext()->Group()) {
    return IPC_FAIL(this, "Illegal cross-group BrowsingContext load");
  }

  // FIXME: We should really initiate the load in the parent before bouncing
  // back down to the child.

  targetBC->InternalLoad(aLoadState);
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvUpdateDocumentURI(nsIURI* aURI) {
  // XXX(nika): Assert that the URI change was one which makes sense (either
  // about:blank -> a real URI, or a legal push/popstate URI change?)
  mDocumentURI = aURI;
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvUpdateDocumentPrincipal(
    nsIPrincipal* aNewDocumentPrincipal) {
  if (!mDocumentPrincipal->Equals(aNewDocumentPrincipal)) {
    return IPC_FAIL(this,
                    "Trying to reuse WindowGlobalParent but the principal of "
                    "the new document does not match the old one");
  }
  mDocumentPrincipal = aNewDocumentPrincipal;
  return IPC_OK();
}
mozilla::ipc::IPCResult WindowGlobalParent::RecvUpdateDocumentTitle(
    const nsString& aTitle) {
  if (mDocumentTitle == aTitle) {
    return IPC_OK();
  }

  mDocumentTitle = aTitle;

  // Send a pagetitlechanged event only for changes to the title
  // for top-level frames.
  if (!BrowsingContext()->IsTop()) {
    return IPC_OK();
  }

  Element* frameElement = BrowsingContext()->GetEmbedderElement();
  if (!frameElement) {
    return IPC_OK();
  }

  (new AsyncEventDispatcher(frameElement, u"pagetitlechanged"_ns,
                            CanBubble::eYes, ChromeOnlyDispatch::eYes))
      ->RunDOMEventWhenSafe();

  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalParent::RecvUpdateHttpsOnlyStatus(
    uint32_t aHttpsOnlyStatus) {
  mHttpsOnlyStatus = aHttpsOnlyStatus;
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvUpdateDocumentHasLoaded(
    bool aDocumentHasLoaded) {
  mDocumentHasLoaded = aDocumentHasLoaded;
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvUpdateDocumentHasUserInteracted(
    bool aDocumentHasUserInteracted) {
  mDocumentHasUserInteracted = aDocumentHasUserInteracted;
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvUpdateSandboxFlags(uint32_t aSandboxFlags) {
  mSandboxFlags = aSandboxFlags;
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvUpdateDocumentCspSettings(
    bool aBlockAllMixedContent, bool aUpgradeInsecureRequests) {
  mBlockAllMixedContent = aBlockAllMixedContent;
  mUpgradeInsecureRequests = aUpgradeInsecureRequests;
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvSetHasBeforeUnload(bool aHasBeforeUnload) {
  mHasBeforeUnload = aHasBeforeUnload;
  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalParent::RecvSetClientInfo(
    const IPCClientInfo& aIPCClientInfo) {
  mClientInfo = Some(ClientInfo(aIPCClientInfo));
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvDestroy() {
  // Make a copy so that we can avoid potential iterator invalidation when
  // calling the user-provided Destroy() methods.
  JSActorWillDestroy();

  if (CanSend()) {
    RefPtr<BrowserParent> browserParent = GetBrowserParent();
    if (!browserParent || !browserParent->IsDestroyed()) {
      Unused << Send__delete__(this);
    }
  }
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvRawMessage(const JSActorMessageMeta& aMeta,
                                             const ClonedMessageData& aData,
                                             const ClonedMessageData& aStack) {
  StructuredCloneData data;
  data.BorrowFromClonedMessageDataForParent(aData);
  StructuredCloneData stack;
  stack.BorrowFromClonedMessageDataForParent(aStack);
  ReceiveRawMessage(aMeta, std::move(data), std::move(stack));
  return IPC_OK();
}

const nsACString& WindowGlobalParent::GetRemoteType() {
  if (RefPtr<BrowserParent> browserParent = GetBrowserParent()) {
    return browserParent->Manager()->GetRemoteType();
  }

  return NOT_REMOTE_TYPE;
}

void WindowGlobalParent::NotifyContentBlockingEvent(
    uint32_t aEvent, nsIRequest* aRequest, bool aBlocked,
    const nsACString& aTrackingOrigin,
    const nsTArray<nsCString>& aTrackingFullHashes,
    const Maybe<ContentBlockingNotifier::StorageAccessPermissionGrantedReason>&
        aReason) {
  MOZ_ASSERT(NS_IsMainThread());
  DebugOnly<bool> isCookiesBlocked =
      aEvent == nsIWebProgressListener::STATE_COOKIES_BLOCKED_TRACKER ||
      aEvent == nsIWebProgressListener::STATE_COOKIES_BLOCKED_SOCIALTRACKER ||
      (aEvent == nsIWebProgressListener::STATE_COOKIES_BLOCKED_FOREIGN &&
       StaticPrefs::network_cookie_rejectForeignWithExceptions_enabled());
  MOZ_ASSERT_IF(aBlocked, aReason.isNothing());
  MOZ_ASSERT_IF(!isCookiesBlocked, aReason.isNothing());
  MOZ_ASSERT_IF(isCookiesBlocked && !aBlocked, aReason.isSome());
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess());
  // TODO: temporarily remove this until we find the root case of Bug 1609144
  // MOZ_DIAGNOSTIC_ASSERT_IF(XRE_IsE10sParentProcess(), !IsInProcess());

  // Return early if this WindowGlobalParent is in process.
  if (IsInProcess()) {
    return;
  }

  Maybe<uint32_t> event = GetContentBlockingLog()->RecordLogParent(
      aTrackingOrigin, aEvent, aBlocked, aReason, aTrackingFullHashes);

  // Notify the OnContentBlockingEvent if necessary.
  if (event) {
    if (!GetBrowsingContext()->GetWebProgress()) {
      return;
    }

    nsCOMPtr<nsIWebProgress> webProgress =
        new RemoteWebProgress(0, false, BrowsingContext()->IsTopContent());
    GetBrowsingContext()->Top()->GetWebProgress()->OnContentBlockingEvent(
        webProgress, aRequest, event.value());
  }
}

already_AddRefed<JSWindowActorParent> WindowGlobalParent::GetActor(
    JSContext* aCx, const nsACString& aName, ErrorResult& aRv) {
  return JSActorManager::GetActor(aCx, aName, aRv)
      .downcast<JSWindowActorParent>();
}

already_AddRefed<JSActor> WindowGlobalParent::InitJSActor(
    JS::HandleObject aMaybeActor, const nsACString& aName, ErrorResult& aRv) {
  RefPtr<JSWindowActorParent> actor;
  if (aMaybeActor.get()) {
    aRv = UNWRAP_OBJECT(JSWindowActorParent, aMaybeActor.get(), actor);
    if (aRv.Failed()) {
      return nullptr;
    }
  } else {
    actor = new JSWindowActorParent();
  }

  MOZ_RELEASE_ASSERT(!actor->GetManager(),
                     "mManager was already initialized once!");
  actor->Init(aName, this);
  return actor.forget();
}

bool WindowGlobalParent::IsCurrentGlobal() {
  return CanSend() && BrowsingContext()->GetCurrentWindowGlobal() == this;
}

namespace {

class ShareHandler final : public PromiseNativeHandler {
 public:
  explicit ShareHandler(
      mozilla::dom::WindowGlobalParent::ShareResolver&& aResolver)
      : mResolver(std::move(aResolver)) {}

  NS_DECL_ISUPPORTS

 public:
  virtual void ResolvedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override {
    mResolver(NS_OK);
  }

  virtual void RejectedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) override {
    if (NS_WARN_IF(!aValue.isObject())) {
      mResolver(NS_ERROR_FAILURE);
      return;
    }

    // nsresult is stored as Exception internally in Promise
    JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
    RefPtr<DOMException> unwrapped;
    nsresult rv = UNWRAP_OBJECT(DOMException, &obj, unwrapped);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mResolver(NS_ERROR_FAILURE);
      return;
    }

    mResolver(unwrapped->GetResult());
  }

 private:
  ~ShareHandler() = default;

  mozilla::dom::WindowGlobalParent::ShareResolver mResolver;
};

NS_IMPL_ISUPPORTS0(ShareHandler)

}  // namespace

mozilla::ipc::IPCResult WindowGlobalParent::RecvGetContentBlockingEvents(
    WindowGlobalParent::GetContentBlockingEventsResolver&& aResolver) {
  uint32_t events = GetContentBlockingLog()->GetContentBlockingEventsInLog();
  aResolver(events);

  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalParent::RecvUpdateCookieJarSettings(
    const CookieJarSettingsArgs& aCookieJarSettingsArgs) {
  net::CookieJarSettings::Deserialize(aCookieJarSettingsArgs,
                                      getter_AddRefs(mCookieJarSettings));
  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalParent::RecvUpdateDocumentSecurityInfo(
    nsITransportSecurityInfo* aSecurityInfo) {
  mSecurityInfo = aSecurityInfo;
  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalParent::RecvShare(
    IPCWebShareData&& aData, WindowGlobalParent::ShareResolver&& aResolver) {
  // Widget Layer handoff...
  nsCOMPtr<nsISharePicker> sharePicker =
      do_GetService("@mozilla.org/sharepicker;1");
  if (!sharePicker) {
    aResolver(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return IPC_OK();
  }

  // Initialize the ShareWidget
  RefPtr<BrowserParent> parent = GetBrowserParent();
  nsCOMPtr<mozIDOMWindowProxy> openerWindow;
  if (parent) {
    openerWindow = parent->GetParentWindowOuter();
    if (!openerWindow) {
      aResolver(NS_ERROR_FAILURE);
      return IPC_OK();
    }
  }
  sharePicker->Init(openerWindow);

  // And finally share the data...
  RefPtr<Promise> promise;
  nsresult rv = sharePicker->Share(aData.title(), aData.text(), aData.url(),
                                   getter_AddRefs(promise));
  if (NS_FAILED(rv)) {
    aResolver(rv);
    return IPC_OK();
  }

  // Handler finally awaits response...
  RefPtr<ShareHandler> handler = new ShareHandler(std::move(aResolver));
  promise->AppendNativeHandler(handler);

  return IPC_OK();
}

already_AddRefed<mozilla::dom::Promise> WindowGlobalParent::DrawSnapshot(
    const DOMRect* aRect, double aScale, const nsACString& aBackgroundColor,
    mozilla::ErrorResult& aRv) {
  nsIGlobalObject* global = GetParentObject();
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nscolor color;
  if (NS_WARN_IF(!ServoCSSParser::ComputeColor(nullptr, NS_RGB(0, 0, 0),
                                               aBackgroundColor, &color,
                                               nullptr, nullptr))) {
    aRv = NS_ERROR_FAILURE;
    return nullptr;
  }

  if (!gfx::CrossProcessPaint::Start(this, aRect, (float)aScale, color,
                                     gfx::CrossProcessPaintFlags::None,
                                     promise)) {
    aRv = NS_ERROR_FAILURE;
    return nullptr;
  }
  return promise.forget();
}

void WindowGlobalParent::DrawSnapshotInternal(gfx::CrossProcessPaint* aPaint,
                                              const Maybe<IntRect>& aRect,
                                              float aScale,
                                              nscolor aBackgroundColor,
                                              uint32_t aFlags) {
  auto promise = SendDrawSnapshot(aRect, aScale, aBackgroundColor, aFlags);

  RefPtr<gfx::CrossProcessPaint> paint(aPaint);
  RefPtr<WindowGlobalParent> wgp(this);
  promise->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [paint, wgp](PaintFragment&& aFragment) {
        paint->ReceiveFragment(wgp, std::move(aFragment));
      },
      [paint, wgp](ResponseRejectReason&& aReason) {
        paint->LostFragment(wgp);
      });
}

already_AddRefed<Promise> WindowGlobalParent::GetSecurityInfo(
    ErrorResult& aRv) {
  RefPtr<BrowserParent> browserParent = GetBrowserParent();
  if (NS_WARN_IF(!browserParent)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsIGlobalObject* global = GetParentObject();
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  SendGetSecurityInfo(
      [promise](Maybe<nsCString>&& aResult) {
        if (aResult) {
          nsCOMPtr<nsISupports> infoObj;
          nsresult rv =
              NS_DeserializeObject(aResult.value(), getter_AddRefs(infoObj));
          if (NS_WARN_IF(NS_FAILED(rv))) {
            promise->MaybeReject(NS_ERROR_FAILURE);
          }
          nsCOMPtr<nsITransportSecurityInfo> info = do_QueryInterface(infoObj);
          if (!info) {
            promise->MaybeReject(NS_ERROR_FAILURE);
          }
          promise->MaybeResolve(info);
        } else {
          promise->MaybeResolveWithUndefined();
        }
      },
      [promise](ResponseRejectReason&& aReason) {
        promise->MaybeReject(NS_ERROR_FAILURE);
      });

  return promise.forget();
}

void WindowGlobalParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (GetBrowsingContext()->IsTopContent() &&
      !mDocumentPrincipal->SchemeIs("about")) {
    // Record the page load
    uint32_t pageLoaded = 1;
    Accumulate(Telemetry::MIXED_CONTENT_UNBLOCK_COUNTER, pageLoaded);

    // Record the mixed content status of the docshell in Telemetry
    enum {
      NO_MIXED_CONTENT = 0,  // There is no Mixed Content on the page
      MIXED_DISPLAY_CONTENT =
          1,  // The page attempted to load Mixed Display Content
      MIXED_ACTIVE_CONTENT =
          2,  // The page attempted to load Mixed Active Content
      MIXED_DISPLAY_AND_ACTIVE_CONTENT = 3  // The page attempted to load Mixed
                                            // Display & Mixed Active Content
    };

    bool hasMixedDisplay =
        mMixedContentSecurityState &
        (nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT |
         nsIWebProgressListener::STATE_BLOCKED_MIXED_DISPLAY_CONTENT);
    bool hasMixedActive =
        mMixedContentSecurityState &
        (nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT |
         nsIWebProgressListener::STATE_BLOCKED_MIXED_ACTIVE_CONTENT);

    uint32_t mixedContentLevel = NO_MIXED_CONTENT;
    if (hasMixedDisplay && hasMixedActive) {
      mixedContentLevel = MIXED_DISPLAY_AND_ACTIVE_CONTENT;
    } else if (hasMixedActive) {
      mixedContentLevel = MIXED_ACTIVE_CONTENT;
    } else if (hasMixedDisplay) {
      mixedContentLevel = MIXED_DISPLAY_CONTENT;
    }
    Accumulate(Telemetry::MIXED_CONTENT_PAGE_LOAD, mixedContentLevel);

    ScalarAdd(Telemetry::ScalarID::MEDIA_PAGE_COUNT, 1);
    if (GetDocTreeHadAudibleMedia()) {
      ScalarAdd(Telemetry::ScalarID::MEDIA_PAGE_HAD_MEDIA_COUNT, 1);
    }
  }

  // If there are any non-discarded nested contexts when this WindowContext is
  // destroyed, tear them down.
  nsTArray<RefPtr<dom::BrowsingContext>> toDiscard;
  toDiscard.AppendElements(Children());
  for (auto& context : toDiscard) {
    context->Detach(/* aFromIPC */ true);
  }

  // Note that our WindowContext has become discarded.
  WindowContext::Discard();

  ContentParent* cp = nullptr;
  if (!IsInProcess()) {
    cp = static_cast<ContentParent*>(Manager()->Manager());
  }

  RefPtr<WindowGlobalParent> self(this);
  Group()->EachOtherParent(cp, [&](ContentParent* otherContent) {
    // Keep the WindowContext alive until other processes have acknowledged it
    // has been discarded.
    auto resolve = [self](bool) {};
    auto reject = [self](mozilla::ipc::ResponseRejectReason) {};
    otherContent->SendDiscardWindowContext(InnerWindowId(), resolve, reject);
  });

  // Report content blocking log when destroyed.
  // There shouldn't have any content blocking log when a documnet is loaded in
  // the parent process(See NotifyContentBlockingeEvent), so we could skip
  // reporting log when it is in-process.
  if (!IsInProcess()) {
    RefPtr<BrowserParent> browserParent =
        static_cast<BrowserParent*>(Manager());
    if (browserParent) {
      nsCOMPtr<nsILoadContext> loadContext = browserParent->GetLoadContext();
      if (loadContext && !loadContext->UsePrivateBrowsing() &&
          BrowsingContext()->IsTopContent()) {
        GetContentBlockingLog()->ReportLog(DocumentPrincipal());

        if (mDocumentURI && (net::SchemeIsHTTP(mDocumentURI) ||
                             net::SchemeIsHTTPS(mDocumentURI))) {
          GetContentBlockingLog()->ReportOrigins();
        }
      }
    }
  }

  // Destroy our JSWindowActors, and reject any pending queries.
  JSActorDidDestroy();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(ToSupports(this), "window-global-destroyed", nullptr);
  }

  if (mOriginCounter) {
    mOriginCounter->Accumulate();
  }
}

WindowGlobalParent::~WindowGlobalParent() = default;

JSObject* WindowGlobalParent::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return WindowGlobalParent_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* WindowGlobalParent::GetParentObject() {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

nsIDOMProcessParent* WindowGlobalParent::GetDomProcess() {
  if (RefPtr<BrowserParent> browserParent = GetBrowserParent()) {
    return browserParent->Manager();
  }
  return InProcessParent::Singleton();
}

void WindowGlobalParent::DidBecomeCurrentWindowGlobal(bool aCurrent) {
  WindowGlobalParent* top = BrowsingContext()->GetTopWindowContext();
  if (top && top->mOriginCounter) {
    top->mOriginCounter->UpdateSiteOriginsFrom(this,
                                               /* aIncrease = */ aCurrent);
  }
}

bool WindowGlobalParent::ShouldTrackSiteOriginTelemetry() {
  CanonicalBrowsingContext* bc = BrowsingContext();

  if (!bc->IsTopContent()) {
    return false;
  }

  RefPtr<BrowserParent> browserParent = GetBrowserParent();
  if (!browserParent ||
      !IsWebRemoteType(browserParent->Manager()->GetRemoteType())) {
    return false;
  }

  return DocumentPrincipal()->GetIsContentPrincipal();
}

void WindowGlobalParent::AddMixedContentSecurityState(uint32_t aStateFlags) {
  MOZ_ASSERT(TopWindowContext() == this);
  MOZ_ASSERT((aStateFlags &
              (nsIWebProgressListener::STATE_LOADED_MIXED_DISPLAY_CONTENT |
               nsIWebProgressListener::STATE_LOADED_MIXED_ACTIVE_CONTENT |
               nsIWebProgressListener::STATE_BLOCKED_MIXED_DISPLAY_CONTENT |
               nsIWebProgressListener::STATE_BLOCKED_MIXED_ACTIVE_CONTENT)) ==
                 aStateFlags,
             "Invalid flags specified!");

  if ((mMixedContentSecurityState & aStateFlags) == aStateFlags) {
    return;
  }

  mMixedContentSecurityState |= aStateFlags;

  if (GetBrowsingContext()->GetCurrentWindowGlobal() == this) {
    GetBrowsingContext()->UpdateSecurityStateForLocationOrMixedContentChange();
  }
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(WindowGlobalParent, WindowContext)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(WindowGlobalParent,
                                               WindowContext)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WindowGlobalParent)
NS_INTERFACE_MAP_END_INHERITING(WindowContext)

NS_IMPL_ADDREF_INHERITED(WindowGlobalParent, WindowContext)
NS_IMPL_RELEASE_INHERITED(WindowGlobalParent, WindowContext)

}  // namespace dom
}  // namespace mozilla
