/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MOZILLA_AUDIOSAMPLEFORMAT_H_
#define MOZILLA_AUDIOSAMPLEFORMAT_H_

#include "nsAlgorithm.h"
#include <algorithm>

namespace mozilla {

/**
 * Audio formats supported in MediaStreams and media elements.
 *
 * Only one of these is supported by AudioStream, and that is determined
 * at compile time (roughly, FLOAT32 on desktops, S16 on mobile). Media decoders
 * produce that format only; queued AudioData always uses that format.
 */
enum AudioSampleFormat
{
  // Native-endian signed 16-bit audio samples
  AUDIO_FORMAT_S16,
  // Signed 32-bit float samples
  AUDIO_FORMAT_FLOAT32,
  // The format used for output by AudioStream.
#ifdef MOZ_SAMPLE_TYPE_S16
  AUDIO_OUTPUT_FORMAT = AUDIO_FORMAT_S16
#else
  AUDIO_OUTPUT_FORMAT = AUDIO_FORMAT_FLOAT32
#endif
};

template <AudioSampleFormat Format> class AudioSampleTraits;

template <> class AudioSampleTraits<AUDIO_FORMAT_FLOAT32> {
public:
  typedef float Type;
};
template <> class AudioSampleTraits<AUDIO_FORMAT_S16> {
public:
  typedef int16_t Type;
};

typedef AudioSampleTraits<AUDIO_OUTPUT_FORMAT>::Type AudioDataValue;

// Single-sample conversion

/*
 * Use "2^N" conversion since it's simple, fast, "bit transparent", used by
 * many other libraries and apparently behaves reasonably.
 * http://blog.bjornroche.com/2009/12/int-float-int-its-jungle-out-there.html
 * http://blog.bjornroche.com/2009/12/linearity-and-dynamic-range-in-int.html
 */
inline float
AudioSampleToFloat(float aValue)
{
  return aValue;
}
inline float
AudioSampleToFloat(int16_t aValue)
{
  return aValue/32768.0f;
}

template <typename T> T FloatToAudioSample(float aValue);

template <> inline float
FloatToAudioSample<float>(float aValue)
{
  return aValue;
}
template <> inline int16_t
FloatToAudioSample<int16_t>(float aValue)
{
  float v = aValue*32768.0f;
  float clamped = std::max(-32768.0f, std::min(32767.0f, v));
  return int16_t(clamped);
}

// Sample buffer conversion

template <typename From, typename To> inline void
ConvertAudioSamples(const From* aFrom, To* aTo, int aCount)
{
  for (int i = 0; i < aCount; ++i) {
    aTo[i] = FloatToAudioSample<To>(AudioSampleToFloat(aFrom[i]));
  }
}
inline void
ConvertAudioSamples(const int16_t* aFrom, int16_t* aTo, int aCount)
{
  memcpy(aTo, aFrom, sizeof(*aTo)*aCount);
}
inline void
ConvertAudioSamples(const float* aFrom, float* aTo, int aCount)
{
  memcpy(aTo, aFrom, sizeof(*aTo)*aCount);
}

// Sample buffer conversion with scale

template <typename From, typename To> inline void
ConvertAudioSamplesWithScale(const From* aFrom, To* aTo, int aCount, float aScale)
{
  if (aScale == 1.0f) {
    ConvertAudioSamples(aFrom, aTo, aCount);
    return;
  }
  for (int i = 0; i < aCount; ++i) {
    aTo[i] = FloatToAudioSample<To>(AudioSampleToFloat(aFrom[i])*aScale);
  }
}
inline void
ConvertAudioSamplesWithScale(const int16_t* aFrom, int16_t* aTo, int aCount, float aScale)
{
  if (aScale == 1.0f) {
    ConvertAudioSamples(aFrom, aTo, aCount);
    return;
  }
  if (0.0f <= aScale && aScale < 1.0f) {
    int32_t scale = int32_t((1 << 16) * aScale);
    for (int i = 0; i < aCount; ++i) {
      aTo[i] = int16_t((int32_t(aFrom[i]) * scale) >> 16);
    }
    return;
  }
  for (int i = 0; i < aCount; ++i) {
    aTo[i] = FloatToAudioSample<int16_t>(AudioSampleToFloat(aFrom[i])*aScale);
  }
}

// In place audio sample scaling.
inline void
ScaleAudioSamples(float* aBuffer, int aCount, float aScale)
{
  for (int32_t i = 0; i < aCount; ++i) {
    aBuffer[i] *= aScale;
  }
}

inline void
ScaleAudioSamples(short* aBuffer, int aCount, float aScale)
{
  int32_t volume = int32_t((1 << 16) * aScale);
  for (int32_t i = 0; i < aCount; ++i) {
    aBuffer[i] = short((int32_t(aBuffer[i]) * volume) >> 16);
  }
}

inline const void*
AddAudioSampleOffset(const void* aBase, AudioSampleFormat aFormat,
                     int32_t aOffset)
{
  switch (aFormat) {
  case AUDIO_FORMAT_FLOAT32:
    return static_cast<const float*>(aBase) + aOffset;
  case AUDIO_FORMAT_S16:
    return static_cast<const int16_t*>(aBase) + aOffset;
  default:
    NS_ERROR("Unknown format");
    return nullptr;
  }
}

} // namespace mozilla

#endif /* MOZILLA_AUDIOSAMPLEFORMAT_H_ */
