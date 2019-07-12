/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>

#include "DriftCompensation.h"
#include "MediaStreamGraph.h"
#include "MediaStreamListener.h"
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
      : DriftCompensator(GetCurrentThreadEventTarget(), VIDEO_TRACK_RATE) {
    ON_CALL(*this, GetVideoTime(_, _))
        .WillByDefault(Invoke([](TimeStamp, TimeStamp t) { return t; }));
  }

  MOCK_METHOD2(GetVideoTime, TimeStamp(TimeStamp, TimeStamp));
};

class TestVP8TrackEncoder : public VP8TrackEncoder {
 public:
  explicit TestVP8TrackEncoder(TrackRate aTrackRate = VIDEO_TRACK_RATE)
      : VP8TrackEncoder(MakeRefPtr<NiceMock<MockDriftCompensator>>(),
                        aTrackRate, FrameDroppingMode::DISALLOW) {}

  MockDriftCompensator* DriftCompensator() {
    return static_cast<MockDriftCompensator*>(mDriftCompensator.get());
  }

  ::testing::AssertionResult TestInit(const InitParam& aParam) {
    nsresult result =
        Init(aParam.mWidth, aParam.mHeight, aParam.mWidth, aParam.mHeight);

    if (((NS_FAILED(result) && aParam.mShouldSucceed)) ||
        (NS_SUCCEEDED(result) && !aParam.mShouldSucceed)) {
      return ::testing::AssertionFailure()
             << " width = " << aParam.mWidth << " height = " << aParam.mHeight;
    }

    return ::testing::AssertionSuccess();
  }
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

  // Pull Encoded Data back from encoder.
  EncodedFrameContainer container;
  EXPECT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));
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

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  // Read out encoded data, and verify.
  const nsTArray<RefPtr<EncodedFrame>>& frames = container.GetEncodedFrames();
  const size_t oneElement = 1;
  ASSERT_EQ(oneElement, frames.Length());

  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[0]->GetFrameType())
      << "We only have one frame, so it should be a keyframe";

  const uint64_t halfSecond = PR_USEC_PER_SEC / 2;
  EXPECT_EQ(halfSecond, frames[0]->GetDuration());
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

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  // Verify total duration being 1.5s.
  uint64_t totalDuration = 0;
  for (auto& frame : container.GetEncodedFrames()) {
    totalDuration += frame->GetDuration();
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

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  // Verify total duration being 100 * 1ms = 100ms.
  uint64_t totalDuration = 0;
  for (auto& frame : container.GetEncodedFrames()) {
    totalDuration += frame->GetDuration();
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
  // then one frame to make the total duration one second.
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

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  // Verify total duration being 1s.
  uint64_t totalDuration = 0;
  for (auto& frame : container.GetEncodedFrames()) {
    totalDuration += frame->GetDuration();
  }
  const uint64_t oneSecond = PR_USEC_PER_SEC;
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

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  // Verify total duration being 0.3s and individual frames being [0.05s, 0.15s,
  // 0.1s]
  uint64_t expectedDurations[] = {(PR_USEC_PER_SEC / 10) / 2,
                                  (PR_USEC_PER_SEC / 10) * 3 / 2,
                                  (PR_USEC_PER_SEC / 10)};
  uint64_t totalDuration = 0;
  size_t i = 0;
  for (auto& frame : container.GetEncodedFrames()) {
    EXPECT_EQ(expectedDurations[i++], frame->GetDuration());
    totalDuration += frame->GetDuration();
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

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  // Verify total duration being 0.6s and individual frames being [0.1s, 0.3s,
  // 0.2s]
  uint64_t expectedDurations[] = {(PR_USEC_PER_SEC / 10),
                                  (PR_USEC_PER_SEC / 10) * 3,
                                  (PR_USEC_PER_SEC / 10) * 2};
  uint64_t totalDuration = 0;
  size_t i = 0;
  for (auto& frame : container.GetEncodedFrames()) {
    EXPECT_EQ(expectedDurations[i++], frame->GetDuration());
    totalDuration += frame->GetDuration();
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

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  // Verify that we have two encoded frames and a total duration of 0.2s.
  const uint64_t two = 2;
  EXPECT_EQ(two, container.GetEncodedFrames().Length());

  uint64_t totalDuration = 0;
  for (auto& frame : container.GetEncodedFrames()) {
    totalDuration += frame->GetDuration();
  }
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

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  // Verify that we have one encoded frames and a total duration of 0.1s.
  const uint64_t one = 1;
  EXPECT_EQ(one, container.GetEncodedFrames().Length());

  uint64_t totalDuration = 0;
  for (auto& frame : container.GetEncodedFrames()) {
    totalDuration += frame->GetDuration();
  }
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

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  // Verify that we have no encoded frames.
  const uint64_t none = 0;
  EXPECT_EQ(none, container.GetEncodedFrames().Length());
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

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  // Verify that we have one encoded frames and a total duration of 0.1s.
  const uint64_t one = 1;
  EXPECT_EQ(one, container.GetEncodedFrames().Length());

  uint64_t totalDuration = 0;
  for (auto& frame : container.GetEncodedFrames()) {
    totalDuration += frame->GetDuration();
  }
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

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  // Verify that we have two encoded frames and a total duration of 0.1s.
  const uint64_t two = 2;
  ASSERT_EQ(two, container.GetEncodedFrames().Length());
  const uint64_t pointFive = (PR_USEC_PER_SEC / 10) * 5;
  EXPECT_EQ(pointFive, container.GetEncodedFrames()[0]->GetDuration());
  const uint64_t pointSeven = (PR_USEC_PER_SEC / 10) * 7;
  EXPECT_EQ(pointSeven, container.GetEncodedFrames()[1]->GetDuration());
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

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  uint64_t totalDuration = 0;
  for (auto& frame : container.GetEncodedFrames()) {
    totalDuration += frame->GetDuration();
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

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  uint64_t totalDuration = 0;
  for (auto& frame : container.GetEncodedFrames()) {
    totalDuration += frame->GetDuration();
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

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  uint64_t totalDuration = 0;
  for (auto& frame : container.GetEncodedFrames()) {
    totalDuration += frame->GetDuration();
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

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  uint64_t totalDuration = 0;
  for (auto& frame : container.GetEncodedFrames()) {
    totalDuration += frame->GetDuration();
  }
  const uint64_t half = PR_USEC_PER_SEC / 2;
  EXPECT_EQ(half, totalDuration);
}

// Test that a video frame that hangs around for a long time gets encoded every
// second.
TEST(VP8VideoTrackEncoder, LongFramesReEncoded)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Pass a frame at t=0 and start encoding.
  // Advancing the current time by 1.5s should encode a 1s frame.
  // Advancing the current time by another 9.5s should encode another 10 1s
  // frames.

  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);

  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));

  {
    encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(1.5));

    EncodedFrameContainer container;
    ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));
    EXPECT_FALSE(encoder.IsEncodingComplete());

    uint64_t totalDuration = 0;
    for (auto& frame : container.GetEncodedFrames()) {
      totalDuration += frame->GetDuration();
    }
    const uint64_t oneSec = PR_USEC_PER_SEC;
    EXPECT_EQ(oneSec, totalDuration);
    EXPECT_EQ(1U, container.GetEncodedFrames().Length());
  }

  {
    encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(11));
    encoder.NotifyEndOfStream();

    EncodedFrameContainer container;
    ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));
    EXPECT_TRUE(encoder.IsEncodingComplete());

    uint64_t totalDuration = 0;
    for (auto& frame : container.GetEncodedFrames()) {
      totalDuration += frame->GetDuration();
    }
    const uint64_t tenSec = PR_USEC_PER_SEC * 10;
    EXPECT_EQ(tenSec, totalDuration);
    EXPECT_EQ(10U, container.GetEncodedFrames().Length());
  }
}

// Test that an encoding with a defined key frame interval encodes keyframes
// as expected. Short here means shorter than the default (1s).
TEST(VP8VideoTrackEncoder, ShortKeyFrameInterval)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Give the encoder a keyframe interval of 500ms.
  // Pass frames at 0, 400ms, 600ms, 750ms, 900ms, 1100ms
  // Expected keys: ^         ^^^^^                ^^^^^^
  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(400));
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(600));
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(750));
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(900));
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(1100));

  encoder.SetKeyFrameInterval(500);
  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(1.2));
  encoder.NotifyEndOfStream();

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  const nsTArray<RefPtr<EncodedFrame>>& frames = container.GetEncodedFrames();
  ASSERT_EQ(6UL, frames.Length());

  // [0, 400ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 400UL, frames[0]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[0]->GetFrameType());

  // [400ms, 600ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 200UL, frames[1]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frames[1]->GetFrameType());

  // [600ms, 750ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 150UL, frames[2]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[2]->GetFrameType());

  // [750ms, 900ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 150UL, frames[3]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frames[3]->GetFrameType());

  // [900ms, 1100ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 200UL, frames[4]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frames[4]->GetFrameType());

  // [1100ms, 1200ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[5]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[5]->GetFrameType());
}

// Test that an encoding with a defined key frame interval encodes keyframes
// as expected. Long here means longer than the default (1s).
TEST(VP8VideoTrackEncoder, LongKeyFrameInterval)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Give the encoder a keyframe interval of 2000ms.
  // Pass frames at 0, 600ms, 900ms, 1100ms, 1900ms, 2100ms
  // Expected keys: ^                ^^^^^^          ^^^^^^
  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(600));
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(900));
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(1100));
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(1900));
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(2100));

  encoder.SetKeyFrameInterval(2000);
  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(2.2));
  encoder.NotifyEndOfStream();

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  const nsTArray<RefPtr<EncodedFrame>>& frames = container.GetEncodedFrames();
  ASSERT_EQ(6UL, frames.Length());

  // [0, 600ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 600UL, frames[0]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[0]->GetFrameType());

  // [600ms, 900ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 300UL, frames[1]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frames[1]->GetFrameType());

  // [900ms, 1100ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 200UL, frames[2]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frames[2]->GetFrameType());

  // [1100ms, 1900ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 800UL, frames[3]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[3]->GetFrameType());

  // [1900ms, 2100ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 200UL, frames[4]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frames[4]->GetFrameType());

  // [2100ms, 2200ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[5]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[5]->GetFrameType());
}

// Test that an encoding with no defined key frame interval encodes keyframes
// as expected. Default interval should be 1000ms.
TEST(VP8VideoTrackEncoder, DefaultKeyFrameInterval)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();

  // Pass frames at 0, 600ms, 900ms, 1100ms, 1900ms, 2100ms
  // Expected keys: ^                ^^^^^^          ^^^^^^
  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false, now);
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(600));
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(900));
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(1100));
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(1900));
  segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE, false,
                      now + TimeDuration::FromMilliseconds(2100));

  encoder.SetStartOffset(now);
  encoder.AppendVideoSegment(std::move(segment));
  encoder.AdvanceCurrentTime(now + TimeDuration::FromSeconds(2.2));
  encoder.NotifyEndOfStream();

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  const nsTArray<RefPtr<EncodedFrame>>& frames = container.GetEncodedFrames();
  ASSERT_EQ(6UL, frames.Length());

  // [0, 600ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 600UL, frames[0]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[0]->GetFrameType());

  // [600ms, 900ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 300UL, frames[1]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frames[1]->GetFrameType());

  // [900ms, 1100ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 200UL, frames[2]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frames[2]->GetFrameType());

  // [1100ms, 1900ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 800UL, frames[3]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[3]->GetFrameType());

  // [1900ms, 2100ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 200UL, frames[4]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frames[4]->GetFrameType());

  // [2100ms, 2200ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[5]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[5]->GetFrameType());
}

// Test that an encoding where the key frame interval is updated dynamically
// encodes keyframes as expected.
TEST(VP8VideoTrackEncoder, DynamicKeyFrameIntervalChanges)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  EncodedFrameContainer container;
  TimeStamp now = TimeStamp::Now();

  // Set keyframe interval to 100ms.
  // Pass frames at 0, 100ms, 120ms, 130ms, 200ms, 300ms
  // Expected keys: ^  ^^^^^                ^^^^^  ^^^^^

  // Then increase keyframe interval to 1100ms. (default is 1000)
  // Pass frames at 500ms, 1300ms, 1400ms, 2400ms
  // Expected keys:        ^^^^^^          ^^^^^^

  // Then decrease keyframe interval to 200ms.
  // Pass frames at 2500ms, 2600ms, 2800ms, 2900ms
  // Expected keys:         ^^^^^^  ^^^^^^

  {
    VideoSegment segment;
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false, now);
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(100));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(120));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(130));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(200));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(300));

    // The underlying encoder only gets passed frame N when frame N+1 is known,
    // so we pass in the next frame *before* the keyframe interval change.
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(500));

    encoder.SetStartOffset(now);
    encoder.SetKeyFrameInterval(100);
    encoder.AppendVideoSegment(std::move(segment));
  }

  // Advancing 501ms, so the first bit of the frame starting at 500ms is
  // included.
  encoder.AdvanceCurrentTime(now + TimeDuration::FromMilliseconds(501));
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  {
    VideoSegment segment;
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(1300));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(1400));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(2400));

    // The underlying encoder only gets passed frame N when frame N+1 is known,
    // so we pass in the next frame *before* the keyframe interval change.
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(2500));

    encoder.SetKeyFrameInterval(1100);
    encoder.AppendVideoSegment(std::move(segment));
  }

  // Advancing 2000ms from 501ms to 2501ms
  encoder.AdvanceCurrentTime(now + TimeDuration::FromMilliseconds(2501));
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  {
    VideoSegment segment;
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(2600));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(2800));
    segment.AppendFrame(generator.GenerateI420Image(), generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE, false,
                        now + TimeDuration::FromMilliseconds(2900));

    encoder.SetKeyFrameInterval(200);
    encoder.AppendVideoSegment(std::move(segment));
  }

  // Advancing 499ms (compensating back 1ms from the first advancement)
  // from 2501ms to 3000ms.
  encoder.AdvanceCurrentTime(now + TimeDuration::FromMilliseconds(3000));

  encoder.NotifyEndOfStream();

  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  const nsTArray<RefPtr<EncodedFrame>>& frames = container.GetEncodedFrames();
  ASSERT_EQ(14UL, frames.Length());

  // [0, 100ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[0]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[0]->GetFrameType());

  // [100ms, 120ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 20UL, frames[1]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[1]->GetFrameType());

  // [120ms, 130ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 10UL, frames[2]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frames[2]->GetFrameType());

  // [130ms, 200ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 70UL, frames[3]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frames[3]->GetFrameType());

  // [200ms, 300ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[4]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[4]->GetFrameType());

  // [300ms, 500ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 200UL, frames[5]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[5]->GetFrameType());

  // [500ms, 1300ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 800UL, frames[6]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frames[6]->GetFrameType());

  // [1300ms, 1400ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[7]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[7]->GetFrameType());

  // [1400ms, 2400ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 1000UL, frames[8]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frames[8]->GetFrameType());

  // [2400ms, 2500ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[9]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[9]->GetFrameType());

  // [2500ms, 2600ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[10]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frames[10]->GetFrameType());

  // [2600ms, 2800ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 200UL, frames[11]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[11]->GetFrameType());

  // [2800ms, 2900ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[12]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[12]->GetFrameType());

  // [2900ms, 3000ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[13]->GetDuration());
  EXPECT_EQ(EncodedFrame::VP8_P_FRAME, frames[13]->GetFrameType());
}

// Test that an encoding which is disabled on a frame timestamp encodes
// frames as expected.
TEST(VP8VideoTrackEncoder, DisableOnFrameTime)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  EncodedFrameContainer container;
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
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));
  EXPECT_TRUE(encoder.IsEncodingComplete());

  const nsTArray<RefPtr<EncodedFrame>>& frames = container.GetEncodedFrames();
  ASSERT_EQ(2UL, frames.Length());

  // [0, 100ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[0]->GetDuration());

  // [100ms, 200ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[1]->GetDuration());
}

// Test that an encoding which is disabled between two frame timestamps encodes
// frames as expected.
TEST(VP8VideoTrackEncoder, DisableBetweenFrames)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  EncodedFrameContainer container;
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
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));
  EXPECT_TRUE(encoder.IsEncodingComplete());

  const nsTArray<RefPtr<EncodedFrame>>& frames = container.GetEncodedFrames();
  ASSERT_EQ(3UL, frames.Length());

  // [0, 50ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 50UL, frames[0]->GetDuration());

  // [50ms, 100ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 50UL, frames[1]->GetDuration());

  // [100ms, 200ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[2]->GetDuration());
}

// Test that an encoding which is enabled on a frame timestamp encodes
// frames as expected.
TEST(VP8VideoTrackEncoder, EnableOnFrameTime)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  EncodedFrameContainer container;
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
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));
  EXPECT_TRUE(encoder.IsEncodingComplete());

  const nsTArray<RefPtr<EncodedFrame>>& frames = container.GetEncodedFrames();
  ASSERT_EQ(2UL, frames.Length());

  // [0, 100ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[0]->GetDuration());

  // [100ms, 200ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[1]->GetDuration());
}

// Test that an encoding which is enabled between two frame timestamps encodes
// frames as expected.
TEST(VP8VideoTrackEncoder, EnableBetweenFrames)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  EncodedFrameContainer container;
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
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));
  EXPECT_TRUE(encoder.IsEncodingComplete());

  const nsTArray<RefPtr<EncodedFrame>>& frames = container.GetEncodedFrames();
  ASSERT_EQ(3UL, frames.Length());

  // [0, 50ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 50UL, frames[0]->GetDuration());

  // [50ms, 100ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 50UL, frames[1]->GetDuration());

  // [100ms, 200ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[2]->GetDuration());
}

// Test that making time go backwards removes any future frames in the encoder.
TEST(VP8VideoTrackEncoder, BackwardsTimeResets)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  EncodedFrameContainer container;
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
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));
  EXPECT_TRUE(encoder.IsEncodingComplete());

  const nsTArray<RefPtr<EncodedFrame>>& frames = container.GetEncodedFrames();
  ASSERT_EQ(4UL, frames.Length());

  // [0, 100ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[0]->GetDuration());

  // [100ms, 150ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 50UL, frames[1]->GetDuration());

  // [150ms, 250ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[2]->GetDuration());

  // [250ms, 300ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 50UL, frames[3]->GetDuration());
}

// Test that trying to encode a null image removes any future frames in the
// encoder.
TEST(VP8VideoTrackEncoder, NullImageResets)
{
  TestVP8TrackEncoder encoder;
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  EncodedFrameContainer container;
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
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));
  EXPECT_TRUE(encoder.IsEncodingComplete());

  const nsTArray<RefPtr<EncodedFrame>>& frames = container.GetEncodedFrames();
  ASSERT_EQ(3UL, frames.Length());

  // [0, 100ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 100UL, frames[0]->GetDuration());

  // [100ms, 250ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 150UL, frames[1]->GetDuration());

  // [250ms, 300ms)
  EXPECT_EQ(PR_USEC_PER_SEC / 1000 * 50UL, frames[2]->GetDuration());
}

// EOS test
TEST(VP8VideoTrackEncoder, EncodeComplete)
{
  TestVP8TrackEncoder encoder;

  // track end notification.
  encoder.NotifyEndOfStream();

  // Pull Encoded Data back from encoder. Since we have sent
  // EOS to encoder, encoder.GetEncodedTrack should return
  // NS_OK immidiately.
  EncodedFrameContainer container;
  EXPECT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());
}
