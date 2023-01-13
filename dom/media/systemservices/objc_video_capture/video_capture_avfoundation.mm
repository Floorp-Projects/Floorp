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
#include "CallbackThreadRegistry.h"
#include "device_info_avfoundation.h"
#include "modules/video_capture/video_capture_defines.h"
#include "mozilla/Assertions.h"
#include "mozilla/Monitor.h"
#include "mozilla/UniquePtr.h"
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
  _capturer->StartFrameRecording(frame.width, frame.height);
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
    : mDevice(aDevice),
      mAdapter([[VideoCaptureAdapter alloc] init]),
      mCapturer(nullptr),
      mCallbackThreadId() {
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

VideoCaptureAvFoundation::~VideoCaptureAvFoundation() {
  // Must block until capture has fully stopped, including async operations.
  StopCapture();
}

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
  AVCaptureDeviceFormat* format = FindFormat(mDevice, aCapability);
  if (!format) {
    return -1;
  }

  {
    MutexLock lock(&api_lock_);
    if (mCapability) {
      if (mCapability->width == aCapability.width && mCapability->height == aCapability.height &&
          mCapability->maxFPS == aCapability.maxFPS &&
          mCapability->videoType == aCapability.videoType) {
        return 0;
      }

      api_lock_.Unlock();
      int32_t rv = StopCapture();
      api_lock_.Lock();

      if (rv != 0) {
        return rv;
      }
    }
  }

  Monitor monitor("VideoCaptureAVFoundation::StartCapture");
  Monitor* copyableMonitor = &monitor;
  MonitorAutoLock lock(monitor);
  __block Maybe<int32_t> rv;

  [mCapturer startCaptureWithDevice:mDevice
                             format:format
                                fps:aCapability.maxFPS
                  completionHandler:^(NSError* error) {
                    MOZ_RELEASE_ASSERT(!rv);
                    rv = Some(error ? -1 : 0);
                    copyableMonitor->Notify();
                  }];

  while (!rv) {
    monitor.Wait();
  }

  if (*rv == 0) {
    MutexLock lock(&api_lock_);
    mCapability = Some(aCapability);
  }

  return *rv;
}

int32_t VideoCaptureAvFoundation::StopCapture() {
  RTC_DCHECK_RUN_ON(&mChecker);
  {
    MutexLock lock(&api_lock_);
    if (!mCapability) {
      return 0;
    }
    mCapability = Nothing();
  }

  Monitor monitor("VideoCaptureAVFoundation::StopCapture");
  Monitor* copyableMonitor = &monitor;
  MonitorAutoLock lock(monitor);
  __block bool done = false;

  [mCapturer stopCaptureWithCompletionHandler:^(void) {
    MOZ_RELEASE_ASSERT(!done);
    done = true;
    copyableMonitor->Notify();
  }];

  while (!done) {
    monitor.Wait();
  }
  return 0;
}

bool VideoCaptureAvFoundation::CaptureStarted() {
  RTC_DCHECK_RUN_ON(&mChecker);
  MutexLock lock(&api_lock_);
  return mCapability.isSome();
}

int32_t VideoCaptureAvFoundation::CaptureSettings(VideoCaptureCapability& aSettings) {
  MOZ_CRASH("Unexpected call");
  return -1;
}

int32_t VideoCaptureAvFoundation::OnFrame(webrtc::VideoFrame& aFrame) {
  MutexLock lock(&api_lock_);
  int32_t rv = DeliverCapturedFrame(aFrame);
  mPerformanceRecorder.Record(0);
  return rv;
}

void VideoCaptureAvFoundation::SetTrackingId(const char* _Nonnull aTrackingId) {
  RTC_DCHECK_RUN_ON(&mChecker);
  MutexLock lock(&api_lock_);
  if (NS_WARN_IF(mTrackingId.isSome())) {
    // This capture instance must be shared across multiple camera requests. For now ignore other
    // requests than the first.
    return;
  }
  mTrackingId = Some(nsCString(aTrackingId));
}

void VideoCaptureAvFoundation::StartFrameRecording(int32_t aWidth, int32_t aHeight) {
  MaybeRegisterCallbackThread();
  MutexLock lock(&api_lock_);
  if (MOZ_UNLIKELY(!mTrackingId)) {
    return;
  }
  auto fromWebrtcVideoType = [](webrtc::VideoType aType) -> CaptureStage::ImageType {
    switch (aType) {
      case webrtc::VideoType::kI420:
        return CaptureStage::ImageType::I420;
      case webrtc::VideoType::kYUY2:
        return CaptureStage::ImageType::YUY2;
      case webrtc::VideoType::kYV12:
        return CaptureStage::ImageType::YV12;
      case webrtc::VideoType::kUYVY:
        return CaptureStage::ImageType::UYVY;
      case webrtc::VideoType::kNV12:
        return CaptureStage::ImageType::NV12;
      case webrtc::VideoType::kNV21:
        return CaptureStage::ImageType::NV21;
      case webrtc::VideoType::kMJPEG:
        return CaptureStage::ImageType::MJPEG;
      default:
        return CaptureStage::ImageType::Unknown;
    }
  };
  mPerformanceRecorder.Start(
      0, "VideoCaptureAVFoundation"_ns, *mTrackingId, aWidth, aHeight,
      mCapability.map([&](const auto& aCap) { return fromWebrtcVideoType(aCap.videoType); })
          .valueOr(CaptureStage::ImageType::Unknown));
}

void VideoCaptureAvFoundation::MaybeRegisterCallbackThread() {
  ProfilerThreadId id = profiler_current_thread_id();
  if (MOZ_LIKELY(id == mCallbackThreadId)) {
    return;
  }
  mCallbackThreadId = id;
  CallbackThreadRegistry::Get()->Register(mCallbackThreadId, "VideoCaptureAVFoundationCallback");
}
}  // namespace webrtc::videocapturemodule
