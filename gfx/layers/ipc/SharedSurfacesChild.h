/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SHAREDSURFACESCHILD_H
#define MOZILLA_GFX_SHAREDSURFACESCHILD_H

#include <stdint.h>                            // for uint32_t, uint64_t
#include "mozilla/Attributes.h"                // for override
#include "mozilla/Maybe.h"                     // for Maybe
#include "mozilla/RefPtr.h"                    // for already_AddRefed
#include "mozilla/StaticPtr.h"                 // for StaticRefPtr
#include "mozilla/gfx/UserData.h"              // for UserDataKey
#include "mozilla/layers/LayersSurfaces.h"     // for SurfaceDescriptor
#include "mozilla/webrender/WebRenderTypes.h"  // for wr::ImageKey
#include "nsTArray.h"                          // for AutoTArray
#include "nsThreadUtils.h"                     // for Runnable
#include "ImageTypes.h"                        // for ContainerProducerID

namespace mozilla {
namespace layers {
class AnimationImageKeyData;
}  // namespace layers
}  // namespace mozilla

template <>
struct nsTArray_RelocationStrategy<mozilla::layers::AnimationImageKeyData> {
  typedef nsTArray_RelocateUsingMoveConstructor<
      mozilla::layers::AnimationImageKeyData>
      Type;
};

namespace mozilla {
namespace gfx {
class SourceSurface;
class SourceSurfaceSharedData;
}  // namespace gfx

namespace wr {
class IpcResourceUpdateQueue;
}  // namespace wr

namespace layers {

class CompositorManagerChild;
class RenderRootStateManager;

class SharedSurfacesChild {
 public:
  /**
   * Request that the surface be mapped into the compositor thread's memory
   * space. This is useful for when the caller itself has no present need for
   * the surface to be mapped, but knows there will be such a need in the
   * future. This may be called from any thread, but it may cause a dispatch to
   * the main thread.
   */
  static void Share(gfx::SourceSurfaceSharedData* aSurface);

  /**
   * Request that the surface be mapped into the compositor thread's memory
   * space, and a valid ExternalImageId be generated for it for use with
   * WebRender. This must be called from the main thread.
   */
  static nsresult Share(gfx::SourceSurface* aSurface, wr::ExternalImageId& aId);

  /**
   * Request that the surface be mapped into the compositor thread's memory
   * space, and a valid ExternalImageId be generated for it for use with
   * WebRender. This must be called from the main thread.
   */
  static nsresult Share(gfx::SourceSurface* aSurface,
                        Maybe<SurfaceDescriptor>& aDesc);

  /**
   * Request that the surface be mapped into the compositor thread's memory
   * space, and a valid ImageKey be generated for it for use with WebRender.
   * This must be called from the main thread.
   */
  static nsresult Share(gfx::SourceSurfaceSharedData* aSurface,
                        RenderRootStateManager* aManager,
                        wr::IpcResourceUpdateQueue& aResources,
                        wr::ImageKey& aKey);

  /**
   * Request that the surface be mapped into the compositor thread's memory
   * space, and a valid ImageKey be generated for it for use with WebRender.
   * This must be called from the main thread.
   */
  static nsresult Share(gfx::SourceSurface* aSurface,
                        RenderRootStateManager* aManager,
                        wr::IpcResourceUpdateQueue& aResources,
                        wr::ImageKey& aKey);

  /**
   * Get the external ID, if any, bound to the shared surface. Used for memory
   * reporting purposes.
   */
  static Maybe<wr::ExternalImageId> GetExternalId(
      const gfx::SourceSurfaceSharedData* aSurface);

  /**
   * Get the surface (or its underlying surface) as a SourceSurfaceSharedData
   * pointer, if valid.
   */
  static gfx::SourceSurfaceSharedData* AsSourceSurfaceSharedData(
      gfx::SourceSurface* aSurface);

  class ImageKeyData {
   public:
    ImageKeyData(RenderRootStateManager* aManager,
                 const wr::ImageKey& aImageKey);
    virtual ~ImageKeyData();

    ImageKeyData(ImageKeyData&& aOther);
    ImageKeyData& operator=(ImageKeyData&& aOther);
    ImageKeyData(const ImageKeyData&) = delete;
    ImageKeyData& operator=(const ImageKeyData&) = delete;

    void MergeDirtyRect(const Maybe<gfx::IntRect>& aDirtyRect);

    Maybe<gfx::IntRect> TakeDirtyRect() { return std::move(mDirtyRect); }

    RefPtr<RenderRootStateManager> mManager;
    Maybe<gfx::IntRect> mDirtyRect;
    wr::ImageKey mImageKey;
  };

 private:
  SharedSurfacesChild() = delete;
  ~SharedSurfacesChild() = delete;

  friend class SharedSurfacesAnimation;

  class SharedUserData final : public Runnable {
   public:
    SharedUserData();
    virtual ~SharedUserData();

    SharedUserData(const SharedUserData& aOther) = delete;
    SharedUserData& operator=(const SharedUserData& aOther) = delete;

    SharedUserData(SharedUserData&& aOther) = delete;
    SharedUserData& operator=(SharedUserData&& aOther) = delete;

    static void Destroy(void* aClosure);

    NS_IMETHOD Run() override;

    const wr::ExternalImageId& Id() const { return mId; }

    void ClearShared() {
      mKeys.Clear();
      mShared = false;
    }

    bool IsShared() const { return mShared; }

    void MarkShared(const wr::ExternalImageId& aId) {
      MOZ_ASSERT(!mShared);
      mId = aId;
      mShared = true;
    }

    wr::ImageKey UpdateKey(RenderRootStateManager* aManager,
                           wr::IpcResourceUpdateQueue& aResources,
                           const Maybe<gfx::IntRect>& aDirtyRect);

   protected:
    AutoTArray<ImageKeyData, 1> mKeys;
    wr::ExternalImageId mId;
    bool mShared : 1;
  };

  static nsresult ShareInternal(gfx::SourceSurfaceSharedData* aSurface,
                                SharedUserData** aUserData);

  static void Unshare(const wr::ExternalImageId& aId, bool aReleaseId,
                      nsTArray<ImageKeyData>& aKeys);

  static void DestroySharedUserData(void* aClosure);

  static gfx::UserDataKey sSharedKey;
};

class AnimationImageKeyData final : public SharedSurfacesChild::ImageKeyData {
 public:
  AnimationImageKeyData(RenderRootStateManager* aManager,
                        const wr::ImageKey& aImageKey);

  virtual ~AnimationImageKeyData();

  AnimationImageKeyData(AnimationImageKeyData&& aOther);
  AnimationImageKeyData& operator=(AnimationImageKeyData&& aOther);

  AutoTArray<RefPtr<gfx::SourceSurfaceSharedData>, 2> mPendingRelease;
};

/**
 * This helper class owns a single ImageKey which will map to different external
 * image IDs representing different frames in an animation.
 */
class SharedSurfacesAnimation final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedSurfacesAnimation)

  SharedSurfacesAnimation() = default;

  void Destroy();

  /**
   * Set the animation to display the given frame.
   * @param aSurface    The current frame.
   * @param aDirtyRect  Dirty rect representing the change between the new frame
   *                    and the previous frame. We will request only the delta
   *                    be reuploaded by WebRender.
   */
  nsresult SetCurrentFrame(gfx::SourceSurfaceSharedData* aSurface,
                           const gfx::IntRect& aDirtyRect);

  /**
   * Generate an ImageKey for the given frame.
   * @param aSurface  The current frame. This should match what was cached via
   *                  SetCurrentFrame, but if it does not, it will need to
   *                  regenerate the cached ImageKey.
   */
  nsresult UpdateKey(gfx::SourceSurfaceSharedData* aSurface,
                     RenderRootStateManager* aManager,
                     wr::IpcResourceUpdateQueue& aResources,
                     wr::ImageKey& aKey);

  /**
   * Release our reference to all frames up to and including the frame which
   * has an external image ID which matches aId.
   */
  void ReleasePreviousFrame(RenderRootStateManager* aManager,
                            const wr::ExternalImageId& aId);

  /**
   * Destroy any state information bound for the given layer manager. Any
   * image keys are already invalid.
   */
  void Invalidate(RenderRootStateManager* aManager);

 private:
  ~SharedSurfacesAnimation();

  void HoldSurfaceForRecycling(AnimationImageKeyData& aEntry,
                               gfx::SourceSurfaceSharedData* aSurface);

  AutoTArray<AnimationImageKeyData, 1> mKeys;
  wr::ExternalImageId mId;
};

}  // namespace layers
}  // namespace mozilla

#endif
