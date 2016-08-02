/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "H264Converter.h"
#include "ImageContainer.h"
#include "MediaDecoderReader.h"
#include "MediaInfo.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/mozalloc.h" // for operator new, and new (fallible)
#include "mozilla/RefPtr.h"
#include "mozilla/TaskQueue.h"
#include "mp4_demuxer/H264.h"
#include "nsAutoPtr.h"
#include "nsRect.h"
#include "PlatformDecoderModule.h"
#include "ReorderQueue.h"
#include "TimeUnits.h"
#include "VideoUtils.h"

namespace mozilla {

// Decoder that uses a passed in object's Create function to create blank
// MediaData objects.
template<class BlankMediaDataCreator>
class BlankMediaDataDecoder : public MediaDataDecoder {
public:

  BlankMediaDataDecoder(BlankMediaDataCreator* aCreator,
                        const CreateDecoderParams& aParams)
    : mCreator(aCreator)
    , mCallback(aParams.mCallback)
    , mMaxRefFrames(aParams.mConfig.GetType() == TrackInfo::kVideoTrack &&
                    H264Converter::IsH264(aParams.mConfig)
                    ? mp4_demuxer::H264::ComputeMaxRefFrames(aParams.VideoConfig().mExtraData)
                    : 0)
    , mType(aParams.mConfig.GetType())
  {
  }

  RefPtr<InitPromise> Init() override {
    return InitPromise::CreateAndResolve(mType, __func__);
  }

  nsresult Shutdown() override {
    return NS_OK;
  }

  nsresult Input(MediaRawData* aSample) override
  {
    RefPtr<MediaData> data =
      mCreator->Create(media::TimeUnit::FromMicroseconds(aSample->mTime),
                       media::TimeUnit::FromMicroseconds(aSample->mDuration),
                       aSample->mOffset);

    OutputFrame(data);

    return NS_OK;
  }

  nsresult Flush() override
  {
    mReorderQueue.Clear();

    return NS_OK;
  }

  nsresult Drain() override
  {
    while (!mReorderQueue.IsEmpty()) {
      mCallback->Output(mReorderQueue.Pop().get());
    }

    mCallback->DrainComplete();
    return NS_OK;
  }

  const char* GetDescriptionName() const override
  {
    return "blank media data decoder";
  }

private:
  void OutputFrame(MediaData* aData)
  {
    if (!aData) {
      mCallback->Error(MediaDataDecoderError::FATAL_ERROR);
      return;
    }

    // Frames come out in DTS order but we need to output them in PTS order.
    mReorderQueue.Push(aData);

    while (mReorderQueue.Length() > mMaxRefFrames) {
      mCallback->Output(mReorderQueue.Pop().get());
    }

    if (mReorderQueue.Length() <= mMaxRefFrames) {
      mCallback->InputExhausted();
    }

  }

private:
  nsAutoPtr<BlankMediaDataCreator> mCreator;
  MediaDataDecoderCallback* mCallback;
  const uint32_t mMaxRefFrames;
  ReorderQueue mReorderQueue;
  TrackInfo::TrackType mType;
};

class BlankVideoDataCreator {
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

  already_AddRefed<MediaData>
  Create(const media::TimeUnit& aDTS, const media::TimeUnit& aDuration, int64_t aOffsetInStream)
  {
    // Create a fake YUV buffer in a 420 format. That is, an 8bpp Y plane,
    // with a U and V plane that are half the size of the Y plane, i.e 8 bit,
    // 2x2 subsampled.
    const int sizeY = mFrameWidth * mFrameHeight;
    const int sizeCbCr = ((mFrameWidth + 1) / 2) * ((mFrameHeight + 1) / 2);
    auto frame = MakeUnique<uint8_t[]>(sizeY + sizeCbCr);
    VideoData::YCbCrBuffer buffer;

    // Y plane.
    buffer.mPlanes[0].mData = frame.get();
    buffer.mPlanes[0].mStride = mFrameWidth;
    buffer.mPlanes[0].mHeight = mFrameHeight;
    buffer.mPlanes[0].mWidth = mFrameWidth;
    buffer.mPlanes[0].mOffset = 0;
    buffer.mPlanes[0].mSkip = 0;

    // Cb plane.
    buffer.mPlanes[1].mData = frame.get() + sizeY;
    buffer.mPlanes[1].mStride = mFrameWidth / 2;
    buffer.mPlanes[1].mHeight = mFrameHeight / 2;
    buffer.mPlanes[1].mWidth = mFrameWidth / 2;
    buffer.mPlanes[1].mOffset = 0;
    buffer.mPlanes[1].mSkip = 0;

    // Cr plane.
    buffer.mPlanes[2].mData = frame.get() + sizeY;
    buffer.mPlanes[2].mStride = mFrameWidth / 2;
    buffer.mPlanes[2].mHeight = mFrameHeight / 2;
    buffer.mPlanes[2].mWidth = mFrameWidth / 2;
    buffer.mPlanes[2].mOffset = 0;
    buffer.mPlanes[2].mSkip = 0;

    // Set to color white.
    memset(buffer.mPlanes[0].mData, 255, sizeY);
    memset(buffer.mPlanes[1].mData, 128, sizeCbCr);

    return VideoData::Create(mInfo,
                             mImageContainer,
                             nullptr,
                             aOffsetInStream,
                             aDTS.ToMicroseconds(),
                             aDuration.ToMicroseconds(),
                             buffer,
                             true,
                             aDTS.ToMicroseconds(),
                             mPicture);
  }

private:
  VideoInfo mInfo;
  gfx::IntRect mPicture;
  uint32_t mFrameWidth;
  uint32_t mFrameHeight;
  RefPtr<layers::ImageContainer> mImageContainer;
};

class BlankAudioDataCreator {
public:
  BlankAudioDataCreator(uint32_t aChannelCount, uint32_t aSampleRate)
    : mFrameSum(0), mChannelCount(aChannelCount), mSampleRate(aSampleRate)
  {
  }

  MediaData* Create(const media::TimeUnit& aDTS,
                    const media::TimeUnit& aDuration,
                    int64_t aOffsetInStream)
  {
    // Convert duration to frames. We add 1 to duration to account for
    // rounding errors, so we get a consistent tone.
    CheckedInt64 frames =
      UsecsToFrames(aDuration.ToMicroseconds()+1, mSampleRate);
    if (!frames.isValid() ||
        !mChannelCount ||
        !mSampleRate ||
        frames.value() > (UINT32_MAX / mChannelCount)) {
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
    return new AudioData(aOffsetInStream,
                         aDTS.ToMicroseconds(),
                         aDuration.ToMicroseconds(),
                         uint32_t(frames.value()),
                         Move(samples),
                         mChannelCount,
                         mSampleRate);
  }

private:
  int64_t mFrameSum;
  uint32_t mChannelCount;
  uint32_t mSampleRate;
};

class BlankDecoderModule : public PlatformDecoderModule {
public:

  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const CreateDecoderParams& aParams) override {
    const VideoInfo& config = aParams.VideoConfig();
    BlankVideoDataCreator* creator = new BlankVideoDataCreator(
      config.mDisplay.width, config.mDisplay.height, aParams.mImageContainer);
    RefPtr<MediaDataDecoder> decoder =
      new BlankMediaDataDecoder<BlankVideoDataCreator>(creator, aParams);
    return decoder.forget();
  }

  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const CreateDecoderParams& aParams) override {
    const AudioInfo& config = aParams.AudioConfig();
    BlankAudioDataCreator* creator = new BlankAudioDataCreator(
      config.mChannels, config.mRate);

    RefPtr<MediaDataDecoder> decoder =
      new BlankMediaDataDecoder<BlankAudioDataCreator>(creator, aParams);
    return decoder.forget();
  }

  bool
  SupportsMimeType(const nsACString& aMimeType,
                   DecoderDoctorDiagnostics* aDiagnostics) const override
  {
    return true;
  }

  ConversionRequired
  DecoderNeedsConversion(const TrackInfo& aConfig) const override
  {
    if (aConfig.IsVideo() && H264Converter::IsH264(aConfig)) {
      return kNeedAVCC;
    } else {
      return kNeedNone;
    }
  }

};

already_AddRefed<PlatformDecoderModule> CreateBlankDecoderModule()
{
  RefPtr<PlatformDecoderModule> pdm = new BlankDecoderModule();
  return pdm.forget();
}

} // namespace mozilla
