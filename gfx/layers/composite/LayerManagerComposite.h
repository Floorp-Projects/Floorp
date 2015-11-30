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

static const int kVisualWarningDuration = 150; // ms

class LayerManagerComposite final : public LayerManager
{
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::IntSize IntSize;
  typedef mozilla::gfx::SurfaceFormat SurfaceFormat;

public:
  explicit LayerManagerComposite(Compositor* aCompositor);
  ~LayerManagerComposite();

  virtual void Destroy() override;

  /**
   * return True if initialization was succesful, false when it was not.
   */
  bool Initialize();

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

  void UpdateRenderBounds(const gfx::IntRect& aRect);

  virtual void BeginTransaction() override;
  virtual void BeginTransactionWithTarget(gfxContext* aTarget) override
  {
    MOZ_CRASH("Use BeginTransactionWithDrawTarget");
  }
  void BeginTransactionWithDrawTarget(gfx::DrawTarget* aTarget,
                                      const gfx::IntRect& aRect);

  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override
  {
    MOZ_CRASH("Use EndTransaction(aTimeStamp)");
    return false;
  }
  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) override
  {
    MOZ_CRASH("Use EndTransaction(aTimeStamp)");
  }
  void EndTransaction(const TimeStamp& aTimeStamp,
                      EndTransactionFlags aFlags = END_DEFAULT);

  virtual void SetRoot(Layer* aLayer) override { mRoot = aLayer; }

  // XXX[nrc]: never called, we should move this logic to ClientLayerManager
  // (bug 946926).
  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize &aSize) override;

  virtual int32_t GetMaxTextureSize() const override
  {
    MOZ_CRASH("Call on compositor, not LayerManagerComposite");
  }

  virtual void ClearCachedResources(Layer* aSubtree = nullptr) override;

  virtual already_AddRefed<PaintedLayer> CreatePaintedLayer() override;
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer() override;
  virtual already_AddRefed<ImageLayer> CreateImageLayer() override;
  virtual already_AddRefed<ColorLayer> CreateColorLayer() override;
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer() override;
  already_AddRefed<PaintedLayerComposite> CreatePaintedLayerComposite();
  already_AddRefed<ContainerLayerComposite> CreateContainerLayerComposite();
  already_AddRefed<ImageLayerComposite> CreateImageLayerComposite();
  already_AddRefed<ColorLayerComposite> CreateColorLayerComposite();
  already_AddRefed<CanvasLayerComposite> CreateCanvasLayerComposite();
  already_AddRefed<RefLayerComposite> CreateRefLayerComposite();

  virtual LayersBackend GetBackendType() override
  {
    MOZ_CRASH("Shouldn't be called for composited layer manager");
  }
  virtual void GetBackendName(nsAString& name) override
  {
    MOZ_CRASH("Shouldn't be called for composited layer manager");
  }

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
                         LayerIntRegion& aVisibleRegion);

  /**
   * RAII helper class to add a mask effect with the compositable from aMaskLayer
   * to the EffectChain aEffect and notify the compositable when we are done.
   */
  class AutoAddMaskEffect
  {
  public:
    AutoAddMaskEffect(Layer* aMaskLayer,
                      EffectChain& aEffect,
                      bool aIs3D = false);
    ~AutoAddMaskEffect();

    bool Failed() const { return mFailed; }
  private:
    CompositableHost* mCompositable;
    bool mFailed;
  };

  /**
   * Creates a DrawTarget which is optimized for inter-operating with this
   * layermanager.
   */
  virtual already_AddRefed<mozilla::gfx::DrawTarget>
    CreateDrawTarget(const mozilla::gfx::IntSize& aSize,
                     mozilla::gfx::SurfaceFormat aFormat) override;

  /**
   * Calculates the 'completeness' of the rendering that intersected with the
   * screen on the last render. This is only useful when progressive tile
   * drawing is enabled, otherwise this will always return 1.0.
   * This function's expense scales with the size of the layer tree and the
   * complexity of individual layers' valid regions.
   */
  float ComputeRenderIntegrity();

  /**
   * returns true if PlatformAllocBuffer will return a buffer that supports
   * direct texturing
   */
  static bool SupportsDirectTexturing();

  static void PlatformSyncBeforeReplyUpdate();

  void AddInvalidRegion(const nsIntRegion& aRegion)
  {
    mInvalidRegion.Or(mInvalidRegion, aRegion);
  }

  Compositor* GetCompositor() const
  {
    return mCompositor;
  }

  /**
   * LayerManagerComposite provides sophisticated debug overlays
   * that can request a next frame.
   */
  bool DebugOverlayWantsNextFrame() { return mDebugOverlayWantsNextFrame; }
  void SetDebugOverlayWantsNextFrame(bool aVal)
  { mDebugOverlayWantsNextFrame = aVal; }

  void NotifyShadowTreeTransaction();

  TextRenderer* GetTextRenderer() { return mTextRenderer; }

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

  void UnusedApzTransformWarning() {
    mUnusedApzTransformWarning = true;
  }

  bool LastFrameMissedHWC() { return mLastFrameMissedHWC; }

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
  void ExtractImageCompositeNotifications(nsTArray<ImageCompositeNotification>* aNotifications)
  {
    aNotifications->AppendElements(Move(mImageCompositeNotifications));
  }

  // Indicate that we need to composite even if nothing in our layers has
  // changed, so that the widget can draw something different in its window
  // overlay.
  void SetWindowOverlayChanged() { mWindowOverlayChanged = true; }

private:
  /** Region we're clipping our current drawing to. */
  nsIntRegion mClippingRegion;
  gfx::IntRect mRenderBounds;

  /** Current root layer. */
  LayerComposite* RootLayer() const;

  /**
   * Recursive helper method for use by ComputeRenderIntegrity. Subtracts
   * any incomplete rendering on aLayer from aScreenRegion. Any low-precision
   * rendering is included in aLowPrecisionScreenRegion. aTransform is the
   * accumulated transform of intermediate surfaces beneath aLayer.
   */
  static void ComputeRenderIntegrityInternal(Layer* aLayer,
                                             nsIntRegion& aScreenRegion,
                                             nsIntRegion& aLowPrecisionScreenRegion,
                                             const gfx::Matrix4x4& aTransform);

  /**
   * Update the invalid region and render it.
   */
  void UpdateAndRender();

  /**
   * Render the current layer tree to the active target.
   */
  void Render(const nsIntRegion& aInvalidRegion);
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
  void RenderToPresentationSurface();
#endif

  /**
   * We need to know our invalid region before we're ready to render.
   */
  void InvalidateDebugOverlay(const gfx::IntRect& aBounds);

  /**
   * Render debug overlays such as the FPS/FrameCounter above the frame.
   */
  void RenderDebugOverlay(const gfx::Rect& aBounds);


  RefPtr<CompositingRenderTarget> PushGroupForLayerEffects();
  void PopGroupForLayerEffects(RefPtr<CompositingRenderTarget> aPreviousTarget,
                               gfx::IntRect aClipRect,
                               bool aGrayscaleEffect,
                               bool aInvertEffect,
                               float aContrastEffect);

  float mWarningLevel;
  mozilla::TimeStamp mWarnTime;
  bool mUnusedApzTransformWarning;
  RefPtr<Compositor> mCompositor;
  UniquePtr<LayerProperties> mClonedLayerTreeProperties;

  nsTArray<ImageCompositeNotification> mImageCompositeNotifications;

  /**
   * Context target, nullptr when drawing directly to our swap chain.
   */
  RefPtr<gfx::DrawTarget> mTarget;
  gfx::IntRect mTargetBounds;

  nsIntRegion mInvalidRegion;
  UniquePtr<FPSState> mFPS;

  bool mInTransaction;
  bool mIsCompositorReady;
  bool mDebugOverlayWantsNextFrame;

  RefPtr<CompositingRenderTarget> mTwoPassTmpTarget;
  RefPtr<TextRenderer> mTextRenderer;
  bool mGeometryChanged;

  // Testing property. If hardware composer is supported, this will return
  // true if the last frame was deemed 'too complicated' to be rendered.
  bool mLastFrameMissedHWC;

  bool mWindowOverlayChanged;
};

/**
 * Composite layers are for use with OMTC on the compositor thread only. There
 * must be corresponding Basic layers on the content thread. For composite
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
class LayerComposite
{
public:
  explicit LayerComposite(LayerManagerComposite* aManager);

  virtual ~LayerComposite();

  virtual LayerComposite* GetFirstChildComposite()
  {
    return nullptr;
  }

  /* Do NOT call this from the generic LayerComposite destructor.  Only from the
   * concrete class destructor
   */
  virtual void Destroy();

  virtual Layer* GetLayer() = 0;

  virtual void SetLayerManager(LayerManagerComposite* aManager);

  LayerManagerComposite* GetLayerManager() const { return mCompositeManager; }

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
  virtual CompositableHost* GetCompositableHost() = 0;

  virtual void CleanupResources() = 0;

  virtual void DestroyFrontBuffer() { }

  void AddBlendModeEffect(EffectChain& aEffectChain);

  virtual void GenEffectChain(EffectChain& aEffect) { }

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

  void SetShadowClipRect(const Maybe<ParentLayerIntRect>& aRect)
  {
    mShadowClipRect = aRect;
  }

  void SetShadowTransform(const gfx::Matrix4x4& aMatrix)
  {
    mShadowTransform = aMatrix;
  }
  void SetShadowTransformSetByAnimation(bool aSetByAnimation)
  {
    mShadowTransformSetByAnimation = aSetByAnimation;
  }

  void SetLayerComposited(bool value)
  {
    mLayerComposited = value;
  }

  void SetClearRect(const gfx::IntRect& aRect)
  {
    mClearRect = aRect;
  }

  // These getters can be used anytime.
  float GetShadowOpacity() { return mShadowOpacity; }
  const Maybe<ParentLayerIntRect>& GetShadowClipRect() { return mShadowClipRect; }
  const LayerIntRegion& GetShadowVisibleRegion() { return mShadowVisibleRegion; }
  const gfx::Matrix4x4& GetShadowTransform() { return mShadowTransform; }
  bool GetShadowTransformSetByAnimation() { return mShadowTransformSetByAnimation; }
  bool HasLayerBeenComposited() { return mLayerComposited; }
  gfx::IntRect GetClearRect() { return mClearRect; }

  /**
   * Return the part of the visible region that has been fully rendered.
   * While progressive drawing is in progress this region will be
   * a subset of the shadow visible region.
   */
  nsIntRegion GetFullyRenderedRegion();

protected:
  gfx::Matrix4x4 mShadowTransform;
  LayerIntRegion mShadowVisibleRegion;
  Maybe<ParentLayerIntRect> mShadowClipRect;
  LayerManagerComposite* mCompositeManager;
  RefPtr<Compositor> mCompositor;
  float mShadowOpacity;
  bool mShadowTransformSetByAnimation;
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
// This function calls aLayer->AsLayerComposite()->AddBlendModeEffect for the
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

  bool firstMaskIs3D = false;
  if (ContainerLayer* container = aLayer->AsContainerLayer()) {
    firstMaskIs3D = !container->GetTransform().CanDraw2D();
  }

  if (maskLayerCount <= 1) {
    // This is the common case. Render in one pass and return.
    EffectChain effectChain(aLayer);
    LayerManagerComposite::AutoAddMaskEffect
      autoMaskEffect(firstMask, effectChain, firstMaskIs3D);
    aLayer->AsLayerComposite()->AddBlendModeEffect(effectChain);
    aRenderCallback(effectChain, gfx::Rect(aClipRect));
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
  gfx::Rect visibleRect(aLayer->GetEffectiveVisibleRegion().ToUnknownRegion().GetBounds());
  gfx::Matrix4x4 transform = aLayer->GetEffectiveTransform();
  // TODO: Use RenderTargetIntRect and TransformTo<...> here
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
      firstMaskEffect(firstMask, firstEffectChain, firstMaskIs3D);
    aRenderCallback(firstEffectChain, gfx::Rect(aClipRect - surfaceRect.TopLeft()));
    // firstTarget now contains the transformed source with the first mask and
    // opacity already applied.
  }

  // Apply the intermediate masks.
  gfx::Rect intermediateClip(surfaceRect - surfaceRect.TopLeft());
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
  aLayer->AsLayerComposite()->AddBlendModeEffect(finalEffectChain);
  LayerManagerComposite::AutoAddMaskEffect autoMaskEffect(finalMask, finalEffectChain);
  if (!autoMaskEffect.Failed()) {
    aCompositor->DrawQuad(gfx::Rect(surfaceRect), gfx::Rect(aClipRect),
                          finalEffectChain, 1.0, gfx::Matrix4x4());
  }
}

} // namespace layers
} // namespace mozilla

#endif /* GFX_LayerManagerComposite_H */
