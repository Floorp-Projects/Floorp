/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WindowGlobalChild.h"

#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/MozFrameLoaderOwnerBinding.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/BrowserBridgeChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/SecurityPolicyViolationEvent.h"
#include "mozilla/dom/SessionStoreRestoreData.h"
#include "mozilla/dom/SessionStoreDataCollector.h"
#include "mozilla/dom/WindowGlobalActorsBinding.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/dom/InProcessChild.h"
#include "mozilla/dom/InProcessParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/PresShell.h"
#include "mozilla/ScopeExit.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsFocusManager.h"
#include "nsFrameLoaderOwner.h"
#include "nsGlobalWindowInner.h"
#include "nsFrameLoaderOwner.h"
#include "nsNetUtil.h"
#include "nsQueryObject.h"
#include "nsSerializationHelper.h"
#include "nsFrameLoader.h"

#include "mozilla/dom/JSWindowActorBinding.h"
#include "mozilla/dom/JSWindowActorChild.h"
#include "mozilla/dom/JSActorService.h"
#include "nsIHttpChannelInternal.h"
#include "nsIURIMutator.h"

#ifdef MOZ_GECKO_PROFILER
#  include "GeckoProfiler.h"
#endif

using namespace mozilla::ipc;
using namespace mozilla::dom::ipc;

namespace mozilla::dom {

WindowGlobalChild::WindowGlobalChild(dom::WindowContext* aWindowContext,
                                     nsIPrincipal* aPrincipal,
                                     nsIURI* aDocumentURI)
    : mWindowContext(aWindowContext),
      mDocumentPrincipal(aPrincipal),
      mDocumentURI(aDocumentURI) {
  MOZ_DIAGNOSTIC_ASSERT(mWindowContext);
  MOZ_DIAGNOSTIC_ASSERT(mDocumentPrincipal);

  if (!mDocumentURI) {
    NS_NewURI(getter_AddRefs(mDocumentURI), "about:blank");
  }

#ifdef MOZ_GECKO_PROFILER
  // Registers a DOM Window with the profiler. It re-registers the same Inner
  // Window ID with different URIs because when a Browsing context is first
  // loaded, the first url loaded in it will be about:blank. This call keeps the
  // first non-about:blank registration of window and discards the previous one.
  uint64_t embedderInnerWindowID = 0;
  if (BrowsingContext()->GetParent()) {
    embedderInnerWindowID = BrowsingContext()->GetEmbedderInnerWindowId();
  }
  profiler_register_page(BrowsingContext()->BrowserId(), InnerWindowId(),
                         aDocumentURI->GetSpecOrDefault(),
                         embedderInnerWindowID);
#endif
}

already_AddRefed<WindowGlobalChild> WindowGlobalChild::Create(
    nsGlobalWindowInner* aWindow) {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  // Opener policy is set when we start to load a document. Here, we ensure we
  // have set the correct Opener policy so that it will be available in the
  // parent process through window global child.
  nsCOMPtr<nsIChannel> chan = aWindow->GetDocument()->GetChannel();
  nsCOMPtr<nsILoadInfo> loadInfo = chan ? chan->LoadInfo() : nullptr;
  nsCOMPtr<nsIHttpChannelInternal> httpChan = do_QueryInterface(chan);
  nsILoadInfo::CrossOriginOpenerPolicy policy;
  if (httpChan &&
      loadInfo->GetExternalContentPolicyType() ==
          ExtContentPolicy::TYPE_DOCUMENT &&
      NS_SUCCEEDED(httpChan->GetCrossOriginOpenerPolicy(&policy))) {
    MOZ_DIAGNOSTIC_ASSERT(policy ==
                          aWindow->GetBrowsingContext()->GetOpenerPolicy());
  }
#endif

  WindowGlobalInit init = WindowGlobalActor::WindowInitializer(aWindow);
  RefPtr<WindowGlobalChild> wgc = CreateDisconnected(init);

  // Send the link constructor over PBrowser, or link over PInProcess.
  if (XRE_IsParentProcess()) {
    InProcessChild* ipChild = InProcessChild::Singleton();
    InProcessParent* ipParent = InProcessParent::Singleton();
    if (!ipChild || !ipParent) {
      return nullptr;
    }

    ManagedEndpoint<PWindowGlobalParent> endpoint =
        ipChild->OpenPWindowGlobalEndpoint(wgc);
    ipParent->BindPWindowGlobalEndpoint(std::move(endpoint),
                                        wgc->WindowContext()->Canonical());
  } else {
    RefPtr<BrowserChild> browserChild =
        BrowserChild::GetFrom(static_cast<mozIDOMWindow*>(aWindow));
    MOZ_ASSERT(browserChild);

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    dom::BrowsingContext* bc = aWindow->GetBrowsingContext();
#endif

    MOZ_DIAGNOSTIC_ASSERT(bc->AncestorsAreCurrent());
    MOZ_DIAGNOSTIC_ASSERT(bc->IsInProcess());

    ManagedEndpoint<PWindowGlobalParent> endpoint =
        browserChild->OpenPWindowGlobalEndpoint(wgc);
    browserChild->SendNewWindowGlobal(std::move(endpoint), init);
  }

  wgc->Init();
  wgc->InitWindowGlobal(aWindow);
  return wgc.forget();
}

already_AddRefed<WindowGlobalChild> WindowGlobalChild::CreateDisconnected(
    const WindowGlobalInit& aInit) {
  RefPtr<dom::BrowsingContext> browsingContext =
      dom::BrowsingContext::Get(aInit.context().mBrowsingContextId);

  RefPtr<dom::WindowContext> windowContext =
      dom::WindowContext::GetById(aInit.context().mInnerWindowId);
  MOZ_RELEASE_ASSERT(!windowContext, "Creating duplicate WindowContext");

  // Create our new WindowContext
  if (XRE_IsParentProcess()) {
    windowContext = WindowGlobalParent::CreateDisconnected(aInit);
  } else {
    dom::WindowContext::FieldValues fields = aInit.context().mFields;
    windowContext = new dom::WindowContext(
        browsingContext, aInit.context().mInnerWindowId,
        aInit.context().mOuterWindowId, std::move(fields));
  }

  RefPtr<WindowGlobalChild> windowChild = new WindowGlobalChild(
      windowContext, aInit.principal(), aInit.documentURI());
  windowContext->mIsInProcess = true;
  windowContext->mWindowGlobalChild = windowChild;
  return windowChild.forget();
}

void WindowGlobalChild::Init() {
  MOZ_ASSERT(mWindowContext->mWindowGlobalChild == this);
  mWindowContext->Init();
}

void WindowGlobalChild::InitWindowGlobal(nsGlobalWindowInner* aWindow) {
  mWindowGlobal = aWindow;
}

void WindowGlobalChild::OnNewDocument(Document* aDocument) {
  MOZ_RELEASE_ASSERT(mWindowGlobal);
  MOZ_RELEASE_ASSERT(aDocument);

  // Send a series of messages to update document-specific state on
  // WindowGlobalParent, when we change documents on an existing WindowGlobal.
  // This data is also all sent when we construct a WindowGlobal, so anything
  // added here should also be added to WindowGlobalActor::WindowInitializer.

  // FIXME: Perhaps these should be combined into a smaller number of messages?
  SetDocumentURI(aDocument->GetDocumentURI());
  SetDocumentPrincipal(aDocument->NodePrincipal());

  nsCOMPtr<nsITransportSecurityInfo> securityInfo;
  if (nsCOMPtr<nsIChannel> channel = aDocument->GetChannel()) {
    nsCOMPtr<nsISupports> securityInfoSupports;
    channel->GetSecurityInfo(getter_AddRefs(securityInfoSupports));
    securityInfo = do_QueryInterface(securityInfoSupports);
  }
  SendUpdateDocumentSecurityInfo(securityInfo);

  SendUpdateDocumentCspSettings(aDocument->GetBlockAllMixedContent(false),
                                aDocument->GetUpgradeInsecureRequests(false));
  SendUpdateSandboxFlags(aDocument->GetSandboxFlags());

  net::CookieJarSettingsArgs csArgs;
  net::CookieJarSettings::Cast(aDocument->CookieJarSettings())
      ->Serialize(csArgs);
  if (!SendUpdateCookieJarSettings(csArgs)) {
    NS_WARNING(
        "Failed to update document's cookie jar settings on the "
        "WindowGlobalParent");
  }

  SendUpdateHttpsOnlyStatus(aDocument->HttpsOnlyStatus());

  // Update window context fields for the newly loaded Document.
  WindowContext::Transaction txn;
  txn.SetCookieBehavior(
      Some(aDocument->CookieJarSettings()->GetCookieBehavior()));
  txn.SetIsOnContentBlockingAllowList(
      aDocument->CookieJarSettings()->GetIsOnContentBlockingAllowList());
  txn.SetIsThirdPartyWindow(aDocument->HasThirdPartyChannel());
  txn.SetIsThirdPartyTrackingResourceWindow(
      nsContentUtils::IsThirdPartyTrackingResourceWindow(mWindowGlobal));
  txn.SetIsSecureContext(mWindowGlobal->IsSecureContext());
  if (auto policy = aDocument->GetEmbedderPolicy()) {
    txn.SetEmbedderPolicy(*policy);
  }

  if (nsCOMPtr<nsIChannel> channel = aDocument->GetChannel()) {
    nsCOMPtr<nsILoadInfo> loadInfo(channel->LoadInfo());
    txn.SetIsOriginalFrameSource(loadInfo->GetOriginalFrameSrcLoad());
  } else {
    txn.SetIsOriginalFrameSource(false);
  }

  // Init Mixed Content Fields
  nsCOMPtr<nsIURI> innerDocURI =
      NS_GetInnermostURI(aDocument->GetDocumentURI());
  if (innerDocURI) {
    txn.SetIsSecure(innerDocURI->SchemeIs("https"));
  }

  MOZ_DIAGNOSTIC_ASSERT(mDocumentPrincipal->GetIsLocalIpAddress() ==
                        mWindowContext->IsLocalIP());

  MOZ_ALWAYS_SUCCEEDS(txn.Commit(mWindowContext));
}

/* static */
already_AddRefed<WindowGlobalChild> WindowGlobalChild::GetByInnerWindowId(
    uint64_t aInnerWindowId) {
  if (RefPtr<dom::WindowContext> context =
          dom::WindowContext::GetById(aInnerWindowId)) {
    return do_AddRef(context->GetWindowGlobalChild());
  }
  return nullptr;
}

dom::BrowsingContext* WindowGlobalChild::BrowsingContext() {
  return mWindowContext->GetBrowsingContext();
}

uint64_t WindowGlobalChild::InnerWindowId() {
  return mWindowContext->InnerWindowId();
}

uint64_t WindowGlobalChild::OuterWindowId() {
  return mWindowContext->OuterWindowId();
}

bool WindowGlobalChild::IsCurrentGlobal() {
  return CanSend() && mWindowGlobal->IsCurrentInnerWindow();
}

already_AddRefed<WindowGlobalParent> WindowGlobalChild::GetParentActor() {
  if (!CanSend()) {
    return nullptr;
  }
  IProtocol* otherSide = InProcessChild::ParentActorFor(this);
  return do_AddRef(static_cast<WindowGlobalParent*>(otherSide));
}

already_AddRefed<BrowserChild> WindowGlobalChild::GetBrowserChild() {
  if (IsInProcess() || !CanSend()) {
    return nullptr;
  }
  return do_AddRef(static_cast<BrowserChild*>(Manager()));
}

uint64_t WindowGlobalChild::ContentParentId() {
  if (XRE_IsParentProcess()) {
    return 0;
  }
  return ContentChild::GetSingleton()->GetID();
}

// A WindowGlobalChild is the root in its process if it has no parent, or its
// embedder is in a different process.
bool WindowGlobalChild::IsProcessRoot() {
  if (!BrowsingContext()->GetParent()) {
    return true;
  }

  return !BrowsingContext()->GetEmbedderElement();
}

void WindowGlobalChild::BeforeUnloadAdded() {
  // Don't bother notifying the parent if we don't have an IPC link open.
  if (mBeforeUnloadListeners == 0 && CanSend()) {
    Unused << mWindowContext->SetHasBeforeUnload(true);
  }

  mBeforeUnloadListeners++;
  MOZ_ASSERT(mBeforeUnloadListeners > 0);
}

void WindowGlobalChild::BeforeUnloadRemoved() {
  mBeforeUnloadListeners--;
  MOZ_ASSERT(mBeforeUnloadListeners >= 0);

  if (mBeforeUnloadListeners == 0) {
    Unused << mWindowContext->SetHasBeforeUnload(false);
  }
}

void WindowGlobalChild::Destroy() {
  JSActorWillDestroy();

  mWindowContext->Discard();

  // Perform async IPC shutdown unless we're not in-process, and our
  // BrowserChild is in the process of being destroyed, which will destroy
  // us as well.
  RefPtr<BrowserChild> browserChild = GetBrowserChild();
  if (!browserChild || !browserChild->IsDestroyed()) {
    SendDestroy();
  }

  if (mSessionStoreDataCollector) {
    mSessionStoreDataCollector->Cancel();
    mSessionStoreDataCollector = nullptr;
  }
}

mozilla::ipc::IPCResult WindowGlobalChild::RecvMakeFrameLocal(
    const MaybeDiscarded<dom::BrowsingContext>& aFrameContext,
    uint64_t aPendingSwitchId) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsContentProcess());

  MOZ_LOG(BrowsingContext::GetLog(), LogLevel::Debug,
          ("RecvMakeFrameLocal ID=%" PRIx64, aFrameContext.ContextId()));

  if (NS_WARN_IF(aFrameContext.IsNullOrDiscarded())) {
    return IPC_OK();
  }
  dom::BrowsingContext* frameContext = aFrameContext.get();

  RefPtr<Element> embedderElt = frameContext->GetEmbedderElement();
  if (NS_WARN_IF(!embedderElt)) {
    return IPC_OK();
  }

  if (NS_WARN_IF(embedderElt->GetOwnerGlobal() != GetWindowGlobal())) {
    return IPC_OK();
  }

  RefPtr<nsFrameLoaderOwner> flo = do_QueryObject(embedderElt);
  MOZ_DIAGNOSTIC_ASSERT(flo, "Embedder must be a nsFrameLoaderOwner");

  // Trigger a process switch into the current process.
  RemotenessOptions options;
  options.mRemoteType = NOT_REMOTE_TYPE;
  options.mPendingSwitchID.Construct(aPendingSwitchId);
  options.mSwitchingInProgressLoad = true;
  flo->ChangeRemoteness(options, IgnoreErrors());
  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalChild::RecvMakeFrameRemote(
    const MaybeDiscarded<dom::BrowsingContext>& aFrameContext,
    ManagedEndpoint<PBrowserBridgeChild>&& aEndpoint, const TabId& aTabId,
    const LayersId& aLayersId, MakeFrameRemoteResolver&& aResolve) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsContentProcess());

  MOZ_LOG(BrowsingContext::GetLog(), LogLevel::Debug,
          ("RecvMakeFrameRemote ID=%" PRIx64, aFrameContext.ContextId()));

  if (!aLayersId.IsValid()) {
    return IPC_FAIL(this, "Received an invalid LayersId");
  }

  // Resolve the promise when this function exits, as we'll have fully unloaded
  // at that point.
  auto scopeExit = MakeScopeExit([&] { aResolve(true); });

  // Get a BrowsingContext if we're not null or discarded. We don't want to
  // early-return before we connect the BrowserBridgeChild, as otherwise we'll
  // never break the channel in the parent.
  RefPtr<dom::BrowsingContext> frameContext;
  if (!aFrameContext.IsDiscarded()) {
    frameContext = aFrameContext.get();
  }

  // Immediately construct the BrowserBridgeChild so we can destroy it cleanly
  // if the process switch fails.
  RefPtr<BrowserBridgeChild> bridge =
      new BrowserBridgeChild(frameContext, aTabId, aLayersId);
  RefPtr<BrowserChild> manager = GetBrowserChild();
  if (NS_WARN_IF(
          !manager->BindPBrowserBridgeEndpoint(std::move(aEndpoint), bridge))) {
    return IPC_OK();
  }

  auto deleteBridge =
      MakeScopeExit([&] { BrowserBridgeChild::Send__delete__(bridge); });

  // Immediately tear down the actor if we don't have a valid FrameContext.
  if (NS_WARN_IF(aFrameContext.IsNullOrDiscarded())) {
    return IPC_OK();
  }

  RefPtr<Element> embedderElt = frameContext->GetEmbedderElement();
  if (NS_WARN_IF(!embedderElt)) {
    return IPC_OK();
  }

  if (NS_WARN_IF(embedderElt->GetOwnerGlobal() != GetWindowGlobal())) {
    return IPC_OK();
  }

  RefPtr<nsFrameLoaderOwner> flo = do_QueryObject(embedderElt);
  MOZ_DIAGNOSTIC_ASSERT(flo, "Embedder must be a nsFrameLoaderOwner");

  // Trgger a process switch into the specified process.
  IgnoredErrorResult rv;
  flo->ChangeRemotenessWithBridge(bridge, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return IPC_OK();
  }

  // Everything succeeded, so don't delete the bridge.
  deleteBridge.release();

  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalChild::RecvDrawSnapshot(
    const Maybe<IntRect>& aRect, const float& aScale,
    const nscolor& aBackgroundColor, const uint32_t& aFlags,
    DrawSnapshotResolver&& aResolve) {
  aResolve(gfx::PaintFragment::Record(BrowsingContext(), aRect, aScale,
                                      aBackgroundColor,
                                      (gfx::CrossProcessPaintFlags)aFlags));
  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalChild::RecvGetSecurityInfo(
    GetSecurityInfoResolver&& aResolve) {
  Maybe<nsCString> result;

  if (nsCOMPtr<Document> doc = mWindowGlobal->GetDoc()) {
    nsCOMPtr<nsISupports> secInfo;
    nsresult rv = NS_OK;

    // First check if there's a failed channel, in case of a certificate
    // error.
    if (nsIChannel* failedChannel = doc->GetFailedChannel()) {
      rv = failedChannel->GetSecurityInfo(getter_AddRefs(secInfo));
    } else {
      // When there's no failed channel we should have a regular
      // security info on the document. In some cases there's no
      // security info at all, i.e. on HTTP sites.
      secInfo = doc->GetSecurityInfo();
    }

    if (NS_SUCCEEDED(rv) && secInfo) {
      nsCOMPtr<nsISerializable> secInfoSer = do_QueryInterface(secInfo);
      result.emplace();
      NS_SerializeToString(secInfoSer, result.ref());
    }
  }

  aResolve(result);
  return IPC_OK();
}

mozilla::ipc::IPCResult
WindowGlobalChild::RecvSaveStorageAccessPermissionGranted() {
  nsCOMPtr<nsPIDOMWindowInner> inner = GetWindowGlobal();
  if (inner) {
    inner->SaveStorageAccessPermissionGranted();
  }

  nsCOMPtr<nsPIDOMWindowOuter> outer =
      nsPIDOMWindowOuter::GetFromCurrentInner(inner);
  if (outer) {
    nsGlobalWindowOuter::Cast(outer)->SetStorageAccessPermissionGranted(true);
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalChild::RecvDispatchSecurityPolicyViolation(
    const nsString& aViolationEventJSON) {
  nsGlobalWindowInner* window = GetWindowGlobal();
  if (!window) {
    return IPC_OK();
  }

  Document* doc = window->GetDocument();
  if (!doc) {
    return IPC_OK();
  }

  SecurityPolicyViolationEventInit violationEvent;
  if (!violationEvent.Init(aViolationEventJSON)) {
    return IPC_OK();
  }

  RefPtr<Event> event = SecurityPolicyViolationEvent::Constructor(
      doc, u"securitypolicyviolation"_ns, violationEvent);
  event->SetTrusted(true);
  doc->DispatchEvent(*event, IgnoreErrors());
  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalChild::RecvAddBlockedFrameNodeByClassifier(
    const MaybeDiscardedBrowsingContext& aNode) {
  if (aNode.IsNullOrDiscarded()) {
    return IPC_OK();
  }

  nsGlobalWindowInner* window = GetWindowGlobal();
  if (!window) {
    return IPC_OK();
  }

  Document* doc = window->GetDocument();
  if (!doc) {
    return IPC_OK();
  }

  MOZ_ASSERT(aNode.get()->GetEmbedderElement()->OwnerDoc() == doc);
  doc->AddBlockedNodeByClassifier(aNode.get()->GetEmbedderElement());
  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalChild::RecvResetScalingZoom() {
  if (Document* doc = mWindowGlobal->GetExtantDoc()) {
    if (PresShell* ps = doc->GetPresShell()) {
      ps->SetResolutionAndScaleTo(1.0,
                                  ResolutionChangeOrigin::MainThreadAdjustment);
    }
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalChild::RecvSetContainerFeaturePolicy(
    dom::FeaturePolicy* aContainerFeaturePolicy) {
  mContainerFeaturePolicy = aContainerFeaturePolicy;
  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalChild::RecvRestoreTabContent(
    dom::SessionStoreRestoreData* aData, RestoreTabContentResolver&& aResolve) {
  aData->RestoreInto(BrowsingContext());
  aResolve(true);
  return IPC_OK();
}

IPCResult WindowGlobalChild::RecvRawMessage(
    const JSActorMessageMeta& aMeta, const Maybe<ClonedMessageData>& aData,
    const Maybe<ClonedMessageData>& aStack) {
  Maybe<StructuredCloneData> data;
  if (aData) {
    data.emplace();
    data->BorrowFromClonedMessageDataForChild(*aData);
  }
  Maybe<StructuredCloneData> stack;
  if (aStack) {
    stack.emplace();
    stack->BorrowFromClonedMessageDataForChild(*aStack);
  }
  ReceiveRawMessage(aMeta, std::move(data), std::move(stack));
  return IPC_OK();
}

void WindowGlobalChild::SetDocumentURI(nsIURI* aDocumentURI) {
#ifdef MOZ_GECKO_PROFILER
  // Registers a DOM Window with the profiler. It re-registers the same Inner
  // Window ID with different URIs because when a Browsing context is first
  // loaded, the first url loaded in it will be about:blank. This call keeps the
  // first non-about:blank registration of window and discards the previous one.
  uint64_t embedderInnerWindowID = 0;
  if (BrowsingContext()->GetParent()) {
    embedderInnerWindowID = BrowsingContext()->GetEmbedderInnerWindowId();
  }
  profiler_register_page(BrowsingContext()->BrowserId(), InnerWindowId(),
                         aDocumentURI->GetSpecOrDefault(),
                         embedderInnerWindowID);
#endif
  mDocumentURI = aDocumentURI;
  SendUpdateDocumentURI(aDocumentURI);
}

void WindowGlobalChild::SetDocumentPrincipal(
    nsIPrincipal* aNewDocumentPrincipal) {
  MOZ_ASSERT(mDocumentPrincipal->Equals(aNewDocumentPrincipal));
  mDocumentPrincipal = aNewDocumentPrincipal;
  SendUpdateDocumentPrincipal(aNewDocumentPrincipal);
}

const nsACString& WindowGlobalChild::GetRemoteType() {
  if (XRE_IsContentProcess()) {
    return ContentChild::GetSingleton()->GetRemoteType();
  }

  return NOT_REMOTE_TYPE;
}

already_AddRefed<JSWindowActorChild> WindowGlobalChild::GetActor(
    JSContext* aCx, const nsACString& aName, ErrorResult& aRv) {
  return JSActorManager::GetActor(aCx, aName, aRv)
      .downcast<JSWindowActorChild>();
}

already_AddRefed<JSActor> WindowGlobalChild::InitJSActor(
    JS::HandleObject aMaybeActor, const nsACString& aName, ErrorResult& aRv) {
  RefPtr<JSWindowActorChild> actor;
  if (aMaybeActor.get()) {
    aRv = UNWRAP_OBJECT(JSWindowActorChild, aMaybeActor.get(), actor);
    if (aRv.Failed()) {
      return nullptr;
    }
  } else {
    actor = new JSWindowActorChild();
  }

  MOZ_RELEASE_ASSERT(!actor->GetManager(),
                     "mManager was already initialized once!");
  actor->Init(aName, this);
  return actor.forget();
}

void WindowGlobalChild::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript(),
             "Destroying WindowGlobalChild can run script");

  // If our WindowContext hasn't been marked as discarded yet, ensure it's
  // marked as discarded at this point.
  mWindowContext->Discard();

#ifdef MOZ_GECKO_PROFILER
  profiler_unregister_page(InnerWindowId());
#endif

  // Destroy our JSActors, and reject any pending queries.
  JSActorDidDestroy();
}

bool WindowGlobalChild::IsSameOriginWith(
    const dom::WindowContext* aOther) const {
  if (aOther == WindowContext()) {
    return true;
  }

  MOZ_DIAGNOSTIC_ASSERT(WindowContext()->Group() == aOther->Group());
  if (nsGlobalWindowInner* otherWin = aOther->GetInnerWindow()) {
    return mDocumentPrincipal->Equals(otherWin->GetPrincipal());
  }
  return false;
}

bool WindowGlobalChild::SameOriginWithTop() {
  return IsSameOriginWith(WindowContext()->TopWindowContext());
}

WindowGlobalChild::~WindowGlobalChild() = default;

JSObject* WindowGlobalChild::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return WindowGlobalChild_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports* WindowGlobalChild::GetParentObject() {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

void WindowGlobalChild::SetSessionStoreDataCollector(
    SessionStoreDataCollector* aCollector) {
  mSessionStoreDataCollector = aCollector;
}

SessionStoreDataCollector* WindowGlobalChild::GetSessionStoreDataCollector()
    const {
  return mSessionStoreDataCollector;
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WEAK_PTR(WindowGlobalChild, mWindowGlobal,
                                               mContainerFeaturePolicy,
                                               mWindowContext,
                                               mSessionStoreDataCollector)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WindowGlobalChild)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WindowGlobalChild)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WindowGlobalChild)

}  // namespace mozilla::dom
