/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_RenderViewMLGPU_h
#define mozilla_gfx_layers_mlgpu_RenderViewMLGPU_h

#include "LayerManagerMLGPU.h"
#include "RenderPassMLGPU.h"
#include "Units.h"
#include <deque>

namespace mozilla {
namespace layers {

class FrameBuilder;
class ContainerLayerMLGPU;
class MLGRenderTarget;

class RenderViewMLGPU
{
public:
  NS_INLINE_DECL_REFCOUNTING(RenderViewMLGPU)

  // Constructor for the widget render target.
  RenderViewMLGPU(FrameBuilder* aBuilder,
                  MLGRenderTarget* aTarget,
                  const nsIntRegion& aInvalidRegion);

  // Constructor for intermediate surfaces.
  RenderViewMLGPU(FrameBuilder* aBuilder,
                  ContainerLayerMLGPU* aContainer,
                  RenderViewMLGPU* aParent);

  void Prepare();
  void Render();
  void AddChild(RenderViewMLGPU* aParent);
  void AddItem(LayerMLGPU* aItem,
               const gfx::IntRect& aBounds,
               Maybe<gfx::Polygon>&& aGeometry);
  void FinishBuilding();

  const gfx::IntPoint& GetTargetOffset() const {
    return mTargetOffset;
  }
  RenderViewMLGPU* GetParent() const {
    return mParent;
  }
  bool HasDepthBuffer() const {
    return mUseDepthBuffer;
  }

  // The size and render target cannot be read until the view has finished
  // building, since we try to right-size the render target to the visible
  // region.
  MLGRenderTarget* GetRenderTarget() const;
  gfx::IntSize GetSize() const;

  gfx::IntRect GetInvalidRect() const {
    return mInvalidBounds;
  }

private:
  RenderViewMLGPU(FrameBuilder* aBuilder, RenderViewMLGPU* aParent);
  ~RenderViewMLGPU();

  void ExecuteRendering();
  bool UpdateVisibleRegion(ItemInfo& aItem);
  void AddItemFrontToBack(LayerMLGPU* aLayer, ItemInfo& aItem);
  void AddItemBackToFront(LayerMLGPU* aLayer, ItemInfo& aItem);

  void PrepareClear();
  void DrawClear();

  void ExecutePass(RenderPassMLGPU* aPass);

  // Return the sorting index offset to use.
  int32_t PrepareDepthBuffer();

private:
  std::deque<RefPtr<RenderPassMLGPU>> mFrontToBack;
  std::deque<RefPtr<RenderPassMLGPU>> mBackToFront;

  FrameBuilder* mBuilder;
  RefPtr<MLGDevice> mDevice;
  RenderViewMLGPU* mParent;
  std::vector<RefPtr<RenderViewMLGPU>> mChildren;

  // Shader data.
  ConstantBufferSection mWorldConstants;

  // If using ClearView-based clears.
  nsTArray<gfx::IntRect> mClearRects;

  // If using shader-based clears.
  VertexBufferSection mClearInput;
  ConstantBufferSection mClearConstants;

  // Either an MLGSwapChain-derived render target, or an intermediate surface.
  RefPtr<MLGRenderTarget> mTarget;

  // For intermediate render targets only, this is the layer owning the render target.
  ContainerLayerMLGPU* mContainer;

  // The offset adjustment from container layer space to render target space.
  // This is 0,0 for the root view.
  gfx::IntPoint mTargetOffset;

  // The invalid bounds as computed by LayerTreeInvalidation. This is the initial
  // render bounds size, if invalidation is disabled.
  gfx::IntRect mInvalidBounds;

  // The actual bounds of the view as computed by summing the visible bounds of
  // child layers. We use this to right-size the intermediate surface. If unset,
  // we do not right-size the intermediate surface.
  Maybe<gfx::IntRect> mActualBounds;

  // The occluded region, which is updated every time we process an opaque,
  // rectangular item. This is not actually in LayerPixels, we do this to
  // avoid FromUnknownRegion which has array copies.
  LayerIntRegion mOccludedRegion;

  // True if we've finished adding layers to the view.
  bool mFinishedBuilding;

  // This state is used to avoid changing buffers while we execute batches.
  size_t mCurrentLayerBufferIndex;
  size_t mCurrentMaskRectBufferIndex;

  // Depth-buffer tracking.
  int32_t mNextSortIndex;
  bool mUseDepthBuffer;
  bool mDepthBufferNeedsClear;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_mlgpu_RenderViewMLGPU_h
