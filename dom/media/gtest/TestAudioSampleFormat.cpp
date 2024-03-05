/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 *
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSampleFormat.h"
#include "gtest/gtest.h"
#include <type_traits>

using namespace mozilla;

template <typename T>
constexpr T LowestSample() {
  if constexpr (std::is_integral_v<T>) {
    return std::numeric_limits<T>::lowest();
  } else {
    return -1.0f;
  }
}

// When converting a sample-type to another sample-type, this returns the
// maximum value possible in the destination format
template <typename Dest>
constexpr Dest HighestSample() {
  if constexpr (std::is_integral_v<Dest>) {
    return std::numeric_limits<Dest>::max();
  } else {
    return +1.0f;
  }
}

// When converting a sample-type to another sample-type, this returns the
// maximum value expected in the destination format
template <typename Dest, typename Source>
constexpr Dest HighestSampleExpected() {
  // When converting small integer samples to large integer sample, the higher
  // bound isn't reached because of positive / negative integer assymetry.
  if constexpr (std::is_same_v<Source, uint8_t> &&
                std::is_same_v<Dest, int16_t>) {
    return 32512;  // INT16_MAX - 2 << 7 + 1
  } else if constexpr (std::is_same_v<Source, uint8_t> &&
                       std::is_same_v<Dest, int32_t>) {
    return 2130706432;  // INT32_MAX - (2 << 23) + 1
  } else if constexpr (std::is_same_v<Source, int16_t> &&
                       std::is_same_v<Dest, int32_t>) {
    return 2147418112;  // INT32_MAX - UINT16_MAX
  }

  if constexpr (std::is_integral_v<Dest>) {
    return std::numeric_limits<Dest>::max();
  } else {
    return +1.0f;
  }
}

template <typename Source, typename Dest>
void TestSampleTypePair() {
  std::cout << __PRETTY_FUNCTION__ << std::endl;

  ASSERT_EQ(LowestSample<Dest>(),
            ConvertAudioSample<Dest>(LowestSample<Source>()));
  Dest expected = HighestSampleExpected<Dest, Source>();
  ASSERT_EQ(expected, ConvertAudioSample<Dest>(HighestSample<Source>()));
  ASSERT_EQ(Bias<Dest>(), ConvertAudioSample<Dest>(Bias<Source>()));
}

template <typename T>
void TestSampleType24bits() {
  std::cout << __PRETTY_FUNCTION__ << std::endl;

  int32_t max_sample_24bits = (2 << 22) - 1;
  int32_t min_sample_24bits = -(2 << 22);
  int32_t silence_24bits = 0;

  ASSERT_EQ(LowestSample<T>(), Int24ToAudioSample<T>(min_sample_24bits));
  ASSERT_EQ(Int24ToAudioSample<T>(min_sample_24bits), LowestSample<T>());
  if constexpr (std::is_same_v<T, int32_t>) {
    // Quantization issue: 2147483392 + (2<<8 - 1) == INT32_MAX
    // See comment on HighestSampleExpected above
    const int32_t HIGHEST_FROM_24BITS = 2147483392;
    ASSERT_EQ(HIGHEST_FROM_24BITS, Int24ToAudioSample<T>(max_sample_24bits));
    ASSERT_EQ(Int24ToAudioSample<T>(max_sample_24bits), HIGHEST_FROM_24BITS);
  } else {
    ASSERT_EQ(HighestSample<T>(), Int24ToAudioSample<T>(max_sample_24bits));
    ASSERT_EQ(Int24ToAudioSample<T>(max_sample_24bits), HighestSample<T>());
  }
  ASSERT_EQ(Bias<T>(), Int24ToAudioSample<T>(silence_24bits));
  ASSERT_EQ(Int24ToAudioSample<T>(silence_24bits), Bias<T>());
}

TEST(AudioSampleFormat, Boundaries)
{
  TestSampleTypePair<uint8_t, uint8_t>();
  TestSampleTypePair<uint8_t, int16_t>();
  TestSampleTypePair<uint8_t, int32_t>();
  TestSampleTypePair<uint8_t, float>();
  TestSampleTypePair<int16_t, uint8_t>();
  TestSampleTypePair<int16_t, int16_t>();
  TestSampleTypePair<int16_t, int32_t>();
  TestSampleTypePair<int16_t, float>();
  TestSampleTypePair<int32_t, uint8_t>();
  TestSampleTypePair<int32_t, int16_t>();
  TestSampleTypePair<int32_t, int32_t>();
  TestSampleTypePair<int32_t, float>();
  TestSampleTypePair<float, uint8_t>();
  TestSampleTypePair<float, int16_t>();
  TestSampleTypePair<float, int32_t>();
  TestSampleTypePair<float, float>();

  // Separately test 24-bit audio stored in 32-bits integers.
  TestSampleType24bits<uint8_t>();
  TestSampleType24bits<int16_t>();
  TestSampleType24bits<int32_t>();
  TestSampleType24bits<float>();
}
