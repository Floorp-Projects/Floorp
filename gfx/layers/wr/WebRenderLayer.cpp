/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderLayer.h"

#include "gfxPrefs.h"
#include "LayersLogging.h"
#include "mozilla/layers/ImageClient.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "nsThreadUtils.h"
#include "UnitTransforms.h"

namespace mozilla {

using namespace gfx;

namespace layers {

WebRenderLayerManager*
WebRenderLayer::WrManager()
{
  return static_cast<WebRenderLayerManager*>(GetLayer()->Manager());
}

WebRenderBridgeChild*
WebRenderLayer::WrBridge()
{
  return WrManager()->WrBridge();
}

WrImageKey
WebRenderLayer::GetImageKey()
{
  WrImageKey key;
  key.mNamespace = WrBridge()->GetNamespace();
  key.mHandle = WrBridge()->GetNextResourceId();
  return key;
}

Maybe<WrImageMask>
WebRenderLayer::BuildWrMaskLayer(const StackingContextHelper& aRelativeTo)
{
  if (GetLayer()->GetMaskLayer()) {
    WebRenderLayer* maskLayer = ToWebRenderLayer(GetLayer()->GetMaskLayer());
    gfx::Matrix4x4 transform = maskLayer->GetLayer()->GetTransform();
    return maskLayer->RenderMaskLayer(aRelativeTo, transform);
  }

  return Nothing();
}

LayerRect
WebRenderLayer::Bounds()
{
  return LayerRect(GetLayer()->GetVisibleRegion().GetBounds());
}

BoundsTransformMatrix
WebRenderLayer::BoundsTransform()
{
  gfx::Matrix4x4 transform = GetLayer()->GetTransform();
  transform._41 = 0.0f;
  transform._42 = 0.0f;
  transform._43 = 0.0f;
  return ViewAs<BoundsTransformMatrix>(transform);
}

LayerRect
WebRenderLayer::BoundsForStackingContext()
{
  LayerRect bounds = Bounds();
  BoundsTransformMatrix transform = BoundsTransform();
  if (!transform.IsIdentity()) {
    // WR will only apply the 'translate' of the transform, so we need to do the scale/rotation manually.
    bounds.MoveTo(transform.TransformPoint(bounds.TopLeft()));
  }

  return bounds;
}

Maybe<wr::ImageKey>
WebRenderLayer::UpdateImageKey(ImageClientSingle* aImageClient,
                               ImageContainer* aContainer,
                               Maybe<wr::ImageKey>& aOldKey,
                               wr::ExternalImageId& aExternalImageId)
{
  MOZ_ASSERT(aImageClient);
  MOZ_ASSERT(aContainer);

  uint32_t oldCounter = aImageClient->GetLastUpdateGenerationCounter();

  bool ret = aImageClient->UpdateImage(aContainer, /* unused */0);
  if (!ret || aImageClient->IsEmpty()) {
    // Delete old key
    if (aOldKey.isSome()) {
      WrManager()->AddImageKeyForDiscard(aOldKey.value());
    }
    return Nothing();
  }

  // Reuse old key if generation is not updated.
  if (oldCounter == aImageClient->GetLastUpdateGenerationCounter() && aOldKey.isSome()) {
    return aOldKey;
  }

  // Delete old key, we are generating a new key.
  if (aOldKey.isSome()) {
    WrManager()->AddImageKeyForDiscard(aOldKey.value());
  }

  WrImageKey key = GetImageKey();
  WrBridge()->AddWebRenderParentCommand(OpAddExternalImage(aExternalImageId, key));
  return Some(key);
}

void
WebRenderLayer::DumpLayerInfo(const char* aLayerType, const LayerRect& aRect)
{
  if (!gfxPrefs::LayersDump()) {
    return;
  }

  Layer* layer = GetLayer();
  Matrix4x4 transform = layer->GetTransform();
  LayerRect bounds = Bounds();
  WrMixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetLayer()->GetMixBlendMode());

  printf_stderr("%s %p using bounds=%s, transform=%s, rect=%s, clip=%s, mix-blend-mode=%s\n",
                aLayerType,
                layer,
                Stringify(bounds).c_str(),
                Stringify(transform).c_str(),
                Stringify(aRect).c_str(),
                layer->GetClipRect() ? Stringify(layer->GetClipRect().value()).c_str() : "none",
                Stringify(mixBlendMode).c_str());
}

} // namespace layers
} // namespace mozilla
