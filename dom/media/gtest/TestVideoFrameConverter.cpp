/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "VideoFrameConverter.h"
#include "YUVBufferGenerator.h"

using namespace mozilla;

class VideoFrameConverterTest;

class FrameListener : public VideoConverterListener {
 public:
  explicit FrameListener(VideoFrameConverterTest* aTest);
  void OnVideoFrameConverted(const webrtc::VideoFrame& aVideoFrame) override;

 private:
  VideoFrameConverterTest* mTest;
};

class VideoFrameConverterTest : public ::testing::Test {
 protected:
  using FrameType = Pair<webrtc::VideoFrame, TimeStamp>;
  Monitor mMonitor;
  RefPtr<VideoFrameConverter> mConverter;
  RefPtr<FrameListener> mListener;
  std::vector<FrameType> mConvertedFrames;

  VideoFrameConverterTest()
      : mMonitor("PacingFixture::mMonitor"),
        mConverter(MakeAndAddRef<VideoFrameConverter>()),
        mListener(MakeAndAddRef<FrameListener>(this)) {
    mConverter->AddListener(mListener);
  }

  void TearDown() override { mConverter->Shutdown(); }

  size_t NumConvertedFrames() {
    MonitorAutoLock lock(mMonitor);
    return mConvertedFrames.size();
  }

  std::vector<FrameType> WaitForNConverted(size_t aN) {
    MonitorAutoLock l(mMonitor);
    while (mConvertedFrames.size() < aN) {
      l.Wait();
    }
    std::vector<FrameType> v(mConvertedFrames.begin(),
                             mConvertedFrames.begin() + aN);
    return v;
  }

 public:
  void OnVideoFrameConverted(const webrtc::VideoFrame& aVideoFrame) {
    MonitorAutoLock lock(mMonitor);
    EXPECT_NE(aVideoFrame.timestamp_us(), 0);
    mConvertedFrames.push_back(MakePair(aVideoFrame, TimeStamp::Now()));
    mMonitor.Notify();
  }
};

FrameListener::FrameListener(VideoFrameConverterTest* aTest) : mTest(aTest) {}
void FrameListener::OnVideoFrameConverted(
    const webrtc::VideoFrame& aVideoFrame) {
  mTest->OnVideoFrameConverted(aVideoFrame);
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

TEST_F(VideoFrameConverterTest, BasicConversion) {
  TimeStamp now = TimeStamp::Now();
  VideoChunk chunk = GenerateChunk(640, 480, now);
  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(chunk, false);
  auto frames = WaitForNConverted(1);
  ASSERT_EQ(frames.size(), 1U);
  EXPECT_EQ(frames[0].first().width(), 640);
  EXPECT_EQ(frames[0].first().height(), 480);
  EXPECT_GT(frames[0].second(), now);
}

TEST_F(VideoFrameConverterTest, BasicPacing) {
  TimeStamp now = TimeStamp::Now();
  TimeStamp future = now + TimeDuration::FromMilliseconds(100);
  VideoChunk chunk = GenerateChunk(640, 480, future);
  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(chunk, false);
  auto frames = WaitForNConverted(1);
  EXPECT_GT(TimeStamp::Now(), future);
  ASSERT_EQ(frames.size(), 1U);
  EXPECT_EQ(frames[0].first().width(), 640);
  EXPECT_EQ(frames[0].first().height(), 480);
  EXPECT_GT(frames[0].second(), future);
}

TEST_F(VideoFrameConverterTest, MultiPacing) {
  TimeStamp now = TimeStamp::Now();
  TimeStamp future1 = now + TimeDuration::FromMilliseconds(100);
  TimeStamp future2 = now + TimeDuration::FromMilliseconds(200);
  VideoChunk chunk = GenerateChunk(640, 480, future1);
  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(chunk, false);
  chunk = GenerateChunk(640, 480, future2);
  mConverter->QueueVideoChunk(chunk, false);
  auto frames = WaitForNConverted(2);
  EXPECT_GT(TimeStamp::Now(), future2);
  ASSERT_EQ(frames.size(), 2U);
  EXPECT_EQ(frames[0].first().width(), 640);
  EXPECT_EQ(frames[0].first().height(), 480);
  EXPECT_GT(frames[0].second(), future1);
  EXPECT_EQ(frames[1].first().width(), 640);
  EXPECT_EQ(frames[1].first().height(), 480);
  EXPECT_GT(frames[1].second(), future2);
  EXPECT_GT(frames[1].second(), frames[0].second());
}

TEST_F(VideoFrameConverterTest, Duplication) {
  TimeStamp now = TimeStamp::Now();
  TimeStamp future1 = now + TimeDuration::FromMilliseconds(100);
  VideoChunk chunk = GenerateChunk(640, 480, future1);
  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(chunk, false);
  auto frames = WaitForNConverted(2);
  EXPECT_GT(TimeStamp::Now(), now + TimeDuration::FromMilliseconds(1100));
  ASSERT_EQ(frames.size(), 2U);
  EXPECT_EQ(frames[0].first().width(), 640);
  EXPECT_EQ(frames[0].first().height(), 480);
  EXPECT_GT(frames[0].second(), future1);
  EXPECT_EQ(frames[1].first().width(), 640);
  EXPECT_EQ(frames[1].first().height(), 480);
  EXPECT_GT(frames[1].second(), now + TimeDuration::FromMilliseconds(1100));
  // Check that the second frame comes between 1s and 2s after the first.
  EXPECT_NEAR(frames[1].first().timestamp_us(),
              frames[0].first().timestamp_us() + ((PR_USEC_PER_SEC * 3) / 2),
              PR_USEC_PER_SEC / 2);
}

TEST_F(VideoFrameConverterTest, DropsOld) {
  TimeStamp now = TimeStamp::Now();
  TimeStamp future1 = now + TimeDuration::FromMilliseconds(1000);
  TimeStamp future2 = now + TimeDuration::FromMilliseconds(100);
  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(GenerateChunk(800, 600, future1), false);
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, future2), false);
  auto frames = WaitForNConverted(1);
  EXPECT_GT(TimeStamp::Now(), future2);
  ASSERT_EQ(frames.size(), 1U);
  EXPECT_EQ(frames[0].first().width(), 640);
  EXPECT_EQ(frames[0].first().height(), 480);
  EXPECT_GT(frames[0].second(), future2);
}

// We check that the disabling code was triggered by sending multiple,
// different, frames to the converter within one second. While black, it shall
// treat all frames identical and issue one black frame per second.
TEST_F(VideoFrameConverterTest, BlackOnDisable) {
  TimeStamp now = TimeStamp::Now();
  TimeStamp future1 = now + TimeDuration::FromMilliseconds(100);
  TimeStamp future2 = now + TimeDuration::FromMilliseconds(200);
  TimeStamp future3 = now + TimeDuration::FromMilliseconds(400);
  mConverter->SetActive(true);
  mConverter->SetTrackEnabled(false);
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, future1), false);
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, future2), false);
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, future3), false);
  auto frames = WaitForNConverted(2);
  EXPECT_GT(TimeStamp::Now(), now + TimeDuration::FromMilliseconds(1100));
  ASSERT_EQ(frames.size(), 2U);
  EXPECT_EQ(frames[0].first().width(), 640);
  EXPECT_EQ(frames[0].first().height(), 480);
  EXPECT_GT(frames[0].second(), future1);
  EXPECT_EQ(frames[1].first().width(), 640);
  EXPECT_EQ(frames[1].first().height(), 480);
  EXPECT_GT(frames[1].second(), now + TimeDuration::FromMilliseconds(1100));
  // Check that the second frame comes between 1s and 2s after the first.
  EXPECT_NEAR(frames[1].first().timestamp_us(),
              frames[0].first().timestamp_us() + ((PR_USEC_PER_SEC * 3) / 2),
              PR_USEC_PER_SEC / 2);
}

TEST_F(VideoFrameConverterTest, ClearFutureFramesOnJumpingBack) {
  TimeStamp start = TimeStamp::Now();
  TimeStamp future1 = start + TimeDuration::FromMilliseconds(100);

  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, future1), false);
  WaitForNConverted(1);

  // We are now at t=100ms+. Queue a future frame and jump back in time to
  // signal a reset.

  TimeStamp step1 = TimeStamp::Now();
  ASSERT_GT(step1 - start, future1 - start);
  TimeStamp future2 = step1 + TimeDuration::FromMilliseconds(200);
  TimeStamp future3 = step1 + TimeDuration::FromMilliseconds(100);
  ASSERT_LT(future2 - start, future1 + TimeDuration::FromSeconds(1) - start);
  mConverter->QueueVideoChunk(GenerateChunk(800, 600, future2), false);
  VideoChunk nullChunk;
  nullChunk.mFrame = VideoFrame(nullptr, gfx::IntSize(800, 600));
  nullChunk.mTimeStamp = step1;
  mConverter->QueueVideoChunk(nullChunk, false);

  // We queue one more chunk after the reset so we don't have to wait a full
  // second for the same-frame timer. It has a different time and resolution
  // so we can differentiate them.
  mConverter->QueueVideoChunk(GenerateChunk(320, 240, future3), false);

  auto frames = WaitForNConverted(2);
  TimeStamp step2 = TimeStamp::Now();
  EXPECT_GT(step2 - start, future3 - start);
  ASSERT_EQ(frames.size(), 2U);
  EXPECT_EQ(frames[0].first().width(), 640);
  EXPECT_EQ(frames[0].first().height(), 480);
  EXPECT_GT(frames[0].second(), future1);
  EXPECT_EQ(frames[1].first().width(), 320);
  EXPECT_EQ(frames[1].first().height(), 240);
  EXPECT_GT(frames[1].second(), future3);
}

// We check that the no frame is converted while inactive, and that on
// activating the most recently queued frame gets converted.
TEST_F(VideoFrameConverterTest, NoConversionsWhileInactive) {
  TimeStamp now = TimeStamp::Now();
  TimeStamp future1 = now - TimeDuration::FromMilliseconds(1);
  TimeStamp future2 = now;
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, future1), false);
  mConverter->QueueVideoChunk(GenerateChunk(800, 600, future2), false);

  // SetActive needs to follow the same async path as the frames to be in sync.
  auto q =
      MakeRefPtr<TaskQueue>(GetMediaThreadPool(MediaThreadType::WEBRTC_DECODER),
                            "VideoFrameConverterTest");
  auto timer = MakeRefPtr<MediaTimer>(false);
  timer->WaitFor(TimeDuration::FromMilliseconds(100), __func__)
      ->Then(q, __func__,
             [converter = mConverter] { converter->SetActive(true); });

  auto frames = WaitForNConverted(1);
  ASSERT_EQ(frames.size(), 1U);
  EXPECT_EQ(frames[0].first().width(), 800);
  EXPECT_EQ(frames[0].first().height(), 600);
}
