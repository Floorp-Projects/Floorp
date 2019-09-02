/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LayerManagerComposite_H
#define GFX_LayerManagerComposite_H

#include <stdint.h>            // for int32_t, uint32_t
#include "CompositableHost.h"  // for CompositableHost, ImageCompositeNotificationInfo
#include "GLDefs.h"            // for GLenum
#include "Layers.h"
#include "Units.h"               // for ParentLayerIntRect
#include "mozilla/Assertions.h"  // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"  // for override
#include "mozilla/RefPtr.h"      // for RefPtr, already_AddRefed
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Point.h"  // for IntSize
#include "mozilla/gfx/Rect.h"   // for Rect
#include "mozilla/gfx/Types.h"  // for SurfaceFormat
#include "mozilla/layers/CompositionRecorder.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/Effects.h"  // for EffectChain
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/layers/LayersTypes.h"  // for LayersBackend, etc
#include "mozilla/Maybe.h"               // for Maybe
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsAString.h"
#include "mozilla/RefPtr.h"   // for nsRefPtr
#include "nsCOMPtr.h"         // for already_AddRefed
#include "nsDebug.h"          // for NS_ASSERTION
#include "nsISupportsImpl.h"  // for Layer::AddRef, etc
#include "nsRect.h"           // for mozilla::gfx::IntRect
#include "nsRegion.h"         // for nsIntRegion
#include "nscore.h"           // for nsAString, etc
#include "LayerTreeInvalidation.h"
#include "mozilla/layers/CompositorScreenshotGrabber.h"

class gfxContext;

#ifdef XP_WIN
#  include <windows.h>
#endif

namespace mozilla {
namespace gfx {
class DrawTarget;
}  // namespace gfx

namespace layers {

class CanvasLayerComposite;
class ColorLayerComposite;
class Compositor;
class ContainerLayerComposite;
class Diagnostics;
struct EffectChain;
class ImageLayer;
class ImageLayerComposite;
class LayerComposite;
class RefLayerComposite;
class PaintedLayerComposite;
class TextRenderer;
class CompositingRenderTarget;
struct FPSState;
class PaintCounter;
class LayerMLGPU;
class LayerManagerMLGPU;
class UiCompositorControllerParent;

static const int kVisualWarningDuration = 150;  // ms

// An implementation of LayerManager that acts as a pair with ClientLayerManager
// and is mirrored across IPDL. This gets managed/updated by
// LayerTransactionParent.
class HostLayerManager : public LayerManager {
 public:
  HostLayerManager();
  virtual ~HostLayerManager();

  bool BeginTransactionWithTarget(gfxContext* aTarget,
                                  const nsCString& aURL) override {
    MOZ_CRASH("GFX: Use BeginTransactionWithDrawTarget");
  }

  bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override {
    MOZ_CRASH("GFX: Use EndTransaction(aTimeStamp)");
    return false;
  }

  void EndTransaction(DrawPaintedLayerCallback aCallback, void* aCallbackData,
                      EndTransactionFlags aFlags = END_DEFAULT) override {
    MOZ_CRASH("GFX: Use EndTransaction(aTimeStamp)");
  }

  int32_t GetMaxTextureSize() const override {
    MOZ_CRASH("GFX: Call on compositor, not LayerManagerComposite");
  }

  void GetBackendName(nsAString& name) override {
    MOZ_CRASH("GFX: Shouldn't be called for composited layer manager");
  }

  virtual void ForcePresent() = 0;
  virtual void AddInvalidRegion(const nsIntRegion& aRegion) = 0;

  virtual void NotifyShadowTreeTransaction() {}
  virtual void BeginTransactionWithDrawTarget(gfx::DrawTarget* aTarget,
                                              const gfx::IntRect& aRect) = 0;
  virtual Compositor* GetCompositor() const = 0;
  virtual TextureSourceProvider* GetTextureSourceProvider() const = 0;
  virtual void EndTransaction(const TimeStamp& aTimeStamp,
                              EndTransactionFlags aFlags = END_DEFAULT) = 0;
  virtual void UpdateRenderBounds(const gfx::IntRect& aRect) {}
  virtual void SetDiagnosticTypes(DiagnosticTypes aDiagnostics) {}
  virtual void InvalidateAll() = 0;

  HostLayerManager* AsHostLayerManager() override { return this; }
  virtual LayerManagerMLGPU* AsLayerManagerMLGPU() { return nullptr; }

  void ExtractImageCompositeNotifications(
      nsTArray<ImageCompositeNotificationInfo>* aNotifications) {
    aNotifications->AppendElements(std::move(mImageCompositeNotifications));
  }

  void AppendImageCompositeNotification(
      const ImageCompositeNotificationInfo& aNotification) {
    // Only send composite notifications when we're drawing to the screen,
    // because that's what they mean.
    // Also when we're not drawing to the screen, DidComposite will not be
    // called to extract and send these notifications, so they might linger
    // and contain stale ImageContainerParent pointers.
    if (IsCompositingToScreen()) {
      mImageCompositeNotifications.AppendElement(aNotification);
    }
  }

  /**
   * LayerManagerComposite provides sophisticated debug overlays
   * that can request a next frame.
   */
  bool DebugOverlayWantsNextFrame() { return mDebugOverlayWantsNextFrame; }
  void SetDebugOverlayWantsNextFrame(bool aVal) {
    mDebugOverlayWantsNextFrame = aVal;
  }

  /**
   * Add an on frame warning.
   * @param severity ranges from 0 to 1. It's used to compute the warning color.
   */
  void VisualFrameWarning(float severity) {
    mozilla::TimeStamp now = TimeStamp::Now();
    if (mWarnTime.IsNull() || severity > mWarningLevel ||
        mWarnTime + TimeDuration::FromMilliseconds(kVisualWarningDuration) <
            now) {
      mWarnTime = now;
      mWarningLevel = severity;
    }
  }

  // Indicate that we need to composite even if nothing in our layers has
  // changed, so that the widget can draw something different in its window
  // overlay.
  void SetWindowOverlayChanged() { mWindowOverlayChanged = true; }

  void SetPaintTime(const TimeDuration& aPaintTime) {
    mLastPaintTime = aPaintTime;
  }

  virtual bool AlwaysScheduleComposite() const { return false; }
  virtual bool IsCompositingToScreen() const { return false; }

  void RecordPaintTimes(const PaintTiming& aTiming);
  void RecordUpdateTime(float aValue);

  TimeStamp GetCompositionTime() const { return mCompositionTime; }
  void SetCompositionTime(TimeStamp aTimeStamp) {
    mCompositionTime = aTimeStamp;
    if (!mCompositionTime.IsNull() && !mCompositeUntilTime.IsNull() &&
        mCompositionTime >= mCompositeUntilTime) {
      mCompositeUntilTime = TimeStamp();
    }
  }

  void CompositeUntil(TimeStamp aTimeStamp) {
    if (mCompositeUntilTime.IsNull() || mCompositeUntilTime < aTimeStamp) {
      mCompositeUntilTime = aTimeStamp;
    }
  }
  TimeStamp GetCompositeUntilTime() const { return mCompositeUntilTime; }

  // We maintaining a global mapping from ID to CompositorBridgeParent for
  // async compositables.
  uint64_t GetCompositorBridgeID() const { return mCompositorBridgeID; }
  void SetCompositorBridgeID(uint64_t aID) {
    MOZ_ASSERT(mCompositorBridgeID == 0,
               "The compositor ID must be set only once.");
    mCompositorBridgeID = aID;
  }

  void SetCompositionRecorder(UniquePtr<CompositionRecorder> aRecorder) {
    mCompositionRecorder = std::move(aRecorder);
  }

  /**
   * Write the frames collected by the |CompositionRecorder| to disk.
   *
   * If there is not currently a |CompositionRecorder|, this is a no-op.
   */
  void WriteCollectedFrames();

 protected:
  bool mDebugOverlayWantsNextFrame;
  nsTArray<ImageCompositeNotificationInfo> mImageCompositeNotifications;
  // Testing property. If hardware composer is supported, this will return
  // true if the last frame was deemed 'too complicated' to be rendered.
  float mWarningLevel;
  mozilla::TimeStamp mWarnTime;
  UniquePtr<Diagnostics> mDiagnostics;
  uint64_t mCompositorBridgeID;

  bool mWindowOverlayChanged;
  TimeDuration mLastPaintTime;
  TimeStamp mRenderStartTime;
  UniquePtr<CompositionRecorder> mCompositionRecorder = nullptr;

  // Render time for the current composition.
  TimeStamp mCompositionTime;

  // When nonnull, during rendering, some compositable indicated that it will
  // change its rendering at this time. In order not to miss it, we composite
  // on every vsync until this time occurs (this is the latest such time).
  TimeStamp mCompositeUntilTime;
#if defined(MOZ_WIDGET_ANDROID)
 public:
  // Used by UiCompositorControllerParent to set itself as the target for the
  // contents of the frame buffer after a composite.
  // Implemented in LayerManagerComposite
  virtual void RequestScreenPixels(UiCompositorControllerParent* aController) {}
#endif  // defined(MOZ_WIDGET_ANDROID)
};

// A layer manager implementation that uses the Compositor API
// to render layers.
class LayerManagerComposite final : public HostLayerManager {
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::IntSize IntSize;
  typedef mozilla::gfx::SurfaceFormat SurfaceFormat;

 public:
  explicit LayerManagerComposite(Compositor* aCompositor);
  virtual ~LayerManagerComposite();

  void Destroy() override;

  /**
   * Sets the clipping region for this layer manager. This is important on
   * windows because using OGL we no longer have GDI's native clipping. Therefor
   * widget must tell us what part of the screen is being invalidated,
   * and we should clip to this.
   *
   * \param aClippingRegion Region to clip to. Setting an empty region
   * will disable clipping.
   */
  void SetClippingRegion(const nsIntRegion& aClippingRegion) {
    mClippingRegion = aClippingRegion;
  }

  /**
   * LayerManager implementation.
   */
  LayerManagerComposite* AsLayerManagerComposite() override { return this; }

  void UpdateRenderBounds(const gfx::IntRect& aRect) override;

  bool BeginTransaction(const nsCString& aURL) override;
  void BeginTransactionWithDrawTarget(gfx::DrawTarget* aTarget,
                                      const gfx::IntRect& aRect) override;
  void EndTransaction(const TimeStamp& aTimeStamp,
                      EndTransactionFlags aFlags = END_DEFAULT) override;
  virtual void EndTransaction(
      DrawPaintedLayerCallback aCallback, void* aCallbackData,
      EndTransactionFlags aFlags = END_DEFAULT) override {
    MOZ_CRASH("GFX: Use EndTransaction(aTimeStamp)");
  }

  void SetRoot(Layer* aLayer) override { mRoot = aLayer; }

  // XXX[nrc]: never called, we should move this logic to ClientLayerManager
  // (bug 946926).
  bool CanUseCanvasLayerForSize(const gfx::IntSize& aSize) override;

  void ClearCachedResources(Layer* aSubtree = nullptr) override;

  already_AddRefed<PaintedLayer> CreatePaintedLayer() override;
  already_AddRefed<ContainerLayer> CreateContainerLayer() override;
  already_AddRefed<ImageLayer> CreateImageLayer() override;
  already_AddRefed<ColorLayer> CreateColorLayer() override;
  already_AddRefed<CanvasLayer> CreateCanvasLayer() override;
  already_AddRefed<RefLayer> CreateRefLayer() override;

  bool AreComponentAlphaLayersEnabled() override;

  already_AddRefed<DrawTarget> CreateOptimalMaskDrawTarget(
      const IntSize& aSize) override;

  const char* Name() const override { return ""; }
  bool IsCompositingToScreen() const override;

  bool AlwaysScheduleComposite() const override;

  /**
   * Post-processes layers before composition. This performs the following:
   *
   *   - Applies occlusion culling. This restricts the shadow visible region of
   *     layers that are covered with opaque content.  |aOpaqueRegion| is the
   *     region already known to be covered with opaque content, in the
   *     post-transform coordinate space of aLayer.
   *
   *   - Recomputes visible regions to account for async transforms.
   *     Each layer accumulates into |aVisibleRegion| its post-transform
   *     (including async transforms) visible region.
   *
   *   - aRenderTargetClip is the exact clip required for aLayer, in the
   *     coordinates of the nearest render target (the same as
   *     GetEffectiveTransform).
   *
   *   - aClipFromAncestors is the approximate combined clip from all
   *     ancestors, in the coordinate space of our parent, but maybe be an
   *     overestimate in the presence of complex transforms.
   */
  void PostProcessLayers(nsIntRegion& aOpaqueRegion);
  void PostProcessLayers(Layer* aLayer, nsIntRegion& aOpaqueRegion,
                         LayerIntRegion& aVisibleRegion,
                         const Maybe<RenderTargetIntRect>& aRenderTargetClip,
                         const Maybe<ParentLayerIntRect>& aClipFromAncestors,
                         bool aCanContributeOpaque);

  /**
   * RAII helper class to add a mask effect with the compositable from
   * aMaskLayer to the EffectChain aEffect and notify the compositable when we
   * are done.
   */
  class AutoAddMaskEffect {
   public:
    AutoAddMaskEffect(Layer* aMaskLayer, EffectChain& aEffect);
    ~AutoAddMaskEffect();

    bool Failed() const { return mFailed; }

   private:
    CompositableHost* mCompositable;
    bool mFailed;
  };

  /**
   * returns true if PlatformAllocBuffer will return a buffer that supports
   * direct texturing
   */
  static bool SupportsDirectTexturing();

  static void PlatformSyncBeforeReplyUpdate();

  void AddInvalidRegion(const nsIntRegion& aRegion) override {
    mInvalidRegion.Or(mInvalidRegion, aRegion);
  }

  Compositor* GetCompositor() const override { return mCompositor; }
  TextureSourceProvider* GetTextureSourceProvider() const override {
    return mCompositor;
  }

  void NotifyShadowTreeTransaction() override;

  TextRenderer* GetTextRenderer() { return mTextRenderer; }

  void UnusedApzTransformWarning() { mUnusedApzTransformWarning = true; }
  void DisabledApzWarning() { mDisabledApzWarning = true; }

  bool AsyncPanZoomEnabled() const override;

 public:
  TextureFactoryIdentifier GetTextureFactoryIdentifier() override {
    return mCompositor->GetTextureFactoryIdentifier();
  }
  LayersBackend GetBackendType() override {
    return mCompositor ? mCompositor->GetBackendType()
                       : LayersBackend::LAYERS_NONE;
  }
  void SetDiagnosticTypes(DiagnosticTypes aDiagnostics) override {
    mCompositor->SetDiagnosticTypes(aDiagnostics);
  }

  void InvalidateAll() override {
    AddInvalidRegion(nsIntRegion(mRenderBounds));
  }

  void ForcePresent() override { mCompositor->ForcePresent(); }

 private:
  /** Region we're clipping our current drawing to. */
  nsIntRegion mClippingRegion;
  gfx::IntRect mRenderBounds;

  /** Current root layer. */
  LayerComposite* RootLayer() const;

  /**
   * Update the invalid region and render it.
   */
  void UpdateAndRender();

  /**
   * Render the current layer tree to the active target.
   * Returns true if the current invalid region can be cleared, false if
   * rendering was canceled.
   */
  bool Render(const nsIntRegion& aInvalidRegion,
              const nsIntRegion& aOpaqueRegion);
#if defined(MOZ_WIDGET_ANDROID)
  void RenderToPresentationSurface();
  // Shifts the content down so the toolbar does not cover it.
  // Returns the Y shift of the content in screen pixels
  ScreenCoord GetContentShiftForToolbar();
  // Renders the static snapshot after the content has been rendered.
  void RenderToolbar();
  // Used by robocop tests to get a snapshot of the frame buffer.
  void HandlePixelsTarget();
#endif

  /**
   * We need to know our invalid region before we're ready to render.
   */
  void InvalidateDebugOverlay(nsIntRegion& aInvalidRegion,
                              const gfx::IntRect& aBounds);

  /**
   * Render debug overlays such as the FPS/FrameCounter above the frame.
   */
  void RenderDebugOverlay(const gfx::IntRect& aBounds);

  RefPtr<CompositingRenderTarget> PushGroupForLayerEffects();
  void PopGroupForLayerEffects(RefPtr<CompositingRenderTarget> aPreviousTarget,
                               gfx::IntRect aClipRect, bool aGrayscaleEffect,
                               bool aInvertEffect, float aContrastEffect);

  /**
   * Create or recycle native layers to cover aRegion or aRect.
   * This method takes existing layers from the front of aLayersToRecycle (or
   * creates new layers if no layers are left to recycle) and appends them to
   * the end of mNativeLayers. The "take from front, add to back" approach keeps
   * the layer to rect assignment stable between frames.
   * Updates the rect and opaqueness on the layers. For layers that moved or
   * resized, *aWindowInvalidRegion is updated to include the area impacted by
   * the move.
   * Any layers left in aLayersToRecycle are not needed and can be disposed of.
   */
  void PlaceNativeLayers(const gfx::IntRegion& aRegion, bool aOpaque,
                         std::deque<RefPtr<NativeLayer>>* aLayersToRecycle,
                         gfx::IntRegion* aWindowInvalidRegion);
  void PlaceNativeLayer(const gfx::IntRect& aRect, bool aOpaque,
                        std::deque<RefPtr<NativeLayer>>* aLayersToRecycle,
                        gfx::IntRegion* aWindowInvalidRegion);

  bool mUnusedApzTransformWarning;
  bool mDisabledApzWarning;
  RefPtr<Compositor> mCompositor;
  UniquePtr<LayerProperties> mClonedLayerTreeProperties;

  /**
   * Context target, nullptr when drawing directly to our swap chain.
   */
  RefPtr<gfx::DrawTarget> mTarget;
  gfx::IntRect mTargetBounds;

  nsIntRegion mInvalidRegion;

  bool mInTransaction;
  bool mIsCompositorReady;

  RefPtr<CompositingRenderTarget> mTwoPassTmpTarget;
  CompositorScreenshotGrabber mProfilerScreenshotGrabber;
  RefPtr<TextRenderer> mTextRenderer;
  RefPtr<NativeLayerRoot> mNativeLayerRoot;
  std::deque<RefPtr<NativeLayer>> mNativeLayers;

#ifdef USE_SKIA
  /**
   * Render paint and composite times above the frame.
   */
  void DrawPaintTimes(Compositor* aCompositor);
  RefPtr<PaintCounter> mPaintCounter;
#endif
#if defined(MOZ_WIDGET_ANDROID)
 public:
  virtual void RequestScreenPixels(
      UiCompositorControllerParent* aController) override {
    mScreenPixelsTarget = aController;
  }

 private:
  UiCompositorControllerParent* mScreenPixelsTarget;
#endif  // defined(MOZ_WIDGET_ANDROID)
};

/**
 * Compositor layers are for use with OMTC on the compositor thread only. There
 * must be corresponding Client layers on the content thread. For composite
 * layers, the layer manager only maintains the layer tree.
 */
class HostLayer {
 public:
  explicit HostLayer(HostLayerManager* aManager)
      : mCompositorManager(aManager),
        mShadowOpacity(1.0),
        mShadowTransformSetByAnimation(false),
        mShadowOpacitySetByAnimation(false) {}

  virtual void SetLayerManager(HostLayerManager* aManager) {
    mCompositorManager = aManager;
  }
  HostLayerManager* GetLayerManager() const { return mCompositorManager; }

  virtual ~HostLayer() = default;

  virtual LayerComposite* GetFirstChildComposite() { return nullptr; }

  virtual Layer* GetLayer() = 0;

  virtual LayerMLGPU* AsLayerMLGPU() { return nullptr; }

  virtual bool SetCompositableHost(CompositableHost*) {
    // We must handle this gracefully, see bug 967824
    NS_WARNING(
        "called SetCompositableHost for a layer type not accepting a "
        "compositable");
    return false;
  }
  virtual CompositableHost* GetCompositableHost() = 0;

  /**
   * The following methods are
   *
   * CONSTRUCTION PHASE ONLY
   *
   * They are analogous to the Layer interface.
   */
  void SetShadowVisibleRegion(const LayerIntRegion& aRegion) {
    mShadowVisibleRegion = aRegion;
  }
  void SetShadowVisibleRegion(LayerIntRegion&& aRegion) {
    mShadowVisibleRegion = std::move(aRegion);
  }

  void SetShadowOpacity(float aOpacity) { mShadowOpacity = aOpacity; }
  void SetShadowOpacitySetByAnimation(bool aSetByAnimation) {
    mShadowOpacitySetByAnimation = aSetByAnimation;
  }

  void SetShadowClipRect(const Maybe<ParentLayerIntRect>& aRect) {
    mShadowClipRect = aRect;
  }

  void SetShadowBaseTransform(const gfx::Matrix4x4& aMatrix) {
    mShadowTransform = aMatrix;
  }
  void SetShadowTransformSetByAnimation(bool aSetByAnimation) {
    mShadowTransformSetByAnimation = aSetByAnimation;
  }

  // These getters can be used anytime.
  float GetShadowOpacity() { return mShadowOpacity; }
  const Maybe<ParentLayerIntRect>& GetShadowClipRect() {
    return mShadowClipRect;
  }
  virtual const LayerIntRegion& GetShadowVisibleRegion() {
    return mShadowVisibleRegion;
  }
  const gfx::Matrix4x4& GetShadowBaseTransform() { return mShadowTransform; }
  gfx::Matrix4x4 GetShadowTransform();
  bool GetShadowTransformSetByAnimation() {
    return mShadowTransformSetByAnimation;
  }
  bool GetShadowOpacitySetByAnimation() { return mShadowOpacitySetByAnimation; }

  void RecomputeShadowVisibleRegionFromChildren();

 protected:
  HostLayerManager* mCompositorManager;

  gfx::Matrix4x4 mShadowTransform;
  LayerIntRegion mShadowVisibleRegion;
  Maybe<ParentLayerIntRect> mShadowClipRect;
  float mShadowOpacity;
  bool mShadowTransformSetByAnimation;
  bool mShadowOpacitySetByAnimation;
};

/**
 * Composite layers are for use with OMTC on the compositor thread only. There
 * must be corresponding Client layers on the content thread. For composite
 * layers, the layer manager only maintains the layer tree, all rendering is
 * done by a Compositor (see Compositor.h). As such, composite layers are
 * platform-independent and can be used on any platform for which there is a
 * Compositor implementation.
 *
 * The composite layer tree reflects exactly the basic layer tree. To
 * composite to screen, the layer manager walks the layer tree calling render
 * methods which in turn call into their CompositableHosts' Composite methods.
 * These call Compositor::DrawQuad to do the rendering.
 *
 * Mostly, layers are updated during the layers transaction. This is done from
 * CompositableClient to CompositableHost without interacting with the layer.
 *
 * A reference to the Compositor is stored in LayerManagerComposite.
 */
class LayerComposite : public HostLayer {
 public:
  explicit LayerComposite(LayerManagerComposite* aManager);

  virtual ~LayerComposite();

  void SetLayerManager(HostLayerManager* aManager) override;

  LayerComposite* GetFirstChildComposite() override { return nullptr; }

  /* Do NOT call this from the generic LayerComposite destructor.  Only from the
   * concrete class destructor
   */
  virtual void Destroy();
  virtual void Cleanup() {}

  /**
   * Perform a first pass over the layer tree to render all of the intermediate
   * surfaces that we can. This allows us to avoid framebuffer switches in the
   * middle of our render which is inefficient especially on mobile GPUs. This
   * must be called before RenderLayer.
   */
  virtual void Prepare(const RenderTargetIntRect& aClipRect) {}

  // TODO: This should also take RenderTargetIntRect like Prepare.
  virtual void RenderLayer(const gfx::IntRect& aClipRect,
                           const Maybe<gfx::Polygon>& aGeometry) = 0;

  bool SetCompositableHost(CompositableHost*) override {
    // We must handle this gracefully, see bug 967824
    NS_WARNING(
        "called SetCompositableHost for a layer type not accepting a "
        "compositable");
    return false;
  }

  virtual void CleanupResources() = 0;

  virtual void DestroyFrontBuffer() {}

  void AddBlendModeEffect(EffectChain& aEffectChain);

  virtual void GenEffectChain(EffectChain& aEffect) {}

  void SetLayerComposited(bool value) { mLayerComposited = value; }

  void SetClearRect(const gfx::IntRect& aRect) { mClearRect = aRect; }

  bool HasLayerBeenComposited() { return mLayerComposited; }
  gfx::IntRect GetClearRect() { return mClearRect; }

  // Returns false if the layer is attached to an older compositor.
  bool HasStaleCompositor() const;

  /**
   * Return the part of the visible region that has been fully rendered.
   * While progressive drawing is in progress this region will be
   * a subset of the shadow visible region.
   */
  virtual nsIntRegion GetFullyRenderedRegion();

 protected:
  LayerManagerComposite* mCompositeManager;

  RefPtr<Compositor> mCompositor;
  bool mDestroyed;
  bool mLayerComposited;
  gfx::IntRect mClearRect;
};

// Render aLayer using aCompositor and apply all mask layers of aLayer: The
// layer's own mask layer (aLayer->GetMaskLayer()), and any ancestor mask
// layers.
// If more than one mask layer needs to be applied, we use intermediate surfaces
// (CompositingRenderTargets) for rendering, applying one mask layer at a time.
// Callers need to provide a callback function aRenderCallback that does the
// actual rendering of the source. It needs to have the following form:
// void (EffectChain& effectChain, const Rect& clipRect)
// aRenderCallback is called exactly once, inside this function, unless aLayer's
// visible region is completely clipped out (in that case, aRenderCallback won't
// be called at all).
// This function calls aLayer->AsHostLayer()->AddBlendModeEffect for the
// final rendering pass.
//
// (This function should really live in LayerManagerComposite.cpp, but we
// need to use templates for passing lambdas until bug 1164522 is resolved.)
template <typename RenderCallbackType>
void RenderWithAllMasks(Layer* aLayer, Compositor* aCompositor,
                        const gfx::IntRect& aClipRect,
                        RenderCallbackType aRenderCallback) {
  Layer* firstMask = nullptr;
  size_t maskLayerCount = 0;
  size_t nextAncestorMaskLayer = 0;

  size_t ancestorMaskLayerCount = aLayer->GetAncestorMaskLayerCount();
  if (Layer* ownMask = aLayer->GetMaskLayer()) {
    firstMask = ownMask;
    maskLayerCount = ancestorMaskLayerCount + 1;
    nextAncestorMaskLayer = 0;
  } else if (ancestorMaskLayerCount > 0) {
    firstMask = aLayer->GetAncestorMaskLayerAt(0);
    maskLayerCount = ancestorMaskLayerCount;
    nextAncestorMaskLayer = 1;
  } else {
    // no mask layers at all
  }

  if (maskLayerCount <= 1) {
    // This is the common case. Render in one pass and return.
    EffectChain effectChain(aLayer);
    LayerManagerComposite::AutoAddMaskEffect autoMaskEffect(firstMask,
                                                            effectChain);
    static_cast<LayerComposite*>(aLayer->AsHostLayer())
        ->AddBlendModeEffect(effectChain);
    aRenderCallback(effectChain, aClipRect);
    return;
  }

  // We have multiple mask layers.
  // We split our list of mask layers into three parts:
  //  (1) The first mask
  //  (2) The list of intermediate masks (every mask except first and last)
  //  (3) The final mask.
  // Part (2) can be empty.
  // For parts (1) and (2) we need to allocate intermediate surfaces to render
  // into. The final mask gets rendered into the original render target.

  // Calculate the size of the intermediate surfaces.
  gfx::Rect visibleRect(
      aLayer->GetLocalVisibleRegion().GetBounds().ToUnknownRect());
  gfx::Matrix4x4 transform = aLayer->GetEffectiveTransform();
  // TODO: Use RenderTargetIntRect and TransformBy here
  gfx::IntRect surfaceRect = RoundedOut(
      transform.TransformAndClipBounds(visibleRect, gfx::Rect(aClipRect)));
  if (surfaceRect.IsEmpty()) {
    return;
  }

  RefPtr<CompositingRenderTarget> originalTarget =
      aCompositor->GetCurrentRenderTarget();

  RefPtr<CompositingRenderTarget> firstTarget =
      aCompositor->CreateRenderTarget(surfaceRect, INIT_MODE_CLEAR);
  if (!firstTarget) {
    return;
  }

  // Render the source while applying the first mask.
  aCompositor->SetRenderTarget(firstTarget);
  {
    EffectChain firstEffectChain(aLayer);
    LayerManagerComposite::AutoAddMaskEffect firstMaskEffect(firstMask,
                                                             firstEffectChain);
    aRenderCallback(firstEffectChain, aClipRect - surfaceRect.TopLeft());
    // firstTarget now contains the transformed source with the first mask and
    // opacity already applied.
  }

  // Apply the intermediate masks.
  gfx::IntRect intermediateClip(surfaceRect - surfaceRect.TopLeft());
  RefPtr<CompositingRenderTarget> previousTarget = firstTarget;
  for (size_t i = nextAncestorMaskLayer; i < ancestorMaskLayerCount - 1; i++) {
    Layer* intermediateMask = aLayer->GetAncestorMaskLayerAt(i);
    RefPtr<CompositingRenderTarget> intermediateTarget =
        aCompositor->CreateRenderTarget(surfaceRect, INIT_MODE_CLEAR);
    if (!intermediateTarget) {
      break;
    }
    aCompositor->SetRenderTarget(intermediateTarget);
    EffectChain intermediateEffectChain(aLayer);
    LayerManagerComposite::AutoAddMaskEffect intermediateMaskEffect(
        intermediateMask, intermediateEffectChain);
    if (intermediateMaskEffect.Failed()) {
      continue;
    }
    intermediateEffectChain.mPrimaryEffect =
        new EffectRenderTarget(previousTarget);
    aCompositor->DrawQuad(gfx::Rect(surfaceRect), intermediateClip,
                          intermediateEffectChain, 1.0, gfx::Matrix4x4());
    previousTarget = intermediateTarget;
  }

  aCompositor->SetRenderTarget(originalTarget);

  // Apply the final mask, rendering into originalTarget.
  EffectChain finalEffectChain(aLayer);
  finalEffectChain.mPrimaryEffect = new EffectRenderTarget(previousTarget);
  Layer* finalMask = aLayer->GetAncestorMaskLayerAt(ancestorMaskLayerCount - 1);

  // The blend mode needs to be applied in this final step, because this is
  // where we're blending with the actual background (which is in
  // originalTarget).
  static_cast<LayerComposite*>(aLayer->AsHostLayer())
      ->AddBlendModeEffect(finalEffectChain);
  LayerManagerComposite::AutoAddMaskEffect autoMaskEffect(finalMask,
                                                          finalEffectChain);
  if (!autoMaskEffect.Failed()) {
    aCompositor->DrawQuad(gfx::Rect(surfaceRect), aClipRect, finalEffectChain,
                          1.0, gfx::Matrix4x4());
  }
}

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_LayerManagerComposite_H */
