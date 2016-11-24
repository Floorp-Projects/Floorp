/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LayerManagerComposite_H
#define GFX_LayerManagerComposite_H

#include <stdint.h>                     // for int32_t, uint32_t
#include "GLDefs.h"                     // for GLenum
#include "Layers.h"
#include "Units.h"                      // for ParentLayerIntRect
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for override
#include "mozilla/RefPtr.h"             // for RefPtr, already_AddRefed
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for SurfaceFormat
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/Effects.h"     // for EffectChain
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/layers/LayersTypes.h"  // for LayersBackend, etc
#include "mozilla/Maybe.h"              // for Maybe
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsAString.h"
#include "mozilla/RefPtr.h"                   // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nscore.h"                     // for nsAString, etc
#include "LayerTreeInvalidation.h"

class gfxContext;

#ifdef XP_WIN
#include <windows.h>
#endif

namespace mozilla {
namespace gfx {
class DrawTarget;
} // namespace gfx

namespace layers {

class CanvasLayerComposite;
class ColorLayerComposite;
class CompositableHost;
class Compositor;
class ContainerLayerComposite;
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

static const int kVisualWarningDuration = 150; // ms

// An implementation of LayerManager that acts as a pair with ClientLayerManager
// and is mirrored across IPDL. This gets managed/updated by LayerTransactionParent.
class HostLayerManager : public LayerManager
{
public:
  HostLayerManager();
  ~HostLayerManager();

  virtual bool BeginTransactionWithTarget(gfxContext* aTarget) override
  {
    MOZ_CRASH("GFX: Use BeginTransactionWithDrawTarget");
  }

  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override
  {
    MOZ_CRASH("GFX: Use EndTransaction(aTimeStamp)");
    return false;
  }

  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) override
  {
    MOZ_CRASH("GFX: Use EndTransaction(aTimeStamp)");
  }

  virtual int32_t GetMaxTextureSize() const override
  {
    MOZ_CRASH("GFX: Call on compositor, not LayerManagerComposite");
  }

  virtual LayersBackend GetBackendType() override
  {
    MOZ_CRASH("GFX: Shouldn't be called for composited layer manager");
  }
  virtual void GetBackendName(nsAString& name) override
  {
    MOZ_CRASH("GFX: Shouldn't be called for composited layer manager");
  }
  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() = 0;


  virtual void ForcePresent() = 0;
  virtual void AddInvalidRegion(const nsIntRegion& aRegion) = 0;
  virtual void ClearApproximatelyVisibleRegions(uint64_t aLayersId,
                                                const Maybe<uint32_t>& aPresShellId) = 0;
  virtual void UpdateApproximatelyVisibleRegion(const ScrollableLayerGuid& aGuid,
                                                const CSSIntRegion& aRegion) = 0;

  virtual void NotifyShadowTreeTransaction() {}
  virtual void BeginTransactionWithDrawTarget(gfx::DrawTarget* aTarget,
                                              const gfx::IntRect& aRect) = 0;
  virtual Compositor* GetCompositor() const = 0;
  virtual void EndTransaction(const TimeStamp& aTimeStamp,
                              EndTransactionFlags aFlags = END_DEFAULT) = 0;
  virtual void UpdateRenderBounds(const gfx::IntRect& aRect) {}

  // Called by CompositorBridgeParent when a new compositor has been created due
  // to a device reset. The layer manager must clear any cached resources
  // attached to the old compositor, and make a best effort at ignoring
  // layer or texture updates against the old compositor.
  virtual void ChangeCompositor(Compositor* aNewCompositor) = 0;

  void ExtractImageCompositeNotifications(nsTArray<ImageCompositeNotification>* aNotifications)
  {
    aNotifications->AppendElements(Move(mImageCompositeNotifications));
  }

  /**
   * LayerManagerComposite provides sophisticated debug overlays
   * that can request a next frame.
   */
  bool DebugOverlayWantsNextFrame() { return mDebugOverlayWantsNextFrame; }
  void SetDebugOverlayWantsNextFrame(bool aVal)
  { mDebugOverlayWantsNextFrame = aVal; }

  /**
   * Add an on frame warning.
   * @param severity ranges from 0 to 1. It's used to compute the warning color.
   */
  void VisualFrameWarning(float severity) {
    mozilla::TimeStamp now = TimeStamp::Now();
    if (mWarnTime.IsNull() ||
        severity > mWarningLevel ||
        mWarnTime + TimeDuration::FromMilliseconds(kVisualWarningDuration) < now) {
      mWarnTime = now;
      mWarningLevel = severity;
    }
  }

  // Indicate that we need to composite even if nothing in our layers has
  // changed, so that the widget can draw something different in its window
  // overlay.
  void SetWindowOverlayChanged() { mWindowOverlayChanged = true; }


  void SetPaintTime(const TimeDuration& aPaintTime) { mLastPaintTime = aPaintTime; }

protected:
  bool mDebugOverlayWantsNextFrame;
  nsTArray<ImageCompositeNotification> mImageCompositeNotifications;
  // Testing property. If hardware composer is supported, this will return
  // true if the last frame was deemed 'too complicated' to be rendered.
  float mWarningLevel;
  mozilla::TimeStamp mWarnTime;

  bool mWindowOverlayChanged;
  RefPtr<PaintCounter> mPaintCounter;
  TimeDuration mLastPaintTime;
  TimeStamp mRenderStartTime;
};

// A layer manager implementation that uses the Compositor API
// to render layers.
class LayerManagerComposite final : public HostLayerManager
{
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::IntSize IntSize;
  typedef mozilla::gfx::SurfaceFormat SurfaceFormat;

public:
  explicit LayerManagerComposite(Compositor* aCompositor);
  ~LayerManagerComposite();

  virtual void Destroy() override;

  /**
   * Sets the clipping region for this layer manager. This is important on
   * windows because using OGL we no longer have GDI's native clipping. Therefor
   * widget must tell us what part of the screen is being invalidated,
   * and we should clip to this.
   *
   * \param aClippingRegion Region to clip to. Setting an empty region
   * will disable clipping.
   */
  void SetClippingRegion(const nsIntRegion& aClippingRegion)
  {
    mClippingRegion = aClippingRegion;
  }

  /**
   * LayerManager implementation.
   */
  virtual LayerManagerComposite* AsLayerManagerComposite() override
  {
    return this;
  }

  void UpdateRenderBounds(const gfx::IntRect& aRect) override;

  virtual bool BeginTransaction() override;
  void BeginTransactionWithDrawTarget(gfx::DrawTarget* aTarget,
                                      const gfx::IntRect& aRect) override;
  void EndTransaction(const TimeStamp& aTimeStamp,
                      EndTransactionFlags aFlags = END_DEFAULT) override;
  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) override
  {
    MOZ_CRASH("GFX: Use EndTransaction(aTimeStamp)");
  }

  virtual void SetRoot(Layer* aLayer) override { mRoot = aLayer; }

  // XXX[nrc]: never called, we should move this logic to ClientLayerManager
  // (bug 946926).
  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize &aSize) override;

  virtual void ClearCachedResources(Layer* aSubtree = nullptr) override;

  virtual already_AddRefed<PaintedLayer> CreatePaintedLayer() override;
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer() override;
  virtual already_AddRefed<ImageLayer> CreateImageLayer() override;
  virtual already_AddRefed<ColorLayer> CreateColorLayer() override;
  virtual already_AddRefed<TextLayer> CreateTextLayer() override;
  virtual already_AddRefed<BorderLayer> CreateBorderLayer() override;
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer() override;
  virtual already_AddRefed<RefLayer> CreateRefLayer() override;

  virtual bool AreComponentAlphaLayersEnabled() override;

  virtual already_AddRefed<DrawTarget>
    CreateOptimalMaskDrawTarget(const IntSize &aSize) override;

  virtual const char* Name() const override { return ""; }

  /**
   * Post-processes layers before composition. This performs the following:
   *
   *   - Applies occlusion culling. This restricts the shadow visible region
   *     of layers that are covered with opaque content.
   *     |aOpaqueRegion| is the region already known to be covered with opaque
   *     content, in the post-transform coordinate space of aLayer.
   *
   *   - Recomputes visible regions to account for async transforms.
   *     Each layer accumulates into |aVisibleRegion| its post-transform
   *     (including async transforms) visible region.
   */
  void PostProcessLayers(Layer* aLayer,
                         nsIntRegion& aOpaqueRegion,
                         LayerIntRegion& aVisibleRegion,
                         const Maybe<ParentLayerIntRect>& aClipFromAncestors);

  /**
   * RAII helper class to add a mask effect with the compositable from aMaskLayer
   * to the EffectChain aEffect and notify the compositable when we are done.
   */
  class AutoAddMaskEffect
  {
  public:
    AutoAddMaskEffect(Layer* aMaskLayer,
                      EffectChain& aEffect);
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

  void AddInvalidRegion(const nsIntRegion& aRegion) override
  {
    mInvalidRegion.Or(mInvalidRegion, aRegion);
  }

  void ClearApproximatelyVisibleRegions(uint64_t aLayersId,
                                        const Maybe<uint32_t>& aPresShellId) override
  {
    for (auto iter = mVisibleRegions.Iter(); !iter.Done(); iter.Next()) {
      if (iter.Key().mLayersId == aLayersId &&
          (!aPresShellId || iter.Key().mPresShellId == *aPresShellId)) {
        iter.Remove();
      }
    }
  }

  void UpdateApproximatelyVisibleRegion(const ScrollableLayerGuid& aGuid,
                                        const CSSIntRegion& aRegion) override
  {
    CSSIntRegion* regionForScrollFrame = mVisibleRegions.LookupOrAdd(aGuid);
    MOZ_ASSERT(regionForScrollFrame);

    *regionForScrollFrame = aRegion;
  }

  CSSIntRegion* GetApproximatelyVisibleRegion(const ScrollableLayerGuid& aGuid)
  {
    return mVisibleRegions.Get(aGuid);
  }

  Compositor* GetCompositor() const override
  {
    return mCompositor;
  }

  // Called by CompositorBridgeParent when a new compositor has been created due
  // to a device reset. The layer manager must clear any cached resources
  // attached to the old compositor, and make a best effort at ignoring
  // layer or texture updates against the old compositor.
  void ChangeCompositor(Compositor* aNewCompositor) override;

  void NotifyShadowTreeTransaction() override;

  TextRenderer* GetTextRenderer() { return mTextRenderer; }

  void UnusedApzTransformWarning() {
    mUnusedApzTransformWarning = true;
  }
  void DisabledApzWarning() {
    mDisabledApzWarning = true;
  }

  bool AsyncPanZoomEnabled() const override;

  void AppendImageCompositeNotification(const ImageCompositeNotification& aNotification)
  {
    // Only send composite notifications when we're drawing to the screen,
    // because that's what they mean.
    // Also when we're not drawing to the screen, DidComposite will not be
    // called to extract and send these notifications, so they might linger
    // and contain stale ImageContainerParent pointers.
    if (!mCompositor->GetTargetContext()) {
      mImageCompositeNotifications.AppendElement(aNotification);
    }
  }
  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() override
  {
    return mCompositor->GetTextureFactoryIdentifier();
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
   */
  void Render(const nsIntRegion& aInvalidRegion, const nsIntRegion& aOpaqueRegion);
#if defined(MOZ_WIDGET_ANDROID)
  void RenderToPresentationSurface();
#endif

  /**
   * Render paint and composite times above the frame.
   */
  void DrawPaintTimes(Compositor* aCompositor);

  /**
   * We need to know our invalid region before we're ready to render.
   */
  void InvalidateDebugOverlay(nsIntRegion& aInvalidRegion, const gfx::IntRect& aBounds);

  /**
   * Render debug overlays such as the FPS/FrameCounter above the frame.
   */
  void RenderDebugOverlay(const gfx::IntRect& aBounds);


  RefPtr<CompositingRenderTarget> PushGroupForLayerEffects();
  void PopGroupForLayerEffects(RefPtr<CompositingRenderTarget> aPreviousTarget,
                               gfx::IntRect aClipRect,
                               bool aGrayscaleEffect,
                               bool aInvertEffect,
                               float aContrastEffect);

  void ChangeCompositorInternal(Compositor* aNewCompositor);

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

  typedef nsClassHashtable<nsGenericHashKey<ScrollableLayerGuid>,
                           CSSIntRegion> VisibleRegions;
  VisibleRegions mVisibleRegions;

  UniquePtr<FPSState> mFPS;

  bool mInTransaction;
  bool mIsCompositorReady;

  RefPtr<CompositingRenderTarget> mTwoPassTmpTarget;
  RefPtr<TextRenderer> mTextRenderer;
  bool mGeometryChanged;
};

/**
 * Compositor layers are for use with OMTC on the compositor thread only. There
 * must be corresponding Client layers on the content thread. For composite
 * layers, the layer manager only maintains the layer tree.
 */
class HostLayer
{
public:
  explicit HostLayer(HostLayerManager* aManager)
    : mCompositorManager(aManager)
    , mShadowOpacity(1.0)
    , mShadowTransformSetByAnimation(false)
    , mShadowOpacitySetByAnimation(false)
  {
  }

  virtual void SetLayerManager(HostLayerManager* aManager)
  {
    mCompositorManager = aManager;
  }
  HostLayerManager* GetLayerManager() const { return mCompositorManager; }


  virtual ~HostLayer() {}

  virtual LayerComposite* GetFirstChildComposite()
  {
    return nullptr;
  }

  virtual Layer* GetLayer() = 0;

  virtual bool SetCompositableHost(CompositableHost*)
  {
    // We must handle this gracefully, see bug 967824
    NS_WARNING("called SetCompositableHost for a layer type not accepting a compositable");
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
  void SetShadowVisibleRegion(const LayerIntRegion& aRegion)
  {
    mShadowVisibleRegion = aRegion;
  }

  void SetShadowOpacity(float aOpacity)
  {
    mShadowOpacity = aOpacity;
  }
  void SetShadowOpacitySetByAnimation(bool aSetByAnimation)
  {
    mShadowOpacitySetByAnimation = aSetByAnimation;
  }

  void SetShadowClipRect(const Maybe<ParentLayerIntRect>& aRect)
  {
    mShadowClipRect = aRect;
  }

  void SetShadowBaseTransform(const gfx::Matrix4x4& aMatrix)
  {
    mShadowTransform = aMatrix;
  }
  void SetShadowTransformSetByAnimation(bool aSetByAnimation)
  {
    mShadowTransformSetByAnimation = aSetByAnimation;
  }

  // These getters can be used anytime.
  float GetShadowOpacity() { return mShadowOpacity; }
  const Maybe<ParentLayerIntRect>& GetShadowClipRect() { return mShadowClipRect; }
  const LayerIntRegion& GetShadowVisibleRegion() { return mShadowVisibleRegion; }
  const gfx::Matrix4x4& GetShadowBaseTransform() { return mShadowTransform; }
  gfx::Matrix4x4 GetShadowTransform();
  bool GetShadowTransformSetByAnimation() { return mShadowTransformSetByAnimation; }
  bool GetShadowOpacitySetByAnimation() { return mShadowOpacitySetByAnimation; }
  
  /**
   * Return true if a checkerboarding background color needs to be drawn
   * for this layer.
   */
  virtual bool NeedToDrawCheckerboarding(gfx::Color* aOutCheckerboardingColor = nullptr) { return false; }

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
class LayerComposite : public HostLayer
{
public:
  explicit LayerComposite(LayerManagerComposite* aManager);

  virtual ~LayerComposite();

  virtual void SetLayerManager(HostLayerManager* aManager);

  virtual LayerComposite* GetFirstChildComposite()
  {
    return nullptr;
  }

  /* Do NOT call this from the generic LayerComposite destructor.  Only from the
   * concrete class destructor
   */
  virtual void Destroy();

  /**
   * Perform a first pass over the layer tree to render all of the intermediate
   * surfaces that we can. This allows us to avoid framebuffer switches in the
   * middle of our render which is inefficient especially on mobile GPUs. This
   * must be called before RenderLayer.
   */
  virtual void Prepare(const RenderTargetIntRect& aClipRect) {}

  // TODO: This should also take RenderTargetIntRect like Prepare.
  virtual void RenderLayer(const gfx::IntRect& aClipRect) = 0;

  virtual bool SetCompositableHost(CompositableHost*)
  {
    // We must handle this gracefully, see bug 967824
    NS_WARNING("called SetCompositableHost for a layer type not accepting a compositable");
    return false;
  }

  virtual void CleanupResources() = 0;

  virtual void DestroyFrontBuffer() { }

  void AddBlendModeEffect(EffectChain& aEffectChain);

  virtual void GenEffectChain(EffectChain& aEffect) { }

  void SetLayerComposited(bool value)
  {
    mLayerComposited = value;
  }

  void SetClearRect(const gfx::IntRect& aRect)
  {
    mClearRect = aRect;
  }

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

  virtual bool NeedToDrawCheckerboarding(gfx::Color* aOutCheckerboardingColor = nullptr);

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
template<typename RenderCallbackType>
void
RenderWithAllMasks(Layer* aLayer, Compositor* aCompositor,
                   const gfx::IntRect& aClipRect,
                   RenderCallbackType aRenderCallback)
{
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
    LayerManagerComposite::AutoAddMaskEffect
      autoMaskEffect(firstMask, effectChain);
    static_cast<LayerComposite*>(aLayer->AsHostLayer())->AddBlendModeEffect(effectChain);
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
  gfx::Rect visibleRect(aLayer->GetLocalVisibleRegion().ToUnknownRegion().GetBounds());
  gfx::Matrix4x4 transform = aLayer->GetEffectiveTransform();
  // TODO: Use RenderTargetIntRect and TransformBy here
  gfx::IntRect surfaceRect =
    RoundedOut(transform.TransformAndClipBounds(visibleRect, gfx::Rect(aClipRect)));
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
    LayerManagerComposite::AutoAddMaskEffect
      firstMaskEffect(firstMask, firstEffectChain);
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
    LayerManagerComposite::AutoAddMaskEffect
      intermediateMaskEffect(intermediateMask, intermediateEffectChain);
    if (intermediateMaskEffect.Failed()) {
      continue;
    }
    intermediateEffectChain.mPrimaryEffect = new EffectRenderTarget(previousTarget);
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
  // where we're blending with the actual background (which is in originalTarget).
  static_cast<LayerComposite*>(aLayer->AsHostLayer())->AddBlendModeEffect(finalEffectChain);
  LayerManagerComposite::AutoAddMaskEffect autoMaskEffect(finalMask, finalEffectChain);
  if (!autoMaskEffect.Failed()) {
    aCompositor->DrawQuad(gfx::Rect(surfaceRect), aClipRect,
                          finalEffectChain, 1.0, gfx::Matrix4x4());
  }
}

} // namespace layers
} // namespace mozilla

#endif /* GFX_LayerManagerComposite_H */
