/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowserBridgeHost.h"

#include "mozilla/Unused.h"
#include "mozilla/dom/Element.h"
#include "nsFrameLoader.h"

namespace mozilla::dom {

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

void BrowserBridgeHost::LoadURL(nsDocShellLoadState* aLoadState) {
  MOZ_ASSERT(aLoadState);
  Unused << mBridge->SendLoadURL(aLoadState);
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

bool BrowserBridgeHost::Show(const OwnerShowInfo& aShowInfo) {
  Unused << mBridge->SendShow(aShowInfo);
  return true;
}

void BrowserBridgeHost::UpdateDimensions(const nsIntRect& aRect,
                                         const ScreenIntSize& aSize) {
  Unused << mBridge->SendUpdateDimensions(aRect, aSize);
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

}  // namespace mozilla::dom
