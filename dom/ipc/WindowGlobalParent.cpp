/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WindowGlobalParent.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ipc/InProcessParent.h"
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

#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"

#include "mozilla/dom/JSWindowActorBinding.h"
#include "mozilla/dom/JSWindowActorParent.h"
#include "mozilla/dom/JSWindowActorService.h"

using namespace mozilla::ipc;
using namespace mozilla::dom::ipc;

namespace mozilla {
namespace dom {

WindowGlobalParent::WindowGlobalParent(const WindowGlobalInit& aInit,
                                       bool aInProcess)
    : WindowContext(aInit.browsingContext().GetMaybeDiscarded(),
                    aInit.innerWindowId(), {}),
      mDocumentPrincipal(aInit.principal()),
      mDocumentURI(aInit.documentURI()),
      mInProcess(aInProcess),
      mIsInitialDocument(false),
      mHasBeforeUnload(false),
      mSandboxFlags(0),
      mDocumentHasLoaded(false),
      mDocumentHasUserInteracted(false),
      mBlockAllMixedContent(false),
      mUpgradeInsecureRequests(false) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsParentProcess(), "Parent process only");

  MOZ_RELEASE_ASSERT(
      BrowsingContext(),
      "Must be made in BrowsingContext, though it may be discarded");
  MOZ_RELEASE_ASSERT(mDocumentPrincipal, "Must have a valid principal");

  mFields.SetWithoutSyncing<IDX_OuterWindowId>(aInit.outerWindowId());
}

void WindowGlobalParent::Init(const WindowGlobalInit& aInit) {
  MOZ_ASSERT(Manager(), "Should have a manager!");

  // Invoke our base class' `Init` method. This will register us in
  // `gWindowContexts`.
  WindowContext::Init();

  // Determine which content process the window global is coming from.
  dom::ContentParentId processId(0);
  ContentParent* cp = nullptr;
  if (!mInProcess) {
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
    BrowsingContext()->SetCurrentInnerWindowId(aInit.innerWindowId());
  }

  mDocContentBlockingAllowListPrincipal =
      aInit.contentBlockingAllowListPrincipal();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(ToSupports(this), "window-global-created", nullptr);
  }
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
    const MaybeDiscarded<dom::BrowsingContext>& aTargetBC,
    nsDocShellLoadState* aLoadState) {
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

  targetBC->InternalLoad(aLoadState, nullptr, nullptr);
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
  mDocumentTitle = aTitle;
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
  nsTArray<RefPtr<JSWindowActorParent>> windowActors(mWindowActors.Count());
  for (auto iter = mWindowActors.Iter(); !iter.Done(); iter.Next()) {
    windowActors.AppendElement(iter.UserData());
  }

  for (auto& windowActor : windowActors) {
    windowActor->StartDestroy();
  }

  if (CanSend()) {
    RefPtr<BrowserParent> browserParent = GetBrowserParent();
    if (!browserParent || !browserParent->IsDestroyed()) {
      Unused << Send__delete__(this);
    }
  }
  return IPC_OK();
}

IPCResult WindowGlobalParent::RecvRawMessage(
    const JSWindowActorMessageMeta& aMeta, const ClonedMessageData& aData,
    const ClonedMessageData& aStack) {
  StructuredCloneData data;
  data.BorrowFromClonedMessageDataForParent(aData);
  StructuredCloneData stack;
  stack.BorrowFromClonedMessageDataForParent(aStack);
  ReceiveRawMessage(aMeta, std::move(data), std::move(stack));
  return IPC_OK();
}

void WindowGlobalParent::ReceiveRawMessage(
    const JSWindowActorMessageMeta& aMeta, StructuredCloneData&& aData,
    StructuredCloneData&& aStack) {
  RefPtr<JSWindowActorParent> actor =
      GetActor(aMeta.actorName(), IgnoreErrors());
  if (actor) {
    actor->ReceiveRawMessage(aMeta, std::move(aData), std::move(aStack));
  }
}

const nsAString& WindowGlobalParent::GetRemoteType() {
  if (RefPtr<BrowserParent> browserParent = GetBrowserParent()) {
    return browserParent->Manager()->GetRemoteType();
  }

  return VoidString();
}

void WindowGlobalParent::NotifyContentBlockingEvent(
    uint32_t aEvent, nsIRequest* aRequest, bool aBlocked,
    const nsACString& aTrackingOrigin,
    const nsTArray<nsCString>& aTrackingFullHashes,
    const Maybe<ContentBlockingNotifier::StorageAccessGrantedReason>& aReason) {
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
    // Get the browser parent from the manager directly since the content
    // blocking event could happen in the early stage of loading, i.e.
    // accessing cookies for the http header. At this stage, the actor is
    // not ready, so we would get a nullptr from GetBrowserParent(). But,
    // we can actually get it from the manager.
    RefPtr<BrowserParent> browserParent =
        static_cast<BrowserParent*>(Manager());
    if (NS_WARN_IF(!browserParent)) {
      return;
    }

    nsCOMPtr<nsIBrowser> browser;
    nsCOMPtr<nsIWebProgress> manager;
    nsCOMPtr<nsIWebProgressListener> managerAsListener;

    if (!browserParent->GetWebProgressListener(
            getter_AddRefs(browser), getter_AddRefs(manager),
            getter_AddRefs(managerAsListener))) {
      return;
    }

    nsCOMPtr<nsIWebProgress> webProgress =
        new RemoteWebProgress(manager, OuterWindowId(), InnerWindowId(), 0,
                              false, BrowsingContext()->IsTopContent());

    Unused << managerAsListener->OnContentBlockingEvent(webProgress, aRequest,
                                                        event.value());
  }
}

already_AddRefed<JSWindowActorParent> WindowGlobalParent::GetActor(
    const nsAString& aName, ErrorResult& aRv) {
  if (!CanSend()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // Check if this actor has already been created, and return it if it has.
  if (mWindowActors.Contains(aName)) {
    return do_AddRef(mWindowActors.GetWeak(aName));
  }

  // Otherwise, we want to create a new instance of this actor.
  JS::RootedObject obj(RootingCx());
  ConstructActor(aName, &obj, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Unwrap our actor to a JSWindowActorParent object.
  RefPtr<JSWindowActorParent> actor;
  if (NS_FAILED(UNWRAP_OBJECT(JSWindowActorParent, &obj, actor))) {
    return nullptr;
  }

  MOZ_RELEASE_ASSERT(!actor->GetManager(),
                     "mManager was already initialized once!");
  actor->Init(aName, this);
  mWindowActors.Put(aName, RefPtr{actor});
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
  // Note that our WindowContext has become discarded.
  WindowContext::Discard();

  ContentParent* cp = nullptr;
  if (!mInProcess) {
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
  if (!mInProcess) {
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
  nsRefPtrHashtable<nsStringHashKey, JSWindowActorParent> windowActors;
  mWindowActors.SwapElements(windowActors);
  for (auto iter = windowActors.Iter(); !iter.Done(); iter.Next()) {
    iter.Data()->RejectPendingQueries();
    iter.Data()->AfterDestroy();
  }
  windowActors.Clear();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(ToSupports(this), "window-global-destroyed", nullptr);
  }
}

WindowGlobalParent::~WindowGlobalParent() {
  MOZ_ASSERT(!mWindowActors.Count());
}

JSObject* WindowGlobalParent::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return WindowGlobalParent_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* WindowGlobalParent::GetParentObject() {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

nsIContentParent* WindowGlobalParent::GetContentParent() {
  RefPtr<BrowserParent> browserParent = GetBrowserParent();
  if (!browserParent) {
    return nullptr;
  }

  return browserParent->Manager();
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(WindowGlobalParent, WindowContext,
                                   mWindowActors)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(WindowGlobalParent,
                                               WindowContext)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WindowGlobalParent)
NS_INTERFACE_MAP_END_INHERITING(WindowContext)

NS_IMPL_ADDREF_INHERITED(WindowGlobalParent, WindowContext)
NS_IMPL_RELEASE_INHERITED(WindowGlobalParent, WindowContext)

}  // namespace dom
}  // namespace mozilla
