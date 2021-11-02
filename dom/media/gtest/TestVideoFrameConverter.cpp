/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <iterator>

#include "gtest/gtest.h"
#include "MediaEventSource.h"
#include "VideoFrameConverter.h"
#include "WaitFor.h"
#include "YUVBufferGenerator.h"

using namespace mozilla;

class VideoFrameConverterTest;

class FrameListener : public VideoConverterListener {
 public:
  void OnVideoFrameConverted(const webrtc::VideoFrame& aVideoFrame) override {
    mVideoFrameConvertedEvent.Notify(aVideoFrame, TimeStamp::Now());
  }

  MediaEventSource<webrtc::VideoFrame, TimeStamp>& VideoFrameConvertedEvent() {
    return mVideoFrameConvertedEvent;
  }

 private:
  MediaEventProducer<webrtc::VideoFrame, TimeStamp> mVideoFrameConvertedEvent;
};

class VideoFrameConverterTest : public ::testing::Test {
 protected:
  RefPtr<VideoFrameConverter> mConverter;
  RefPtr<FrameListener> mListener;

  VideoFrameConverterTest()
      : mConverter(MakeAndAddRef<VideoFrameConverter>()),
        mListener(MakeAndAddRef<FrameListener>()) {
    mConverter->AddListener(mListener);
  }

  void TearDown() override { mConverter->Shutdown(); }

  RefPtr<TakeNPromise<webrtc::VideoFrame, TimeStamp>> TakeNConvertedFrames(
      size_t aN) {
    return TakeN(mListener->VideoFrameConvertedEvent(), aN);
  }
};

static bool IsPlane(const uint8_t* aData, int aWidth, int aHeight, int aStride,
                    uint8_t aValue) {
  for (int i = 0; i < aHeight; ++i) {
    for (int j = 0; j < aWidth; ++j) {
      if (aData[i * aStride + j] != aValue) {
        return false;
      }
    }
  }
  return true;
}

static bool IsFrameBlack(const webrtc::VideoFrame& aFrame) {
  RefPtr<webrtc::I420BufferInterface> buffer =
      aFrame.video_frame_buffer()->ToI420().get();
  return IsPlane(buffer->DataY(), buffer->width(), buffer->height(),
                 buffer->StrideY(), 0x00) &&
         IsPlane(buffer->DataU(), buffer->ChromaWidth(), buffer->ChromaHeight(),
                 buffer->StrideU(), 0x80) &&
         IsPlane(buffer->DataV(), buffer->ChromaWidth(), buffer->ChromaHeight(),
                 buffer->StrideV(), 0x80);
}

VideoChunk GenerateChunk(int32_t aWidth, int32_t aHeight, TimeStamp aTime) {
  YUVBufferGenerator generator;
  generator.Init(gfx::IntSize(aWidth, aHeight));
  VideoFrame f(generator.GenerateI420Image(), gfx::IntSize(aWidth, aHeight));
  VideoChunk c;
  c.mFrame.TakeFrom(&f);
  c.mTimeStamp = aTime;
  c.mDuration = 0;
  return c;
}

static TimeDuration SameFrameTimeDuration() {
  // On some platforms, particularly Windows, we have observed the same-frame
  // timer firing early. To not unittest the timer itself we allow a tiny amount
  // of fuzziness in when the timer is allowed to fire.
  return TimeDuration::FromSeconds(1) - TimeDuration::FromMilliseconds(0.5);
}

TEST_F(VideoFrameConverterTest, BasicConversion) {
  auto framesPromise = TakeNConvertedFrames(1);
  TimeStamp now = TimeStamp::Now();
  VideoChunk chunk = GenerateChunk(640, 480, now);
  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(chunk, false);
  auto frames = WaitFor(framesPromise).unwrap();
  ASSERT_EQ(frames.size(), 1U);
  const auto& [frame, conversionTime] = frames[0];
  EXPECT_EQ(frame.width(), 640);
  EXPECT_EQ(frame.height(), 480);
  EXPECT_FALSE(IsFrameBlack(frame));
  EXPECT_GT(conversionTime - now, TimeDuration::FromMilliseconds(0));
}

TEST_F(VideoFrameConverterTest, BasicPacing) {
  auto framesPromise = TakeNConvertedFrames(1);
  TimeStamp now = TimeStamp::Now();
  TimeStamp future = now + TimeDuration::FromMilliseconds(100);
  VideoChunk chunk = GenerateChunk(640, 480, future);
  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(chunk, false);
  auto frames = WaitFor(framesPromise).unwrap();
  EXPECT_GT(TimeStamp::Now() - now, future - now);
  ASSERT_EQ(frames.size(), 1U);
  const auto& [frame, conversionTime] = frames[0];
  EXPECT_EQ(frame.width(), 640);
  EXPECT_EQ(frame.height(), 480);
  EXPECT_FALSE(IsFrameBlack(frame));
  EXPECT_GT(conversionTime - now, future - now);
}

TEST_F(VideoFrameConverterTest, MultiPacing) {
  auto framesPromise = TakeNConvertedFrames(2);
  TimeStamp now = TimeStamp::Now();
  TimeStamp future1 = now + TimeDuration::FromMilliseconds(100);
  TimeStamp future2 = now + TimeDuration::FromMilliseconds(200);
  VideoChunk chunk = GenerateChunk(640, 480, future1);
  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(chunk, false);
  chunk = GenerateChunk(640, 480, future2);
  mConverter->QueueVideoChunk(chunk, false);
  auto frames = WaitFor(framesPromise).unwrap();
  EXPECT_GT(TimeStamp::Now(), future2);
  ASSERT_EQ(frames.size(), 2U);
  const auto& [frame0, conversionTime0] = frames[0];
  EXPECT_EQ(frame0.width(), 640);
  EXPECT_EQ(frame0.height(), 480);
  EXPECT_FALSE(IsFrameBlack(frame0));
  EXPECT_GT(conversionTime0 - now, future1 - now);

  const auto& [frame1, conversionTime1] = frames[1];
  EXPECT_EQ(frame1.width(), 640);
  EXPECT_EQ(frame1.height(), 480);
  EXPECT_FALSE(IsFrameBlack(frame1));
  EXPECT_GT(conversionTime1, future2);
  EXPECT_GT(conversionTime1 - now, conversionTime0 - now);
}

TEST_F(VideoFrameConverterTest, Duplication) {
  auto framesPromise = TakeNConvertedFrames(2);
  TimeStamp now = TimeStamp::Now();
  TimeStamp future1 = now + TimeDuration::FromMilliseconds(100);
  VideoChunk chunk = GenerateChunk(640, 480, future1);
  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(chunk, false);
  auto frames = WaitFor(framesPromise).unwrap();
  EXPECT_GT(TimeStamp::Now() - now,
            SameFrameTimeDuration() + TimeDuration::FromMilliseconds(100));
  ASSERT_EQ(frames.size(), 2U);
  const auto& [frame0, conversionTime0] = frames[0];
  EXPECT_EQ(frame0.width(), 640);
  EXPECT_EQ(frame0.height(), 480);
  EXPECT_FALSE(IsFrameBlack(frame0));
  EXPECT_GT(conversionTime0, future1);

  const auto& [frame1, conversionTime1] = frames[1];
  EXPECT_EQ(frame1.width(), 640);
  EXPECT_EQ(frame1.height(), 480);
  EXPECT_FALSE(IsFrameBlack(frame1));
  EXPECT_GT(conversionTime1 - now,
            SameFrameTimeDuration() + TimeDuration::FromMilliseconds(100));
  // Check that the second frame comes between 1s and 2s after the first.
  EXPECT_GT(TimeDuration::FromMicroseconds(frame1.timestamp_us()) -
                TimeDuration::FromMicroseconds(frame0.timestamp_us()),
            SameFrameTimeDuration());
  EXPECT_LT(TimeDuration::FromMicroseconds(frame1.timestamp_us()) -
                TimeDuration::FromMicroseconds(frame0.timestamp_us()),
            TimeDuration::FromSeconds(2));
}

TEST_F(VideoFrameConverterTest, DropsOld) {
  auto framesPromise = TakeNConvertedFrames(1);
  TimeStamp now = TimeStamp::Now();
  TimeStamp future1 = now + TimeDuration::FromMilliseconds(1000);
  TimeStamp future2 = now + TimeDuration::FromMilliseconds(100);
  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(GenerateChunk(800, 600, future1), false);
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, future2), false);
  auto frames = WaitFor(framesPromise).unwrap();
  EXPECT_GT(TimeStamp::Now(), future2);
  ASSERT_EQ(frames.size(), 1U);
  const auto& [frame, conversionTime] = frames[0];
  EXPECT_EQ(frame.width(), 640);
  EXPECT_EQ(frame.height(), 480);
  EXPECT_FALSE(IsFrameBlack(frame));
  EXPECT_GT(conversionTime - now, future2 - now);
}

// We check that the disabling code was triggered by sending multiple,
// different, frames to the converter within one second. While black, it shall
// treat all frames identical and issue one black frame per second.
TEST_F(VideoFrameConverterTest, BlackOnDisable) {
  auto framesPromise = TakeNConvertedFrames(2);
  TimeStamp now = TimeStamp::Now();
  TimeStamp future1 = now + TimeDuration::FromMilliseconds(100);
  TimeStamp future2 = now + TimeDuration::FromMilliseconds(200);
  TimeStamp future3 = now + TimeDuration::FromMilliseconds(400);
  mConverter->SetActive(true);
  mConverter->SetTrackEnabled(false);
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, future1), false);
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, future2), false);
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, future3), false);
  auto frames = WaitFor(framesPromise).unwrap();
  EXPECT_GT(TimeStamp::Now() - now, SameFrameTimeDuration());
  ASSERT_EQ(frames.size(), 2U);
  // The first frame was created instantly by SetTrackEnabled().
  const auto& [frame0, conversionTime0] = frames[0];
  EXPECT_EQ(frame0.width(), 640);
  EXPECT_EQ(frame0.height(), 480);
  EXPECT_TRUE(IsFrameBlack(frame0));
  EXPECT_GT(conversionTime0 - now, TimeDuration::FromSeconds(0));
  // The second frame was created by the same-frame timer (after 1s).
  const auto& [frame1, conversionTime1] = frames[1];
  EXPECT_EQ(frame1.width(), 640);
  EXPECT_EQ(frame1.height(), 480);
  EXPECT_TRUE(IsFrameBlack(frame1));
  EXPECT_GT(conversionTime1 - now, SameFrameTimeDuration());
  // Check that the second frame comes between 1s and 2s after the first.
  EXPECT_NEAR(frame1.timestamp_us(),
              frame0.timestamp_us() + ((PR_USEC_PER_SEC * 3) / 2),
              PR_USEC_PER_SEC / 2);
}

TEST_F(VideoFrameConverterTest, ClearFutureFramesOnJumpingBack) {
  TimeStamp start = TimeStamp::Now();
  TimeStamp future1 = start + TimeDuration::FromMilliseconds(100);

  auto framesPromise = TakeNConvertedFrames(1);
  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, future1), false);
  auto frames = WaitFor(framesPromise).unwrap();

  // We are now at t=100ms+. Queue a future frame and jump back in time to
  // signal a reset.

  framesPromise = TakeNConvertedFrames(1);
  TimeStamp step1 = TimeStamp::Now();
  ASSERT_GT(step1 - start, future1 - start);
  TimeStamp future2 = step1 + TimeDuration::FromMilliseconds(200);
  TimeStamp future3 = step1 + TimeDuration::FromMilliseconds(100);
  ASSERT_LT(future2 - start, future1 + SameFrameTimeDuration() - start);
  mConverter->QueueVideoChunk(GenerateChunk(800, 600, future2), false);
  VideoChunk nullChunk;
  nullChunk.mFrame = VideoFrame(nullptr, gfx::IntSize(800, 600));
  nullChunk.mTimeStamp = step1;
  mConverter->QueueVideoChunk(nullChunk, false);

  // We queue one more chunk after the reset so we don't have to wait a full
  // second for the same-frame timer. It has a different time and resolution
  // so we can differentiate them.
  mConverter->QueueVideoChunk(GenerateChunk(320, 240, future3), false);

  {
    auto newFrames = WaitFor(framesPromise).unwrap();
    frames.insert(frames.end(), std::make_move_iterator(newFrames.begin()),
                  std::make_move_iterator(newFrames.end()));
  }
  TimeStamp step2 = TimeStamp::Now();
  EXPECT_GT(step2 - start, future3 - start);
  ASSERT_EQ(frames.size(), 2U);
  const auto& [frame0, conversionTime0] = frames[0];
  EXPECT_EQ(frame0.width(), 640);
  EXPECT_EQ(frame0.height(), 480);
  EXPECT_FALSE(IsFrameBlack(frame0));
  EXPECT_GT(conversionTime0 - start, future1 - start);
  const auto& [frame1, conversionTime1] = frames[1];
  EXPECT_EQ(frame1.width(), 320);
  EXPECT_EQ(frame1.height(), 240);
  EXPECT_FALSE(IsFrameBlack(frame1));
  EXPECT_GT(conversionTime1 - start, future3 - start);
}

// We check that the no frame is converted while inactive, and that on
// activating the most recently queued frame gets converted.
TEST_F(VideoFrameConverterTest, NoConversionsWhileInactive) {
  auto framesPromise = TakeNConvertedFrames(1);
  TimeStamp now = TimeStamp::Now();
  TimeStamp future1 = now - TimeDuration::FromMilliseconds(1);
  TimeStamp future2 = now;
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, future1), false);
  mConverter->QueueVideoChunk(GenerateChunk(800, 600, future2), false);

  // SetActive needs to follow the same async path as the frames to be in sync.
  auto q =
      MakeRefPtr<TaskQueue>(GetMediaThreadPool(MediaThreadType::WEBRTC_WORKER),
                            "VideoFrameConverterTest");
  auto timer = MakeRefPtr<MediaTimer>(false);
  timer->WaitFor(TimeDuration::FromMilliseconds(100), __func__)
      ->Then(q, __func__,
             [converter = mConverter] { converter->SetActive(true); });

  auto frames = WaitFor(framesPromise).unwrap();
  ASSERT_EQ(frames.size(), 1U);
  const auto& [frame, conversionTime] = frames[0];
  Unused << conversionTime;
  EXPECT_EQ(frame.width(), 800);
  EXPECT_EQ(frame.height(), 600);
  EXPECT_FALSE(IsFrameBlack(frame));
}
