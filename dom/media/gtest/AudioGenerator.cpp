/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "AudioGenerator.h"

#include "AudioSegment.h"

using namespace mozilla;

AudioGenerator::AudioGenerator(int32_t aChannels, int32_t aSampleRate)
    : mGenerator(aSampleRate, 1000), mChannels(aChannels) {}

void AudioGenerator::Generate(AudioSegment& aSegment, const int32_t& aSamples) {
  RefPtr<SharedBuffer> buffer =
      SharedBuffer::Create(aSamples * sizeof(int16_t));
  int16_t* dest = static_cast<int16_t*>(buffer->Data());
  mGenerator.generate(dest, aSamples);
  AutoTArray<const int16_t*, 1> channels;
  for (int32_t i = 0; i < mChannels; i++) {
    channels.AppendElement(dest);
  }
  aSegment.AppendFrames(buffer.forget(), channels, aSamples,
                        PRINCIPAL_HANDLE_NONE);
}
