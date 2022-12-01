/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CamerasTypes.h"

namespace mozilla::camera {

TrackingId::Source CaptureEngineToTrackingSourceStr(
    const CaptureEngine& aEngine) {
  switch (aEngine) {
    case ScreenEngine:
      return TrackingId::Source::Screen;
    case BrowserEngine:
      return TrackingId::Source::Tab;
    case WinEngine:
      return TrackingId::Source::Window;
    case CameraEngine:
      return TrackingId::Source::Camera;
    default:
      return TrackingId::Source::Unimplemented;
  }
}
}  // namespace mozilla::camera
