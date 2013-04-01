/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WebAudioUtils_h_
#define WebAudioUtils_h_

#include <cmath>
#include "AudioParamTimeline.h"

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
   * Converts a decibel to a linear value.
   */
  static float ConvertDecibelToLinear(float aDecibel)
  {
    return std::pow(10.0f, 0.05f * aDecibel);
  }
};

}
}

#endif

