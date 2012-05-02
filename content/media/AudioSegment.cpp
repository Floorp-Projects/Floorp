/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSegment.h"

namespace mozilla {

static PRUint16
FlipByteOrderIfBigEndian(PRUint16 aValue)
{
  PRUint16 s = aValue;
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
SampleToFloat(PRUint8 aValue)
{
  return (aValue - 128)/128.0f;
}
static float
SampleToFloat(PRInt16 aValue)
{
  return PRInt16(FlipByteOrderIfBigEndian(aValue))/32768.0f;
}

static void
FloatToSample(float aValue, float* aOut)
{
  *aOut = aValue;
}
static void
FloatToSample(float aValue, PRUint8* aOut)
{
  float v = aValue*128 + 128;
  float clamped = NS_MAX(0.0f, NS_MIN(255.0f, v));
  *aOut = PRUint8(clamped);
}
static void
FloatToSample(float aValue, PRInt16* aOut)
{
  float v = aValue*32768.0f;
  float clamped = NS_MAX(-32768.0f, NS_MIN(32767.0f, v));
  *aOut = PRInt16(FlipByteOrderIfBigEndian(PRInt16(clamped)));
}

template <class SrcT, class DestT>
static void
InterleaveAndConvertBuffer(const SrcT* aSource, PRInt32 aSourceLength,
                           PRInt32 aLength,
                           float aVolume,
                           PRInt32 aChannels,
                           DestT* aOutput)
{
  DestT* output = aOutput;
  for (PRInt32 i = 0; i < aLength; ++i) {
    for (PRInt32 channel = 0; channel < aChannels; ++channel) {
      float v = SampleToFloat(aSource[channel*aSourceLength + i])*aVolume;
      FloatToSample(v, output);
      ++output;
    }
  }
}

static void
InterleaveAndConvertBuffer(const PRInt16* aSource, PRInt32 aSourceLength,
                           PRInt32 aLength,
                           float aVolume,
                           PRInt32 aChannels,
                           PRInt16* aOutput)
{
  PRInt16* output = aOutput;
  float v = NS_MAX(NS_MIN(aVolume, 1.0f), -1.0f);
  PRInt32 volume = PRInt32((1 << 16) * v);
  for (PRInt32 i = 0; i < aLength; ++i) {
    for (PRInt32 channel = 0; channel < aChannels; ++channel) {
      PRInt16 s = FlipByteOrderIfBigEndian(aSource[channel*aSourceLength + i]);
      *output = FlipByteOrderIfBigEndian(PRInt16((PRInt32(s) * volume) >> 16));
      ++output;
    }
  }
}

template <class SrcT>
static void
InterleaveAndConvertBuffer(const SrcT* aSource, PRInt32 aSourceLength,
                           PRInt32 aLength,
                           float aVolume,
                           PRInt32 aChannels,
                           void* aOutput, nsAudioStream::SampleFormat aOutputFormat)
{
  switch (aOutputFormat) {
  case nsAudioStream::FORMAT_FLOAT32:
    InterleaveAndConvertBuffer(aSource, aSourceLength, aLength, aVolume,
                               aChannels, static_cast<float*>(aOutput));
    break;
  case nsAudioStream::FORMAT_S16_LE:
    InterleaveAndConvertBuffer(aSource, aSourceLength, aLength, aVolume,
                               aChannels, static_cast<PRInt16*>(aOutput));
    break;
  case nsAudioStream::FORMAT_U8:
    InterleaveAndConvertBuffer(aSource, aSourceLength, aLength, aVolume,
                               aChannels, static_cast<PRUint8*>(aOutput));
    break;
  }
}

static void
InterleaveAndConvertBuffer(const void* aSource, nsAudioStream::SampleFormat aSourceFormat,
                           PRInt32 aSourceLength,
                           PRInt32 aOffset, PRInt32 aLength,
                           float aVolume,
                           PRInt32 aChannels,
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
    InterleaveAndConvertBuffer(static_cast<const PRInt16*>(aSource) + aOffset, aSourceLength,
                               aLength,
                               aVolume,
                               aChannels,
                               aOutput, aOutputFormat);
    break;
  case nsAudioStream::FORMAT_U8:
    InterleaveAndConvertBuffer(static_cast<const PRUint8*>(aSource) + aOffset, aSourceLength,
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
  nsAutoTArray<PRUint8,STATIC_AUDIO_BUFFER_BYTES> buf;
  PRUint32 frameSize = GetSampleSize(aOutput->GetFormat())*mChannels;
  for (ChunkIterator ci(*this); !ci.IsEnded(); ci.Next()) {
    AudioChunk& c = *ci;
    if (frameSize*c.mDuration > PR_UINT32_MAX) {
      NS_ERROR("Buffer overflow");
      return;
    }
    buf.SetLength(PRInt32(frameSize*c.mDuration));
    if (c.mBuffer) {
      InterleaveAndConvertBuffer(c.mBuffer->Data(), c.mBufferFormat, c.mBufferLength,
                                 c.mOffset, PRInt32(c.mDuration),
                                 c.mVolume,
                                 aOutput->GetChannels(),
                                 buf.Elements(), aOutput->GetFormat());
    } else {
      // Assumes that a bit pattern of zeroes == 0.0f
      memset(buf.Elements(), 0, buf.Length());
    }
    aOutput->Write(buf.Elements(), PRInt32(c.mDuration));
  }
}

}
