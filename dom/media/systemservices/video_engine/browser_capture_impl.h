/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBRTC_MODULES_BROWSER_CAPTURE_MAIN_SOURCE_BROWSER_CAPTURE_IMPL_H_
#define WEBRTC_MODULES_BROWSER_CAPTURE_MAIN_SOURCE_BROWSER_CAPTURE_IMPL_H_

#include "webrtc/modules/video_capture/video_capture.h"

using namespace webrtc::videocapturemodule;

namespace webrtc {

class BrowserDeviceInfoImpl : public VideoCaptureModule::DeviceInfo {
 public:
  virtual uint32_t NumberOfDevices() { return 1; }

  virtual int32_t Refresh() { return 0; }

  virtual int32_t GetDeviceName(uint32_t deviceNumber, char* deviceNameUTF8,
                                uint32_t deviceNameSize,
                                char* deviceUniqueIdUTF8,
                                uint32_t deviceUniqueIdUTF8Size,
                                char* productUniqueIdUTF8 = NULL,
                                uint32_t productUniqueIdUTF8Size = 0,
                                pid_t* pid = 0) {
    deviceNameUTF8 = const_cast<char*>(kDeviceName);
    deviceUniqueIdUTF8 = const_cast<char*>(kUniqueDeviceName);
    productUniqueIdUTF8 = const_cast<char*>(kProductUniqueId);
    return 1;
  };

  virtual int32_t NumberOfCapabilities(const char* deviceUniqueIdUTF8) {
    return 0;
  }

  virtual int32_t GetCapability(const char* deviceUniqueIdUTF8,
                                const uint32_t deviceCapabilityNumber,
                                VideoCaptureCapability& capability) {
    return 0;
  };

  virtual int32_t GetOrientation(const char* deviceUniqueIdUTF8,
                                 VideoRotation& orientation) {
    return 0;
  }

  virtual int32_t GetBestMatchedCapability(
      const char* deviceUniqueIdUTF8, const VideoCaptureCapability& requested,
      VideoCaptureCapability& resulting) {
    return 0;
  }

  virtual int32_t DisplayCaptureSettingsDialogBox(
      const char* deviceUniqueIdUTF8, const char* dialogTitleUTF8,
      void* parentWindow, uint32_t positionX, uint32_t positionY) {
    return 0;
  }

  BrowserDeviceInfoImpl()
      : kDeviceName("browser"),
        kUniqueDeviceName("browser"),
        kProductUniqueId("browser") {}

  static BrowserDeviceInfoImpl* CreateDeviceInfo() {
    return new BrowserDeviceInfoImpl();
  }
  virtual ~BrowserDeviceInfoImpl() {}

 private:
  const char* kDeviceName;
  const char* kUniqueDeviceName;
  const char* kProductUniqueId;
};
}  // namespace webrtc
#endif
