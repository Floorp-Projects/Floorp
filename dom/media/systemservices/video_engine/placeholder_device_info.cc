/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "placeholder_device_info.h"
#include "modules/video_capture/video_capture_factory.h"

namespace mozilla {

PlaceholderDeviceInfo::PlaceholderDeviceInfo(bool aCameraPresent)
    : mCameraPresent(aCameraPresent) {}

PlaceholderDeviceInfo::~PlaceholderDeviceInfo() = default;

uint32_t PlaceholderDeviceInfo::NumberOfDevices() { return mCameraPresent; }

int32_t PlaceholderDeviceInfo::Init() { return 0; }

int32_t PlaceholderDeviceInfo::GetDeviceName(
    uint32_t aDeviceNumber, char* aDeviceNameUTF8, uint32_t aDeviceNameLength,
    char* aDeviceUniqueIdUTF8, uint32_t aDeviceUniqueIdUTF8Length,
    char* aProductUniqueIdUTF8, uint32_t aProductUniqueIdUTF8Length,
    pid_t* aPid, bool* aDeviceIsPlaceholder) {
  // Check whether there is camera device reported by the Camera portal
  // When the promise is resolved, it means there is a camera available
  // but we have to use a placeholder device.
  if (!mCameraPresent) {
    return -1;
  }

  // Making these empty to follow the specs for non-legacy enumeration:
  // https://w3c.github.io/mediacapture-main/#access-control-model
  memset(aDeviceNameUTF8, 0, aDeviceNameLength);
  memset(aDeviceUniqueIdUTF8, 0, aDeviceUniqueIdUTF8Length);

  if (aProductUniqueIdUTF8) {
    memset(aProductUniqueIdUTF8, 0, aProductUniqueIdUTF8Length);
  }

  if (aDeviceIsPlaceholder) {
    *aDeviceIsPlaceholder = true;
  }

  return 0;
}

int32_t PlaceholderDeviceInfo::CreateCapabilityMap(
    const char* /*aDeviceUniqueIdUTF8*/) {
  return -1;
}

int32_t PlaceholderDeviceInfo::DisplayCaptureSettingsDialogBox(
    const char* /*deviceUniqueIdUTF8*/, const char* /*dialogTitleUTF8*/,
    void* /*parentWindow*/, uint32_t /*positionX*/, uint32_t /*positionY*/) {
  return -1;
}

}  // namespace mozilla
