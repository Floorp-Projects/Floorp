/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDecodingTask.h"

#include "nsThreadUtils.h"

#include "Decoder.h"
#include "DecodePool.h"
#include "RasterImage.h"
#include "SurfaceCache.h"

namespace mozilla {

using gfx::IntRect;

namespace image {

///////////////////////////////////////////////////////////////////////////////
// Helpers for sending notifications to the image associated with a decoder.
///////////////////////////////////////////////////////////////////////////////

void IDecodingTask::EnsureHasEventTarget(NotNull<RasterImage*> aImage) {
  if (!mEventTarget) {
    // We determine the event target as late as possible, at the first dispatch
    // time, because the observers bound to an imgRequest will affect it.
    // We cache it rather than query for the event target each time because the
    // event target can change. We don't want to risk events being executed in
    // a different order than they are dispatched, which can happen if we
    // selected scheduler groups which have no ordering guarantees relative to
    // each other (e.g. it moves from scheduler group A for doc group DA to
    // scheduler group B for doc group DB due to changing observers -- if we
    // dispatched the first event on A, and the second on B, we don't know which
    // will execute first.)
    RefPtr<ProgressTracker> tracker = aImage->GetProgressTracker();
    if (tracker) {
      mEventTarget = tracker->GetEventTarget();
    } else {
      mEventTarget = GetMainThreadSerialEventTarget();
    }
  }
}

bool IDecodingTask::IsOnEventTarget() const {
  // This is essentially equivalent to NS_IsOnMainThread() because all of the
  // event targets are for the main thread (although perhaps with a different
  // label / scheduler group). The observers in ProgressTracker may have
  // different event targets from this, so this is just a best effort guess.
  bool current = false;
  mEventTarget->IsOnCurrentThread(&current);
  return current;
}

void IDecodingTask::NotifyProgress(NotNull<RasterImage*> aImage,
                                   NotNull<Decoder*> aDecoder) {
  MOZ_ASSERT(aDecoder->HasProgress() && !aDecoder->IsMetadataDecode());
  EnsureHasEventTarget(aImage);

  // Capture the decoder's state. If we need to notify asynchronously, it's
  // important that we don't wait until the lambda actually runs to capture the
  // state that we're going to notify. That would both introduce data races on
  // the decoder's state and cause inconsistencies between the NotifyProgress()
  // calls we make off-main-thread and the notifications that RasterImage
  // actually receives, which would cause bugs.
  Progress progress = aDecoder->TakeProgress();
  UnorientedIntRect invalidRect =
      UnorientedIntRect::FromUnknownRect(aDecoder->TakeInvalidRect());
  Maybe<uint32_t> frameCount = aDecoder->TakeCompleteFrameCount();
  DecoderFlags decoderFlags = aDecoder->GetDecoderFlags();
  SurfaceFlags surfaceFlags = aDecoder->GetSurfaceFlags();

  // Synchronously notify if we can.
  if (IsOnEventTarget() && !(decoderFlags & DecoderFlags::ASYNC_NOTIFY)) {
    aImage->NotifyProgress(progress, invalidRect, frameCount, decoderFlags,
                           surfaceFlags);
    return;
  }

  // We're forced to notify asynchronously.
  NotNull<RefPtr<RasterImage>> image = aImage;
  mEventTarget->Dispatch(CreateMediumHighRunnable(NS_NewRunnableFunction(
                             "IDecodingTask::NotifyProgress",
                             [=]() -> void {
                               image->NotifyProgress(progress, invalidRect,
                                                     frameCount, decoderFlags,
                                                     surfaceFlags);
                             })),
                         NS_DISPATCH_NORMAL);
}

void IDecodingTask::NotifyDecodeComplete(NotNull<RasterImage*> aImage,
                                         NotNull<Decoder*> aDecoder) {
  MOZ_ASSERT(aDecoder->HasError() || !aDecoder->InFrame(),
             "Decode complete in the middle of a frame?");
  EnsureHasEventTarget(aImage);

  // Capture the decoder's state.
  DecoderFinalStatus finalStatus = aDecoder->FinalStatus();
  ImageMetadata metadata = aDecoder->GetImageMetadata();
  DecoderTelemetry telemetry = aDecoder->Telemetry();
  Progress progress = aDecoder->TakeProgress();
  UnorientedIntRect invalidRect =
      UnorientedIntRect::FromUnknownRect(aDecoder->TakeInvalidRect());
  Maybe<uint32_t> frameCount = aDecoder->TakeCompleteFrameCount();
  DecoderFlags decoderFlags = aDecoder->GetDecoderFlags();
  SurfaceFlags surfaceFlags = aDecoder->GetSurfaceFlags();

  // Synchronously notify if we can.
  if (IsOnEventTarget() && !(decoderFlags & DecoderFlags::ASYNC_NOTIFY)) {
    aImage->NotifyDecodeComplete(finalStatus, metadata, telemetry, progress,
                                 invalidRect, frameCount, decoderFlags,
                                 surfaceFlags);
    return;
  }

  // We're forced to notify asynchronously.
  NotNull<RefPtr<RasterImage>> image = aImage;
  mEventTarget->Dispatch(CreateMediumHighRunnable(NS_NewRunnableFunction(
                             "IDecodingTask::NotifyDecodeComplete",
                             [=]() -> void {
                               image->NotifyDecodeComplete(
                                   finalStatus, metadata, telemetry, progress,
                                   invalidRect, frameCount, decoderFlags,
                                   surfaceFlags);
                             })),
                         NS_DISPATCH_NORMAL);
}

///////////////////////////////////////////////////////////////////////////////
// IDecodingTask implementation.
///////////////////////////////////////////////////////////////////////////////

void IDecodingTask::Resume() { DecodePool::Singleton()->AsyncRun(this); }

///////////////////////////////////////////////////////////////////////////////
// MetadataDecodingTask implementation.
///////////////////////////////////////////////////////////////////////////////

MetadataDecodingTask::MetadataDecodingTask(NotNull<Decoder*> aDecoder)
    : mMutex("mozilla::image::MetadataDecodingTask"), mDecoder(aDecoder) {
  MOZ_ASSERT(mDecoder->IsMetadataDecode(),
             "Use DecodingTask for non-metadata decodes");
}

void MetadataDecodingTask::Run() {
  MutexAutoLock lock(mMutex);

  LexerResult result = mDecoder->Decode(WrapNotNull(this));

  if (result.is<TerminalState>()) {
    NotifyDecodeComplete(mDecoder->GetImage(), mDecoder);
    return;  // We're done.
  }

  if (result == LexerResult(Yield::NEED_MORE_DATA)) {
    // We can't make any more progress right now. We also don't want to report
    // any progress, because it's important that metadata decode results are
    // delivered atomically. The decoder itself will ensure that we get
    // reenqueued when more data is available; just return for now.
    return;
  }

  MOZ_ASSERT_UNREACHABLE("Metadata decode yielded for an unexpected reason");
}

///////////////////////////////////////////////////////////////////////////////
// AnonymousDecodingTask implementation.
///////////////////////////////////////////////////////////////////////////////

AnonymousDecodingTask::AnonymousDecodingTask(NotNull<Decoder*> aDecoder,
                                             bool aResumable)
    : mDecoder(aDecoder), mResumable(aResumable) {}

void AnonymousDecodingTask::Run() {
  while (true) {
    LexerResult result = mDecoder->Decode(WrapNotNull(this));

    if (result.is<TerminalState>()) {
      return;  // We're done.
    }

    if (result == LexerResult(Yield::NEED_MORE_DATA)) {
      // We can't make any more progress right now. Let the caller decide how to
      // handle it.
      return;
    }

    // Right now we don't do anything special for other kinds of yields, so just
    // keep working.
    MOZ_ASSERT(result.is<Yield>());
  }
}

void AnonymousDecodingTask::Resume() {
  // Anonymous decoders normally get all their data at once. We have tests
  // where they don't; typically in these situations, the test re-runs them
  // manually. However some tests want to verify Resume works, so they will
  // explicitly request this behaviour.
  if (mResumable) {
    RefPtr<AnonymousDecodingTask> self(this);
    NS_DispatchToMainThread(
        NS_NewRunnableFunction("image::AnonymousDecodingTask::Resume",
                               [self]() -> void { self->Run(); }));
  }
}

}  // namespace image
}  // namespace mozilla
