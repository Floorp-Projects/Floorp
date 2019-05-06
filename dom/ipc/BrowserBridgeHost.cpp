/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowserBridgeHost.h"

namespace mozilla {
namespace dom {

BrowserBridgeHost::BrowserBridgeHost(BrowserBridgeChild* aChild)
    : mBridge(aChild) {}

mozilla::layers::LayersId BrowserBridgeHost::GetLayersId() const {
  return LayersId{0};
}

BrowsingContext* BrowserBridgeHost::GetBrowsingContext() const {
  return nullptr;
}

nsILoadContext* BrowserBridgeHost::GetLoadContext() const { return nullptr; }

void BrowserBridgeHost::LoadURL(nsIURI* aURI) {}

void BrowserBridgeHost::ResumeLoad(uint64_t aPendingSwitchId) {}

void BrowserBridgeHost::DestroyStart() {}

void BrowserBridgeHost::DestroyComplete() {}

bool BrowserBridgeHost::Show(const ScreenIntSize& aSize, bool aParentIsActive) {
  return true;
}

void BrowserBridgeHost::UpdateDimensions(const nsIntRect& aRect,
                                         const ScreenIntSize& aSize) {}

}  // namespace dom
}  // namespace mozilla
