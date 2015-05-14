/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AUDIO_OFFLOAD_PLAYER_BASE_H_
#define AUDIO_OFFLOAD_PLAYER_BASE_H_

#include "MediaDecoder.h"
#include "MediaDecoderOwner.h"

namespace mozilla {

/**
 * AudioOffloadPlayer interface class which has funtions used by MediaOmxDecoder
 * This is to reduce the dependency of AudioOffloadPlayer in MediaOmxDecoder
 */
class AudioOffloadPlayerBase
{
  typedef android::status_t status_t;
  typedef android::MediaSource MediaSource;

public:
  virtual ~AudioOffloadPlayerBase() {};

  // Caller retains ownership of "aSource".
  virtual void SetSource(const android::sp<MediaSource> &aSource) {}

  // Start the source if it's not already started and open the AudioSink to
  // create an offloaded audio track
  virtual status_t Start(bool aSourceAlreadyStarted = false)
  {
    return android::NO_INIT;
  }

  virtual status_t ChangeState(MediaDecoder::PlayState aState)
  {
    return android::NO_INIT;
  }

  virtual void SetVolume(double aVolume) {}

  virtual int64_t GetMediaTimeUs() { return 0; }

  // To update progress bar when the element is visible
  virtual void SetElementVisibility(bool aIsVisible) {}

  // Update ready state based on current play state. Not checking data
  // availability since offloading is currently done only when whole compressed
  // data is available
  virtual MediaDecoderOwner::NextFrameStatus GetNextFrameStatus()
  {
    return MediaDecoderOwner::NEXT_FRAME_UNAVAILABLE;
  }

  virtual nsRefPtr<MediaDecoder::SeekPromise> Seek(SeekTarget aTarget) = 0;
};

} // namespace mozilla

#endif // AUDIO_OFFLOAD_PLAYER_BASE_H_
