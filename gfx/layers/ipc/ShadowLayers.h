/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ShadowLayers_h
#define mozilla_layers_ShadowLayers_h 1

#include "gfxASurface.h"
#include "GLDefs.h"

#include "ImageLayers.h"
#include "LayersBackend.h"
#include "mozilla/ipc/SharedMemory.h"
#include "mozilla/WidgetUtils.h"

class gfxSharedImageSurface;

namespace mozilla {

namespace gl {
class GLContext;
class TextureImage;
}
using namespace gl;

namespace layers {

class Edit;
class EditReply;
class OptionalThebesBuffer;
class PLayerChild;
class PLayersChild;
class PLayersParent;
class ShadowableLayer;
class ShadowThebesLayer;
class ShadowContainerLayer;
class ShadowImageLayer;
class ShadowColorLayer;
class ShadowCanvasLayer;
class ShadowRefLayer;
class SurfaceDescriptor;
class ThebesBuffer;
class TiledLayerComposer;
class Transaction;
class SharedImage;
class CanvasSurface;
class BasicTiledLayerBuffer;

enum BufferCapabilities {
  DEFAULT_BUFFER_CAPS = 0,
  /** 
   * The allocated buffer must be efficiently mappable as a
   * gfxImageSurface.
   */
  MAP_AS_IMAGE_SURFACE = 1 << 0
};

enum OpenMode {
  OPEN_READ_ONLY,
  OPEN_READ_WRITE
};

/**
 * We want to share layer trees across thread contexts and address
 * spaces for several reasons; chief among them
 *
 *  - a parent process can paint a child process's layer tree while
 *    the child process is blocked, say on content script.  This is
 *    important on mobile devices where UI responsiveness is key.
 *
 *  - a dedicated "compositor" process can asynchronously (wrt the
 *    browser process) composite and animate layer trees, allowing a
 *    form of pipeline parallelism between compositor/browser/content
 *
 *  - a dedicated "compositor" process can take all responsibility for
 *    accessing the GPU, which is desirable on systems with
 *    buggy/leaky drivers because the compositor process can die while
 *    browser and content live on (and failover mechanisms can be
 *    installed to quickly bring up a replacement compositor)
 *
 * The Layers model has a crisply defined API, which makes it easy to
 * safely "share" layer trees.  The ShadowLayers API extends Layers to
 * allow a remote, parent process to access a child process's layer
 * tree.
 *
 * ShadowLayerForwarder publishes a child context's layer tree to a
 * parent context.  This comprises recording layer-tree modifications
 * into atomic transactions and pushing them over IPC.
 *
 * ShadowLayerManager grafts layer subtrees published by child-context
 * ShadowLayerForwarder(s) into a parent-context layer tree.
 *
 * (Advanced note: because our process tree may have a height >2, a
 * non-leaf subprocess may both receive updates from child processes
 * and publish them to parent processes.  Put another way,
 * LayerManagers may be both ShadowLayerManagers and
 * ShadowLayerForwarders.)
 *
 * There are only shadow types for layers that have different shadow
 * vs. not-shadow behavior.  ColorLayers and ContainerLayers behave
 * the same way in both regimes (so far).
 */

class ShadowLayerForwarder
{
  friend class AutoOpenSurface;

public:
  typedef gfxASurface::gfxContentType gfxContentType;

  virtual ~ShadowLayerForwarder();

  /**
   * Begin recording a transaction to be forwarded atomically to a
   * ShadowLayerManager.
   */
  void BeginTransaction(const nsIntRect& aTargetBounds,
                        ScreenRotation aRotation);

  /**
   * The following methods may only be called after BeginTransaction()
   * but before EndTransaction().  They mirror the LayerManager
   * interface in Layers.h.
   */

  /**
   * Notify the shadow manager that a new, "real" layer has been
   * created, and a corresponding shadow layer should be created in
   * the compositing process.
   */
  void CreatedThebesLayer(ShadowableLayer* aThebes);
  void CreatedContainerLayer(ShadowableLayer* aContainer);
  void CreatedImageLayer(ShadowableLayer* aImage);
  void CreatedColorLayer(ShadowableLayer* aColor);
  void CreatedCanvasLayer(ShadowableLayer* aCanvas);
  void CreatedRefLayer(ShadowableLayer* aRef);

  /**
   * The specified layer is destroying its buffers.
   * |aBackBufferToDestroy| is deallocated when this transaction is
   * posted to the parent.  During the parent-side transaction, the
   * shadow is told to destroy its front buffer.  This can happen when
   * a new front/back buffer pair have been created because of a layer
   * resize, e.g.
   */
  void DestroyedThebesBuffer(ShadowableLayer* aThebes,
                             const SurfaceDescriptor& aBackBufferToDestroy);

  /**
   * At least one attribute of |aMutant| has changed, and |aMutant|
   * needs to sync to its shadow layer.  This initial implementation
   * forwards all attributes when any is mutated.
   */
  void Mutated(ShadowableLayer* aMutant);

  void SetRoot(ShadowableLayer* aRoot);
  /**
   * Insert |aChild| after |aAfter| in |aContainer|.  |aAfter| can be
   * NULL to indicated that |aChild| should be appended to the end of
   * |aContainer|'s child list.
   */
  void InsertAfter(ShadowableLayer* aContainer,
                   ShadowableLayer* aChild,
                   ShadowableLayer* aAfter=NULL);
  void RemoveChild(ShadowableLayer* aContainer,
                   ShadowableLayer* aChild);

  /**
   * Set aMaskLayer as the mask on aLayer.
   * Note that only image layers are properly supported
   * ShadowLayersParent::UpdateMask and accompanying ipdl
   * will need changing to update properties for other kinds
   * of mask layer.
   */
  void SetMask(ShadowableLayer* aLayer,
               ShadowableLayer* aMaskLayer);

  /**
   * Notify the shadow manager that the specified layer's back buffer
   * has new pixels and should become the new front buffer, and be
   * re-rendered, in the compositing process.  The former front buffer
   * is swapped for |aNewFrontBuffer| and becomes the new back buffer
   * for the "real" layer.
   */
  /**
   * |aBufferRect| is the screen rect covered as a whole by the
   * possibly-toroidally-rotated |aNewFrontBuffer|.  |aBufferRotation|
   * is buffer's rotation, if any.
   */
  void PaintedThebesBuffer(ShadowableLayer* aThebes,
                           const nsIntRegion& aUpdatedRegion,
                           const nsIntRect& aBufferRect,
                           const nsIntPoint& aBufferRotation,
                           const SurfaceDescriptor& aNewFrontBuffer);

  /**
   * Notify the compositor that a tiled layer buffer has changed
   * that needs to be synced to the shadow retained copy. The tiled
   * layer buffer will operate directly on the shadow retained buffer
   * and is free to choose it's own internal representation (double buffering,
   * copy on write, tiling).
   */
  void PaintedTiledLayerBuffer(ShadowableLayer* aThebes,
                               BasicTiledLayerBuffer* aTiledLayerBuffer);

  /**
   * NB: this initial implementation only forwards RGBA data for
   * ImageLayers.  This is slow, and will be optimized.
   */
  void PaintedImage(ShadowableLayer* aImage,
                    const SharedImage& aNewFrontImage);
  void PaintedCanvas(ShadowableLayer* aCanvas,
                     bool aNeedYFlip,
                     const SurfaceDescriptor& aNewFrontSurface);

  /**
   * End the current transaction and forward it to ShadowLayerManager.
   * |aReplies| are directions from the ShadowLayerManager to the
   * caller of EndTransaction().
   */
  bool EndTransaction(InfallibleTArray<EditReply>* aReplies);

  /**
   * Composite ShadowLayerManager's layer tree into aTarget.
   */
  bool ShadowDrawToTarget(gfxContext* aTarget);

  /**
   * Set an actor through which layer updates will be pushed.
   */
  void SetShadowManager(PLayersChild* aShadowManager)
  {
    mShadowManager = aShadowManager;
  }

  void SetParentBackendType(LayersBackend aBackendType)
  {
    mParentBackend = aBackendType;
  }

  /**
   * True if this is forwarding to a ShadowLayerManager.
   */
  bool HasShadowManager() const { return !!mShadowManager; }
  PLayersChild* GetShadowManager() const { return mShadowManager; }

  /**
   * The following Alloc/Open/Destroy interfaces abstract over the
   * details of working with surfaces that are shared across
   * processes.  They provide the glue between C++ Layers and the
   * ShadowLayer IPC system.
   *
   * The basic lifecycle is
   *
   *  - a Layer needs a buffer.  Its ShadowableLayer subclass calls
   *    AllocBuffer(), then calls one of the Created*Buffer() methods
   *    above to transfer the (temporary) front buffer to its
   *    ShadowLayer in the other process.  The Layer needs a
   *    gfxASurface to paint, so the ShadowableLayer uses
   *    OpenDescriptor(backBuffer) to get that surface, and hands it
   *    out to the Layer.
   *
   * - a Layer has painted new pixels.  Its ShadowableLayer calls one
   *   of the Painted*Buffer() methods above with the back buffer
   *   descriptor.  This notification is forwarded to the ShadowLayer,
   *   which uses OpenDescriptor() to access the newly-painted pixels.
   *   The ShadowLayer then updates its front buffer in a Layer- and
   *   platform-dependent way, and sends a surface descriptor back to
   *   the ShadowableLayer that becomes its new back back buffer.
   *
   * - a Layer wants to destroy its buffers.  Its ShadowableLayer
   *   calls Destroyed*Buffer(), which gives up control of the back
   *   buffer descriptor.  The actual back buffer surface is then
   *   destroyed using DestroySharedSurface() just before notifying
   *   the parent process.  When the parent process is notified, the
   *   ShadowLayer also calls DestroySharedSurface() on its front
   *   buffer, and the double-buffer pair is gone.
   */

  /**
   * Shmem (gfxSharedImageSurface) buffers are available on all
   * platforms, but they may not be optimal.
   *
   * In the absence of platform-specific buffers these fall back to
   * Shmem/gfxSharedImageSurface.
   */
  bool AllocBuffer(const gfxIntSize& aSize,
                   gfxASurface::gfxContentType aContent,
                   SurfaceDescriptor* aBuffer);

  bool AllocBufferWithCaps(const gfxIntSize& aSize,
                           gfxASurface::gfxContentType aContent,
                           uint32_t aCaps,
                           SurfaceDescriptor* aBuffer);

  void DestroySharedSurface(SurfaceDescriptor* aSurface);

  /**
   * Construct a shadow of |aLayer| on the "other side", at the
   * ShadowLayerManager.
   */
  PLayerChild* ConstructShadowFor(ShadowableLayer* aLayer);

  LayersBackend GetParentBackendType()
  {
    return mParentBackend;
  }

  /**
   * Flag the next paint as the first for a document.
   */
  void SetIsFirstPaint() { mIsFirstPaint = true; }

  virtual PRInt32 GetMaxTextureSize() const { return mMaxTextureSize; }
  void SetMaxTextureSize(PRInt32 aMaxTextureSize) { mMaxTextureSize = aMaxTextureSize; }

protected:
  ShadowLayerForwarder();

  PLayersChild* mShadowManager;

private:
  bool AllocBuffer(const gfxIntSize& aSize,
                   gfxASurface::gfxContentType aContent,
                   gfxSharedImageSurface** aBuffer);

  bool PlatformAllocBuffer(const gfxIntSize& aSize,
                           gfxASurface::gfxContentType aContent,
                           uint32_t aCaps,
                           SurfaceDescriptor* aBuffer);

  /**
   * Try to query the content type efficiently, but at worst map the
   * surface and return it in *aSurface.
   */
  static gfxContentType
  GetDescriptorSurfaceContentType(const SurfaceDescriptor& aDescriptor,
                                  OpenMode aMode,
                                  gfxASurface** aSurface);
  /**
   * It can be expensive to open a descriptor just to query its
   * content type.  If the platform impl can do this cheaply, it will
   * set *aContent and return true.
   */
  static bool
  PlatformGetDescriptorSurfaceContentType(const SurfaceDescriptor& aDescriptor,
                                          OpenMode aMode,
                                          gfxContentType* aContent,
                                          gfxASurface** aSurface);
  // (Same as above, but for surface size.)
  static gfxIntSize
  GetDescriptorSurfaceSize(const SurfaceDescriptor& aDescriptor,
                           OpenMode aMode,
                           gfxASurface** aSurface);
  static bool
  PlatformGetDescriptorSurfaceSize(const SurfaceDescriptor& aDescriptor,
                                   OpenMode aMode,
                                   gfxIntSize* aSize,
                                   gfxASurface** aSurface);

  static already_AddRefed<gfxASurface>
  OpenDescriptor(OpenMode aMode, const SurfaceDescriptor& aSurface);

  static already_AddRefed<gfxASurface>
  PlatformOpenDescriptor(OpenMode aMode, const SurfaceDescriptor& aDescriptor);

  /** Make this descriptor unusable for gfxASurface clients.  A
   * private interface with AutoOpenSurface. */
  static void
  CloseDescriptor(const SurfaceDescriptor& aDescriptor);

  static bool
  PlatformCloseDescriptor(const SurfaceDescriptor& aDescriptor);

  bool PlatformDestroySharedSurface(SurfaceDescriptor* aSurface);

  static void PlatformSyncBeforeUpdate();

  Transaction* mTxn;
  PRInt32 mMaxTextureSize;
  LayersBackend mParentBackend;

  bool mIsFirstPaint;
};

class ShadowLayerManager : public LayerManager
{
public:
  virtual ~ShadowLayerManager() {}

  virtual void GetBackendName(nsAString& name) { name.AssignLiteral("Shadow"); }

  void DestroySharedSurface(gfxSharedImageSurface* aSurface,
                            PLayersParent* aDeallocator);

  void DestroySharedSurface(SurfaceDescriptor* aSurface,
                            PLayersParent* aDeallocator);

  /** CONSTRUCTION PHASE ONLY */
  virtual already_AddRefed<ShadowThebesLayer> CreateShadowThebesLayer() = 0;
  /** CONSTRUCTION PHASE ONLY */
  virtual already_AddRefed<ShadowContainerLayer> CreateShadowContainerLayer() = 0;
  /** CONSTRUCTION PHASE ONLY */
  virtual already_AddRefed<ShadowImageLayer> CreateShadowImageLayer() = 0;
  /** CONSTRUCTION PHASE ONLY */
  virtual already_AddRefed<ShadowColorLayer> CreateShadowColorLayer() = 0;
  /** CONSTRUCTION PHASE ONLY */
  virtual already_AddRefed<ShadowCanvasLayer> CreateShadowCanvasLayer() = 0;
  /** CONSTRUCTION PHASE ONLY */
  virtual already_AddRefed<ShadowRefLayer> CreateShadowRefLayer() { return nsnull; }

  /**
   * Try to open |aDescriptor| for direct texturing.  If the
   * underlying surface supports direct texturing, a non-null
   * TextureImage is returned.  Otherwise null is returned.
   */
  static already_AddRefed<TextureImage>
  OpenDescriptorForDirectTexturing(GLContext* aContext,
                                   const SurfaceDescriptor& aDescriptor,
                                   GLenum aWrapMode);

  static void PlatformSyncBeforeReplyUpdate();

  void SetCompositorID(PRUint32 aID)
  {
    NS_ASSERTION(mCompositorID==0, "The compositor ID must be set only once.");
    mCompositorID = aID;
  }
  PRUint32 GetCompositorID() const
  {
    return mCompositorID;
  }

protected:
  ShadowLayerManager()
  : mCompositorID(0) {}

  bool PlatformDestroySharedSurface(SurfaceDescriptor* aSurface);
  PRUint32 mCompositorID;
};


/**
 * A ShadowableLayer is a Layer can be shared with a parent context
 * through a ShadowLayerForwarder.  A ShadowableLayer maps to a
 * Shadow*Layer in a parent context.
 *
 * Note that ShadowLayers can themselves be ShadowableLayers.
 */
class ShadowableLayer
{
public:
  virtual ~ShadowableLayer() {}

  virtual Layer* AsLayer() = 0;

  /**
   * True if this layer has a shadow in a parent process.
   */
  bool HasShadow() { return !!mShadow; }

  /**
   * Return the IPC handle to a Shadow*Layer referring to this if one
   * exists, NULL if not.
   */
  PLayerChild* GetShadow() { return mShadow; }

protected:
  ShadowableLayer() : mShadow(NULL) {}

  PLayerChild* mShadow;
};

/**
 * SurfaceDeallocator interface
 */
class ISurfaceDeAllocator
{
public:
  virtual void DestroySharedSurface(gfxSharedImageSurface* aSurface) = 0;
  virtual void DestroySharedSurface(SurfaceDescriptor* aSurface) = 0;
protected:
  ~ISurfaceDeAllocator() {}
};

/**
 * A ShadowLayer is the representation of a child-context's Layer in a
 * parent context.  They can be transformed, clipped,
 * etc. independently of their origin Layers.
 *
 * Note that ShadowLayers can themselves have a shadow in a parent
 * context.
 */
class ShadowLayer
{
public:
  virtual ~ShadowLayer() {}

  /**
   * Set deallocator for data recieved from IPC protocol
   * We should be able to set allocator right before swap call
   * that is why allowed multiple call with the same Allocator
   */
  virtual void SetAllocator(ISurfaceDeAllocator* aAllocator)
  {
    NS_ASSERTION(!mAllocator || mAllocator == aAllocator, "Stomping allocator?");
    mAllocator = aAllocator;
  }

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
    mUseShadowClipRect = aRect != nsnull;
    if (aRect) {
      mShadowClipRect = *aRect;
    }
  }

  void SetShadowTransform(const gfx3DMatrix& aMatrix)
  {
    mShadowTransform = aMatrix;
  }

  // These getters can be used anytime.
  float GetShadowOpacity() { return mShadowOpacity; }
  const nsIntRect* GetShadowClipRect() { return mUseShadowClipRect ? &mShadowClipRect : nsnull; }
  const nsIntRegion& GetShadowVisibleRegion() { return mShadowVisibleRegion; }
  const gfx3DMatrix& GetShadowTransform() { return mShadowTransform; }

  virtual TiledLayerComposer* AsTiledLayerComposer() { return NULL; }

protected:
  ShadowLayer()
    : mAllocator(nsnull)
    , mShadowOpacity(1.0)
    , mUseShadowClipRect(false)
  {}

  ISurfaceDeAllocator* mAllocator;
  nsIntRegion mShadowVisibleRegion;
  gfx3DMatrix mShadowTransform;
  nsIntRect mShadowClipRect;
  float mShadowOpacity;
  bool mUseShadowClipRect;
};


class ShadowThebesLayer : public ShadowLayer,
                          public ThebesLayer
{
public:
  virtual void InvalidateRegion(const nsIntRegion& aRegion)
  {
    NS_RUNTIMEABORT("ShadowThebesLayers can't fill invalidated regions");
  }

  /**
   * CONSTRUCTION PHASE ONLY
   */
  virtual void SetValidRegion(const nsIntRegion& aRegion)
  {
    mValidRegion = aRegion;
    Mutated();
  }

  /**
   * CONSTRUCTION PHASE ONLY
   *
   * Publish the remote layer's back ThebesLayerBuffer to this shadow,
   * swapping out the old front ThebesLayerBuffer (the new back buffer
   * for the remote layer).
   */
  virtual void
  Swap(const ThebesBuffer& aNewFront, const nsIntRegion& aUpdatedRegion,
       OptionalThebesBuffer* aNewBack, nsIntRegion* aNewBackValidRegion,
       OptionalThebesBuffer* aReadOnlyFront, nsIntRegion* aFrontUpdatedRegion) = 0;

  /**
   * CONSTRUCTION PHASE ONLY
   *
   * Destroy the current front buffer.
   */
  virtual void DestroyFrontBuffer() = 0;

  virtual ShadowLayer* AsShadowLayer() { return this; }

  MOZ_LAYER_DECL_NAME("ShadowThebesLayer", TYPE_SHADOW)

protected:
  ShadowThebesLayer(LayerManager* aManager, void* aImplData)
    : ThebesLayer(aManager, aImplData)
  {}
};


class ShadowContainerLayer : public ShadowLayer,
                             public ContainerLayer
{
public:
  virtual ShadowLayer* AsShadowLayer() { return this; }

  MOZ_LAYER_DECL_NAME("ShadowContainerLayer", TYPE_SHADOW)

protected:
  ShadowContainerLayer(LayerManager* aManager, void* aImplData)
    : ContainerLayer(aManager, aImplData)
  {}
};


class ShadowCanvasLayer : public ShadowLayer,
                          public CanvasLayer
{
public:
  /**
   * CONSTRUCTION PHASE ONLY
   *
   * Publish the remote layer's back surface to this shadow, swapping
   * out the old front surface (the new back surface for the remote
   * layer).
   */
  virtual void Swap(const CanvasSurface& aNewFront, bool needYFlip,
                    CanvasSurface* aNewBack) = 0;

  virtual ShadowLayer* AsShadowLayer() { return this; }

  MOZ_LAYER_DECL_NAME("ShadowCanvasLayer", TYPE_SHADOW)

protected:
  ShadowCanvasLayer(LayerManager* aManager, void* aImplData)
    : CanvasLayer(aManager, aImplData)
  {}
};


class ShadowImageLayer : public ShadowLayer,
                         public ImageLayer
{
public:
  /**
   * CONSTRUCTION PHASE ONLY
   * @see ShadowCanvasLayer::Swap
   */
  virtual void Swap(const SharedImage& aFront,
                    SharedImage* aNewBack) = 0;

  virtual ShadowLayer* AsShadowLayer() { return this; }

  MOZ_LAYER_DECL_NAME("ShadowImageLayer", TYPE_SHADOW)

protected:
  ShadowImageLayer(LayerManager* aManager, void* aImplData)
    : ImageLayer(aManager, aImplData), 
      mImageContainerID(0),
      mImageVersion(0)
  {}

  // ImageBridge protocol:
  PRUint32 mImageContainerID;
  PRUint32 mImageVersion;
};


class ShadowColorLayer : public ShadowLayer,
                         public ColorLayer
{
public:
  virtual ShadowLayer* AsShadowLayer() { return this; }

  MOZ_LAYER_DECL_NAME("ShadowColorLayer", TYPE_SHADOW)

protected:
  ShadowColorLayer(LayerManager* aManager, void* aImplData)
    : ColorLayer(aManager, aImplData)
  {}
};

class ShadowRefLayer : public ShadowLayer,
                       public RefLayer
{
public:
  virtual ShadowLayer* AsShadowLayer() { return this; }

  MOZ_LAYER_DECL_NAME("ShadowRefLayer", TYPE_SHADOW)

protected:
  ShadowRefLayer(LayerManager* aManager, void* aImplData)
    : RefLayer(aManager, aImplData)
  {}
};

bool IsSurfaceDescriptorValid(const SurfaceDescriptor& aSurface);

ipc::SharedMemory::SharedMemoryType OptimalShmemType();


} // namespace layers
} // namespace mozilla

#endif // ifndef mozilla_layers_ShadowLayers_h
