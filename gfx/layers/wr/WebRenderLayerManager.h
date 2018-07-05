/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERLAYERMANAGER_H
#define GFX_WEBRENDERLAYERMANAGER_H

#include <unordered_set>
#include <vector>

#include "gfxPrefs.h"
#include "Layers.h"
#include "mozilla/MozPromise.h"
#include "mozilla/layers/APZTestData.h"
#include "mozilla/layers/FocusTarget.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/TransactionIdAllocator.h"
#include "mozilla/layers/WebRenderCommandBuilder.h"
#include "mozilla/layers/WebRenderScrollData.h"
#include "mozilla/layers/WebRenderUserData.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "nsDisplayList.h"

class nsIWidget;

namespace mozilla {

struct ActiveScrolledRoot;

namespace dom {
class TabGroup;
}

namespace layers {

class CompositorBridgeChild;
class KnowsCompositor;
class PCompositorBridgeChild;
class WebRenderBridgeChild;
class WebRenderParentCommand;

class WebRenderLayerManager final : public LayerManager
{
  typedef nsTArray<RefPtr<Layer> > LayerRefArray;
  typedef nsTHashtable<nsRefPtrHashKey<WebRenderUserData>> WebRenderUserDataRefTable;

public:
  explicit WebRenderLayerManager(nsIWidget* aWidget);
  bool Initialize(PCompositorBridgeChild* aCBChild, wr::PipelineId aLayersId, TextureFactoryIdentifier* aTextureFactoryIdentifier);

  virtual void Destroy() override;

  void DoDestroy(bool aIsSync);

protected:
  virtual ~WebRenderLayerManager();

public:
  virtual KnowsCompositor* AsKnowsCompositor() override;
  WebRenderLayerManager* AsWebRenderLayerManager() override { return this; }
  virtual CompositorBridgeChild* GetCompositorBridgeChild() override;

  // WebRender can handle images larger than the max texture size via tiling.
  virtual int32_t GetMaxTextureSize() const override { return INT32_MAX; }

  virtual bool BeginTransactionWithTarget(gfxContext* aTarget) override;
  virtual bool BeginTransaction() override;
  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override;
  void EndTransactionWithoutLayer(nsDisplayList* aDisplayList,
                                  nsDisplayListBuilder* aDisplayListBuilder,
                                  const nsTArray<wr::WrFilterOp>& aFilters = nsTArray<wr::WrFilterOp>(),
                                  WebRenderBackgroundData* aBackground = nullptr);
  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) override;

  virtual LayersBackend GetBackendType() override { return LayersBackend::LAYERS_WR; }
  virtual void GetBackendName(nsAString& name) override { name.AssignLiteral("WebRender"); }
  virtual const char* Name() const override { return "WebRender"; }

  virtual void SetRoot(Layer* aLayer) override;

  already_AddRefed<PaintedLayer> CreatePaintedLayer() override { return nullptr; }
  already_AddRefed<ContainerLayer> CreateContainerLayer() override { return nullptr; }
  already_AddRefed<ImageLayer> CreateImageLayer() override { return nullptr; }
  already_AddRefed<ColorLayer> CreateColorLayer() override { return nullptr; }
  already_AddRefed<CanvasLayer> CreateCanvasLayer() override { return nullptr; }

  virtual bool NeedsWidgetInvalidation() override { return false; }

  virtual void SetLayerObserverEpoch(uint64_t aLayerObserverEpoch) override;

  virtual void DidComposite(TransactionId aTransactionId,
                            const mozilla::TimeStamp& aCompositeStart,
                            const mozilla::TimeStamp& aCompositeEnd) override;

  virtual void ClearCachedResources(Layer* aSubtree = nullptr) override;
  virtual void UpdateTextureFactoryIdentifier(const TextureFactoryIdentifier& aNewIdentifier) override;
  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() override;

  virtual void SetTransactionIdAllocator(TransactionIdAllocator* aAllocator) override;
  virtual TransactionId GetLastTransactionId() override;

  virtual void AddDidCompositeObserver(DidCompositeObserver* aObserver) override;
  virtual void RemoveDidCompositeObserver(DidCompositeObserver* aObserver) override;

  virtual void FlushRendering() override;
  virtual void WaitOnTransactionProcessed() override;

  virtual void SendInvalidRegion(const nsIntRegion& aRegion) override;

  virtual void ScheduleComposite() override;

  virtual void SetNeedsComposite(bool aNeedsComposite) override
  {
    mNeedsComposite = aNeedsComposite;
  }
  virtual bool NeedsComposite() const override { return mNeedsComposite; }
  virtual void SetIsFirstPaint() override { mIsFirstPaint = true; }
  virtual void SetFocusTarget(const FocusTarget& aFocusTarget) override;

  virtual already_AddRefed<PersistentBufferProvider>
  CreatePersistentBufferProvider(const gfx::IntSize& aSize, gfx::SurfaceFormat aFormat) override;

  bool AsyncPanZoomEnabled() const override;

  // adds an imagekey to a list of keys that will be discarded on the next
  // transaction or destruction
  void AddImageKeyForDiscard(wr::ImageKey);
  void DiscardImages();
  void DiscardLocalImages();

  // Methods to manage the compositor animation ids. Active animations are still
  // going, and when they end we discard them and remove them from the active
  // list.
  void AddActiveCompositorAnimationId(uint64_t aId);
  void AddCompositorAnimationsIdForDiscard(uint64_t aId);
  void DiscardCompositorAnimations();

  WebRenderBridgeChild* WrBridge() const { return mWrChild; }

  // See equivalent function in ClientLayerManager
  void LogTestDataForCurrentPaint(FrameMetrics::ViewID aScrollId,
                                  const std::string& aKey,
                                  const std::string& aValue) {
    MOZ_ASSERT(gfxPrefs::APZTestLoggingEnabled(), "don't call me");
    mApzTestData.LogTestDataForPaint(mPaintSequenceNumber, aScrollId, aKey, aValue);
  }
  // See equivalent function in ClientLayerManager
  const APZTestData& GetAPZTestData() const
  { return mApzTestData; }

  WebRenderCommandBuilder& CommandBuilder() { return mWebRenderCommandBuilder; }
  WebRenderUserDataRefTable* GetWebRenderUserDataTable() { return mWebRenderCommandBuilder.GetWebRenderUserDataTable(); }
  WebRenderScrollData& GetScrollData() { return mScrollData; }

  void WrUpdated();
  void WindowOverlayChanged() { mWindowOverlayChanged = true; }
  nsIWidget* GetWidget() { return mWidget; }

  dom::TabGroup* GetTabGroup();

private:
  /**
   * Take a snapshot of the parent context, and copy
   * it into mTarget.
   */
  void MakeSnapshotIfRequired(LayoutDeviceIntSize aSize);

private:
  nsIWidget* MOZ_NON_OWNING_REF mWidget;
  nsTArray<wr::ImageKey> mImageKeysToDelete;

  // Set of compositor animation ids for which there are active animations (as
  // of the last transaction) on the compositor side.
  std::unordered_set<uint64_t> mActiveCompositorAnimationIds;
  // Compositor animation ids for animations that are done now and that we want
  // the compositor to discard information for.
  nsTArray<uint64_t> mDiscardedCompositorAnimationsIds;

  RefPtr<WebRenderBridgeChild> mWrChild;

  RefPtr<TransactionIdAllocator> mTransactionIdAllocator;
  TransactionId mLatestTransactionId;

  nsTArray<DidCompositeObserver*> mDidCompositeObservers;

  // This holds the scroll data that we need to send to the compositor for
  // APZ to do it's job
  WebRenderScrollData mScrollData;

  bool mWindowOverlayChanged;
  bool mNeedsComposite;
  bool mIsFirstPaint;
  FocusTarget mFocusTarget;

  // When we're doing a transaction in order to draw to a non-default
  // target, the layers transaction is only performed in order to send
  // a PLayers:Update.  We save the original non-default target to
  // mTarget, and then perform the transaction. After the transaction ends,
  // we send a message to our remote side to capture the actual pixels
  // being drawn to the default target, and then copy those pixels
  // back to mTarget.
  RefPtr<gfxContext> mTarget;

  // See equivalent field in ClientLayerManager
  uint32_t mPaintSequenceNumber;
  // See equivalent field in ClientLayerManager
  APZTestData mApzTestData;

  TimeStamp mTransactionStart;
  WebRenderCommandBuilder mWebRenderCommandBuilder;

  size_t mLastDisplayListSize;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_WEBRENDERLAYERMANAGER_H */
