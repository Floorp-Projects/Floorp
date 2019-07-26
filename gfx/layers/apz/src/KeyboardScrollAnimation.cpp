/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KeyboardScrollAnimation.h"
#include "ScrollAnimationBezierPhysics.h"
#include "mozilla/StaticPrefs_general.h"

namespace mozilla {
namespace layers {

static ScrollAnimationBezierPhysicsSettings SettingsForType(
    KeyboardScrollAction::KeyboardScrollActionType aType) {
  int32_t minMS = 0;
  int32_t maxMS = 0;

  switch (aType) {
    case KeyboardScrollAction::eScrollCharacter:
    case KeyboardScrollAction::eScrollLine: {
      maxMS = clamped(StaticPrefs::general_smoothScroll_lines_durationMaxMS(),
                      0, 10000);
      minMS = clamped(StaticPrefs::general_smoothScroll_lines_durationMinMS(),
                      0, maxMS);
      break;
    }
    case KeyboardScrollAction::eScrollPage: {
      maxMS = clamped(StaticPrefs::general_smoothScroll_pages_durationMaxMS(),
                      0, 10000);
      minMS = clamped(StaticPrefs::general_smoothScroll_pages_durationMinMS(),
                      0, maxMS);
      break;
    }
    case KeyboardScrollAction::eScrollComplete: {
      maxMS = clamped(StaticPrefs::general_smoothScroll_other_durationMaxMS(),
                      0, 10000);
      minMS = clamped(StaticPrefs::general_smoothScroll_other_durationMinMS(),
                      0, maxMS);
      break;
    }
  }

  // The pref is 100-based int percentage, while mIntervalRatio is 1-based ratio
  double intervalRatio =
      ((double)StaticPrefs::general_smoothScroll_durationToIntervalRatio()) /
      100.0;
  intervalRatio = std::max(1.0, intervalRatio);
  return ScrollAnimationBezierPhysicsSettings{minMS, maxMS, intervalRatio};
}

KeyboardScrollAnimation::KeyboardScrollAnimation(
    AsyncPanZoomController& aApzc, const nsPoint& aInitialPosition,
    KeyboardScrollAction::KeyboardScrollActionType aType)
    : GenericScrollAnimation(aApzc, aInitialPosition, SettingsForType(aType)) {}

}  // namespace layers
}  // namespace mozilla
