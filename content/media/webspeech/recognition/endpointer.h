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

#ifndef CONTENT_BROWSER_SPEECH_ENDPOINTER_ENDPOINTER_H_
#define CONTENT_BROWSER_SPEECH_ENDPOINTER_ENDPOINTER_H_

#include "energy_endpointer.h"

namespace mozilla {

struct AudioChunk;

// A simple interface to the underlying energy-endpointer implementation, this
// class lets callers provide audio as being recorded and let them poll to find
// when the user has stopped speaking.
//
// There are two events that may trigger the end of speech:
//
// speechInputPossiblyComplete event:
//
// Signals that silence/noise has  been detected for a *short* amount of
// time after some speech has been detected. It can be used for low latency
// UI feedback. To disable it, set it to a large amount.
//
// speechInputComplete event:
//
// This event is intended to signal end of input and to stop recording.
// The amount of time to wait after speech is set by
// speech_input_complete_silence_length_ and optionally two other
// parameters (see below).
// This time can be held constant, or can change as more speech is detected.
// In the latter case, the time changes after a set amount of time from the
// *beginning* of speech.  This is motivated by the expectation that there
// will be two distinct types of inputs: short search queries and longer
// dictation style input.
//
// Three parameters are used to define the piecewise constant timeout function.
// The timeout length is speech_input_complete_silence_length until
// long_speech_length, when it changes to
// long_speech_input_complete_silence_length.
class Endpointer {
 public:
  explicit Endpointer(int sample_rate);

  // Start the endpointer. This should be called at the beginning of a session.
  void StartSession();

  // Stop the endpointer.
  void EndSession();

  // Start environment estimation. Audio will be used for environment estimation
  // i.e. noise level estimation.
  void SetEnvironmentEstimationMode();

  // Start user input. This should be called when the user indicates start of
  // input, e.g. by pressing a button.
  void SetUserInputMode();

  // Process a segment of audio, which may be more than one frame.
  // The status of the last frame will be returned.
  EpStatus ProcessAudio(const AudioChunk& raw_audio, float* rms_out);

  // Get the status of the endpointer.
  EpStatus Status(int64_t *time_us);

  // Get the expected frame size for audio chunks. Audio chunks are expected
  // to contain a number of samples that is a multiple of this number, and extra
  // samples will be dropped.
  int32_t FrameSize() const {
    return frame_size_;
  }

  // Returns true if the endpointer detected reasonable audio levels above
  // background noise which could be user speech, false if not.
  bool DidStartReceivingSpeech() const {
    return speech_previously_detected_;
  }

  bool IsEstimatingEnvironment() const {
    return energy_endpointer_.estimating_environment();
  }

  void set_speech_input_complete_silence_length(int64_t time_us) {
    speech_input_complete_silence_length_us_ = time_us;
  }

  void set_long_speech_input_complete_silence_length(int64_t time_us) {
    long_speech_input_complete_silence_length_us_ = time_us;
  }

  void set_speech_input_possibly_complete_silence_length(int64_t time_us) {
    speech_input_possibly_complete_silence_length_us_ = time_us;
  }

  void set_long_speech_length(int64_t time_us) {
    long_speech_length_us_ = time_us;
  }

  bool speech_input_complete() const {
    return speech_input_complete_;
  }

  // RMS background noise level in dB.
  float NoiseLevelDb() const { return energy_endpointer_.GetNoiseLevelDb(); }

 private:
  // Reset internal states. Helper method common to initial input utterance
  // and following input utternaces.
  void Reset();

  // Minimum allowable length of speech input.
  int64_t speech_input_minimum_length_us_;

  // The speechInputPossiblyComplete event signals that silence/noise has been
  // detected for a *short* amount of time after some speech has been detected.
  // This proporty specifies the time period.
  int64_t speech_input_possibly_complete_silence_length_us_;

  // The speechInputComplete event signals that silence/noise has been
  // detected for a *long* amount of time after some speech has been detected.
  // This property specifies the time period.
  int64_t speech_input_complete_silence_length_us_;

  // Same as above, this specifies the required silence period after speech
  // detection. This period is used instead of
  // speech_input_complete_silence_length_ when the utterance is longer than
  // long_speech_length_. This parameter is optional.
  int64_t long_speech_input_complete_silence_length_us_;

  // The period of time after which the endpointer should consider
  // long_speech_input_complete_silence_length_ as a valid silence period
  // instead of speech_input_complete_silence_length_. This parameter is
  // optional.
  int64_t long_speech_length_us_;

  // First speech onset time, used in determination of speech complete timeout.
  int64_t speech_start_time_us_;

  // Most recent end time, used in determination of speech complete timeout.
  int64_t speech_end_time_us_;

  int64_t audio_frame_time_us_;
  EpStatus old_ep_status_;
  bool waiting_for_speech_possibly_complete_timeout_;
  bool waiting_for_speech_complete_timeout_;
  bool speech_previously_detected_;
  bool speech_input_complete_;
  EnergyEndpointer energy_endpointer_;
  int sample_rate_;
  int32_t frame_size_;
};

}  // namespace mozilla

#endif  // CONTENT_BROWSER_SPEECH_ENDPOINTER_ENDPOINTER_H_
