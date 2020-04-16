/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef ACCESSIBILITY
#  include "mozilla/a11y/DocAccessibleParent.h"
#endif
#include "mozilla/dom/BrowserBridgeParent.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/layers/InputAPZContext.h"

using namespace mozilla::ipc;
using namespace mozilla::layout;
using namespace mozilla::hal;

namespace mozilla {
namespace dom {

BrowserBridgeParent::BrowserBridgeParent() = default;

BrowserBridgeParent::~BrowserBridgeParent() { Destroy(); }

nsresult BrowserBridgeParent::InitWithProcess(
    ContentParent* aContentParent, const nsString& aPresentationURL,
    const WindowGlobalInit& aWindowInit, uint32_t aChromeFlags, TabId aTabId) {
  if (aWindowInit.browsingContext().IsNullOrDiscarded()) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<CanonicalBrowsingContext> browsingContext =
      aWindowInit.browsingContext().get_canonical();

  // We can inherit most TabContext fields for the new BrowserParent actor from
  // our Manager BrowserParent. We also need to sync the first party domain if
  // the content principal exists.
  MutableTabContext tabContext;
  OriginAttributes attrs;
  attrs = Manager()->OriginAttributesRef();
  RefPtr<nsIPrincipal> principal = Manager()->GetContentPrincipal();
  if (principal) {
    attrs.SetFirstPartyDomain(
        true, principal->OriginAttributesRef().mFirstPartyDomain);
  }

  tabContext.SetTabContext(false, Manager()->ChromeOuterWindowID(),
                           Manager()->ShowFocusRings(), attrs, aPresentationURL,
                           Manager()->GetMaxTouchPoints());

  // Ensure that our content process is subscribed to our newly created
  // BrowsingContextGroup.
  browsingContext->Group()->EnsureSubscribed(aContentParent);
  browsingContext->SetOwnerProcessId(aContentParent->ChildID());

  // Construct the BrowserParent object for our subframe.
  auto browserParent = MakeRefPtr<BrowserParent>(
      aContentParent, aTabId, tabContext, browsingContext, aChromeFlags);
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

  auto windowParent =
      MakeRefPtr<WindowGlobalParent>(aWindowInit, /* inprocess */ false);

  ManagedEndpoint<PWindowGlobalChild> windowChildEp =
      browserParent->OpenPWindowGlobalEndpoint(windowParent);
  if (NS_WARN_IF(!windowChildEp.IsValid())) {
    MOZ_ASSERT(false, "WindowGlobal Open Endpoint Failed");
    return NS_ERROR_FAILURE;
  }

  // Tell the content process to set up its PBrowserChild.
  bool ok = aContentParent->SendConstructBrowser(
      std::move(childEp), std::move(windowChildEp), aTabId,
      tabContext.AsIPCTabContext(), aWindowInit, aChromeFlags,
      aContentParent->ChildID(), aContentParent->IsForBrowser(),
      /* aIsTopLevel */ false);
  if (NS_WARN_IF(!ok)) {
    MOZ_ASSERT(false, "Browser Constructor Failed");
    return NS_ERROR_FAILURE;
  }

  // Set our BrowserParent object to the newly created browser.
  mBrowserParent = std::move(browserParent);
  mBrowserParent->SetOwnerElement(Manager()->GetOwnerElement());
  mBrowserParent->InitRendering();

  windowParent->Init(aWindowInit);

  // Send the newly created layers ID back into content.
  Unused << SendSetLayersId(mBrowserParent->GetLayersId());
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
    mBrowserParent->Destroy();
    mBrowserParent->SetBrowserBridgeParent(nullptr);
    mBrowserParent = nullptr;
  }
}

IPCResult BrowserBridgeParent::RecvShow(const OwnerShowInfo& aOwnerInfo) {
  mBrowserParent->AttachLayerManager();
  Unused << mBrowserParent->SendShow(mBrowserParent->GetShowInfo(), aOwnerInfo);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvScrollbarPreferenceChanged(
    ScrollbarPreference aPref) {
  Unused << mBrowserParent->SendScrollbarPreferenceChanged(aPref);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvLoadURL(const nsCString& aUrl) {
  Unused << mBrowserParent->SendLoadURL(aUrl, mBrowserParent->GetShowInfo());
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

IPCResult BrowserBridgeParent::RecvActivate() {
  mBrowserParent->Activate();
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvDeactivate(const bool& aWindowLowering) {
  mBrowserParent->Deactivate(aWindowLowering);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvSetIsUnderHiddenEmbedderElement(
    const bool& aIsUnderHiddenEmbedderElement) {
  Unused << mBrowserParent->SendSetIsUnderHiddenEmbedderElement(
      aIsUnderHiddenEmbedderElement);
  return IPC_OK();
}

#ifdef ACCESSIBILITY
IPCResult BrowserBridgeParent::RecvSetEmbedderAccessible(
    PDocAccessibleParent* aDoc, uint64_t aID) {
  mEmbedderAccessibleDoc = static_cast<a11y::DocAccessibleParent*>(aDoc);
  mEmbedderAccessibleID = aID;
  if (auto embeddedBrowser = GetBrowserParent()) {
    a11y::DocAccessibleParent* childDocAcc =
        embeddedBrowser->GetTopLevelDocAccessible();
    if (childDocAcc && !childDocAcc->IsShutdown()) {
      // The embedded DocAccessibleParent has already been created. This can
      // happen if, for example, an iframe is hidden and then shown or
      // an iframe is reflowed by layout.
      mEmbedderAccessibleDoc->AddChildDoc(childDocAcc, aID,
                                          /* aCreating */ false);
    }
  }
  return IPC_OK();
}
#endif

void BrowserBridgeParent::ActorDestroy(ActorDestroyReason aWhy) { Destroy(); }

}  // namespace dom
}  // namespace mozilla
