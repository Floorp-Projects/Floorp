/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CheckedInt.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/RefPtr.h"
#include "nsRect.h"
#include "nsSize.h"
#include "DummyMediaDataDecoder.h"
#include "ImageContainer.h"
#include "MediaData.h"
#include "MediaInfo.h"
#include "PlatformDecoderModule.h"
#include "VideoUtils.h"

namespace mozilla {

class BlankVideoDataCreator : public DummyDataCreator {
public:
  BlankVideoDataCreator(uint32_t aFrameWidth,
                        uint32_t aFrameHeight,
                        layers::ImageContainer* aImageContainer)
    : mFrameWidth(aFrameWidth)
    , mFrameHeight(aFrameHeight)
    , mImageContainer(aImageContainer)
  {
    mInfo.mDisplay = nsIntSize(mFrameWidth, mFrameHeight);
    mPicture = gfx::IntRect(0, 0, mFrameWidth, mFrameHeight);
  }

  already_AddRefed<MediaData> Create(MediaRawData* aSample) override
  {
    // Create a fake YUV buffer in a 420 format. That is, an 8bpp Y plane,
    // with a U and V plane that are half the size of the Y plane, i.e 8 bit,
    // 2x2 subsampled. Have the data pointer of each frame point to the
    // first plane, they'll always be zero'd memory anyway.
    auto frame = MakeUnique<uint8_t[]>(mFrameWidth * mFrameHeight);
    memset(frame.get(), 0, mFrameWidth * mFrameHeight);
    VideoData::YCbCrBuffer buffer;

    // Y plane.
    buffer.mPlanes[0].mData = frame.get();
    buffer.mPlanes[0].mStride = mFrameWidth;
    buffer.mPlanes[0].mHeight = mFrameHeight;
    buffer.mPlanes[0].mWidth = mFrameWidth;
    buffer.mPlanes[0].mOffset = 0;
    buffer.mPlanes[0].mSkip = 0;

    // Cb plane.
    buffer.mPlanes[1].mData = frame.get();
    buffer.mPlanes[1].mStride = (mFrameWidth + 1) / 2;
    buffer.mPlanes[1].mHeight = (mFrameHeight + 1) / 2;
    buffer.mPlanes[1].mWidth = (mFrameWidth + 1) / 2;
    buffer.mPlanes[1].mOffset = 0;
    buffer.mPlanes[1].mSkip = 0;

    // Cr plane.
    buffer.mPlanes[2].mData = frame.get();
    buffer.mPlanes[2].mStride = (mFrameWidth + 1) / 2;
    buffer.mPlanes[2].mHeight = (mFrameHeight + 1) / 2;
    buffer.mPlanes[2].mWidth = (mFrameWidth + 1) / 2;
    buffer.mPlanes[2].mOffset = 0;
    buffer.mPlanes[2].mSkip = 0;

    return VideoData::CreateAndCopyData(mInfo,
                                        mImageContainer,
                                        aSample->mOffset,
                                        aSample->mTime,
                                        aSample->mDuration,
                                        buffer,
                                        aSample->mKeyframe,
                                        aSample->mTime,
                                        mPicture);
  }

private:
  VideoInfo mInfo;
  gfx::IntRect mPicture;
  uint32_t mFrameWidth;
  uint32_t mFrameHeight;
  RefPtr<layers::ImageContainer> mImageContainer;
};

class BlankAudioDataCreator : public DummyDataCreator
{
public:
  BlankAudioDataCreator(uint32_t aChannelCount, uint32_t aSampleRate)
    : mFrameSum(0), mChannelCount(aChannelCount), mSampleRate(aSampleRate)
  {
  }

  already_AddRefed<MediaData> Create(MediaRawData* aSample) override
  {
    // Convert duration to frames. We add 1 to duration to account for
    // rounding errors, so we get a consistent tone.
    CheckedInt64 frames = UsecsToFrames(aSample->mDuration+1, mSampleRate);
    if (!frames.isValid()
        || !mChannelCount
        || !mSampleRate
        || frames.value() > (UINT32_MAX / mChannelCount)) {
      return nullptr;
    }
    AlignedAudioBuffer samples(frames.value() * mChannelCount);
    if (!samples) {
      return nullptr;
    }
    // Fill the sound buffer with an A4 tone.
    static const float pi = 3.14159265f;
    static const float noteHz = 440.0f;
    for (int i = 0; i < frames.value(); i++) {
      float f = sin(2 * pi * noteHz * mFrameSum / mSampleRate);
      for (unsigned c = 0; c < mChannelCount; c++) {
        samples[i * mChannelCount + c] = AudioDataValue(f);
      }
      mFrameSum++;
    }
    RefPtr<AudioData> data(new AudioData(aSample->mOffset,
                                         aSample->mTime,
                                         aSample->mDuration,
                                         uint32_t(frames.value()),
                                         Move(samples),
                                         mChannelCount,
                                         mSampleRate));
    return data.forget();
  }

private:
  int64_t mFrameSum;
  uint32_t mChannelCount;
  uint32_t mSampleRate;
};

class BlankDecoderModule : public PlatformDecoderModule
{
public:

  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const CreateDecoderParams& aParams) override
  {
    const VideoInfo& config = aParams.VideoConfig();
    UniquePtr<DummyDataCreator> creator =
      MakeUnique<BlankVideoDataCreator>(config.mDisplay.width, config.mDisplay.height, aParams.mImageContainer);
    RefPtr<MediaDataDecoder> decoder =
      new DummyMediaDataDecoder(Move(creator), "blank media data decoder", aParams);
    return decoder.forget();
  }

  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const CreateDecoderParams& aParams) override
  {
    const AudioInfo& config = aParams.AudioConfig();
    UniquePtr<DummyDataCreator> creator =
      MakeUnique<BlankAudioDataCreator>(config.mChannels, config.mRate);
    RefPtr<MediaDataDecoder> decoder =
      new DummyMediaDataDecoder(Move(creator), "blank media data decoder", aParams);
    return decoder.forget();
  }

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const override
  {
    return true;
  }
};

already_AddRefed<PlatformDecoderModule> CreateBlankDecoderModule()
{
  RefPtr<PlatformDecoderModule> pdm = new BlankDecoderModule();
  return pdm.forget();
}

} // namespace mozilla
