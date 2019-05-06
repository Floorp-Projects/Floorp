/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BrowserHost.h"

#include "mozilla/dom/CancelContentJSOptionsBinding.h"
#include "mozilla/dom/WindowGlobalParent.h"

namespace mozilla {
namespace dom {

BrowserHost::BrowserHost(BrowserParent* aParent) : mRoot(aParent) {}

mozilla::layers::LayersId BrowserHost::GetLayersId() const {
  return LayersId{0};
}

BrowsingContext* BrowserHost::GetBrowsingContext() const { return nullptr; }

nsILoadContext* BrowserHost::GetLoadContext() const { return nullptr; }

void BrowserHost::LoadURL(nsIURI* aURI) {}

void BrowserHost::ResumeLoad(uint64_t aPendingSwitchId) {}

void BrowserHost::DestroyStart() {}

void BrowserHost::DestroyComplete() {}

bool BrowserHost::Show(const ScreenIntSize& aSize, bool aParentIsActive) {
  return true;
}

void BrowserHost::UpdateDimensions(const nsIntRect& aRect,
                                   const ScreenIntSize& aSize) {}

}  // namespace dom
}  // namespace mozilla
