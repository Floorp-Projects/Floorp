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

#ifndef CONTENT_BROWSER_SPEECH_ENDPOINTER_ENERGY_ENDPOINTER_PARAMS_H_
#define CONTENT_BROWSER_SPEECH_ENDPOINTER_ENERGY_ENDPOINTER_PARAMS_H_

namespace mozilla {

// Input parameters for the EnergyEndpointer class.
class EnergyEndpointerParams {
 public:
  EnergyEndpointerParams();

  void SetDefaults();

  void operator=(const EnergyEndpointerParams& source);

  // Accessors and mutators
  float frame_period() const { return frame_period_; }
  void set_frame_period(float frame_period) {
    frame_period_ = frame_period;
  }

  float frame_duration() const { return frame_duration_; }
  void set_frame_duration(float frame_duration) {
    frame_duration_ = frame_duration;
  }

  float endpoint_margin() const { return endpoint_margin_; }
  void set_endpoint_margin(float endpoint_margin) {
    endpoint_margin_ = endpoint_margin;
  }

  float onset_window() const { return onset_window_; }
  void set_onset_window(float onset_window) { onset_window_ = onset_window; }

  float speech_on_window() const { return speech_on_window_; }
  void set_speech_on_window(float speech_on_window) {
    speech_on_window_ = speech_on_window;
  }

  float offset_window() const { return offset_window_; }
  void set_offset_window(float offset_window) {
    offset_window_ = offset_window;
  }

  float onset_detect_dur() const { return onset_detect_dur_; }
  void set_onset_detect_dur(float onset_detect_dur) {
    onset_detect_dur_ = onset_detect_dur;
  }

  float onset_confirm_dur() const { return onset_confirm_dur_; }
  void set_onset_confirm_dur(float onset_confirm_dur) {
    onset_confirm_dur_ = onset_confirm_dur;
  }

  float on_maintain_dur() const { return on_maintain_dur_; }
  void set_on_maintain_dur(float on_maintain_dur) {
    on_maintain_dur_ = on_maintain_dur;
  }

  float offset_confirm_dur() const { return offset_confirm_dur_; }
  void set_offset_confirm_dur(float offset_confirm_dur) {
    offset_confirm_dur_ = offset_confirm_dur;
  }

  float decision_threshold() const { return decision_threshold_; }
  void set_decision_threshold(float decision_threshold) {
    decision_threshold_ = decision_threshold;
  }

  float min_decision_threshold() const { return min_decision_threshold_; }
  void set_min_decision_threshold(float min_decision_threshold) {
    min_decision_threshold_ = min_decision_threshold;
  }

  float fast_update_dur() const { return fast_update_dur_; }
  void set_fast_update_dur(float fast_update_dur) {
    fast_update_dur_ = fast_update_dur;
  }

  float sample_rate() const { return sample_rate_; }
  void set_sample_rate(float sample_rate) { sample_rate_ = sample_rate; }

  float min_fundamental_frequency() const { return min_fundamental_frequency_; }
  void set_min_fundamental_frequency(float min_fundamental_frequency) {
    min_fundamental_frequency_ = min_fundamental_frequency;
  }

  float max_fundamental_frequency() const { return max_fundamental_frequency_; }
  void set_max_fundamental_frequency(float max_fundamental_frequency) {
    max_fundamental_frequency_ = max_fundamental_frequency;
  }

  float contamination_rejection_period() const {
    return contamination_rejection_period_;
  }
  void set_contamination_rejection_period(
      float contamination_rejection_period) {
    contamination_rejection_period_ = contamination_rejection_period;
  }

 private:
  float frame_period_;  // Frame period
  float frame_duration_;  // Window size
  float onset_window_;  // Interval scanned for onset activity
  float speech_on_window_;  // Inverval scanned for ongoing speech
  float offset_window_;  // Interval scanned for offset evidence
  float offset_confirm_dur_;  // Silence duration required to confirm offset
  float decision_threshold_;  // Initial rms detection threshold
  float min_decision_threshold_;  // Minimum rms detection threshold
  float fast_update_dur_;  // Period for initial estimation of levels.
  float sample_rate_;  // Expected sample rate.

  // Time to add on either side of endpoint threshold crossings
  float endpoint_margin_;
  // Total dur within onset_window required to enter ONSET state
  float onset_detect_dur_;
  // Total on time within onset_window required to enter SPEECH_ON state
  float onset_confirm_dur_;
  // Minimum dur in SPEECH_ON state required to maintain ON state
  float on_maintain_dur_;
  // Minimum fundamental frequency for autocorrelation.
  float min_fundamental_frequency_;
  // Maximum fundamental frequency for autocorrelation.
  float max_fundamental_frequency_;
  // Period after start of user input that above threshold values are ignored.
  // This is to reject audio feedback contamination.
  float contamination_rejection_period_;
};

}  //  namespace mozilla

#endif  // CONTENT_BROWSER_SPEECH_ENDPOINTER_ENERGY_ENDPOINTER_PARAMS_H_
