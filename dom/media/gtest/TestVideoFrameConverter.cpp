/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <iterator>

#include "gtest/gtest.h"
#include "libwebrtcglue/SystemTime.h"
#include "mozilla/gtest/WaitFor.h"
#include "MediaEventSource.h"
#include "VideoFrameConverter.h"
#include "YUVBufferGenerator.h"

using namespace mozilla;

class VideoFrameConverterTest;

class FrameListener {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FrameListener)

  explicit FrameListener(MediaEventSourceExc<webrtc::VideoFrame>& aSource) {
    mListener = aSource.Connect(AbstractThread::GetCurrent(), this,
                                &FrameListener::OnVideoFrameConverted);
  }

  void OnVideoFrameConverted(webrtc::VideoFrame aVideoFrame) {
    mVideoFrameConvertedEvent.Notify(std::move(aVideoFrame), TimeStamp::Now());
  }

  MediaEventSource<webrtc::VideoFrame, TimeStamp>& VideoFrameConvertedEvent() {
    return mVideoFrameConvertedEvent;
  }

 private:
  ~FrameListener() { mListener.Disconnect(); }

  MediaEventListener mListener;
  MediaEventProducer<webrtc::VideoFrame, TimeStamp> mVideoFrameConvertedEvent;
};

class DebugVideoFrameConverter : public VideoFrameConverter {
 public:
  explicit DebugVideoFrameConverter(
      const dom::RTCStatsTimestampMaker& aTimestampMaker)
      : VideoFrameConverter(aTimestampMaker) {}

  using VideoFrameConverter::QueueForProcessing;
  using VideoFrameConverter::RegisterListener;
};

class VideoFrameConverterTest : public ::testing::Test {
 protected:
  const dom::RTCStatsTimestampMaker mTimestampMaker;
  RefPtr<DebugVideoFrameConverter> mConverter;
  RefPtr<FrameListener> mListener;

  VideoFrameConverterTest()
      : mTimestampMaker(dom::RTCStatsTimestampMaker::Create()),
        mConverter(MakeAndAddRef<DebugVideoFrameConverter>(mTimestampMaker)),
        mListener(MakeAndAddRef<FrameListener>(
            mConverter->VideoFrameConvertedEvent())) {
    mConverter->RegisterListener();
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
  EXPECT_GT(TimeStamp::Now() - now, TimeDuration::FromMilliseconds(1100));
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
  EXPECT_GT(conversionTime1 - now, TimeDuration::FromMilliseconds(1100));
  EXPECT_EQ(frame1.timestamp_us() - frame0.timestamp_us(), USECS_PER_S);

  // Check that we re-used the old buffer.
  EXPECT_EQ(frame0.video_frame_buffer(), frame1.video_frame_buffer());
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
// This version disables before queuing a frame. A frame will have to be
// invented.
TEST_F(VideoFrameConverterTest, BlackOnDisableCreated) {
  auto framesPromise = TakeNConvertedFrames(2);
  TimeStamp now = TimeStamp::Now();
  TimeStamp future1 = now + TimeDuration::FromMilliseconds(100);
  TimeStamp future2 = now + TimeDuration::FromMilliseconds(200);
  TimeStamp future3 = now + TimeDuration::FromMilliseconds(400);
  mConverter->SetActive(true);
  mConverter->SetTrackEnabled(false);
  mConverter->QueueVideoChunk(GenerateChunk(800, 600, future1), false);
  mConverter->QueueVideoChunk(GenerateChunk(800, 600, future2), false);
  mConverter->QueueVideoChunk(GenerateChunk(800, 600, future3), false);
  auto frames = WaitFor(framesPromise).unwrap();
  EXPECT_GT(TimeStamp::Now() - now, TimeDuration::FromSeconds(1));
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
  EXPECT_GT(conversionTime1 - now, TimeDuration::FromSeconds(1));
  // Check that the second frame comes 1s after the first.
  EXPECT_EQ(frame1.timestamp_us(), frame0.timestamp_us() + PR_USEC_PER_SEC);
}

// We check that the disabling code was triggered by sending multiple,
// different, frames to the converter within one second. While black, it shall
// treat all frames identical and issue one black frame per second.
// This version queues a frame before disabling.
TEST_F(VideoFrameConverterTest, BlackOnDisableDuplicated) {
  TimeStamp now = TimeStamp::Now();
  TimeStamp future1 = now + TimeDuration::FromMilliseconds(100);
  TimeStamp future2 = now + TimeDuration::FromMilliseconds(200);
  TimeStamp future3 = now + TimeDuration::FromMilliseconds(400);
  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(GenerateChunk(800, 600, future1), false);
  mConverter->QueueVideoChunk(GenerateChunk(800, 600, future2), false);
  mConverter->QueueVideoChunk(GenerateChunk(800, 600, future3), false);

  const auto [frame0, conversionTime0] =
      WaitFor(TakeNConvertedFrames(1)).unwrap()[0];
  mConverter->SetTrackEnabled(false);
  // The first frame was queued.
  EXPECT_EQ(frame0.width(), 800);
  EXPECT_EQ(frame0.height(), 600);
  EXPECT_FALSE(IsFrameBlack(frame0));
  EXPECT_GT(conversionTime0 - now, future1 - now);

  auto frames = WaitFor(TakeNConvertedFrames(2)).unwrap();
  ASSERT_EQ(frames.size(), 2U);
  // The second frame was duplicated by SetTrackEnabled.
  const auto& [frame1, conversionTime1] = frames[0];
  EXPECT_EQ(frame1.width(), 800);
  EXPECT_EQ(frame1.height(), 600);
  EXPECT_TRUE(IsFrameBlack(frame1));
  EXPECT_GT(conversionTime1 - now, future1 - now);
  // The third frame was created by the same-frame timer (after 1s).
  const auto& [frame2, conversionTime2] = frames[1];
  EXPECT_EQ(frame2.width(), 800);
  EXPECT_EQ(frame2.height(), 600);
  EXPECT_TRUE(IsFrameBlack(frame2));
  EXPECT_GT(conversionTime2 - now,
            future1 - now + TimeDuration::FromSeconds(1));
  // Check that the third frame comes 1s after the second.
  EXPECT_EQ(frame2.timestamp_us(), frame1.timestamp_us() + PR_USEC_PER_SEC);
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
  auto q = TaskQueue::Create(GetMediaThreadPool(MediaThreadType::WEBRTC_WORKER),
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

TEST_F(VideoFrameConverterTest, TimestampPropagation) {
  auto framesPromise = TakeNConvertedFrames(2);
  TimeStamp now = TimeStamp::Now();
  TimeDuration d1 = TimeDuration::FromMilliseconds(1);
  TimeDuration d2 = TimeDuration::FromMilliseconds(29);

  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, now + d1), false);
  mConverter->QueueVideoChunk(GenerateChunk(800, 600, now + d2), false);

  auto frames = WaitFor(framesPromise).unwrap();
  ASSERT_EQ(frames.size(), 2U);
  const auto& [frame0, conversionTime0] = frames[0];
  EXPECT_EQ(frame0.width(), 640);
  EXPECT_EQ(frame0.height(), 480);
  EXPECT_FALSE(IsFrameBlack(frame0));
  EXPECT_EQ(frame0.timestamp_us(),
            dom::RTCStatsTimestamp::FromMozTime(mTimestampMaker, now + d1)
                .ToRealtime()
                .us());
  EXPECT_GE(conversionTime0 - now, d1);

  const auto& [frame1, conversionTime1] = frames[1];
  EXPECT_EQ(frame1.width(), 800);
  EXPECT_EQ(frame1.height(), 600);
  EXPECT_FALSE(IsFrameBlack(frame1));
  EXPECT_EQ(frame1.timestamp_us(),
            dom::RTCStatsTimestamp::FromMozTime(mTimestampMaker, now + d2)
                .ToRealtime()
                .us());
  EXPECT_GE(conversionTime1 - now, d2);
}

TEST_F(VideoFrameConverterTest, IgnoreOldFrames) {
  TimeStamp now = TimeStamp::Now();
  TimeDuration d1 = TimeDuration::FromMilliseconds(100);
  TimeDuration d2 = d1 + TimeDuration::FromMicroseconds(1);

  auto framesPromise = TakeNConvertedFrames(1);
  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, now + d1), false);
  auto frames = WaitFor(framesPromise).unwrap();

  framesPromise = TakeNConvertedFrames(2);

  // Time is now ~t1. This processes an extra frame using t=now().
  mConverter->SetActive(false);
  mConverter->SetActive(true);

  // This processes a new chunk with an earlier timestamp than the extra frame
  // above. But it gets processed after the extra frame, so time will appear to
  // go backwards. This simulates a frame from the pacer being in flight when we
  // flip SetActive() above. This frame is expected to get ignored.
  Unused << WaitFor(InvokeAsync(mConverter->mTaskQueue, __func__, [&] {
    mConverter->QueueForProcessing(
        GenerateChunk(800, 600, now + d2).mFrame.GetImage(), now + d2,
        gfx::IntSize(800, 600), false);
    return GenericPromise::CreateAndResolve(true, __func__);
  }));

  {
    auto newFrames = WaitFor(framesPromise).unwrap();
    frames.insert(frames.end(), std::make_move_iterator(newFrames.begin()),
                  std::make_move_iterator(newFrames.end()));
  }
  ASSERT_EQ(frames.size(), 3U);
  const auto& [frame0, conversionTime0] = frames[0];
  EXPECT_EQ(frame0.width(), 640);
  EXPECT_EQ(frame0.height(), 480);
  EXPECT_FALSE(IsFrameBlack(frame0));
  EXPECT_EQ(frame0.timestamp_us(),
            dom::RTCStatsTimestamp::FromMozTime(mTimestampMaker, now + d1)
                .ToRealtime()
                .us());
  EXPECT_GE(conversionTime0 - now, d1);

  const auto& [frame1, conversionTime1] = frames[1];
  EXPECT_EQ(frame1.width(), 640);
  EXPECT_EQ(frame1.height(), 480);
  EXPECT_FALSE(IsFrameBlack(frame1));
  EXPECT_GT(frame1.timestamp_us(),
            dom::RTCStatsTimestamp::FromMozTime(mTimestampMaker, now + d2)
                .ToRealtime()
                .us());
  EXPECT_GE(conversionTime1 - now, d2);

  const auto& [frame2, conversionTime2] = frames[2];
  EXPECT_EQ(frame2.width(), 640);
  EXPECT_EQ(frame2.height(), 480);
  EXPECT_FALSE(IsFrameBlack(frame2));
  EXPECT_EQ(frame2.timestamp_us(), frame1.timestamp_us() + USECS_PER_S);
  EXPECT_GE(conversionTime2 - now, d2 + TimeDuration::FromSeconds(1));
}

TEST_F(VideoFrameConverterTest, SameFrameTimerRacingWithPacing) {
  TimeStamp now = TimeStamp::Now();
  TimeDuration d1 = TimeDuration::FromMilliseconds(100);
  TimeDuration d2 =
      d1 + TimeDuration::FromSeconds(1) - TimeDuration::FromMicroseconds(1);

  auto framesPromise = TakeNConvertedFrames(3);
  mConverter->SetActive(true);
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, now + d1), false);
  mConverter->QueueVideoChunk(GenerateChunk(640, 480, now + d2), false);
  auto frames = WaitFor(framesPromise).unwrap();

  // The expected order here (in timestamps) is t1, t2, t2+1s.
  //
  // If the same-frame timer doesn't check what is queued we could end up with
  // t1, t1+1s, t2.

  ASSERT_EQ(frames.size(), 3U);
  const auto& [frame0, conversionTime0] = frames[0];
  EXPECT_EQ(frame0.width(), 640);
  EXPECT_EQ(frame0.height(), 480);
  EXPECT_FALSE(IsFrameBlack(frame0));
  EXPECT_EQ(frame0.timestamp_us(),
            dom::RTCStatsTimestamp::FromMozTime(mTimestampMaker, now + d1)
                .ToRealtime()
                .us());
  EXPECT_GE(conversionTime0 - now, d1);

  const auto& [frame1, conversionTime1] = frames[1];
  EXPECT_EQ(frame1.width(), 640);
  EXPECT_EQ(frame1.height(), 480);
  EXPECT_FALSE(IsFrameBlack(frame1));
  EXPECT_EQ(frame1.timestamp_us(),
            dom::RTCStatsTimestamp::FromMozTime(mTimestampMaker, now + d2)
                .ToRealtime()
                .us());
  EXPECT_GE(conversionTime1 - now, d2);

  const auto& [frame2, conversionTime2] = frames[2];
  EXPECT_EQ(frame2.width(), 640);
  EXPECT_EQ(frame2.height(), 480);
  EXPECT_FALSE(IsFrameBlack(frame2));
  EXPECT_EQ(frame2.timestamp_us(),
            dom::RTCStatsTimestamp::FromMozTime(
                mTimestampMaker, now + d2 + TimeDuration::FromSeconds(1))
                .ToRealtime()
                .us());
  EXPECT_GE(conversionTime2 - now, d2 + TimeDuration::FromSeconds(1));
}
