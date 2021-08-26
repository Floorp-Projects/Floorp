/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "mozilla/PresShell.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "nsFrameLoader.h"
#include "nsStyleStructInlines.h"
#include "nsSubDocumentFrame.h"
#include "RemoteLayerTreeOwner.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/WebRenderScrollData.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/dom/EffectsInfo.h"

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;

namespace mozilla {
namespace layout {

static already_AddRefed<WindowRenderer> GetWindowRenderer(
    BrowserParent* aBrowserParent) {
  RefPtr<WindowRenderer> renderer;
  if (Element* element = aBrowserParent->GetOwnerElement()) {
    renderer = nsContentUtils::WindowRendererForContent(element);
    if (renderer) {
      return renderer.forget();
    }
    renderer = nsContentUtils::WindowRendererForDocument(element->OwnerDoc());
    if (renderer) {
      return renderer.forget();
    }
  }
  return nullptr;
}

RemoteLayerTreeOwner::RemoteLayerTreeOwner()
    : mLayersId{0},
      mBrowserParent(nullptr),
      mInitialized(false),
      mLayersConnected(false) {}

RemoteLayerTreeOwner::~RemoteLayerTreeOwner() = default;

bool RemoteLayerTreeOwner::Initialize(BrowserParent* aBrowserParent) {
  if (mInitialized || !aBrowserParent) {
    return false;
  }

  mBrowserParent = aBrowserParent;
  RefPtr<WindowRenderer> renderer = GetWindowRenderer(mBrowserParent);
  PCompositorBridgeChild* compositor =
      renderer ? renderer->GetCompositorBridgeChild() : nullptr;
  mTabProcessId = mBrowserParent->Manager()->OtherPid();

  // Our remote frame will push layers updates to the compositor,
  // and we'll keep an indirect reference to that tree.
  GPUProcessManager* gpm = GPUProcessManager::Get();
  mLayersConnected = gpm->AllocateAndConnectLayerTreeId(
      compositor, mTabProcessId, &mLayersId, &mCompositorOptions);

  mInitialized = true;
  return true;
}

void RemoteLayerTreeOwner::Destroy() {
  if (mLayersId.IsValid()) {
    GPUProcessManager::Get()->UnmapLayerTreeId(mLayersId, mTabProcessId);
  }

  mBrowserParent = nullptr;
  mWindowRenderer = nullptr;
}

void RemoteLayerTreeOwner::EnsureLayersConnected(
    CompositorOptions* aCompositorOptions) {
  RefPtr<WindowRenderer> renderer = GetWindowRenderer(mBrowserParent);
  if (!renderer) {
    return;
  }

  if (!renderer->GetCompositorBridgeChild()) {
    return;
  }

  mLayersConnected =
      renderer->GetCompositorBridgeChild()->SendNotifyChildRecreated(
          mLayersId, &mCompositorOptions);
  *aCompositorOptions = mCompositorOptions;
}

bool RemoteLayerTreeOwner::AttachWindowRenderer() {
  RefPtr<WindowRenderer> renderer;
  if (mBrowserParent) {
    renderer = GetWindowRenderer(mBrowserParent);
  }

  // Perhaps the document containing this frame currently has no presentation?
  if (renderer && renderer->GetCompositorBridgeChild() &&
      renderer != mWindowRenderer) {
    mLayersConnected =
        renderer->GetCompositorBridgeChild()->SendAdoptChild(mLayersId);
  }

  mWindowRenderer = std::move(renderer);
  return !!mWindowRenderer;
}

void RemoteLayerTreeOwner::OwnerContentChanged() {
  Unused << AttachWindowRenderer();
}

void RemoteLayerTreeOwner::GetTextureFactoryIdentifier(
    TextureFactoryIdentifier* aTextureFactoryIdentifier) const {
  RefPtr<WindowRenderer> renderer =
      mBrowserParent ? GetWindowRenderer(mBrowserParent) : nullptr;
  // Perhaps the document containing this frame currently has no presentation?
  if (renderer && renderer->AsLayerManager()) {
    *aTextureFactoryIdentifier =
        renderer->AsLayerManager()->GetTextureFactoryIdentifier();
  } else {
    *aTextureFactoryIdentifier = TextureFactoryIdentifier();
  }
}

}  // namespace layout
}  // namespace mozilla
