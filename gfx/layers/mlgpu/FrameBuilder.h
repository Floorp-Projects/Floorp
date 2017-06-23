/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_FrameBuilder_h
#define mozilla_gfx_layers_mlgpu_FrameBuilder_h

#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Types.h"
#include "MaskOperation.h"
#include "nsDataHashtable.h"
#include "nsRefPtrHashtable.h"
#include "ShaderDefinitionsMLGPU.h"
#include "SharedBufferMLGPU.h"
#include "Units.h"
#include <map>
#include <vector>

namespace mozilla {
namespace layers {

class ContainerLayer;
class ContainerLayerMLGPU;
class Layer;
class LayerMLGPU;
class LayerManagerMLGPU;
class MLGDevice;
class MLGRenderTarget;
class MLGSwapChain;
class RenderViewMLGPU;
struct ItemInfo;

class FrameBuilder final
{
public:
  FrameBuilder(LayerManagerMLGPU* aManager, MLGSwapChain* aSwapChain);
  ~FrameBuilder();

  bool Build();
  void Render();

  bool AddLayerToConstantBuffer(ItemInfo& aItem);

  LayerManagerMLGPU* GetManager() const {
    return mManager;
  }
  MLGDevice* GetDevice() const {
    return mDevice;
  }
  const ConstantBufferSection& GetDefaultMaskInfo() const {
    return mDefaultMaskInfo;
  }

  // Called during tile construction. Finds or adds a mask layer chain to the
  // cache, that will be flattened as a dependency to rendering batches.
  MaskOperation* AddMaskOperation(LayerMLGPU* aLayer);

  // Note: These should only be called during batch construction.
  size_t CurrentLayerBufferIndex() const;
  size_t CurrentMaskRectBufferIndex() const;

  // These are called during rendering, and may return null if a buffer
  // couldn't be allocated.
  ConstantBufferSection GetLayerBufferByIndex(size_t aIndex) const;
  ConstantBufferSection GetMaskRectBufferByIndex(size_t aIndex) const;

  // Hold a layer alive until the frame ends.
  void RetainTemporaryLayer(LayerMLGPU* aLayer);

private:
  void AssignLayer(Layer* aLayer,
                   RenderViewMLGPU* aView,
                   const RenderTargetIntRect& aClipRect,
                   Maybe<gfx::Polygon>&& aGeometry);

  void ProcessChildList(ContainerLayer* aContainer,
                        RenderViewMLGPU* aView,
                        const RenderTargetIntRect& aParentClipRect,
                        const Maybe<gfx::Polygon>& aParentGeometry);

  mlg::LayerConstants* AllocateLayerInfo(ItemInfo& aItem);
  bool AddMaskRect(const gfx::Rect& aRect, uint32_t* aOutIndex);
  void FinishCurrentLayerBuffer();
  void FinishCurrentMaskRectBuffer();

  // Returns true to continue, false to stop - false does not indicate
  // failure.
  bool ProcessContainerLayer(ContainerLayer* aLayer,
                             RenderViewMLGPU* aView,
                             const RenderTargetIntRect& aClipRect,
                             Maybe<gfx::Polygon>& aGeometry);

private:
  RefPtr<LayerManagerMLGPU> mManager;
  RefPtr<MLGDevice> mDevice;
  RefPtr<MLGSwapChain> mSwapChain;
  RefPtr<RenderViewMLGPU> mWidgetRenderView;
  LayerMLGPU* mRoot;

  // Each time we consume a layer in a tile, we make sure a constant buffer
  // exists that contains information about the layer. The mapping is valid
  // for the most recent buffer, and once the buffer fills, we begin a new
  // one and clear the map.
  nsTArray<ConstantBufferSection> mLayerBuffers;
  nsTArray<mlg::LayerConstants> mCurrentLayerBuffer;
  nsDataHashtable<nsPtrHashKey<LayerMLGPU>, uint32_t> mLayerBufferMap;

  // We keep mask rects in a separate buffer since they're rare.
  nsTArray<ConstantBufferSection> mMaskRectBuffers;
  nsTArray<gfx::Rect> mCurrentMaskRectList;

  // For values that *can* change every render pass, but almost certainly do
  // not, we pre-fill and cache some buffers.
  ConstantBufferSection mDefaultMaskInfo;

  // Cache for MaskOperations.
  nsRefPtrHashtable<nsRefPtrHashKey<TextureSource>, MaskOperation> mSingleTextureMasks;
  std::map<MaskTextureList, RefPtr<MaskCombineOperation>> mCombinedTextureMasks;

  // This list of temporary layers is wiped out when the frame is completed.
  std::vector<RefPtr<Layer>> mTemporaryLayers;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_mlgpu_FrameBuilder_h
