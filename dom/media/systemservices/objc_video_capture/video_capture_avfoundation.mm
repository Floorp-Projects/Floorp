/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "video_capture_avfoundation.h"

#import "api/video_frame_buffer/RTCNativeI420Buffer+Private.h"
#import "base/RTCI420Buffer.h"
#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/capturer/RTCCameraVideoCapturer.h"
#import "helpers/NSString+StdString.h"

#include "api/scoped_refptr.h"
#include "api/video/video_rotation.h"
#include "device_info_avfoundation.h"
#include "modules/video_capture/video_capture_defines.h"
#include "mozilla/Assertions.h"
#include "rtc_base/time_utils.h"

using namespace mozilla;
using namespace webrtc::videocapturemodule;

namespace {
webrtc::VideoRotation ToNativeRotation(RTCVideoRotation aRotation) {
  switch (aRotation) {
    case RTCVideoRotation_0:
      return webrtc::kVideoRotation_0;
    case RTCVideoRotation_90:
      return webrtc::kVideoRotation_90;
    case RTCVideoRotation_180:
      return webrtc::kVideoRotation_180;
    case RTCVideoRotation_270:
      return webrtc::kVideoRotation_270;
    default:
      MOZ_CRASH_UNSAFE_PRINTF("Unexpected rotation %d", static_cast<int>(aRotation));
      return webrtc::kVideoRotation_0;
  }
}

AVCaptureDeviceFormat* _Nullable FindFormat(AVCaptureDevice* _Nonnull aDevice,
                                            webrtc::VideoCaptureCapability aCapability) {
  for (AVCaptureDeviceFormat* format in [aDevice formats]) {
    CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
    if (dimensions.width != aCapability.width) {
      continue;
    }
    if (dimensions.height != aCapability.height) {
      continue;
    }
    FourCharCode fourcc = CMFormatDescriptionGetMediaSubType(format.formatDescription);
    if (aCapability.videoType != DeviceInfoAvFoundation::ConvertFourCCToVideoType(fourcc)) {
      continue;
    }
    if ([format.videoSupportedFrameRateRanges
            indexOfObjectPassingTest:^BOOL(AVFrameRateRange* _Nonnull obj, NSUInteger idx,
                                           BOOL* _Nonnull stop) {
              return static_cast<BOOL>(DeviceInfoAvFoundation::ConvertAVFrameRateToCapabilityFPS(
                                           obj.maxFrameRate) == aCapability.maxFPS);
            }] == NSNotFound) {
      continue;
    }

    return format;
  }
  return nullptr;
}
}  // namespace

@implementation VideoCaptureAdapter
@synthesize capturer = _capturer;

- (void)capturer:(RTC_OBJC_TYPE(RTCVideoCapturer) * _Nonnull)capturer
    didCaptureVideoFrame:(RTC_OBJC_TYPE(RTCVideoFrame) * _Nonnull)frame {
  const int64_t timestamp_us = frame.timeStampNs / rtc::kNumNanosecsPerMicrosec;
  RTC_OBJC_TYPE(RTCI420Buffer)* buffer = [[frame buffer] toI420];
  // Accessing the (intended-to-be-private) native buffer directly is hacky but lets us skip two
  // copies
  rtc::scoped_refptr<webrtc::I420BufferInterface> nativeBuffer = [buffer nativeI420Buffer];
  webrtc::VideoFrame nativeFrame = webrtc::VideoFrame::Builder()
                                       .set_video_frame_buffer(nativeBuffer)
                                       .set_rotation(ToNativeRotation(frame.rotation))
                                       .set_timestamp_us(timestamp_us)
                                       .build();
  _capturer->OnFrame(nativeFrame);
}

@end

namespace webrtc::videocapturemodule {
VideoCaptureAvFoundation::VideoCaptureAvFoundation(AVCaptureDevice* _Nonnull aDevice)
    : mDevice(aDevice), mAdapter([[VideoCaptureAdapter alloc] init]), mCapturer(nullptr) {
  {
    const char* uniqueId = [[aDevice uniqueID] UTF8String];
    size_t len = strlen(uniqueId);
    _deviceUniqueId = new (std::nothrow) char[len + 1];
    if (_deviceUniqueId) {
      memcpy(_deviceUniqueId, uniqueId, len + 1);
    }
  }

  mAdapter.capturer = this;
  mCapturer = [[RTC_OBJC_TYPE(RTCCameraVideoCapturer) alloc] initWithDelegate:mAdapter];
}

VideoCaptureAvFoundation::~VideoCaptureAvFoundation() = default;

/* static */
rtc::scoped_refptr<VideoCaptureModule> VideoCaptureAvFoundation::Create(
    const char* _Nullable aDeviceUniqueIdUTF8) {
  std::string uniqueId(aDeviceUniqueIdUTF8);
  for (AVCaptureDevice* device in [RTCCameraVideoCapturer captureDevices]) {
    if ([NSString stdStringForString:device.uniqueID] == uniqueId) {
      rtc::scoped_refptr<VideoCaptureModule> module(
          new rtc::RefCountedObject<VideoCaptureAvFoundation>(device));
      return module;
    }
  }
  return nullptr;
}

int32_t VideoCaptureAvFoundation::StartCapture(const VideoCaptureCapability& aCapability) {
  RTC_DCHECK_RUN_ON(&mChecker);
  if (AVCaptureDeviceFormat* format = FindFormat(mDevice, aCapability)) {
    mCapability = Some(aCapability);
    [mCapturer startCaptureWithDevice:mDevice
                               format:format
                                  fps:aCapability.maxFPS
                    completionHandler:nullptr];
    return 0;
  }
  return -1;
}

int32_t VideoCaptureAvFoundation::StopCapture() {
  RTC_DCHECK_RUN_ON(&mChecker);
  mCapability = Nothing();
  [mCapturer stopCapture];
  return 0;
}

bool VideoCaptureAvFoundation::CaptureStarted() {
  RTC_DCHECK_RUN_ON(&mChecker);
  return mCapability.isSome();
}

int32_t VideoCaptureAvFoundation::CaptureSettings(VideoCaptureCapability& aSettings) {
  MOZ_CRASH("Unexpected call");
  return -1;
}

int32_t VideoCaptureAvFoundation::OnFrame(webrtc::VideoFrame& aFrame) {
  MutexLock lock(&api_lock_);
  return DeliverCapturedFrame(aFrame);
}
}  // namespace webrtc::videocapturemodule
