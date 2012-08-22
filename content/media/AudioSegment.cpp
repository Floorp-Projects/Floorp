/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSegment.h"

namespace mozilla {

static uint16_t
FlipByteOrderIfBigEndian(uint16_t aValue)
{
  uint16_t s = aValue;
#if defined(IS_BIG_ENDIAN)
  s = (s << 8) | (s >> 8);
#endif
  return s;
}

/*
 * Use "2^N" conversion since it's simple, fast, "bit transparent", used by
 * many other libraries and apparently behaves reasonably.
 * http://blog.bjornroche.com/2009/12/int-float-int-its-jungle-out-there.html
 * http://blog.bjornroche.com/2009/12/linearity-and-dynamic-range-in-int.html
 */
static float
SampleToFloat(float aValue)
{
  return aValue;
}
static float
SampleToFloat(uint8_t aValue)
{
  return (aValue - 128)/128.0f;
}
static float
SampleToFloat(int16_t aValue)
{
  return int16_t(FlipByteOrderIfBigEndian(aValue))/32768.0f;
}

static void
FloatToSample(float aValue, float* aOut)
{
  *aOut = aValue;
}
static void
FloatToSample(float aValue, uint8_t* aOut)
{
  float v = aValue*128 + 128;
  float clamped = NS_MAX(0.0f, NS_MIN(255.0f, v));
  *aOut = uint8_t(clamped);
}
static void
FloatToSample(float aValue, int16_t* aOut)
{
  float v = aValue*32768.0f;
  float clamped = NS_MAX(-32768.0f, NS_MIN(32767.0f, v));
  *aOut = int16_t(FlipByteOrderIfBigEndian(int16_t(clamped)));
}

template <class SrcT, class DestT>
static void
InterleaveAndConvertBuffer(const SrcT* aSource, int32_t aSourceLength,
                           int32_t aLength,
                           float aVolume,
                           int32_t aChannels,
                           DestT* aOutput)
{
  DestT* output = aOutput;
  for (int32_t i = 0; i < aLength; ++i) {
    for (int32_t channel = 0; channel < aChannels; ++channel) {
      float v = SampleToFloat(aSource[channel*aSourceLength + i])*aVolume;
      FloatToSample(v, output);
      ++output;
    }
  }
}

static void
InterleaveAndConvertBuffer(const int16_t* aSource, int32_t aSourceLength,
                           int32_t aLength,
                           float aVolume,
                           int32_t aChannels,
                           int16_t* aOutput)
{
  int16_t* output = aOutput;
  float v = NS_MAX(NS_MIN(aVolume, 1.0f), -1.0f);
  int32_t volume = int32_t((1 << 16) * v);
  for (int32_t i = 0; i < aLength; ++i) {
    for (int32_t channel = 0; channel < aChannels; ++channel) {
      int16_t s = FlipByteOrderIfBigEndian(aSource[channel*aSourceLength + i]);
      *output = FlipByteOrderIfBigEndian(int16_t((int32_t(s) * volume) >> 16));
      ++output;
    }
  }
}

template <class SrcT>
static void
InterleaveAndConvertBuffer(const SrcT* aSource, int32_t aSourceLength,
                           int32_t aLength,
                           float aVolume,
                           int32_t aChannels,
                           void* aOutput, nsAudioStream::SampleFormat aOutputFormat)
{
  switch (aOutputFormat) {
  case nsAudioStream::FORMAT_FLOAT32:
    InterleaveAndConvertBuffer(aSource, aSourceLength, aLength, aVolume,
                               aChannels, static_cast<float*>(aOutput));
    break;
  case nsAudioStream::FORMAT_S16_LE:
    InterleaveAndConvertBuffer(aSource, aSourceLength, aLength, aVolume,
                               aChannels, static_cast<int16_t*>(aOutput));
    break;
  case nsAudioStream::FORMAT_U8:
    InterleaveAndConvertBuffer(aSource, aSourceLength, aLength, aVolume,
                               aChannels, static_cast<uint8_t*>(aOutput));
    break;
  }
}

static void
InterleaveAndConvertBuffer(const void* aSource, nsAudioStream::SampleFormat aSourceFormat,
                           int32_t aSourceLength,
                           int32_t aOffset, int32_t aLength,
                           float aVolume,
                           int32_t aChannels,
                           void* aOutput, nsAudioStream::SampleFormat aOutputFormat)
{
  switch (aSourceFormat) {
  case nsAudioStream::FORMAT_FLOAT32:
    InterleaveAndConvertBuffer(static_cast<const float*>(aSource) + aOffset, aSourceLength,
                               aLength,
                               aVolume,
                               aChannels,
                               aOutput, aOutputFormat);
    break;
  case nsAudioStream::FORMAT_S16_LE:
    InterleaveAndConvertBuffer(static_cast<const int16_t*>(aSource) + aOffset, aSourceLength,
                               aLength,
                               aVolume,
                               aChannels,
                               aOutput, aOutputFormat);
    break;
  case nsAudioStream::FORMAT_U8:
    InterleaveAndConvertBuffer(static_cast<const uint8_t*>(aSource) + aOffset, aSourceLength,
                               aLength,
                               aVolume,
                               aChannels,
                               aOutput, aOutputFormat);
    break;
  }
}

void
AudioSegment::ApplyVolume(float aVolume)
{
  for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
    ci->mVolume *= aVolume;
  }
}

static const int STATIC_AUDIO_BUFFER_BYTES = 50000;

void
AudioSegment::WriteTo(nsAudioStream* aOutput)
{
  NS_ASSERTION(mChannels == aOutput->GetChannels(), "Wrong number of channels");
  nsAutoTArray<uint8_t,STATIC_AUDIO_BUFFER_BYTES> buf;
  uint32_t frameSize = GetSampleSize(aOutput->GetFormat())*mChannels;
  for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
    AudioChunk& c = *ci;
    if (frameSize*c.mDuration > PR_UINT32_MAX) {
      NS_ERROR("Buffer overflow");
      return;
    }
    buf.SetLength(int32_t(frameSize*c.mDuration));
    if (c.mBuffer) {
      InterleaveAndConvertBuffer(c.mBuffer->Data(), c.mBufferFormat, c.mBufferLength,
                                 c.mOffset, int32_t(c.mDuration),
                                 c.mVolume,
                                 aOutput->GetChannels(),
                                 buf.Elements(), aOutput->GetFormat());
    } else {
      // Assumes that a bit pattern of zeroes == 0.0f
      memset(buf.Elements(), 0, buf.Length());
    }
    aOutput->Write(buf.Elements(), int32_t(c.mDuration));
  }
}

}
