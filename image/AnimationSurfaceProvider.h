/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * An ISurfaceProvider for animated images.
 */

#ifndef mozilla_image_AnimationSurfaceProvider_h
#define mozilla_image_AnimationSurfaceProvider_h

#include "mozilla/UniquePtr.h"

#include "Decoder.h"
#include "FrameAnimator.h"
#include "IDecodingTask.h"
#include "ISurfaceProvider.h"
#include "AnimationFrameBuffer.h"

namespace mozilla {
namespace layers {
class SharedSurfacesAnimation;
}

namespace image {

/**
 * An ISurfaceProvider that manages the decoding of animated images and
 * dynamically generates surfaces for the current playback state of the
 * animation.
 */
class AnimationSurfaceProvider final : public ISurfaceProvider,
                                       public IDecodingTask,
                                       public IDecoderFrameRecycler {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AnimationSurfaceProvider, override)

  AnimationSurfaceProvider(NotNull<RasterImage*> aImage,
                           const SurfaceKey& aSurfaceKey,
                           NotNull<Decoder*> aDecoder, size_t aCurrentFrame);

  //////////////////////////////////////////////////////////////////////////////
  // ISurfaceProvider implementation.
  //////////////////////////////////////////////////////////////////////////////

 public:
  bool IsFinished() const override;
  bool IsFullyDecoded() const override;
  size_t LogicalSizeInBytes() const override;
  void AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                              const AddSizeOfCb& aCallback) override;
  void Reset() override;
  void Advance(size_t aFrame) override;
  bool MayAdvance() const override { return mCompositedFrameRequested; }
  void MarkMayAdvance() override { mCompositedFrameRequested = true; }

 protected:
  DrawableFrameRef DrawableRef(size_t aFrame) override;
  already_AddRefed<imgFrame> GetFrame(size_t aFrame) override;

  // Animation frames are always locked. This is because we only want to release
  // their memory atomically (due to the surface cache discarding them). If they
  // were unlocked, the OS could end up releasing the memory of random frames
  // from the middle of the animation, which is not worth the complexity of
  // dealing with.
  bool IsLocked() const override { return true; }
  void SetLocked(bool) override {}

  //////////////////////////////////////////////////////////////////////////////
  // IDecodingTask implementation.
  //////////////////////////////////////////////////////////////////////////////

 public:
  void Run() override;
  bool ShouldPreferSyncRun() const override;

  // Full decodes are low priority compared to metadata decodes because they
  // don't block layout or page load.
  TaskPriority Priority() const override { return TaskPriority::eLow; }

  //////////////////////////////////////////////////////////////////////////////
  // IDecoderFrameRecycler implementation.
  //////////////////////////////////////////////////////////////////////////////

 public:
  RawAccessFrameRef RecycleFrame(gfx::IntRect& aRecycleRect) override;

  //////////////////////////////////////////////////////////////////////////////
  // IDecoderFrameRecycler implementation.
  //////////////////////////////////////////////////////////////////////////////

 public:
  nsresult UpdateKey(layers::RenderRootStateManager* aManager,
                     wr::IpcResourceUpdateQueue& aResources,
                     wr::ImageKey& aKey) override;

 private:
  virtual ~AnimationSurfaceProvider();

  void DropImageReference();
  void AnnounceSurfaceAvailable();
  void FinishDecoding();
  void RequestFrameDiscarding();

  // @returns Whether or not we should continue decoding.
  bool CheckForNewFrameAtYield();

  // @returns Whether or not we should restart decoding.
  bool CheckForNewFrameAtTerminalState();

  /// The image associated with our decoder.
  RefPtr<RasterImage> mImage;

  /// A mutex to protect mDecoder. Always taken before mFramesMutex.
  mutable Mutex mDecodingMutex MOZ_UNANNOTATED;

  /// The decoder used to decode this animation.
  RefPtr<Decoder> mDecoder;

  /// A mutex to protect mFrames. Always taken after mDecodingMutex.
  mutable Mutex mFramesMutex MOZ_UNANNOTATED;

  /// The frames of this animation, in order.
  UniquePtr<AnimationFrameBuffer> mFrames;

  /// Whether the current frame was requested for display since the last time we
  /// advanced the animation.
  bool mCompositedFrameRequested;

  ///
  RefPtr<layers::SharedSurfacesAnimation> mSharedAnimation;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_AnimationSurfaceProvider_h
