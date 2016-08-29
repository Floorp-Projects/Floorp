/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecodedSurfaceProvider.h"

#include "gfxPrefs.h"
#include "nsProxyRelease.h"

#include "Decoder.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace image {

DecodedSurfaceProvider::DecodedSurfaceProvider(NotNull<RasterImage*> aImage,
                                               const SurfaceKey& aSurfaceKey,
                                               NotNull<Decoder*> aDecoder)
  : ISurfaceProvider(ImageKey(aImage.get()), aSurfaceKey,
                     AvailabilityState::StartAsPlaceholder())
  , mImage(aImage.get())
  , mMutex("mozilla::image::DecodedSurfaceProvider")
  , mDecoder(aDecoder.get())
{
  MOZ_ASSERT(!mDecoder->IsMetadataDecode(),
             "Use MetadataDecodingTask for metadata decodes");
  MOZ_ASSERT(mDecoder->IsFirstFrameDecode(),
             "Use AnimationSurfaceProvider for animation decodes");
}

DecodedSurfaceProvider::~DecodedSurfaceProvider()
{
  DropImageReference();
}

void
DecodedSurfaceProvider::DropImageReference()
{
  if (!mImage) {
    return;  // Nothing to do.
  }

  // RasterImage objects need to be destroyed on the main thread. We also need
  // to destroy them asynchronously, because if our surface cache entry is
  // destroyed and we were the only thing keeping |mImage| alive, RasterImage's
  // destructor may call into the surface cache while whatever code caused us to
  // get evicted is holding the surface cache lock, causing deadlock.
  RefPtr<RasterImage> image = mImage;
  mImage = nullptr;
  NS_ReleaseOnMainThread(image.forget(), /* aAlwaysProxy = */ true);
}

DrawableFrameRef
DecodedSurfaceProvider::DrawableRef(size_t aFrame)
{
  MOZ_ASSERT(aFrame == 0,
             "Requesting an animation frame from a DecodedSurfaceProvider?");

  // We depend on SurfaceCache::SurfaceAvailable() to provide synchronization
  // for methods that touch |mSurface|; after SurfaceAvailable() is called,
  // |mSurface| should be non-null and shouldn't be mutated further until we get
  // destroyed. That means that the assertions below are very important; we'll
  // end up with data races if these assumptions are violated.
  if (Availability().IsPlaceholder()) {
    MOZ_ASSERT_UNREACHABLE("Calling DrawableRef() on a placeholder");
    return DrawableFrameRef();
  }

  if (!mSurface) {
    MOZ_ASSERT_UNREACHABLE("Calling DrawableRef() when we have no surface");
    return DrawableFrameRef();
  }

  return mSurface->DrawableRef();
}

bool
DecodedSurfaceProvider::IsFinished() const
{
  // See DrawableRef() for commentary on these assertions.
  if (Availability().IsPlaceholder()) {
    MOZ_ASSERT_UNREACHABLE("Calling IsFinished() on a placeholder");
    return false;
  }

  if (!mSurface) {
    MOZ_ASSERT_UNREACHABLE("Calling IsFinished() when we have no surface");
    return false;
  }

  return mSurface->IsFinished();
}

void
DecodedSurfaceProvider::SetLocked(bool aLocked)
{
  // See DrawableRef() for commentary on these assertions.
  if (Availability().IsPlaceholder()) {
    MOZ_ASSERT_UNREACHABLE("Calling SetLocked() on a placeholder");
    return;
  }

  if (!mSurface) {
    MOZ_ASSERT_UNREACHABLE("Calling SetLocked() when we have no surface");
    return;
  }

  if (aLocked == IsLocked()) {
    return;  // Nothing to do.
  }

  // If we're locked, hold a DrawableFrameRef to |mSurface|, which will keep any
  // volatile buffer it owns in memory.
  mLockRef = aLocked ? mSurface->DrawableRef()
                     : DrawableFrameRef();
}

size_t
DecodedSurfaceProvider::LogicalSizeInBytes() const
{
  // Single frame images are always 32bpp.
  IntSize size = GetSurfaceKey().Size();
  return size.width * size.height * sizeof(uint32_t);
}

void
DecodedSurfaceProvider::Run()
{
  MutexAutoLock lock(mMutex);

  if (!mDecoder || !mImage) {
    MOZ_ASSERT_UNREACHABLE("Running after decoding finished?");
    return;
  }

  // Run the decoder.
  LexerResult result = mDecoder->Decode(WrapNotNull(this));

  // If there's a new surface available, announce it to the surface cache.
  CheckForNewSurface();

  if (result.is<TerminalState>()) {
    FinishDecoding();
    return;  // We're done.
  }

  // Notify for the progress we've made so far.
  if (mDecoder->HasProgress()) {
    NotifyProgress(WrapNotNull(mImage), WrapNotNull(mDecoder));
  }

  MOZ_ASSERT(result.is<Yield>());

  if (result == LexerResult(Yield::NEED_MORE_DATA)) {
    // We can't make any more progress right now. The decoder itself will ensure
    // that we get reenqueued when more data is available; just return for now.
    return;
  }

  // Single-frame images shouldn't yield for any reason except NEED_MORE_DATA.
  MOZ_ASSERT_UNREACHABLE("Unexpected yield for single-frame image");
  mDecoder->TerminateFailure();
  FinishDecoding();
}

void
DecodedSurfaceProvider::CheckForNewSurface()
{
  mMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(mDecoder);

  if (mSurface) {
    // Single-frame images should produce no more than one surface, so if we
    // have one, it should be the same one the decoder is working on.
    MOZ_ASSERT(mSurface.get() == mDecoder->GetCurrentFrameRef().get(),
               "DecodedSurfaceProvider and Decoder have different surfaces?");
    return;
  }

  // We don't have a surface yet; try to get one from the decoder.
  mSurface = mDecoder->GetCurrentFrameRef().get();
  if (!mSurface) {
    return;  // No surface yet.
  }

  // We just got a surface for the first time; let the surface cache know.
  MOZ_ASSERT(mImage);
  SurfaceCache::SurfaceAvailable(WrapNotNull(this));
}

void
DecodedSurfaceProvider::FinishDecoding()
{
  mMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(mImage);
  MOZ_ASSERT(mDecoder);

  // Send notifications.
  NotifyDecodeComplete(WrapNotNull(mImage), WrapNotNull(mDecoder));

  // Destroy our decoder; we don't need it anymore. (And if we don't destroy it,
  // our surface can never be optimized, because the decoder has a
  // RawAccessFrameRef to it.)
  mDecoder = nullptr;

  // We don't need a reference to our image anymore, either, and we don't want
  // one. We may be stored in the surface cache for a long time after decoding
  // finishes. If we don't drop our reference to the image, we'll end up
  // keeping it alive as long as we remain in the surface cache, which could
  // greatly extend the image's lifetime - in fact, if the image isn't
  // discardable, it'd result in a leak!
  DropImageReference();
}

bool
DecodedSurfaceProvider::ShouldPreferSyncRun() const
{
  return mDecoder->ShouldSyncDecode(gfxPrefs::ImageMemDecodeBytesAtATime());
}

} // namespace image
} // namespace mozilla
