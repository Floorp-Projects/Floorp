/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationFrameBuffer.h"
#include "mozilla/Move.h"             // for Move

namespace mozilla {
namespace image {

AnimationFrameBuffer::AnimationFrameBuffer()
  : mThreshold(0)
  , mBatch(0)
  , mPending(0)
  , mAdvance(0)
  , mInsertIndex(0)
  , mGetIndex(0)
  , mSizeKnown(false)
{ }

void
AnimationFrameBuffer::Initialize(size_t aThreshold,
                                 size_t aBatch,
                                 size_t aStartFrame)
{
  MOZ_ASSERT(mThreshold == 0);
  MOZ_ASSERT(mBatch == 0);
  MOZ_ASSERT(mPending == 0);
  MOZ_ASSERT(mAdvance == 0);
  MOZ_ASSERT(mFrames.IsEmpty());

  mThreshold = aThreshold;
  mBatch = aBatch;
  mAdvance = aStartFrame;

  if (mBatch > SIZE_MAX/4) {
    // Batch size is so big, we will just end up decoding the whole animation.
    mBatch = SIZE_MAX/4;
  } else if (mBatch < 1) {
    // Never permit a batch size smaller than 1. We always want to be asking for
    // at least one frame to start.
    mBatch = 1;
  }

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
AnimationFrameBuffer::Insert(RawAccessFrameRef&& aFrame)
{
  // We should only insert new frames if we actually asked for them.
  MOZ_ASSERT(mPending > 0);

  if (mSizeKnown) {
    // We only insert after the size is known if we are repeating the animation
    // and we did not keep all of the frames. Replace whatever is there
    // (probably an empty frame) with the new frame.
    MOZ_ASSERT(MayDiscard());
    MOZ_ASSERT(mInsertIndex < mFrames.Length());

    if (mInsertIndex > 0) {
      MOZ_ASSERT(!mFrames[mInsertIndex]);
      mFrames[mInsertIndex] = Move(aFrame);
    }
  } else if (mInsertIndex == mFrames.Length()) {
    // We are still on the first pass of the animation decoding, so this is
    // the first time we have seen this frame.
    mFrames.AppendElement(Move(aFrame));

    if (mInsertIndex == mThreshold) {
      // We just tripped over the threshold for the first time. This is our
      // chance to do any clearing of already displayed frames. After this,
      // we only need to release as we advance or force a restart.
      MOZ_ASSERT(MayDiscard());
      MOZ_ASSERT(mGetIndex < mInsertIndex);
      for (size_t i = 1; i < mGetIndex; ++i) {
        RawAccessFrameRef discard = Move(mFrames[i]);
      }
    }
  } else if (mInsertIndex > 0) {
    // We were forced to restart an animation before we decoded the last
    // frame. If we were discarding frames, then we tossed what we had
    // except for the first frame.
    MOZ_ASSERT(mInsertIndex < mFrames.Length());
    MOZ_ASSERT(!mFrames[mInsertIndex]);
    MOZ_ASSERT(MayDiscard());
    mFrames[mInsertIndex] = Move(aFrame);
  } else { // mInsertIndex == 0
    // We were forced to restart an animation before we decoded the last
    // frame. We don't need the redecoded first frame because we always keep
    // the original.
    MOZ_ASSERT(MayDiscard());
  }

  MOZ_ASSERT(mFrames[mInsertIndex]);
  ++mInsertIndex;

  // Ensure we only request more decoded frames if we actually need them. If we
  // need to advance to a certain point in the animation on behalf of the owner,
  // then do so. This ensures we keep decoding. If the batch size is really
  // small (i.e. 1), it is possible advancing will request the decoder to
  // "restart", but we haven't told it to stop yet. Note that we skip the first
  // insert because we actually start "advanced" to the first frame anyways.
  bool continueDecoding = --mPending > 0;
  if (mAdvance > 0 && mInsertIndex > 1) {
    continueDecoding |= AdvanceInternal();
    --mAdvance;
  }
  return continueDecoding;
}

bool
AnimationFrameBuffer::MarkComplete()
{
  // We reached the end of the animation, the next frame we get, if we get
  // another, will be the first frame again.
  MOZ_ASSERT(mInsertIndex == mFrames.Length());
  mInsertIndex = 0;

  // Since we only request advancing when we want to resume at a certain point
  // in the animation, we should never exceed the number of frames.
  MOZ_ASSERT(mAdvance == 0);

  if (!mSizeKnown) {
    // We just received the last frame in the animation. Compact the frame array
    // because we know we won't need to grow beyond here.
    mSizeKnown = true;
    mFrames.Compact();

    if (!MayDiscard()) {
      // If we did not meet the threshold, then we know we want to keep all of the
      // frames. If we also hit the last frame, we don't want to ask for more.
      mPending = 0;
    }
  }

  return mPending > 0;
}

DrawableFrameRef
AnimationFrameBuffer::Get(size_t aFrame)
{
  // We should not have asked for a frame if we never inserted.
  if (mFrames.IsEmpty()) {
    MOZ_ASSERT_UNREACHABLE("Calling Get() when we have no frames");
    return DrawableFrameRef();
  }

  // If we don't have that frame, return an empty frame ref.
  if (aFrame >= mFrames.Length()) {
    return DrawableFrameRef();
  }

  // We've got the requested frame because we are not discarding frames. While
  // we typically should have not run out of frames since we ask for more before
  // we want them, it is possible the decoder is behind.
  if (!mFrames[aFrame]) {
    MOZ_ASSERT(MayDiscard());
    return DrawableFrameRef();
  }

  // If we are advancing on behalf of the animation, we don't expect it to be
  // getting any frames (besides the first) until we get the desired frame.
  MOZ_ASSERT(aFrame == 0 || mAdvance == 0);
  return mFrames[aFrame]->DrawableRef();
}

bool
AnimationFrameBuffer::AdvanceTo(size_t aExpectedFrame)
{
  // The owner should only be advancing once it has reached the requested frame
  // in the animation.
  MOZ_ASSERT(mAdvance == 0);
  bool restartDecoder = AdvanceInternal();
  // Advancing should always be successful, as it should only happen after the
  // owner has accessed the next (now current) frame.
  MOZ_ASSERT(mGetIndex == aExpectedFrame);
  return restartDecoder;
}

bool
AnimationFrameBuffer::AdvanceInternal()
{
  // We should not have advanced if we never inserted.
  if (mFrames.IsEmpty()) {
    MOZ_ASSERT_UNREACHABLE("Calling Advance() when we have no frames");
    return false;
  }

  // We only want to change the current frame index if we have advanced. This
  // means either a higher frame index, or going back to the beginning.
  size_t framesLength = mFrames.Length();
  // We should never have advanced beyond the frame buffer.
  MOZ_ASSERT(mGetIndex < framesLength);
  // We should never advance if the current frame is null -- it needs to know
  // the timeout from it at least to know when to advance.
  MOZ_ASSERT(mFrames[mGetIndex]);
  if (++mGetIndex == framesLength) {
    MOZ_ASSERT(mSizeKnown);
    mGetIndex = 0;
  }
  // The owner should have already accessed the next frame, so it should also
  // be available.
  MOZ_ASSERT(mFrames[mGetIndex]);

  // If we moved forward, that means we can remove the previous frame, assuming
  // that frame is not the first frame. If we looped and are back at the first
  // frame, we can remove the last frame.
  if (MayDiscard()) {
    RawAccessFrameRef discard;
    if (mGetIndex > 1) {
      discard = Move(mFrames[mGetIndex - 1]);
    } else if (mGetIndex == 0) {
      MOZ_ASSERT(mSizeKnown && framesLength > 1);
      discard = Move(mFrames[framesLength - 1]);
    }
  }

  if (!mSizeKnown || MayDiscard()) {
    // Calculate how many frames we have requested ahead of the current frame.
    size_t buffered = mPending;
    if (mGetIndex > mInsertIndex) {
      // It wrapped around and we are decoding the beginning again before the
      // the display has finished the loop.
      MOZ_ASSERT(mSizeKnown);
      buffered += mInsertIndex + framesLength - mGetIndex - 1;
    } else {
      buffered += mInsertIndex - mGetIndex - 1;
    }

    if (buffered < mBatch) {
      // If we have fewer frames than the batch size, then ask for more. If we
      // do not have any pending, then we know that there is no active decoding.
      mPending += mBatch;
      return mPending == mBatch;
    }
  }

  return false;
}

bool
AnimationFrameBuffer::Reset()
{
  // The animation needs to start back at the beginning.
  mGetIndex = 0;
  mAdvance = 0;

  if (!MayDiscard()) {
    // If we haven't crossed the threshold, then we know by definition we have
    // not discarded any frames. If we previously requested more frames, but
    // it would have been more than we would have buffered otherwise, we can
    // stop the decoding after one more frame.
    if (mPending > 1 && mInsertIndex - 1 >= mBatch * 2) {
      MOZ_ASSERT(!mSizeKnown);
      mPending = 1;
    }

    // Either the decoder is still running, or we have enough frames already.
    // No need for us to restart it.
    return false;
  }

  // If we are over the threshold, then we know we will have missing frames in
  // our buffer. The easiest thing to do is to drop everything but the first
  // frame and go back to the initial state.
  bool restartDecoder = mPending == 0;
  mInsertIndex = 0;
  mPending = 2 * mBatch;

  // Discard all frames besides the first, because the decoder always expects
  // that when it re-inserts a frame, it is not present. (It doesn't re-insert
  // the first frame.)
  for (size_t i = 1; i < mFrames.Length(); ++i) {
    RawAccessFrameRef discard = Move(mFrames[i]);
  }

  return restartDecoder;
}

} // namespace image
} // namespace mozilla
