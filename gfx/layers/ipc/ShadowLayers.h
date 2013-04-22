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
#include "mozilla/layers/Compositor.h"
#include "mozilla/ipc/SharedMemory.h"
#include "mozilla/WidgetUtils.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "mozilla/layers/CompositableForwarder.h"

class gfxSharedImageSurface;

namespace mozilla {

namespace gl {
class GLContext;
class TextureImage;
}

namespace layers {

class CompositableClient;
class Edit;
class EditReply;
class OptionalThebesBuffer;
class PLayerChild;
class PLayerTransactionChild;
class PLayerTransactionParent;
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
class SurfaceDescriptor;
class CanvasSurface;
class TextureClientShmem;
class ContentClientRemote;
class CompositableChild;
class ImageClient;
class CanvasClient;
class ContentClient;

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
 *
 *
 * The mecanism to shadow the layer tree on the compositor through IPC works as
 * follows:
 * The layer tree is managed on the content thread, and shadowed in the compositor
 * thread. The shadow layer tree is only kept in sync with whatever happens in
 * the content thread. To do this we use IPDL protocols. IPDL is a domain
 * specific language that describes how two processes or thread should
 * communicate. C++ code is generated from .ipdl files to implement the message
 * passing, synchronization and serialization logic. To use the generated code
 * we implement classes that inherit the generated IPDL actor. the ipdl actors
 * of a protocol PX are PXChild or PXParent (the generated class), and we
 * conventionally implement XChild and XParent. The Parent side of the protocol
 * is the one that lives on the compositor thread. Think of IPDL actors as
 * endpoints of communication. they are useful to send messages and also to
 * dispatch the message to the right actor on the other side. One nice property
 * of an IPDL actor is that when an actor, say PXChild is sent in a message, the
 * PXParent comes out in the other side. we use this property a lot to dispatch
 * messages to the right layers and compositable, each of which have their own
 * ipdl actor on both side.
 *
 * Most of the synchronization logic happens in layer transactions and
 * compositable transactions.
 * A transaction is a set of changes to the layers and/or the compositables
 * that are sent and applied together to the compositor thread to keep the
 * ShadowLayer in a coherent state.
 * Layer transactions maintain the shape of the shadow layer tree, and
 * synchronize the texture data held by compositables. Layer transactions
 * are always between the content thread and the compositor thread.
 * Compositable transactions are subset of a layer transaction with which only
 * compositables and textures can be manipulated, and does not always originate
 * from the content thread. (See CompositableForwarder.h and ImageBridgeChild.h)
 */

class ShadowLayerForwarder : public CompositableForwarder
{
  friend class AutoOpenSurface;
  friend class TextureClientShmem;

public:
  typedef gfxASurface::gfxContentType gfxContentType;

  virtual ~ShadowLayerForwarder();

  /**
   * Setup the IPDL actor for aCompositable to be part of layers
   * transactions.
   */
  void Connect(CompositableClient* aCompositable);

  virtual void CreatedSingleBuffer(CompositableClient* aCompositable,
                                   const SurfaceDescriptor& aDescriptor,
                                   const TextureInfo& aTextureInfo,
                                   const SurfaceDescriptor* aDescriptorOnWhite = nullptr) MOZ_OVERRIDE;
  virtual void CreatedDoubleBuffer(CompositableClient* aCompositable,
                                   const SurfaceDescriptor& aFrontDescriptor,
                                   const SurfaceDescriptor& aBackDescriptor,
                                   const TextureInfo& aTextureInfo,
                                   const SurfaceDescriptor* aFrontDescriptorOnWhite = nullptr,
                                   const SurfaceDescriptor* aBackDescriptorOnWhite = nullptr) MOZ_OVERRIDE;
  virtual void DestroyThebesBuffer(CompositableClient* aCompositable) MOZ_OVERRIDE;

  /**
   * Adds an edit in the layers transaction in order to attach
   * the corresponding compositable and layer on the compositor side.
   * Connect must have been called on aCompositable beforehand.
   */
  void Attach(CompositableClient* aCompositable,
              ShadowableLayer* aLayer);

  /**
   * Adds an edit in the transaction in order to attach a Compositable that
   * is not managed by this ShadowLayerForwarder (for example, by ImageBridge
   * in the case of async-video).
   * Since the compositable is not managed by this forwarder, we can't use
   * the compositable or it's IPDL actor here, so we use an ID instead, that
   * is matched on the compositor side.
   */
  void AttachAsyncCompositable(uint64_t aCompositableID,
                               ShadowableLayer* aLayer);

  /**
   * Begin recording a transaction to be forwarded atomically to a
   * ShadowLayerManager.
   */
  void BeginTransaction(const nsIntRect& aTargetBounds,
                        ScreenRotation aRotation,
                        const nsIntRect& aClientBounds,
                        mozilla::dom::ScreenOrientation aOrientation);

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
  virtual void DestroyedThebesBuffer(const SurfaceDescriptor& aBackBufferToDestroy) MOZ_OVERRIDE;

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
  void RepositionChild(ShadowableLayer* aContainer,
                       ShadowableLayer* aChild,
                       ShadowableLayer* aAfter=NULL);

  /**
   * Set aMaskLayer as the mask on aLayer.
   * Note that only image layers are properly supported
   * LayerTransactionParent::UpdateMask and accompanying ipdl
   * will need changing to update properties for other kinds
   * of mask layer.
   */
  void SetMask(ShadowableLayer* aLayer,
               ShadowableLayer* aMaskLayer);

  /**
   * Notify the compositor that a tiled layer buffer has changed
   * that needs to be synced to the shadow retained copy. The tiled
   * layer buffer will operate directly on the shadow retained buffer
   * and is free to choose it's own internal representation (double buffering,
   * copy on write, tiling).
   */
  virtual void PaintedTiledLayerBuffer(CompositableClient* aCompositable,
                                       BasicTiledLayerBuffer* aTiledLayerBuffer) MOZ_OVERRIDE;

  /**
   * Notify the compositor that a compositable will be updated asynchronously
   * through ImageBridge, using an ID to connect the protocols on the
   * compositor side.
   */
  void AttachAsyncCompositable(PLayerTransactionChild* aLayer, uint64_t aID);

  /**
   * Communicate to the compositor that the texture identified by aLayer
   * and aIdentifier has been updated to aImage.
   */
  virtual void UpdateTexture(CompositableClient* aCompositable,
                             TextureIdentifier aTextureId,
                             SurfaceDescriptor* aDescriptor) MOZ_OVERRIDE;

  /**
   * Communicate to the compositor that aRegion in the texture identified by aLayer
   * and aIdentifier has been updated to aThebesBuffer.
   */
  virtual void UpdateTextureRegion(CompositableClient* aCompositable,
                                   const ThebesBufferData& aThebesBufferData,
                                   const nsIntRegion& aUpdatedRegion) MOZ_OVERRIDE;

  /**
   * Communicate the picture rect of an image to the compositor
   */
  void UpdatePictureRect(CompositableClient* aCompositable,
                         const nsIntRect& aRect);

  /**
   * End the current transaction and forward it to ShadowLayerManager.
   * |aReplies| are directions from the ShadowLayerManager to the
   * caller of EndTransaction().
   */
  bool EndTransaction(InfallibleTArray<EditReply>* aReplies);

  /**
   * Set an actor through which layer updates will be pushed.
   */
  void SetShadowManager(PLayerTransactionChild* aShadowManager)
  {
    mShadowManager = aShadowManager;
  }

  /**
   * True if this is forwarding to a ShadowLayerManager.
   */
  bool HasShadowManager() const { return !!mShadowManager; }
  PLayerTransactionChild* GetShadowManager() const { return mShadowManager; }

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

  // ISurfaceAllocator
  virtual bool AllocUnsafeShmem(size_t aSize,
                                ipc::SharedMemory::SharedMemoryType aType,
                                ipc::Shmem* aShmem) MOZ_OVERRIDE;
  virtual bool AllocShmem(size_t aSize,
                          ipc::SharedMemory::SharedMemoryType aType,
                          ipc::Shmem* aShmem) MOZ_OVERRIDE;
  virtual void DeallocShmem(ipc::Shmem& aShmem) MOZ_OVERRIDE;

  /**
   * Construct a shadow of |aLayer| on the "other side", at the
   * ShadowLayerManager.
   */
  PLayerChild* ConstructShadowFor(ShadowableLayer* aLayer);

  /**
   * Flag the next paint as the first for a document.
   */
  void SetIsFirstPaint() { mIsFirstPaint = true; }

  static void PlatformSyncBeforeUpdate();

  static already_AddRefed<gfxASurface>
  OpenDescriptor(OpenMode aMode, const SurfaceDescriptor& aSurface);

protected:
  ShadowLayerForwarder();

  PLayerTransactionChild* mShadowManager;

#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  virtual PGrallocBufferChild* AllocGrallocBuffer(const gfxIntSize& aSize,
                                                  gfxASurface::gfxContentType aContent,
                                                  MaybeMagicGrallocBufferHandle* aHandle) MOZ_OVERRIDE;
#endif

private:
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
  PlatformOpenDescriptor(OpenMode aMode, const SurfaceDescriptor& aDescriptor);

  /**
   * Make this descriptor unusable for gfxASurface clients. A
   * private interface with AutoOpenSurface.
   */
  static void
  CloseDescriptor(const SurfaceDescriptor& aDescriptor);

  static bool
  PlatformCloseDescriptor(const SurfaceDescriptor& aDescriptor);

  bool PlatformDestroySharedSurface(SurfaceDescriptor* aSurface);

  Transaction* mTxn;

  bool mIsFirstPaint;
};

class ShadowLayerManager : public LayerManager
{
public:
  virtual ~ShadowLayerManager() {}

  virtual void GetBackendName(nsAString& name) { name.AssignLiteral("Shadow"); }

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
  virtual already_AddRefed<ShadowRefLayer> CreateShadowRefLayer() { return nullptr; }

  virtual void NotifyShadowTreeTransaction() {}

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

  void SetCompositorID(uint32_t aID)
  {
    NS_ASSERTION(mCompositor, "No compositor");
    mCompositor->SetCompositorID(aID);
  }

  Compositor* GetCompositor() const
  {
    return mCompositor;
  }

protected:
  ShadowLayerManager()
  : mCompositor(nullptr)
  {}

  bool PlatformDestroySharedSurface(SurfaceDescriptor* aSurface);
  RefPtr<Compositor> mCompositor;
};

class CompositableClient;

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

  virtual CompositableClient* GetCompositableClient() { return nullptr; }
protected:
  ShadowableLayer() : mShadow(NULL) {}

  PLayerChild* mShadow;
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

  // These getters can be used anytime.
  float GetShadowOpacity() { return mShadowOpacity; }
  const nsIntRect* GetShadowClipRect() { return mUseShadowClipRect ? &mShadowClipRect : nullptr; }
  const nsIntRegion& GetShadowVisibleRegion() { return mShadowVisibleRegion; }
  const gfx3DMatrix& GetShadowTransform() { return mShadowTransform; }

protected:
  ShadowLayer()
    : mShadowOpacity(1.0f)
    , mUseShadowClipRect(false)
  {}

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
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ValidRegion", this));
    mValidRegion = aRegion;
    Mutated();
  }

  const nsIntRegion& GetValidRegion() { return mValidRegion; }

  virtual void
  Swap(const ThebesBuffer& aNewFront, const nsIntRegion& aUpdatedRegion,
       OptionalThebesBuffer* aNewBack, nsIntRegion* aNewBackValidRegion,
       OptionalThebesBuffer* aReadOnlyFront, nsIntRegion* aFrontUpdatedRegion) {
    NS_RUNTIMEABORT("should not use layer swap");
  };

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
  virtual void Swap(const SurfaceDescriptor& aNewFront, bool needYFlip,
                    SurfaceDescriptor* aNewBack) = 0;

  virtual ShadowLayer* AsShadowLayer() { return this; }

  void SetBounds(nsIntRect aBounds) { mBounds = aBounds; }

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
  virtual ShadowLayer* AsShadowLayer() { return this; }

  MOZ_LAYER_DECL_NAME("ShadowImageLayer", TYPE_SHADOW)

protected:
  ShadowImageLayer(LayerManager* aManager, void* aImplData)
    : ImageLayer(aManager, aImplData)
  {}
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

} // namespace layers
} // namespace mozilla

#endif // ifndef mozilla_layers_ShadowLayers_h
