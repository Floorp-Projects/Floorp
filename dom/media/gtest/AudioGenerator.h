/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_GTEST_AUDIO_GENERATOR_H_
#define DOM_MEDIA_GTEST_AUDIO_GENERATOR_H_

#include "prtime.h"
#include "SineWaveGenerator.h"

namespace mozilla {
class AudioSegment;
}

class AudioGenerator {
 public:
  AudioGenerator(int32_t aChannels, int32_t aSampleRate);
  void Generate(mozilla::AudioSegment& aSegment, const int32_t& aSamples);

 private:
  mozilla::SineWaveGenerator mGenerator;
  const int32_t mChannels;
};

#endif  // DOM_MEDIA_GTEST_AUDIO_GENERATOR_H_
