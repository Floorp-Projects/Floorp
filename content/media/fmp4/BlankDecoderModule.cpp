/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDecoderReader.h"
#include "PlatformDecoderModule.h"
#include "nsRect.h"
#include "mozilla/RefPtr.h"
#include "mozilla/CheckedInt.h"
#include "VideoUtils.h"
#include "ImageContainer.h"

namespace mozilla {

// Decoder that uses a passed in object's Create function to create blank
// MediaData objects.
template<class BlankMediaDataCreator>
class BlankMediaDataDecoder : public MediaDataDecoder {
public:

  BlankMediaDataDecoder(BlankMediaDataCreator* aCreator)
    : mCreator(aCreator),
      mNextDTS(-1),
      mNextOffset(-1)
  {
  }

  virtual nsresult Shutdown() MOZ_OVERRIDE {
    return NS_OK;
  }

  virtual DecoderStatus Input(const uint8_t* aData,
                              uint32_t aLength,
                              Microseconds aDTS,
                              Microseconds aPTS,
                              int64_t aOffsetInStream) MOZ_OVERRIDE
  {
    // Accepts input, and outputs on the second input, using the difference
    // in DTS as the duration.
    if (mOutput) {
      return DECODE_STATUS_NOT_ACCEPTING;
    }
    if (mNextDTS != -1 && mNextOffset != -1) {
      Microseconds duration = aDTS - mNextDTS;
      mOutput = mCreator->Create(mNextDTS, duration, mNextOffset);
    }

    mNextDTS = aDTS;
    mNextOffset = aOffsetInStream;
    return DECODE_STATUS_OK;
  }

  virtual DecoderStatus Output(nsAutoPtr<MediaData>& aOutData) MOZ_OVERRIDE
  {
    if (!mOutput) {
      return DECODE_STATUS_NEED_MORE_INPUT;
    }
    aOutData = mOutput.forget();
    return DECODE_STATUS_OK;
  }

  virtual DecoderStatus Flush()  MOZ_OVERRIDE {
    return DECODE_STATUS_OK;
  }
private:
  nsAutoPtr<BlankMediaDataCreator> mCreator;
  Microseconds mNextDTS;
  int64_t mNextOffset;
  nsAutoPtr<MediaData> mOutput;
  bool mHasInput;
};

static const uint32_t sFrameWidth = 320;
static const uint32_t sFrameHeight = 240;

class BlankVideoDataCreator {
public:
  BlankVideoDataCreator(layers::ImageContainer* aImageContainer)
    : mImageContainer(aImageContainer)
  {
    mInfo.mDisplay = nsIntSize(sFrameWidth, sFrameHeight);
    mPicture = nsIntRect(0, 0, sFrameWidth, sFrameHeight);
  }

  MediaData* Create(Microseconds aDTS,
                    Microseconds aDuration,
                    int64_t aOffsetInStream)
  {
    // Create a fake YUV buffer in a 420 format. That is, an 8bpp Y plane,
    // with a U and V plane that are half the size of the Y plane, i.e 8 bit,
    // 2x2 subsampled. Have the data pointers of each frame point to the
    // first plane, they'll always be zero'd memory anyway.
    uint8_t* frame = new uint8_t[sFrameWidth * sFrameHeight];
    memset(frame, 0, sFrameWidth * sFrameHeight);
    VideoData::YCbCrBuffer buffer;

    // Y plane.
    buffer.mPlanes[0].mData = frame;
    buffer.mPlanes[0].mStride = sFrameWidth;
    buffer.mPlanes[0].mHeight = sFrameHeight;
    buffer.mPlanes[0].mWidth = sFrameWidth;
    buffer.mPlanes[0].mOffset = 0;
    buffer.mPlanes[0].mSkip = 0;

    // Cb plane.
    buffer.mPlanes[1].mData = frame;
    buffer.mPlanes[1].mStride = sFrameWidth / 2;
    buffer.mPlanes[1].mHeight = sFrameHeight / 2;
    buffer.mPlanes[1].mWidth = sFrameWidth / 2;
    buffer.mPlanes[1].mOffset = 0;
    buffer.mPlanes[1].mSkip = 0;

    // Cr plane.
    buffer.mPlanes[2].mData = frame;
    buffer.mPlanes[2].mStride = sFrameWidth / 2;
    buffer.mPlanes[2].mHeight = sFrameHeight / 2;
    buffer.mPlanes[2].mWidth = sFrameWidth / 2;
    buffer.mPlanes[2].mOffset = 0;
    buffer.mPlanes[2].mSkip = 0;

    return VideoData::Create(mInfo,
                             mImageContainer,
                             nullptr,
                             aOffsetInStream,
                             aDTS,
                             aDuration,
                             buffer,
                             true,
                             aDTS,
                             mPicture);
  }
private:
  VideoInfo mInfo;
  nsIntRect mPicture;
  RefPtr<layers::ImageContainer> mImageContainer;
};


class BlankAudioDataCreator {
public:
  BlankAudioDataCreator(uint32_t aChannelCount,
                        uint32_t aSampleRate,
                        uint16_t aBitsPerSample)
    : mFrameSum(0),
      mChannelCount(aChannelCount),
      mSampleRate(aSampleRate),
      mBitsPerSample(aBitsPerSample)
  {
  }

  MediaData* Create(Microseconds aDTS,
                    Microseconds aDuration,
                    int64_t aOffsetInStream)
  {
    // Convert duration to frames. We add 1 to duration to account for
    // rounding errors, so we get a consistent tone.
    CheckedInt64 frames = UsecsToFrames(aDuration+1, mSampleRate);
    if (!frames.isValid() ||
        !mChannelCount ||
        !mSampleRate ||
        frames.value() > (UINT32_MAX / mChannelCount)) {
      return nullptr;
    }
    AudioDataValue* samples = new AudioDataValue[frames.value() * mChannelCount];
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
                         aDTS,
                         aDuration,
                         uint32_t(frames.value()),
                         samples,
                         mChannelCount);
  }

private:
  int64_t mFrameSum;
  uint32_t mChannelCount;
  uint32_t mSampleRate;
  uint16_t mBitsPerSample;
};

class BlankDecoderModule : public PlatformDecoderModule {
public:

  // Called when the decoders have shutdown. Main thread only.
  virtual nsresult Shutdown() MOZ_OVERRIDE {
    return NS_OK;
  }

  // Decode thread.
  virtual MediaDataDecoder* CreateH264Decoder(layers::LayersBackend aLayersBackend,
                                              layers::ImageContainer* aImageContainer) MOZ_OVERRIDE {
    BlankVideoDataCreator* decoder = new BlankVideoDataCreator(aImageContainer);
    return new BlankMediaDataDecoder<BlankVideoDataCreator>(decoder);
  }

  // Decode thread.
  virtual MediaDataDecoder* CreateAACDecoder(uint32_t aChannelCount,
                                             uint32_t aSampleRate,
                                             uint16_t aBitsPerSample,
                                             const uint8_t* aUserData,
                                             uint32_t aUserDataLength) MOZ_OVERRIDE {
    BlankAudioDataCreator* decoder = new BlankAudioDataCreator(aChannelCount,
                                                               aSampleRate,
                                                               aBitsPerSample);
    return new BlankMediaDataDecoder<BlankAudioDataCreator>(decoder);
  }
};

PlatformDecoderModule* CreateBlankDecoderModule()
{
  return new BlankDecoderModule();
}

} // namespace mozilla
