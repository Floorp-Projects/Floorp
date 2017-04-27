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

Rect
WebRenderLayer::RelativeToVisible(Rect aRect)
{
  IntRect bounds = GetLayer()->GetVisibleRegion().GetBounds().ToUnknownRect();
  aRect.MoveBy(-bounds.x, -bounds.y);
  return aRect;
}

Rect
WebRenderLayer::RelativeToTransformedVisible(Rect aRect)
{
  IntRect bounds = GetLayer()->GetVisibleRegion().GetBounds().ToUnknownRect();
  Rect transformed = GetLayer()->GetTransform().TransformBounds(IntRectToRect(bounds));
  aRect.MoveBy(-transformed.x, -transformed.y);
  return aRect;
}

Rect
WebRenderLayer::ParentStackingContextBounds()
{
  // Walk up to find the parent stacking context. This will be created either
  // by the nearest scrollable metrics, or by the parent layer which must be a
  // ContainerLayer.
  Layer* layer = GetLayer();
  if (layer->GetParent()) {
    return IntRectToRect(layer->GetParent()->GetVisibleRegion().GetBounds().ToUnknownRect());
  }
  return Rect();
}

Rect
WebRenderLayer::RelativeToParent(Rect aRect)
{
  Rect parentBounds = ParentStackingContextBounds();
  aRect.MoveBy(-parentBounds.x, -parentBounds.y);
  return aRect;
}

Point
WebRenderLayer::GetOffsetToParent()
{
  Rect parentBounds = ParentStackingContextBounds();
  return parentBounds.TopLeft();
}

Rect
WebRenderLayer::VisibleBoundsRelativeToParent()
{
  return RelativeToParent(IntRectToRect(GetLayer()->GetVisibleRegion().GetBounds().ToUnknownRect()));
}

Rect
WebRenderLayer::TransformedVisibleBoundsRelativeToParent()
{
  IntRect bounds = GetLayer()->GetVisibleRegion().GetBounds().ToUnknownRect();
  Rect transformed = GetLayer()->GetTransform().TransformBounds(IntRectToRect(bounds));
  return RelativeToParent(transformed);
}

Maybe<WrImageMask>
WebRenderLayer::BuildWrMaskLayer(bool aUnapplyLayerTransform)
{
  if (GetLayer()->GetMaskLayer()) {
    WebRenderLayer* maskLayer = ToWebRenderLayer(GetLayer()->GetMaskLayer());

    // The size of mask layer is transformed, and we may set the layer transform
    // to wr stacking context. So we should apply inverse transform for mask layer
    // and reverse the offset of the stacking context.
    gfx::Matrix4x4 transform = maskLayer->GetLayer()->GetTransform();
    if (aUnapplyLayerTransform) {
      gfx::Rect bounds = IntRectToRect(GetLayer()->GetVisibleRegion().GetBounds().ToUnknownRect());
      transform = transform.PreTranslate(-bounds.x, -bounds.y, 0);
      transform = transform * GetLayer()->GetTransform().Inverse();
    }

    return maskLayer->RenderMaskLayer(transform);
  }

  return Nothing();
}

gfx::Rect
WebRenderLayer::GetWrBoundsRect()
{
  LayerIntRect bounds = GetLayer()->GetVisibleRegion().GetBounds();
  return Rect(0, 0, bounds.width, bounds.height);
}

gfx::Rect
WebRenderLayer::GetWrClipRect(gfx::Rect& aRect)
{
  Maybe<LayerRect> clip = ClipRect();
  if (clip) {
    return RelativeToVisible(clip.ref().ToUnknownRect());
  }
  return aRect;
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

gfx::Rect
WebRenderLayer::GetWrRelBounds()
{
  return RelativeToParent(BoundsForStackingContext().ToUnknownRect());
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
WebRenderLayer::DumpLayerInfo(const char* aLayerType, gfx::Rect& aRect)
{
  if (!gfxPrefs::LayersDump()) {
    return;
  }

  Matrix4x4 transform = GetLayer()->GetTransform();
  Rect clip = GetWrClipRect(aRect);
  Rect relBounds = GetWrRelBounds();
  Rect overflow(0, 0, relBounds.width, relBounds.height);
  WrMixBlendMode mixBlendMode = wr::ToWrMixBlendMode(GetLayer()->GetMixBlendMode());

  printf_stderr("%s %p using bounds=%s, overflow=%s, transform=%s, rect=%s, clip=%s, mix-blend-mode=%s\n",
                aLayerType,
                GetLayer(),
                Stringify(relBounds).c_str(),
                Stringify(overflow).c_str(),
                Stringify(transform).c_str(),
                Stringify(aRect).c_str(),
                Stringify(clip).c_str(),
                Stringify(mixBlendMode).c_str());
}

} // namespace layers
} // namespace mozilla
