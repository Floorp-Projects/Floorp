/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "DriftCompensation.h"
#include "MediaTrackGraph.h"
#include "MediaTrackListener.h"
#include "VP8TrackEncoder.h"
#include "WebMWriter.h"  // TODO: it's weird to include muxer header to get the class definition of VP8 METADATA
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"
#include "prtime.h"

#include "YUVBufferGenerator.h"

#define VIDEO_TRACK_RATE 90000

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::TestWithParam;
using ::testing::Values;

using namespace mozilla::layers;
using namespace mozilla;

struct InitParam {
  bool mShouldSucceed;  // This parameter should cause success or fail result
  int mWidth;           // frame width
  int mHeight;          // frame height
};

class MockDriftCompensator : public DriftCompensator {
 public:
  MockDriftCompensator()
      : DriftCompensator(GetCurrentSerialEventTarget(), VIDEO_TRACK_RATE) {
    ON_CALL(*this, GetVideoTime(_, _))
        .WillByDefault(Invoke([](TimeStamp, TimeStamp t) { return t; }));
  }

  MOCK_METHOD2(GetVideoTime, TimeStamp(TimeStamp, TimeStamp));
};

class TestVP8TrackEncoder : public VP8TrackEncoder {
 public:
  explicit TestVP8TrackEncoder(Maybe<float> aKeyFrameIntervalFactor = Nothing())
      : VP8TrackEncoder(MakeRefPtr<NiceMock<MockDriftCompensator>>(),
                        VIDEO_TRACK_RATE, mEncodedVideoQueue,
                        FrameDroppingMode::DISALLOW, aKeyFrameIntervalFactor) {}

  MockDriftCompensator* DriftCompensator() {
    return static_cast<MockDriftCompensator*>(mDriftCompensator.get());
  }

  ::testing::AssertionResult TestInit(const InitParam& aParam) {
    nsresult result =
        Init(aParam.mWidth, aParam.mHeight, aParam.mWidth, aParam.mHeight, 30);

    if (((NS_FAILED(result) && aParam.mShouldSucceed)) ||
        (NS_SUCCEEDED(result) && !aParam.mShouldSucceed)) {
      return ::testing::AssertionFailure()
             << " width = " << aParam.mWidth << " height = " << aParam.mHeight;
    }

    return ::testing::AssertionSuccess();
  }

  MediaQueue<EncodedFrame> mEncodedVideoQueue;
};

// Init test
TEST(VP8VideoTrackEncoder, Initialization)
{
  InitParam params[] = {
      // Failure cases.
      {false, 0, 0},  // Height/ width should be larger than 1.
      {false, 0, 1},  // Height/ width should be larger than 1.
      {false, 1, 0},  // Height/ width should be larger than 1.

      // Success cases
      {true, 640, 480},  // Standard VGA
      {true, 800, 480},  // Standard WVGA
      {true, 960, 540},  // Standard qHD
      {true, 1280, 720}  // Standard HD
  };

  for (const InitParam& param : params) {
    TestVP8TrackEncoder encoder;
    EXPECT_TRUE(encoder.TestInit(param));
  }
}

// Get MetaData test
TEST(VP8VideoTrackEncoder, FetchMetaData)
{
  InitParam params[] = {
      // Success cases
      {true, 640, 480},  // Standard VGA
      {true, 800, 480},  // Standard WVGA
      {true, 960, 540},  // Standard qHD
      {true, 1280, 720}  // Standard HD
  };

  for (const InitParam& param : params) {
    TestVP8TrackEncoder encoder;
    EXPECT_TRUE(encoder.TestInit(param));

    RefPtr<TrackMetadataBase> meta = encoder.GetMetadata();
    RefPtr<VP8Metadata> vp8Meta(static_cast<VP8Metadata*>(meta.get()));

    // METADATA should be depend on how to initiate encoder.
    EXPECT_EQ(vp8Meta->mWidth, param.mWidth);
    EXPECT_EQ(vp8Meta->mHeight, param.mHeight);
  }
}

// Encode test
TEST(VP8VideoTrackEncoder, FrameEncode)
{
  TestVP8TrackEncoder encoder;
  TimeStamp now = TimeStamp::Now();

  // Create YUV images as source.
  nsTArray<RefPtr<Image>> images;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  images.AppendElement(generator.GenerateI420Image());
  images.AppendElement(generator.GenerateNV12Image());
  images.AppendElement(generator.GenerateNV21Image());

  // Put generated YUV frame into video segment.
  // Duration of each frame is 1 second.
  VideoSegment segment;
  for (nsTArray<RefPtr<Image>>::size_type i = 0; i < images.Length(); i++) {
    RefPtr<Image> image = images[i];
    segment.AppendFrame(image.forget(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromSeconds(i));
  }

  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(images.Length()));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());
}

// Test that encoding a single frame gives useful output.
TEST(VP8VideoTrackEncoder, SingleFrameEncode)
{
  TestVP8TrackEncoder encoder;
  TimeStamp now = TimeStamp::Now();
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));

  // Pass a half-second frame to the encoder.
  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);

  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(0.5));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // Read out encoded data, and verify.
  RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frame->mFrameType)
      << "We only have one frame, so it should be a keyframe";

  const uint64_t halfSecond = PR_USEC_PER_SEC / 2;
  EXPECT_EQ(halfSecond, frame->mDuration);

  EXPECT_TRUE(encoder.mEncodedVideoQueue.AtEndOfStream());
}

// Test that encoding a couple of identical images gives useful output.
TEST(VP8VideoTrackEncoder, SameFrameEncode)
{
  TestVP8TrackEncoder encoder;
  TimeStamp now = TimeStamp::Now();
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));

  // Pass 15 100ms frames to the encoder.
  RefPtr<Image> image = generator.GenerateI420Image();
  VideoSegment segment;
  for (uint32_t i = 0; i < 15; ++i) {
    segment.AppendFrame(do_AddRef(image), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromSeconds(i * 0.1));
  }

  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(1.5));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // Verify total duration being 1.5s.
  uint64_t totalDuration = 0;
  while (RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront()) {
    totalDuration += frame->mDuration;
  }
  const uint64_t oneAndAHalf = (PR_USEC_PER_SEC / 2) * 3;
  EXPECT_EQ(oneAndAHalf, totalDuration);
}

// Test encoding a track that has to skip frames.
TEST(VP8VideoTrackEncoder, SkippedFrames)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Pass 100 frames of the shortest possible duration where we don't get
  // rounding errors between input/output rate.
  VideoSegment segment;
  for (uint32_t i = 0; i < 100; ++i) {
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(i));
  }

  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromMilliseconds(100));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // Verify total duration being 100 * 1ms = 100ms.
  uint64_t totalDuration = 0;
  while (RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront()) {
    totalDuration += frame->mDuration;
  }
  const uint64_t hundredMillis = PR_USEC_PER_SEC / 10;
  EXPECT_EQ(hundredMillis, totalDuration);
}

// Test encoding a track with frames subject to rounding errors.
TEST(VP8VideoTrackEncoder, RoundingErrorFramesEncode)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Pass nine frames with timestamps not expressable in 90kHz sample rate,
  // then one frame to make the total duration close to one second.
  VideoSegment segment;
  uint32_t usPerFrame = 99999;  // 99.999ms
  for (uint32_t i = 0; i < 9; ++i) {
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMicroseconds(i * usPerFrame));
  }

  // This last frame has timestamp start + 0.9s and duration 0.1s.
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromSeconds(0.9));

  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(1));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // Verify total duration being 1s.
  uint64_t totalDuration = 0;
  while (RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront()) {
    totalDuration += frame->mDuration;
  }
  // Not exact, the stream is encoded in time base 90kHz.
  const uint64_t oneSecond = PR_USEC_PER_SEC - 1;
  EXPECT_EQ(oneSecond, totalDuration);
}

// Test that we're encoding timestamps rather than durations.
TEST(VP8VideoTrackEncoder, TimestampFrameEncode)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromSeconds(0.05));
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromSeconds(0.2));

  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(0.3));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // Verify total duration being 0.3s and individual frames being [0.05s, 0.15s,
  // 0.1s]
  uint64_t expectedDurations[] = {(PR_USEC_PER_SEC / 10) / 2,
                                  (PR_USEC_PER_SEC / 10) * 3 / 2,
                                  (PR_USEC_PER_SEC / 10)};
  uint64_t totalDuration = 0;
  size_t i = 0;
  while (RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront()) {
    EXPECT_EQ(expectedDurations[i], frame->mDuration);
    i++;
    totalDuration += frame->mDuration;
  }
  const uint64_t pointThree = (PR_USEC_PER_SEC / 10) * 3;
  EXPECT_EQ(pointThree, totalDuration);
}

// Test that we're compensating for drift when encoding.
TEST(VP8VideoTrackEncoder, DriftingFrameEncode)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Set up major drift -- audio that goes twice as fast as video.
  // This should make the given video durations double as they get encoded.
  EXPECT_CALL(*encoder.DriftCompensator(), GetVideoTime(_, _))
      .WillRepeatedly(Invoke(
          [&](TimeStamp, TimeStamp aTime) { return now + (aTime - now) * 2; }));

  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromSeconds(0.05));
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromSeconds(0.2));

  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(0.3));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // Verify total duration being 0.6s and individual frames being [0.1s, 0.3s,
  // 0.2s]
  uint64_t expectedDurations[] = {(PR_USEC_PER_SEC / 10),
                                  (PR_USEC_PER_SEC / 10) * 3,
                                  (PR_USEC_PER_SEC / 10) * 2};
  uint64_t totalDuration = 0;
  size_t i = 0;
  while (RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront()) {
    EXPECT_EQ(expectedDurations[i], frame->mDuration);
    i++;
    totalDuration += frame->mDuration;
  }
  const uint64_t pointSix = (PR_USEC_PER_SEC / 10) * 6;
  EXPECT_EQ(pointSix, totalDuration);
}

// Test that suspending an encoding works.
TEST(VP8VideoTrackEncoder, Suspended)
{
  TestVP8TrackEncoder encoder;
  TimeStamp now = TimeStamp::Now();
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));

  // Pass 3 frames with duration 0.1s. We suspend before and resume after the
  // second frame.

  {
    VideoSegment segment;
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false, now);

    encoder.SetStartOffset(now);
    encoder.AppendVideoSegment(std::move(segment));
    encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(0.1));
  }

  encoder.Suspend(now + TimeDuration::FromSeconds(0.1));

  {
    VideoSegment segment;
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromSeconds(0.1));
    encoder.AppendVideoSegment(std::move(segment));
    encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(0.2));
  }

  encoder.Resume(now + TimeDuration::FromSeconds(0.2));

  {
    VideoSegment segment;
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromSeconds(0.2));
    encoder.AppendVideoSegment(std::move(segment));
    encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(0.3));
  }

  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // Verify that we have two encoded frames and a total duration of 0.2s.
  uint64_t count = 0;
  uint64_t totalDuration = 0;
  while (RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront()) {
    ++count;
    totalDuration += frame->mDuration;
  }
  const uint64_t two = 2;
  EXPECT_EQ(two, count);
  const uint64_t pointTwo = (PR_USEC_PER_SEC / 10) * 2;
  EXPECT_EQ(pointTwo, totalDuration);
}

// Test that ending a track while the video track encoder is suspended works.
TEST(VP8VideoTrackEncoder, SuspendedUntilEnd)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Pass 2 frames with duration 0.1s. We suspend before the second frame.

  {
    VideoSegment segment;
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false, now);

    encoder.SetStartOffset(now);
    encoder.AppendVideoSegment(std::move(segment));
    encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(0.1));
  }

  encoder.Suspend(now + TimeDuration::FromSeconds(0.1));

  {
    VideoSegment segment;
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromSeconds(0.1));
    encoder.AppendVideoSegment(std::move(segment));
    encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(0.2));
  }

  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // Verify that we have one encoded frames and a total duration of 0.1s.
  uint64_t count = 0;
  uint64_t totalDuration = 0;
  while (RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront()) {
    ++count;
    totalDuration += frame->mDuration;
  }
  const uint64_t one = 1;
  EXPECT_EQ(one, count);
  const uint64_t pointOne = PR_USEC_PER_SEC / 10;
  EXPECT_EQ(pointOne, totalDuration);
}

// Test that ending a track that was always suspended works.
TEST(VP8VideoTrackEncoder, AlwaysSuspended)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Suspend and then pass a frame with duration 2s.

  encoder.Suspend(now);

  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);

  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(2));

  encoder.NotifyEndOfStream();

  // Verify that we have no encoded frames.
  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.AtEndOfStream());
}

// Test that encoding a track that is suspended in the beginning works.
TEST(VP8VideoTrackEncoder, SuspendedBeginning)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Suspend and pass a frame with duration 0.5s. Then resume and pass one more.
  encoder.Suspend(now);

  {
    VideoSegment segment;
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false, now);

    encoder.SetStartOffset(now);
    encoder.AppendVideoSegment(std::move(segment));
    encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(0.5));
  }

  encoder.Resume(now + TimeDuration::FromSeconds(0.5));

  {
    VideoSegment segment;
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromSeconds(0.5));
    encoder.AppendVideoSegment(std::move(segment));
    encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(1));
  }

  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // Verify that we have one encoded frames and a total duration of 0.1s.
  uint64_t count = 0;
  uint64_t totalDuration = 0;
  while (RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront()) {
    ++count;
    totalDuration += frame->mDuration;
  }
  const uint64_t one = 1;
  EXPECT_EQ(one, count);
  const uint64_t half = PR_USEC_PER_SEC / 2;
  EXPECT_EQ(half, totalDuration);
}

// Test that suspending and resuming in the middle of already pushed data
// works.
TEST(VP8VideoTrackEncoder, SuspendedOverlap)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  {
    // Pass a 1s frame and suspend after 0.5s.
    VideoSegment segment;
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false, now);

    encoder.SetStartOffset(now);
    encoder.AppendVideoSegment(std::move(segment));
  }

  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(0.5));
  encoder.Suspend(now + TimeDuration::FromSeconds(0.5));

  {
    // Pass another 1s frame and resume after 0.3 of this new frame.
    VideoSegment segment;
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromSeconds(1));
    encoder.AppendVideoSegment(std::move(segment));
  }

  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(1.3));
  encoder.Resume(now + TimeDuration::FromSeconds(1.3));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(2));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // Verify that we have two encoded frames and a total duration of 0.1s.
  RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront();
  const uint64_t pointFive = (PR_USEC_PER_SEC / 10) * 5;
  EXPECT_EQ(pointFive, frame->mDuration);
  frame = encoder.mEncodedVideoQueue.PopFront();
  const uint64_t pointSeven = (PR_USEC_PER_SEC / 10) * 7;
  EXPECT_EQ(pointSeven, frame->mDuration);
  EXPECT_TRUE(encoder.mEncodedVideoQueue.AtEndOfStream());
}

// Test that ending a track in the middle of already pushed data works.
TEST(VP8VideoTrackEncoder, PrematureEnding)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Pass a 1s frame and end the track after 0.5s.
  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);

  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(0.5));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  uint64_t totalDuration = 0;
  while (RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront()) {
    totalDuration += frame->mDuration;
  }
  const uint64_t half = PR_USEC_PER_SEC / 2;
  EXPECT_EQ(half, totalDuration);
}

// Test that a track that starts at t > 0 works as expected.
TEST(VP8VideoTrackEncoder, DelayedStart)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Pass a 2s frame, start (pass first CurrentTime) at 0.5s, end at 1s.
  // Should result in a 0.5s encoding.
  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);

  encoder.SetStartOffset(now + TimeDuration::FromSeconds(0.5));
  encoder.AppendVideoSegment(std::move(segment));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(1));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  uint64_t totalDuration = 0;
  while (RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront()) {
    totalDuration += frame->mDuration;
  }
  const uint64_t half = PR_USEC_PER_SEC / 2;
  EXPECT_EQ(half, totalDuration);
}

// Test that a track that starts at t > 0 works as expected, when
// SetStartOffset comes after AppendVideoSegment.
TEST(VP8VideoTrackEncoder, DelayedStartOtherEventOrder)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Pass a 2s frame, start (pass first CurrentTime) at 0.5s, end at 1s.
  // Should result in a 0.5s encoding.
  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);

  encoder.AppendVideoSegment(std::move(segment));
  encoder.SetStartOffset(now + TimeDuration::FromSeconds(0.5));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(1));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  uint64_t totalDuration = 0;
  while (RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront()) {
    totalDuration += frame->mDuration;
  }
  const uint64_t half = PR_USEC_PER_SEC / 2;
  EXPECT_EQ(half, totalDuration);
}

// Test that a track that starts at t >>> 0 works as expected.
TEST(VP8VideoTrackEncoder, VeryDelayedStart)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Pass a 1s frame, start (pass first CurrentTime) at 10s, end at 10.5s.
  // Should result in a 0.5s encoding.
  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);

  encoder.SetStartOffset(now + TimeDuration::FromSeconds(10));
  encoder.AppendVideoSegment(std::move(segment));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(10.5));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  uint64_t totalDuration = 0;
  while (RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront()) {
    totalDuration += frame->mDuration;
  }
  const uint64_t half = PR_USEC_PER_SEC / 2;
  EXPECT_EQ(half, totalDuration);
}

// Test that a video frame that hangs around for a long time gets encoded
// every second.
TEST(VP8VideoTrackEncoder, LongFramesReEncoded)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Pass a frame at t=0 and start encoding.
  // Advancing the current time by 6.5s should encode six 1s frames.
  // Advancing the current time by another 5.5s should encode another five 1s
  // frames.

  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);

  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));

  {
    encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(6.5));

    EXPECT_FALSE(encoder.IsEncodingComplete());
    EXPECT_FALSE(encoder.mEncodedVideoQueue.IsFinished());

    uint64_t count = 0;
    uint64_t totalDuration = 0;
    while (RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront()) {
      ++count;
      totalDuration += frame->mDuration;
    }
    const uint64_t sixSec = 6 * PR_USEC_PER_SEC;
    EXPECT_EQ(sixSec, totalDuration);
    EXPECT_EQ(6U, count);
  }

  {
    encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(11));
    encoder.NotifyEndOfStream();

    EXPECT_TRUE(encoder.IsEncodingComplete());
    EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());

    uint64_t count = 0;
    uint64_t totalDuration = 0;
    while (RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront()) {
      ++count;
      totalDuration += frame->mDuration;
    }
    const uint64_t fiveSec = 5 * PR_USEC_PER_SEC;
    EXPECT_EQ(fiveSec, totalDuration);
    EXPECT_EQ(5U, count);
  }
}

// Test that an encoding with no defined key frame interval encodes keyframes
// as expected. Default interval should be 10s.
TEST(VP8VideoTrackEncoder, DefaultKeyFrameInterval)
{
  // Set the factor high to only test the keyframe-forcing logic
  TestVP8TrackEncoder encoder(Some(2.0));
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Pass a frame at t=0, and the frame-duplication logic will encode frames
  // every second. Keyframes are expected at t=0, 10s and 20s.
  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);

  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(21.5));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // Duplication logic ensures no frame duration is longer than 1 second.

  // [0, 1000ms) - key-frame.
  RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 1000UL, frame->mDuration);
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frame->mFrameType);

  // [1000ms, 10000ms) - non-key-frames
  for (int i = 0; i < 9; ++i) {
    frame = encoder.mEncodedVideoQueue.PopFront();
    EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 1000UL, frame->mDuration)
        << "Start time: " << frame->mTime.ToMicroseconds() << "us";
    EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frame->mFrameType)
        << "Start time: " << frame->mTime.ToMicroseconds() << "us";
  }

  // [10000ms, 11000ms) - key-frame
  frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 1000UL, frame->mDuration);
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frame->mFrameType);

  // [11000ms, 20000ms) - non-key-frames
  for (int i = 0; i < 9; ++i) {
    frame = encoder.mEncodedVideoQueue.PopFront();
    EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 1000UL, frame->mDuration)
        << "Start time: " << frame->mTime.ToMicroseconds() << "us";
    EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frame->mFrameType)
        << "Start time: " << frame->mTime.ToMicroseconds() << "us";
  }

  // [20000ms, 21000ms) - key-frame
  frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 1000UL, frame->mDuration);
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frame->mFrameType);

  // [21000ms, 21500ms) - non-key-frame
  frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 500UL, frame->mDuration);
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frame->mFrameType);

  EXPECT_TRUE(encoder.mEncodedVideoQueue.AtEndOfStream());
}

// Test that an encoding which is disabled on a frame timestamp encodes
// frames as expected.
TEST(VP8VideoTrackEncoder, DisableOnFrameTime)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Pass a frame in at t=0.
  // Pass another frame in at t=100ms.
  // Disable the track at t=100ms.
  // Stop encoding at t=200ms.
  // Should yield 2 frames, 1 real; 1 black.

  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(100));

  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));

  // Advancing 100ms, for simplicity.
  encoder.AdvanceCurrentTime(now + TimeDuration::FromMilliseconds(100));

  encoder.Disable(now + TimeDuration::FromMilliseconds(100));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromMilliseconds(200));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // [0, 100ms)
  RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frame->mDuration);

  // [100ms, 200ms)
  frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frame->mDuration);

  EXPECT_TRUE(encoder.mEncodedVideoQueue.AtEndOfStream());
}

// Test that an encoding which is disabled between two frame timestamps
// encodes frames as expected.
TEST(VP8VideoTrackEncoder, DisableBetweenFrames)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Pass a frame in at t=0.
  // Disable the track at t=50ms.
  // Pass another frame in at t=100ms.
  // Stop encoding at t=200ms.
  // Should yield 3 frames, 1 real [0, 50); 2 black [50, 100) and [100, 200).

  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(100));

  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));

  encoder.Disable(now + TimeDuration::FromMilliseconds(50));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromMilliseconds(200));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // [0, 50ms)
  RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 50UL, frame->mDuration);

  // [50ms, 100ms)
  frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 50UL, frame->mDuration);

  // [100ms, 200ms)
  frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frame->mDuration);

  EXPECT_TRUE(encoder.mEncodedVideoQueue.AtEndOfStream());
}

// Test that an encoding which is disabled before the first frame becomes
// black immediately.
TEST(VP8VideoTrackEncoder, DisableBeforeFirstFrame)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Disable the track at t=0.
  // Pass a frame in at t=50ms.
  // Enable the track at t=100ms.
  // Stop encoding at t=200ms.
  // Should yield 2 frames, 1 black [0, 100); 1 real [100, 200).

  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(50));

  encoder.SetStartOffset(now);
  encoder.Disable(now);
  encoder.AppendVideoSegment(std::move(segment));

  encoder.Enable(now + TimeDuration::FromMilliseconds(100));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromMilliseconds(200));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // [0, 100ms)
  RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frame->mDuration);

  // [100ms, 200ms)
  frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frame->mDuration);

  EXPECT_TRUE(encoder.mEncodedVideoQueue.AtEndOfStream());
}

// Test that an encoding which is enabled on a frame timestamp encodes
// frames as expected.
TEST(VP8VideoTrackEncoder, EnableOnFrameTime)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Disable the track at t=0.
  // Pass a frame in at t=0.
  // Pass another frame in at t=100ms.
  // Enable the track at t=100ms.
  // Stop encoding at t=200ms.
  // Should yield 2 frames, 1 black; 1 real.

  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(100));

  encoder.SetStartOffset(now);
  encoder.Disable(now);
  encoder.AppendVideoSegment(std::move(segment));

  // Advancing 100ms, for simplicity.
  encoder.AdvanceCurrentTime(now + TimeDuration::FromMilliseconds(100));

  encoder.Enable(now + TimeDuration::FromMilliseconds(100));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromMilliseconds(200));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // [0, 100ms)
  RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frame->mDuration);

  // [100ms, 200ms)
  frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frame->mDuration);

  EXPECT_TRUE(encoder.mEncodedVideoQueue.AtEndOfStream());
}

// Test that an encoding which is enabled between two frame timestamps encodes
// frames as expected.
TEST(VP8VideoTrackEncoder, EnableBetweenFrames)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Disable the track at t=0.
  // Pass a frame in at t=0.
  // Enable the track at t=50ms.
  // Pass another frame in at t=100ms.
  // Stop encoding at t=200ms.
  // Should yield 3 frames, 1 black [0, 50); 2 real [50, 100) and [100, 200).

  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(100));

  encoder.SetStartOffset(now);
  encoder.Disable(now);
  encoder.AppendVideoSegment(std::move(segment));

  encoder.Enable(now + TimeDuration::FromMilliseconds(50));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromMilliseconds(200));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // [0, 50ms)
  RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 50UL, frame->mDuration);

  // [50ms, 100ms)
  frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 50UL, frame->mDuration);

  // [100ms, 200ms)
  frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frame->mDuration);

  EXPECT_TRUE(encoder.mEncodedVideoQueue.AtEndOfStream());
}

// Test that making time go backwards removes any future frames in the
// encoder.
TEST(VP8VideoTrackEncoder, BackwardsTimeResets)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  encoder.SetStartOffset(now);

  // Pass frames in at t=0, t=100ms, t=200ms, t=300ms.
  // Advance time to t=125ms.
  // Pass frames in at t=150ms, t=250ms, t=350ms.
  // Stop encoding at t=300ms.
  // Should yield 4 frames, at t=0, t=100ms, t=150ms, t=250ms.

  {
    VideoSegment segment;
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false, now);
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(100));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(200));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(300));

    encoder.AppendVideoSegment(std::move(segment));
  }

  encoder.AdvanceCurrentTime(now + TimeDuration::FromMilliseconds(125));

  {
    VideoSegment segment;
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(150));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(250));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(350));

    encoder.AppendVideoSegment(std::move(segment));
  }

  encoder.AdvanceCurrentTime(now + TimeDuration::FromMilliseconds(300));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // [0, 100ms)
  RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frame->mDuration);

  // [100ms, 150ms)
  frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 50UL, frame->mDuration);

  // [150ms, 250ms)
  frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frame->mDuration);

  // [250ms, 300ms)
  frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 50UL, frame->mDuration);

  EXPECT_TRUE(encoder.mEncodedVideoQueue.AtEndOfStream());
}

// Test that trying to encode a null image removes any future frames in the
// encoder.
TEST(VP8VideoTrackEncoder, NullImageResets)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  encoder.SetStartOffset(now);

  // Pass frames in at t=0, t=100ms, t=200ms, t=300ms.
  // Advance time to t=125ms.
  // Pass in a null image at t=125ms.
  // Pass frames in at t=250ms, t=350ms.
  // Stop encoding at t=300ms.
  // Should yield 3 frames, at t=0, t=100ms, t=250ms.

  {
    VideoSegment segment;
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false, now);
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(100));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(200));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(300));

    encoder.AppendVideoSegment(std::move(segment));
  }

  encoder.AdvanceCurrentTime(now + TimeDuration::FromMilliseconds(125));

  {
    VideoSegment segment;
    segment.AppendFrame(nullptr, generator.GetSize(), PRINCIPAL_HANDLE_NONE,
                        false, now + TimeDuration::FromMilliseconds(125));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(250));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(350));

    encoder.AppendVideoSegment(std::move(segment));
  }

  encoder.AdvanceCurrentTime(now + TimeDuration::FromMilliseconds(300));
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // [0, 100ms)
  RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frame->mDuration);

  // [100ms, 250ms)
  frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 150UL, frame->mDuration);

  // [250ms, 300ms)
  frame = encoder.mEncodedVideoQueue.PopFront();
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 50UL, frame->mDuration);

  EXPECT_TRUE(encoder.mEncodedVideoQueue.AtEndOfStream());
}

TEST(VP8VideoTrackEncoder, MaxKeyFrameDistanceLowFramerate)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(240, 180));
  TimeStamp now = TimeStamp::Now();

  encoder.SetStartOffset(now);

  // Pass 10s worth of frames at 2 fps and verify that the key frame interval
  // is ~7.5s.
  const TimeDuration duration = TimeDuration::FromSeconds(10);
  const uint32_t numFrames = 10 * 2;
  const TimeDuration frameDuration = duration / static_cast<int64_t>(numFrames);

  {
    VideoSegment segment;
    for (uint32_t i = 0; i < numFrames; ++i) {
      segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                          PRINCIPAL_HANDLE_NONE, false,
                          now + frameDuration * i);
    }
    encoder.AppendVideoSegment(std::move(segment));
  }

  encoder.AdvanceCurrentTime(now + duration);
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  for (uint32_t i = 0; i < numFrames; ++i) {
    const RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront();
    EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 500UL, frame->mDuration)
        << "Frame " << i << ", with start: " << frame->mTime.ToMicroseconds()
        << "us";
    // 7.5s key frame interval at 2 fps becomes the 15th frame.
    EXPECT_EQ(
        i % 15 == 0 ? EncodedFrame::VP8_I_FRAME : EncodedFrame::VP8_P_FRAME,
        frame->mFrameType)
        << "Frame " << i << ", with start: " << frame->mTime.ToMicroseconds()
        << "us";
  }

  EXPECT_TRUE(encoder.mEncodedVideoQueue.AtEndOfStream());
}

// This is "High" framerate, as in higher than the test for "Low" framerate.
// We don't make it too high because the test takes considerably longer to
// run.
TEST(VP8VideoTrackEncoder, MaxKeyFrameDistanceHighFramerate)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(240, 180));
  TimeStamp now = TimeStamp::Now();

  encoder.SetStartOffset(now);

  // Pass 10s worth of frames at 8 fps and verify that the key frame interval
  // is ~7.5s.
  const TimeDuration duration = TimeDuration::FromSeconds(10);
  const uint32_t numFrames = 10 * 8;
  const TimeDuration frameDuration = duration / static_cast<int64_t>(numFrames);

  {
    VideoSegment segment;
    for (uint32_t i = 0; i < numFrames; ++i) {
      segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                          PRINCIPAL_HANDLE_NONE, false,
                          now + frameDuration * i);
    }
    encoder.AppendVideoSegment(std::move(segment));
  }

  encoder.AdvanceCurrentTime(now + duration);
  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  for (uint32_t i = 0; i < numFrames; ++i) {
    const RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront();
    EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 125UL, frame->mDuration)
        << "Frame " << i << ", with start: " << frame->mTime.ToMicroseconds()
        << "us";
    // 7.5s key frame interval at 8 fps becomes the 60th frame.
    EXPECT_EQ(
        i % 60 == 0 ? EncodedFrame::VP8_I_FRAME : EncodedFrame::VP8_P_FRAME,
        frame->mFrameType)
        << "Frame " << i << ", with start: " << frame->mTime.ToMicroseconds()
        << "us";
  }

  EXPECT_TRUE(encoder.mEncodedVideoQueue.AtEndOfStream());
}

TEST(VP8VideoTrackEncoder, MaxKeyFrameDistanceAdaptiveFramerate)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(240, 180));
  TimeStamp now = TimeStamp::Now();

  encoder.SetStartOffset(now);

  // Pass 11s worth of frames at 2 fps and verify that there is a key frame
  // at 7.5s. Then pass 14s worth of frames at 10 fps and verify that there is
  // a key frame at 15s (due to re-init) and then one at 22.5s.

  const TimeDuration firstDuration = TimeDuration::FromSeconds(11);
  const uint32_t firstNumFrames = 11 * 2;
  const TimeDuration firstFrameDuration =
      firstDuration / static_cast<int64_t>(firstNumFrames);
  {
    VideoSegment segment;
    for (uint32_t i = 0; i < firstNumFrames; ++i) {
      segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                          PRINCIPAL_HANDLE_NONE, false,
                          now + firstFrameDuration * i);
    }
    encoder.AppendVideoSegment(std::move(segment));
  }
  encoder.AdvanceCurrentTime(now + firstDuration);

  const TimeDuration secondDuration = TimeDuration::FromSeconds(14);
  const uint32_t secondNumFrames = 14 * 10;
  const TimeDuration secondFrameDuration =
      secondDuration / static_cast<int64_t>(secondNumFrames);
  {
    VideoSegment segment;
    for (uint32_t i = 0; i < secondNumFrames; ++i) {
      segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                          PRINCIPAL_HANDLE_NONE, false,
                          now + firstDuration + secondFrameDuration * i);
    }
    encoder.AppendVideoSegment(std::move(segment));
  }
  encoder.AdvanceCurrentTime(now + firstDuration + secondDuration);

  encoder.NotifyEndOfStream();

  EXPECT_TRUE(encoder.IsEncodingComplete());
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
  EXPECT_FALSE(encoder.mEncodedVideoQueue.AtEndOfStream());

  // [0, 11s) - keyframe distance is now 7.5s@2fps = 15.
  for (uint32_t i = 0; i < 22; ++i) {
    const RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront();
    EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 500UL, frame->mDuration)
        << "Frame " << i << ", with start: " << frame->mTime.ToMicroseconds()
        << "us";
    // 7.5s key frame interval at 2 fps becomes the 15th frame.
    EXPECT_EQ(
        i % 15 == 0 ? EncodedFrame::VP8_I_FRAME : EncodedFrame::VP8_P_FRAME,
        frame->mFrameType)
        << "Frame " << i << ", with start: " << frame->mTime.ToMicroseconds()
        << "us";
  }

  // Input framerate is now 10fps.
  // Framerate re-evaluation every 5s, so the keyframe distance changed at
  // 15s.
  for (uint32_t i = 22; i < 162; ++i) {
    const RefPtr<EncodedFrame> frame = encoder.mEncodedVideoQueue.PopFront();
    EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frame->mDuration)
        << "Frame " << i << ", with start: " << frame->mTime.ToMicroseconds()
        << "us";
    if (i < 22 + 40) {
      // [11s, 15s) - 40 frames at 10fps but with the 2fps keyframe distance.
      EXPECT_EQ(
          i % 15 == 0 ? EncodedFrame::VP8_I_FRAME : EncodedFrame::VP8_P_FRAME,
          frame->mFrameType)
          << "Frame " << i << ", with start: " << frame->mTime.ToMicroseconds()
          << "us";
    } else {
      // [15s, 25s) - 100 frames at 10fps. Keyframe distance 75. Starts with
      // keyframe due to re-init.
      EXPECT_EQ((i - 22 - 40) % 75 == 0 ? EncodedFrame::VP8_I_FRAME
                                        : EncodedFrame::VP8_P_FRAME,
                frame->mFrameType)
          << "Frame " << i << ", with start: " << frame->mTime.ToMicroseconds()
          << "us";
    }
  }

  EXPECT_TRUE(encoder.mEncodedVideoQueue.AtEndOfStream());
}

// EOS test
TEST(VP8VideoTrackEncoder, EncodeComplete)
{
  TestVP8TrackEncoder encoder;

  // NotifyEndOfStream should wrap up the encoding immediately.
  encoder.NotifyEndOfStream();
  EXPECT_TRUE(encoder.mEncodedVideoQueue.IsFinished());
}
