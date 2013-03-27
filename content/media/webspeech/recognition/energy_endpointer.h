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

// The EnergyEndpointer class finds likely speech onset and offset points.
//
// The implementation described here is about the simplest possible.
// It is based on timings of threshold crossings for overall signal
// RMS. It is suitable for light weight applications.
//
// As written, the basic idea is that one specifies intervals that
// must be occupied by super- and sub-threshold energy levels, and
// defers decisions re onset and offset times until these
// specifications have been met.  Three basic intervals are tested: an
// onset window, a speech-on window, and an offset window.  We require
// super-threshold to exceed some mimimum total durations in the onset
// and speech-on windows before declaring the speech onset time, and
// we specify a required sub-threshold residency in the offset window
// before declaring speech offset. As the various residency requirements are
// met, the EnergyEndpointer instance assumes various states, and can return the
// ID of these states to the client (see EpStatus below).
//
// The levels of the speech and background noise are continuously updated. It is
// important that the background noise level be estimated initially for
// robustness in noisy conditions. The first frames are assumed to be background
// noise and a fast update rate is used for the noise level. The duration for
// fast update is controlled by the fast_update_dur_ paramter.
//
// If used in noisy conditions, the endpointer should be started and run in the
// EnvironmentEstimation mode, for at least 200ms, before switching to
// UserInputMode.
// Audio feedback contamination can appear in the input audio, if not cut
// out or handled by echo cancellation. Audio feedback can trigger a false
// accept. The false accepts can be ignored by setting
// ep_contamination_rejection_period.

#ifndef CONTENT_BROWSER_SPEECH_ENDPOINTER_ENERGY_ENDPOINTER_H_
#define CONTENT_BROWSER_SPEECH_ENDPOINTER_ENERGY_ENDPOINTER_H_

#include <vector>

#include "nsAutoPtr.h"

#include "energy_endpointer_params.h"

namespace mozilla {

// Endpointer status codes
enum EpStatus {
  EP_PRE_SPEECH = 10,
  EP_POSSIBLE_ONSET,
  EP_SPEECH_PRESENT,
  EP_POSSIBLE_OFFSET,
  EP_POST_SPEECH,
};

class EnergyEndpointer {
 public:
  // The default construction MUST be followed by Init(), before any
  // other use can be made of the instance.
  EnergyEndpointer();
  virtual ~EnergyEndpointer();

  void Init(const EnergyEndpointerParams& params);

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

  // Computes the next input frame and modifies EnergyEndpointer status as
  // appropriate based on the computation.
  void ProcessAudioFrame(int64_t time_us,
                         const int16_t* samples, int num_samples,
                         float* rms_out);

  // Returns the current state of the EnergyEndpointer and the time
  // corresponding to the most recently computed frame.
  EpStatus Status(int64_t* status_time_us) const;

  bool estimating_environment() const {
    return estimating_environment_;
  }

  // Returns estimated noise level in dB.
  float GetNoiseLevelDb() const;

 private:
  class HistoryRing;

  // Resets the endpointer internal state.  If reset_threshold is true, the
  // state will be reset completely, including adaptive thresholds and the
  // removal of all history information.
  void Restart(bool reset_threshold);

  // Update internal speech and noise levels.
  void UpdateLevels(float rms);

  // Returns the number of frames (or frame number) corresponding to
  // the 'time' (in seconds).
  int TimeToFrame(float time) const;

  EpStatus status_;  // The current state of this instance.
  float offset_confirm_dur_sec_;  // max on time allowed to confirm POST_SPEECH
  int64_t endpointer_time_us_;  // Time of the most recently received audio frame.
  int64_t fast_update_frames_; // Number of frames for initial level adaptation.
  int64_t frame_counter_;  // Number of frames seen. Used for initial adaptation.
  float max_window_dur_;  // Largest search window size (seconds)
  float sample_rate_;  // Sampling rate.

  // Ring buffers to hold the speech activity history.
  nsAutoPtr<HistoryRing> history_;

  // Configuration parameters.
  EnergyEndpointerParams params_;

  // RMS which must be exceeded to conclude frame is speech.
  float decision_threshold_;

  // Flag to indicate that audio should be used to estimate environment, prior
  // to receiving user input.
  bool estimating_environment_;

  // Estimate of the background noise level. Used externally for UI feedback.
  float noise_level_;

  // An adaptive threshold used to update decision_threshold_ when appropriate.
  float rms_adapt_;

  // Start lag corresponds to the highest fundamental frequency.
  int start_lag_;

  // End lag corresponds to the lowest fundamental frequency.
  int end_lag_;

  // Time when mode switched from environment estimation to user input. This
  // is used to time forced rejection of audio feedback contamination.
  int64_t user_input_start_time_us_;

  // prevent copy constructor and assignment
  EnergyEndpointer(const EnergyEndpointer&);
  void operator=(const EnergyEndpointer&);
};

}  // namespace mozilla

#endif  // CONTENT_BROWSER_SPEECH_ENDPOINTER_ENERGY_ENDPOINTER_H_
