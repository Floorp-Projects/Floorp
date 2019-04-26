/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CamerasTypes_h
#define mozilla_CamerasTypes_h

#include "ipc/IPCMessageUtils.h"

namespace mozilla {

namespace camera {

enum CaptureEngine : int {
  InvalidEngine = 0,
  ScreenEngine,
  BrowserEngine,
  WinEngine,
  CameraEngine,
  MaxEngine
};

}  // namespace camera
}  // namespace mozilla

namespace IPC {
template <>
struct ParamTraits<mozilla::camera::CaptureEngine>
    : public ContiguousEnumSerializer<
          mozilla::camera::CaptureEngine,
          mozilla::camera::CaptureEngine::InvalidEngine,
          mozilla::camera::CaptureEngine::MaxEngine> {};
}  // namespace IPC

#endif  // mozilla_CamerasTypes_h
