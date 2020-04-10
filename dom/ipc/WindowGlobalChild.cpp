/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WindowGlobalChild.h"

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
#include "mozilla/ipc/InProcessChild.h"
#include "mozilla/ipc/InProcessParent.h"
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
#include "mozilla/dom/JSWindowActorService.h"
#include "nsIHttpChannelInternal.h"
#include "nsIURIMutator.h"

using namespace mozilla::ipc;
using namespace mozilla::dom::ipc;

namespace mozilla {
namespace dom {

typedef nsRefPtrHashtable<nsUint64HashKey, WindowGlobalChild> WGCByIdMap;
static StaticAutoPtr<WGCByIdMap> gWindowGlobalChildById;

WindowGlobalChild::WindowGlobalChild(const WindowGlobalInit& aInit,
                                     nsGlobalWindowInner* aWindow)
    : mWindowGlobal(aWindow),
      mBrowsingContext(aInit.browsingContext().GetMaybeDiscarded()),
      mDocumentPrincipal(aInit.principal()),
      mDocumentURI(aInit.documentURI()),
      mInnerWindowId(aInit.innerWindowId()),
      mOuterWindowId(aInit.outerWindowId()),
      mBeforeUnloadListeners(0) {
  MOZ_DIAGNOSTIC_ASSERT(mBrowsingContext);
  MOZ_DIAGNOSTIC_ASSERT(mDocumentPrincipal);

  MOZ_ASSERT_IF(aWindow, mInnerWindowId == aWindow->WindowID());
  MOZ_ASSERT_IF(aWindow,
                mOuterWindowId == aWindow->GetOuterWindow()->WindowID());
}

already_AddRefed<WindowGlobalChild> WindowGlobalChild::Create(
    nsGlobalWindowInner* aWindow) {
  nsCOMPtr<nsIPrincipal> principal = aWindow->GetPrincipal();
  MOZ_ASSERT(principal);

  RefPtr<nsDocShell> docshell = nsDocShell::Cast(aWindow->GetDocShell());
  MOZ_ASSERT(docshell);

  // Initalize our WindowGlobalChild object.
  RefPtr<dom::BrowsingContext> bc = docshell->GetBrowsingContext();

  // When creating a new window global child we also need to look at the
  // channel's Cross-Origin-Opener-Policy and set it on the browsing context
  // so it's available in the parent process.
  nsCOMPtr<nsIChannel> chan = aWindow->GetDocument()->GetChannel();
  nsCOMPtr<nsILoadInfo> loadInfo = chan ? chan->LoadInfo() : nullptr;
  nsCOMPtr<nsIHttpChannelInternal> httpChan = do_QueryInterface(chan);
  nsILoadInfo::CrossOriginOpenerPolicy policy;
  if (httpChan &&
      loadInfo->GetExternalContentPolicyType() ==
          nsIContentPolicy::TYPE_DOCUMENT &&
      NS_SUCCEEDED(httpChan->GetCrossOriginOpenerPolicy(&policy))) {
    bc->SetOpenerPolicy(policy);
  }

  WindowGlobalInit init(principal,
                        aWindow->GetDocumentContentBlockingAllowListPrincipal(),
                        aWindow->GetDocumentURI(), bc, aWindow->WindowID(),
                        aWindow->GetOuterWindow()->WindowID());

  auto wgc = MakeRefPtr<WindowGlobalChild>(init, aWindow);

  // If we have already closed our browsing context, return a pre-destroyed
  // WindowGlobalChild actor.
  if (bc->IsDiscarded()) {
    wgc->ActorDestroy(FailedConstructor);
    return wgc.forget();
  }

  // Send the link constructor over PBrowser, or link over PInProcess.
  if (XRE_IsParentProcess()) {
    InProcessChild* ipChild = InProcessChild::Singleton();
    InProcessParent* ipParent = InProcessParent::Singleton();
    if (!ipChild || !ipParent) {
      return nullptr;
    }

    // Note: ref is released in DeallocPWindowGlobalChild
    ManagedEndpoint<PWindowGlobalParent> endpoint =
        ipChild->OpenPWindowGlobalEndpoint(wgc);

    auto wgp = MakeRefPtr<WindowGlobalParent>(init, /* aInProcess */ true);

    // Note: ref is released in DeallocPWindowGlobalParent
    ipParent->BindPWindowGlobalEndpoint(std::move(endpoint), wgp);
    wgp->Init(init);
  } else {
    RefPtr<BrowserChild> browserChild =
        BrowserChild::GetFrom(static_cast<mozIDOMWindow*>(aWindow));
    MOZ_ASSERT(browserChild);

    ManagedEndpoint<PWindowGlobalParent> endpoint =
        browserChild->OpenPWindowGlobalEndpoint(wgc);

    browserChild->SendNewWindowGlobal(std::move(endpoint), init);
  }

  wgc->Init();
  return wgc.forget();
}

void WindowGlobalChild::Init() {
  if (!mDocumentURI) {
    NS_NewURI(getter_AddRefs(mDocumentURI), "about:blank");
  }

  // Ensure we have a corresponding WindowContext object.
  if (XRE_IsParentProcess()) {
    mWindowContext = GetParentActor();
  } else {
    mWindowContext = WindowContext::Create(this);
  }

  // Register this WindowGlobal in the gWindowGlobalParentsById map.
  if (!gWindowGlobalChildById) {
    gWindowGlobalChildById = new WGCByIdMap();
    ClearOnShutdown(&gWindowGlobalChildById);
  }
  auto entry = gWindowGlobalChildById->LookupForAdd(mInnerWindowId);
  MOZ_RELEASE_ASSERT(!entry, "Duplicate WindowGlobalChild entry for ID!");
  entry.OrInsert([&] { return this; });
}

void WindowGlobalChild::InitWindowGlobal(nsGlobalWindowInner* aWindow) {
  mWindowGlobal = aWindow;
}

/* static */
already_AddRefed<WindowGlobalChild> WindowGlobalChild::GetByInnerWindowId(
    uint64_t aInnerWindowId) {
  if (!gWindowGlobalChildById) {
    return nullptr;
  }
  return gWindowGlobalChildById->Get(aInnerWindowId);
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
        nsTArray<RefPtr<JSWindowActorChild>> windowActors(
            self->mWindowActors.Count());
        for (auto iter = self->mWindowActors.Iter(); !iter.Done();
             iter.Next()) {
          windowActors.AppendElement(iter.UserData());
        }

        for (auto& windowActor : windowActors) {
          windowActor->StartDestroy();
        }

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
  options.mRemoteType.Assign(VoidString());
  options.mPendingSwitchID.Construct(aPendingSwitchId);
  flo->ChangeRemoteness(options, IgnoreErrors());
  return IPC_OK();
}

mozilla::ipc::IPCResult WindowGlobalChild::RecvMakeFrameRemote(
    const MaybeDiscarded<dom::BrowsingContext>& aFrameContext,
    ManagedEndpoint<PBrowserBridgeChild>&& aEndpoint, const TabId& aTabId,
    MakeFrameRemoteResolver&& aResolve) {
  MOZ_DIAGNOSTIC_ASSERT(XRE_IsContentProcess());

  MOZ_LOG(BrowsingContext::GetLog(), LogLevel::Debug,
          ("RecvMakeFrameRemote ID=%" PRIx64, aFrameContext.ContextId()));

  // Immediately resolve the promise, acknowledging the request.
  aResolve(true);

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
      new BrowserBridgeChild(frameContext, aTabId);
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
      doc, NS_LITERAL_STRING("securitypolicyviolation"), violationEvent);
  event->SetTrusted(true);
  doc->DispatchEvent(*event, IgnoreErrors());
  return IPC_OK();
}

IPCResult WindowGlobalChild::RecvRawMessage(
    const JSWindowActorMessageMeta& aMeta, const ClonedMessageData& aData,
    const ClonedMessageData& aStack) {
  StructuredCloneData data;
  data.BorrowFromClonedMessageDataForChild(aData);
  StructuredCloneData stack;
  stack.BorrowFromClonedMessageDataForChild(aStack);
  ReceiveRawMessage(aMeta, std::move(data), std::move(stack));
  return IPC_OK();
}

void WindowGlobalChild::ReceiveRawMessage(const JSWindowActorMessageMeta& aMeta,
                                          StructuredCloneData&& aData,
                                          StructuredCloneData&& aStack) {
  RefPtr<JSWindowActorChild> actor =
      GetActor(aMeta.actorName(), IgnoreErrors());
  if (actor) {
    actor->ReceiveRawMessage(aMeta, std::move(aData), std::move(aStack));
  }
}

void WindowGlobalChild::SetDocumentURI(nsIURI* aDocumentURI) {
#ifdef MOZ_GECKO_PROFILER
  // Registers a DOM Window with the profiler. It re-registers the same Inner
  // Window ID with different URIs because when a Browsing context is first
  // loaded, the first url loaded in it will be about:blank. This call keeps the
  // first non-about:blank registration of window and discards the previous one.
  uint64_t embedderInnerWindowID = 0;
  if (mBrowsingContext->GetParent()) {
    embedderInnerWindowID = mBrowsingContext->GetEmbedderInnerWindowId();
  }
  profiler_register_page(mBrowsingContext->Id(), mInnerWindowId,
                         aDocumentURI->GetSpecOrDefault(),
                         embedderInnerWindowID);
#endif
  mDocumentURI = aDocumentURI;
  SendUpdateDocumentURI(aDocumentURI);
}

const nsAString& WindowGlobalChild::GetRemoteType() {
  if (XRE_IsContentProcess()) {
    return ContentChild::GetSingleton()->GetRemoteType();
  }

  return VoidString();
}

already_AddRefed<JSWindowActorChild> WindowGlobalChild::GetActor(
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

  // Unwrap our actor to a JSWindowActorChild object.
  RefPtr<JSWindowActorChild> actor;
  if (NS_FAILED(UNWRAP_OBJECT(JSWindowActorChild, &obj, actor))) {
    return nullptr;
  }

  MOZ_RELEASE_ASSERT(!actor->GetManager(),
                     "mManager was already initialized once!");
  actor->Init(aName, this);
  mWindowActors.Put(aName, RefPtr{actor});
  return actor.forget();
}

void WindowGlobalChild::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(nsContentUtils::IsSafeToRunScript(),
             "Destroying WindowGlobalChild can run script");

  gWindowGlobalChildById->Remove(mInnerWindowId);

#ifdef MOZ_GECKO_PROFILER
  profiler_unregister_page(mInnerWindowId);
#endif

  // Destroy our JSWindowActors, and reject any pending queries.
  nsRefPtrHashtable<nsStringHashKey, JSWindowActorChild> windowActors;
  mWindowActors.SwapElements(windowActors);
  for (auto iter = windowActors.Iter(); !iter.Done(); iter.Next()) {
    iter.Data()->RejectPendingQueries();
    iter.Data()->AfterDestroy();
  }
  windowActors.Clear();
}

WindowGlobalChild::~WindowGlobalChild() {
  MOZ_ASSERT(!gWindowGlobalChildById ||
             !gWindowGlobalChildById->Contains(mInnerWindowId));
  MOZ_ASSERT(!mWindowActors.Count());
}

JSObject* WindowGlobalChild::WrapObject(JSContext* aCx,
                                        JS::Handle<JSObject*> aGivenProto) {
  return WindowGlobalChild_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports* WindowGlobalChild::GetParentObject() {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WindowGlobalChild, mWindowGlobal,
                                      mBrowsingContext, mWindowActors)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WindowGlobalChild)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(WindowGlobalChild)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WindowGlobalChild)

}  // namespace dom
}  // namespace mozilla
