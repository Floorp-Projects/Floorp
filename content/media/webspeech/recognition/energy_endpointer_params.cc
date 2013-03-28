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

#include "energy_endpointer_params.h"

namespace mozilla {

EnergyEndpointerParams::EnergyEndpointerParams() {
  SetDefaults();
}

void EnergyEndpointerParams::SetDefaults() {
  frame_period_ = 0.01f;
  frame_duration_ = 0.01f;
  endpoint_margin_ = 0.2f;
  onset_window_ = 0.15f;
  speech_on_window_ = 0.4f;
  offset_window_ = 0.15f;
  onset_detect_dur_ = 0.09f;
  onset_confirm_dur_ = 0.075f;
  on_maintain_dur_ = 0.10f;
  offset_confirm_dur_ = 0.12f;
  decision_threshold_ = 150.0f;
  min_decision_threshold_ = 50.0f;
  fast_update_dur_ = 0.2f;
  sample_rate_ = 8000.0f;
  min_fundamental_frequency_ = 57.143f;
  max_fundamental_frequency_ = 400.0f;
  contamination_rejection_period_ = 0.25f;
}

void EnergyEndpointerParams::operator=(const EnergyEndpointerParams& source) {
  frame_period_ = source.frame_period();
  frame_duration_ = source.frame_duration();
  endpoint_margin_ = source.endpoint_margin();
  onset_window_ = source.onset_window();
  speech_on_window_ = source.speech_on_window();
  offset_window_ = source.offset_window();
  onset_detect_dur_ = source.onset_detect_dur();
  onset_confirm_dur_ = source.onset_confirm_dur();
  on_maintain_dur_ = source.on_maintain_dur();
  offset_confirm_dur_ = source.offset_confirm_dur();
  decision_threshold_ = source.decision_threshold();
  min_decision_threshold_ = source.min_decision_threshold();
  fast_update_dur_ = source.fast_update_dur();
  sample_rate_ = source.sample_rate();
  min_fundamental_frequency_ = source.min_fundamental_frequency();
  max_fundamental_frequency_ = source.max_fundamental_frequency();
  contamination_rejection_period_ = source.contamination_rejection_period();
}

}  //  namespace mozilla
