/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_AnimationFrameBuffer_h
#define mozilla_image_AnimationFrameBuffer_h

#include "ISurfaceProvider.h"
#include <deque>

namespace mozilla {
namespace image {

/**
 * An AnimationFrameBuffer owns the frames outputted by an animated image
 * decoder as well as directing its owner on how to drive the decoder,
 * whether to produce more or to stop.
 *
 * This should be subclassed by the different types of queues, depending on
 * what behaviour is desired.
 */
class AnimationFrameBuffer {
 public:
  enum class InsertStatus : uint8_t {
    YIELD,            // No more frames required at this time.
    CONTINUE,         // Continue decoding more frames.
    DISCARD_YIELD,    // Crossed threshold, switch to discarding structure
                      // and stop decoding more frames.
    DISCARD_CONTINUE  // Crossed threshold, switch to discarding structure
                      // and continue decoding more frames.
  };

  /**
   * @param aBatch      Number of frames we request to be decoded each time it
   *                    decides we need more.
   *
   * @param aStartFrame The starting frame for the animation. The frame buffer
   *                    will auto-advance (and thus keep the decoding pipeline
   *                    going) until it has reached this frame. Useful when the
   *                    animation was progressing, but the surface was
   *                    discarded, and we had to redecode.
   */
  AnimationFrameBuffer(size_t aBatch, size_t aStartFrame)
      : mSize(0),
        mBatch(aBatch),
        mGetIndex(0),
        mAdvance(aStartFrame),
        mPending(0),
        mSizeKnown(false),
        mMayDiscard(false),
        mRedecodeError(false),
        mRecycling(false) {
    if (mBatch > SIZE_MAX / 4) {
      // Batch size is so big, we will just end up decoding the whole animation.
      mBatch = SIZE_MAX / 4;
    } else if (mBatch < 1) {
      // Never permit a batch size smaller than 1. We always want to be asking
      // for at least one frame to start.
      mBatch = 1;
    }
  }

  AnimationFrameBuffer(const AnimationFrameBuffer& aOther)
      : mSize(aOther.mSize),
        mBatch(aOther.mBatch),
        mGetIndex(aOther.mGetIndex),
        mAdvance(aOther.mAdvance),
        mPending(aOther.mPending),
        mSizeKnown(aOther.mSizeKnown),
        mMayDiscard(aOther.mMayDiscard),
        mRedecodeError(aOther.mRedecodeError),
        mRecycling(aOther.mRecycling) {}

  virtual ~AnimationFrameBuffer() {}

  /**
   * @returns True if frames post-advance may be discarded and redecoded on
   *          demand, else false.
   */
  bool MayDiscard() const { return mMayDiscard; }

  /**
   * @returns True if frames post-advance may be reused after displaying, else
   *          false. Implies MayDiscard().
   */
  bool IsRecycling() const {
    MOZ_ASSERT_IF(mRecycling, mMayDiscard);
    return mRecycling;
  }

  /**
   * @returns True if the frame buffer was ever marked as complete. This implies
   *          that the total number of frames is known and may be gotten from
   *          Frames().Length().
   */
  bool SizeKnown() const { return mSizeKnown; }

  /**
   * @returns The total number of frames in the animation. If SizeKnown() is
   *          true, then this is a constant, else it is just the total number of
   *          frames we have decoded thus far.
   */
  size_t Size() const { return mSize; }

  /**
   * @returns True if encountered an error during redecode which should cause
   *          the caller to stop inserting frames.
   */
  bool HasRedecodeError() const { return mRedecodeError; }

  /**
   * @returns The current frame index we have advanced to.
   */
  size_t Displayed() const { return mGetIndex; }

  /**
   * @returns Outstanding frames desired from the decoder.
   */
  size_t PendingDecode() const { return mPending; }

  /**
   * @returns Outstanding frames to advance internally.
   */
  size_t PendingAdvance() const { return mAdvance; }

  /**
   * @returns Number of frames we request to be decoded each time it decides we
   *          need more.
   */
  size_t Batch() const { return mBatch; }

  /**
   * Resets the currently displayed frame of the frame buffer to the beginning.
   *
   * @returns True if the caller should restart the decoder.
   */
  bool Reset() {
    mGetIndex = 0;
    mAdvance = 0;
    return ResetInternal();
  }

  /**
   * Advance the currently displayed frame of the frame buffer. If it reaches
   * the end, it will loop back to the beginning. It should not be called unless
   * a call to Get has returned a valid frame for the next frame index.
   *
   * As we advance, the number of frames we have buffered ahead of the current
   * will shrink. Once that becomes too few, we will request a batch-sized set
   * of frames to be decoded from the decoder.
   *
   * @param aExpectedFrame  The frame we expect to have advanced to. This is
   *                        used for confirmation purposes (e.g. asserts).
   *
   * @returns True if the caller should restart the decoder.
   */
  bool AdvanceTo(size_t aExpectedFrame) {
    MOZ_ASSERT(mAdvance == 0);

    if (++mGetIndex == mSize && mSizeKnown) {
      mGetIndex = 0;
    }
    MOZ_ASSERT(mGetIndex == aExpectedFrame);

    bool hasPending = mPending > 0;
    AdvanceInternal();
    // Restart the decoder if we transitioned from no pending frames being
    // decoded, to some pending frames to be decoded.
    return !hasPending && mPending > 0;
  }

  /**
   * Inserts a frame into the frame buffer.
   *
   * Once we have a sufficient number of frames buffered relative to the
   * currently displayed frame, it will return YIELD to indicate the caller
   * should stop decoding. Otherwise it will return CONTINUE.
   *
   * If we cross the threshold, it will return DISCARD_YIELD or DISCARD_CONTINUE
   * to indicate that the caller should switch to a new queue type.
   *
   * @param aFrame      The frame to insert into the buffer.
   *
   * @returns True if the decoder should decode another frame.
   */
  InsertStatus Insert(RefPtr<imgFrame>&& aFrame) {
    MOZ_ASSERT(mPending > 0);
    MOZ_ASSERT(aFrame);

    --mPending;
    bool retain = InsertInternal(std::move(aFrame));

    if (mAdvance > 0 && mSize > 1) {
      --mAdvance;
      ++mGetIndex;
      AdvanceInternal();
    }

    if (!retain) {
      return mPending > 0 ? InsertStatus::DISCARD_CONTINUE
                          : InsertStatus::DISCARD_YIELD;
    }

    return mPending > 0 ? InsertStatus::CONTINUE : InsertStatus::YIELD;
  }

  /**
   * Access a specific frame from the frame buffer. It should generally access
   * frames in sequential order, increasing in tandem with AdvanceTo calls. The
   * first frame may be accessed at any time. The access order should start with
   * the same value as that given in Initialize (aStartFrame).
   *
   * @param aFrame      The frame index to access.
   *
   * @returns The frame, if available.
   */
  virtual imgFrame* Get(size_t aFrame, bool aForDisplay) = 0;

  /**
   * @returns True if the first frame of the animation (not of the queue) is
   *          available/finished, else false.
   */
  virtual bool IsFirstFrameFinished() const = 0;

  /**
   * @returns True if the last inserted frame matches the given frame, else
   *          false.
   */
  virtual bool IsLastInsertedFrame(imgFrame* aFrame) const = 0;

  /**
   * This should be called after the last frame has been inserted. If the buffer
   * is discarding old frames, it may request more frames to be decoded. In this
   * case that means the decoder should start again from the beginning. This
   * return value should be used in preference to that of the Insert call.
   *
   * @returns True if the decoder should decode another frame.
   */
  virtual bool MarkComplete(const gfx::IntRect& aFirstFrameRefreshArea) = 0;

  typedef ISurfaceProvider::AddSizeOfCbData AddSizeOfCbData;
  typedef ISurfaceProvider::AddSizeOfCb AddSizeOfCb;

  /**
   * Accumulate the total cost of all the frames in the buffer.
   */
  virtual void AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                      const AddSizeOfCb& aCallback) = 0;

  /**
   * Request a recycled frame buffer, and if available, set aRecycleRect to be
   * the dirty rect between the contents of the recycled frame, and the restore
   * frame (e.g. what we composite on top of) for the next frame to be created.
   *
   * @returns The frame to be recycled, if available.
   */
  virtual RawAccessFrameRef RecycleFrame(gfx::IntRect& aRecycleRect) {
    MOZ_ASSERT(!mRecycling);
    return RawAccessFrameRef();
  }

 protected:
  /**
   * Perform the actual insertion of the given frame into the underlying buffer
   * representation. mGetIndex shall be the index of the frame we are inserting,
   * and mSize and mPending have already been adjusted as needed.
   *
   * @returns True if the caller should continue as normal, false if the discard
   *          threshold was crossed and we should change queue types.
   */
  virtual bool InsertInternal(RefPtr<imgFrame>&& aFrame) = 0;

  /**
   * Advance from the current frame to the immediately adjacent next frame.
   * mGetIndex shall be the the index of the new current frame after advancing.
   * mPending may be adjusted to request more frames.
   */
  virtual void AdvanceInternal() = 0;

  /**
   * Discard any frames as necessary for the reset. mPending may be adjusted to
   * request more frames.
   *
   * @returns True if the caller should resume decoding new frames, else false.
   */
  virtual bool ResetInternal() = 0;

  // The total number of frames in the animation. If mSizeKnown is true, it is
  // the actual total regardless of how many frames are available, otherwise it
  // is the total number of inserted frames.
  size_t mSize;

  // The minimum number of frames that we want buffered ahead of the display.
  size_t mBatch;

  // The sequential index of the frame we have advanced to.
  size_t mGetIndex;

  // The number of frames we need to auto-advance to synchronize with the
  // caller.
  size_t mAdvance;

  // The number of frames to decode before we stop.
  size_t mPending;

  // True if the total number of frames for the animation is known.
  bool mSizeKnown;

  // True if this buffer may discard frames.
  bool mMayDiscard;

  // True if we encountered an error while redecoding.
  bool mRedecodeError;

  // True if this buffer is recycling frames.
  bool mRecycling;
};

/**
 * An AnimationFrameRetainedBuffer will retain all of the frames inserted into
 * it. Once it crosses its maximum number of frames, it will recommend
 * conversion to a discarding queue.
 */
class AnimationFrameRetainedBuffer final : public AnimationFrameBuffer {
 public:
  /**
   * @param aThreshold  Maximum number of frames that may be stored in the frame
   *                    buffer before it may discard already displayed frames.
   *                    Once exceeded, it will discard the previous frame to the
   *                    current frame whenever Advance is called. It always
   *                    retains the first frame.
   *
   * @param aBatch      See AnimationFrameBuffer::AnimationFrameBuffer.
   *
   * @param aStartFrame See AnimationFrameBuffer::AnimationFrameBuffer.
   */
  AnimationFrameRetainedBuffer(size_t aThreshold, size_t aBatch,
                               size_t aCurrentFrame);

  /**
   * @returns Maximum number of frames before we start discarding previous
   *          frames post-advance.
   */
  size_t Threshold() const { return mThreshold; }

  /**
   * @returns The frames of this animation, in order. Each element will always
   *          contain a valid frame.
   */
  const nsTArray<RefPtr<imgFrame>>& Frames() const { return mFrames; }

  imgFrame* Get(size_t aFrame, bool aForDisplay) override;
  bool IsFirstFrameFinished() const override;
  bool IsLastInsertedFrame(imgFrame* aFrame) const override;
  bool MarkComplete(const gfx::IntRect& aFirstFrameRefreshArea) override;
  void AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                              const AddSizeOfCb& aCallback) override;

 private:
  friend class AnimationFrameDiscardingQueue;
  friend class AnimationFrameRecyclingQueue;

  bool InsertInternal(RefPtr<imgFrame>&& aFrame) override;
  void AdvanceInternal() override;
  bool ResetInternal() override;

  // The frames of this animation, in order.
  nsTArray<RefPtr<imgFrame>> mFrames;

  // The maximum number of frames we can have before discarding.
  size_t mThreshold;
};

/**
 * An AnimationFrameDiscardingQueue will only retain up to mBatch * 2 frames.
 * When the animation advances, it will discard the old current frame.
 */
class AnimationFrameDiscardingQueue : public AnimationFrameBuffer {
 public:
  explicit AnimationFrameDiscardingQueue(AnimationFrameRetainedBuffer&& aQueue);

  imgFrame* Get(size_t aFrame, bool aForDisplay) final;
  bool IsFirstFrameFinished() const final;
  bool IsLastInsertedFrame(imgFrame* aFrame) const final;
  bool MarkComplete(const gfx::IntRect& aFirstFrameRefreshArea) override;
  void AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                              const AddSizeOfCb& aCallback) override;

  const std::deque<RefPtr<imgFrame>>& Display() const { return mDisplay; }
  const imgFrame* FirstFrame() const { return mFirstFrame; }
  size_t PendingInsert() const { return mInsertIndex; }

 protected:
  bool InsertInternal(RefPtr<imgFrame>&& aFrame) override;
  void AdvanceInternal() override;
  bool ResetInternal() override;

  /// The sequential index of the frame we inserting next.
  size_t mInsertIndex;

  /// Queue storing frames to be displayed by the animator. The first frame in
  /// the queue is the currently displayed frame.
  std::deque<RefPtr<imgFrame>> mDisplay;

  /// The first frame which is never discarded, and preferentially reused.
  RefPtr<imgFrame> mFirstFrame;
};

/**
 * An AnimationFrameRecyclingQueue will only retain up to mBatch * 2 frames.
 * When the animation advances, it will place the old current frame into a
 * recycling queue to be reused for a future allocation. This only works for
 * animated images where we decoded full sized frames into their own buffers,
 * so that the buffers are all identically sized and contain the complete frame
 * data.
 */
class AnimationFrameRecyclingQueue final
    : public AnimationFrameDiscardingQueue {
 public:
  explicit AnimationFrameRecyclingQueue(AnimationFrameRetainedBuffer&& aQueue);

  bool MarkComplete(const gfx::IntRect& aFirstFrameRefreshArea) override;
  void AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                              const AddSizeOfCb& aCallback) override;

  RawAccessFrameRef RecycleFrame(gfx::IntRect& aRecycleRect) override;

  struct RecycleEntry {
    explicit RecycleEntry(const gfx::IntRect& aDirtyRect)
        : mDirtyRect(aDirtyRect) {}

    RecycleEntry(RecycleEntry&& aOther)
        : mFrame(std::move(aOther.mFrame)), mDirtyRect(aOther.mDirtyRect) {}

    RecycleEntry& operator=(RecycleEntry&& aOther) {
      mFrame = std::move(aOther.mFrame);
      mDirtyRect = aOther.mDirtyRect;
      return *this;
    }

    RecycleEntry(const RecycleEntry& aOther) = delete;
    RecycleEntry& operator=(const RecycleEntry& aOther) = delete;

    RefPtr<imgFrame> mFrame;  // The frame containing the buffer to recycle.
    gfx::IntRect mDirtyRect;  // The dirty rect of the frame itself.
  };

  const std::deque<RecycleEntry>& Recycle() const { return mRecycle; }
  const gfx::IntRect& FirstFrameRefreshArea() const {
    return mFirstFrameRefreshArea;
  }

 protected:
  void AdvanceInternal() override;
  bool ResetInternal() override;

  /// Queue storing frames to be recycled by the decoder to produce its future
  /// frames. May contain up to mBatch frames, where the last frame in the queue
  /// is adjacent to the first frame in the mDisplay queue.
  std::deque<RecycleEntry> mRecycle;

  /// The first frame refresh area. This is used instead of the dirty rect for
  /// the last frame when transitioning back to the first frame.
  gfx::IntRect mFirstFrameRefreshArea;

  /// Force recycled frames to use the first frame refresh area as their dirty
  /// rect. This is used when we are recycling frames from the end of an
  /// animation to produce frames at the beginning of an animation.
  bool mForceUseFirstFrameRefreshArea;
};

}  // namespace image
}  // namespace mozilla

#endif  // mozilla_image_AnimationFrameBuffer_h
