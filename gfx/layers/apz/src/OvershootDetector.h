/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_OvershootDetector_h_
#define mozilla_layers_OvershootDetector_h_

#include "mozilla/gfx/Types.h"  // for Side
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {

class ScrollWheelInput;

namespace layers {

/*
 * Handles detection and telemetry reporting of overshoot events, which are
 * situations where the user changes scrolling direction shortly after a
 * previous scroll in the other direction, indicating that the user likely
 * scrolled too far and had to backtrack.
 *
 * Currently this data collection is focused on forms of input that trigger
 * animated rather than instant scrolling. That includes ScrollWheelInput
 * (currently implemented) and KeyboardInput (planned in bug 1838568).
 */
class OvershootDetector {
 public:
  OvershootDetector() = default;

  // Updates the internal state machine with a new incoming scrollwheel input.
  void Update(const ScrollWheelInput& aInput);

 private:
  TimeStamp mLastTimeStamp;
  Maybe<mozilla::Side> mLastDirection;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_OvershootDetector_h_
