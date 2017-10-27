/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERPASSMLGPU_H
#define MOZILLA_GFX_RENDERPASSMLGPU_H

#include "LayerManagerMLGPU.h"
#include "ShaderDefinitionsMLGPU.h"
#include "SharedBufferMLGPU.h"
#include "StagingBuffer.h"

namespace mozilla {
namespace layers {

using namespace mlg;

class RenderViewMLGPU;

enum class RenderPassType {
  ClearView,
  SolidColor,
  SingleTexture,
  RenderView,
  Video,
  ComponentAlpha,
  Unknown
};

enum class RenderOrder
{
  // Used for all items when not using a depth buffer. Otherwise, used for
  // items that may draw transparent pixels.
  BackToFront,

  // Only used when the depth buffer is enabled, and only for items that are
  // guaranteed to only draw opaque pixels.
  FrontToBack
};

static const uint32_t kInvalidResourceIndex = uint32_t(-1);

struct ItemInfo {
  explicit ItemInfo(FrameBuilder* aBuilder,
                    RenderViewMLGPU* aView,
                    LayerMLGPU* aLayer,
                    int32_t aSortOrder,
                    const gfx::IntRect& aBounds,
                    Maybe<gfx::Polygon>&& aGeometry);

  // Return true if a layer can be clipped by the vertex shader; false
  // otherwise. Any kind of textured mask or non-rectilinear transform
  // will cause this to return false.
  bool HasRectTransformAndClip() const {
    return rectilinear && !layer->GetMask();
  }

  RenderViewMLGPU* view;
  LayerMLGPU* layer;
  RenderPassType type;
  uint32_t layerIndex;
  int32_t sortOrder;
  gfx::IntRect bounds;
  RenderOrder renderOrder;
  Maybe<gfx::Polygon> geometry;

  // Set only when the transform is a 2D integer translation.
  Maybe<gfx::IntPoint> translation;

  // Set when the item bounds will occlude anything below it.
  bool opaque;

  // Set when the item's transform is 2D and rectilinear.
  bool rectilinear;
};

// Base class for anything that can render in a batch to the GPU.
class RenderPassMLGPU
{
  NS_INLINE_DECL_REFCOUNTING(RenderPassMLGPU)

public:
  static RenderPassType GetPreferredPassType(FrameBuilder* aBuilder,
                                             const ItemInfo& aInfo);

  static RefPtr<RenderPassMLGPU> CreatePass(FrameBuilder* aBuilder,
                                            const ItemInfo& aInfo);

  // Return true if this pass is compatible with the given item, false
  // otherwise. This does not guarantee the pass will accept the item,
  // but does guarantee we can try.
  virtual bool IsCompatible(const ItemInfo& aItem);

  virtual RenderPassType GetType() const = 0;

  // Return true if the layer was compatible with and added to this pass,
  // false otherwise.
  bool AcceptItem(ItemInfo& aInfo);

  // Prepare constants buffers and textures.
  virtual void PrepareForRendering();

  // Execute this render pass to the currently selected surface.
  virtual void ExecuteRendering() = 0;

  virtual Maybe<MLGBlendState> GetBlendState() const {
    return Nothing();
  }

  size_t GetLayerBufferIndex() const {
    return mLayerBufferIndex;
  }
  Maybe<uint32_t> GetMaskRectBufferIndex() const {
    return mMaskRectBufferIndex == kInvalidResourceIndex
           ? Nothing()
           : Some(mMaskRectBufferIndex);
  }

  // Returns true if this pass overlaps the affected region of an item. This
  // only ever returns true for transparent items and transparent batches,
  // and should not be used otherwise.
  bool Intersects(const ItemInfo& aItem);

  // Returns true if pass has been successfully prepared.
  bool IsPrepared() const {
    return mPrepared;
  }

protected:
  RenderPassMLGPU(FrameBuilder* aBuilder, const ItemInfo& aItem);
  virtual ~RenderPassMLGPU();

  // Return true if the item was consumed, false otherwise.
  virtual bool AddToPass(LayerMLGPU* aItem, ItemInfo& aInfo) = 0;

protected:
  enum class GeometryMode {
    Unknown,
    UnitQuad,
    Polygon
  };

protected:
  FrameBuilder* mBuilder;
  RefPtr<MLGDevice> mDevice;
  size_t mLayerBufferIndex;
  size_t mMaskRectBufferIndex;
  gfx::IntRegion mAffectedRegion;
  bool mPrepared;
};

// Shader-based render passes execute a draw call, vs. non-shader passes that
// use non-shader APIs (like ClearView).
class ShaderRenderPass : public RenderPassMLGPU
{
public:
  ShaderRenderPass(FrameBuilder* aBuilder, const ItemInfo& aItem);

  // Used by ShaderDefinitions for writing traits.
  VertexStagingBuffer* GetInstances() {
    return &mInstances;
  }

  bool IsCompatible(const ItemInfo& aItem) override;
  void PrepareForRendering() override;
  void ExecuteRendering() override;

  virtual Maybe<MLGBlendState> GetBlendState() const override{
    return Some(MLGBlendState::Over);
  }

protected:
  // If this batch has a uniform opacity, return it here. Otherwise this should
  // return 1.0.
  virtual float GetOpacity() const = 0;

  // Set any components of the pipeline that won't be handled by
  // ExecuteRendering. This is called only once even if multiple draw calls
  // are issued.
  virtual void SetupPipeline() = 0;

protected:
  // Set the geometry this pass will use. This must be called by every
  // derived constructor. Use GeometryMode::Unknown to pick the default
  // behavior: UnitQuads for rectilinear transform+clips, and polygons
  // otherwise.
  void SetGeometry(const ItemInfo& aItem, GeometryMode aMode);

  void SetDefaultGeometry(const ItemInfo& aItem) {
    SetGeometry(aItem, GeometryMode::Unknown);
  }

  // Called after PrepareForRendering() has finished. If this returns false,
  // PrepareForRendering() will return false.
  virtual bool OnPrepareBuffers() {
    return true;
  }

  // Prepare the mask/opacity buffer bound in most pixel shaders.
  bool SetupPSBuffer0(float aOpacity);

  bool HasMask() const {
    return !!mMask;
  }
  MaskOperation* GetMask() const {
    return mMask;
  }

protected:
  GeometryMode mGeometry;
  RefPtr<MaskOperation> mMask;
  bool mHasRectTransformAndClip;

  VertexStagingBuffer mInstances;
  VertexBufferSection mInstanceBuffer;

  ConstantBufferSection mPSBuffer0;
};

// This contains various helper functions for building vertices and shader
// inputs for layers.
template <typename Traits>
class BatchRenderPass : public ShaderRenderPass
{
public:
  BatchRenderPass(FrameBuilder* aBuilder, const ItemInfo& aItem)
   : ShaderRenderPass(aBuilder, aItem)
  {}

protected:
  // It is tricky to determine ahead of time whether or not we'll have enough
  // room in our buffers to hold all draw commands for a layer, especially
  // since layers can have multiple draw rects. We don't want to draw one rect,
  // reject the item, then redraw the same rect again in another batch.
  // To deal with this we use a transaction approach and reject the transaction
  // if we couldn't add everything.
  class Txn {
  public:
    explicit Txn(BatchRenderPass* aPass)
     : mPass(aPass),
       mPrevInstancePos(aPass->mInstances.GetPosition())
    {}

    bool Add(const Traits& aTraits) {
      if (!AddImpl(aTraits)) {
        return Fail();
      }
      return true;
    }

    // Add an item based on a draw rect, layer, and optional geometry. This is
    // defined in RenderPassMLGPU-inl.h, since it needs access to
    // ShaderDefinitionsMLGPU-inl.h.
    bool AddImpl(const Traits& aTraits);

    bool Fail() {
      MOZ_ASSERT(!mStatus.isSome() || !mStatus.value());
      mStatus = Some(false);
      return false;
    }

    bool Commit() {
      MOZ_ASSERT(!mStatus.isSome() || !mStatus.value());
      if (mStatus.isSome()) {
        return false;
      }
      mStatus = Some(true);
      return true;
    }

    ~Txn() {
      if (!mStatus.isSome() || !mStatus.value()) {
        mPass->mInstances.RestorePosition(mPrevInstancePos);
      }
    }

  private:
    BatchRenderPass* mPass;
    VertexStagingBuffer::Position mPrevVertexPos;
    VertexStagingBuffer::Position mPrevItemPos;
    ConstantStagingBuffer::Position mPrevInstancePos;
    Maybe<bool> mStatus;
  };
};

// Shaders which sample from a texture should inherit from this.
class TexturedRenderPass : public BatchRenderPass<TexturedTraits>
{
public:
  explicit TexturedRenderPass(FrameBuilder* aBuilder, const ItemInfo& aItem);

protected:
  struct Info {
    Info(const ItemInfo& aItem, PaintedLayerMLGPU* aLayer);
    Info(const ItemInfo& aItem, TexturedLayerMLGPU* aLayer);
    Info(const ItemInfo& aItem, ContainerLayerMLGPU* aLayer);

    const ItemInfo& item;
    gfx::IntSize textureSize;
    gfx::Point destOrigin;
    Maybe<gfx::Size> scale;
    bool decomposeIntoNoRepeatRects;
  };

  // Add a set of draw rects based on a visible region. The texture size and
  // scaling factor are used to compute uv-coordinates.
  //
  // The origin is the offset from the draw rect to the layer bounds. You can
  // also think of it as the translation from layer space into texture space,
  // pre-scaling. For example, ImageLayers use the texture bounds as their
  // draw rect, so the origin will be (0, 0). ContainerLayer intermediate
  // surfaces, on the other hand, are relative to the target offset of the
  // layer. In all cases the visible region may be partially occluded, so
  // knowing the true origin is important.
  template <typename RegionType>
  bool AddItems(Txn& aTxn,
                const Info& aInfo,
                const RegionType& aDrawRegion)
  {
    for (auto iter = aDrawRegion.RectIter(); !iter.Done(); iter.Next()) {
      gfx::Rect drawRect = gfx::Rect(iter.Get().ToUnknownRect());
      if (!AddItem(aTxn, aInfo, drawRect)) {
        return false;
      }
    }
    return true;
  }

private:
  // Add a draw instance to the given destination rect. Texture coordinates
  // are built from the given texture size, optional scaling factor, and
  // texture origin relative to the draw rect. This will ultimately call
  // AddClippedItem, potentially clipping the draw rect if needed.
  bool AddItem(Txn& aTxn, const Info& aInfo, const gfx::Rect& aDrawRect);

  // Add an item that has gone through any necessary clipping already. This
  // is the final destination for handling textured items.
  bool AddClippedItem(Txn& aTxn, const Info& aInfo, const gfx::Rect& aDrawRect);

protected:
  TextureFlags mTextureFlags;
};

// This is only available when MLGDevice::CanUseClearView returns true.
class ClearViewPass final : public RenderPassMLGPU
{
public:
  ClearViewPass(FrameBuilder* aBuilder, const ItemInfo& aItem);

  bool IsCompatible(const ItemInfo& aItem) override;
  void ExecuteRendering() override;

  RenderPassType GetType() const override {
    return RenderPassType::ClearView;
  }

private:
  bool AddToPass(LayerMLGPU* aItem, ItemInfo& aInfo) override;

private:
  // Note: Not a RefPtr since this would create a cycle.
  RenderViewMLGPU* mView;
  gfx::Color mColor;
  nsTArray<gfx::IntRect> mRects;
};

// SolidColorPass is used when ClearViewPass is not available, or when
// the layer has masks, or subpixel or complex transforms.
class SolidColorPass final : public BatchRenderPass<ColorTraits>
{
public:
  explicit SolidColorPass(FrameBuilder* aBuilder, const ItemInfo& aItem);

  RenderPassType GetType() const override {
    return RenderPassType::SolidColor;
  }

private:
  bool AddToPass(LayerMLGPU* aItem, ItemInfo& aInfo) override;
  void SetupPipeline() override;
  float GetOpacity() const override;
};

class SingleTexturePass final : public TexturedRenderPass
{
public:
  explicit SingleTexturePass(FrameBuilder* aBuilder, const ItemInfo& aItem);

  RenderPassType GetType() const override {
    return RenderPassType::SingleTexture;
  }

private:
  bool AddToPass(LayerMLGPU* aItem, ItemInfo& aInfo) override;
  void SetupPipeline() override;
  float GetOpacity() const override {
    return mOpacity;
  }
  Maybe<MLGBlendState> GetBlendState() const override;

private:
  RefPtr<TextureSource> mTexture;
  SamplerMode mSamplerMode;
  float mOpacity;
};

class ComponentAlphaPass final : public TexturedRenderPass
{
public:
  explicit ComponentAlphaPass(FrameBuilder* aBuilder, const ItemInfo& aItem);

  RenderPassType GetType() const override {
    return RenderPassType::ComponentAlpha;
  }

private:
  bool AddToPass(LayerMLGPU* aItem, ItemInfo& aInfo) override;
  void SetupPipeline() override;
  float GetOpacity() const override;
  Maybe<MLGBlendState> GetBlendState() const override {
    return Some(MLGBlendState::ComponentAlpha);
  }

private:
  PaintedLayerMLGPU* mAssignedLayer;
  RefPtr<TextureSource> mTextureOnBlack;
  RefPtr<TextureSource> mTextureOnWhite;
};

class VideoRenderPass final : public TexturedRenderPass
{
public:
  explicit VideoRenderPass(FrameBuilder* aBuilder, const ItemInfo& aItem);

  RenderPassType GetType() const override {
    return RenderPassType::Video;
  }

private:
  bool AddToPass(LayerMLGPU* aItem, ItemInfo& aInfo) override;
  void SetupPipeline() override;
  float GetOpacity() const override {
    return mOpacity;
  }

private:
  RefPtr<TextureHost> mHost;
  RefPtr<TextureSource> mTexture;
  SamplerMode mSamplerMode;
  float mOpacity;
};

class RenderViewPass final : public TexturedRenderPass
{
public:
  RenderViewPass(FrameBuilder* aBuilder, const ItemInfo& aItem);

  RenderPassType GetType() const override {
    return RenderPassType::RenderView;
  }

private:
  bool AddToPass(LayerMLGPU* aItem, ItemInfo& aInfo) override;
  void SetupPipeline() override;
  bool OnPrepareBuffers() override;
  void ExecuteRendering() override;
  float GetOpacity() const override;
  bool PrepareBlendState();
  void RenderWithBackdropCopy();

private:
  ConstantBufferSection mBlendConstants;
  ContainerLayerMLGPU* mAssignedLayer;
  RefPtr<MLGRenderTarget> mSource;
  // Note: we don't use RefPtr here since that would cause a cycle. RenderViews
  // and RenderPasses are both scoped to the frame anyway.
  RenderViewMLGPU* mParentView;
  gfx::IntRect mBackdropCopyRect;
  Maybe<gfx::CompositionOp> mBlendMode;
};

} // namespace layers
} // namespace mozilla

#endif
