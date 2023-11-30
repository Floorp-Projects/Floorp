/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_SYSTEMSERVICES_VIDEO_ENGINE_PLACEHOLDER_DEVICE_INFO_H_
#define DOM_MEDIA_SYSTEMSERVICES_VIDEO_ENGINE_PLACEHOLDER_DEVICE_INFO_H_

#include "modules/video_capture/device_info_impl.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_impl.h"

namespace mozilla {

class PlaceholderDeviceInfo
    : public webrtc::videocapturemodule::DeviceInfoImpl {
 public:
  explicit PlaceholderDeviceInfo(bool aCameraPresent);
  ~PlaceholderDeviceInfo() override;

  uint32_t NumberOfDevices() override;
  int32_t GetDeviceName(uint32_t aDeviceNumber, char* aDeviceNameUTF8,
                        uint32_t aDeviceNameLength, char* aDeviceUniqueIdUTF8,
                        uint32_t aDeviceUniqueIdUTF8Length,
                        char* aProductUniqueIdUTF8 = nullptr,
                        uint32_t aProductUniqueIdUTF8Length = 0,
                        pid_t* aPid = nullptr,
                        bool* aDeviceIsPlaceholder = nullptr) override;

  int32_t CreateCapabilityMap(const char* aDeviceUniqueIdUTF8) override;
  int32_t DisplayCaptureSettingsDialogBox(const char* aDeviceUniqueIdUTF8,
                                          const char* aDialogTitleUTF8,
                                          void* aParentWindow,
                                          uint32_t aPositionX,
                                          uint32_t aPositionY) override;
  int32_t Init() override;

 private:
  const bool mCameraPresent;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_SYSTEMSERVICES_VIDEO_ENGINE_PLACEHOLDER_DEVICE_INFO_H_
