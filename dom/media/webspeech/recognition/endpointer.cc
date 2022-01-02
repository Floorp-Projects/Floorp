// Copyright (c) 2013 The Chromium Authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "endpointer.h"

#include "AudioSegment.h"

namespace {
const int kFrameRate = 200;  // 1 frame = 5ms of audio.
}

namespace mozilla {

Endpointer::Endpointer(int sample_rate)
    : speech_input_possibly_complete_silence_length_us_(-1),
      speech_input_complete_silence_length_us_(-1),
      audio_frame_time_us_(0),
      sample_rate_(sample_rate),
      frame_size_(0) {
  Reset();

  frame_size_ = static_cast<int>(sample_rate / static_cast<float>(kFrameRate));

  speech_input_minimum_length_us_ =
      static_cast<int64_t>(1.7 * 1000000);
  speech_input_complete_silence_length_us_ =
      static_cast<int64_t>(0.5 * 1000000);
  long_speech_input_complete_silence_length_us_ = -1;
  long_speech_length_us_ = -1;
  speech_input_possibly_complete_silence_length_us_ =
      1 * 1000000;

  // Set the default configuration for Push To Talk mode.
  EnergyEndpointerParams ep_config;
  ep_config.set_frame_period(1.0f / static_cast<float>(kFrameRate));
  ep_config.set_frame_duration(1.0f / static_cast<float>(kFrameRate));
  ep_config.set_endpoint_margin(0.2f);
  ep_config.set_onset_window(0.15f);
  ep_config.set_speech_on_window(0.4f);
  ep_config.set_offset_window(0.15f);
  ep_config.set_onset_detect_dur(0.09f);
  ep_config.set_onset_confirm_dur(0.075f);
  ep_config.set_on_maintain_dur(0.10f);
  ep_config.set_offset_confirm_dur(0.12f);
  ep_config.set_decision_threshold(1000.0f);
  ep_config.set_min_decision_threshold(50.0f);
  ep_config.set_fast_update_dur(0.2f);
  ep_config.set_sample_rate(static_cast<float>(sample_rate));
  ep_config.set_min_fundamental_frequency(57.143f);
  ep_config.set_max_fundamental_frequency(400.0f);
  ep_config.set_contamination_rejection_period(0.25f);
  energy_endpointer_.Init(ep_config);
}

void Endpointer::Reset() {
  old_ep_status_ = EP_PRE_SPEECH;
  waiting_for_speech_possibly_complete_timeout_ = false;
  waiting_for_speech_complete_timeout_ = false;
  speech_previously_detected_ = false;
  speech_input_complete_ = false;
  audio_frame_time_us_ = 0; // Reset time for packets sent to endpointer.
  speech_end_time_us_ = -1;
  speech_start_time_us_ = -1;
}

void Endpointer::StartSession() {
  Reset();
  energy_endpointer_.StartSession();
}

void Endpointer::EndSession() {
  energy_endpointer_.EndSession();
}

void Endpointer::SetEnvironmentEstimationMode() {
  Reset();
  energy_endpointer_.SetEnvironmentEstimationMode();
}

void Endpointer::SetUserInputMode() {
  energy_endpointer_.SetUserInputMode();
}

EpStatus Endpointer::Status(int64_t *time) {
  return energy_endpointer_.Status(time);
}

EpStatus Endpointer::ProcessAudio(const AudioChunk& raw_audio, float* rms_out) {
  MOZ_ASSERT(raw_audio.mBufferFormat == AUDIO_FORMAT_S16, "Audio is not in 16 bit format");
  const int16_t* audio_data = static_cast<const int16_t*>(raw_audio.mChannelData[0]);
  const int num_samples = raw_audio.mDuration;
  EpStatus ep_status = EP_PRE_SPEECH;

  // Process the input data in blocks of frame_size_, dropping any incomplete
  // frames at the end (which is ok since typically the caller will be recording
  // audio in multiples of our frame size).
  int sample_index = 0;
  while (sample_index + frame_size_ <= num_samples) {
    // Have the endpointer process the frame.
    energy_endpointer_.ProcessAudioFrame(audio_frame_time_us_,
                                         audio_data + sample_index,
                                         frame_size_,
                                         rms_out);
    sample_index += frame_size_;
    audio_frame_time_us_ += (frame_size_ * 1000000) /
                         sample_rate_;

    // Get the status of the endpointer.
    int64_t ep_time;
    ep_status = energy_endpointer_.Status(&ep_time);
    if (old_ep_status_ != ep_status)
        fprintf(stderr, "Status changed old= %d, new= %d\n", old_ep_status_, ep_status);

    // Handle state changes.
    if ((EP_SPEECH_PRESENT == ep_status) &&
        (EP_POSSIBLE_ONSET == old_ep_status_)) {
      speech_end_time_us_ = -1;
      waiting_for_speech_possibly_complete_timeout_ = false;
      waiting_for_speech_complete_timeout_ = false;
      // Trigger SpeechInputDidStart event on first detection.
      if (false == speech_previously_detected_) {
        speech_previously_detected_ = true;
        speech_start_time_us_ = ep_time;
      }
    }
    if ((EP_PRE_SPEECH == ep_status) &&
        (EP_POSSIBLE_OFFSET == old_ep_status_)) {
      speech_end_time_us_ = ep_time;
      waiting_for_speech_possibly_complete_timeout_ = true;
      waiting_for_speech_complete_timeout_ = true;
    }
    if (ep_time > speech_input_minimum_length_us_) {
      // Speech possibly complete timeout.
      if ((waiting_for_speech_possibly_complete_timeout_) &&
          (ep_time - speech_end_time_us_ >
              speech_input_possibly_complete_silence_length_us_)) {
        waiting_for_speech_possibly_complete_timeout_ = false;
      }
      if (waiting_for_speech_complete_timeout_) {
        // The length of the silence timeout period can be held constant, or it
        // can be changed after a fixed amount of time from the beginning of
        // speech.
        bool has_stepped_silence =
            (long_speech_length_us_ > 0) &&
            (long_speech_input_complete_silence_length_us_ > 0);
        int64_t requested_silence_length;
        if (has_stepped_silence &&
            (ep_time - speech_start_time_us_) > long_speech_length_us_) {
          requested_silence_length =
              long_speech_input_complete_silence_length_us_;
        } else {
          requested_silence_length =
              speech_input_complete_silence_length_us_;
        }

        // Speech complete timeout.
        if ((ep_time - speech_end_time_us_) > requested_silence_length) {
          waiting_for_speech_complete_timeout_ = false;
          speech_input_complete_ = true;
        }
      }
    }
    old_ep_status_ = ep_status;
  }
  return ep_status;
}

}  // namespace mozilla
