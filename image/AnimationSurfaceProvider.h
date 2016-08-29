/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * An ISurfaceProvider for animated images.
 */

#ifndef mozilla_image_AnimationSurfaceProvider_h
#define mozilla_image_AnimationSurfaceProvider_h

#include "FrameAnimator.h"
#include "IDecodingTask.h"
#include "ISurfaceProvider.h"

namespace mozilla {
namespace image {

/**
 * An ISurfaceProvider that manages the decoding of animated images and
 * dynamically generates surfaces for the current playback state of the
 * animation.
 */
class AnimationSurfaceProvider final
  : public ISurfaceProvider
  , public IDecodingTask
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AnimationSurfaceProvider, override)

  AnimationSurfaceProvider(NotNull<RasterImage*> aImage,
                           const SurfaceKey& aSurfaceKey,
                           NotNull<Decoder*> aDecoder);


  //////////////////////////////////////////////////////////////////////////////
  // ISurfaceProvider implementation.
  //////////////////////////////////////////////////////////////////////////////

public:
  // We use the ISurfaceProvider constructor of DrawableSurface to indicate that
  // our surfaces are computed lazily.
  DrawableSurface Surface() override { return DrawableSurface(WrapNotNull(this)); }

  bool IsFinished() const override;
  size_t LogicalSizeInBytes() const override;
  void AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                              size_t& aHeapSizeOut,
                              size_t& aNonHeapSizeOut) override;

protected:
  DrawableFrameRef DrawableRef(size_t aFrame) override;

  // Animation frames are always locked. This is because we only want to release
  // their memory atomically (due to the surface cache discarding them). If they
  // were unlocked, the OS could end up releasing the memory of random frames
  // from the middle of the animation, which is not worth the complexity of
  // dealing with.
  bool IsLocked() const override { return true; }
  void SetLocked(bool) override { }


  //////////////////////////////////////////////////////////////////////////////
  // IDecodingTask implementation.
  //////////////////////////////////////////////////////////////////////////////

public:
  void Run() override;
  bool ShouldPreferSyncRun() const override;

  // Full decodes are low priority compared to metadata decodes because they
  // don't block layout or page load.
  TaskPriority Priority() const override { return TaskPriority::eLow; }

private:
  virtual ~AnimationSurfaceProvider();

  void DropImageReference();
  void CheckForNewFrameAtYield();
  void CheckForNewFrameAtTerminalState();
  void AnnounceSurfaceAvailable();
  void FinishDecoding();

  /// The image associated with our decoder.
  RefPtr<RasterImage> mImage;

  /// A mutex to protect mDecoder. Always taken before mFramesMutex.
  mutable Mutex mDecodingMutex;

  /// The decoder used to decode this animation.
  RefPtr<Decoder> mDecoder;

  /// A mutex to protect mFrames. Always taken after mDecodingMutex.
  mutable Mutex mFramesMutex;

  /// The frames of this animation, in order.
  nsTArray<RawAccessFrameRef> mFrames;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_AnimationSurfaceProvider_h
