/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "AnnexB.h"
#include "ImageContainer.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/media/MediaUtils.h"  // For media::Await
#include "nsMimeTypes.h"
#include "PEMFactory.h"
#include "TimeUnits.h"
#include "VideoUtils.h"
#include <algorithm>

#include <fstream>

#define RUN_IF_SUPPORTED(mimeType, test)                   \
  do {                                                     \
    RefPtr<PEMFactory> f(new PEMFactory());                \
    if (f->SupportsMimeType(nsLiteralCString(mimeType))) { \
      test();                                              \
    }                                                      \
  } while (0)

#define BLOCK_SIZE 64
#define WIDTH 640
#define HEIGHT 480
#define NUM_FRAMES 150UL
#define FRAME_RATE 30
#define FRAME_DURATION (1000000 / FRAME_RATE)
#define BIT_RATE (1000 * 1000)        // 1Mbps
#define KEYFRAME_INTERVAL FRAME_RATE  // 1 keyframe per second

using namespace mozilla;

static gfx::IntSize kImageSize(WIDTH, HEIGHT);

class MediaDataEncoderTest : public testing::Test {
 protected:
  void SetUp() override { mData.Init(kImageSize); }

  void TearDown() override { mData.Deinit(); }

 public:
  struct FrameSource final {
    layers::PlanarYCbCrData mYUV;
    UniquePtr<uint8_t[]> mBuffer;
    RefPtr<layers::BufferRecycleBin> mRecycleBin;
    int16_t mColorStep = 4;

    void Init(const gfx::IntSize& aSize) {
      mYUV.mPicSize = aSize;
      mYUV.mYStride = aSize.width;
      mYUV.mYSize = aSize;
      mYUV.mCbCrStride = aSize.width / 2;
      mYUV.mCbCrSize = gfx::IntSize(aSize.width / 2, aSize.height / 2);
      size_t bufferSize = mYUV.mYStride * mYUV.mYSize.height +
                          mYUV.mCbCrStride * mYUV.mCbCrSize.height +
                          mYUV.mCbCrStride * mYUV.mCbCrSize.height;
      mBuffer = MakeUnique<uint8_t[]>(bufferSize);
      std::fill_n(mBuffer.get(), bufferSize, 0x7F);
      mYUV.mYChannel = mBuffer.get();
      mYUV.mCbChannel = mYUV.mYChannel + mYUV.mYStride * mYUV.mYSize.height;
      mYUV.mCrChannel =
          mYUV.mCbChannel + mYUV.mCbCrStride * mYUV.mCbCrSize.height;
      mRecycleBin = new layers::BufferRecycleBin();
    }

    void Deinit() {
      mBuffer.reset();
      mRecycleBin = nullptr;
    }

    already_AddRefed<MediaData> GetFrame(const size_t aIndex) {
      Draw(aIndex);
      RefPtr<layers::PlanarYCbCrImage> img =
          new layers::RecyclingPlanarYCbCrImage(mRecycleBin);
      img->CopyData(mYUV);
      RefPtr<MediaData> frame = VideoData::CreateFromImage(
          kImageSize, 0,
          media::TimeUnit::FromMicroseconds(aIndex * FRAME_DURATION),
          media::TimeUnit::FromMicroseconds(FRAME_DURATION), img,
          (aIndex & 0xF) == 0,
          media::TimeUnit::FromMicroseconds(aIndex * FRAME_DURATION));
      return frame.forget();
    }

    void DrawChessboard(uint8_t* aAddr, const size_t aWidth,
                        const size_t aHeight, const size_t aOffset) {
      uint8_t pixels[2][BLOCK_SIZE];
      size_t x = aOffset % BLOCK_SIZE;
      if ((aOffset / BLOCK_SIZE) & 1) {
        x = BLOCK_SIZE - x;
      }
      for (size_t i = 0; i < x; i++) {
        pixels[0][i] = 0x00;
        pixels[1][i] = 0xFF;
      }
      for (size_t i = x; i < BLOCK_SIZE; i++) {
        pixels[0][i] = 0xFF;
        pixels[1][i] = 0x00;
      }

      uint8_t* p = aAddr;
      for (size_t row = 0; row < aHeight; row++) {
        for (size_t col = 0; col < aWidth; col += BLOCK_SIZE) {
          memcpy(p, pixels[((row / BLOCK_SIZE) + (col / BLOCK_SIZE)) % 2],
                 BLOCK_SIZE);
          p += BLOCK_SIZE;
        }
      }
    }

    void Draw(const size_t aIndex) {
      DrawChessboard(mYUV.mYChannel, mYUV.mYSize.width, mYUV.mYSize.height,
                     aIndex << 1);
      int16_t color = mYUV.mCbChannel[0] + mColorStep;
      if (color > 255 || color < 0) {
        mColorStep = -mColorStep;
        color = mYUV.mCbChannel[0] + mColorStep;
      }

      size_t size = (mYUV.mCrChannel - mYUV.mCbChannel);

      std::fill_n(mYUV.mCbChannel, size, static_cast<uint8_t>(color));
      std::fill_n(mYUV.mCrChannel, size, 0xFF - static_cast<uint8_t>(color));
    }
  };

 public:
  FrameSource mData;
};

template <typename T>
already_AddRefed<MediaDataEncoder> CreateVideoEncoder(
    const char* aMimeType, MediaDataEncoder::Usage aUsage,
    MediaDataEncoder::PixelFormat aPixelFormat, int32_t aWidth, int32_t aHeight,
    const Maybe<T>& aSpecific) {
  RefPtr<PEMFactory> f(new PEMFactory());

  if (!f->SupportsMimeType(nsCString(aMimeType))) {
    return nullptr;
  }

  VideoInfo videoInfo(aWidth, aHeight);
  videoInfo.mMimeType = nsCString(aMimeType);
  const RefPtr<TaskQueue> taskQueue(
      new TaskQueue(GetMediaThreadPool(MediaThreadType::PLATFORM_ENCODER)));

  RefPtr<MediaDataEncoder> e;
  if (aSpecific) {
    e = f->CreateEncoder(CreateEncoderParams(
        videoInfo /* track info */, aUsage, taskQueue, aPixelFormat,
        FRAME_RATE /* FPS */, KEYFRAME_INTERVAL /* keyframe interval */,
        BIT_RATE /* bitrate */, aSpecific.value()));
  } else {
    e = f->CreateEncoder(CreateEncoderParams(
        videoInfo /* track info */, aUsage, taskQueue, aPixelFormat,
        FRAME_RATE /* FPS */, KEYFRAME_INTERVAL /* keyframe interval */,
        BIT_RATE /* bitrate */));
  }

  return e.forget();
}

static already_AddRefed<MediaDataEncoder> CreateH264Encoder(
    MediaDataEncoder::Usage aUsage = MediaDataEncoder::Usage::Realtime,
    MediaDataEncoder::PixelFormat aPixelFormat =
        MediaDataEncoder::PixelFormat::YUV420P,
    int32_t aWidth = WIDTH, int32_t aHeight = HEIGHT,
    const Maybe<MediaDataEncoder::H264Specific>& aSpecific =
        Some(MediaDataEncoder::H264Specific(
            MediaDataEncoder::H264Specific::ProfileLevel::BaselineAutoLevel))) {
  return CreateVideoEncoder(VIDEO_MP4, aUsage, aPixelFormat, aWidth, aHeight,
                            aSpecific);
}

void WaitForShutdown(RefPtr<MediaDataEncoder> aEncoder) {
  MOZ_ASSERT(aEncoder);

  Maybe<bool> result;
  // media::Await() supports exclusive promises only, but ShutdownPromise is
  // not.
  aEncoder->Shutdown()->Then(
      AbstractThread::MainThread(), __func__,
      [&result](bool rv) {
        EXPECT_TRUE(rv);
        result = Some(true);
      },
      [&result]() {
        FAIL() << "Shutdown should never be rejected";
        result = Some(false);
      });
  SpinEventLoopUntil([&result]() { return result; });
}

TEST_F(MediaDataEncoderTest, H264Create) {
  RUN_IF_SUPPORTED(VIDEO_MP4, []() {
  RefPtr<MediaDataEncoder> e = CreateH264Encoder();
  EXPECT_TRUE(e);
  WaitForShutdown(e);
  });
}

static bool EnsureInit(RefPtr<MediaDataEncoder> aEncoder) {
  if (!aEncoder) {
    return false;
  }

  bool succeeded;
  media::Await(
      GetMediaThreadPool(MediaThreadType::SUPERVISOR), aEncoder->Init(),
      [&succeeded](TrackInfo::TrackType t) {
        EXPECT_EQ(TrackInfo::TrackType::kVideoTrack, t);
        succeeded = true;
      },
      [&succeeded](MediaResult r) { succeeded = false; });
  return succeeded;
}

TEST_F(MediaDataEncoderTest, H264Inits) {
  RUN_IF_SUPPORTED(VIDEO_MP4, []() {
    // w/o codec specific.
  RefPtr<MediaDataEncoder> e = CreateH264Encoder(
        MediaDataEncoder::Usage::Realtime,
        MediaDataEncoder::PixelFormat::YUV420P, WIDTH, HEIGHT, Nothing());
  EXPECT_TRUE(EnsureInit(e));
  WaitForShutdown(e);

    // w/ codec specific
    e = CreateH264Encoder();
  EXPECT_TRUE(EnsureInit(e));
  WaitForShutdown(e);
  });
}

static MediaDataEncoder::EncodedData Encode(
    const RefPtr<MediaDataEncoder> aEncoder, const size_t aNumFrames,
    MediaDataEncoderTest::FrameSource& aSource) {
  MediaDataEncoder::EncodedData output;
  bool succeeded;
  for (size_t i = 0; i < aNumFrames; i++) {
    RefPtr<MediaData> frame = aSource.GetFrame(i);
    media::Await(
        GetMediaThreadPool(MediaThreadType::SUPERVISOR),
        aEncoder->Encode(frame),
        [&output, &succeeded](MediaDataEncoder::EncodedData encoded) {
          output.AppendElements(std::move(encoded));
          succeeded = true;
        },
        [&succeeded](MediaResult r) { succeeded = false; });
    EXPECT_TRUE(succeeded);
    if (!succeeded) {
      return output;
    }
  }

  size_t pending = 0;
  do {
    media::Await(
        GetMediaThreadPool(MediaThreadType::SUPERVISOR), aEncoder->Drain(),
        [&pending, &output, &succeeded](MediaDataEncoder::EncodedData encoded) {
          pending = encoded.Length();
          output.AppendElements(std::move(encoded));
          succeeded = true;
        },
        [&succeeded](MediaResult r) { succeeded = false; });
    EXPECT_TRUE(succeeded);
    if (!succeeded) {
      return output;
    }
  } while (pending > 0);

  return output;
}

TEST_F(MediaDataEncoderTest, H264Encodes) {
  RUN_IF_SUPPORTED(VIDEO_MP4, [this]() {
    // Encode one frame and output in AnnexB format.
  RefPtr<MediaDataEncoder> e = CreateH264Encoder();
  EnsureInit(e);
  MediaDataEncoder::EncodedData output = Encode(e, 1UL, mData);
  EXPECT_EQ(output.Length(), 1UL);
  EXPECT_TRUE(AnnexB::IsAnnexB(output[0]));
  WaitForShutdown(e);

    // Encode multiple frames and output in AnnexB format.
    e = CreateH264Encoder();
  EnsureInit(e);
    output = Encode(e, NUM_FRAMES, mData);
  EXPECT_EQ(output.Length(), NUM_FRAMES);
  for (auto frame : output) {
    EXPECT_TRUE(AnnexB::IsAnnexB(frame));
  }
  WaitForShutdown(e);

    // Encode one frame and output in avcC format.
    e = CreateH264Encoder(MediaDataEncoder::Usage::Record);
  EnsureInit(e);
    output = Encode(e, NUM_FRAMES, mData);
  EXPECT_EQ(output.Length(), NUM_FRAMES);
  AnnexB::IsAVCC(output[0]);  // Only 1st frame has extra data.
  for (auto frame : output) {
    EXPECT_FALSE(AnnexB::IsAnnexB(frame));
  }
  WaitForShutdown(e);
  });
}

#ifndef DEBUG  // Zero width or height will assert/crash in debug builds.
TEST_F(MediaDataEncoderTest, InvalidSize) {
  RUN_IF_SUPPORTED(VIDEO_MP4, []() {
  RefPtr<MediaDataEncoder> e0x0 =
      CreateH264Encoder(MediaDataEncoder::Usage::Realtime,
                        MediaDataEncoder::PixelFormat::YUV420P, 0, 0);
  EXPECT_NE(e0x0, nullptr);
  EXPECT_FALSE(EnsureInit(e0x0));

  RefPtr<MediaDataEncoder> e0x1 =
      CreateH264Encoder(MediaDataEncoder::Usage::Realtime,
                        MediaDataEncoder::PixelFormat::YUV420P, 0, 1);
  EXPECT_NE(e0x1, nullptr);
  EXPECT_FALSE(EnsureInit(e0x1));

  RefPtr<MediaDataEncoder> e1x0 =
      CreateH264Encoder(MediaDataEncoder::Usage::Realtime,
                        MediaDataEncoder::PixelFormat::YUV420P, 1, 0);
  EXPECT_NE(e1x0, nullptr);
  EXPECT_FALSE(EnsureInit(e1x0));
  });
}
#endif

#ifdef MOZ_WIDGET_ANDROID
TEST_F(MediaDataEncoderTest, AndroidNotSupportedSize) {
  RUN_IF_SUPPORTED(VIDEO_MP4, []() {
  RefPtr<MediaDataEncoder> e =
      CreateH264Encoder(MediaDataEncoder::Usage::Realtime,
                        MediaDataEncoder::PixelFormat::YUV420P, 1, 1);
  EXPECT_NE(e, nullptr);
  EXPECT_FALSE(EnsureInit(e));
  });
}
#endif
