/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowserBridgeParent.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/layers/InputAPZContext.h"

using namespace mozilla::ipc;
using namespace mozilla::layout;
using namespace mozilla::hal;

namespace mozilla {
namespace dom {

BrowserBridgeParent::BrowserBridgeParent() : mIPCOpen(false) {}

BrowserBridgeParent::~BrowserBridgeParent() { Destroy(); }

nsresult BrowserBridgeParent::Init(const nsString& aPresentationURL,
                                   const nsString& aRemoteType,
                                   CanonicalBrowsingContext* aBrowsingContext,
                                   const uint32_t& aChromeFlags) {
  mIPCOpen = true;

  // FIXME: This should actually use a non-bogus TabContext, probably inherited
  // from our Manager().
  OriginAttributes attrs;
  attrs.mInIsolatedMozBrowser = false;
  attrs.SyncAttributesWithPrivateBrowsing(false);
  MutableTabContext tabContext;
  tabContext.SetTabContext(false, 0, UIStateChangeType_Set,
                           UIStateChangeType_Set, attrs, aPresentationURL);

  ProcessPriority initialPriority = PROCESS_PRIORITY_FOREGROUND;

  // Get our ConstructorSender object.
  RefPtr<ContentParent> constructorSender =
      ContentParent::GetNewOrUsedBrowserProcess(
          nullptr, aRemoteType, initialPriority, nullptr, false);
  if (NS_WARN_IF(!constructorSender)) {
    MOZ_ASSERT(false, "Unable to allocate content process!");
    return NS_ERROR_FAILURE;
  }

  // Ensure that our content process is subscribed to our newly created
  // BrowsingContextGroup.
  aBrowsingContext->Group()->EnsureSubscribed(constructorSender);
  aBrowsingContext->SetOwnerProcessId(constructorSender->ChildID());

  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  TabId tabId(nsContentUtils::GenerateTabId());
  cpm->RegisterRemoteFrame(tabId, ContentParentId(0), TabId(0),
                           tabContext.AsIPCTabContext(),
                           constructorSender->ChildID());

  // Construct the BrowserParent object for our subframe.
  RefPtr<BrowserParent> browserParent(
      new BrowserParent(constructorSender, tabId, tabContext, aBrowsingContext,
                        aChromeFlags, this));

  // Open a remote endpoint for our PBrowser actor. DeallocPBrowserParent
  // releases the ref taken.
  ManagedEndpoint<PBrowserChild> childEp =
      constructorSender->OpenPBrowserEndpoint(do_AddRef(browserParent).take());
  if (NS_WARN_IF(!childEp.IsValid())) {
    MOZ_ASSERT(false, "Browser Open Endpoint Failed");
    return NS_ERROR_FAILURE;
  }

  // Tell the content process to set up its PBrowserChild.
  bool ok = constructorSender->SendConstructBrowser(
      std::move(childEp), tabId, TabId(0), tabContext.AsIPCTabContext(),
      aBrowsingContext, aChromeFlags, constructorSender->ChildID(),
      constructorSender->IsForBrowser());
  if (NS_WARN_IF(!ok)) {
    MOZ_ASSERT(false, "Browser Constructor Failed");
    return NS_ERROR_FAILURE;
  }

  // Set our BrowserParent object to the newly created browser.
  mBrowserParent = browserParent.forget();
  mBrowserParent->SetOwnerElement(Manager()->GetOwnerElement());
  mBrowserParent->InitRendering();

  RenderFrame* rf = mBrowserParent->GetRenderFrame();
  if (NS_WARN_IF(!rf)) {
    MOZ_ASSERT(false, "No RenderFrame");
    return NS_ERROR_FAILURE;
  }

  // Send the newly created layers ID back into content.
  Unused << SendSetLayersId(rf->GetLayersId());
  return NS_OK;
}

CanonicalBrowsingContext* BrowserBridgeParent::GetBrowsingContext() {
  return mBrowserParent->GetBrowsingContext();
}

BrowserParent* BrowserBridgeParent::Manager() {
  MOZ_ASSERT(mIPCOpen);
  return static_cast<BrowserParent*>(PBrowserBridgeParent::Manager());
}

void BrowserBridgeParent::Destroy() {
  if (mBrowserParent) {
    mBrowserParent->Destroy();
    mBrowserParent = nullptr;
  }
}

IPCResult BrowserBridgeParent::RecvShow(const ScreenIntSize& aSize,
                                        const bool& aParentIsActive,
                                        const nsSizeMode& aSizeMode) {
  RenderFrame* rf = mBrowserParent->GetRenderFrame();
  if (!rf->AttachLayerManager()) {
    MOZ_CRASH();
  }

  Unused << mBrowserParent->SendShow(aSize, mBrowserParent->GetShowInfo(),
                                     aParentIsActive, aSizeMode);
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
    const DimensionInfo& aDimensions) {
  Unused << mBrowserParent->SendUpdateDimensions(aDimensions);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvRenderLayers(
    const bool& aEnabled, const bool& aForceRepaint,
    const layers::LayersObserverEpoch& aEpoch) {
  Unused << mBrowserParent->SendRenderLayers(aEnabled, aForceRepaint, aEpoch);
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

IPCResult BrowserBridgeParent::RecvSkipBrowsingContextDetach() {
  mBrowserParent->SkipBrowsingContextDetach();
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

void BrowserBridgeParent::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCOpen = false;
  Destroy();
}

}  // namespace dom
}  // namespace mozilla
