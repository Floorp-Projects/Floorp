/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "nsMimeTypes.h"
#include "VideoUtils.h"
#include "PEMFactory.h"
#include "ImageContainer.h"
#include "AnnexB.h"

#include "mozilla/AbstractThread.h"
#include "mozilla/media/MediaUtils.h"  // For media::Await

#include <algorithm>

#include <fstream>

#define SKIP_IF_NOT_SUPPORTED(mimeType)                       \
  do {                                                        \
    RefPtr<PEMFactory> f(new PEMFactory());                   \
    if (!f->SupportsMimeType(NS_LITERAL_CSTRING(mimeType))) { \
      return;                                                 \
    }                                                         \
  } while (0)

using namespace mozilla;

static gfx::IntSize kImageSize(640, 480);

class MediaDataEncoderTest : public testing::Test {
 protected:
  void SetUp() override { InitData(kImageSize); }

  void TearDown() override { DeinitData(); }

  layers::PlanarYCbCrData mData;
  UniquePtr<uint8_t[]> mBackBuffer;

 private:
  void InitData(const gfx::IntSize& aSize) {
    mData.mPicSize = aSize;
    mData.mYStride = aSize.width;
    mData.mYSize = aSize;
    mData.mCbCrStride = aSize.width / 2;
    mData.mCbCrSize = gfx::IntSize(aSize.width / 2, aSize.height / 2);
    size_t bufferSize = mData.mYStride * mData.mYSize.height +
                        mData.mCbCrStride * mData.mCbCrSize.height +
                        mData.mCbCrStride * mData.mCbCrSize.height;
    mBackBuffer = MakeUnique<uint8_t[]>(bufferSize);
    std::fill_n(mBackBuffer.get(), bufferSize, 42);
    mData.mYChannel = mBackBuffer.get();
    mData.mCbChannel = mData.mYChannel + mData.mYStride * mData.mYSize.height;
    mData.mCrChannel =
        mData.mCbChannel + mData.mCbCrStride * mData.mCbCrSize.height;
  }

  void DeinitData() { mBackBuffer.reset(); }
};

static already_AddRefed<MediaDataEncoder> CreateH264Encoder(
    MediaDataEncoder::Usage aUsage,
    MediaDataEncoder::PixelFormat aPixelFormat) {
  RefPtr<PEMFactory> f(new PEMFactory());

  if (!f->SupportsMimeType(NS_LITERAL_CSTRING(VIDEO_MP4))) {
    return nullptr;
  }

  VideoInfo videoInfo(1280, 720);
  videoInfo.mMimeType = NS_LITERAL_CSTRING(VIDEO_MP4);
  const RefPtr<TaskQueue> taskQueue(
      new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK)));
  CreateEncoderParams c(videoInfo /* track info */, aUsage, taskQueue,
                        aPixelFormat, 30 /* FPS */,
                        10 * 1024 * 1024 /* bitrate */);
  return f->CreateEncoder(c);
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
  SKIP_IF_NOT_SUPPORTED(VIDEO_MP4);

  RefPtr<MediaDataEncoder> e =
      CreateH264Encoder(MediaDataEncoder::Usage::Realtime,
                        MediaDataEncoder::PixelFormat::YUV420P);

  EXPECT_TRUE(e);

  WaitForShutdown(e);
}

static bool EnsureInit(RefPtr<MediaDataEncoder> aEncoder) {
  if (!aEncoder) {
    return false;
  }

  bool succeeded;
  media::Await(
      GetMediaThreadPool(MediaThreadType::PLAYBACK), aEncoder->Init(),
      [&succeeded](TrackInfo::TrackType t) {
        EXPECT_EQ(TrackInfo::TrackType::kVideoTrack, t);
        succeeded = true;
      },
      [&succeeded](MediaResult r) { succeeded = false; });
  return succeeded;
}

TEST_F(MediaDataEncoderTest, H264Init) {
  SKIP_IF_NOT_SUPPORTED(VIDEO_MP4);

  RefPtr<MediaDataEncoder> e =
      CreateH264Encoder(MediaDataEncoder::Usage::Realtime,
                        MediaDataEncoder::PixelFormat::YUV420P);

  EXPECT_TRUE(EnsureInit(e));

  WaitForShutdown(e);
}

static MediaDataEncoder::EncodedData Encode(
    const RefPtr<MediaDataEncoder> aEncoder, const size_t aNumFrames,
    const layers::PlanarYCbCrData& aYCbCrData) {
  MediaDataEncoder::EncodedData output;
  bool succeeded;
  for (size_t i = 0; i < aNumFrames; i++) {
    RefPtr<layers::PlanarYCbCrImage> img =
        new layers::RecyclingPlanarYCbCrImage(new layers::BufferRecycleBin());
    img->AdoptData(aYCbCrData);
    RefPtr<MediaData> frame = VideoData::CreateFromImage(
        kImageSize, 0, TimeUnit::FromMicroseconds(i * 30000),
        TimeUnit::FromMicroseconds(30000), img, (i & 0xF) == 0,
        TimeUnit::FromMicroseconds(i * 30000));
    media::Await(
        GetMediaThreadPool(MediaThreadType::PLAYBACK), aEncoder->Encode(frame),
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
  media::Await(
      GetMediaThreadPool(MediaThreadType::PLAYBACK), aEncoder->Drain(),
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

  if (pending > 0) {
    media::Await(
        GetMediaThreadPool(MediaThreadType::PLAYBACK), aEncoder->Drain(),
        [&succeeded](MediaDataEncoder::EncodedData encoded) {
          EXPECT_EQ(encoded.Length(), 0UL);
          succeeded = true;
        },
        [&succeeded](MediaResult r) { succeeded = false; });
    EXPECT_TRUE(succeeded);
  }

  return output;
}

TEST_F(MediaDataEncoderTest, H264EncodeOneFrameAsAnnexB) {
  SKIP_IF_NOT_SUPPORTED(VIDEO_MP4);

  RefPtr<MediaDataEncoder> e =
      CreateH264Encoder(MediaDataEncoder::Usage::Realtime,
                        MediaDataEncoder::PixelFormat::YUV420P);
  EnsureInit(e);

  MediaDataEncoder::EncodedData output = Encode(e, 1UL, mData);
  EXPECT_EQ(output.Length(), 1UL);
  EXPECT_TRUE(AnnexB::IsAnnexB(output[0]));

  WaitForShutdown(e);
}

TEST_F(MediaDataEncoderTest, EncodeMultipleFramesAsAnnexB) {
  SKIP_IF_NOT_SUPPORTED(VIDEO_MP4);

  RefPtr<MediaDataEncoder> e =
      CreateH264Encoder(MediaDataEncoder::Usage::Realtime,
                        MediaDataEncoder::PixelFormat::YUV420P);
  EnsureInit(e);

  MediaDataEncoder::EncodedData output = Encode(e, 30UL, mData);
  EXPECT_EQ(output.Length(), 30UL);
  for (auto frame : output) {
    EXPECT_TRUE(AnnexB::IsAnnexB(frame));
  }

  WaitForShutdown(e);
}

TEST_F(MediaDataEncoderTest, EncodeMultipleFramesAsAVCC) {
  SKIP_IF_NOT_SUPPORTED(VIDEO_MP4);

  RefPtr<MediaDataEncoder> e = CreateH264Encoder(
      MediaDataEncoder::Usage::Record, MediaDataEncoder::PixelFormat::YUV420P);
  EnsureInit(e);

  MediaDataEncoder::EncodedData output = Encode(e, 30UL, mData);
  EXPECT_EQ(output.Length(), 30UL);
  AnnexB::IsAVCC(output[0]);  // Only 1st frame has extra data.
  for (auto frame : output) {
    EXPECT_FALSE(AnnexB::IsAnnexB(frame));
  }

  WaitForShutdown(e);
}