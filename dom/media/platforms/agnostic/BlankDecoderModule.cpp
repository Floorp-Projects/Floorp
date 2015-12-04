/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageContainer.h"
#include "MediaDecoderReader.h"
#include "MediaInfo.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/mozalloc.h" // for operator new, and new (fallible)
#include "mozilla/RefPtr.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/UniquePtr.h"
#include "nsRect.h"
#include "PlatformDecoderModule.h"
#include "TimeUnits.h"
#include "VideoUtils.h"

namespace mozilla {

// Decoder that uses a passed in object's Create function to create blank
// MediaData objects.
template<class BlankMediaDataCreator>
class BlankMediaDataDecoder : public MediaDataDecoder {
public:

  BlankMediaDataDecoder(BlankMediaDataCreator* aCreator,
                        FlushableTaskQueue* aTaskQueue,
                        MediaDataDecoderCallback* aCallback,
                        TrackInfo::TrackType aType)
    : mCreator(aCreator)
    , mTaskQueue(aTaskQueue)
    , mCallback(aCallback)
    , mType(aType)
  {
  }

  RefPtr<InitPromise> Init() override {
    return InitPromise::CreateAndResolve(mType, __func__);
  }

  nsresult Shutdown() override {
    return NS_OK;
  }

  class OutputEvent : public nsRunnable {
  public:
    OutputEvent(MediaRawData* aSample,
                MediaDataDecoderCallback* aCallback,
                BlankMediaDataCreator* aCreator)
      : mSample(aSample)
      , mCreator(aCreator)
      , mCallback(aCallback)
    {
    }
    NS_IMETHOD Run() override
    {
      RefPtr<MediaData> data =
        mCreator->Create(media::TimeUnit::FromMicroseconds(mSample->mTime),
                         media::TimeUnit::FromMicroseconds(mSample->mDuration),
                         mSample->mOffset);
      if (!data) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      mCallback->Output(data);
      return NS_OK;
    }
  private:
    RefPtr<MediaRawData> mSample;
    BlankMediaDataCreator* mCreator;
    MediaDataDecoderCallback* mCallback;
  };

  nsresult Input(MediaRawData* aSample) override
  {
    // The MediaDataDecoder must delete the sample when we're finished
    // with it, so the OutputEvent stores it in an nsAutoPtr and deletes
    // it once it's run.
    RefPtr<nsIRunnable> r(new OutputEvent(aSample, mCallback, mCreator));
    mTaskQueue->Dispatch(r.forget());
    return NS_OK;
  }

  nsresult Flush() override {
    mTaskQueue->Flush();
    return NS_OK;
  }

  nsresult Drain() override {
    mCallback->DrainComplete();
    return NS_OK;
  }

private:
  nsAutoPtr<BlankMediaDataCreator> mCreator;
  RefPtr<FlushableTaskQueue> mTaskQueue;
  MediaDataDecoderCallback* mCallback;
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
    // 2x2 subsampled. Have the data pointers of each frame point to the
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
    buffer.mPlanes[1].mStride = mFrameWidth / 2;
    buffer.mPlanes[1].mHeight = mFrameHeight / 2;
    buffer.mPlanes[1].mWidth = mFrameWidth / 2;
    buffer.mPlanes[1].mOffset = 0;
    buffer.mPlanes[1].mSkip = 0;

    // Cr plane.
    buffer.mPlanes[2].mData = frame.get();
    buffer.mPlanes[2].mStride = mFrameWidth / 2;
    buffer.mPlanes[2].mHeight = mFrameHeight / 2;
    buffer.mPlanes[2].mWidth = mFrameWidth / 2;
    buffer.mPlanes[2].mOffset = 0;
    buffer.mPlanes[2].mSkip = 0;

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
    UniquePtr<AudioDataValue[]> samples(
      new (fallible) AudioDataValue[frames.value() * mChannelCount]);
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
  CreateVideoDecoder(const VideoInfo& aConfig,
                     layers::LayersBackend aLayersBackend,
                     layers::ImageContainer* aImageContainer,
                     FlushableTaskQueue* aVideoTaskQueue,
                     MediaDataDecoderCallback* aCallback) override {
    BlankVideoDataCreator* creator = new BlankVideoDataCreator(
      aConfig.mDisplay.width, aConfig.mDisplay.height, aImageContainer);
    RefPtr<MediaDataDecoder> decoder =
      new BlankMediaDataDecoder<BlankVideoDataCreator>(creator,
                                                       aVideoTaskQueue,
                                                       aCallback,
                                                       TrackInfo::kVideoTrack);
    return decoder.forget();
  }

  // Decode thread.
  already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const AudioInfo& aConfig,
                     FlushableTaskQueue* aAudioTaskQueue,
                     MediaDataDecoderCallback* aCallback) override {
    BlankAudioDataCreator* creator = new BlankAudioDataCreator(
      aConfig.mChannels, aConfig.mRate);

    RefPtr<MediaDataDecoder> decoder =
      new BlankMediaDataDecoder<BlankAudioDataCreator>(creator,
                                                       aAudioTaskQueue,
                                                       aCallback,
                                                       TrackInfo::kAudioTrack);
    return decoder.forget();
  }

  bool
  SupportsMimeType(const nsACString& aMimeType) const override
  {
    return true;
  }

  ConversionRequired
  DecoderNeedsConversion(const TrackInfo& aConfig) const override
  {
    return kNeedNone;
  }

};

already_AddRefed<PlatformDecoderModule> CreateBlankDecoderModule()
{
  RefPtr<PlatformDecoderModule> pdm = new BlankDecoderModule();
  return pdm.forget();
}

} // namespace mozilla
