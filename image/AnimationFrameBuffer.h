/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_AnimationFrameBuffer_h
#define mozilla_image_AnimationFrameBuffer_h

#include "ISurfaceProvider.h"

namespace mozilla {
namespace image {

/**
 * An AnimationFrameBuffer owns the frames outputted by an animated image
 * decoder as well as directing its owner on how to drive the decoder,
 * whether to produce more or to stop.
 *
 * Based upon its given configuration parameters, it will retain up to a
 * certain number of frames in the buffer before deciding to discard previous
 * frames, and relying upon the decoder to recreate older frames when the
 * animation loops. It will also request that the decoder stop producing more
 * frames when the display of the frames are far behind -- this allows other
 * tasks and images which require decoding to take execution priority.
 *
 * The desire is that smaller animated images should be kept completely in
 * memory while larger animated images should only keep a certain number of
 * frames to minimize our memory footprint at the cost of CPU.
 */
class AnimationFrameBuffer final
{
public:
  AnimationFrameBuffer();

  /**
   * Configure the frame buffer with a particular threshold and batch size. Note
   * that the frame buffer may adjust the given values.
   *
   * @param aThreshold  Maximum number of frames that may be stored in the frame
   *                    buffer before it may discard already displayed frames.
   *                    Once exceeded, it will discard the previous frame to the
   *                    current frame whenever Advance is called. It always
   *                    retains the first frame.
   *
   * @param aBatch      Number of frames we request to be decoded each time it
   *                    decides we need more.
   *
   * @param aStartFrame The starting frame for the animation. The frame buffer
   *                    will auto-advance (and thus keep the decoding pipeline
   *                    going) until it has reached this frame. Useful when the
   *                    animation was progressing, but the surface was
   *                    discarded, and we had to redecode.
   */
  void Initialize(size_t aThreshold, size_t aBatch, size_t aStartFrame);

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
  imgFrame* Get(size_t aFrame);

  /**
   * Inserts a frame into the frame buffer. If it has yet to fully decode the
   * animated image yet, then it will append the frame to its internal buffer.
   * If it has been fully decoded, it will replace the next frame in its buffer
   * with the given frame.
   *
   * Once we have a sufficient number of frames buffered relative to the
   * currently displayed frame, it will return false to indicate the caller
   * should stop decoding.
   *
   * @param aFrame      The frame to insert into the buffer.
   *
   * @returns True if the decoder should decode another frame.
   */
  bool Insert(RawAccessFrameRef&& aFrame);

  /**
   * This should be called after the last frame has been inserted. If the buffer
   * is discarding old frames, it may request more frames to be decoded. In this
   * case that means the decoder should start again from the beginning. This
   * return value should be used in preference to that of the Insert call.
   *
   * @returns True if the decoder should decode another frame.
   */
  bool MarkComplete();

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
  bool AdvanceTo(size_t aExpectedFrame);

  /**
   * Resets the currently displayed frame of the frame buffer to the beginning.
   * If the buffer is discarding old frames, it will actually discard all frames
   * besides the first.
   *
   * @returns True if the caller should restart the decoder.
   */
  bool Reset();

  /**
   * @returns True if frames post-advance may be discarded and redecoded on
   *          demand, else false.
   */
  bool MayDiscard() const { return mFrames.Length() > mThreshold; }

  /**
   * @returns True if the frame buffer was ever marked as complete. This implies
   *          that the total number of frames is known and may be gotten from
   *          Frames().Length().
   */
  bool SizeKnown() const { return mSizeKnown; }

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
   * @returns Maximum number of frames before we start discarding previous
   *          frames post-advance.
   */
  size_t Threshold() const { return mThreshold; }

  /**
   * @returns The frames of this animation, in order. May contain empty indices.
   */
  const nsTArray<RawAccessFrameRef>& Frames() const { return mFrames; }

private:
  bool AdvanceInternal();

  /// The frames of this animation, in order, but may have holes if discarding.
  nsTArray<RawAccessFrameRef> mFrames;

  // The maximum number of frames we can have before discarding.
  size_t mThreshold;

  // The minimum number of frames that we want buffered ahead of the display.
  size_t mBatch;

  // The number of frames to decode before we stop.
  size_t mPending;

  // The number of frames we need to auto-advance to synchronize with the caller.
  size_t mAdvance;

  // The mFrames index in which to insert the next decoded frame.
  size_t mInsertIndex;

  // The mFrames index that we have advanced to.
  size_t mGetIndex;

  // True if the total number of frames is known.
  bool mSizeKnown;

  // True if we encountered an error while redecoding.
  bool mRedecodeError;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_AnimationFrameBuffer_h
