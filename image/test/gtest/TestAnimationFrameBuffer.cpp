/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/Move.h"
#include "AnimationFrameBuffer.h"

using namespace mozilla;
using namespace mozilla::image;

static RawAccessFrameRef
CreateEmptyFrame()
{
  RefPtr<imgFrame> frame = new imgFrame();
  nsresult rv = frame->InitForAnimator(nsIntSize(1, 1), SurfaceFormat::B8G8R8A8);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  RawAccessFrameRef frameRef = frame->RawAccessRef();
  frame->Finish();
  return frameRef;
}

static bool
Fill(AnimationFrameBuffer& buffer, size_t aLength)
{
  bool keepDecoding = false;
  for (size_t i = 0; i < aLength; ++i) {
    RawAccessFrameRef frame = CreateEmptyFrame();
    keepDecoding = buffer.Insert(Move(frame->RawAccessRef()));
  }
  return keepDecoding;
}

static void
CheckFrames(const AnimationFrameBuffer& buffer, size_t aStart, size_t aEnd, bool aExpected)
{
  for (size_t i = aStart; i < aEnd; ++i) {
    EXPECT_EQ(aExpected, !!buffer.Frames()[i]);
  }
}

static void
CheckRemoved(const AnimationFrameBuffer& buffer, size_t aStart, size_t aEnd)
{
  CheckFrames(buffer, aStart, aEnd, false);
}

static void
CheckRetained(const AnimationFrameBuffer& buffer, size_t aStart, size_t aEnd)
{
  CheckFrames(buffer, aStart, aEnd, true);
}

class ImageAnimationFrameBuffer : public ::testing::Test
{
public:
  ImageAnimationFrameBuffer()
  { }

private:
  AutoInitializeImageLib mInit;
};

TEST_F(ImageAnimationFrameBuffer, InitialState)
{
  const size_t kThreshold = 800;
  const size_t kBatch = 100;
  AnimationFrameBuffer buffer;
  buffer.Initialize(kThreshold, kBatch, 0);

  EXPECT_EQ(kThreshold, buffer.Threshold());
  EXPECT_EQ(kBatch, buffer.Batch());
  EXPECT_EQ(size_t(0), buffer.Displayed());
  EXPECT_EQ(kBatch * 2, buffer.PendingDecode());
  EXPECT_EQ(size_t(0), buffer.PendingAdvance());
  EXPECT_FALSE(buffer.MayDiscard());
  EXPECT_FALSE(buffer.SizeKnown());
  EXPECT_TRUE(buffer.Frames().IsEmpty());
}

TEST_F(ImageAnimationFrameBuffer, ThresholdTooSmall)
{
  const size_t kThreshold = 0;
  const size_t kBatch = 10;
  AnimationFrameBuffer buffer;
  buffer.Initialize(kThreshold, kBatch, 0);

  EXPECT_EQ(kBatch * 2 + 1, buffer.Threshold());
  EXPECT_EQ(kBatch, buffer.Batch());
  EXPECT_EQ(kBatch * 2, buffer.PendingDecode());
  EXPECT_EQ(size_t(0), buffer.PendingAdvance());
}

TEST_F(ImageAnimationFrameBuffer, BatchTooSmall)
{
  const size_t kThreshold = 10;
  const size_t kBatch = 0;
  AnimationFrameBuffer buffer;
  buffer.Initialize(kThreshold, kBatch, 0);

  EXPECT_EQ(kThreshold, buffer.Threshold());
  EXPECT_EQ(size_t(1), buffer.Batch());
  EXPECT_EQ(size_t(2), buffer.PendingDecode());
  EXPECT_EQ(size_t(0), buffer.PendingAdvance());
}

TEST_F(ImageAnimationFrameBuffer, BatchTooBig)
{
  const size_t kThreshold = 50;
  const size_t kBatch = SIZE_MAX;
  AnimationFrameBuffer buffer;
  buffer.Initialize(kThreshold, kBatch, 0);

  // The rounding is important here (e.g. SIZE_MAX/4 * 2 != SIZE_MAX/2).
  EXPECT_EQ(SIZE_MAX/4, buffer.Batch());
  EXPECT_EQ(buffer.Batch() * 2 + 1, buffer.Threshold());
  EXPECT_EQ(buffer.Batch() * 2, buffer.PendingDecode());
  EXPECT_EQ(size_t(0), buffer.PendingAdvance());
}

TEST_F(ImageAnimationFrameBuffer, FinishUnderBatchAndThreshold)
{
  const size_t kThreshold = 30;
  const size_t kBatch = 10;
  AnimationFrameBuffer buffer;
  buffer.Initialize(kThreshold, kBatch, 0);
  const auto& frames = buffer.Frames();

  EXPECT_EQ(kBatch * 2, buffer.PendingDecode());

  RawAccessFrameRef firstFrame;
  for (size_t i = 0; i < 5; ++i) {
    RawAccessFrameRef frame = CreateEmptyFrame();
    bool keepDecoding = buffer.Insert(Move(frame->RawAccessRef()));
    EXPECT_TRUE(keepDecoding);
    EXPECT_FALSE(buffer.SizeKnown());

    if (i == 4) {
      EXPECT_EQ(size_t(15), buffer.PendingDecode());
      keepDecoding = buffer.MarkComplete();
      EXPECT_FALSE(keepDecoding);
      EXPECT_TRUE(buffer.SizeKnown());
      EXPECT_EQ(size_t(0), buffer.PendingDecode());
      EXPECT_FALSE(buffer.HasRedecodeError());
    }

    EXPECT_FALSE(buffer.MayDiscard());

    DrawableFrameRef gotFrame = buffer.Get(i);
    EXPECT_EQ(frame.get(), gotFrame.get());
    ASSERT_EQ(i + 1, frames.Length());
    EXPECT_EQ(frame.get(), frames[i].get());

    if (i == 0) {
      firstFrame = Move(frame);
      EXPECT_EQ(size_t(0), buffer.Displayed());
    } else {
      EXPECT_EQ(i - 1, buffer.Displayed());
      bool restartDecoder = buffer.AdvanceTo(i);
      EXPECT_FALSE(restartDecoder);
      EXPECT_EQ(i, buffer.Displayed());
    }

    gotFrame = buffer.Get(0);
    EXPECT_EQ(firstFrame.get(), gotFrame.get());
  }

  // Loop again over the animation and make sure it is still all there.
  for (size_t i = 0; i < frames.Length(); ++i) {
    DrawableFrameRef gotFrame = buffer.Get(i);
    EXPECT_TRUE(gotFrame);

    bool restartDecoder = buffer.AdvanceTo(i);
    EXPECT_FALSE(restartDecoder);
  }
}

TEST_F(ImageAnimationFrameBuffer, FinishMultipleBatchesUnderThreshold)
{
  const size_t kThreshold = 30;
  const size_t kBatch = 2;
  AnimationFrameBuffer buffer;
  buffer.Initialize(kThreshold, kBatch, 0);
  const auto& frames = buffer.Frames();

  EXPECT_EQ(kBatch * 2, buffer.PendingDecode());

  // Add frames until it tells us to stop.
  bool keepDecoding;
  do {
    keepDecoding = buffer.Insert(CreateEmptyFrame());
    EXPECT_FALSE(buffer.SizeKnown());
    EXPECT_FALSE(buffer.MayDiscard());
  } while (keepDecoding);

  EXPECT_EQ(size_t(0), buffer.PendingDecode());
  EXPECT_EQ(size_t(4), frames.Length());

  // Progress through the animation until it lets us decode again.
  bool restartDecoder = false;
  size_t i = 0;
  do {
    DrawableFrameRef gotFrame = buffer.Get(i);
    EXPECT_TRUE(gotFrame);
    if (i > 0) {
      restartDecoder = buffer.AdvanceTo(i);
    }
    ++i;
  } while (!restartDecoder);

  EXPECT_EQ(size_t(2), buffer.PendingDecode());
  EXPECT_EQ(size_t(2), buffer.Displayed());

  // Add the last frame.
  keepDecoding = buffer.Insert(CreateEmptyFrame());
  EXPECT_TRUE(keepDecoding);
  keepDecoding = buffer.MarkComplete();
  EXPECT_FALSE(keepDecoding);
  EXPECT_TRUE(buffer.SizeKnown());
  EXPECT_EQ(size_t(0), buffer.PendingDecode());
  EXPECT_EQ(size_t(5), frames.Length());
  EXPECT_FALSE(buffer.HasRedecodeError());

  // Finish progressing through the animation.
  for ( ; i < frames.Length(); ++i) {
    DrawableFrameRef gotFrame = buffer.Get(i);
    EXPECT_TRUE(gotFrame);
    restartDecoder = buffer.AdvanceTo(i);
    EXPECT_FALSE(restartDecoder);
  }

  // Loop again over the animation and make sure it is still all there.
  for (i = 0; i < frames.Length(); ++i) {
    DrawableFrameRef gotFrame = buffer.Get(i);
    EXPECT_TRUE(gotFrame);
    restartDecoder = buffer.AdvanceTo(i);
    EXPECT_FALSE(restartDecoder);
  }

  // Loop to the third frame and then reset the animation.
  for (i = 0; i < 3; ++i) {
    DrawableFrameRef gotFrame = buffer.Get(i);
    EXPECT_TRUE(gotFrame);
    restartDecoder = buffer.AdvanceTo(i);
    EXPECT_FALSE(restartDecoder);
  }

  // Since we are below the threshold, we can reset the get index only.
  // Nothing else should have changed.
  restartDecoder = buffer.Reset();
  EXPECT_FALSE(restartDecoder);
  CheckRetained(buffer, 0, 5);
  EXPECT_EQ(size_t(0), buffer.PendingDecode());
  EXPECT_EQ(size_t(0), buffer.PendingAdvance());
  EXPECT_EQ(size_t(0), buffer.Displayed());
}

TEST_F(ImageAnimationFrameBuffer, MayDiscard)
{
  const size_t kThreshold = 8;
  const size_t kBatch = 3;
  AnimationFrameBuffer buffer;
  buffer.Initialize(kThreshold, kBatch, 0);
  const auto& frames = buffer.Frames();

  EXPECT_EQ(kBatch * 2, buffer.PendingDecode());

  // Add frames until it tells us to stop.
  bool keepDecoding;
  do {
    keepDecoding = buffer.Insert(CreateEmptyFrame());
    EXPECT_FALSE(buffer.SizeKnown());
    EXPECT_FALSE(buffer.MayDiscard());
  } while (keepDecoding);

  EXPECT_EQ(size_t(0), buffer.PendingDecode());
  EXPECT_EQ(size_t(6), frames.Length());

  // Progress through the animation until it lets us decode again.
  bool restartDecoder = false;
  size_t i = 0;
  do {
    DrawableFrameRef gotFrame = buffer.Get(i);
    EXPECT_TRUE(gotFrame);
    if (i > 0) {
      restartDecoder = buffer.AdvanceTo(i);
    }
    ++i;
  } while (!restartDecoder);

  EXPECT_EQ(size_t(3), buffer.PendingDecode());
  EXPECT_EQ(size_t(3), buffer.Displayed());

  // Add more frames.
  do {
    keepDecoding = buffer.Insert(CreateEmptyFrame());
    EXPECT_FALSE(buffer.SizeKnown());
  } while (keepDecoding);

  EXPECT_TRUE(buffer.MayDiscard());
  EXPECT_EQ(size_t(0), buffer.PendingDecode());
  EXPECT_EQ(size_t(9), frames.Length());

  // It should have be able to remove two frames given we have advanced to the
  // fourth frame.
  CheckRetained(buffer, 0, 1);
  CheckRemoved(buffer, 1, 3);
  CheckRetained(buffer, 3, 9);

  // Progress through the animation so more. Make sure it removes frames as we
  // go along.
  do {
    DrawableFrameRef gotFrame = buffer.Get(i);
    EXPECT_TRUE(gotFrame);
    restartDecoder = buffer.AdvanceTo(i);
    EXPECT_FALSE(frames[i - 1]);
    EXPECT_TRUE(frames[i]);
    i++;
  } while (!restartDecoder);

  EXPECT_EQ(size_t(3), buffer.PendingDecode());
  EXPECT_EQ(size_t(6), buffer.Displayed());

  // Add the last frame. It should still let us add more frames, but the next
  // frame will restart at the beginning.
  keepDecoding = buffer.Insert(CreateEmptyFrame());
  EXPECT_TRUE(keepDecoding);
  keepDecoding = buffer.MarkComplete();
  EXPECT_TRUE(keepDecoding);
  EXPECT_TRUE(buffer.SizeKnown());
  EXPECT_EQ(size_t(2), buffer.PendingDecode());
  EXPECT_EQ(size_t(10), frames.Length());
  EXPECT_FALSE(buffer.HasRedecodeError());

  // Use remaining pending room. It shouldn't add new frames, only replace.
  do {
    keepDecoding = buffer.Insert(CreateEmptyFrame());
  } while (keepDecoding);

  EXPECT_EQ(size_t(0), buffer.PendingDecode());
  EXPECT_EQ(size_t(10), frames.Length());

  // Advance as far as we can. This should require us to loop the animation to
  // reach a missing frame.
  do {
    if (i == frames.Length()) {
      i = 0;
    }

    DrawableFrameRef gotFrame = buffer.Get(i);
    if (!gotFrame) {
      break;
    }

    restartDecoder = buffer.AdvanceTo(i);
    ++i;
  } while (true);

  EXPECT_EQ(size_t(3), buffer.PendingDecode());
  EXPECT_EQ(size_t(2), i);
  EXPECT_EQ(size_t(1), buffer.Displayed());

  // Decode some more.
  keepDecoding = Fill(buffer, buffer.PendingDecode());
  EXPECT_FALSE(keepDecoding);
  EXPECT_EQ(size_t(0), buffer.PendingDecode());

  // Can we retry advancing again?
  DrawableFrameRef gotFrame = buffer.Get(i);
  EXPECT_TRUE(gotFrame);
  restartDecoder = buffer.AdvanceTo(i);
  EXPECT_EQ(size_t(2), buffer.Displayed());
  EXPECT_FALSE(frames[i - 1]);
  EXPECT_TRUE(frames[i]);

  // Since we are above the threshold, we must reset everything.
  restartDecoder = buffer.Reset();
  EXPECT_FALSE(restartDecoder);
  CheckRetained(buffer, 0, 1);
  CheckRemoved(buffer, 1, frames.Length());
  EXPECT_EQ(kBatch * 2, buffer.PendingDecode());
  EXPECT_EQ(size_t(0), buffer.PendingAdvance());
  EXPECT_EQ(size_t(0), buffer.Displayed());
}

TEST_F(ImageAnimationFrameBuffer, ResetIncompleteAboveThreshold)
{
  const size_t kThreshold = 5;
  const size_t kBatch = 2;
  AnimationFrameBuffer buffer;
  buffer.Initialize(kThreshold, kBatch, 0);
  const auto& frames = buffer.Frames();

  // Add frames until we exceed the threshold.
  bool keepDecoding;
  bool restartDecoder;
  size_t i = 0;
  do {
    keepDecoding = buffer.Insert(CreateEmptyFrame());
    EXPECT_TRUE(keepDecoding);
    if (i > 0) {
      restartDecoder = buffer.AdvanceTo(i);
      EXPECT_FALSE(restartDecoder);
    }
    ++i;
  } while (!buffer.MayDiscard());

  // Should have threshold + 1 frames, and still not complete.
  EXPECT_EQ(size_t(6), frames.Length());
  EXPECT_FALSE(buffer.SizeKnown());

  // Restart the animation, we still had pending frames to decode since we
  // advanced in lockstep, so it should not ask us to restart the decoder.
  restartDecoder = buffer.Reset();
  EXPECT_FALSE(restartDecoder);
  CheckRetained(buffer, 0, 1);
  CheckRemoved(buffer, 1, frames.Length());
  EXPECT_EQ(kBatch * 2, buffer.PendingDecode());
  EXPECT_EQ(size_t(0), buffer.PendingAdvance());
  EXPECT_EQ(size_t(0), buffer.Displayed());

  // Adding new frames should not grow the insertion array, but instead
  // should reuse the space already allocated. Given that we are able to
  // discard frames once we cross the threshold, we should confirm that
  // we only do so if we have advanced beyond them.
  size_t oldFramesLength = frames.Length();
  size_t advanceUpTo = frames.Length() - kBatch;
  for (i = 0; i < oldFramesLength; ++i) {
    keepDecoding = buffer.Insert(CreateEmptyFrame());
    EXPECT_TRUE(keepDecoding);
    EXPECT_TRUE(frames[i]);
    EXPECT_EQ(oldFramesLength, frames.Length());
    if (i > 0) {
      // If we stop advancing, we should still retain the previous frames.
      EXPECT_TRUE(frames[i-1]);
      if (i <= advanceUpTo) {
        restartDecoder = buffer.AdvanceTo(i);
        EXPECT_FALSE(restartDecoder);
      }
    }
  }

  // Add one more frame. It should have grown the array this time.
  keepDecoding = buffer.Insert(CreateEmptyFrame());
  EXPECT_TRUE(keepDecoding);
  ASSERT_EQ(i + 1, frames.Length());
  EXPECT_TRUE(frames[i]);
}

TEST_F(ImageAnimationFrameBuffer, StartAfterBeginning)
{
  const size_t kThreshold = 30;
  const size_t kBatch = 2;
  const size_t kStartFrame = 7;
  AnimationFrameBuffer buffer;
  buffer.Initialize(kThreshold, kBatch, kStartFrame);

  EXPECT_EQ(kStartFrame, buffer.PendingAdvance());

  // Add frames until it tells us to stop. It should be later than before,
  // because it auto-advances until its displayed frame is kStartFrame.
  bool keepDecoding;
  size_t i = 0;
  do {
    keepDecoding = buffer.Insert(CreateEmptyFrame());
    EXPECT_FALSE(buffer.SizeKnown());
    EXPECT_FALSE(buffer.MayDiscard());

    if (i <= kStartFrame) {
      EXPECT_EQ(i, buffer.Displayed());
      EXPECT_EQ(kStartFrame - i, buffer.PendingAdvance());
    } else {
      EXPECT_EQ(kStartFrame, buffer.Displayed());
      EXPECT_EQ(size_t(0), buffer.PendingAdvance());
    }

    i++;
  } while (keepDecoding);

  EXPECT_EQ(size_t(0), buffer.PendingDecode());
  EXPECT_EQ(size_t(0), buffer.PendingAdvance());
  EXPECT_EQ(size_t(10), buffer.Frames().Length());
}

TEST_F(ImageAnimationFrameBuffer, StartAfterBeginningAndReset)
{
  const size_t kThreshold = 30;
  const size_t kBatch = 2;
  const size_t kStartFrame = 7;
  AnimationFrameBuffer buffer;
  buffer.Initialize(kThreshold, kBatch, kStartFrame);

  EXPECT_EQ(kStartFrame, buffer.PendingAdvance());

  // Add frames until it tells us to stop. It should be later than before,
  // because it auto-advances until its displayed frame is kStartFrame.
  for (size_t i = 0; i < 5; ++i) {
    bool keepDecoding = buffer.Insert(CreateEmptyFrame());
    EXPECT_TRUE(keepDecoding);
    EXPECT_FALSE(buffer.SizeKnown());
    EXPECT_FALSE(buffer.MayDiscard());
    EXPECT_EQ(i, buffer.Displayed());
    EXPECT_EQ(kStartFrame - i, buffer.PendingAdvance());
  }

  // When we reset the animation, it goes back to the beginning. That means
  // we can forget about what we were told to advance to at the start. While
  // we have plenty of frames in our buffer, we still need one more because
  // in the real scenario, the decoder thread is still running and it is easier
  // to let it insert its last frame than to coordinate quitting earlier.
  buffer.Reset();
  EXPECT_EQ(size_t(0), buffer.Displayed());
  EXPECT_EQ(size_t(1), buffer.PendingDecode());
  EXPECT_EQ(size_t(0), buffer.PendingAdvance());
}

TEST_F(ImageAnimationFrameBuffer, RedecodeMoreFrames)
{
  const size_t kThreshold = 5;
  const size_t kBatch = 2;
  AnimationFrameBuffer buffer;
  buffer.Initialize(kThreshold, kBatch, 0);
  const auto& frames = buffer.Frames();

  // Add frames until we exceed the threshold.
  bool keepDecoding;
  bool restartDecoder;
  size_t i = 0;
  do {
    keepDecoding = buffer.Insert(CreateEmptyFrame());
    EXPECT_TRUE(keepDecoding);
    if (i > 0) {
      restartDecoder = buffer.AdvanceTo(i);
      EXPECT_FALSE(restartDecoder);
    }
    ++i;
  } while (!buffer.MayDiscard());

  // Should have threshold + 1 frames, and still not complete.
  EXPECT_EQ(size_t(6), frames.Length());
  EXPECT_FALSE(buffer.SizeKnown());

  // Now we lock in at 6 frames.
  keepDecoding = buffer.MarkComplete();
  EXPECT_TRUE(keepDecoding);
  EXPECT_TRUE(buffer.SizeKnown());
  EXPECT_FALSE(buffer.HasRedecodeError());

  // Reinsert 6 frames first.
  i = 0;
  do {
    keepDecoding = buffer.Insert(CreateEmptyFrame());
    EXPECT_TRUE(keepDecoding);
    restartDecoder = buffer.AdvanceTo(i);
    EXPECT_FALSE(restartDecoder);
    ++i;
  } while (i < 6);

  // We should now encounter an error and shutdown further decodes.
  keepDecoding = buffer.Insert(CreateEmptyFrame());
  EXPECT_FALSE(keepDecoding);
  EXPECT_EQ(size_t(0), buffer.PendingDecode());
  EXPECT_TRUE(buffer.HasRedecodeError());
}

TEST_F(ImageAnimationFrameBuffer, RedecodeFewerFrames)
{
  const size_t kThreshold = 5;
  const size_t kBatch = 2;
  AnimationFrameBuffer buffer;
  buffer.Initialize(kThreshold, kBatch, 0);
  const auto& frames = buffer.Frames();

  // Add frames until we exceed the threshold.
  bool keepDecoding;
  bool restartDecoder;
  size_t i = 0;
  do {
    keepDecoding = buffer.Insert(CreateEmptyFrame());
    EXPECT_TRUE(keepDecoding);
    if (i > 0) {
      restartDecoder = buffer.AdvanceTo(i);
      EXPECT_FALSE(restartDecoder);
    }
    ++i;
  } while (!buffer.MayDiscard());

  // Should have threshold + 1 frames, and still not complete.
  EXPECT_EQ(size_t(6), frames.Length());
  EXPECT_FALSE(buffer.SizeKnown());

  // Now we lock in at 6 frames.
  keepDecoding = buffer.MarkComplete();
  EXPECT_TRUE(keepDecoding);
  EXPECT_TRUE(buffer.SizeKnown());
  EXPECT_FALSE(buffer.HasRedecodeError());

  // Reinsert 5 frames before marking complete.
  i = 0;
  do {
    keepDecoding = buffer.Insert(CreateEmptyFrame());
    EXPECT_TRUE(keepDecoding);
    restartDecoder = buffer.AdvanceTo(i);
    EXPECT_FALSE(restartDecoder);
    ++i;
  } while (i < 5);

  // We should now encounter an error and shutdown further decodes.
  keepDecoding = buffer.MarkComplete();
  EXPECT_FALSE(keepDecoding);
  EXPECT_EQ(size_t(0), buffer.PendingDecode());
  EXPECT_TRUE(buffer.HasRedecodeError());
}

TEST_F(ImageAnimationFrameBuffer, RedecodeFewerFramesAndBehindAdvancing)
{
  const size_t kThreshold = 5;
  const size_t kBatch = 2;
  AnimationFrameBuffer buffer;
  buffer.Initialize(kThreshold, kBatch, 0);
  const auto& frames = buffer.Frames();

  // Add frames until we exceed the threshold.
  bool keepDecoding;
  bool restartDecoder;
  size_t i = 0;
  do {
    keepDecoding = buffer.Insert(CreateEmptyFrame());
    EXPECT_TRUE(keepDecoding);
    if (i > 0) {
      restartDecoder = buffer.AdvanceTo(i);
      EXPECT_FALSE(restartDecoder);
    }
    ++i;
  } while (!buffer.MayDiscard());

  // Should have threshold + 1 frames, and still not complete.
  EXPECT_EQ(size_t(6), frames.Length());
  EXPECT_FALSE(buffer.SizeKnown());

  // Now we lock in at 6 frames.
  keepDecoding = buffer.MarkComplete();
  EXPECT_TRUE(keepDecoding);
  EXPECT_TRUE(buffer.SizeKnown());
  EXPECT_FALSE(buffer.HasRedecodeError());

  // Reinsert frames without advancing until we exhaust our pending space. This
  // should be less than the current buffer length by definition.
  i = 0;
  do {
    keepDecoding = buffer.Insert(CreateEmptyFrame());
    ++i;
  } while (keepDecoding);

  EXPECT_EQ(size_t(2), i);

  // We should now encounter an error and shutdown further decodes.
  keepDecoding = buffer.MarkComplete();
  EXPECT_FALSE(keepDecoding);
  EXPECT_EQ(size_t(0), buffer.PendingDecode());
  EXPECT_TRUE(buffer.HasRedecodeError());

  // We should however be able to continue advancing to the last decoded frame
  // without it requesting the decoder to restart.
  i = 0;
  do {
    restartDecoder = buffer.AdvanceTo(i);
    EXPECT_FALSE(restartDecoder);
    ++i;
  } while (i < 2);
}

