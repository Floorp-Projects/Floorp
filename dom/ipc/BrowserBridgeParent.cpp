/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowserBridgeParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/BrowsingContextGroup.h"

using namespace mozilla::ipc;
using namespace mozilla::layout;
using namespace mozilla::hal;

namespace mozilla {
namespace dom {

BrowserBridgeParent::BrowserBridgeParent() : mIPCOpen(false) {}

BrowserBridgeParent::~BrowserBridgeParent() {
  if (mTabParent) {
    mTabParent->mBrowserBridgeParent = nullptr;
  }
}

nsresult BrowserBridgeParent::Init(const nsString& aPresentationURL,
                                   const nsString& aRemoteType,
                                   CanonicalBrowsingContext* aBrowsingContext) {
  mIPCOpen = true;

  // FIXME: This should actually use a non-bogus TabContext, probably inherited
  // from our Manager().
  OriginAttributes attrs;
  attrs.mInIsolatedMozBrowser = false;
  attrs.mAppId = nsIScriptSecurityManager::NO_APP_ID;
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

  // Construct the TabParent object for our subframe.
  uint32_t chromeFlags = 0;
  RefPtr<TabParent> tabParent(new TabParent(constructorSender, tabId,
                                            tabContext, aBrowsingContext,
                                            chromeFlags, this));

  PBrowserParent* browser = constructorSender->SendPBrowserConstructor(
      // DeallocPBrowserParent() releases this ref.
      tabParent.forget().take(), tabId, TabId(0), tabContext.AsIPCTabContext(),
      chromeFlags, constructorSender->ChildID(), aBrowsingContext,
      constructorSender->IsForBrowser());
  if (NS_WARN_IF(!browser)) {
    MOZ_ASSERT(false, "Browser Constructor Failed");
    return NS_ERROR_FAILURE;
  }

  // Set our TabParent object to the newly created browser.
  mTabParent = TabParent::GetFrom(browser);
  mTabParent->SetOwnerElement(Manager()->GetOwnerElement());
  mTabParent->InitRendering();

  RenderFrame* rf = mTabParent->GetRenderFrame();
  if (NS_WARN_IF(!rf)) {
    MOZ_ASSERT(false, "No RenderFrame");
    return NS_ERROR_FAILURE;
  }

  // Send the newly created layers ID back into content.
  Unused << SendSetLayersId(rf->GetLayersId());
  return NS_OK;
}

IPCResult BrowserBridgeParent::RecvShow(const ScreenIntSize& aSize,
                                        const bool& aParentIsActive,
                                        const nsSizeMode& aSizeMode) {
  RenderFrame* rf = mTabParent->GetRenderFrame();
  if (!rf->AttachLayerManager()) {
    MOZ_CRASH();
  }

  Unused << mTabParent->SendShow(aSize, mTabParent->GetShowInfo(),
                                 aParentIsActive, aSizeMode);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvLoadURL(const nsCString& aUrl) {
  Unused << mTabParent->SendLoadURL(aUrl, mTabParent->GetShowInfo());
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvUpdateDimensions(
    const DimensionInfo& aDimensions) {
  Unused << mTabParent->SendUpdateDimensions(aDimensions);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvRenderLayers(
    const bool& aEnabled, const bool& aForceRepaint,
    const layers::LayersObserverEpoch& aEpoch) {
  Unused << mTabParent->SendRenderLayers(aEnabled, aForceRepaint, aEpoch);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvNavigateByKey(
    const bool& aForward, const bool& aForDocumentNavigation) {
  Unused << mTabParent->SendNavigateByKey(aForward, aForDocumentNavigation);
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvActivate() {
  mTabParent->Activate();
  return IPC_OK();
}

IPCResult BrowserBridgeParent::RecvDeactivate() {
  mTabParent->Deactivate();
  return IPC_OK();
}

void BrowserBridgeParent::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCOpen = false;
}

}  // namespace dom
}  // namespace mozilla
