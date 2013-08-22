/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BUFFERHOST_H
#define MOZILLA_GFX_BUFFERHOST_H

#include <stdint.h>                     // for uint64_t
#include <stdio.h>                      // for FILE
#include "mozilla-config.h"             // for MOZ_DUMP_PAINTING
#include "gfxPoint.h"                   // for gfxSize
#include "gfxRect.h"                    // for gfxRect
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr, RefCounted, etc
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for Filter
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/CompositorTypes.h"  // for TextureInfo, etc
#include "mozilla/layers/LayersTypes.h"  // for LayerRenderState, etc
#include "mozilla/layers/PCompositableParent.h"
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsRegion.h"                   // for nsIntRegion
#include "nscore.h"                     // for nsACString

class gfxImageSurface;
struct nsIntPoint;
struct nsIntRect;

namespace mozilla {
namespace gfx {
class Matrix4x4;
}

namespace layers {

// Some properties of a Layer required for tiling
struct TiledLayerProperties
{
  nsIntRegion mVisibleRegion;
  nsIntRegion mValidRegion;
  gfxRect mDisplayPort;
  gfxSize mEffectiveResolution;
  gfxRect mCompositionBounds;
  bool mRetainTiles;
};

class Layer;
class DeprecatedTextureHost;
class TextureHost;
class SurfaceDescriptor;
class Compositor;
class ISurfaceAllocator;
class ThebesBufferData;
class TiledLayerComposer;
struct EffectChain;

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
class CompositableHost : public RefCounted<CompositableHost>
{
public:
  CompositableHost(const TextureInfo& aTextureInfo);

  virtual ~CompositableHost();

  static TemporaryRef<CompositableHost> Create(const TextureInfo& aTextureInfo);

  virtual CompositableType GetType() = 0;

  // If base class overrides, it should still call the parent implementation
  virtual void SetCompositor(Compositor* aCompositor);

  // composite the contents of this buffer host to the compositor's surface
  virtual void Composite(EffectChain& aEffectChain,
                         float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Point& aOffset,
                         const gfx::Filter& aFilter,
                         const gfx::Rect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr,
                         TiledLayerProperties* aLayerProperties = nullptr) = 0;

  /**
   * @return true if we should schedule a composition.
   */
  virtual bool Update(const SurfaceDescriptor& aImage,
                      SurfaceDescriptor* aResult = nullptr);

  /**
   * Update the content host.
   * aUpdated is the region which should be updated
   * aUpdatedRegionBack is the region in aNewBackResult which has been updated
   */
  virtual void UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack,
                            nsIntRegion* aUpdatedRegionBack)
  {
    MOZ_ASSERT(false, "should be implemented or not used");
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
   * Ensure that a suitable texture host exists in this compositable. The
   * compositable host may or may not create a new texture host. If a texture
   * host is replaced, then the compositable is responsible for enusring it is
   * destroyed correctly (without leaking resources).
   * aTextureId - identifies the texture within the compositable, how the
   * compositable chooses to use this is between the compositable client and
   * host and will vary between types of compositable.
   * aSurface - the new or existing texture host should support surface
   * descriptors of the same type and, if necessary, this specific surface
   * descriptor. Whether it is necessary or not depends on the protocol between
   * the compositable client and host.
   * aAllocator - the allocator used to allocate and de-allocate resources.
   * aTextureInfo - contains flags for the texture.
   */
  virtual void EnsureDeprecatedTextureHost(TextureIdentifier aTextureId,
                                 const SurfaceDescriptor& aSurface,
                                 ISurfaceAllocator* aAllocator,
                                 const TextureInfo& aTextureInfo)
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
  virtual void EnsureDeprecatedTextureHostIncremental(ISurfaceAllocator* aAllocator,
                                            const TextureInfo& aTextureInfo,
                                            const nsIntRect& aBufferRect)
  {
    MOZ_ASSERT(false, "should be implemented or not used");
  }

  virtual DeprecatedTextureHost* GetDeprecatedTextureHost() { return nullptr; }

  /**
   * Returns the front buffer.
   */
  virtual TextureHost* GetTextureHost() { return nullptr; }

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
  void Detach(Layer* aLayer = nullptr)
  {
    if (!mKeepAttached ||
        aLayer == mLayer) {
      SetLayer(nullptr);
      SetCompositor(nullptr);
      mAttached = false;
      mKeepAttached = false;
    }
  }
  bool IsAttached() { return mAttached; }

#ifdef MOZ_DUMP_PAINTING
  virtual void Dump(FILE* aFile=nullptr,
                    const char* aPrefix="",
                    bool aDumpHtml=false) { }
  static void DumpDeprecatedTextureHost(FILE* aFile, DeprecatedTextureHost* aTexture);
  static void DumpTextureHost(FILE* aFile, TextureHost* aTexture);

  virtual already_AddRefed<gfxImageSurface> GetAsSurface() { return nullptr; }
#endif

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual void PrintInfo(nsACString& aTo, const char* aPrefix) { }
#endif

  void AddTextureHost(TextureHost* aTexture);
  virtual void UseTextureHost(TextureHost* aTexture) {}
  void RemoveTextureHost(uint64_t aTextureID);
  TextureHost* GetTextureHost(uint64_t aTextureID);

protected:
  TextureInfo mTextureInfo;
  Compositor* mCompositor;
  Layer* mLayer;
  RefPtr<TextureHost> mFirstTexture;
  bool mAttached;
  bool mKeepAttached;
};

class CompositableParentManager;

class CompositableParent : public PCompositableParent
{
public:
  CompositableParent(CompositableParentManager* aMgr,
                     const TextureInfo& aTextureInfo,
                     uint64_t aID = 0);
  ~CompositableParent();

  virtual void ActorDestroy(ActorDestroyReason why) MOZ_OVERRIDE;

  CompositableHost* GetCompositableHost() const
  {
    return mHost;
  }

  void SetCompositableHost(CompositableHost* aHost)
  {
    mHost = aHost;
  }

  CompositableType GetType() const
  {
    return mType;
  }

  CompositableParentManager* GetCompositableManager() const
  {
    return mManager;
  }

  void SetCompositorID(uint64_t aCompositorID)
  {
    mCompositorID = aCompositorID;
  }

  uint64_t GetCompositorID() const
  {
    return mCompositorID;
  }

private:
  RefPtr<CompositableHost> mHost;
  CompositableParentManager* mManager;
  CompositableType mType;
  uint64_t mID;
  uint64_t mCompositorID;
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
  CompositableParent* Get(uint64_t aID);
  void Set(uint64_t aID, CompositableParent* aParent);
  void Erase(uint64_t aID);
  void Clear();
} // CompositableMap


} // namespace
} // namespace

#endif
