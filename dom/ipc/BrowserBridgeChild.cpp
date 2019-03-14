/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowserBridgeChild.h"
#include "nsFocusManager.h"
#include "nsFrameLoader.h"
#include "nsFrameLoaderOwner.h"
#include "nsQueryObject.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

BrowserBridgeChild::BrowserBridgeChild(nsFrameLoader* aFrameLoader)
    : mLayersId{0}, mIPCOpen(true), mFrameLoader(aFrameLoader) {}

BrowserBridgeChild::~BrowserBridgeChild() {}

already_AddRefed<BrowserBridgeChild> BrowserBridgeChild::Create(
    nsFrameLoader* aFrameLoader, const TabContext& aContext,
    const nsString& aRemoteType) {
  MOZ_ASSERT(XRE_IsContentProcess());

  // Determine our embedder's TabChild actor.
  RefPtr<Element> owner = aFrameLoader->GetOwnerContent();
  MOZ_DIAGNOSTIC_ASSERT(owner);

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(owner->GetOwnerGlobal());
  MOZ_DIAGNOSTIC_ASSERT(docShell);

  RefPtr<TabChild> tabChild = TabChild::GetFrom(docShell);
  MOZ_DIAGNOSTIC_ASSERT(tabChild);

  RefPtr<BrowserBridgeChild> browserBridge =
      new BrowserBridgeChild(aFrameLoader);
  // Reference is freed in TabChild::DeallocPBrowserBridgeChild.
  tabChild->SendPBrowserBridgeConstructor(
      do_AddRef(browserBridge).take(),
      PromiseFlatString(aContext.PresentationURL()), aRemoteType);
  browserBridge->mIPCOpen = true;

  return browserBridge.forget();
}

void BrowserBridgeChild::UpdateDimensions(const nsIntRect& aRect,
                                          const mozilla::ScreenIntSize& aSize) {
  MOZ_DIAGNOSTIC_ASSERT(mIPCOpen);

  RefPtr<Element> owner = mFrameLoader->GetOwnerContent();
  nsCOMPtr<nsIWidget> widget = nsContentUtils::WidgetForContent(owner);
  if (!widget) {
    widget = nsContentUtils::WidgetForDocument(owner->OwnerDoc());
  }
  MOZ_DIAGNOSTIC_ASSERT(widget);

  CSSToLayoutDeviceScale widgetScale = widget->GetDefaultScale();

  LayoutDeviceIntRect devicePixelRect = ViewAs<LayoutDevicePixel>(
      aRect, PixelCastJustification::LayoutDeviceIsScreenForTabDims);
  LayoutDeviceIntSize devicePixelSize = ViewAs<LayoutDevicePixel>(
      aSize, PixelCastJustification::LayoutDeviceIsScreenForTabDims);

  // XXX What are clientOffset and chromeOffset used for? Are they meaningful
  // for nested iframes with transforms?
  LayoutDeviceIntPoint clientOffset;
  LayoutDeviceIntPoint chromeOffset;

  CSSRect unscaledRect = devicePixelRect / widgetScale;
  CSSSize unscaledSize = devicePixelSize / widgetScale;
  hal::ScreenOrientation orientation = hal::eScreenOrientation_Default;
  DimensionInfo di(unscaledRect, unscaledSize, orientation, clientOffset,
                   chromeOffset);

  Unused << SendUpdateDimensions(di);
}

void BrowserBridgeChild::NavigateByKey(bool aForward,
                                       bool aForDocumentNavigation) {
  Unused << SendNavigateByKey(aForward, aForDocumentNavigation);
}

void BrowserBridgeChild::Activate() { Unused << SendActivate(); }

void BrowserBridgeChild::Deactivate() { Unused << SendDeactivate(); }

/*static*/
BrowserBridgeChild* BrowserBridgeChild::GetFrom(nsFrameLoader* aFrameLoader) {
  if (!aFrameLoader) {
    return nullptr;
  }
  return aFrameLoader->GetBrowserBridgeChild();
}

/*static*/
BrowserBridgeChild* BrowserBridgeChild::GetFrom(nsIContent* aContent) {
  RefPtr<nsFrameLoaderOwner> loaderOwner = do_QueryObject(aContent);
  if (!loaderOwner) {
    return nullptr;
  }
  RefPtr<nsFrameLoader> frameLoader = loaderOwner->GetFrameLoader();
  return GetFrom(frameLoader);
}

IPCResult BrowserBridgeChild::RecvSetLayersId(
    const mozilla::layers::LayersId& aLayersId) {
  MOZ_ASSERT(!mLayersId.IsValid() && aLayersId.IsValid());
  mLayersId = aLayersId;
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserBridgeChild::RecvRequestFocus(
    const bool& aCanRaise) {
  // Adapted from TabParent
  nsCOMPtr<nsIFocusManager> fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return IPC_OK();
  }

  RefPtr<Element> owner = mFrameLoader->GetOwnerContent();

  if (!owner || !owner->OwnerDoc()) {
    return IPC_OK();
  }

  uint32_t flags = nsIFocusManager::FLAG_NOSCROLL;
  if (aCanRaise) {
    flags |= nsIFocusManager::FLAG_RAISE;
  }

  fm->SetFocus(owner, flags);
  return IPC_OK();
}

void BrowserBridgeChild::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCOpen = false;
}

}  // namespace dom
}  // namespace mozilla
