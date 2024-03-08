/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#ifndef MOZILLA_AUDIOSAMPLEFORMAT_H_
#define MOZILLA_AUDIOSAMPLEFORMAT_H_

#include "mozilla/Assertions.h"
#include "mozilla/PodOperations.h"
#include <algorithm>
#include <type_traits>
#include <limits>

namespace mozilla {

/**
 * Audio formats supported in MediaTracks and media elements.
 *
 * Only one of these is supported by AudioStream, and that is determined
 * at compile time (roughly, FLOAT32 on desktops, S16 on mobile). Media decoders
 * produce that format only; queued AudioData always uses that format.
 */
enum AudioSampleFormat {
  // Silence: format will be chosen later
  AUDIO_FORMAT_SILENCE,
  // Native-endian signed 16-bit audio samples
  AUDIO_FORMAT_S16,
  // Signed 32-bit float samples
  AUDIO_FORMAT_FLOAT32,
  // The format used for output by AudioStream.
  AUDIO_OUTPUT_FORMAT = AUDIO_FORMAT_FLOAT32
};

enum { MAX_AUDIO_SAMPLE_SIZE = sizeof(float) };

template <AudioSampleFormat Format>
class AudioSampleTraits;

template <>
class AudioSampleTraits<AUDIO_FORMAT_FLOAT32> {
 public:
  using Type = float;
};
template <>
class AudioSampleTraits<AUDIO_FORMAT_S16> {
 public:
  using Type = int16_t;
};

using AudioDataValue = AudioSampleTraits<AUDIO_OUTPUT_FORMAT>::Type;

template <typename T>
class AudioSampleTypeToFormat;

template <>
class AudioSampleTypeToFormat<float> {
 public:
  static const AudioSampleFormat Format = AUDIO_FORMAT_FLOAT32;
};

template <>
class AudioSampleTypeToFormat<short> {
 public:
  static const AudioSampleFormat Format = AUDIO_FORMAT_S16;
};

template <typename T>
constexpr float MaxAsFloat() {
  return static_cast<float>(std::numeric_limits<T>::max());
}

template <typename T>
constexpr float LowestAsFloat() {
  return static_cast<float>(std::numeric_limits<T>::lowest());
}

// The maximum value for an audio sample. If T is signed, the absolute value of
// this number is smaller (by exactly 1) than ::Min().
template <typename T>
constexpr T Max() {
  return std::numeric_limits<T>::max();
}

// The minimum value for an audio sample. If T is signed, the absolute value of
// this number is greater (by exactly 1) than ::Max()
template <typename T>
constexpr T Min() {
  return std::numeric_limits<T>::lowest();
}

template <>
constexpr float Max<float>() {
  return 1.0f;
}

template <>
constexpr float Min<float>() {
  return -1.0f;
}

// The bias value is the middle of the range. In linear PCM audio, if the
// values are all equal to the bias value, the audio is silent.
template <typename T>
constexpr T Bias() {
  return 0;
}

template <>
constexpr uint8_t Bias<uint8_t>() {
  return 128;
}

// Clip a floating point audio sample to its nominal range. This is
// destructive, and is only used here for avoiding overflow in some edge cases,
// so it's not going to be generally audible.
inline float Clip(float aValue) { return std::clamp(aValue, -1.0f, 1.0f); }

template <typename T>
T FloatToAudioSample(float aValue) {
  if constexpr (std::is_same_v<float, T>) {
    return aValue;
  }
  if constexpr (std::is_same_v<uint8_t, T>) {
    return static_cast<T>(std::clamp((aValue + 1.0f) * 128.f,
                                     LowestAsFloat<T>(), MaxAsFloat<T>()));
  } else if constexpr (std::is_same_v<int16_t, T>) {
    // This produces correct results accross the range.
    return static_cast<T>(std::clamp(aValue * -LowestAsFloat<T>(),
                                     LowestAsFloat<T>(), MaxAsFloat<T>()));
  } else if constexpr (std::is_same_v<int32_t, T>) {
    // We need to handle this differently because of rounding between INT32_MAX
    // and float 32-bits, to maximise precision.
    if (aValue >= 0.) {
      // if the input sample is greater OR EQUAL to 1.0, then clip and return
      // the max value.
      if (aValue >= 1.0) {
        return std::numeric_limits<T>::max();
      }
      // otherwise cast to a double and map to the positive range.
      // float 32-bits cannot represent int32_max (but can represent int32_min)
      constexpr double magnitudePos = std::numeric_limits<int32_t>::max();
      return static_cast<int32_t>(aValue * magnitudePos);
    }
    // Similarly for the negative range.
    if (aValue <= -1.0) {
      return std::numeric_limits<T>::lowest();
    }
    constexpr double magnitudeNegative =
        -1.0 * std::numeric_limits<int32_t>::lowest();
    return static_cast<int32_t>(aValue * magnitudeNegative);
  }
}

template <typename T>
T UInt8bitToAudioSample(uint8_t aValue) {
  if constexpr (std::is_same_v<uint8_t, T>) {
    return aValue;
  } else if constexpr (std::is_same_v<int16_t, T>) {
    return (static_cast<int16_t>(aValue) << 8) - (1 << 15);
  } else if constexpr (std::is_same_v<int32_t, T>) {
    return (static_cast<int32_t>(aValue) << 24) - (1 << 31);
  } else if constexpr (std::is_same_v<float, T>) {
    float biased = static_cast<float>(aValue) - Bias<uint8_t>();
    if (aValue >= Bias<uint8_t>()) {
      return Clip(biased / MaxAsFloat<int8_t>());
    }
    return Clip(biased / -LowestAsFloat<int8_t>());
  }
}

template <typename T>
T Int16ToAudioSample(int16_t aValue) {
  if constexpr (std::is_same_v<uint8_t, T>) {
    return static_cast<uint8_t>(aValue >> 8) + 128;
  } else if constexpr (std::is_same_v<int16_t, T>) {
    return aValue;
  } else if constexpr (std::is_same_v<int32_t, T>) {
    return aValue << 16;
  } else if constexpr (std::is_same_v<float, T>) {
    if (aValue >= 0) {
      return Clip(static_cast<float>(aValue) / MaxAsFloat<int16_t>());
    }
    return Clip(static_cast<float>(aValue) / -LowestAsFloat<int16_t>());
  }
}

// 24-bits audio samples are stored in 32-bits variables.
template <typename T>
T Int24ToAudioSample(int32_t aValue) {
  if constexpr (std::is_same_v<uint8_t, T>) {
    return static_cast<uint8_t>(aValue >> 16) + 128;
  } else if constexpr (std::is_same_v<int16_t, T>) {
    return static_cast<int16_t>(aValue >> 8);
  } else if constexpr (std::is_same_v<int32_t, T>) {
    return aValue << 8;
  } else if constexpr (std::is_same_v<float, T>) {
    const int32_t min = -(2 << 22);
    const int32_t max = (2 << 22) - 1;
    if (aValue >= 0) {
      return Clip(static_cast<float>(aValue) / static_cast<float>(max));
    }
    return Clip(static_cast<float>(aValue) / -static_cast<float>(min));
  }
}

template <typename T>
T Int32ToAudioSample(int32_t aValue) {
  if constexpr (std::is_same_v<uint8_t, T>) {
    return static_cast<uint8_t>(aValue >> 24) + 128;
  } else if constexpr (std::is_same_v<int16_t, T>) {
    return aValue >> 16;
  } else if constexpr (std::is_same_v<int32_t, T>) {
    return aValue;
  } else if constexpr (std::is_same_v<float, T>) {
    if (aValue >= 0) {
      return Clip(static_cast<float>(aValue) / MaxAsFloat<int32_t>());
    }
    return Clip(static_cast<float>(aValue) / -LowestAsFloat<int32_t>());
  }
}

// This does not handle 24-bits audio, call the function explicitly when
// needed.
template <typename D, typename S>
inline D ConvertAudioSample(const S& aSource) {
  if constexpr (std::is_same_v<S, D>) {
    return aSource;
  } else if constexpr (std::is_same_v<S, uint8_t>) {
    return UInt8bitToAudioSample<D>(aSource);
  } else if constexpr (std::is_same_v<S, int16_t>) {
    return Int16ToAudioSample<D>(aSource);
  } else if constexpr (std::is_same_v<S, int32_t>) {
    return Int32ToAudioSample<D>(aSource);
  } else if constexpr (std::is_same_v<S, float>) {
    return FloatToAudioSample<D>(aSource);
  }
}

// Sample buffer conversion
template <typename From, typename To>
inline void ConvertAudioSamples(const From* aFrom, To* aTo, int aCount) {
  if constexpr (std::is_same_v<From, To>) {
    PodCopy(aTo, aFrom, aCount);
    return;
  }
  for (int i = 0; i < aCount; ++i) {
    aTo[i] = ConvertAudioSample<To>(aFrom[i]);
  }
}

// Sample buffer conversion with scale
template <typename From, typename To>
inline void ConvertAudioSamplesWithScale(const From* aFrom, To* aTo, int aCount,
                                         float aScale) {
  if (aScale == 1.0f) {
    ConvertAudioSamples(aFrom, aTo, aCount);
    return;
  }
  for (int i = 0; i < aCount; ++i) {
    aTo[i] =
        ConvertAudioSample<To>(ConvertAudioSample<float>(aFrom[i]) * aScale);
  }
}
inline void ConvertAudioSamplesWithScale(const int16_t* aFrom, int16_t* aTo,
                                         int aCount, float aScale) {
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
    aTo[i] = FloatToAudioSample<int16_t>(ConvertAudioSample<float>(aFrom[i]) *
                                         aScale);
  }
}

template <typename From, typename To>
inline void AddAudioSamplesWithScale(const From* aFrom, To* aTo, int aCount,
                                     float aScale) {
  for (int i = 0; i < aCount; ++i) {
    aTo[i] =
        ConvertAudioSample<To>(ConvertAudioSample<float>(aTo[i]) +
                               ConvertAudioSample<float>(aFrom[i]) * aScale);
  }
}

// In place audio sample scaling.
inline void ScaleAudioSamples(float* aBuffer, int aCount, float aScale) {
  for (int32_t i = 0; i < aCount; ++i) {
    aBuffer[i] *= aScale;
  }
}

inline void ScaleAudioSamples(short* aBuffer, int aCount, float aScale) {
  int32_t volume = int32_t((1 << 16) * aScale);
  for (int32_t i = 0; i < aCount; ++i) {
    aBuffer[i] = short((int32_t(aBuffer[i]) * volume) >> 16);
  }
}

inline const void* AddAudioSampleOffset(const void* aBase,
                                        AudioSampleFormat aFormat,
                                        int32_t aOffset) {
  static_assert(AUDIO_FORMAT_S16 == 1, "Bad constant");
  static_assert(AUDIO_FORMAT_FLOAT32 == 2, "Bad constant");
  MOZ_ASSERT(aFormat == AUDIO_FORMAT_S16 || aFormat == AUDIO_FORMAT_FLOAT32);

  return static_cast<const uint8_t*>(aBase) + aFormat * 2 * aOffset;
}

}  // namespace mozilla

#endif /* MOZILLA_AUDIOSAMPLEFORMAT_H_ */
