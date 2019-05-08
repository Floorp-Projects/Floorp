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
  return mRoot->GetRenderFrame()->GetLayersId();
}

BrowsingContext* BrowserHost::GetBrowsingContext() const {
  return mRoot->GetBrowsingContext();
}

nsILoadContext* BrowserHost::GetLoadContext() const {
  RefPtr<nsILoadContext> loadContext = mRoot->GetLoadContext();
  return loadContext;
}

void BrowserHost::LoadURL(nsIURI* aURI) { mRoot->LoadURL(aURI); }

void BrowserHost::ResumeLoad(uint64_t aPendingSwitchId) {
  mRoot->ResumeLoad(aPendingSwitchId);
}

void BrowserHost::DestroyStart() { mRoot->Destroy(); }

void BrowserHost::DestroyComplete() {
  if (!mRoot) {
    return;
  }
  mRoot->SetOwnerElement(nullptr);
  mRoot->Destroy();
  mRoot = nullptr;
}

bool BrowserHost::Show(const ScreenIntSize& aSize, bool aParentIsActive) {
  return mRoot->Show(aSize, aParentIsActive);
}

void BrowserHost::UpdateDimensions(const nsIntRect& aRect,
                                   const ScreenIntSize& aSize) {
  mRoot->UpdateDimensions(aRect, aSize);
}

}  // namespace dom
}  // namespace mozilla
