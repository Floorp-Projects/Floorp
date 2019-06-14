/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowserBridgeChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "nsFocusManager.h"
#include "nsFrameLoader.h"
#include "nsFrameLoaderOwner.h"
#include "nsQueryObject.h"
#include "nsIDocShellTreeOwner.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

BrowserBridgeChild::BrowserBridgeChild(nsFrameLoader* aFrameLoader,
                                       BrowsingContext* aBrowsingContext)
    : mLayersId{0},
      mIPCOpen(true),
      mFrameLoader(aFrameLoader),
      mBrowsingContext(aBrowsingContext) {}

BrowserBridgeChild::~BrowserBridgeChild() {}

already_AddRefed<BrowserBridgeChild> BrowserBridgeChild::Create(
    nsFrameLoader* aFrameLoader, const TabContext& aContext,
    const nsString& aRemoteType, BrowsingContext* aBrowsingContext) {
  MOZ_ASSERT(XRE_IsContentProcess());

  // Determine our embedder's BrowserChild actor.
  RefPtr<Element> owner = aFrameLoader->GetOwnerContent();
  MOZ_DIAGNOSTIC_ASSERT(owner);

  nsCOMPtr<nsIDocShell> docShell = do_GetInterface(owner->GetOwnerGlobal());
  MOZ_DIAGNOSTIC_ASSERT(docShell);

  RefPtr<BrowserChild> browserChild = BrowserChild::GetFrom(docShell);
  MOZ_DIAGNOSTIC_ASSERT(browserChild);

  uint32_t chromeFlags = 0;

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  if (docShell) {
    docShell->GetTreeOwner(getter_AddRefs(treeOwner));
  }
  if (treeOwner) {
    nsCOMPtr<nsIWebBrowserChrome> wbc = do_GetInterface(treeOwner);
    if (wbc) {
      wbc->GetChromeFlags(&chromeFlags);
    }
  }

  // Checking that this actually does something useful is
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1542710
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(docShell);
  if (loadContext && loadContext->UsePrivateBrowsing()) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;
  }
  if (docShell->GetAffectPrivateSessionLifetime()) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_PRIVATE_LIFETIME;
  }

  RefPtr<BrowserBridgeChild> browserBridge =
      new BrowserBridgeChild(aFrameLoader, aBrowsingContext);
  // Reference is freed in BrowserChild::DeallocPBrowserBridgeChild.
  browserChild->SendPBrowserBridgeConstructor(
      do_AddRef(browserBridge).take(),
      PromiseFlatString(aContext.PresentationURL()), aRemoteType,
      aBrowsingContext, chromeFlags);
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

void BrowserBridgeChild::Deactivate(bool aWindowLowering) {
  Unused << SendDeactivate(aWindowLowering);
}

void BrowserBridgeChild::SetIsUnderHiddenEmbedderElement(
    bool aIsUnderHiddenEmbedderElement) {
  Unused << SendSetIsUnderHiddenEmbedderElement(aIsUnderHiddenEmbedderElement);
}

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

  // Invalidate the nsSubdocumentFrame now that we have a layers ID for the
  // child browser
  if (RefPtr<Element> owner = mFrameLoader->GetOwnerContent()) {
    if (nsIFrame* frame = owner->GetPrimaryFrame()) {
      frame->InvalidateFrame();
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserBridgeChild::RecvRequestFocus(
    const bool& aCanRaise) {
  // Adapted from BrowserParent
  RefPtr<Element> owner = mFrameLoader->GetOwnerContent();
  if (!owner) {
    return IPC_OK();
  }
  nsContentUtils::RequestFrameFocus(*owner, aCanRaise);
  return IPC_OK();
}

mozilla::ipc::IPCResult BrowserBridgeChild::RecvMoveFocus(
    const bool& aForward, const bool& aForDocumentNavigation) {
  // Adapted from BrowserParent
  nsCOMPtr<nsIFocusManager> fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return IPC_OK();
  }

  RefPtr<Element> owner = mFrameLoader->GetOwnerContent();
  if (!owner) {
    return IPC_OK();
  }

  RefPtr<Element> dummy;

  uint32_t type =
      aForward
          ? (aForDocumentNavigation
                 ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FORWARDDOC)
                 : static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_FORWARD))
          : (aForDocumentNavigation
                 ? static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_BACKWARDDOC)
                 : static_cast<uint32_t>(nsIFocusManager::MOVEFOCUS_BACKWARD));
  fm->MoveFocus(nullptr, owner, type, nsIFocusManager::FLAG_BYKEY,
                getter_AddRefs(dummy));
  return IPC_OK();
}

void BrowserBridgeChild::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCOpen = false;
}

}  // namespace dom
}  // namespace mozilla
