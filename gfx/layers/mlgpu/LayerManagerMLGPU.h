/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_LAYERMANAGERMLGPU_H
#define MOZILLA_GFX_LAYERMANAGERMLGPU_H

#include "Layers.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "LayerMLGPU.h"

namespace mozilla {
namespace layers {

class FrameBuilder;
class LayerManagerMLGPU;
class RenderPassMLGPU;
class SharedBufferMLGPU;
class RenderViewMLGPU;
class TextRenderer;
class TextureSourceProviderMLGPU;
class MLGBuffer;
class MLGDevice;
class MLGSwapChain;
class MLGTileBuffer;
struct LayerProperties;

class LayerManagerMLGPU final : public HostLayerManager
{
public:
  explicit LayerManagerMLGPU(widget::CompositorWidget* aWidget);
  ~LayerManagerMLGPU();

  bool Initialize();
  void Destroy() override;

  // LayerManager methods
  bool BeginTransaction() override;
  void BeginTransactionWithDrawTarget(gfx::DrawTarget* aTarget, const gfx::IntRect& aRect) override;
  void SetRoot(Layer* aLayer) override;
  already_AddRefed<PaintedLayer> CreatePaintedLayer() override;
  already_AddRefed<ContainerLayer> CreateContainerLayer() override;
  already_AddRefed<ImageLayer> CreateImageLayer() override;
  already_AddRefed<ColorLayer> CreateColorLayer() override;
  already_AddRefed<TextLayer> CreateTextLayer() override;
  already_AddRefed<CanvasLayer> CreateCanvasLayer() override;
  already_AddRefed<RefLayer> CreateRefLayer() override;
  already_AddRefed<BorderLayer> CreateBorderLayer() override;

  bool AreComponentAlphaLayersEnabled() override;
  bool BlendingRequiresIntermediateSurface() override;
  bool SupportsBackdropCopyForComponentAlpha() override;

  // HostLayerManager methods
  void ForcePresent() override;
  TextureFactoryIdentifier GetTextureFactoryIdentifier() override;
  LayersBackend GetBackendType() override;
  void AddInvalidRegion(const nsIntRegion& aRegion) override;
  void ClearApproximatelyVisibleRegions(uint64_t aLayersId,
                                                const Maybe<uint32_t>& aPresShellId) override {}
  void UpdateApproximatelyVisibleRegion(const ScrollableLayerGuid& aGuid,
                                                const CSSIntRegion& aRegion) override {}
  void EndTransaction(const TimeStamp& aTimeStamp, EndTransactionFlags aFlags) override;
  void EndTransaction(DrawPaintedLayerCallback aCallback,
                      void* aCallbackData,
                      EndTransactionFlags aFlags) override;
  Compositor* GetCompositor() const override {
    return nullptr;
  }
  bool IsCompositingToScreen() const override;
  TextureSourceProvider* GetTextureSourceProvider() const override;
  void ClearCachedResources(Layer* aSubtree = nullptr) override;
  void NotifyShadowTreeTransaction() override;
  void UpdateRenderBounds(const gfx::IntRect& aRect) override;

  LayerManagerMLGPU* AsLayerManagerMLGPU() override {
    return this;
  }
  const char* Name() const override {
    return "";
  }

  // This should only be called while a FrameBuilder is live.
  FrameBuilder* GetCurrentFrame() const {
    MOZ_ASSERT(mCurrentFrame);
    return mCurrentFrame;
  }
  MLGDevice* GetDevice() {
    return mDevice;
  }

  TimeStamp GetLastCompositionEndTime() const {
    return mLastCompositionEndTime;
  }
  const nsIntRegion& GetRegionToClear() const {
    return mRegionToClear;
  }
  uint32_t GetDebugFrameNumber() const {
    return mDebugFrameNumber;
  }

private:
  void Composite();
  void ComputeInvalidRegion();
  void RenderLayers();
  void DrawDebugOverlay();
  bool PreRender();
  void PostRender();

private:
  RefPtr<MLGDevice> mDevice;
  RefPtr<MLGSwapChain> mSwapChain;
  RefPtr<TextureSourceProviderMLGPU> mTextureSourceProvider;
  RefPtr<TextRenderer> mTextRenderer;
  widget::CompositorWidget* mWidget;

  UniquePtr<LayerProperties> mClonedLayerTreeProperties;
  nsIntRegion mNextFrameInvalidRegion;
  gfx::IntRect mRenderBounds;

  // These are per-frame only.
  bool mDrawDiagnostics;
  bool mUsingInvalidation;
  nsIntRegion mInvalidRegion;
  Maybe<widget::WidgetRenderingContext> mWidgetContext;
  
  IntSize mWindowSize;
  TimeStamp mCompositionStartTime;
  TimeStamp mLastCompositionEndTime;

  RefPtr<DrawTarget> mTarget;
  gfx::IntRect mTargetRect;
  FrameBuilder* mCurrentFrame;

  // The debug frame number is incremented every frame and is included in the
  // WorldConstants bound to vertex shaders. This allows us to correlate
  // a frame in RenderDoc to spew in the console.
  uint32_t mDebugFrameNumber;
  RefPtr<MLGBuffer> mDiagnosticVertices;
};

} // namespace layers
} // namespace mozilla

#endif
