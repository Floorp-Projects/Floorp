/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef MOZILLA_AUDIOSAMPLEFORMAT_H_
#define MOZILLA_AUDIOSAMPLEFORMAT_H_

namespace mozilla {

/**
 * Audio formats supported in MediaStreams and media elements.
 *
 * Only one of these is supported by nsAudioStream, and that is determined
 * at compile time (roughly, FLOAT32 on desktops, S16 on mobile). That format
 * is returned by nsAudioStream::Format().
 */
enum AudioSampleFormat
{
  // Native-endian signed 16-bit audio samples
  AUDIO_FORMAT_S16,
  // Signed 32-bit float samples
  AUDIO_FORMAT_FLOAT32
};

}

#endif /* MOZILLA_AUDIOSAMPLEFORMAT_H_ */
