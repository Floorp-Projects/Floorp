/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_PAINTING_WINDOWRENDERER_H
#define MOZILLA_PAINTING_WINDOWRENDERER_H

#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/dom/Animation.h"  // for Animation
#include "mozilla/layers/ScrollableLayerGuid.h"  // for ScrollableLayerGuid, ScrollableLayerGuid::ViewID
#include "mozilla/ScrollPositionUpdate.h"  // for ScrollPositionUpdate
#include "nsRefPtrHashtable.h"             // for nsRefPtrHashtable
#include "gfxContext.h"

namespace mozilla {
namespace layers {
class LayerManager;
class WebRenderLayerManager;
class KnowsCompositor;
class CompositorBridgeChild;
class FrameUniformityData;
class PersistentBufferProvider;
}  // namespace layers
class FallbackRenderer;
class nsDisplayListBuilder;
class nsDisplayList;

class FrameRecorder {
 public:
  /**
   * Record (and return) frame-intervals and paint-times for frames which were
   * presented between calling StartFrameTimeRecording and
   * StopFrameTimeRecording.
   *
   * - Uses a cyclic buffer and serves concurrent consumers, so if Stop is
   *   called too late
   *     (elements were overwritten since Start), result is considered invalid
   *      and hence empty.)
   * - Buffer is capable of holding 10 seconds @ 60fps (or more if frames were
   *   less frequent).
   *     Can be changed (up to 1 hour) via pref:
   *     toolkit.framesRecording.bufferSize.
   * - Note: the first frame-interval may be longer than expected because last
   *   frame
   *     might have been presented some time before calling
   *     StartFrameTimeRecording.
   */

  /**
   * Returns a handle which represents current recording start position.
   */
  virtual uint32_t StartFrameTimeRecording(int32_t aBufferSize);

  /**
   *  Clears, then populates aFrameIntervals with the recorded frame timing
   *  data. The array will be empty if data was overwritten since
   *  aStartIndex was obtained.
   */
  virtual void StopFrameTimeRecording(uint32_t aStartIndex,
                                      nsTArray<float>& aFrameIntervals);

  void RecordFrame();

 private:
  struct FramesTimingRecording {
    // Stores state and data for frame intervals and paint times recording.
    // see LayerManager::StartFrameTimeRecording() at Layers.cpp for more
    // details.
    FramesTimingRecording()
        : mNextIndex(0),
          mLatestStartIndex(0),
          mCurrentRunStartIndex(0),
          mIsPaused(true) {}
    nsTArray<float> mIntervals;
    TimeStamp mLastFrameTime;
    uint32_t mNextIndex;
    uint32_t mLatestStartIndex;
    uint32_t mCurrentRunStartIndex;
    bool mIsPaused;
  };
  FramesTimingRecording mRecording;
};

/**
 * WindowRenderer is the retained rendering object owned by an nsIWidget for
 * drawing the contents of that window, the role previously handled by
 * LayerManager.
 *
 * It can be WebRender, (deprecated) Layers, or an immediate-mode
 * FallbackRenderer.
 *
 * The intention is for LayerManager to be removed entirely in the near future,
 * with WebRender inheriting directly from this class. It is likely that more
 * cleanup can be done once that happens.
 */
class WindowRenderer : public FrameRecorder {
  NS_INLINE_DECL_REFCOUNTING(WindowRenderer)

 public:
  // Cast to implementation types.
  virtual layers::WebRenderLayerManager* AsWebRender() { return nullptr; }
  virtual FallbackRenderer* AsFallback() { return nullptr; }

  // Required functionality

  /**
   * Start a new transaction. Nested transactions are not allowed so
   * there must be no transaction currently in progress.
   * This transaction will update the state of the window from which
   * this LayerManager was obtained.
   */
  virtual bool BeginTransaction(const nsCString& aURL = nsCString()) = 0;

  enum EndTransactionFlags {
    END_DEFAULT = 0,
    END_NO_IMMEDIATE_REDRAW = 1 << 0,  // Do not perform the drawing phase
    END_NO_COMPOSITE =
        1 << 1,  // Do not composite after drawing painted layer contents.
    END_NO_REMOTE_COMPOSITE = 1 << 2  // Do not schedule a composition with a
                                      // remote Compositor, if one exists.
  };

  /**
   * Attempts to end an "empty transaction". There must have been no
   * changes to the layer tree since the BeginTransaction().
   * It's possible for this to fail; PaintedLayers may need to be updated
   * due to VRAM data being lost, for example. In such cases this method
   * returns false, and the caller must proceed with a normal layer tree
   * update and EndTransaction.
   */
  virtual bool EndEmptyTransaction(
      EndTransactionFlags aFlags = END_DEFAULT) = 0;

  virtual void Destroy() {}

  /**
   * Type of layer manager this is. This is to be used sparsely in order to
   * avoid a lot of Layers backend specific code. It should be used only when
   * Layers backend specific functionality is necessary.
   */
  virtual layers::LayersBackend GetBackendType() = 0;

  /**
   * Type of layers backend that will be used to composite this layer tree.
   * When compositing is done remotely, then this returns the layers type
   * of the compositor.
   */
  virtual layers::LayersBackend GetCompositorBackendType() {
    return GetBackendType();
  }

  /**
   * Checks if we need to invalidate the OS widget to trigger
   * painting when updating this renderer.
   */
  virtual bool NeedsWidgetInvalidation() { return true; }

  /**
   * Make sure that the previous transaction has been entirely
   * completed.
   *
   * Note: This may sychronously wait on a remote compositor
   * to complete rendering.
   */
  virtual void FlushRendering(wr::RenderReasons aReasons) {}

  /**
   * Make sure that the previous transaction has been
   * received. This will synchronsly wait on a remote compositor.
   */
  virtual void WaitOnTransactionProcessed() {}

  virtual bool IsCompositingCheap() { return true; }

  /**
   * returns the maximum texture size on this layer backend, or INT32_MAX
   * if there is no maximum
   */
  virtual int32_t GetMaxTextureSize() const { return INT32_MAX; }

  /**
   * Return the name of the layer manager's backend.
   */
  virtual void GetBackendName(nsAString& aName) = 0;

  virtual void GetFrameUniformity(layers::FrameUniformityData* aOutData) {}

  virtual bool AddPendingScrollUpdateForNextTransaction(
      layers::ScrollableLayerGuid::ViewID aScrollId,
      const ScrollPositionUpdate& aUpdateInfo) {
    return false;
  }

  /**
   * Creates a PersistentBufferProvider for use with canvas which is optimized
   * for inter-operating with this layermanager.
   */
  virtual already_AddRefed<layers::PersistentBufferProvider>
  CreatePersistentBufferProvider(const mozilla::gfx::IntSize& aSize,
                                 mozilla::gfx::SurfaceFormat aFormat,
                                 bool aWillReadFrequently = false);

  // Helper wrappers around cast to impl and then cast again.

  virtual layers::KnowsCompositor* AsKnowsCompositor() { return nullptr; }

  virtual layers::CompositorBridgeChild* GetCompositorBridgeChild() {
    return nullptr;
  }

  // Provided functionality

  void AddPartialPrerenderedAnimation(uint64_t aCompositorAnimationId,
                                      dom::Animation* aAnimation);
  void RemovePartialPrerenderedAnimation(uint64_t aCompositorAnimationId,
                                         dom::Animation* aAnimation);
  void UpdatePartialPrerenderedAnimations(
      const nsTArray<uint64_t>& aJankedAnimations);

  const TimeStamp& GetAnimationReadyTime() const { return mAnimationReadyTime; }

 protected:
  virtual ~WindowRenderer() = default;

  // Transform animations which are not fully pre-rendered because it's on a
  // large frame.  We need to update the pre-rendered area once after we tried
  // to composite area which is outside of the pre-rendered area on the
  // compositor.
  nsRefPtrHashtable<nsUint64HashKey, dom::Animation>
      mPartialPrerenderedAnimations;

  // The time when painting most recently finished. This is recorded so that
  // we can time any play-pending animations from this point.
  TimeStamp mAnimationReadyTime;
};

/**
 * FallbackRenderer is non-retained renderer that acts as a direct wrapper
 * around calling Paint on the provided DisplayList. This is used for cases
 * where initializing WebRender is too costly, and we don't need
 * retaining/invalidation (like small popup windows).
 *
 * It doesn't support any sort of EmptyTransaction, and only draws during
 * EndTransaction if a composite is requested (no END_NO_COMPOSITE flag
 * provided)
 */
class FallbackRenderer : public WindowRenderer {
 public:
  FallbackRenderer* AsFallback() override { return this; }

  void SetTarget(gfxContext* aContext, layers::BufferMode aDoubleBuffering);

  bool BeginTransaction(const nsCString& aURL = nsCString()) override;

  bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override {
    return false;
  }

  layers::LayersBackend GetBackendType() override {
    return layers::LayersBackend::LAYERS_NONE;
  }

  virtual void GetBackendName(nsAString& name) override {
    name.AssignLiteral("Fallback");
  }

  bool IsCompositingCheap() override { return false; }

  void EndTransactionWithColor(const nsIntRect& aRect,
                               const gfx::DeviceColor& aColor);
  void EndTransactionWithList(nsDisplayListBuilder* aBuilder,
                              nsDisplayList* aList,
                              int32_t aAppUnitsPerDevPixel,
                              EndTransactionFlags aFlags);

  gfxContext* mTarget;
  layers::BufferMode mBufferMode;
};

}  // namespace mozilla

#endif /* MOZILLA_PAINTING_WINDOWRENDERER_H */
