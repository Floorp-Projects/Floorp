/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LayerManagerComposite_H
#define GFX_LayerManagerComposite_H

#include <stdint.h>                     // for int32_t, uint32_t
#include "GLDefs.h"                     // for GLenum
#include "Layers.h"
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr, TemporaryRef
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for SurfaceFormat
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersTypes.h"  // for LayersBackend, etc
#include "mozilla/RefPtr.h"
#include "nsAString.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nscore.h"                     // for nsAString, etc
#include "LayerTreeInvalidation.h"

class gfxContext;
struct nsIntPoint;
struct nsIntSize;

#ifdef XP_WIN
#include <windows.h>
#endif

namespace mozilla {
namespace gfx {
class DrawTarget;
}

namespace gl {
class GLContext;
class TextureImage;
}

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
class SurfaceDescriptor;
class ThebesLayerComposite;
class TiledLayerComposer;
class TextRenderer;
class CompositingRenderTarget;
struct FPSState;

class LayerManagerComposite MOZ_FINAL : public LayerManager
{
  typedef mozilla::gfx::DrawTarget DrawTarget;
  typedef mozilla::gfx::IntSize IntSize;
  typedef mozilla::gfx::SurfaceFormat SurfaceFormat;

public:
  explicit LayerManagerComposite(Compositor* aCompositor);
  ~LayerManagerComposite();

  virtual void Destroy() MOZ_OVERRIDE;

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
  virtual LayerManagerComposite* AsLayerManagerComposite() MOZ_OVERRIDE
  {
    return this;
  }

  void UpdateRenderBounds(const nsIntRect& aRect);

  virtual void BeginTransaction() MOZ_OVERRIDE;
  virtual void BeginTransactionWithTarget(gfxContext* aTarget) MOZ_OVERRIDE
  {
    MOZ_CRASH("Use BeginTransactionWithDrawTarget");
  }
  void BeginTransactionWithDrawTarget(gfx::DrawTarget* aTarget, const nsIntRect& aRect);

  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) MOZ_OVERRIDE;
  virtual void EndTransaction(DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) MOZ_OVERRIDE;

  virtual void SetRoot(Layer* aLayer) MOZ_OVERRIDE { mRoot = aLayer; }

  // XXX[nrc]: never called, we should move this logic to ClientLayerManager
  // (bug 946926).
  virtual bool CanUseCanvasLayerForSize(const gfx::IntSize &aSize) MOZ_OVERRIDE;

  virtual int32_t GetMaxTextureSize() const MOZ_OVERRIDE
  {
    MOZ_CRASH("Call on compositor, not LayerManagerComposite");
  }

  virtual void ClearCachedResources(Layer* aSubtree = nullptr) MOZ_OVERRIDE;

  virtual already_AddRefed<ThebesLayer> CreateThebesLayer() MOZ_OVERRIDE;
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer() MOZ_OVERRIDE;
  virtual already_AddRefed<ImageLayer> CreateImageLayer() MOZ_OVERRIDE;
  virtual already_AddRefed<ColorLayer> CreateColorLayer() MOZ_OVERRIDE;
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer() MOZ_OVERRIDE;
  already_AddRefed<ThebesLayerComposite> CreateThebesLayerComposite();
  already_AddRefed<ContainerLayerComposite> CreateContainerLayerComposite();
  already_AddRefed<ImageLayerComposite> CreateImageLayerComposite();
  already_AddRefed<ColorLayerComposite> CreateColorLayerComposite();
  already_AddRefed<CanvasLayerComposite> CreateCanvasLayerComposite();
  already_AddRefed<RefLayerComposite> CreateRefLayerComposite();

  virtual LayersBackend GetBackendType() MOZ_OVERRIDE
  {
    MOZ_CRASH("Shouldn't be called for composited layer manager");
  }
  virtual void GetBackendName(nsAString& name) MOZ_OVERRIDE
  {
    MOZ_CRASH("Shouldn't be called for composited layer manager");
  }

  virtual bool AreComponentAlphaLayersEnabled() MOZ_OVERRIDE;

  virtual TemporaryRef<DrawTarget>
    CreateOptimalMaskDrawTarget(const IntSize &aSize) MOZ_OVERRIDE;

  virtual const char* Name() const MOZ_OVERRIDE { return ""; }

  enum WorldTransforPolicy {
    ApplyWorldTransform,
    DontApplyWorldTransform
  };

  /**
   * Setup World transform matrix.
   * Transform will be ignored if it is not PreservesAxisAlignedRectangles
   * or has non integer scale
   */
  void SetWorldTransform(const gfx::Matrix& aMatrix);
  gfx::Matrix& GetWorldTransform(void);

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
  virtual TemporaryRef<mozilla::gfx::DrawTarget>
    CreateDrawTarget(const mozilla::gfx::IntSize& aSize,
                     mozilla::gfx::SurfaceFormat aFormat) MOZ_OVERRIDE;

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

private:
  /** Region we're clipping our current drawing to. */
  nsIntRegion mClippingRegion;
  nsIntRect mRenderBounds;

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
   * Render the current layer tree to the active target.
   */
  void Render();

  /**
   * Render debug overlays such as the FPS/FrameCounter above the frame.
   */
  void RenderDebugOverlay(const gfx::Rect& aBounds);

  void WorldTransformRect(nsIntRect& aRect);

  RefPtr<CompositingRenderTarget> PushGroup();
  void PopGroup(RefPtr<CompositingRenderTarget> aPreviousTarget, nsIntRect aClipRect);

  RefPtr<Compositor> mCompositor;
  nsAutoPtr<LayerProperties> mClonedLayerTreeProperties;

  /**
   * Context target, nullptr when drawing directly to our swap chain.
   */
  RefPtr<gfx::DrawTarget> mTarget;
  nsIntRect mTargetBounds;

  gfx::Matrix mWorldMatrix;
  nsIntRegion mInvalidRegion;
  nsAutoPtr<FPSState> mFPS;

  bool mInTransaction;
  bool mIsCompositorReady;
  bool mDebugOverlayWantsNextFrame;

  RefPtr<CompositingRenderTarget> mTwoPassTmpTarget;
  RefPtr<TextRenderer> mTextRenderer;
  bool mGeometryChanged;
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

  /**
   * Perform a first pass over the layer tree to prepare intermediate surfaces.
   * This allows us on to avoid framebuffer switches in the middle of our render
   * which is inefficient. This must be called before RenderLayer.
   */
  virtual void Prepare(const RenderTargetIntRect& aClipRect) {}

  // TODO: This should also take RenderTargetIntRect like Prepare.
  virtual void RenderLayer(const nsIntRect& aClipRect) = 0;

  virtual bool SetCompositableHost(CompositableHost*)
  {
    // We must handle this gracefully, see bug 967824
    NS_WARNING("called SetCompositableHost for a layer type not accepting a compositable");
    return false;
  }
  virtual CompositableHost* GetCompositableHost() = 0;

  virtual void CleanupResources() = 0;

  virtual TiledLayerComposer* GetTiledLayerComposer() { return nullptr; }

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
  void SetShadowVisibleRegion(const nsIntRegion& aRegion)
  {
    mShadowVisibleRegion = aRegion;
  }

  void SetShadowOpacity(float aOpacity)
  {
    mShadowOpacity = aOpacity;
  }

  void SetShadowClipRect(const nsIntRect* aRect)
  {
    mUseShadowClipRect = aRect != nullptr;
    if (aRect) {
      mShadowClipRect = *aRect;
    }
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

  void SetClearRect(const nsIntRect& aRect)
  {
    mClearRect = aRect;
  }

  // These getters can be used anytime.
  float GetShadowOpacity() { return mShadowOpacity; }
  const nsIntRect* GetShadowClipRect() { return mUseShadowClipRect ? &mShadowClipRect : nullptr; }
  const nsIntRegion& GetShadowVisibleRegion() { return mShadowVisibleRegion; }
  const gfx::Matrix4x4& GetShadowTransform() { return mShadowTransform; }
  bool GetShadowTransformSetByAnimation() { return mShadowTransformSetByAnimation; }
  bool HasLayerBeenComposited() { return mLayerComposited; }
  nsIntRect GetClearRect() { return mClearRect; }

protected:
  gfx::Matrix4x4 mShadowTransform;
  nsIntRegion mShadowVisibleRegion;
  nsIntRect mShadowClipRect;
  LayerManagerComposite* mCompositeManager;
  RefPtr<Compositor> mCompositor;
  float mShadowOpacity;
  bool mUseShadowClipRect;
  bool mShadowTransformSetByAnimation;
  bool mDestroyed;
  bool mLayerComposited;
  nsIntRect mClearRect;
};


} /* layers */
} /* mozilla */

#endif /* GFX_LayerManagerComposite_H */
