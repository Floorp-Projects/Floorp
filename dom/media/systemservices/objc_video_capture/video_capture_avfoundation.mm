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
#include "mozilla/TaskQueue.h"
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
- (void)capturer:(RTCVideoCapturer* _Nonnull)capturer
    didCaptureVideoFrame:(RTCVideoFrame* _Nonnull)frame {
  frameEvent.Notify(frame);
}
@end

namespace webrtc::videocapturemodule {
VideoCaptureAvFoundation::VideoCaptureAvFoundation(AVCaptureDevice* _Nonnull aDevice)
    : mCallbackThread(TaskQueue::Create(do_AddRef(GetCurrentSerialEventTarget()),
                                        "VideoCaptureAvFoundation::mCallbackThread")),
      mDevice(aDevice),
      mAdapter([[VideoCaptureAdapter alloc] init]),
      mCapturer([[RTC_OBJC_TYPE(RTCCameraVideoCapturer) alloc] initWithDelegate:mAdapter]),
      mWatchManager(this, mCallbackThread),
      mNextFrameToProcess(nil, "VideoCaptureAvFoundation::mNextFrameToProcess") {
  mWatchManager.Watch(mNextFrameToProcess, &VideoCaptureAvFoundation::ProcessNextFrame);
  const char* uniqueId = [[aDevice uniqueID] UTF8String];
  size_t len = strlen(uniqueId);
  _deviceUniqueId = new (std::nothrow) char[len + 1];
  if (_deviceUniqueId) {
    memcpy(_deviceUniqueId, uniqueId, len + 1);
  }
}

VideoCaptureAvFoundation::~VideoCaptureAvFoundation() {
  // Must block until capture has fully stopped, including async operations.
  MOZ_RELEASE_ASSERT(mOkToDestroy);
  mWatchManager.Shutdown();
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

  mFrameListener =
      mAdapter->frameEvent.Connect(mCallbackThread, this, &VideoCaptureAvFoundation::OnFrame);

  {
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

    if (*rv != 0) {
      return *rv;
    }
  }

  MutexLock lock(&api_lock_);
  mCapability = Some(aCapability);
  mImageType = Some([type = aCapability.videoType] {
    switch (type) {
      case webrtc::VideoType::kI420:
        return CaptureStage::ImageType::I420;
      case webrtc::VideoType::kYUY2:
        return CaptureStage::ImageType::YUY2;
      case webrtc::VideoType::kYV12:
      case webrtc::VideoType::kIYUV:
        return CaptureStage::ImageType::YV12;
      case webrtc::VideoType::kUYVY:
        return CaptureStage::ImageType::UYVY;
      case webrtc::VideoType::kNV12:
        return CaptureStage::ImageType::NV12;
      case webrtc::VideoType::kNV21:
        return CaptureStage::ImageType::NV21;
      case webrtc::VideoType::kMJPEG:
        return CaptureStage::ImageType::MJPEG;
      case webrtc::VideoType::kRGB24:
      case webrtc::VideoType::kABGR:
      case webrtc::VideoType::kARGB:
      case webrtc::VideoType::kARGB4444:
      case webrtc::VideoType::kRGB565:
      case webrtc::VideoType::kARGB1555:
      case webrtc::VideoType::kBGRA:
      case webrtc::VideoType::kUnknown:
        // Unlikely, and not represented by CaptureStage::ImageType.
        return CaptureStage::ImageType::Unknown;
    }
    return CaptureStage::ImageType::Unknown;
  }());

  return 0;
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

  mFrameListener.Disconnect();

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

void VideoCaptureAvFoundation::OnFrame(__strong RTCVideoFrame* _Nonnull aFrame) {
  RTC_DCHECK_RUN_ON(&mChecker);
  if (MOZ_LIKELY(mTrackingId)) {
    const int64_t timestamp_us = aFrame.timeStampNs / rtc::kNumNanosecsPerMicrosec;
    mCaptureRecorder.Start(timestamp_us, "VideoCaptureAVFoundation"_ns, *mTrackingId, aFrame.width,
                           aFrame.height, mImageType.valueOr(CaptureStage::ImageType::Unknown));
  }

  if (mNextFrameToProcess) {
    // Skipping a frame -- make sure it still shows up in the profiler.
    mCaptureRecorder.Record(mNextFrameToProcess.Ref().timeStampNs / rtc::kNumNanosecsPerMicrosec);
  }

  mNextFrameToProcess = aFrame;
}

void VideoCaptureAvFoundation::ProcessNextFrame() {
  RTC_DCHECK_RUN_ON(&mChecker);
  if (!mNextFrameToProcess) {
    return;
  }

  RTCVideoFrame* frame = mNextFrameToProcess;
  mNextFrameToProcess = nil;

  const int64_t timestamp_us = frame.timeStampNs / rtc::kNumNanosecsPerMicrosec;
  if (mTrackingId && mImageType && *mImageType != CaptureStage::ImageType::I420) {
    mConversionRecorder.Start(timestamp_us, "VideoCaptureAVFoundation"_ns, *mTrackingId,
                              frame.width, frame.height);
  }
  RTCI420Buffer* buffer = [frame.buffer toI420];
  mConversionRecorder.Record(timestamp_us);
  // Accessing the (intended-to-be-private) native buffer directly is hacky but lets us skip two
  // copies
  rtc::scoped_refptr<webrtc::I420BufferInterface> nativeBuffer = buffer.nativeI420Buffer;
  auto videoFrame = webrtc::VideoFrame::Builder()
                        .set_video_frame_buffer(nativeBuffer)
                        .set_rotation(ToNativeRotation(frame.rotation))
                        .set_timestamp_us(timestamp_us)
                        .build();

  MutexLock lock(&api_lock_);
  DeliverCapturedFrame(videoFrame);
  mCaptureRecorder.Record(timestamp_us);
}

void VideoCaptureAvFoundation::SetTrackingId(uint32_t aTrackingIdProcId) {
  RTC_DCHECK_RUN_ON(&mChecker);
  if (NS_WARN_IF(mTrackingId.isSome())) {
    // This capture instance must be shared across multiple camera requests. For now ignore other
    // requests than the first.
    return;
  }
  mTrackingId.emplace(TrackingId::Source::Camera, aTrackingIdProcId);
}
}  // namespace webrtc::videocapturemodule
