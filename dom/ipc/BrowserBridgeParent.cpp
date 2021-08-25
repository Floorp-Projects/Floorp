/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef ACCESSIBILITY
#  include "mozilla/a11y/DocAccessibleParent.h"
#endif

#include "mozilla/MouseEvents.h"
#include "mozilla/dom/BrowserBridgeParent.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/InputAPZContext.h"

using namespace mozilla::ipc;
using namespace mozilla::layout;
using namespace mozilla::hal;

namespace mozilla::dom {

BrowserBridgeParent::BrowserBridgeParent() = default;

BrowserBridgeParent::~BrowserBridgeParent() { Destroy(); }

nsresult BrowserBridgeParent::InitWithProcess(
    BrowserParent* aParentBrowser, ContentParent* aContentParent,
    const WindowGlobalInit& aWindowInit, uint32_t aChromeFlags, TabId aTabId) {
  MOZ_ASSERT(!CanSend(),
             "This should be called before the object is connected to IPC");
  MOZ_DIAGNOSTIC_ASSERT(!aContentParent->IsLaunching());
  MOZ_DIAGNOSTIC_ASSERT(!aContentParent->IsDead());

  RefPtr<CanonicalBrowsingContext> browsingContext =
      CanonicalBrowsingContext::Get(aWindowInit.context().mBrowsingContextId);
  if (!browsingContext || browsingContext->IsDiscarded()) {
    return NS_ERROR_UNEXPECTED;
  }

  MOZ_DIAGNOSTIC_ASSERT(
      !browsingContext->GetBrowserParent(),
      "BrowsingContext must have had previous BrowserParent cleared");

  MOZ_DIAGNOSTIC_ASSERT(
      aParentBrowser->Manager() != aContentParent,
      "Cannot create OOP iframe in the same process as its parent document");

  // Unfortunately, due to the current racy destruction of BrowsingContext
  // instances when Fission is enabled, while `browsingContext` may not be
  // discarded, an ancestor might be.
  //
  // A discarded ancestor will cause us issues when creating our `BrowserParent`
  // in the new content process, so abort the attempt if we have one.
  //
  // FIXME: We should never have a non-discarded BrowsingContext with discarded
  // ancestors. (bug 1634759)
  if (NS_WARN_IF(!browsingContext->AncestorsAreCurrent())) {
    return NS_ERROR_UNEXPECTED;
  }

  // Ensure that our content process is subscribed to our newly created
  // BrowsingContextGroup.
  browsingContext->Group()->EnsureHostProcess(aContentParent);
  browsingContext->SetOwnerProcessId(aContentParent->ChildID());

  // Construct the BrowserParent object for our subframe.
  auto browserParent = MakeRefPtr<BrowserParent>(
      aContentParent, aTabId, *aParentBrowser, browsingContext, aChromeFlags);
  browserParent->SetBrowserBridgeParent(this);

  // Open a remote endpoint for our PBrowser actor.
  ManagedEndpoint<PBrowserChild> childEp =
      aContentParent->OpenPBrowserEndpoint(browserParent);
  if (NS_WARN_IF(!childEp.IsValid())) {
    MOZ_ASSERT(false, "Browser Open Endpoint Failed");
    return NS_ERROR_FAILURE;
  }

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  cpm->RegisterRemoteFrame(browserParent);

  RefPtr<WindowGlobalParent> windowParent =
      WindowGlobalParent::CreateDisconnected(aWindowInit);
  if (!windowParent) {
    return NS_ERROR_UNEXPECTED;
  }

  ManagedEndpoint<PWindowGlobalChild> windowChildEp =
      browserParent->OpenPWindowGlobalEndpoint(windowParent);
  if (NS_WARN_IF(!windowChildEp.IsValid())) {
    MOZ_ASSERT(false, "WindowGlobal Open Endpoint Failed");
    return NS_ERROR_FAILURE;
  }

  MOZ_DIAGNOSTIC_ASSERT(!browsingContext->IsDiscarded(),
                        "bc cannot have become discarded");

  // Tell the content process to set up its PBrowserChild.
  bool ok = aContentParent->SendConstructBrowser(
      std::move(childEp), std::move(windowChildEp), aTabId,
      browserParent->AsIPCTabContext(), aWindowInit, aChromeFlags,
      aContentParent->ChildID(), aContentParent->IsForBrowser(),
      /* aIsTopLevel */ false);
  if (NS_WARN_IF(!ok)) {
    MOZ_ASSERT(false, "Browser Constructor Failed");
    return NS_ERROR_FAILURE;
  }

  // Set our BrowserParent object to the newly created browser.
  mBrowserParent = std::move(browserParent);
  mBrowserParent->SetOwnerElement(aParentBrowser->GetOwnerElement());
  mBrowserParent->InitRendering();

  GetBrowsingContext()->SetCurrentBrowserParent(mBrowserParent);

  windowParent->Init();
  return NS_OK;
}

CanonicalBrowsingContext* BrowserBridgeParent::GetBrowsingContext() {
  return mBrowserParent->GetBrowsingContext();
}

BrowserParent* BrowserBridgeParent::Manager() {
  MOZ_ASSERT(CanSend());
  return static_cast<BrowserParent*>(PBrowserBridgeParent::Manager());
}

void BrowserBridgeParent::Destroy() {
  if (mBrowserParent) {
#ifdef ACCESSIBILITY
    if (mEmbedderAccessibleDoc && !mEmbedderAccessibleDoc->IsShutdown()) {
      mEmbedderAccessibleDoc->RemovePendingOOPChildDoc(this);
    }
#endif
    mBrowserParent->Destroy();
    mBrowserParent->SetBrowserBridgeParent(nullptr);
    mBrowserParent = nullptr;
  }
}

IPCResult BrowserBridgeParent::RecvShow(const OwnerShowInfo& aOwnerInfo) {
  mBrowserParent->AttachWindowRenderer();
  Unused << mBrowserParent->SendShow(mBrowserParent->GetShowInfo(), aOwnerInfo);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvScrollbarPreferenceChanged(
    ScrollbarPreference aPref) {
  Unused << mBrowserParent->SendScrollbarPreferenceChanged(aPref);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvLoadURL(nsDocShellLoadState* aLoadState) {
  Unused << mBrowserParent->SendLoadURL(aLoadState,
                                        mBrowserParent->GetShowInfo());
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvResumeLoad(uint64_t aPendingSwitchID) {
  mBrowserParent->ResumeLoad(aPendingSwitchID);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvUpdateDimensions(
    const nsIntRect& aRect, const ScreenIntSize& aSize) {
  mBrowserParent->UpdateDimensions(aRect, aSize);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvUpdateEffects(const EffectsInfo& aEffects) {
  Unused << mBrowserParent->SendUpdateEffects(aEffects);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvUpdateRemotePrintSettings(
    const embedding::PrintData& aPrintData) {
  Unused << mBrowserParent->SendUpdateRemotePrintSettings(aPrintData);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvRenderLayers(
    const bool& aEnabled, const layers::LayersObserverEpoch& aEpoch) {
  Unused << mBrowserParent->SendRenderLayers(aEnabled, aEpoch);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvNavigateByKey(
    const bool& aForward, const bool& aForDocumentNavigation) {
  Unused << mBrowserParent->SendNavigateByKey(aForward, aForDocumentNavigation);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvDispatchSynthesizedMouseEvent(
    const WidgetMouseEvent& aEvent) {
  if (aEvent.mMessage != eMouseMove ||
      aEvent.mReason != WidgetMouseEvent::eSynthesized) {
    return IPC_FAIL(this, "Unexpected event type");
  }

  WidgetMouseEvent event = aEvent;
  // Convert mRefPoint from the dispatching child process coordinate space
  // to the parent coordinate space. The SendRealMouseEvent call will convert
  // it into the dispatchee child process coordinate space
  event.mRefPoint = Manager()->TransformChildToParent(event.mRefPoint);
  // We need to set up an InputAPZContext on the stack because
  // BrowserParent::SendRealMouseEvent requires one. But the only thing in
  // that context that is actually used in this scenario is the layers id,
  // and we already have that on the mouse event.
  layers::InputAPZContext context(
      layers::ScrollableLayerGuid(event.mLayersId, 0,
                                  layers::ScrollableLayerGuid::NULL_SCROLL_ID),
      0, nsEventStatus_eIgnore);
  mBrowserParent->SendRealMouseEvent(event);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvWillChangeProcess() {
  Unused << mBrowserParent->SendWillChangeProcess();
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvActivate(uint64_t aActionId) {
  mBrowserParent->Activate(aActionId);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvDeactivate(const bool& aWindowLowering,
                                              uint64_t aActionId) {
  mBrowserParent->Deactivate(aWindowLowering, aActionId);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvSetIsUnderHiddenEmbedderElement(
    const bool& aIsUnderHiddenEmbedderElement) {
  Unused << mBrowserParent->SendSetIsUnderHiddenEmbedderElement(
      aIsUnderHiddenEmbedderElement);
  return IPC_OK();
}

#ifdef ACCESSIBILITY
a11y::DocAccessibleParent* BrowserBridgeParent::GetDocAccessibleParent() {
  auto* embeddedBrowser = GetBrowserParent();
  if (!embeddedBrowser) {
    return nullptr;
  }
  a11y::DocAccessibleParent* docAcc =
      embeddedBrowser->GetTopLevelDocAccessible();
  return docAcc && !docAcc->IsShutdown() ? docAcc : nullptr;
}

IPCResult BrowserBridgeParent::RecvSetEmbedderAccessible(
    PDocAccessibleParent* aDoc, uint64_t aID) {
  MOZ_ASSERT(aDoc || mEmbedderAccessibleDoc,
             "Embedder doc shouldn't be cleared if it wasn't set");
  MOZ_ASSERT(!mEmbedderAccessibleDoc || !aDoc || mEmbedderAccessibleDoc == aDoc,
             "Embedder doc shouldn't change from one doc to another");
  if (!aDoc && mEmbedderAccessibleDoc &&
      !mEmbedderAccessibleDoc->IsShutdown()) {
    // We're clearing the embedder doc, so remove the pending child doc addition
    // (if any).
    mEmbedderAccessibleDoc->RemovePendingOOPChildDoc(this);
  }
  mEmbedderAccessibleDoc = static_cast<a11y::DocAccessibleParent*>(aDoc);
  mEmbedderAccessibleID = aID;
  if (!aDoc) {
    MOZ_ASSERT(!aID);
    return IPC_OK();
  }
  MOZ_ASSERT(aID);
  if (GetDocAccessibleParent()) {
    // The embedded DocAccessibleParent has already been created. This can
    // happen if, for example, an iframe is hidden and then shown or
    // an iframe is reflowed by layout.
    mEmbedderAccessibleDoc->AddChildDoc(this);
  }
  return IPC_OK();
}

a11y::DocAccessibleParent* BrowserBridgeParent::GetEmbedderAccessibleDoc() {
  return mEmbedderAccessibleDoc && !mEmbedderAccessibleDoc->IsShutdown()
             ? mEmbedderAccessibleDoc.get()
             : nullptr;
}
#endif

void BrowserBridgeParent::ActorDestroy(ActorDestroyReason aWhy) { Destroy(); }

}  // namespace mozilla::dom
