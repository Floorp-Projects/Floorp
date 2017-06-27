/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_LayerMLGPU_h
#define mozilla_gfx_layers_mlgpu_LayerMLGPU_h

#include "Layers.h"
#include "mozilla/layers/LayerManagerComposite.h"

namespace mozilla {
namespace layers {

class CanvasLayerMLGPU;
class ColorLayerMLGPU;
class ContainerLayerMLGPU;
class FrameBuilder;
class ImageHost;
class ImageLayerMLGPU;
class LayerManagerMLGPU;
class MaskOperation;
class MLGRenderTarget;
class PaintedLayerMLGPU;
class RefLayerMLGPU;
class RenderViewMLGPU;
class TexturedLayerMLGPU;
class TextureSource;

class LayerMLGPU : public HostLayer
{
public:
  LayerMLGPU* AsLayerMLGPU() override { return this; }
  virtual PaintedLayerMLGPU* AsPaintedLayerMLGPU() { return nullptr; }
  virtual ImageLayerMLGPU* AsImageLayerMLGPU() { return nullptr; }
  virtual CanvasLayerMLGPU* AsCanvasLayerMLGPU() { return nullptr; }
  virtual ContainerLayerMLGPU* AsContainerLayerMLGPU() { return nullptr; }
  virtual RefLayerMLGPU* AsRefLayerMLGPU() { return nullptr; }
  virtual ColorLayerMLGPU* AsColorLayerMLGPU() { return nullptr; }
  virtual TexturedLayerMLGPU* AsTexturedLayerMLGPU() { return nullptr; }

  static void BeginFrame();

  // Ask the layer to acquire any resources or per-frame information needed
  // to render. If this returns false, the layer will be skipped entirely.
  bool PrepareToRender(FrameBuilder* aBuilder, const RenderTargetIntRect& aClipRect);

  Layer::LayerType GetType() {
    return GetLayer()->GetType();
  }
  const RenderTargetIntRect& GetComputedClipRect() const {
    return mComputedClipRect;
  }
  MaskOperation* GetMask() const {
    return mMask;
  }
  float GetComputedOpacity() const {
    return mComputedOpacity;
  }

  // Return the bounding box of this layer in render target space, clipped to
  // the computed clip rect, and rounded out to an integer rect.
  gfx::IntRect GetClippedBoundingBox(RenderViewMLGPU* aView,
                                     const Maybe<gfx::Polygon>& aGeometry);

  // If this layer has already been prepared for the current frame, return
  // true. This should only be used to guard against double-processing
  // container layers after 3d-sorting.
  bool IsPrepared() const {
    return mFrameKey == sFrameKey && mPrepared;
  }

  // Return true if the content in this layer is opaque (not factoring in
  // blend modes or opacity), false otherwise.
  virtual bool IsContentOpaque();

  // This is a wrapper around SetShadowVisibleRegion. Some layers have visible
  // regions that extend beyond what is actually drawn. When performing CPU-
  // based occlusion culling we must clamp the visible region to the actual
  // area.
  virtual void SetRegionToRender(LayerIntRegion&& aRegion);

  virtual void AssignToView(FrameBuilder* aBuilder,
                            RenderViewMLGPU* aView,
                            Maybe<gfx::Polygon>&& aGeometry);

  // Callback for when PrepareToRender has finished successfully. If this
  // returns false, PrepareToRender will return false.
  virtual bool OnPrepareToRender(FrameBuilder* aBuilder) {
    return true;
  }

  virtual void ClearCachedResources() {}
  virtual CompositableHost* GetCompositableHost() override {
    return nullptr;
  }

protected:
  LayerMLGPU(LayerManagerMLGPU* aManager);
  LayerManagerMLGPU* GetManager();

  void AddBoundsToView(FrameBuilder* aBuilder,
                       RenderViewMLGPU* aView,
                       Maybe<gfx::Polygon>&& aGeometry);

  void MarkPrepared();

  // We don't want derivative layers overriding this directly - we provide a
  // callback instead.
  void SetLayerManager(HostLayerManager* aManager) override;
  virtual void OnLayerManagerChange(LayerManagerMLGPU* aManager) {}

private:
  // This is a monotonic counter used to check whether a layer appears twice
  // when 3d sorting.
  static uint64_t sFrameKey;

protected:
  // These are set during PrepareToRender.
  RenderTargetIntRect mComputedClipRect;
  RefPtr<MaskOperation> mMask;
  uint64_t mFrameKey;
  float mComputedOpacity;
  bool mPrepared;
};

class RefLayerMLGPU final : public RefLayer
                          , public LayerMLGPU
{
public:
  explicit RefLayerMLGPU(LayerManagerMLGPU* aManager);
  ~RefLayerMLGPU() override;

  // Layer
  HostLayer* AsHostLayer() override { return this; }
  RefLayerMLGPU* AsRefLayerMLGPU() override { return this; }
  Layer* GetLayer() override { return this; }

  // ContainerLayer
  void ComputeEffectiveTransforms(const gfx::Matrix4x4& aTransformToSurface) override
  {
    DefaultComputeEffectiveTransforms(aTransformToSurface);
  }

  MOZ_LAYER_DECL_NAME("RefLayerMLGPU", TYPE_REF)
};

class ColorLayerMLGPU final : public ColorLayer
                            , public LayerMLGPU
{
public:
  explicit ColorLayerMLGPU(LayerManagerMLGPU* aManager);
  ~ColorLayerMLGPU() override;

  // LayerMLGPU
  bool IsContentOpaque() override {
    return mColor.a >= 1.0f;
  }

  // Layer
  HostLayer* AsHostLayer() override { return this; }
  ColorLayerMLGPU* AsColorLayerMLGPU() override { return this; }
  Layer* GetLayer() override { return this; }

  MOZ_LAYER_DECL_NAME("ColorLayerMLGPU", TYPE_COLOR)
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_mlgpu_LayerMLGPU_h
