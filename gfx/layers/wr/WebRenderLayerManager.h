/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERLAYERMANAGER_H
#define GFX_WEBRENDERLAYERMANAGER_H

#include "Layers.h"
#include "mozilla/layers/CompositorController.h"
#include "mozilla/layers/TransactionIdAllocator.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/webrender/WebRenderAPI.h"

class nsIWidget;

namespace mozilla {
namespace layers {

class CompositorBridgeChild;
class ImageClientSingle;
class KnowsCompositor;
class PCompositorBridgeChild;
class WebRenderBridgeChild;
class WebRenderLayerManager;
class APZCTreeManager;

class WebRenderLayer
{
public:
  virtual Layer* GetLayer() = 0;
  virtual void RenderLayer(wr::DisplayListBuilder& aBuilder) = 0;
  virtual Maybe<WrImageMask> RenderMaskLayer(const gfx::Matrix4x4& aTransform)
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
                                     uint64_t aExternalImageId);

  WebRenderLayerManager* WrManager();
  WebRenderBridgeChild* WrBridge();
  WrImageKey GetImageKey();

  gfx::Rect RelativeToVisible(gfx::Rect aRect);
  gfx::Rect RelativeToTransformedVisible(gfx::Rect aRect);
  gfx::Rect ParentStackingContextBounds();
  gfx::Rect RelativeToParent(gfx::Rect aRect);
  gfx::Rect VisibleBoundsRelativeToParent();
  gfx::Point GetOffsetToParent();
  gfx::Rect TransformedVisibleBoundsRelativeToParent();
protected:
  gfx::Rect GetWrBoundsRect();
  gfx::Rect GetWrRelBounds();
  gfx::Rect GetWrClipRect(gfx::Rect& aRect);
  gfx::Matrix4x4 GetWrBoundTransform();
  void DumpLayerInfo(const char* aLayerType, gfx::Rect& aRect);
  Maybe<WrImageMask> BuildWrMaskLayer(bool aUnapplyLayerTransform);
};

class WebRenderLayerManager final : public LayerManager
{
  typedef nsTArray<RefPtr<Layer> > LayerRefArray;

public:
  explicit WebRenderLayerManager(nsIWidget* aWidget);
  void Initialize(PCompositorBridgeChild* aCBChild, wr::PipelineId aLayersId, TextureFactoryIdentifier* aTextureFactoryIdentifier);

  virtual void Destroy() override;

protected:
  virtual ~WebRenderLayerManager();

public:
  virtual KnowsCompositor* AsKnowsCompositor() override;
  WebRenderLayerManager* AsWebRenderLayerManager() override { return this; }
  virtual CompositorBridgeChild* GetCompositorBridgeChild() override;

  virtual int32_t GetMaxTextureSize() const override;

  virtual bool BeginTransactionWithTarget(gfxContext* aTarget) override;
  virtual bool BeginTransaction() override;
  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override;
  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) override;

  virtual LayersBackend GetBackendType() override { return LayersBackend::LAYERS_WR; }
  virtual void GetBackendName(nsAString& name) override { name.AssignLiteral("WebRender"); }
  virtual const char* Name() const override { return "WebRender"; }

  virtual void SetRoot(Layer* aLayer) override;

  virtual already_AddRefed<PaintedLayer> CreatePaintedLayer() override;
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer() override;
  virtual already_AddRefed<ImageLayer> CreateImageLayer() override;
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer() override;
  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer() override;
  virtual already_AddRefed<ColorLayer> CreateColorLayer() override;
  virtual already_AddRefed<RefLayer> CreateRefLayer() override;
  virtual already_AddRefed<TextLayer> CreateTextLayer() override;
  virtual already_AddRefed<BorderLayer> CreateBorderLayer() override;
  virtual already_AddRefed<DisplayItemLayer> CreateDisplayItemLayer() override;

  virtual bool NeedsWidgetInvalidation() override { return false; }

  virtual void SetLayerObserverEpoch(uint64_t aLayerObserverEpoch) override;

  virtual void DidComposite(uint64_t aTransactionId,
                            const mozilla::TimeStamp& aCompositeStart,
                            const mozilla::TimeStamp& aCompositeEnd) override;

  virtual void ClearCachedResources(Layer* aSubtree = nullptr) override;
  virtual void UpdateTextureFactoryIdentifier(const TextureFactoryIdentifier& aNewIdentifier,
                                              uint64_t aDeviceResetSeqNo) override;
  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() override;

  virtual void SetTransactionIdAllocator(TransactionIdAllocator* aAllocator) override
  { mTransactionIdAllocator = aAllocator; }

  virtual void AddDidCompositeObserver(DidCompositeObserver* aObserver) override;
  virtual void RemoveDidCompositeObserver(DidCompositeObserver* aObserver) override;

  virtual void FlushRendering() override;

  virtual void SendInvalidRegion(const nsIntRegion& aRegion) override;

  virtual void Composite() override;

  virtual void SetNeedsComposite(bool aNeedsComposite) override
  {
    mNeedsComposite = aNeedsComposite;
  }
  virtual bool NeedsComposite() const override { return mNeedsComposite; }

  DrawPaintedLayerCallback GetPaintedLayerCallback() const
  { return mPaintedLayerCallback; }

  void* GetPaintedLayerCallbackData() const
  { return mPaintedLayerCallbackData; }

  // adds an imagekey to a list of keys that will be discarded on the next
  // transaction or destruction
  void AddImageKeyForDiscard(wr::ImageKey);
  void DiscardImages();

  // Before destroying a layer with animations, add its compositorAnimationsId
  // to a list of ids that will be discarded on the next transaction
  void AddCompositorAnimationsIdForDiscard(uint64_t aId);
  void DiscardCompositorAnimations();

  WebRenderBridgeChild* WrBridge() const { return mWrChild; }

  void Hold(Layer* aLayer);

private:
  /**
   * Take a snapshot of the parent context, and copy
   * it into mTarget.
   */
  void MakeSnapshotIfRequired(LayoutDeviceIntSize aSize);

  void ClearLayer(Layer* aLayer);

private:
  nsIWidget* MOZ_NON_OWNING_REF mWidget;
  std::vector<wr::ImageKey> mImageKeys;
  std::vector<uint64_t> mDiscardedCompositorAnimationsIds;

  /* PaintedLayer callbacks; valid at the end of a transaciton,
   * while rendering */
  DrawPaintedLayerCallback mPaintedLayerCallback;
  void *mPaintedLayerCallbackData;

  RefPtr<WebRenderBridgeChild> mWrChild;

  RefPtr<TransactionIdAllocator> mTransactionIdAllocator;
  uint64_t mLatestTransactionId;

  nsTArray<DidCompositeObserver*> mDidCompositeObservers;

  LayerRefArray mKeepAlive;

  bool mNeedsComposite;

 // When we're doing a transaction in order to draw to a non-default
 // target, the layers transaction is only performed in order to send
 // a PLayers:Update.  We save the original non-default target to
 // mTarget, and then perform the transaction. After the transaction ends,
 // we send a message to our remote side to capture the actual pixels
 // being drawn to the default target, and then copy those pixels
 // back to mTarget.
 RefPtr<gfxContext> mTarget;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_WEBRENDERLAYERMANAGER_H */
