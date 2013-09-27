/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LayerManagerComposite_H
#define GFX_LayerManagerComposite_H

#include <stdint.h>                     // for int32_t, uint32_t
#include "GLDefs.h"                     // for GLenum
#include "Layers.h"
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxMatrix.h"                  // for gfxMatrix
#include "gfxPoint.h"                   // for gfxIntSize
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr, TemporaryRef
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for SurfaceFormat
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersTypes.h"  // for LayersBackend, etc
#include "nsAString.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nscore.h"                     // for nsAString, etc

class gfxASurface;
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
class Composer2D;
class CompositableHost;
class Compositor;
class ContainerLayerComposite;
class EffectChain;
class ImageLayer;
class ImageLayerComposite;
class LayerComposite;
class RefLayerComposite;
class SurfaceDescriptor;
class ThebesLayerComposite;
class TiledLayerComposer;

class LayerManagerComposite : public LayerManager
{
public:
  LayerManagerComposite(Compositor* aCompositor);
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
  void BeginTransactionWithDrawTarget(gfx::DrawTarget* aTarget);

  void NotifyShadowTreeTransaction();

  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) MOZ_OVERRIDE;
  virtual void EndTransaction(DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) MOZ_OVERRIDE;

  virtual void SetRoot(Layer* aLayer) MOZ_OVERRIDE { mRoot = aLayer; }

  virtual bool CanUseCanvasLayerForSize(const gfxIntSize &aSize) MOZ_OVERRIDE;

  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() MOZ_OVERRIDE;

  virtual int32_t GetMaxTextureSize() const MOZ_OVERRIDE;

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
    return LAYERS_NONE;
  }
  virtual void GetBackendName(nsAString& name) MOZ_OVERRIDE
  {
    MOZ_ASSERT(false, "Shouldn't be called for composited layer manager");
    name.AssignLiteral("Composite");
  }

  virtual already_AddRefed<gfxASurface>
    CreateOptimalMaskSurface(const gfxIntSize &aSize) MOZ_OVERRIDE;


  DrawThebesLayerCallback GetThebesLayerCallback() const
  { return mThebesLayerCallback; }

  void* GetThebesLayerCallbackData() const
  { return mThebesLayerCallbackData; }

  /*
   * Helper functions for our layers
   */
  void CallThebesLayerDrawCallback(ThebesLayer* aLayer,
                                   gfxContext* aContext,
                                   const nsIntRegion& aRegionToDraw)
  {
    NS_ASSERTION(mThebesLayerCallback,
                 "CallThebesLayerDrawCallback without callback!");
    mThebesLayerCallback(aLayer, aContext,
                         aRegionToDraw, nsIntRegion(),
                         mThebesLayerCallbackData);
  }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() const MOZ_OVERRIDE { return ""; }
#endif // MOZ_LAYERS_HAVE_LOG

  enum WorldTransforPolicy {
    ApplyWorldTransform,
    DontApplyWorldTransform
  };

  /**
   * Setup World transform matrix.
   * Transform will be ignored if it is not PreservesAxisAlignedRectangles
   * or has non integer scale
   */
  void SetWorldTransform(const gfxMatrix& aMatrix);
  gfxMatrix& GetWorldTransform(void);

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

  private:
    CompositableHost* mCompositable;
  };

  /**
   * Creates a DrawTarget which is optimized for inter-operating with this
   * layermanager.
   */
  virtual TemporaryRef<mozilla::gfx::DrawTarget>
    CreateDrawTarget(const mozilla::gfx::IntSize &aSize,
                     mozilla::gfx::SurfaceFormat aFormat) MOZ_OVERRIDE;

  const nsIntSize& GetWidgetSize();

  /**
   * Calculates the 'completeness' of the rendering that intersected with the
   * screen on the last render. This is only useful when progressive tile
   * drawing is enabled, otherwise this will always return 1.0.
   * This function's expense scales with the size of the layer tree and the
   * complexity of individual layers' valid regions.
   */
  float ComputeRenderIntegrity();

  /**
   * Try to open |aDescriptor| for direct texturing.  If the
   * underlying surface supports direct texturing, a non-null
   * TextureImage is returned.  Otherwise null is returned.
   */
  static already_AddRefed<gl::TextureImage>
  OpenDescriptorForDirectTexturing(gl::GLContext* aContext,
                                   const SurfaceDescriptor& aDescriptor,
                                   GLenum aWrapMode);

  /**
   * returns true if PlatformAllocBuffer will return a buffer that supports
   * direct texturing
   */
  static bool SupportsDirectTexturing();

  static void PlatformSyncBeforeReplyUpdate();

  void SetCompositorID(uint32_t aID);

  Compositor* GetCompositor() const
  {
    return mCompositor;
  }

  bool PlatformDestroySharedSurface(SurfaceDescriptor* aSurface);
  RefPtr<Compositor> mCompositor;

private:
  /** Region we're clipping our current drawing to. */
  nsIntRegion mClippingRegion;
  nsIntRect mRenderBounds;

  /** Current root layer. */
  LayerComposite *RootLayer() const;

  /**
   * Recursive helper method for use by ComputeRenderIntegrity. Subtracts
   * any incomplete rendering on aLayer from aScreenRegion. Any low-precision
   * rendering is included in aLowPrecisionScreenRegion. aTransform is the
   * accumulated transform of intermediate surfaces beneath aLayer.
   */
  static void ComputeRenderIntegrityInternal(Layer* aLayer,
                                             nsIntRegion& aScreenRegion,
                                             nsIntRegion& aLowPrecisionScreenRegion,
                                             const gfx3DMatrix& aTransform);

  /**
   * Render the current layer tree to the active target.
   */
  void Render();

  /**
   * Render debug overlays such as the FPS/FrameCounter above the frame.
   */
  void RenderDebugOverlay(const gfx::Rect& aBounds);

  void WorldTransformRect(nsIntRect& aRect);

  /** Our more efficient but less powerful alter ego, if one is available. */
  nsRefPtr<Composer2D> mComposer2D;

  /* Thebes layer callbacks; valid at the end of a transaciton,
   * while rendering */
  DrawThebesLayerCallback mThebesLayerCallback;
  void *mThebesLayerCallbackData;
  gfxMatrix mWorldMatrix;
  bool mInTransaction;
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
  LayerComposite(LayerManagerComposite* aManager);

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

  virtual void RenderLayer(const nsIntPoint& aOffset,
                           const nsIntRect& aClipRect) = 0;

  virtual void SetCompositableHost(CompositableHost* aHost)
  {
    MOZ_ASSERT(false, "called SetCompositableHost for a layer without a compositable host");
  }
  virtual CompositableHost* GetCompositableHost() = 0;

  virtual void CleanupResources() = 0;

  virtual TiledLayerComposer* GetTiledLayerComposer() { return nullptr; }


  virtual void DestroyFrontBuffer() { }

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

  void SetShadowTransform(const gfx3DMatrix& aMatrix)
  {
    mShadowTransform = aMatrix;
  }
  void SetShadowTransformSetByAnimation(bool aSetByAnimation)
  {
    mShadowTransformSetByAnimation = aSetByAnimation;
  }

  // These getters can be used anytime.
  float GetShadowOpacity() { return mShadowOpacity; }
  const nsIntRect* GetShadowClipRect() { return mUseShadowClipRect ? &mShadowClipRect : nullptr; }
  const nsIntRegion& GetShadowVisibleRegion() { return mShadowVisibleRegion; }
  const gfx3DMatrix& GetShadowTransform() { return mShadowTransform; }
  bool GetShadowTransformSetByAnimation() { return mShadowTransformSetByAnimation; }

protected:
  gfx3DMatrix mShadowTransform;
  nsIntRegion mShadowVisibleRegion;
  nsIntRect mShadowClipRect;
  LayerManagerComposite* mCompositeManager;
  RefPtr<Compositor> mCompositor;
  float mShadowOpacity;
  bool mUseShadowClipRect;
  bool mShadowTransformSetByAnimation;
  bool mDestroyed;
};


} /* layers */
} /* mozilla */

#endif /* GFX_LayerManagerComposite_H */
