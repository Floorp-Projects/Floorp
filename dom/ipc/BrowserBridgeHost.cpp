/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowserBridgeHost.h"

#include "mozilla/Unused.h"

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BrowserBridgeHost)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(BrowserBridgeHost)

NS_IMPL_CYCLE_COLLECTING_ADDREF(BrowserBridgeHost)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BrowserBridgeHost)

BrowserBridgeHost::BrowserBridgeHost(BrowserBridgeChild* aChild)
    : mBridge(aChild) {}

TabId BrowserBridgeHost::GetTabId() const { return mBridge->GetTabId(); }

mozilla::layers::LayersId BrowserBridgeHost::GetLayersId() const {
  return mBridge->GetLayersId();
}

BrowsingContext* BrowserBridgeHost::GetBrowsingContext() const {
  return mBridge->GetBrowsingContext();
}

nsILoadContext* BrowserBridgeHost::GetLoadContext() const {
  return mBridge->GetLoadContext();
}

void BrowserBridgeHost::LoadURL(nsIURI* aURI) {
  nsAutoCString spec;
  aURI->GetSpec(spec);
  Unused << mBridge->SendLoadURL(spec);
}

void BrowserBridgeHost::ResumeLoad(uint64_t aPendingSwitchId) {
  Unused << mBridge->SendResumeLoad(aPendingSwitchId);
}

void BrowserBridgeHost::DestroyStart() { DestroyComplete(); }

void BrowserBridgeHost::DestroyComplete() {
  if (!mBridge) {
    return;
  }

  Unused << mBridge->Send__delete__(mBridge);
  mBridge = nullptr;
}

bool BrowserBridgeHost::Show(const ScreenIntSize& aSize, bool aParentIsActive) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    NS_WARNING("No widget found in BrowserBridgeHost::Show");
    return false;
  }
  nsSizeMode sizeMode = widget ? widget->SizeMode() : nsSizeMode_Normal;

  Unused << mBridge->SendShow(aSize, aParentIsActive, sizeMode);
  return true;
}

void BrowserBridgeHost::UpdateDimensions(const nsIntRect& aRect,
                                         const ScreenIntSize& aSize) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  if (!widget) {
    NS_WARNING("No widget found in BrowserBridgeHost::UpdateDimensions");
    return;
  }

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

  Unused << mBridge->SendUpdateDimensions(di);
}

void BrowserBridgeHost::UpdateEffects(EffectsInfo aEffects) {
  if (!mBridge || mEffectsInfo == aEffects) {
    return;
  }
  mEffectsInfo = aEffects;
  Unused << mBridge->SendUpdateEffects(mEffectsInfo);
}

already_AddRefed<nsIWidget> BrowserBridgeHost::GetWidget() const {
  RefPtr<Element> owner = mBridge->GetFrameLoader()->GetOwnerContent();
  nsCOMPtr<nsIWidget> widget = nsContentUtils::WidgetForContent(owner);
  if (!widget) {
    widget = nsContentUtils::WidgetForDocument(owner->OwnerDoc());
  }
  return widget.forget();
}

}  // namespace dom
}  // namespace mozilla
