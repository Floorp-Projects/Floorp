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
struct nsTArray_CopyChooser<mozilla::layers::AnimationImageKeyData> {
  typedef nsTArray_CopyWithConstructors<mozilla::layers::AnimationImageKeyData>
      Type;
};

namespace mozilla {
namespace gfx {
class SourceSurfaceSharedData;
}  // namespace gfx

namespace wr {
class IpcResourceUpdateQueue;
}  // namespace wr

namespace layers {

class CompositorManagerChild;
class ImageContainer;
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
   * Request that the first surface in the image container's current images be
   * mapped into the compositor thread's memory space, and a valid ImageKey be
   * generated for it for use with WebRender. If a different method should be
   * used to share the image data for this particular container, it will return
   * NS_ERROR_NOT_IMPLEMENTED. This must be called from the main thread.
   */
  static nsresult Share(ImageContainer* aContainer,
                        RenderRootStateManager* aManager,
                        wr::IpcResourceUpdateQueue& aResources,
                        wr::ImageKey& aKey, ContainerProducerID aProducerId);

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

  static nsresult UpdateAnimation(ImageContainer* aContainer,
                                  gfx::SourceSurface* aSurface,
                                  const gfx::IntRect& aDirtyRect);

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
    explicit SharedUserData(const wr::ExternalImageId& aId);
    virtual ~SharedUserData();

    SharedUserData(const SharedUserData& aOther) = delete;
    SharedUserData& operator=(const SharedUserData& aOther) = delete;

    SharedUserData(SharedUserData&& aOther) = delete;
    SharedUserData& operator=(SharedUserData&& aOther) = delete;

    static void Destroy(void* aClosure);

    NS_IMETHOD Run() override;

    const wr::ExternalImageId& Id() const { return mId; }

    void SetId(const wr::ExternalImageId& aId) {
      mId = aId;
      mKeys.Clear();
      mShared = false;
    }

    bool IsShared() const { return mShared; }

    void MarkShared() {
      MOZ_ASSERT(!mShared);
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

  AutoTArray<RefPtr<gfx::SourceSurface>, 2> mPendingRelease;
  bool mRecycling;
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
   * @param aParentSurface The owning surface of aSurface. This may be the same
   *                       or it may be a wrapper surface such as
   *                       RecyclingSourceSurface.
   * @param aSurface    The current frame.
   * @param aDirtyRect  Dirty rect representing the change between the new frame
   *                    and the previous frame. We will request only the delta
   *                    be reuploaded by WebRender.
   */
  nsresult SetCurrentFrame(gfx::SourceSurface* aParentSurface,
                           gfx::SourceSurfaceSharedData* aSurface,
                           const gfx::IntRect& aDirtyRect);

  /**
   * Generate an ImageKey for the given frame.
   * @param aParentSurface The owning surface of aSurface. This may be the same
   *                       or it may be a wrapper surface such as
   *                       RecyclingSourceSurface.
   * @param aSurface  The current frame. This should match what was cached via
   *                  SetCurrentFrame, but if it does not, it will need to
   *                  regenerate the cached ImageKey.
   */
  nsresult UpdateKey(gfx::SourceSurface* aParentSurface,
                     gfx::SourceSurfaceSharedData* aSurface,
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
                               gfx::SourceSurface* aParentSurface,
                               gfx::SourceSurfaceSharedData* aSurface);

  AutoTArray<AnimationImageKeyData, 1> mKeys;
  wr::ExternalImageId mId;
};

}  // namespace layers
}  // namespace mozilla

#endif
