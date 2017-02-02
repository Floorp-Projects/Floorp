/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationSurfaceProvider.h"

#include "gfxPrefs.h"
#include "nsProxyRelease.h"

#include "Decoder.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace image {

AnimationSurfaceProvider::AnimationSurfaceProvider(NotNull<RasterImage*> aImage,
                                                   const SurfaceKey& aSurfaceKey,
                                                   NotNull<Decoder*> aDecoder)
  : ISurfaceProvider(ImageKey(aImage.get()), aSurfaceKey,
                     AvailabilityState::StartAsPlaceholder())
  , mImage(aImage.get())
  , mDecodingMutex("AnimationSurfaceProvider::mDecoder")
  , mDecoder(aDecoder.get())
  , mFramesMutex("AnimationSurfaceProvider::mFrames")
{
  MOZ_ASSERT(!mDecoder->IsMetadataDecode(),
             "Use MetadataDecodingTask for metadata decodes");
  MOZ_ASSERT(!mDecoder->IsFirstFrameDecode(),
             "Use DecodedSurfaceProvider for single-frame image decodes");
}

AnimationSurfaceProvider::~AnimationSurfaceProvider()
{
  DropImageReference();
}

void
AnimationSurfaceProvider::DropImageReference()
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
  NS_ReleaseOnMainThreadSystemGroup(image.forget(), /* aAlwaysProxy = */ true);
}

DrawableFrameRef
AnimationSurfaceProvider::DrawableRef(size_t aFrame)
{
  MutexAutoLock lock(mFramesMutex);

  if (Availability().IsPlaceholder()) {
    MOZ_ASSERT_UNREACHABLE("Calling DrawableRef() on a placeholder");
    return DrawableFrameRef();
  }

  if (mFrames.IsEmpty()) {
    MOZ_ASSERT_UNREACHABLE("Calling DrawableRef() when we have no frames");
    return DrawableFrameRef();
  }

  // If we don't have that frame, return an empty frame ref.
  if (aFrame >= mFrames.Length()) {
    return DrawableFrameRef();
  }

  // We've got the requested frame. Return it.
  MOZ_ASSERT(mFrames[aFrame]);
  return mFrames[aFrame]->DrawableRef();
}

bool
AnimationSurfaceProvider::IsFinished() const
{
  MutexAutoLock lock(mFramesMutex);

  if (Availability().IsPlaceholder()) {
    MOZ_ASSERT_UNREACHABLE("Calling IsFinished() on a placeholder");
    return false;
  }

  if (mFrames.IsEmpty()) {
    MOZ_ASSERT_UNREACHABLE("Calling IsFinished() when we have no frames");
    return false;
  }

  // As long as we have at least one finished frame, we're finished.
  return mFrames[0]->IsFinished();
}

size_t
AnimationSurfaceProvider::LogicalSizeInBytes() const
{
  // When decoding animated images, we need at most three live surfaces: the
  // composited surface, the previous composited surface for
  // DisposalMethod::RESTORE_PREVIOUS, and the surface we're currently decoding
  // into. The composited surfaces are always BGRA. Although the surface we're
  // decoding into may be paletted, and may be smaller than the real size of the
  // image, we assume the worst case here.
  // XXX(seth): Note that this is actually not accurate yet; we're storing the
  // full sequence of frames, not just the three live surfaces mentioned above.
  // Unfortunately there's no way to know in advance how many frames an
  // animation has, so we really can't do better here. This will become correct
  // once bug 1289954 is complete.
  IntSize size = GetSurfaceKey().Size();
  return 3 * size.width * size.height * sizeof(uint32_t);
}

void
AnimationSurfaceProvider::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                                 size_t& aHeapSizeOut,
                                                 size_t& aNonHeapSizeOut)
{
  // Note that the surface cache lock is already held here, and then we acquire
  // mFramesMutex. For this method, this ordering is unavoidable, which means
  // that we must be careful to always use the same ordering elsewhere.
  MutexAutoLock lock(mFramesMutex);

  for (const RawAccessFrameRef& frame : mFrames) {
    frame->AddSizeOfExcludingThis(aMallocSizeOf, aHeapSizeOut, aNonHeapSizeOut);
  }
}

void
AnimationSurfaceProvider::Run()
{
  MutexAutoLock lock(mDecodingMutex);

  if (!mDecoder || !mImage) {
    MOZ_ASSERT_UNREACHABLE("Running after decoding finished?");
    return;
  }

  while (true) {
    // Run the decoder.
    LexerResult result = mDecoder->Decode(WrapNotNull(this));

    if (result.is<TerminalState>()) {
      // We may have a new frame now, but it's not guaranteed - a decoding
      // failure or truncated data may mean that no new frame got produced.
      // Since we're not sure, rather than call CheckForNewFrameAtYield() here
      // we call CheckForNewFrameAtTerminalState(), which handles both of these
      // possibilities.
      CheckForNewFrameAtTerminalState();

      // We're done!
      FinishDecoding();
      return;
    }

    // Notify for the progress we've made so far.
    if (mDecoder->HasProgress()) {
      NotifyProgress(WrapNotNull(mImage), WrapNotNull(mDecoder));
    }

    if (result == LexerResult(Yield::NEED_MORE_DATA)) {
      // We can't make any more progress right now. The decoder itself will ensure
      // that we get reenqueued when more data is available; just return for now.
      return;
    }

    // There's new output available - a new frame! Grab it.
    MOZ_ASSERT(result == LexerResult(Yield::OUTPUT_AVAILABLE));
    CheckForNewFrameAtYield();
  }
}

void
AnimationSurfaceProvider::CheckForNewFrameAtYield()
{
  mDecodingMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(mDecoder);

  bool justGotFirstFrame = false;

  {
    MutexAutoLock lock(mFramesMutex);

    // Try to get the new frame from the decoder.
    RawAccessFrameRef frame = mDecoder->GetCurrentFrameRef();
    if (!frame) {
      MOZ_ASSERT_UNREACHABLE("Decoder yielded but didn't produce a frame?");
      return;
    }

    // We should've gotten a different frame than last time.
    MOZ_ASSERT_IF(!mFrames.IsEmpty(),
                  mFrames.LastElement().get() != frame.get());

    // Append the new frame to the list.
    mFrames.AppendElement(Move(frame));

    if (mFrames.Length() == 1) {
      justGotFirstFrame = true;
    }
  }

  if (justGotFirstFrame) {
    AnnounceSurfaceAvailable();
  }
}

void
AnimationSurfaceProvider::CheckForNewFrameAtTerminalState()
{
  mDecodingMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(mDecoder);

  bool justGotFirstFrame = false;

  {
    MutexAutoLock lock(mFramesMutex);

    RawAccessFrameRef frame = mDecoder->GetCurrentFrameRef();
    if (!frame) {
      return;
    }

    if (!mFrames.IsEmpty() && mFrames.LastElement().get() == frame.get()) {
      return;  // We already have this one.
    }

    // Append the new frame to the list.
    mFrames.AppendElement(Move(frame));

    if (mFrames.Length() == 1) {
      justGotFirstFrame = true;
    }
  }

  if (justGotFirstFrame) {
    AnnounceSurfaceAvailable();
  }
}

void
AnimationSurfaceProvider::AnnounceSurfaceAvailable()
{
  mFramesMutex.AssertNotCurrentThreadOwns();
  MOZ_ASSERT(mImage);

  // We just got the first frame; let the surface cache know. We deliberately do
  // this outside of mFramesMutex to avoid a potential deadlock with
  // AddSizeOfExcludingThis(), since otherwise we'd be acquiring mFramesMutex
  // and then the surface cache lock, while the memory reporting code would
  // acquire the surface cache lock and then mFramesMutex.
  SurfaceCache::SurfaceAvailable(WrapNotNull(this));
}

void
AnimationSurfaceProvider::FinishDecoding()
{
  mDecodingMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(mImage);
  MOZ_ASSERT(mDecoder);

  // Send notifications.
  NotifyDecodeComplete(WrapNotNull(mImage), WrapNotNull(mDecoder));

  // Destroy our decoder; we don't need it anymore.
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
AnimationSurfaceProvider::ShouldPreferSyncRun() const
{
  MutexAutoLock lock(mDecodingMutex);
  MOZ_ASSERT(mDecoder);

  return mDecoder->ShouldSyncDecode(gfxPrefs::ImageMemDecodeBytesAtATime());
}

} // namespace image
} // namespace mozilla
