/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/Move.h"
#include "AnimationFrameBuffer.h"

using namespace mozilla;
using namespace mozilla::image;

static already_AddRefed<imgFrame>
CreateEmptyFrame()
{
  RefPtr<imgFrame> frame = new imgFrame();
  nsresult rv = frame->InitForAnimator(nsIntSize(1, 1), SurfaceFormat::B8G8R8A8);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  RawAccessFrameRef frameRef = frame->RawAccessRef();
  frame->Finish();
  return frame.forget();
}

class ImageAnimationFrameBuffer : public ::testing::Test
{
public:
  ImageAnimationFrameBuffer()
  { }

private:
  AutoInitializeImageLib mInit;
};

TEST_F(ImageAnimationFrameBuffer, RetainedInitialState)
{
  const size_t kThreshold = 800;
  const size_t kBatch = 100;
  AnimationFrameRetainedBuffer buffer(kThreshold, kBatch, 0);

  EXPECT_EQ(kThreshold, buffer.Threshold());
  EXPECT_EQ(kBatch, buffer.Batch());
  EXPECT_EQ(size_t(0), buffer.Displayed());
  EXPECT_EQ(kBatch * 2, buffer.PendingDecode());
  EXPECT_EQ(size_t(0), buffer.PendingAdvance());
  EXPECT_FALSE(buffer.MayDiscard());
  EXPECT_FALSE(buffer.SizeKnown());
  EXPECT_EQ(size_t(0), buffer.Size());
}

TEST_F(ImageAnimationFrameBuffer, ThresholdTooSmall)
{
  const size_t kThreshold = 0;
  const size_t kBatch = 10;
  AnimationFrameRetainedBuffer buffer(kThreshold, kBatch, 0);

  EXPECT_EQ(kBatch * 2 + 1, buffer.Threshold());
  EXPECT_EQ(kBatch, buffer.Batch());
  EXPECT_EQ(kBatch * 2, buffer.PendingDecode());
  EXPECT_EQ(size_t(0), buffer.PendingAdvance());
}

TEST_F(ImageAnimationFrameBuffer, BatchTooSmall)
{
  const size_t kThreshold = 10;
  const size_t kBatch = 0;
  AnimationFrameRetainedBuffer buffer(kThreshold, kBatch, 0);

  EXPECT_EQ(kThreshold, buffer.Threshold());
  EXPECT_EQ(size_t(1), buffer.Batch());
  EXPECT_EQ(size_t(2), buffer.PendingDecode());
  EXPECT_EQ(size_t(0), buffer.PendingAdvance());
}

TEST_F(ImageAnimationFrameBuffer, BatchTooBig)
{
  const size_t kThreshold = 50;
  const size_t kBatch = SIZE_MAX;
  AnimationFrameRetainedBuffer buffer(kThreshold, kBatch, 0);

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
  AnimationFrameRetainedBuffer buffer(kThreshold, kBatch, 0);
  const auto& frames = buffer.Frames();

  EXPECT_EQ(kBatch * 2, buffer.PendingDecode());

  RefPtr<imgFrame> firstFrame;
  for (size_t i = 0; i < 5; ++i) {
    RefPtr<imgFrame> frame = CreateEmptyFrame();
    auto status = buffer.Insert(RefPtr<imgFrame>(frame));
    EXPECT_EQ(status, AnimationFrameBuffer::InsertStatus::CONTINUE);
    EXPECT_FALSE(buffer.SizeKnown());
    EXPECT_EQ(buffer.Size(), i + 1);

    if (i == 4) {
      EXPECT_EQ(size_t(15), buffer.PendingDecode());
      bool keepDecoding = buffer.MarkComplete(IntRect(0, 0, 1, 1));
      EXPECT_FALSE(keepDecoding);
      EXPECT_TRUE(buffer.SizeKnown());
      EXPECT_EQ(size_t(0), buffer.PendingDecode());
      EXPECT_FALSE(buffer.HasRedecodeError());
    }

    EXPECT_FALSE(buffer.MayDiscard());

    imgFrame* gotFrame = buffer.Get(i, false);
    EXPECT_EQ(frame.get(), gotFrame);
    ASSERT_EQ(i + 1, frames.Length());
    EXPECT_EQ(frame.get(), frames[i].get());

    if (i == 0) {
      firstFrame = std::move(frame);
      EXPECT_EQ(size_t(0), buffer.Displayed());
    } else {
      EXPECT_EQ(i - 1, buffer.Displayed());
      bool restartDecoder = buffer.AdvanceTo(i);
      EXPECT_FALSE(restartDecoder);
      EXPECT_EQ(i, buffer.Displayed());
    }

    gotFrame = buffer.Get(0, false);
    EXPECT_EQ(firstFrame.get(), gotFrame);
  }

  // Loop again over the animation and make sure it is still all there.
  for (size_t i = 0; i < frames.Length(); ++i) {
    EXPECT_TRUE(buffer.Get(i, false) != nullptr);

    bool restartDecoder = buffer.AdvanceTo(i);
    EXPECT_FALSE(restartDecoder);
  }
}

TEST_F(ImageAnimationFrameBuffer, FinishMultipleBatchesUnderThreshold)
{
  const size_t kThreshold = 30;
  const size_t kBatch = 2;
  AnimationFrameRetainedBuffer buffer(kThreshold, kBatch, 0);
  const auto& frames = buffer.Frames();

  EXPECT_EQ(kBatch * 2, buffer.PendingDecode());

  // Add frames until it tells us to stop.
  AnimationFrameBuffer::InsertStatus status;
  do {
    status = buffer.Insert(CreateEmptyFrame());
    EXPECT_FALSE(buffer.SizeKnown());
    EXPECT_FALSE(buffer.MayDiscard());
  } while (status == AnimationFrameBuffer::InsertStatus::CONTINUE);

  EXPECT_EQ(size_t(0), buffer.PendingDecode());
  EXPECT_EQ(size_t(4), frames.Length());
  EXPECT_EQ(status, AnimationFrameBuffer::InsertStatus::YIELD);

  // Progress through the animation until it lets us decode again.
  bool restartDecoder = false;
  size_t i = 0;
  do {
    EXPECT_TRUE(buffer.Get(i, false) != nullptr);
    if (i > 0) {
      restartDecoder = buffer.AdvanceTo(i);
    }
    ++i;
  } while (!restartDecoder);

  EXPECT_EQ(size_t(2), buffer.PendingDecode());
  EXPECT_EQ(size_t(2), buffer.Displayed());

  // Add the last frame.
  status = buffer.Insert(CreateEmptyFrame());
  EXPECT_EQ(status, AnimationFrameBuffer::InsertStatus::CONTINUE);
  bool keepDecoding = buffer.MarkComplete(IntRect(0, 0, 1, 1));
  EXPECT_FALSE(keepDecoding);
  EXPECT_TRUE(buffer.SizeKnown());
  EXPECT_EQ(size_t(0), buffer.PendingDecode());
  EXPECT_EQ(size_t(5), frames.Length());
  EXPECT_FALSE(buffer.HasRedecodeError());

  // Finish progressing through the animation.
  for ( ; i < frames.Length(); ++i) {
    EXPECT_TRUE(buffer.Get(i, false) != nullptr);
    restartDecoder = buffer.AdvanceTo(i);
    EXPECT_FALSE(restartDecoder);
  }

  // Loop again over the animation and make sure it is still all there.
  for (i = 0; i < frames.Length(); ++i) {
    EXPECT_TRUE(buffer.Get(i, false) != nullptr);
    restartDecoder = buffer.AdvanceTo(i);
    EXPECT_FALSE(restartDecoder);
  }

  // Loop to the third frame and then reset the animation.
  for (i = 0; i < 3; ++i) {
    EXPECT_TRUE(buffer.Get(i, false) != nullptr);
    restartDecoder = buffer.AdvanceTo(i);
    EXPECT_FALSE(restartDecoder);
  }

  // Since we are below the threshold, we can reset the get index only.
  // Nothing else should have changed.
  restartDecoder = buffer.Reset();
  EXPECT_FALSE(restartDecoder);
  for (i = 0; i < 5; ++i) {
    EXPECT_TRUE(buffer.Get(i, false) != nullptr);
  }
  EXPECT_EQ(size_t(0), buffer.PendingDecode());
  EXPECT_EQ(size_t(0), buffer.PendingAdvance());
  EXPECT_EQ(size_t(0), buffer.Displayed());
}

TEST_F(ImageAnimationFrameBuffer, StartAfterBeginning)
{
  const size_t kThreshold = 30;
  const size_t kBatch = 2;
  const size_t kStartFrame = 7;
  AnimationFrameRetainedBuffer buffer(kThreshold, kBatch, kStartFrame);

  EXPECT_EQ(kStartFrame, buffer.PendingAdvance());

  // Add frames until it tells us to stop. It should be later than before,
  // because it auto-advances until its displayed frame is kStartFrame.
  AnimationFrameBuffer::InsertStatus status;
  size_t i = 0;
  do {
    status = buffer.Insert(CreateEmptyFrame());
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
  } while (status == AnimationFrameBuffer::InsertStatus::CONTINUE);

  EXPECT_EQ(size_t(0), buffer.PendingDecode());
  EXPECT_EQ(size_t(0), buffer.PendingAdvance());
  EXPECT_EQ(size_t(10), buffer.Size());
}

TEST_F(ImageAnimationFrameBuffer, StartAfterBeginningAndReset)
{
  const size_t kThreshold = 30;
  const size_t kBatch = 2;
  const size_t kStartFrame = 7;
  AnimationFrameRetainedBuffer buffer(kThreshold, kBatch, kStartFrame);

  EXPECT_EQ(kStartFrame, buffer.PendingAdvance());

  // Add frames until it tells us to stop. It should be later than before,
  // because it auto-advances until its displayed frame is kStartFrame.
  for (size_t i = 0; i < 5; ++i) {
    AnimationFrameBuffer::InsertStatus status =
      buffer.Insert(CreateEmptyFrame());
    EXPECT_EQ(status, AnimationFrameBuffer::InsertStatus::CONTINUE);
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
  EXPECT_EQ(size_t(5), buffer.Size());
}

