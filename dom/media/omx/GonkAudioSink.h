/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
 * Copyright (C) 2007 The Android Open Source Project
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

#ifndef GONK_AUDIO_SINK_H_
#define GONK_AUDIO_SINK_H_

#include <utils/Errors.h>
#include <utils/String8.h>
#include <system/audio.h>

#define DEFAULT_AUDIOSINK_BUFFERCOUNT 4
#define DEFAULT_AUDIOSINK_BUFFERSIZE 1200
#define DEFAULT_AUDIOSINK_SAMPLERATE 44100

// when the channel mask isn't known, use the channel count to derive a mask in
// AudioSink::open()
#define CHANNEL_MASK_USE_CHANNEL_ORDER 0

namespace mozilla {

/**
 * AudioSink: abstraction layer for audio output
 * Stripped version of Android KK MediaPlayerBase::AudioSink class
 */

class GonkAudioSink : public android::RefBase
{
  typedef android::String8 String8;
  typedef android::status_t status_t;

public:
  enum cb_event_t {
    CB_EVENT_FILL_BUFFER,   // Request to write more data to buffer.
    CB_EVENT_STREAM_END,    // Sent after all the buffers queued in AF and HW
                            // are played back (after stop is called)
    CB_EVENT_TEAR_DOWN      // The AudioTrack was invalidated due to usecase
                            // change. Need to re-evaluate offloading options
  };

  // Callback returns the number of bytes actually written to the buffer.
  typedef size_t (*AudioCallback)(GonkAudioSink* aAudioSink,
                                  void* aBuffer,
                                  size_t aSize,
                                  void* aCookie,
                                  cb_event_t aEvent);
  virtual ~GonkAudioSink() {}
  virtual ssize_t FrameSize() const = 0;
  virtual status_t GetPosition(uint32_t* aPosition) const = 0;
  virtual status_t SetVolume(float aVolume) const = 0;
  virtual status_t SetParameters(const String8& aKeyValuePairs)
  {
    return android::NO_ERROR;
  }

  virtual status_t Open(uint32_t aSampleRate,
                        int aChannelCount,
                        audio_channel_mask_t aChannelMask,
                        audio_format_t aFormat=AUDIO_FORMAT_PCM_16_BIT,
                        AudioCallback aCb = nullptr,
                        void* aCookie = nullptr,
                        audio_output_flags_t aFlags = AUDIO_OUTPUT_FLAG_NONE,
                        const audio_offload_info_t* aOffloadInfo = nullptr) = 0;

  virtual status_t Start() = 0;
  virtual void Stop() = 0;
  virtual void Flush() = 0;
  virtual void Pause() = 0;
  virtual void Close() = 0;
};

} // namespace mozilla

#endif // GONK_AUDIO_SINK_H_
