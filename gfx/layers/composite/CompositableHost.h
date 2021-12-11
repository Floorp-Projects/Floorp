/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BUFFERHOST_H
#define MOZILLA_GFX_BUFFERHOST_H

#include <stdint.h>                 // for uint64_t
#include <stdio.h>                  // for FILE
#include "gfxRect.h"                // for gfxRect
#include "mozilla/Assertions.h"     // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"     // for override
#include "mozilla/RefPtr.h"         // for RefPtr, RefCounted, etc
#include "mozilla/gfx/MatrixFwd.h"  // for Matrix4x4
#include "mozilla/gfx/Point.h"      // for Point
#include "mozilla/gfx/Polygon.h"    // for Polygon
#include "mozilla/gfx/Rect.h"       // for Rect
#include "mozilla/gfx/Types.h"      // for SamplingFilter
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/layers/Compositor.h"       // for Compositor
#include "mozilla/layers/CompositorTypes.h"  // for TextureInfo, etc
#include "mozilla/layers/Effects.h"          // for Texture Effect
#include "mozilla/layers/LayersTypes.h"      // for LayerRenderState, etc
#include "mozilla/layers/LayersMessages.h"
#include "mozilla/layers/TextureHost.h"  // for TextureHost
#include "mozilla/mozalloc.h"            // for operator delete
#include "nsCOMPtr.h"                    // for already_AddRefed
#include "nsRegion.h"                    // for nsIntRegion
#include "nscore.h"                      // for nsACString
#include "Units.h"                       // for CSSToScreenScale

namespace mozilla {
namespace gfx {
class DataSourceSurface;
}  // namespace gfx

namespace layers {

class Layer;
class LayerComposite;
class ImageHost;
class Compositor;
class ThebesBufferData;
class TiledContentHost;
class CompositableParentManager;
class WebRenderImageHost;
class ContentHost;
class ContentHostTexture;
struct EffectChain;

struct ImageCompositeNotificationInfo {
  base::ProcessId mImageBridgeProcessId;
  ImageCompositeNotification mNotification;
};

struct AsyncCompositableRef {
  AsyncCompositableRef() : mProcessId(mozilla::ipc::kInvalidProcessId) {}
  AsyncCompositableRef(base::ProcessId aProcessId,
                       const CompositableHandle& aHandle)
      : mProcessId(aProcessId), mHandle(aHandle) {}
  explicit operator bool() const { return !!mHandle; }
  base::ProcessId mProcessId;
  CompositableHandle mHandle;
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
 * Composite is called by the owning layer when it is composited.
 * CompositableHost will use its TextureHost(s) and call Compositor::DrawQuad to
 * do the actual rendering.
 */
class CompositableHost {
 protected:
  virtual ~CompositableHost();

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CompositableHost)
  explicit CompositableHost(const TextureInfo& aTextureInfo);

  static already_AddRefed<CompositableHost> Create(
      const TextureInfo& aTextureInfo);

  virtual CompositableType GetType() = 0;

  // If base class overrides, it should still call the parent implementation
  virtual void SetTextureSourceProvider(TextureSourceProvider* aProvider);

  // composite the contents of this buffer host to the compositor's surface
  virtual void Composite(Compositor* aCompositor, LayerComposite* aLayer,
                         EffectChain& aEffectChain, float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::SamplingFilter aSamplingFilter,
                         const gfx::IntRect& aClipRect,
                         const nsIntRegion* aVisibleRegion = nullptr,
                         const Maybe<gfx::Polygon>& aGeometry = Nothing()) = 0;

  /**
   * Update the content host.
   * aUpdated is the region which should be updated.
   */
  virtual bool UpdateThebes(const ThebesBufferData& aData,
                            const nsIntRegion& aUpdated,
                            const nsIntRegion& aOldValidRegionBack) {
    NS_ERROR("should be implemented or not used");
    return false;
  }

  /**
   * Returns the front buffer.
   * *aPictureRect (if non-null, and the returned TextureHost is non-null)
   * is set to the picture rect.
   */
  virtual TextureHost* GetAsTextureHost(gfx::IntRect* aPictureRect = nullptr) {
    return nullptr;
  }

  virtual gfx::IntSize GetImageSize() {
    MOZ_ASSERT(false, "Should have been overridden");
    return gfx::IntSize();
  }

  const TextureInfo& GetTextureInfo() const { return mTextureInfo; }

  TextureSourceProvider* GetTextureSourceProvider() const;

  Layer* GetLayer() const { return mLayer; }
  void SetLayer(Layer* aLayer) { mLayer = aLayer; }

  virtual ContentHost* AsContentHost() { return nullptr; }
  virtual ContentHostTexture* AsContentHostTexture() { return nullptr; }
  virtual ImageHost* AsImageHost() { return nullptr; }
  virtual TiledContentHost* AsTiledContentHost() { return nullptr; }
  virtual WebRenderImageHost* AsWebRenderImageHost() { return nullptr; }

  typedef uint32_t AttachFlags;
  static const AttachFlags NO_FLAGS = 0;
  static const AttachFlags ALLOW_REATTACH = 1;
  static const AttachFlags KEEP_ATTACHED = 2;
  static const AttachFlags FORCE_DETACH = 2;

  virtual void Attach(Layer* aLayer, TextureSourceProvider* aProvider,
                      AttachFlags aFlags = NO_FLAGS) {
    MOZ_ASSERT(aProvider);
    NS_ASSERTION(aFlags & ALLOW_REATTACH || !mAttached,
                 "Re-attaching compositables must be explicitly authorised");
    SetTextureSourceProvider(aProvider);
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
  virtual void Detach(Layer* aLayer = nullptr, AttachFlags aFlags = NO_FLAGS) {
    if (!mKeepAttached || aLayer == mLayer || aFlags & FORCE_DETACH) {
      SetLayer(nullptr);
      mAttached = false;
      mKeepAttached = false;
    }
  }
  bool IsAttached() { return mAttached; }

  virtual void Dump(std::stringstream& aStream, const char* aPrefix = "",
                    bool aDumpHtml = false) {}
  static void DumpTextureHost(std::stringstream& aStream,
                              TextureHost* aTexture);

  virtual already_AddRefed<gfx::DataSourceSurface> GetAsSurface() {
    return nullptr;
  }

  struct TimedTexture {
    CompositableTextureHostRef mTexture;
    TimeStamp mTimeStamp;
    gfx::IntRect mPictureRect;
    int32_t mFrameID;
    int32_t mProducerID;
  };
  virtual void UseTextureHost(const nsTArray<TimedTexture>& aTextures);
  virtual void UseComponentAlphaTextures(TextureHost* aTextureOnBlack,
                                         TextureHost* aTextureOnWhite);
  virtual void RemoveTextureHost(TextureHost* aTexture);

  uint64_t GetCompositorBridgeID() const { return mCompositorBridgeID; }

  const AsyncCompositableRef& GetAsyncRef() const { return mAsyncRef; }
  void SetAsyncRef(const AsyncCompositableRef& aRef) { mAsyncRef = aRef; }

  void SetCompositorBridgeID(uint64_t aID) { mCompositorBridgeID = aID; }

  virtual bool Lock() { return false; }

  virtual void Unlock() {}

  virtual already_AddRefed<TexturedEffect> GenEffect(
      const gfx::SamplingFilter aSamplingFilter) {
    return nullptr;
  }

  /// Called when shutting down the layer tree.
  /// This is a good place to clear all potential gpu resources before the
  /// widget is is destroyed.
  virtual void CleanupResources() {}

  virtual void BindTextureSource() {}

  virtual uint32_t GetDroppedFrames() { return 0; }

 protected:
 protected:
  TextureInfo mTextureInfo;
  AsyncCompositableRef mAsyncRef;
  uint64_t mCompositorBridgeID;
  RefPtr<TextureSourceProvider> mTextureSourceProvider;
  Layer* mLayer;
  bool mAttached;
  bool mKeepAttached;
};

class AutoLockCompositableHost final {
 public:
  explicit AutoLockCompositableHost(CompositableHost* aHost) : mHost(aHost) {
    mSucceeded = (mHost && mHost->Lock());
  }

  ~AutoLockCompositableHost() {
    if (mSucceeded && mHost) {
      mHost->Unlock();
    }
  }

  bool Failed() const { return !mSucceeded; }

 private:
  RefPtr<CompositableHost> mHost;
  bool mSucceeded;
};

}  // namespace layers
}  // namespace mozilla

#endif
