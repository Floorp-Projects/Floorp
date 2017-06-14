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
WebRenderLayer::BuildWrMaskLayer(const StackingContextHelper& aRelativeTo,
                                 const StackingContextHelper* aUnapplySc)
{
  if (GetLayer()->GetMaskLayer()) {
    WebRenderLayer* maskLayer = ToWebRenderLayer(GetLayer()->GetMaskLayer());

    // If |this| layer is pushing a stacking context, that should be passed in
    // as |aUnapplySc|. We need to unapply the transform from that stacking
    // context because the mask layer (according to WR) is outside that stacking
    // context.
    gfx::Matrix4x4 transform = maskLayer->GetLayer()->GetTransform();
    if (aUnapplySc) {
      transform = transform * aUnapplySc->TransformToRoot();
    }

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

Maybe<LayerRect>
WebRenderLayer::ClipRect()
{
  Layer* layer = GetLayer();
  if (!layer->GetClipRect()) {
    return Nothing();
  }
  ParentLayerRect clip(layer->GetClipRect().ref());
  LayerToParentLayerMatrix4x4 transform = layer->GetLocalTransformTyped();
  return Some(transform.Inverse().TransformBounds(clip));
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

  Matrix4x4 transform = GetLayer()->GetTransform();
  LayerRect bounds = Bounds();
  WrMixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetLayer()->GetMixBlendMode());

  printf_stderr("%s %p using bounds=%s, transform=%s, rect=%s, clip=%s, mix-blend-mode=%s\n",
                aLayerType,
                GetLayer(),
                Stringify(bounds).c_str(),
                Stringify(transform).c_str(),
                Stringify(aRect).c_str(),
                ClipRect() ? Stringify(ClipRect().value()).c_str() : "none",
                Stringify(mixBlendMode).c_str());
}

} // namespace layers
} // namespace mozilla
