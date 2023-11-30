/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_SYSTEMSERVICES_OBJC_VIDEO_CAPTURE_DEVICE_INFO_AVFOUNDATION_H_
#define DOM_MEDIA_SYSTEMSERVICES_OBJC_VIDEO_CAPTURE_DEVICE_INFO_AVFOUNDATION_H_

#include <map>
#include <string>

#include "api/sequence_checker.h"
#include "device_info_objc.h"
#include "modules/video_capture/device_info_impl.h"

namespace webrtc::videocapturemodule {

/**
 * DeviceInfo implementation for the libwebrtc ios/mac sdk camera backend.
 * Single threaded except for DeviceChange() that happens on a platform callback
 * thread.
 */
class DeviceInfoAvFoundation : public DeviceInfoImpl {
 public:
  static int32_t ConvertAVFrameRateToCapabilityFPS(Float64 aRate);
  static webrtc::VideoType ConvertFourCCToVideoType(FourCharCode aCode);

  DeviceInfoAvFoundation();
  virtual ~DeviceInfoAvFoundation();

  // Implementation of DeviceInfoImpl.
  int32_t Init() override { return 0; }
  void DeviceChange() override;
  uint32_t NumberOfDevices() override;
  int32_t GetDeviceName(uint32_t aDeviceNumber, char* aDeviceNameUTF8,
                        uint32_t aDeviceNameLength, char* aDeviceUniqueIdUTF8,
                        uint32_t aDeviceUniqueIdUTF8Length,
                        char* aProductUniqueIdUTF8 = nullptr,
                        uint32_t aProductUniqueIdUTF8Length = 0,
                        pid_t* aPid = nullptr,
                        bool* deviceIsPlaceholder = 0) override;
  int32_t NumberOfCapabilities(const char* aDeviceUniqueIdUTF8) override;
  int32_t GetCapability(const char* aDeviceUniqueIdUTF8,
                        const uint32_t aDeviceCapabilityNumber,
                        VideoCaptureCapability& aCapability) override;
  int32_t DisplayCaptureSettingsDialogBox(const char* aDeviceUniqueIdUTF8,
                                          const char* aDialogTitleUTF8,
                                          void* aParentWindow,
                                          uint32_t aPositionX,
                                          uint32_t aPositionY) override {
    return -1;
  }
  int32_t CreateCapabilityMap(const char* aDeviceUniqueIdUTF8) override
      RTC_EXCLUSIVE_LOCKS_REQUIRED(_apiLock);

 private:
  const std::tuple<std::string, std::string, VideoCaptureCapabilities>*
  FindDeviceAndCapabilities(const std::string& aDeviceUniqueId) const;
  void EnsureCapabilitiesMap();

  SequenceChecker mChecker;
  std::atomic<bool> mInvalidateCapabilities;
  // [{uniqueId, name, capabilities}]
  std::vector<std::tuple<std::string, std::string, VideoCaptureCapabilities>>
      mDevicesAndCapabilities RTC_GUARDED_BY(mChecker);
  const DeviceInfoIosObjC* mDeviceChangeCaptureInfo RTC_GUARDED_BY(mChecker);
};

}  // namespace webrtc::videocapturemodule

#endif
