/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationFrameBuffer.h"

#include <utility>  // for Move

namespace mozilla {
namespace image {

AnimationFrameRetainedBuffer::AnimationFrameRetainedBuffer(size_t aThreshold,
                                                           size_t aBatch,
                                                           size_t aStartFrame)
    : AnimationFrameBuffer(aBatch, aStartFrame), mThreshold(aThreshold) {
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

bool AnimationFrameRetainedBuffer::InsertInternal(RefPtr<imgFrame>&& aFrame) {
  // We should only insert new frames if we actually asked for them.
  MOZ_ASSERT(!mSizeKnown);
  MOZ_ASSERT(mFrames.Length() < mThreshold);

  ++mSize;
  mFrames.AppendElement(std::move(aFrame));
  MOZ_ASSERT(mSize == mFrames.Length());
  return mSize < mThreshold;
}

bool AnimationFrameRetainedBuffer::ResetInternal() {
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

bool AnimationFrameRetainedBuffer::MarkComplete(
    const gfx::IntRect& aFirstFrameRefreshArea) {
  MOZ_ASSERT(!mSizeKnown);
  mSizeKnown = true;
  mPending = 0;
  mFrames.Compact();
  return false;
}

void AnimationFrameRetainedBuffer::AdvanceInternal() {
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

imgFrame* AnimationFrameRetainedBuffer::Get(size_t aFrame, bool aForDisplay) {
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

bool AnimationFrameRetainedBuffer::IsFirstFrameFinished() const {
  return !mFrames.IsEmpty() && mFrames[0]->IsFinished();
}

bool AnimationFrameRetainedBuffer::IsLastInsertedFrame(imgFrame* aFrame) const {
  return !mFrames.IsEmpty() && mFrames.LastElement().get() == aFrame;
}

void AnimationFrameRetainedBuffer::AddSizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf, const AddSizeOfCb& aCallback) {
  size_t i = 0;
  for (const RefPtr<imgFrame>& frame : mFrames) {
    ++i;
    frame->AddSizeOfExcludingThis(aMallocSizeOf,
                                  [&](AddSizeOfCbData& aMetadata) {
                                    aMetadata.mIndex = i;
                                    aCallback(aMetadata);
                                  });
  }
}

AnimationFrameDiscardingQueue::AnimationFrameDiscardingQueue(
    AnimationFrameRetainedBuffer&& aQueue)
    : AnimationFrameBuffer(aQueue),
      mInsertIndex(aQueue.mFrames.Length()),
      mFirstFrame(aQueue.mFrames[0]) {
  MOZ_ASSERT(!mSizeKnown);
  MOZ_ASSERT(!mRedecodeError);
  MOZ_ASSERT(mInsertIndex > 0);
  mMayDiscard = true;

  // We avoided moving aQueue.mFrames[0] for mFirstFrame above because it is
  // possible the animation was reset back to the beginning, and then we crossed
  // the threshold without advancing further. That would mean mGetIndex is 0.
  for (size_t i = mGetIndex; i < mInsertIndex; ++i) {
    MOZ_ASSERT(aQueue.mFrames[i]);
    mDisplay.push_back(std::move(aQueue.mFrames[i]));
  }
}

bool AnimationFrameDiscardingQueue::InsertInternal(RefPtr<imgFrame>&& aFrame) {
  if (mInsertIndex == mSize) {
    if (mSizeKnown) {
      // We produced more frames on a subsequent decode than on the first pass.
      mRedecodeError = true;
      mPending = 0;
      return true;
    }
    ++mSize;
  }

  // Even though we don't use redecoded first frames for display purposes, we
  // will still use them for recycling, so we still need to insert it.
  mDisplay.push_back(std::move(aFrame));
  ++mInsertIndex;
  MOZ_ASSERT(mInsertIndex <= mSize);
  return true;
}

bool AnimationFrameDiscardingQueue::ResetInternal() {
  mDisplay.clear();
  mInsertIndex = 0;

  bool restartDecoder = mPending == 0;
  mPending = 2 * mBatch;
  return restartDecoder;
}

bool AnimationFrameDiscardingQueue::MarkComplete(
    const gfx::IntRect& aFirstFrameRefreshArea) {
  if (NS_WARN_IF(mInsertIndex != mSize)) {
    mRedecodeError = true;
    mPending = 0;
  }

  // We reached the end of the animation, the next frame we get, if we get
  // another, will be the first frame again.
  mInsertIndex = 0;
  mSizeKnown = true;

  // Since we only request advancing when we want to resume at a certain point
  // in the animation, we should never exceed the number of frames.
  MOZ_ASSERT(mAdvance == 0);
  return mPending > 0;
}

void AnimationFrameDiscardingQueue::AdvanceInternal() {
  // We only want to change the current frame index if we have advanced. This
  // means either a higher frame index, or going back to the beginning.
  // We should never have advanced beyond the frame buffer.
  MOZ_ASSERT(mGetIndex < mSize);

  // We should have the current frame still in the display queue. Either way,
  // we should at least have an entry in the queue which we need to consume.
  MOZ_ASSERT(!mDisplay.empty());
  MOZ_ASSERT(mDisplay.front());
  mDisplay.pop_front();
  MOZ_ASSERT(!mDisplay.empty());
  MOZ_ASSERT(mDisplay.front());

  if (mDisplay.size() + mPending - 1 < mBatch) {
    // If we have fewer frames than the batch size, then ask for more. If we
    // do not have any pending, then we know that there is no active decoding.
    mPending += mBatch;
  }
}

imgFrame* AnimationFrameDiscardingQueue::Get(size_t aFrame, bool aForDisplay) {
  // The first frame is stored separately. If we only need the frame for
  // display purposes, we can return it right away. If we need it for advancing
  // the animation, we want to verify the recreated first frame is available
  // before allowing it continue.
  if (aForDisplay && aFrame == 0) {
    return mFirstFrame.get();
  }

  // If we don't have that frame, return an empty frame ref.
  if (aFrame >= mSize) {
    return nullptr;
  }

  size_t offset;
  if (aFrame >= mGetIndex) {
    offset = aFrame - mGetIndex;
  } else if (!mSizeKnown) {
    MOZ_ASSERT_UNREACHABLE("Requesting previous frame after we have advanced!");
    return nullptr;
  } else {
    offset = mSize - mGetIndex + aFrame;
  }

  if (offset >= mDisplay.size()) {
    return nullptr;
  }

  // If we are advancing on behalf of the animation, we don't expect it to be
  // getting any frames (besides the first) until we get the desired frame.
  MOZ_ASSERT(aFrame == 0 || mAdvance == 0);

  // If we have space for the frame, it should always be available.
  MOZ_ASSERT(mDisplay[offset]);
  return mDisplay[offset].get();
}

bool AnimationFrameDiscardingQueue::IsFirstFrameFinished() const {
  MOZ_ASSERT(mFirstFrame);
  MOZ_ASSERT(mFirstFrame->IsFinished());
  return true;
}

bool AnimationFrameDiscardingQueue::IsLastInsertedFrame(
    imgFrame* aFrame) const {
  return !mDisplay.empty() && mDisplay.back().get() == aFrame;
}

void AnimationFrameDiscardingQueue::AddSizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf, const AddSizeOfCb& aCallback) {
  mFirstFrame->AddSizeOfExcludingThis(aMallocSizeOf,
                                      [&](AddSizeOfCbData& aMetadata) {
                                        aMetadata.mIndex = 1;
                                        aCallback(aMetadata);
                                      });

  size_t i = mGetIndex;
  for (const RefPtr<imgFrame>& frame : mDisplay) {
    ++i;
    if (mSize < i) {
      i = 1;
      if (mFirstFrame.get() == frame.get()) {
        // First frame again, we already covered it above. We can have a
        // different frame in the first frame position in the discard queue
        // on subsequent passes of the animation. This is useful for recycling.
        continue;
      }
    }

    frame->AddSizeOfExcludingThis(aMallocSizeOf,
                                  [&](AddSizeOfCbData& aMetadata) {
                                    aMetadata.mIndex = i;
                                    aCallback(aMetadata);
                                  });
  }
}

AnimationFrameRecyclingQueue::AnimationFrameRecyclingQueue(
    AnimationFrameRetainedBuffer&& aQueue)
    : AnimationFrameDiscardingQueue(std::move(aQueue)),
      mForceUseFirstFrameRefreshArea(false) {
  // In an ideal world, we would always save the already displayed frames for
  // recycling but none of the frames were marked as recyclable. We will incur
  // the extra allocation cost for a few more frames.
  mRecycling = true;

  // Until we reach the end of the animation, set the first frame refresh area
  // to match that of the full area of the first frame.
  mFirstFrameRefreshArea = mFirstFrame->GetRect();
}

void AnimationFrameRecyclingQueue::AddSizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf, const AddSizeOfCb& aCallback) {
  AnimationFrameDiscardingQueue::AddSizeOfExcludingThis(aMallocSizeOf,
                                                        aCallback);

  for (const RecycleEntry& entry : mRecycle) {
    if (entry.mFrame) {
      entry.mFrame->AddSizeOfExcludingThis(
          aMallocSizeOf, [&](AddSizeOfCbData& aMetadata) {
            aMetadata.mIndex = 0;  // Frame is not applicable
            aCallback(aMetadata);
          });
    }
  }
}

void AnimationFrameRecyclingQueue::AdvanceInternal() {
  // We only want to change the current frame index if we have advanced. This
  // means either a higher frame index, or going back to the beginning.
  // We should never have advanced beyond the frame buffer.
  MOZ_ASSERT(mGetIndex < mSize);

  MOZ_ASSERT(!mDisplay.empty());
  MOZ_ASSERT(mDisplay.front());

  // We have advanced past the first frame. That means the next frame we are
  // putting in the queue to recycling is the first frame in the animation,
  // and we no longer need to worry about having looped around.
  if (mGetIndex == 1) {
    mForceUseFirstFrameRefreshArea = false;
  }

  RefPtr<imgFrame>& front = mDisplay.front();
  RecycleEntry newEntry(mForceUseFirstFrameRefreshArea ? mFirstFrameRefreshArea
                                                       : front->GetDirtyRect());

  // If we are allowed to recycle the frame, then we should save it before the
  // base class's AdvanceInternal discards it.
  newEntry.mFrame = std::move(front);

  // Even if the frame itself isn't saved, we want the dirty rect to calculate
  // the recycle rect for future recycled frames.
  mRecycle.push_back(std::move(newEntry));
  mDisplay.pop_front();
  MOZ_ASSERT(!mDisplay.empty());
  MOZ_ASSERT(mDisplay.front());

  if (mDisplay.size() + mPending - 1 < mBatch) {
    // If we have fewer frames than the batch size, then ask for more. If we
    // do not have any pending, then we know that there is no active decoding.
    //
    // We limit the batch to avoid using the frame we just added to the queue.
    // This gives other parts of the system time to switch to the new current
    // frame, and maximize buffer reuse. In particular this is useful for
    // WebRender which holds onto the previous frame for much longer.
    size_t newPending = std::min(mPending + mBatch, mRecycle.size() - 1);
    if (newPending == 0 && (mDisplay.size() <= 1 || mPending > 0)) {
      // If we already have pending frames, then the decoder is active and we
      // cannot go below one. If we are displaying the only frame we have, and
      // there are none pending, then we must request at least one more frame to
      // continue to animation, because we won't advance again without a new
      // frame. This may cause us to skip recycling because the previous frame
      // is still in use.
      newPending = 1;
    }
    mPending = newPending;
  }
}

bool AnimationFrameRecyclingQueue::ResetInternal() {
  // We should save any display frames that we can to save on at least the
  // allocation. The first frame refresh area is guaranteed to be the aggregate
  // dirty rect or the entire frame, and so the bare minimum area we can
  // recycle. We don't need to worry about updating the dirty rect for the
  // existing mRecycle entries, because that will happen in RecycleFrame when
  // we try to pull out a frame to redecode the first frame.
  for (RefPtr<imgFrame>& frame : mDisplay) {
    RecycleEntry newEntry(mFirstFrameRefreshArea);
    newEntry.mFrame = std::move(frame);
    mRecycle.push_back(std::move(newEntry));
  }

  return AnimationFrameDiscardingQueue::ResetInternal();
}

RawAccessFrameRef AnimationFrameRecyclingQueue::RecycleFrame(
    gfx::IntRect& aRecycleRect) {
  if (mInsertIndex == 0) {
    // If we are recreating the first frame, then we actually have already
    // precomputed aggregate of the dirty rects as the first frame refresh
    // area. We know that all of the frames still in the recycling queue
    // need to take into account the same dirty rect because they are also
    // frames which cross the boundary.
    //
    // Note that this may actually shrink the dirty rect if we estimated it
    // earlier with the full frame size and now we have the actual, more
    // conservative aggregate for the animation.
    for (RecycleEntry& entry : mRecycle) {
      entry.mDirtyRect = mFirstFrameRefreshArea;
    }
    // Until we advance to the first frame again, any subsequent recycled
    // frames should also use the first frame refresh area.
    mForceUseFirstFrameRefreshArea = true;
  }

  if (mRecycle.empty()) {
    return RawAccessFrameRef();
  }

  RawAccessFrameRef recycledFrame;
  if (mRecycle.front().mFrame) {
    recycledFrame = mRecycle.front().mFrame->RawAccessRef();
    MOZ_ASSERT(recycledFrame);
    mRecycle.pop_front();

    if (mForceUseFirstFrameRefreshArea) {
      // We are still crossing the loop boundary and cannot rely upon the dirty
      // rects of entries in mDisplay to be representative. E.g. The first frame
      // is probably has a full frame dirty rect.
      aRecycleRect = mFirstFrameRefreshArea;
    } else {
      // Calculate the recycle rect for the recycled frame. This is the
      // cumulative dirty rect of all of the frames ahead of us to be displayed,
      // and to be used for recycling. Or in other words, the dirty rect between
      // the recycled frame and the decoded frame which reuses the buffer.
      //
      // We know at this point that mRecycle contains either frames from the end
      // of the animation with the first frame refresh area as the dirty rect
      // (plus the first frame likewise) and frames with their actual dirty rect
      // from the start. mDisplay should also only contain frames from the start
      // of the animation onwards.
      aRecycleRect.SetRect(0, 0, 0, 0);
      for (const RefPtr<imgFrame>& frame : mDisplay) {
        aRecycleRect = aRecycleRect.Union(frame->GetDirtyRect());
      }
      for (const RecycleEntry& entry : mRecycle) {
        aRecycleRect = aRecycleRect.Union(entry.mDirtyRect);
      }
    }
  } else {
    mRecycle.pop_front();
  }

  return recycledFrame;
}

bool AnimationFrameRecyclingQueue::MarkComplete(
    const gfx::IntRect& aFirstFrameRefreshArea) {
  bool continueDecoding =
      AnimationFrameDiscardingQueue::MarkComplete(aFirstFrameRefreshArea);

  // If we encounter a redecode error, just make the first frame refresh area to
  // be the full frame, because we don't really know what we can safely recycle.
  mFirstFrameRefreshArea =
      mRedecodeError ? mFirstFrame->GetRect() : aFirstFrameRefreshArea;
  return continueDecoding;
}

}  // namespace image
}  // namespace mozilla
