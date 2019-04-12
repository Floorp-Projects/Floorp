/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WindowOrientationObserver.h"

#include "nsGlobalWindow.h"
#include "mozilla/Hal.h"

using namespace mozilla::dom;

/**
 * This class is used by nsGlobalWindowInner to implement window.orientation
 * and window.onorientationchange. This class is defined in its own file
 * because Hal.h pulls in windows.h and can't be included from
 * nsGlobalWindow.cpp
 */
WindowOrientationObserver::WindowOrientationObserver(
    nsGlobalWindowInner* aGlobalWindow)
    : mWindow(aGlobalWindow) {
  MOZ_ASSERT(aGlobalWindow);
  hal::RegisterScreenConfigurationObserver(this);

  hal::ScreenConfiguration config;
  hal::GetCurrentScreenConfiguration(&config);
  mAngle = config.angle();
}

WindowOrientationObserver::~WindowOrientationObserver() {
  hal::UnregisterScreenConfigurationObserver(this);
}

void WindowOrientationObserver::Notify(
    const mozilla::hal::ScreenConfiguration& aConfiguration) {
  uint16_t currentAngle = aConfiguration.angle();
  if (mAngle != currentAngle && mWindow->IsCurrentInnerWindow()) {
    mAngle = currentAngle;
    mWindow->GetOuterWindow()->DispatchCustomEvent(
        NS_LITERAL_STRING("orientationchange"));
  }
}

/* static */
int16_t WindowOrientationObserver::OrientationAngle() {
  hal::ScreenConfiguration config;
  hal::GetCurrentScreenConfiguration(&config);
  int16_t angle = static_cast<int16_t>(config.angle());
  // config.angle() returns 0, 90, 180 or 270.
  // window.orientation returns -90, 0, 90 or 180.
  return angle <= 180 ? angle : angle - 360;
}
