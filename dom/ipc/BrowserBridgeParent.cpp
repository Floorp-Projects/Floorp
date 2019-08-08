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

BrowserBridgeParent::BrowserBridgeParent()
    :
#ifdef ACCESSIBILITY
      mEmbedderAccessibleID(0),
#endif
      mIPCOpen(false) {
}

BrowserBridgeParent::~BrowserBridgeParent() { Destroy(); }

nsresult BrowserBridgeParent::Init(const nsString& aPresentationURL,
                                   const nsString& aRemoteType,
                                   const WindowGlobalInit& aWindowInit,
                                   const uint32_t& aChromeFlags, TabId aTabId) {
  mIPCOpen = true;

  RefPtr<CanonicalBrowsingContext> browsingContext =
      aWindowInit.browsingContext()->Canonical();

  // We can inherit most TabContext fields for the new BrowserParent actor from
  // our Manager BrowserParent.
  MutableTabContext tabContext;
  tabContext.SetTabContext(false, Manager()->ChromeOuterWindowID(),
                           Manager()->ShowFocusRings(),
                           Manager()->OriginAttributesRef(), aPresentationURL,
                           Manager()->GetMaxTouchPoints());

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
  browsingContext->Group()->EnsureSubscribed(constructorSender);
  browsingContext->SetOwnerProcessId(constructorSender->ChildID());

  // Construct the BrowserParent object for our subframe.
  auto browserParent = MakeRefPtr<BrowserParent>(
      constructorSender, aTabId, tabContext, browsingContext, aChromeFlags);
  browserParent->SetBrowserBridgeParent(this);

  // Open a remote endpoint for our PBrowser actor. DeallocPBrowserParent
  // releases the ref taken.
  ManagedEndpoint<PBrowserChild> childEp =
      constructorSender->OpenPBrowserEndpoint(do_AddRef(browserParent).take());
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
  bool ok = constructorSender->SendConstructBrowser(
      std::move(childEp), std::move(windowChildEp), aTabId, TabId(0),
      tabContext.AsIPCTabContext(), aWindowInit, aChromeFlags,
      constructorSender->ChildID(), constructorSender->IsForBrowser(),
      /* aIsTopLevel */ false);
  if (NS_WARN_IF(!ok)) {
    MOZ_ASSERT(false, "Browser Constructor Failed");
    return NS_ERROR_FAILURE;
  }

  // Set our BrowserParent object to the newly created browser.
  mBrowserParent = browserParent.forget();
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
  MOZ_ASSERT(mIPCOpen);
  return static_cast<BrowserParent*>(PBrowserBridgeParent::Manager());
}

void BrowserBridgeParent::Destroy() {
  if (mBrowserParent) {
    mBrowserParent->Destroy();
    mBrowserParent->SetBrowserBridgeParent(nullptr);
    mBrowserParent = nullptr;
  }
}

IPCResult BrowserBridgeParent::RecvShow(const ScreenIntSize& aSize,
                                        const bool& aParentIsActive,
                                        const nsSizeMode& aSizeMode) {
  if (!mBrowserParent->AttachLayerManager()) {
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

IPCResult BrowserBridgeParent::RecvUpdateEffects(const EffectsInfo& aEffects) {
  Unused << mBrowserParent->SendUpdateEffects(aEffects);
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
  Unused << mBrowserParent->SendSkipBrowsingContextDetach();
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

IPCResult BrowserBridgeParent::RecvSetEmbedderAccessible(
    PDocAccessibleParent* aDoc, uint64_t aID) {
#ifdef ACCESSIBILITY
  mEmbedderAccessibleDoc = static_cast<a11y::DocAccessibleParent*>(aDoc);
  mEmbedderAccessibleID = aID;
#endif
  return IPC_OK();
}

void BrowserBridgeParent::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCOpen = false;
  Destroy();
}

}  // namespace dom
}  // namespace mozilla
