/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BUFFERHOST_H
#define MOZILLA_GFX_BUFFERHOST_H

#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/PCompositableParent.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "ThebesLayerBuffer.h"
#include "BasicTiledThebesLayer.h" // for BasicTiledLayerBuffer
#include "mozilla/RefPtr.h"

namespace mozilla {
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
class TextureHost;
class SurfaceDescriptor;

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
  CompositableHost(Compositor* aCompositor = nullptr)
    : mCompositor(aCompositor)
    , mLayer(nullptr)
  {
    MOZ_COUNT_CTOR(CompositableHost);
  }

  virtual ~CompositableHost()
  {
    MOZ_COUNT_DTOR(CompositableHost);
  }

  static TemporaryRef<CompositableHost> Create(CompositableType aType,
                                               Compositor* aCompositor = nullptr);

  virtual CompositableType GetType() = 0;

  virtual void SetCompositor(Compositor* aCompositor)
  {
    mCompositor = aCompositor;
  }

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
   * aTextureInfo identifies the texture host which should be updated.
   * aNewBack is the new data
   * aUpdated is the region which should be updated
   * aNewfront may point to the old data in this content host after the call
   * aNewBackResult may point to the updated data in this content host
   * aUpdatedRegionBack is the region in aNewBackResult which has been updated
   */
  virtual void UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack,
                            nsIntRegion* aUpdatedRegionBack)
  {
    MOZ_ASSERT(false, "should be implemented or not used");
  }

  virtual void AddTextureHost(TextureHost* aTextureHost,
                              ISurfaceAllocator* aAllocator = nullptr) = 0;
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

  Compositor* GetCompositor() const
  {
    return mCompositor;
  }

  Layer* GetLayer() const { return mLayer; }
  void SetLayer(Layer* aLayer) { mLayer = aLayer; }

  void Attach(Layer* aLayer, Compositor* aCompositor)
  {
    SetCompositor(aCompositor);
    SetLayer(aLayer);
  }
  void Detach() {
    SetLayer(nullptr);
    SetCompositor(nullptr);
  }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual void PrintInfo(nsACString& aTo, const char* aPrefix) { }
#endif

protected:
  Compositor* mCompositor;
  Layer* mLayer;
};

class CompositableParentManager;

class CompositableParent : public PCompositableParent
{
public:
  CompositableParent(CompositableParentManager* aMgr,
                     CompositableType aType, uint64_t aID = 0);
  ~CompositableParent();
  PTextureParent* AllocPTexture(const TextureInfo& aInfo) MOZ_OVERRIDE;
  bool DeallocPTexture(PTextureParent* aActor) MOZ_OVERRIDE;

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
 * reference to whatever we have created with PLayers. So, the only way to
 * actually connect these two worlds is to have something global that they can
 * both query (in the same  thread). The map is not allocated the map on the 
 * stack to avoid the badness of static initialization.
 *
 * Also, we have a compositor/PLayers protocol/etc. per layer manager, and the
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
