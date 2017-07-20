/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include <algorithm>

#include "prtime.h"
#include "mozilla/ArrayUtils.h"
#include "VP8TrackEncoder.h"
#include "ImageContainer.h"
#include "MediaStreamGraph.h"
#include "MediaStreamListener.h"
#include "WebMWriter.h" // TODO: it's weird to include muxer header to get the class definition of VP8 METADATA

using ::testing::TestWithParam;
using ::testing::Values;

using namespace mozilla::layers;
using namespace mozilla;

// A helper object to generate of different YUV planes.
class YUVBufferGenerator {
public:
  YUVBufferGenerator() {}

  void Init(const mozilla::gfx::IntSize &aSize)
  {
    mImageSize = aSize;

    int yPlaneLen = aSize.width * aSize.height;
    int cbcrPlaneLen = (yPlaneLen + 1) / 2;
    int frameLen = yPlaneLen + cbcrPlaneLen;

    // Generate source buffer.
    mSourceBuffer.SetLength(frameLen);

    // Fill Y plane.
    memset(mSourceBuffer.Elements(), 0x10, yPlaneLen);

    // Fill Cb/Cr planes.
    memset(mSourceBuffer.Elements() + yPlaneLen, 0x80, cbcrPlaneLen);
  }

  mozilla::gfx::IntSize GetSize() const
  {
    return mImageSize;
  }

  already_AddRefed<Image> GenerateI420Image()
  {
    return do_AddRef(CreateI420Image());
  }

  already_AddRefed<Image> GenerateNV12Image()
  {
    return do_AddRef(CreateNV12Image());
  }

  already_AddRefed<Image> GenerateNV21Image()
  {
    return do_AddRef(CreateNV21Image());
  }

private:
  Image *CreateI420Image()
  {
    PlanarYCbCrImage *image = new RecyclingPlanarYCbCrImage(new BufferRecycleBin());
    PlanarYCbCrData data;
    data.mPicSize = mImageSize;

    const uint32_t yPlaneSize = mImageSize.width * mImageSize.height;
    const uint32_t halfWidth = (mImageSize.width + 1) / 2;
    const uint32_t halfHeight = (mImageSize.height + 1) / 2;
    const uint32_t uvPlaneSize = halfWidth * halfHeight;

    // Y plane.
    uint8_t *y = mSourceBuffer.Elements();
    data.mYChannel = y;
    data.mYSize.width = mImageSize.width;
    data.mYSize.height = mImageSize.height;
    data.mYStride = mImageSize.width;
    data.mYSkip = 0;

    // Cr plane.
    uint8_t *cr = y + yPlaneSize + uvPlaneSize;
    data.mCrChannel = cr;
    data.mCrSkip = 0;

    // Cb plane
    uint8_t *cb = y + yPlaneSize;
    data.mCbChannel = cb;
    data.mCbSkip = 0;

    // CrCb plane vectors.
    data.mCbCrStride = halfWidth;
    data.mCbCrSize.width = halfWidth;
    data.mCbCrSize.height = halfHeight;

    image->CopyData(data);
    return image;
  }

  Image *CreateNV12Image()
  {
    PlanarYCbCrImage *image = new RecyclingPlanarYCbCrImage(new BufferRecycleBin());
    PlanarYCbCrData data;
    data.mPicSize = mImageSize;

    const uint32_t yPlaneSize = mImageSize.width * mImageSize.height;
    const uint32_t halfWidth = (mImageSize.width + 1) / 2;
    const uint32_t halfHeight = (mImageSize.height + 1) / 2;

    // Y plane.
    uint8_t *y = mSourceBuffer.Elements();
    data.mYChannel = y;
    data.mYSize.width = mImageSize.width;
    data.mYSize.height = mImageSize.height;
    data.mYStride = mImageSize.width;
    data.mYSkip = 0;

    // Cr plane.
    uint8_t *cr = y + yPlaneSize;
    data.mCrChannel = cr;
    data.mCrSkip = 1;

    // Cb plane
    uint8_t *cb = y + yPlaneSize + 1;
    data.mCbChannel = cb;
    data.mCbSkip = 1;

    // 4:2:0.
    data.mCbCrStride = mImageSize.width;
    data.mCbCrSize.width = halfWidth;
    data.mCbCrSize.height = halfHeight;

    image->CopyData(data);
    return image;
  }

  Image *CreateNV21Image()
  {
    PlanarYCbCrImage *image = new RecyclingPlanarYCbCrImage(new BufferRecycleBin());
    PlanarYCbCrData data;
    data.mPicSize = mImageSize;

    const uint32_t yPlaneSize = mImageSize.width * mImageSize.height;
    const uint32_t halfWidth = (mImageSize.width + 1) / 2;
    const uint32_t halfHeight = (mImageSize.height + 1) / 2;

    // Y plane.
    uint8_t *y = mSourceBuffer.Elements();
    data.mYChannel = y;
    data.mYSize.width = mImageSize.width;
    data.mYSize.height = mImageSize.height;
    data.mYStride = mImageSize.width;
    data.mYSkip = 0;

    // Cr plane.
    uint8_t *cr = y + yPlaneSize + 1;
    data.mCrChannel = cr;
    data.mCrSkip = 1;

    // Cb plane
    uint8_t *cb = y + yPlaneSize;
    data.mCbChannel = cb;
    data.mCbSkip = 1;

    // 4:2:0.
    data.mCbCrStride = mImageSize.width;
    data.mCbCrSize.width = halfWidth;
    data.mCbCrSize.height = halfHeight;

    image->CopyData(data);
    return image;
  }

private:
  mozilla::gfx::IntSize mImageSize;
  nsTArray<uint8_t> mSourceBuffer;
};

struct InitParam {
  bool mShouldSucceed;  // This parameter should cause success or fail result
  int  mWidth;          // frame width
  int  mHeight;         // frame height
};

class TestVP8TrackEncoder: public VP8TrackEncoder
{
public:
  explicit TestVP8TrackEncoder(TrackRate aTrackRate = 90000)
    : VP8TrackEncoder(aTrackRate) {}

  ::testing::AssertionResult TestInit(const InitParam &aParam)
  {
    nsresult result = Init(aParam.mWidth, aParam.mHeight, aParam.mWidth, aParam.mHeight);

    if (((NS_FAILED(result) && aParam.mShouldSucceed)) || (NS_SUCCEEDED(result) && !aParam.mShouldSucceed))
    {
      return ::testing::AssertionFailure()
                << " width = " << aParam.mWidth
                << " height = " << aParam.mHeight;
    }
    else
    {
      return ::testing::AssertionSuccess();
    }
  }
};

// Init test
TEST(VP8VideoTrackEncoder, Initialization)
{
  InitParam params[] = {
    // Failure cases.
    { false, 0, 0},      // Height/ width should be larger than 1.
    { false, 0, 1},      // Height/ width should be larger than 1.
    { false, 1, 0},       // Height/ width should be larger than 1.

    // Success cases
    { true, 640, 480},    // Standard VGA
    { true, 800, 480},    // Standard WVGA
    { true, 960, 540},    // Standard qHD
    { true, 1280, 720}    // Standard HD
  };

  for (size_t i = 0; i < ArrayLength(params); i++)
  {
    TestVP8TrackEncoder encoder;
    EXPECT_TRUE(encoder.TestInit(params[i]));
  }
}

// Get MetaData test
TEST(VP8VideoTrackEncoder, FetchMetaData)
{
  InitParam params[] = {
    // Success cases
    { true, 640, 480},    // Standard VGA
    { true, 800, 480},    // Standard WVGA
    { true, 960, 540},    // Standard qHD
    { true, 1280, 720}    // Standard HD
  };

  for (size_t i = 0; i < ArrayLength(params); i++)
  {
    TestVP8TrackEncoder encoder;
    EXPECT_TRUE(encoder.TestInit(params[i]));

    RefPtr<TrackMetadataBase> meta = encoder.GetMetadata();
    RefPtr<VP8Metadata> vp8Meta(static_cast<VP8Metadata*>(meta.get()));

    // METADATA should be depend on how to initiate encoder.
    EXPECT_TRUE(vp8Meta->mWidth == params[i].mWidth);
    EXPECT_TRUE(vp8Meta->mHeight == params[i].mHeight);
  }
}

// Encode test
TEST(VP8VideoTrackEncoder, FrameEncode)
{
  // Initiate VP8 encoder
  TestVP8TrackEncoder encoder;
  InitParam param = {true, 640, 480};
  encoder.TestInit(param);

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
  TimeStamp now = TimeStamp::Now();
  for (nsTArray<RefPtr<Image>>::size_type i = 0; i < images.Length(); i++)
  {
    RefPtr<Image> image = images[i];
    segment.AppendFrame(image.forget(),
                        mozilla::StreamTime(90000),
                        generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE,
                        false,
                        now + TimeDuration::FromSeconds(i));
  }

  // track change notification.
  encoder.SetCurrentFrames(segment);

  // Pull Encoded Data back from encoder.
  EncodedFrameContainer container;
  EXPECT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));
}

// Test that encoding a single frame gives useful output.
TEST(VP8VideoTrackEncoder, SingleFrameEncode)
{
  // Initiate VP8 encoder
  TestVP8TrackEncoder encoder;
  InitParam param = {true, 640, 480};
  encoder.TestInit(param);

  // Pass a half-second frame to the encoder.
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  VideoSegment segment;
  segment.AppendFrame(generator.GenerateI420Image(),
                      mozilla::StreamTime(45000), // 1/2 second
                      generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE);

  encoder.SetCurrentFrames(segment);

  // End the track.
  segment.Clear();
  encoder.NotifyQueuedTrackChanges(nullptr, 0, 0, TrackEventCommand::TRACK_EVENT_ENDED, segment);

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  // Read out encoded data, and verify.
  const nsTArray<RefPtr<EncodedFrame>>& frames = container.GetEncodedFrames();
  const size_t oneElement = 1;
  ASSERT_EQ(oneElement, frames.Length());

  EXPECT_EQ(EncodedFrame::VP8_I_FRAME, frames[0]->GetFrameType()) <<
    "We only have one frame, so it should be a keyframe";

  const uint64_t halfSecond = PR_USEC_PER_SEC / 2;
  EXPECT_EQ(halfSecond, frames[0]->GetDuration());
}

// Test that encoding a couple of identical images gives useful output.
TEST(VP8VideoTrackEncoder, SameFrameEncode)
{
  // Initiate VP8 encoder
  TestVP8TrackEncoder encoder;
  InitParam param = {true, 640, 480};
  encoder.TestInit(param);

  // Pass 15 100ms frames to the encoder.
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  RefPtr<Image> image = generator.GenerateI420Image();
  TimeStamp now = TimeStamp::Now();
  VideoSegment segment;
  for (uint32_t i = 0; i < 15; ++i) {
    segment.AppendFrame(do_AddRef(image),
                        mozilla::StreamTime(9000), // 100ms
                        generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE,
                        false,
                        now + TimeDuration::FromSeconds(i * 0.1));
  }

  encoder.SetCurrentFrames(segment);

  // End the track.
  segment.Clear();
  encoder.NotifyQueuedTrackChanges(nullptr, 0, 0, TrackEventCommand::TRACK_EVENT_ENDED, segment);

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

// Test encoding a track that starts with null data
TEST(VP8VideoTrackEncoder, NullFrameFirst)
{
  // Initiate VP8 encoder
  TestVP8TrackEncoder encoder;
  InitParam param = {true, 640, 480};
  encoder.TestInit(param);
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  RefPtr<Image> image = generator.GenerateI420Image();
  TimeStamp now = TimeStamp::Now();
  VideoSegment segment;

  // Pass 2 100ms null frames to the encoder.
  for (uint32_t i = 0; i < 2; ++i) {
    segment.AppendFrame(nullptr,
                        mozilla::StreamTime(9000), // 100ms
                        generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE,
                        false,
                        now + TimeDuration::FromSeconds(i * 0.1));
  }

  // Pass a real 100ms frame to the encoder.
  segment.AppendFrame(image.forget(),
                      mozilla::StreamTime(9000), // 100ms
                      generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE,
                      false,
                      now + TimeDuration::FromSeconds(0.3));

  encoder.SetCurrentFrames(segment);

  // End the track.
  segment.Clear();
  encoder.NotifyQueuedTrackChanges(nullptr, 0, 0, TrackEventCommand::TRACK_EVENT_ENDED, segment);

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  // Verify total duration being 0.3s.
  uint64_t totalDuration = 0;
  for (auto& frame : container.GetEncodedFrames()) {
    totalDuration += frame->GetDuration();
  }
  const uint64_t pointThree = (PR_USEC_PER_SEC / 10) * 3;
  EXPECT_EQ(pointThree, totalDuration);
}

// Test encoding a track that has to skip frames.
TEST(VP8VideoTrackEncoder, SkippedFrames)
{
  // Initiate VP8 encoder
  TestVP8TrackEncoder encoder;
  InitParam param = {true, 640, 480};
  encoder.TestInit(param);
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();
  VideoSegment segment;

  // Pass 100 frames of the shortest possible duration where we don't get
  // rounding errors between input/output rate.
  for (uint32_t i = 0; i < 100; ++i) {
    segment.AppendFrame(generator.GenerateI420Image(),
                        mozilla::StreamTime(90), // 1ms
                        generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE,
                        false,
                        now + TimeDuration::FromMilliseconds(i));
  }

  encoder.SetCurrentFrames(segment);

  // End the track.
  segment.Clear();
  encoder.NotifyQueuedTrackChanges(nullptr, 0, 0, TrackEventCommand::TRACK_EVENT_ENDED, segment);

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
  // Initiate VP8 encoder
  TestVP8TrackEncoder encoder;
  InitParam param = {true, 640, 480};
  encoder.TestInit(param);
  YUVBufferGenerator generator;
  generator.Init(mozilla::gfx::IntSize(640, 480));
  TimeStamp now = TimeStamp::Now();
  VideoSegment segment;

  // Pass nine frames with timestamps not expressable in 90kHz sample rate,
  // then one frame to make the total duration one second.
  uint32_t usPerFrame = 99999; //99.999ms
  for (uint32_t i = 0; i < 9; ++i) {
    segment.AppendFrame(generator.GenerateI420Image(),
                        mozilla::StreamTime(9000), // 100ms
                        generator.GetSize(),
                        PRINCIPAL_HANDLE_NONE,
                        false,
                        now + TimeDuration::FromMicroseconds(i * usPerFrame));
  }

  // This last frame has timestamp start + 0.9s and duration 0.1s.
  segment.AppendFrame(generator.GenerateI420Image(),
                      mozilla::StreamTime(9000), // 100ms
                      generator.GetSize(),
                      PRINCIPAL_HANDLE_NONE,
                      false,
                      now + TimeDuration::FromSeconds(0.9));

  encoder.SetCurrentFrames(segment);

  // End the track.
  segment.Clear();
  encoder.NotifyQueuedTrackChanges(nullptr, 0, 0, TrackEventCommand::TRACK_EVENT_ENDED, segment);

  EncodedFrameContainer container;
  ASSERT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());

  // Verify total duration being 1s.
  uint64_t totalDuration = 0;
  for (auto& frame : container.GetEncodedFrames()) {
    totalDuration += frame->GetDuration();
  }
  const uint64_t oneSecond= PR_USEC_PER_SEC;
  EXPECT_EQ(oneSecond, totalDuration);
}

// EOS test
TEST(VP8VideoTrackEncoder, EncodeComplete)
{
  // Initiate VP8 encoder
  TestVP8TrackEncoder encoder;
  InitParam param = {true, 640, 480};
  encoder.TestInit(param);

  // track end notification.
  VideoSegment segment;
  encoder.NotifyQueuedTrackChanges(nullptr, 0, 0, TrackEventCommand::TRACK_EVENT_ENDED, segment);

  // Pull Encoded Data back from encoder. Since we have sent
  // EOS to encoder, encoder.GetEncodedTrack should return
  // NS_OK immidiately.
  EncodedFrameContainer container;
  EXPECT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));

  EXPECT_TRUE(encoder.IsEncodingComplete());
}
