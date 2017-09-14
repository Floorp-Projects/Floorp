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
namespace wr {
class IpcResourceUpdateQueue;
}
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
                           wr::IpcResourceUpdateQueue& aResources,
                           const StackingContextHelper& aSc) = 0;
  virtual Maybe<wr::WrImageMask> RenderMaskLayer(const StackingContextHelper& aSc,
                                                 const gfx::Matrix4x4& aTransform,
                                                 wr::IpcResourceUpdateQueue& aResources)
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

  WebRenderLayerManager* WrManager();
  WebRenderBridgeChild* WrBridge();
  wr::WrImageKey GenerateImageKey();

  LayerRect Bounds();
  LayerRect BoundsForStackingContext();

  // Builds a WrImageMask from the mask layer on this layer, if there is one.
  // The |aRelativeTo| parameter should be a reference to the stacking context
  // that we want this mask to be relative to. This is usually the stacking
  // context of the *parent* layer of |this|, because that is what the mask
  // is relative to in the layer tree.
  Maybe<wr::WrImageMask> BuildWrMaskLayer(const StackingContextHelper& aRelativeTo,
                                          wr::IpcResourceUpdateQueue& aResources);

protected:
  BoundsTransformMatrix BoundsTransform();

  void DumpLayerInfo(const char* aLayerType, const LayerRect& aRect);
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_WEBRENDERLAYER_H */
