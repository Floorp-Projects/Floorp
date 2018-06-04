/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationFrameBuffer.h"
#include "mozilla/Move.h"             // for Move

namespace mozilla {
namespace image {

AnimationFrameRetainedBuffer::AnimationFrameRetainedBuffer(size_t aThreshold,
                                                           size_t aBatch,
                                                           size_t aStartFrame)
  : AnimationFrameBuffer(aBatch, aStartFrame)
  , mThreshold(aThreshold)
{
  // To simplify the code, we have the assumption that the threshold for
  // entering discard-after-display mode is at least twice the batch size (since
  // that is the most frames-pending-decode we will request) + 1 for the current
  // frame. That way the redecoded frames being inserted will never risk
  // overlapping the frames we will discard due to the animation progressing.
  // That may cause us to use a little more memory than we want but that is an
  // acceptable tradeoff for simplicity.
  size_t minThreshold = 2 * mBatch + 1;
  if (mThreshold < minThreshold) {
    mThreshold = minThreshold;
  }

  // The maximum number of frames we should ever have decoded at one time is
  // twice the batch. That is a good as number as any to start our decoding at.
  mPending = mBatch * 2;
}

bool
AnimationFrameRetainedBuffer::InsertInternal(RefPtr<imgFrame>&& aFrame)
{
  // We should only insert new frames if we actually asked for them.
  MOZ_ASSERT(!mSizeKnown);
  MOZ_ASSERT(mFrames.Length() < mThreshold);

  mFrames.AppendElement(std::move(aFrame));
  MOZ_ASSERT(mSize == mFrames.Length());
  return mSize < mThreshold;
}

bool
AnimationFrameRetainedBuffer::ResetInternal()
{
  // If we haven't crossed the threshold, then we know by definition we have
  // not discarded any frames. If we previously requested more frames, but
  // it would have been more than we would have buffered otherwise, we can
  // stop the decoding after one more frame.
  if (mPending > 1 && mSize >= mBatch * 2 + 1) {
    MOZ_ASSERT(!mSizeKnown);
    mPending = 1;
  }

  // Either the decoder is still running, or we have enough frames already.
  // No need for us to restart it.
  return false;
}

bool
AnimationFrameRetainedBuffer::MarkComplete(const gfx::IntRect& aFirstFrameRefreshArea)
{
  MOZ_ASSERT(!mSizeKnown);
  mSizeKnown = true;
  mPending = 0;
  mFrames.Compact();
  return false;
}

void
AnimationFrameRetainedBuffer::AdvanceInternal()
{
  // We should not have advanced if we never inserted.
  MOZ_ASSERT(!mFrames.IsEmpty());
  // We only want to change the current frame index if we have advanced. This
  // means either a higher frame index, or going back to the beginning.
  size_t framesLength = mFrames.Length();
  // We should never have advanced beyond the frame buffer.
  MOZ_ASSERT(mGetIndex < framesLength);
  // We should never advance if the current frame is null -- it needs to know
  // the timeout from it at least to know when to advance.
  MOZ_ASSERT_IF(mGetIndex > 0, mFrames[mGetIndex - 1]);
  MOZ_ASSERT_IF(mGetIndex == 0, mFrames[framesLength - 1]);
  // The owner should have already accessed the next frame, so it should also
  // be available.
  MOZ_ASSERT(mFrames[mGetIndex]);

  if (!mSizeKnown) {
    // Calculate how many frames we have requested ahead of the current frame.
    size_t buffered = mPending + framesLength - mGetIndex - 1;
    if (buffered < mBatch) {
      // If we have fewer frames than the batch size, then ask for more. If we
      // do not have any pending, then we know that there is no active decoding.
      mPending += mBatch;
    }
  }
}

imgFrame*
AnimationFrameRetainedBuffer::Get(size_t aFrame, bool aForDisplay)
{
  // We should not have asked for a frame if we never inserted.
  if (mFrames.IsEmpty()) {
    MOZ_ASSERT_UNREACHABLE("Calling Get() when we have no frames");
    return nullptr;
  }

  // If we don't have that frame, return an empty frame ref.
  if (aFrame >= mFrames.Length()) {
    return nullptr;
  }

  // If we have space for the frame, it should always be available.
  if (!mFrames[aFrame]) {
    MOZ_ASSERT_UNREACHABLE("Calling Get() when frame is unavailable");
    return nullptr;
  }

  // If we are advancing on behalf of the animation, we don't expect it to be
  // getting any frames (besides the first) until we get the desired frame.
  MOZ_ASSERT(aFrame == 0 || mAdvance == 0);
  return mFrames[aFrame].get();
}

bool
AnimationFrameRetainedBuffer::IsFirstFrameFinished() const
{
  return !mFrames.IsEmpty() && mFrames[0]->IsFinished();
}

bool
AnimationFrameRetainedBuffer::IsLastInsertedFrame(imgFrame* aFrame) const
{
  return !mFrames.IsEmpty() && mFrames.LastElement().get() == aFrame;
}

void
AnimationFrameRetainedBuffer::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                                     const AddSizeOfCb& aCallback)
{
  size_t i = 0;
  for (const RefPtr<imgFrame>& frame : mFrames) {
    ++i;
    frame->AddSizeOfExcludingThis(aMallocSizeOf,
      [&](AddSizeOfCbData& aMetadata) {
        aMetadata.index = i;
        aCallback(aMetadata);
      }
    );
  }
}

} // namespace image
} // namespace mozilla
