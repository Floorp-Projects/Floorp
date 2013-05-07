/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebAudioUtils_h_
#define WebAudioUtils_h_

#include <cmath>
#include <limits>
#include "mozilla/TypeTraits.h"
#include "mozilla/Assertions.h"
#include "AudioParamTimeline.h"
#include "MediaSegment.h"

namespace mozilla {

class AudioNodeStream;

namespace dom {

struct WebAudioUtils {
  static bool FuzzyEqual(float v1, float v2)
  {
    using namespace std;
    return fabsf(v1 - v2) < 1e-7f;
  }
  static bool FuzzyEqual(double v1, double v2)
  {
    using namespace std;
    return fabs(v1 - v2) < 1e-7;
  }

  /**
   * Computes an exponential smoothing rate for a time based variable
   * over aDuration seconds.
   */
  static double ComputeSmoothingRate(double aDuration, double aSampleRate)
  {
    return 1.0 - std::exp(-1.0 / (aDuration * aSampleRate));
  }

  /**
   * Converts AudioParamTimeline floating point time values to tick values
   * with respect to a source and a destination AudioNodeStream.
   *
   * This needs to be called for each AudioParamTimeline that gets sent to an
   * AudioNodeEngine on the engine side where the AudioParamTimeline is
   * received.  This means that such engines need to be aware of their source
   * and destination streams as well.
   */
  static void ConvertAudioParamToTicks(AudioParamTimeline& aParam,
                                       AudioNodeStream* aSource,
                                       AudioNodeStream* aDest);

  /**
   * Converts a linear value to decibels.  Returns aMinDecibels if the linear
   * value is 0.
   */
  static float ConvertLinearToDecibels(float aLinearValue, float aMinDecibels)
  {
    return aLinearValue ? 20.0f * std::log10(aLinearValue) : aMinDecibels;
  }

  /**
   * Converts a decibel value to a linear value.
   */
  static float ConvertDecibelsToLinear(float aDecibels)
  {
    return std::pow(10.0f, 0.05f * aDecibels);
  }

  /**
   * Converts a decibel to a linear value.
   */
  static float ConvertDecibelToLinear(float aDecibel)
  {
    return std::pow(10.0f, 0.05f * aDecibel);
  }

  static void FixNaN(double& aDouble)
  {
    if (MOZ_DOUBLE_IS_NaN(aDouble) || MOZ_DOUBLE_IS_INFINITE(aDouble)) {
      aDouble = 0.0;
    }
  }

  static double DiscreteTimeConstantForSampleRate(double timeConstant, double sampleRate)
  {
    return 1.0 - std::exp(-1.0 / (sampleRate * timeConstant));
  }

  /**
   * Convert a stream position into the time coordinate of the destination
   * stream.
   */
  static double StreamPositionToDestinationTime(TrackTicks aSourcePosition,
                                                AudioNodeStream* aSource,
                                                AudioNodeStream* aDestination);

  /**
   * Converts a floating point value to an integral type in a safe and
   * platform agnostic way.  The following program demonstrates the kinds
   * of ways things can go wrong depending on the CPU architecture you're
   * compiling for:
   *
   * #include <stdio.h>
   * volatile float r;
   * int main()
   * {
   *   unsigned int q;
   *   r = 1e100;
   *   q = r;
   *   printf("%f %d\n", r, q);
   *   r = -1e100;
   *   q = r;
   *   printf("%f %d\n", r, q);
   *   r = 1e15;
   *   q = r;
   *   printf("%f %x\n", r, q);
   *   r = 0/0.;
   *   q = r;
   *   printf("%f %d\n", r, q);
   * }
   *
   * This program, when compiled for unsigned int, generates the following
   * results depending on the architecture:
   *
   * x86 and x86-64
   * ---
   *  inf 0
   *  -inf 0
   *  999999995904.000000 -727384064 d4a50000
   *  nan 0
   *
   * ARM
   * ---
   *  inf -1
   *  -inf 0
   *  999999995904.000000 -1
   *  nan 0
   *
   * When compiled for int, this program generates the following results:
   *
   * x86 and x86-64
   * ---
   *  inf -2147483648
   *  -inf -2147483648
   *  999999995904.000000 -2147483648
   *  nan -2147483648
   *
   * ARM
   * ---
   *  inf 2147483647
   *  -inf -2147483648
   *  999999995904.000000 2147483647
   *  nan 0
   *
   * Note that the caller is responsible to make sure that the value
   * passed to this function is not a NaN.  This function will abort if
   * it sees a NaN.
   */
  template <typename IntType, typename FloatType>
  static IntType TruncateFloatToInt(FloatType f)
  {
    using namespace std;

    MOZ_STATIC_ASSERT((mozilla::IsIntegral<IntType>::value == true),
                      "IntType must be an integral type");
    MOZ_STATIC_ASSERT((mozilla::IsFloatingPoint<FloatType>::value == true),
                      "FloatType must be a floating point type");

    if (f != f) {
      // It is the responsibility of the caller to deal with NaN values.
      // If we ever get to this point, we have a serious bug to fix.
      NS_RUNTIMEABORT("We should never see a NaN here");
    }

    if (f > FloatType(numeric_limits<IntType>::max())) {
      // If the floating point value is outside of the range of maximum
      // integral value for this type, just clamp to the maximum value.
      return numeric_limits<IntType>::max();
    }

    if (f < FloatType(numeric_limits<IntType>::min())) {
      // If the floating point value is outside of the range of minimum
      // integral value for this type, just clamp to the minimum value.
      return numeric_limits<IntType>::min();
    }

    // Otherwise, this conversion must be well defined.
    return IntType(f);
  }
};

}
}

#endif

