/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BUFFERHOST_H
#define MOZILLA_GFX_BUFFERHOST_H

#include <stdint.h>                     // for uint64_t
#include <stdio.h>                      // for FILE
#include "gfxRect.h"                    // for gfxRect
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr, RefCounted, etc
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for Filter
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/CompositorTypes.h"  // for TextureInfo, etc
#include "mozilla/layers/Effects.h"     // for Texture Effect
#include "mozilla/layers/LayersTypes.h"  // for LayerRenderState, etc
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/layers/TextureHost.h" // for TextureHost
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsRegion.h"                   // for nsIntRegion
#include "nscore.h"                     // for nsACString
#include "Units.h"                      // for CSSToScreenScale

struct nsIntPoint;
struct nsIntRect;

namespace mozilla {
namespace gfx {
class Matrix4x4;
class DataSourceSurface;
}

namespace layers {

class Layer;
class SurfaceDescriptor;
class Compositor;
class ISurfaceAllocator;
class ThebesBufferData;
class TiledLayerComposer;
class CompositableParentManager;
class PCompositableParent;
struct EffectChain;

/**
 * A base class for doing CompositableHost and platform dependent task on TextureHost.
 */
class CompositableBackendSpecificData
{
protected:
  virtual ~CompositableBackendSpecificData() { }

public:
  NS_INLINE_DECL_REFCOUNTING(CompositableBackendSpecificData)

  CompositableBackendSpecificData()
  {
  }

  virtual void SetCompositor(Compositor* aCompositor) {}
  virtual void ClearData() {}

};

/**
 * The compositor-side counterpart to CompositableClient. Responsible for
 * updating textures and data about textures from IPC and how textures are
 * composited (tiling, double buffering, etc.).
 *
 * Update (for images/canvases) and UpdateThebes (for Thebes) are called during
 * the layers transaction to update the Compositbale's textures from the
 * content side. The actual update (and any syncronous upload) is done by the
 * TextureHost, but it is coordinated by the CompositableHost.
 *
 * Composite is called by the owning layer when it is composited. CompositableHost
 * will use its TextureHost(s) and call Compositor::DrawQuad to do the actual
 * rendering.
 */
class CompositableHost
{
protected:
  virtual ~CompositableHost();

public:
  NS_INLINE_DECL_REFCOUNTING(CompositableHost)
  explicit CompositableHost(const TextureInfo& aTextureInfo);

  static TemporaryRef<CompositableHost> Create(const TextureInfo& aTextureInfo);

  virtual CompositableType GetType() = 0;

  virtual CompositableBackendSpecificData* GetCompositableBackendSpecificData()
  {
    return mBackendData;
  }

  virtual void SetCompositableBackendSpecificData(CompositableBackendSpecificData* aBackendData)
  {
    mBackendData = aBackendData;
  }

  // If base class overrides, it should still call the parent implementation
  virtual void SetCompositor(Compositor* aCompositor);

  // composite the contents of this buffer host to the compositor's surface
  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr) = 0;

  /**
   * Update the content host.
   * aUpdated is the region which should be updated
   * aUpdatedRegionBack is the region in aNewBackResult which has been updated
   */
  virtual bool UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack,
                            nsIntRegion* aUpdatedRegionBack)
  {
    NS_ERROR("should be implemented or not used");
    return false;
  }

  /**
   * Update the content host using a surface that only contains the updated
   * region.
   *
   * Takes ownership of aSurface, and is responsible for freeing it.
   *
   * @param aTextureId Texture to update.
   * @param aSurface Surface containing the update area. Its contents are relative
   *                 to aUpdated.TopLeft()
   * @param aUpdated Area of the content host to update.
   * @param aBufferRect New area covered by the content host.
   * @param aBufferRotation New buffer rotation.
   */
  virtual void UpdateIncremental(TextureIdentifier aTextureId,
                                 SurfaceDescriptor& aSurface,
                                 const nsIntRegion& aUpdated,
                                 const nsIntRect& aBufferRect,
                                 const nsIntPoint& aBufferRotation)
  {
    MOZ_ASSERT(false, "should be implemented or not used");
  }

  /**
   * Ensure that a suitable texture host exists in this compsitable.
   *
   * Only used with ContentHostIncremental.
   *
   * No SurfaceDescriptor or TextureIdentifier is provider as we
   * don't have a single surface for the texture contents, and we
   * need to allocate our own one to be updated later.
   */
  virtual bool CreatedIncrementalTexture(ISurfaceAllocator* aAllocator,
                                         const TextureInfo& aTextureInfo,
                                         const nsIntRect& aBufferRect)
  {
    NS_ERROR("should be implemented or not used");
    return false;
  }

  /**
   * Returns the front buffer.
   */
  virtual TextureHost* GetAsTextureHost() { return nullptr; }

  virtual LayerRenderState GetRenderState() = 0;

  virtual void SetPictureRect(const nsIntRect& aPictureRect)
  {
    MOZ_ASSERT(false, "Should have been overridden");
  }

  /**
   * Adds a mask effect using this texture as the mask, if possible.
   * @return true if the effect was added, false otherwise.
   */
  bool AddMaskEffect(EffectChain& aEffects,
                     const gfx::Matrix4x4& aTransform,
                     bool aIs3D = false);

  void RemoveMaskEffect();

  Compositor* GetCompositor() const
  {
    return mCompositor;
  }

  Layer* GetLayer() const { return mLayer; }
  void SetLayer(Layer* aLayer) { mLayer = aLayer; }

  virtual TiledLayerComposer* AsTiledLayerComposer() { return nullptr; }

  typedef uint32_t AttachFlags;
  static const AttachFlags NO_FLAGS = 0;
  static const AttachFlags ALLOW_REATTACH = 1;
  static const AttachFlags KEEP_ATTACHED = 2;
  static const AttachFlags FORCE_DETACH = 2;

  virtual void Attach(Layer* aLayer,
                      Compositor* aCompositor,
                      AttachFlags aFlags = NO_FLAGS)
  {
    MOZ_ASSERT(aCompositor, "Compositor is required");
    NS_ASSERTION(aFlags & ALLOW_REATTACH || !mAttached,
                 "Re-attaching compositables must be explicitly authorised");
    SetCompositor(aCompositor);
    SetLayer(aLayer);
    mAttached = true;
    mKeepAttached = aFlags & KEEP_ATTACHED;
  }
  // Detach this compositable host from its layer.
  // If we are used for async video, then it is not safe to blindly detach since
  // we might be re-attached to a different layer. aLayer is the layer which the
  // caller expects us to be attached to, we will only detach if we are in fact
  // attached to that layer. If we are part of a normal layer, then we will be
  // detached in any case. if aLayer is null, then we will only detach if we are
  // not async.
  // Only force detach if the IPDL tree is being shutdown.
  virtual void Detach(Layer* aLayer = nullptr, AttachFlags aFlags = NO_FLAGS)
  {
    if (!mKeepAttached ||
        aLayer == mLayer ||
        aFlags & FORCE_DETACH) {
      SetLayer(nullptr);
      mAttached = false;
      mKeepAttached = false;
      if (mBackendData) {
        mBackendData->ClearData();
      }
    }
  }
  bool IsAttached() { return mAttached; }

#ifdef MOZ_DUMP_PAINTING
  virtual void Dump(std::stringstream& aStream,
                    const char* aPrefix="",
                    bool aDumpHtml=false) { }
  static void DumpTextureHost(std::stringstream& aStream, TextureHost* aTexture);

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() { return nullptr; }
#endif

  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix) = 0;

  virtual void UseTextureHost(TextureHost* aTexture);
  virtual void UseComponentAlphaTextures(TextureHost* aTextureOnBlack,
                                         TextureHost* aTextureOnWhite);
  virtual void UseOverlaySource(OverlaySource aOverlay) { }

  virtual void RemoveTextureHost(TextureHost* aTexture);

  // Called every time this is composited
  void BumpFlashCounter() {
    mFlashCounter = mFlashCounter >= DIAGNOSTIC_FLASH_COUNTER_MAX
                  ? DIAGNOSTIC_FLASH_COUNTER_MAX : mFlashCounter + 1;
  }

  static PCompositableParent*
  CreateIPDLActor(CompositableParentManager* mgr,
                  const TextureInfo& textureInfo,
                  uint64_t asyncID);

  static bool DestroyIPDLActor(PCompositableParent* actor);

  static CompositableHost* FromIPDLActor(PCompositableParent* actor);

  uint64_t GetCompositorID() const { return mCompositorID; }

  uint64_t GetAsyncID() const { return mAsyncID; }

  void SetCompositorID(uint64_t aID) { mCompositorID = aID; }

  void SetAsyncID(uint64_t aID) { mAsyncID = aID; }

  virtual bool Lock() { return false; }

  virtual void Unlock() { }

  virtual TemporaryRef<TexturedEffect> GenEffect(const gfx::Filter& aFilter) {
    return nullptr;
  }

protected:
  TextureInfo mTextureInfo;
  uint64_t mAsyncID;
  uint64_t mCompositorID;
  Compositor* mCompositor;
  Layer* mLayer;
  RefPtr<CompositableBackendSpecificData> mBackendData;
  uint32_t mFlashCounter; // used when the pref "layers.flash-borders" is true.
  bool mAttached;
  bool mKeepAttached;
};

class AutoLockCompositableHost MOZ_FINAL
{
public:
  explicit AutoLockCompositableHost(CompositableHost* aHost)
    : mHost(aHost)
  {
    mSucceeded = mHost->Lock();
  }

  ~AutoLockCompositableHost()
  {
    if (mSucceeded) {
      mHost->Unlock();
    }
  }

  bool Failed() const { return !mSucceeded; }

private:
  RefPtr<CompositableHost> mHost;
  bool mSucceeded;
};

/**
 * Global CompositableMap, to use in the compositor thread only.
 *
 * PCompositable and PLayer can, in the case of async textures, be managed by
 * different top level protocols. In this case they don't share the same
 * communication channel and we can't send an OpAttachCompositable (PCompositable,
 * PLayer) message.
 *
 * In order to attach a layer and the right compositable if the the compositable
 * is async, we store references to the async compositables in a CompositableMap
 * that is accessed only on the compositor thread. During a layer transaction we
 * send the message OpAttachAsyncCompositable(ID, PLayer), and on the compositor
 * side we lookup the ID in the map and attach the correspondig compositable to
 * the layer.
 *
 * CompositableMap must be global because the image bridge doesn't have any
 * reference to whatever we have created with PLayerTransaction. So, the only way to
 * actually connect these two worlds is to have something global that they can
 * both query (in the same  thread). The map is not allocated the map on the 
 * stack to avoid the badness of static initialization.
 *
 * Also, we have a compositor/PLayerTransaction protocol/etc. per layer manager, and the
 * ImageBridge is used by all the existing compositors that have a video, so
 * there isn't an instance or "something" that lives outside the boudaries of a
 * given layer manager on the compositor thread except the image bridge and the
 * thread itself.
 */
namespace CompositableMap {
  void Create();
  void Destroy();
  PCompositableParent* Get(uint64_t aID);
  void Set(uint64_t aID, PCompositableParent* aParent);
  void Erase(uint64_t aID);
  void Clear();
} // CompositableMap


} // namespace
} // namespace

#endif
