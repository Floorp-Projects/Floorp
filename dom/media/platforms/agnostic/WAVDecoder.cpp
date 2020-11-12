/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WAVDecoder.h"

#include "AudioSampleFormat.h"
#include "BufferReader.h"
#include "VideoUtils.h"
#include "mozilla/Casting.h"
#include "mozilla/SyncRunnable.h"

namespace mozilla {

int16_t DecodeALawSample(uint8_t aValue) {
  aValue = aValue ^ 0x55;
  int8_t sign = (aValue & 0x80) ? -1 : 1;
  uint8_t exponent = (aValue & 0x70) >> 4;
  uint8_t mantissa = aValue & 0x0F;
  int16_t sample = mantissa << 4;
  switch (exponent) {
    case 0:
      sample += 8;
      break;
    case 1:
      sample += 0x108;
      break;
    default:
      sample += 0x108;
      sample <<= exponent - 1;
  }
  return sign * sample;
}

int16_t DecodeULawSample(uint8_t aValue) {
  aValue = aValue ^ 0xFF;
  int8_t sign = (aValue & 0x80) ? -1 : 1;
  uint8_t exponent = (aValue & 0x70) >> 4;
  uint8_t mantissa = aValue & 0x0F;
  int16_t sample = (33 + 2 * mantissa) * (2 << (exponent + 1)) - 33;
  return sign * sample;
}

WaveDataDecoder::WaveDataDecoder(const CreateDecoderParams& aParams)
    : mInfo(aParams.AudioConfig()) {}

RefPtr<ShutdownPromise> WaveDataDecoder::Shutdown() {
  // mThread may not be set if Init hasn't been called first.
  MOZ_ASSERT(!mThread || mThread->IsOnCurrentThread());
  return ShutdownPromise::CreateAndResolve(true, __func__);
}

RefPtr<MediaDataDecoder::InitPromise> WaveDataDecoder::Init() {
  mThread = GetCurrentSerialEventTarget();
  return InitPromise::CreateAndResolve(TrackInfo::kAudioTrack, __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> WaveDataDecoder::Decode(
    MediaRawData* aSample) {
  MOZ_ASSERT(mThread->IsOnCurrentThread());
  size_t aLength = aSample->Size();
  BufferReader aReader(aSample->Data(), aLength);
  int64_t aOffset = aSample->mOffset;

  int32_t frames = aLength * 8 / mInfo.mBitDepth / mInfo.mChannels;

  AlignedAudioBuffer buffer(frames * mInfo.mChannels);
  if (!buffer) {
    return DecodePromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY, __func__), __func__);
  }
  for (int i = 0; i < frames; ++i) {
    for (unsigned int j = 0; j < mInfo.mChannels; ++j) {
      if (mInfo.mProfile == 3) {  // IEEE Float Data
        auto res = aReader.ReadLEU32();
        if (res.isErr()) {
          return DecodePromise::CreateAndReject(
              MediaResult(res.unwrapErr(), __func__), __func__);
        }
        float sample = BitwiseCast<float>(res.unwrap());
        buffer[i * mInfo.mChannels + j] =
            FloatToAudioSample<AudioDataValue>(sample);
      } else if (mInfo.mProfile == 6) {  // ALAW Data
        auto res = aReader.ReadU8();
        if (res.isErr()) {
          return DecodePromise::CreateAndReject(
              MediaResult(res.unwrapErr(), __func__), __func__);
        }
        int16_t decoded = DecodeALawSample(res.unwrap());
        buffer[i * mInfo.mChannels + j] =
            IntegerToAudioSample<AudioDataValue>(decoded);
      } else if (mInfo.mProfile == 7) {  // ULAW Data
        auto res = aReader.ReadU8();
        if (res.isErr()) {
          return DecodePromise::CreateAndReject(
              MediaResult(res.unwrapErr(), __func__), __func__);
        }
        int16_t decoded = DecodeULawSample(res.unwrap());
        buffer[i * mInfo.mChannels + j] =
            IntegerToAudioSample<AudioDataValue>(decoded);
      } else {  // PCM Data
        if (mInfo.mBitDepth == 8) {
          auto res = aReader.ReadU8();
          if (res.isErr()) {
            return DecodePromise::CreateAndReject(
                MediaResult(res.unwrapErr(), __func__), __func__);
          }
          buffer[i * mInfo.mChannels + j] =
              UInt8bitToAudioSample<AudioDataValue>(res.unwrap());
        } else if (mInfo.mBitDepth == 16) {
          auto res = aReader.ReadLE16();
          if (res.isErr()) {
            return DecodePromise::CreateAndReject(
                MediaResult(res.unwrapErr(), __func__), __func__);
          }
          buffer[i * mInfo.mChannels + j] =
              IntegerToAudioSample<AudioDataValue>(res.unwrap());
        } else if (mInfo.mBitDepth == 24) {
          auto res = aReader.ReadLE24();
          if (res.isErr()) {
            return DecodePromise::CreateAndReject(
                MediaResult(res.unwrapErr(), __func__), __func__);
          }
          buffer[i * mInfo.mChannels + j] =
              Int24bitToAudioSample<AudioDataValue>(res.unwrap());
        }
      }
    }
  }

  return DecodePromise::CreateAndResolve(
      DecodedData{new AudioData(aOffset, aSample->mTime, std::move(buffer),
                                mInfo.mChannels, mInfo.mRate)},
      __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> WaveDataDecoder::Drain() {
  MOZ_ASSERT(mThread->IsOnCurrentThread());
  return DecodePromise::CreateAndResolve(DecodedData(), __func__);
}

RefPtr<MediaDataDecoder::FlushPromise> WaveDataDecoder::Flush() {
  MOZ_ASSERT(mThread->IsOnCurrentThread());
  return FlushPromise::CreateAndResolve(true, __func__);
}

/* static */
bool WaveDataDecoder::IsWave(const nsACString& aMimeType) {
  // Some WebAudio uses "audio/x-wav",
  // WAVdemuxer uses "audio/wave; codecs=aNum".
  return aMimeType.EqualsLiteral("audio/x-wav") ||
         aMimeType.EqualsLiteral("audio/wave; codecs=1") ||
         aMimeType.EqualsLiteral("audio/wave; codecs=3") ||
         aMimeType.EqualsLiteral("audio/wave; codecs=6") ||
         aMimeType.EqualsLiteral("audio/wave; codecs=7") ||
         aMimeType.EqualsLiteral("audio/wave; codecs=65534");
}

}  // namespace mozilla
#undef LOG
