/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CamerasTypes_h
#define mozilla_CamerasTypes_h

#include "ipc/EnumSerializer.h"
#include "PerformanceRecorder.h"

namespace mozilla::camera {

enum CaptureEngine : int {
  InvalidEngine = 0,
  ScreenEngine,
  BrowserEngine,
  WinEngine,
  CameraEngine,
  MaxEngine
};

enum class CamerasAccessStatus {
  // We have full access to cameras, either because it was granted, or because
  // requesting it from the user was not necessary.
  Granted = 1,
  // A permission request to the platform is required before we know the
  // camera access status. Enumeration will result in a single placeholder
  // device, should any cameras be present on the system. The placeholder
  // device cannot be captured.
  RequestRequired,
  // A permission request was made and was rejected by the platform.
  Rejected,
  // Generic error while doing the request, for instance with pipewire most
  // likely the xdg-desktop-portal request failed.
  Error,
};

TrackingId::Source CaptureEngineToTrackingSourceStr(
    const CaptureEngine& aEngine);

}  // namespace mozilla::camera

namespace IPC {
template <>
struct ParamTraits<mozilla::camera::CaptureEngine>
    : public ContiguousEnumSerializer<
          mozilla::camera::CaptureEngine,
          mozilla::camera::CaptureEngine::InvalidEngine,
          mozilla::camera::CaptureEngine::MaxEngine> {};

template <>
struct ParamTraits<mozilla::camera::CamerasAccessStatus>
    : public ContiguousEnumSerializer<
          mozilla::camera::CamerasAccessStatus,
          mozilla::camera::CamerasAccessStatus::Granted,
          mozilla::camera::CamerasAccessStatus::Error> {};
}  // namespace IPC

#endif  // mozilla_CamerasTypes_h
