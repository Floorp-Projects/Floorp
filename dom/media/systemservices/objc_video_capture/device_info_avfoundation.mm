/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "device_info_avfoundation.h"
#include <CoreVideo/CVPixelBuffer.h>

#include <string>

#include "components/capturer/RTCCameraVideoCapturer.h"
#import "helpers/NSString+StdString.h"
#include "media/base/video_common.h"
#include "modules/video_capture/video_capture_defines.h"
#include "rtc_base/logging.h"

namespace webrtc::videocapturemodule {
/* static */
int32_t DeviceInfoAvFoundation::ConvertAVFrameRateToCapabilityFPS(
    Float64 aRate) {
  return static_cast<int32_t>(aRate);
}

/* static */
webrtc::VideoType DeviceInfoAvFoundation::ConvertFourCCToVideoType(
    FourCharCode aCode) {
  switch (aCode) {
    case kCVPixelFormatType_420YpCbCr8Planar:
    case kCVPixelFormatType_420YpCbCr8PlanarFullRange:
      return webrtc::VideoType::kI420;
    case kCVPixelFormatType_24BGR:
      return webrtc::VideoType::kRGB24;
    case kCVPixelFormatType_32ABGR:
      return webrtc::VideoType::kABGR;
    case kCMPixelFormat_32ARGB:
      return webrtc::VideoType::kBGRA;
    case kCMPixelFormat_32BGRA:
      return webrtc::VideoType::kARGB;
    case kCMPixelFormat_16LE565:
      return webrtc::VideoType::kRGB565;
    case kCMPixelFormat_16LE555:
    case kCMPixelFormat_16LE5551:
      return webrtc::VideoType::kARGB1555;
    case kCMPixelFormat_422YpCbCr8_yuvs:
      return webrtc::VideoType::kYUY2;
    case kCMPixelFormat_422YpCbCr8:
      return webrtc::VideoType::kUYVY;
    case kCMVideoCodecType_JPEG:
    case kCMVideoCodecType_JPEG_OpenDML:
      return webrtc::VideoType::kMJPEG;
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
      return webrtc::VideoType::kNV12;
    default:
      RTC_LOG(LS_WARNING) << "Unhandled FourCharCode" << aCode;
      return webrtc::VideoType::kUnknown;
  }
}

DeviceInfoAvFoundation::DeviceInfoAvFoundation()
    : mInvalidateCapabilities(false),
      mDeviceChangeCaptureInfo([[DeviceInfoIosObjC alloc] init]) {
  [mDeviceChangeCaptureInfo registerOwner:this];
}

DeviceInfoAvFoundation::~DeviceInfoAvFoundation() {
  [mDeviceChangeCaptureInfo registerOwner:nil];
}

void DeviceInfoAvFoundation::DeviceChange() {
  mInvalidateCapabilities = true;
  DeviceInfo::DeviceChange();
}

uint32_t DeviceInfoAvFoundation::NumberOfDevices() {
  RTC_DCHECK_RUN_ON(&mChecker);
  EnsureCapabilitiesMap();
  return mDevicesAndCapabilities.size();
}

int32_t DeviceInfoAvFoundation::GetDeviceName(
    uint32_t aDeviceNumber, char* aDeviceNameUTF8, uint32_t aDeviceNameLength,
    char* aDeviceUniqueIdUTF8, uint32_t aDeviceUniqueIdUTF8Length,
    char* /* aProductUniqueIdUTF8 */, uint32_t /* aProductUniqueIdUTF8Length */,
    pid_t* /* aPid */, bool* /*deviceIsPlaceholder*/) {
  RTC_DCHECK_RUN_ON(&mChecker);
  // Don't EnsureCapabilitiesMap() here, since:
  // 1) That might invalidate the capabilities map
  // 2) This function depends on the device index

  if (aDeviceNumber >= mDevicesAndCapabilities.size()) {
    return -1;
  }

  const auto& [uniqueId, name, _] = mDevicesAndCapabilities[aDeviceNumber];

  strncpy(aDeviceUniqueIdUTF8, uniqueId.c_str(), aDeviceUniqueIdUTF8Length);
  aDeviceUniqueIdUTF8[aDeviceUniqueIdUTF8Length - 1] = '\0';

  strncpy(aDeviceNameUTF8, name.c_str(), aDeviceNameLength);
  aDeviceNameUTF8[aDeviceNameLength - 1] = '\0';

  return 0;
}

int32_t DeviceInfoAvFoundation::NumberOfCapabilities(
    const char* aDeviceUniqueIdUTF8) {
  RTC_DCHECK_RUN_ON(&mChecker);

  std::string deviceUniqueId(aDeviceUniqueIdUTF8);
  const auto* tup = FindDeviceAndCapabilities(deviceUniqueId);
  if (!tup) {
    return 0;
  }

  const auto& [_, __, capabilities] = *tup;
  return static_cast<int32_t>(capabilities.size());
}

int32_t DeviceInfoAvFoundation::GetCapability(
    const char* aDeviceUniqueIdUTF8, const uint32_t aDeviceCapabilityNumber,
    VideoCaptureCapability& aCapability) {
  RTC_DCHECK_RUN_ON(&mChecker);

  std::string deviceUniqueId(aDeviceUniqueIdUTF8);
  const auto* tup = FindDeviceAndCapabilities(deviceUniqueId);
  if (!tup) {
    return -1;
  }

  const auto& [_, __, capabilities] = *tup;
  if (aDeviceCapabilityNumber >= capabilities.size()) {
    return -1;
  }

  aCapability = capabilities[aDeviceCapabilityNumber];
  return 0;
}

int32_t DeviceInfoAvFoundation::CreateCapabilityMap(
    const char* aDeviceUniqueIdUTF8) {
  RTC_DCHECK_RUN_ON(&mChecker);

  const size_t deviceUniqueIdUTF8Length = strlen(aDeviceUniqueIdUTF8);
  if (deviceUniqueIdUTF8Length > kVideoCaptureUniqueNameLength) {
    RTC_LOG(LS_INFO) << "Device name too long";
    return -1;
  }
  RTC_LOG(LS_INFO) << "CreateCapabilityMap called for device "
                   << aDeviceUniqueIdUTF8;
  std::string deviceUniqueId(aDeviceUniqueIdUTF8);
  const auto* tup = FindDeviceAndCapabilities(deviceUniqueId);
  if (!tup) {
    RTC_LOG(LS_INFO) << "no matching device found";
    return -1;
  }

  // Store the new used device name
  _lastUsedDeviceNameLength = deviceUniqueIdUTF8Length;
  _lastUsedDeviceName = static_cast<char*>(
      realloc(_lastUsedDeviceName, _lastUsedDeviceNameLength + 1));
  memcpy(_lastUsedDeviceName, aDeviceUniqueIdUTF8,
         _lastUsedDeviceNameLength + 1);

  const auto& [_, __, capabilities] = *tup;
  _captureCapabilities = capabilities;
  return static_cast<int32_t>(_captureCapabilities.size());
}

auto DeviceInfoAvFoundation::FindDeviceAndCapabilities(
    const std::string& aDeviceUniqueId) const
    -> const std::tuple<std::string, std::string, VideoCaptureCapabilities>* {
  RTC_DCHECK_RUN_ON(&mChecker);
  for (const auto& tup : mDevicesAndCapabilities) {
    if (std::get<0>(tup) == aDeviceUniqueId) {
      return &tup;
    }
  }
  return nullptr;
}

void DeviceInfoAvFoundation::EnsureCapabilitiesMap() {
  RTC_DCHECK_RUN_ON(&mChecker);

  if (mInvalidateCapabilities.exchange(false)) {
    mDevicesAndCapabilities.clear();
  }

  if (!mDevicesAndCapabilities.empty()) {
    return;
  }

  for (AVCaptureDevice* device in [RTCCameraVideoCapturer
           captureDevicesWithDeviceTypes:[RTCCameraVideoCapturer
                                             defaultCaptureDeviceTypes]]) {
    std::string uniqueId = [NSString stdStringForString:device.uniqueID];
    std::string name = [NSString stdStringForString:device.localizedName];
    auto& [_, __, capabilities] = mDevicesAndCapabilities.emplace_back(
        uniqueId, name, VideoCaptureCapabilities());

    for (AVCaptureDeviceFormat* format in
         [RTCCameraVideoCapturer supportedFormatsForDevice:device]) {
      VideoCaptureCapability cap;
      FourCharCode fourcc =
          CMFormatDescriptionGetMediaSubType(format.formatDescription);
      cap.videoType = ConvertFourCCToVideoType(fourcc);
      CMVideoDimensions dimensions =
          CMVideoFormatDescriptionGetDimensions(format.formatDescription);
      cap.width = dimensions.width;
      cap.height = dimensions.height;

      for (AVFrameRateRange* range in format.videoSupportedFrameRateRanges) {
        cap.maxFPS = ConvertAVFrameRateToCapabilityFPS(range.maxFrameRate);
        capabilities.push_back(cap);
      }

      if (capabilities.empty()) {
        cap.maxFPS = 30;
        capabilities.push_back(cap);
      }
    }
  }
}
}  // namespace webrtc::videocapturemodule
