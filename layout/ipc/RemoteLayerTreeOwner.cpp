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

static already_AddRefed<LayerManager> GetLayerManager(
    BrowserParent* aBrowserParent) {
  if (Element* element = aBrowserParent->GetOwnerElement()) {
    if (RefPtr<LayerManager> lm =
            nsContentUtils::LayerManagerForContent(element)) {
      return lm.forget();
    }
    return nsContentUtils::LayerManagerForDocument(element->OwnerDoc());
  }
  return nullptr;
}

RemoteLayerTreeOwner::RemoteLayerTreeOwner()
    : mLayersId{0},
      mBrowserParent(nullptr),
      mLayerManager(nullptr),
      mInitialized(false),
      mLayersConnected(false) {}

RemoteLayerTreeOwner::~RemoteLayerTreeOwner() {}

bool RemoteLayerTreeOwner::Initialize(BrowserParent* aBrowserParent) {
  if (mInitialized || !aBrowserParent) {
    return false;
  }

  mBrowserParent = aBrowserParent;
  RefPtr<LayerManager> lm = GetLayerManager(mBrowserParent);
  PCompositorBridgeChild* compositor =
      lm ? lm->GetCompositorBridgeChild() : nullptr;
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
  mLayerManager = nullptr;
}

void RemoteLayerTreeOwner::EnsureLayersConnected(
    CompositorOptions* aCompositorOptions) {
  RefPtr<LayerManager> lm = GetLayerManager(mBrowserParent);
  if (!lm) {
    return;
  }

  if (!lm->GetCompositorBridgeChild()) {
    return;
  }

  mLayersConnected = lm->GetCompositorBridgeChild()->SendNotifyChildRecreated(
      mLayersId, &mCompositorOptions);
  *aCompositorOptions = mCompositorOptions;
}

LayerManager* RemoteLayerTreeOwner::AttachLayerManager() {
  RefPtr<LayerManager> lm;
  if (mBrowserParent) {
    lm = GetLayerManager(mBrowserParent);
  }

  // Perhaps the document containing this frame currently has no presentation?
  if (lm && lm->GetCompositorBridgeChild() && lm != mLayerManager) {
    mLayersConnected =
        lm->GetCompositorBridgeChild()->SendAdoptChild(mLayersId);
    FrameLayerBuilder::InvalidateAllLayers(lm);
  }

  mLayerManager = lm.forget();
  return mLayerManager;
}

void RemoteLayerTreeOwner::OwnerContentChanged() {
  Unused << AttachLayerManager();
}

void RemoteLayerTreeOwner::GetTextureFactoryIdentifier(
    TextureFactoryIdentifier* aTextureFactoryIdentifier) const {
  RefPtr<LayerManager> lm =
      mBrowserParent ? GetLayerManager(mBrowserParent) : nullptr;
  // Perhaps the document containing this frame currently has no presentation?
  if (lm) {
    *aTextureFactoryIdentifier = lm->GetTextureFactoryIdentifier();
  } else {
    *aTextureFactoryIdentifier = TextureFactoryIdentifier();
  }
}

}  // namespace layout
}  // namespace mozilla
