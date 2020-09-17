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
#include "mozilla/dom/WindowGlobalActorsBinding.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/dom/InProcessChild.h"
#include "mozilla/dom/InProcessParent.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsFocusManager.h"
#include "nsFrameLoaderOwner.h"
#include "nsGlobalWindowInner.h"
#include "nsFrameLoaderOwner.h"
#include "nsQueryObject.h"
#include "nsSerializationHelper.h"
#include "nsFrameLoader.h"

#include "mozilla/dom/JSWindowActorBinding.h"
#include "mozilla/dom/JSWindowActorChild.h"
#include "mozilla/dom/JSActorService.h"
#include "nsIHttpChannelInternal.h"
#include "nsIURIMutator.h"

using namespace mozilla::ipc;
using namespace mozilla::dom::ipc;

namespace mozilla {
namespace dom {

typedef nsRefPtrHashtable<nsUint64HashKey, WindowGlobalChild> WGCByIdMap;
static StaticAutoPtr<WGCByIdMap> gWindowGlobalChildById;

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
  profiler_register_page(BrowsingContext()->Id(), InnerWindowId(),
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
          nsIContentPolicy::TYPE_DOCUMENT &&
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
    windowContext =
        WindowGlobalParent::CreateDisconnected(aInit, /* aInProcess */ true);
  } else {
    dom::WindowContext::FieldValues fields = aInit.context().mFields;
    windowContext =
        new dom::WindowContext(browsingContext, aInit.context().mInnerWindowId,
                               aInit.context().mOuterWindowId,
                               /* aInProcess */ true, std::move(fields));
  }

  RefPtr<WindowGlobalChild> windowChild = new WindowGlobalChild(
      windowContext, aInit.principal(), aInit.documentURI());
  return windowChild.forget();
}

void WindowGlobalChild::Init() {
  mWindowContext->Init();

  // Register this WindowGlobal in the gWindowGlobalParentsById map.
  if (!gWindowGlobalChildById) {
    gWindowGlobalChildById = new WGCByIdMap();
    ClearOnShutdown(&gWindowGlobalChildById);
  }
  auto entry = gWindowGlobalChildById->LookupForAdd(InnerWindowId());
  MOZ_RELEASE_ASSERT(!entry, "Duplicate WindowGlobalChild entry for ID!");
  entry.OrInsert([&] { return this; });
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
  auto policy = aDocument->GetEmbedderPolicy();
  if (policy.isSome()) {
    txn.SetEmbedderPolicy(policy.ref());
  }

  // Init Mixed Content Fields
  nsCOMPtr<nsIURI> innerDocURI =
      NS_GetInnermostURI(aDocument->GetDocumentURI());
  if (innerDocURI) {
    txn.SetIsSecure(innerDocURI->SchemeIs("https"));
  }
  nsCOMPtr<nsIChannel> mixedChannel;
  mWindowGlobal->GetDocShell()->GetMixedContentChannel(
      getter_AddRefs(mixedChannel));
  // A non null mixedContent channel on the docshell indicates,
  // that the user has overriden mixed content to allow mixed
  // content loads to happen.
  if (mixedChannel && (mixedChannel == aDocument->GetChannel())) {
    txn.SetAllowMixedContent(true);
  }

  MOZ_ALWAYS_SUCCEEDS(txn.Commit(mWindowContext));
}

/* static */
already_AddRefed<WindowGlobalChild> WindowGlobalChild::GetByInnerWindowId(
    uint64_t aInnerWindowId) {
  if (!gWindowGlobalChildById) {
    return nullptr;
  }
  return gWindowGlobalChildById->Get(aInnerWindowId);
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
    SendSetHasBeforeUnload(true);
  }

  mBeforeUnloadListeners++;
  MOZ_ASSERT(mBeforeUnloadListeners > 0);
}

void WindowGlobalChild::BeforeUnloadRemoved() {
  mBeforeUnloadListeners--;
  MOZ_ASSERT(mBeforeUnloadListeners >= 0);

  // Don't bother notifying the parent if we don't have an IPC link open.
  if (mBeforeUnloadListeners == 0 && CanSend()) {
    SendSetHasBeforeUnload(false);
  }
}

void WindowGlobalChild::Destroy() {
  // Destroying a WindowGlobalChild requires running script, so hold off on
  // doing it until we can safely run JS callbacks.
  nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
      "WindowGlobalChild::Destroy", [self = RefPtr<WindowGlobalChild>(this)]() {
        // Make a copy so that we can avoid potential iterator invalidation when
        // calling the user-provided Destroy() methods.
        self->JSActorWillDestroy();

        // Perform async IPC shutdown unless we're not in-process, and our
        // BrowserChild is in the process of being destroyed, which will destroy
        // us as well.
        RefPtr<BrowserChild> browserChild = self->GetBrowserChild();
        if (!browserChild || !browserChild->IsDestroyed()) {
          self->SendDestroy();
        }
      }));
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

  // Immediately resolve the promise, acknowledging the request.
  aResolve(true);

  if (!aLayersId.IsValid()) {
    return IPC_FAIL(this, "Received an invalid LayersId");
  }

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

  // Immediately tear down the actor if we don't have a valid FrameContext.
  if (NS_WARN_IF(aFrameContext.IsNullOrDiscarded())) {
    BrowserBridgeChild::Send__delete__(bridge);
    return IPC_OK();
  }

  RefPtr<Element> embedderElt = frameContext->GetEmbedderElement();
  if (NS_WARN_IF(!embedderElt)) {
    BrowserBridgeChild::Send__delete__(bridge);
    return IPC_OK();
  }

  if (NS_WARN_IF(embedderElt->GetOwnerGlobal() != GetWindowGlobal())) {
    BrowserBridgeChild::Send__delete__(bridge);
    return IPC_OK();
  }

  RefPtr<nsFrameLoaderOwner> flo = do_QueryObject(embedderElt);
  MOZ_DIAGNOSTIC_ASSERT(flo, "Embedder must be a nsFrameLoaderOwner");

  // Trgger a process switch into the specified process.
  IgnoredErrorResult rv;
  flo->ChangeRemotenessWithBridge(bridge, rv);
  if (NS_WARN_IF(rv.Failed())) {
    BrowserBridgeChild::Send__delete__(bridge);
    return IPC_OK();
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalChild::RecvDrawSnapshot(
    const Maybe<IntRect>& aRect, const float& aScale,
    const nscolor& aBackgroundColor, const uint32_t& aFlags,
    DrawSnapshotResolver&& aResolve) {
  nsCOMPtr<nsIDocShell> docShell = BrowsingContext()->GetDocShell();
  if (!docShell) {
    aResolve(gfx::PaintFragment{});
    return IPC_OK();
  }

  aResolve(gfx::PaintFragment::Record(docShell, aRect, aScale, aBackgroundColor,
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

IPCResult WindowGlobalChild::RecvRawMessage(const JSActorMessageMeta& aMeta,
                                            const ClonedMessageData& aData,
                                            const ClonedMessageData& aStack) {
  StructuredCloneData data;
  data.BorrowFromClonedMessageDataForChild(aData);
  StructuredCloneData stack;
  stack.BorrowFromClonedMessageDataForChild(aStack);
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
  profiler_register_page(BrowsingContext()->Id(), InnerWindowId(),
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

  gWindowGlobalChildById->Remove(InnerWindowId());

#ifdef MOZ_GECKO_PROFILER
  profiler_unregister_page(InnerWindowId());
#endif

  // Destroy our JSActors, and reject any pending queries.
  JSActorDidDestroy();
}

bool WindowGlobalChild::SameOriginWithTop() {
  nsGlobalWindowInner* topWindow =
      WindowContext()->TopWindowContext()->GetInnerWindow();
  if (!topWindow) {
    return false;
  }
  return mWindowGlobal == topWindow ||
         mDocumentPrincipal->Equals(topWindow->GetPrincipal());
}

WindowGlobalChild::~WindowGlobalChild() {
  MOZ_ASSERT(!gWindowGlobalChildById ||
             !gWindowGlobalChildById->Contains(InnerWindowId()));
}

JSObject* WindowGlobalChild::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return WindowGlobalChild_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports* WindowGlobalChild::GetParentObject() {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

void WindowGlobalChild::MaybeSendUpdateDocumentWouldPreloadResources() {
  if (!mDocumentWouldPreloadResources) {
    mDocumentWouldPreloadResources = true;
    SendUpdateDocumentWouldPreloadResources();
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WindowGlobalChild, mWindowGlobal)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WindowGlobalChild)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WindowGlobalChild)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WindowGlobalChild)

}  // namespace dom
}  // namespace mozilla
