/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_AudioInputSource_H_
#define DOM_MEDIA_AudioInputSource_H_

#include "AudioSegment.h"
#include "CubebUtils.h"

namespace mozilla {

// This is an interface to operate an input-only audio stream. Once users call
// Start(), the audio data can be read by GetAudioSegment(). GetAudioSegment()
// is not thread-safe, which means it can only be call in one specific thread.
// The audio data is periodically produced by the underlying audio stream on its
// callback thread.
class AudioInputSource {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING;

  using Id = uint32_t;

  // These functions should always be called in the same thread:
  // Starts producing audio data.
  virtual void Start() = 0;
  // Stops producing audio data.
  virtual void Stop() = 0;
  // Returns the AudioSegment with aDuration of data inside.
  virtual AudioSegment GetAudioSegment(TrackTime aDuration) = 0;

  // Any threads:
  // The unique id of this source.
  const Id mId;
  // The id of this audio device producing the data.
  const CubebUtils::AudioDeviceID mDeviceId;
  // The channel count of audio data produced.
  const uint32_t mChannelCount;
  // The sample rate of the audio data produced.
  const TrackRate mRate;
  // Indicate whether the audio stream is for voice or not.
  const bool mIsVoice;
  // The principal of the audio data produced.
  const PrincipalHandle mPrincipalHandle;

 protected:
  AudioInputSource(Id aId, CubebUtils::AudioDeviceID aDeviceId,
                   uint32_t aChannelCount, TrackRate aRate, bool aIsVoice,
                   const PrincipalHandle& aPrincipalHandle)
      : mId(aId),
        mDeviceId(aDeviceId),
        mChannelCount(aChannelCount),
        mRate(aRate),
        mIsVoice(aIsVoice),
        mPrincipalHandle(aPrincipalHandle) {}
  virtual ~AudioInputSource() = default;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_AudioInputSource_H_
