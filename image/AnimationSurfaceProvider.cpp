/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationSurfaceProvider.h"

#include "gfxPrefs.h"
#include "nsProxyRelease.h"

#include "DecodePool.h"
#include "Decoder.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace image {

AnimationSurfaceProvider::AnimationSurfaceProvider(NotNull<RasterImage*> aImage,
                                                   const SurfaceKey& aSurfaceKey,
                                                   NotNull<Decoder*> aDecoder,
                                                   size_t aCurrentFrame)
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

  // We still produce paletted surfaces for GIF which means the frames are
  // smaller than one would expect for APNG. This may be removed if/when
  // bug 1337111 lands and it is enabled by default.
  size_t pixelSize = aDecoder->GetType() == DecoderType::GIF
                     ? sizeof(uint8_t) : sizeof(uint32_t);

  // Calculate how many frames we need to decode in this animation before we
  // enter decode-on-demand mode.
  IntSize frameSize = aSurfaceKey.Size();
  size_t threshold =
    (size_t(gfxPrefs::ImageAnimatedDecodeOnDemandThresholdKB()) * 1024) /
    (pixelSize * frameSize.width * frameSize.height);
  size_t batch = gfxPrefs::ImageAnimatedDecodeOnDemandBatchSize();

  mFrames.Initialize(threshold, batch, aCurrentFrame);
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

  // RasterImage objects need to be destroyed on the main thread.
  NS_ReleaseOnMainThreadSystemGroup("AnimationSurfaceProvider::mImage",
                                    mImage.forget());
}

void
AnimationSurfaceProvider::Reset()
{
  // We want to go back to the beginning.
  bool mayDiscard;
  bool restartDecoder;

  {
    MutexAutoLock lock(mFramesMutex);

    // If we have not crossed the threshold, we know we haven't discarded any
    // frames, and thus we know it is safe move our display index back to the
    // very beginning. It would be cleaner to let the frame buffer make this
    // decision inside the AnimationFrameBuffer::Reset method, but if we have
    // crossed the threshold, we need to hold onto the decoding mutex too. We
    // should avoid blocking the main thread on the decoder threads.
    mayDiscard = mFrames.MayDiscard();
    if (!mayDiscard) {
      restartDecoder = mFrames.Reset();
    }
  }

  if (mayDiscard) {
    // We are over the threshold and have started discarding old frames. In
    // that case we need to seize the decoding mutex. Thankfully we know that
    // we are in the process of decoding at most the batch size frames, so
    // this should not take too long to acquire.
    MutexAutoLock lock(mDecodingMutex);

    // Recreate the decoder so we can regenerate the frames again.
    mDecoder = DecoderFactory::CloneAnimationDecoder(mDecoder);
    MOZ_ASSERT(mDecoder);

    MutexAutoLock lock2(mFramesMutex);
    restartDecoder = mFrames.Reset();
  }

  if (restartDecoder) {
    DecodePool::Singleton()->AsyncRun(this);
  }
}

void
AnimationSurfaceProvider::Advance(size_t aFrame)
{
  bool restartDecoder;

  {
    // Typical advancement of a frame.
    MutexAutoLock lock(mFramesMutex);
    restartDecoder = mFrames.AdvanceTo(aFrame);
  }

  if (restartDecoder) {
    DecodePool::Singleton()->AsyncRun(this);
  }
}

DrawableFrameRef
AnimationSurfaceProvider::DrawableRef(size_t aFrame)
{
  MutexAutoLock lock(mFramesMutex);

  if (Availability().IsPlaceholder()) {
    MOZ_ASSERT_UNREACHABLE("Calling DrawableRef() on a placeholder");
    return DrawableFrameRef();
  }

  return mFrames.Get(aFrame);
}

bool
AnimationSurfaceProvider::IsFinished() const
{
  MutexAutoLock lock(mFramesMutex);

  if (Availability().IsPlaceholder()) {
    MOZ_ASSERT_UNREACHABLE("Calling IsFinished() on a placeholder");
    return false;
  }

  if (mFrames.Frames().IsEmpty()) {
    MOZ_ASSERT_UNREACHABLE("Calling IsFinished() when we have no frames");
    return false;
  }

  // As long as we have at least one finished frame, we're finished.
  return mFrames.Frames()[0]->IsFinished();
}

bool
AnimationSurfaceProvider::IsFullyDecoded() const
{
  MutexAutoLock lock(mFramesMutex);
  return mFrames.SizeKnown() && !mFrames.MayDiscard();
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
                                                 size_t& aNonHeapSizeOut,
                                                 size_t& aExtHandlesOut)
{
  // Note that the surface cache lock is already held here, and then we acquire
  // mFramesMutex. For this method, this ordering is unavoidable, which means
  // that we must be careful to always use the same ordering elsewhere.
  MutexAutoLock lock(mFramesMutex);

  for (const RawAccessFrameRef& frame : mFrames.Frames()) {
    if (frame) {
      frame->AddSizeOfExcludingThis(aMallocSizeOf, aHeapSizeOut,
                                    aNonHeapSizeOut, aExtHandlesOut);
    }
  }
}

void
AnimationSurfaceProvider::Run()
{
  MutexAutoLock lock(mDecodingMutex);

  if (!mDecoder) {
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
      bool continueDecoding = CheckForNewFrameAtTerminalState();
      FinishDecoding();

      // Even if it is the last frame, we may not have enough frames buffered
      // ahead of the current. If we are shutting down, we want to ensure we
      // release the thread as soon as possible. The animation may advance even
      // during shutdown, which keeps us decoding, and thus blocking the decode
      // pool during teardown.
      if (!mDecoder || !continueDecoding ||
          DecodePool::Singleton()->IsShuttingDown()) {
        return;
      }

      // Restart from the very beginning because the decoder was recreated.
      continue;
    }

    // Notify for the progress we've made so far.
    if (mImage && mDecoder->HasProgress()) {
      NotifyProgress(WrapNotNull(mImage), WrapNotNull(mDecoder));
    }

    if (result == LexerResult(Yield::NEED_MORE_DATA)) {
      // We can't make any more progress right now. The decoder itself will ensure
      // that we get reenqueued when more data is available; just return for now.
      return;
    }

    // There's new output available - a new frame! Grab it. If we don't need any
    // more for the moment we can break out of the loop. If we are shutting
    // down, we want to ensure we release the thread as soon as possible. The
    // animation may advance even during shutdown, which keeps us decoding, and
    // thus blocking the decode pool during teardown.
    MOZ_ASSERT(result == LexerResult(Yield::OUTPUT_AVAILABLE));
    if (!CheckForNewFrameAtYield() ||
        DecodePool::Singleton()->IsShuttingDown()) {
      return;
    }
  }
}

bool
AnimationSurfaceProvider::CheckForNewFrameAtYield()
{
  mDecodingMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(mDecoder);

  bool justGotFirstFrame = false;
  bool continueDecoding;

  {
    MutexAutoLock lock(mFramesMutex);

    // Try to get the new frame from the decoder.
    RawAccessFrameRef frame = mDecoder->GetCurrentFrameRef();
    MOZ_ASSERT(mDecoder->HasFrameToTake());
    mDecoder->ClearHasFrameToTake();

    if (!frame) {
      MOZ_ASSERT_UNREACHABLE("Decoder yielded but didn't produce a frame?");
      return true;
    }

    // We should've gotten a different frame than last time.
    MOZ_ASSERT_IF(!mFrames.Frames().IsEmpty(),
                  mFrames.Frames().LastElement().get() != frame.get());

    // Append the new frame to the list.
    continueDecoding = mFrames.Insert(Move(frame));

    // We only want to handle the first frame if it is the first pass for the
    // animation decoder. The owning image will be cleared after that.
    size_t frameCount = mFrames.Frames().Length();
    if (frameCount == 1 && mImage) {
      justGotFirstFrame = true;
    }
  }

  if (justGotFirstFrame) {
    AnnounceSurfaceAvailable();
  }

  return continueDecoding;
}

bool
AnimationSurfaceProvider::CheckForNewFrameAtTerminalState()
{
  mDecodingMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(mDecoder);

  bool justGotFirstFrame = false;
  bool continueDecoding;

  {
    MutexAutoLock lock(mFramesMutex);

    // The decoder may or may not have a new frame for us at this point. Avoid
    // reinserting the same frame again.
    RawAccessFrameRef frame = mDecoder->GetCurrentFrameRef();

    // If the decoder didn't finish a new frame (ie if, after starting the
    // frame, it got an error and aborted the frame and the rest of the decode)
    // that means it won't be reporting it to the image or FrameAnimator so we
    // should ignore it too, that's what HasFrameToTake tracks basically.
    if (!mDecoder->HasFrameToTake()) {
      frame = RawAccessFrameRef();
    } else {
      MOZ_ASSERT(frame);
      mDecoder->ClearHasFrameToTake();
    }

    if (!frame || (!mFrames.Frames().IsEmpty() &&
                   mFrames.Frames().LastElement().get() == frame.get())) {
      return mFrames.MarkComplete();
    }

    // Append the new frame to the list.
    mFrames.Insert(Move(frame));
    continueDecoding = mFrames.MarkComplete();

    // We only want to handle the first frame if it is the first pass for the
    // animation decoder. The owning image will be cleared after that.
    if (mFrames.Frames().Length() == 1 && mImage) {
      justGotFirstFrame = true;
    }
  }

  if (justGotFirstFrame) {
    AnnounceSurfaceAvailable();
  }

  return continueDecoding;
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
  MOZ_ASSERT(mDecoder);

  if (mImage) {
    // Send notifications.
    NotifyDecodeComplete(WrapNotNull(mImage), WrapNotNull(mDecoder));
  }

  // Determine if we need to recreate the decoder, in case we are discarding
  // frames and need to loop back to the beginning.
  bool recreateDecoder;
  {
    MutexAutoLock lock(mFramesMutex);
    recreateDecoder = !mFrames.HasRedecodeError() && mFrames.MayDiscard();
  }

  if (recreateDecoder) {
    mDecoder = DecoderFactory::CloneAnimationDecoder(mDecoder);
    MOZ_ASSERT(mDecoder);
  } else {
    mDecoder = nullptr;
  }

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
