/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERLAYER_H
#define GFX_WEBRENDERLAYER_H

#include "Layers.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/webrender/WebRenderAPI.h"

namespace mozilla {
namespace layers {

class ImageClientSingle;
class StackingContextHelper;
class WebRenderBridgeChild;
class WebRenderLayerManager;

typedef gfx::Matrix4x4Typed<LayerPixel, LayerPixel> BoundsTransformMatrix;

class WebRenderLayer
{
public:
  virtual Layer* GetLayer() = 0;
  virtual void RenderLayer(wr::DisplayListBuilder& aBuilder,
                           const StackingContextHelper& aSc) = 0;
  virtual Maybe<WrImageMask> RenderMaskLayer(const StackingContextHelper& aSc,
                                             const gfx::Matrix4x4& aTransform)
  {
    MOZ_ASSERT(false);
    return Nothing();
  }

  virtual already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() { return nullptr; }
  static inline WebRenderLayer*
  ToWebRenderLayer(Layer* aLayer)
  {
    return static_cast<WebRenderLayer*>(aLayer->ImplData());
  }

  Maybe<wr::ImageKey> UpdateImageKey(ImageClientSingle* aImageClient,
                                     ImageContainer* aContainer,
                                     Maybe<wr::ImageKey>& aOldKey,
                                     wr::ExternalImageId& aExternalImageId);

  WebRenderLayerManager* WrManager();
  WebRenderBridgeChild* WrBridge();
  WrImageKey GetImageKey();

  LayerRect Bounds();
  LayerRect BoundsForStackingContext();

  // Builds a WrImageMask from the mask layer on this layer, if there is one.
  // The |aRelativeTo| parameter should be a reference to the stacking context
  // that we want this mask to be relative to. This is usually the stacking
  // context of the *parent* layer of |this|, because that is what the mask
  // is relative to in the layer tree.
  // If |this| layer has already pushed a stacking context, and will be pushing
  // the mask as part of a clip "inside" the stacking context, then WR will
  // interpret the WrImageMask as relative to that stacking context. However,
  // we usually don't want that, again because the layer tree semantics are that
  // the mask is relative to the parent. Therefore callers that are doing this
  // can pass in a pointer to the stacking context pushed by |this| layer in
  // |aUnapplySc|, and this function will unapply the transforms from that
  // stacking context to make sure the WrImageMask is correct.
  Maybe<WrImageMask> BuildWrMaskLayer(const StackingContextHelper& aRelativeTo,
                                      const StackingContextHelper* aUnapplySc);

protected:
  BoundsTransformMatrix BoundsTransform();
  Maybe<LayerRect> ClipRect();

  void DumpLayerInfo(const char* aLayerType, const LayerRect& aRect);
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_WEBRENDERLAYER_H */
