/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_SYSTEMSERVICES_OBJC_VIDEO_CAPTURE_VIDEO_CAPTURE2_H_
#define DOM_MEDIA_SYSTEMSERVICES_OBJC_VIDEO_CAPTURE_VIDEO_CAPTURE2_H_

#import "components/capturer/RTCCameraVideoCapturer.h"

#include "api/sequence_checker.h"
#include "modules/video_capture/video_capture_impl.h"
#include "mozilla/Maybe.h"
#include "PerformanceRecorder.h"

@class VideoCaptureAdapter;

namespace webrtc::videocapturemodule {

/**
 * VideoCaptureImpl implementation of the libwebrtc ios/mac sdk camera backend.
 * Single threaded except for OnFrame() that happens on a platform callback thread.
 */
class VideoCaptureAvFoundation : public VideoCaptureImpl {
 public:
  VideoCaptureAvFoundation(AVCaptureDevice* _Nonnull aDevice);
  virtual ~VideoCaptureAvFoundation();

  static rtc::scoped_refptr<VideoCaptureModule> Create(const char* _Nullable aDeviceUniqueIdUTF8);

  // Implementation of VideoCaptureImpl. Single threaded.
  int32_t StartCapture(const VideoCaptureCapability& aCapability) MOZ_EXCLUDES(api_lock_) override;
  int32_t StopCapture() MOZ_EXCLUDES(api_lock_) override;
  bool CaptureStarted() MOZ_EXCLUDES(api_lock_) override;
  int32_t CaptureSettings(VideoCaptureCapability& aSettings) override;

  // Callback. This can be called on any thread.
  int32_t OnFrame(webrtc::VideoFrame& aFrame) MOZ_EXCLUDES(api_lock_);

  void SetTrackingId(const char* _Nonnull aTrackingId) MOZ_EXCLUDES(api_lock_) override;

  // Allows the capturer to start the recording before calling OnFrame, to cover more operations
  // under the same measurement.
  void StartFrameRecording(int32_t aWidth, int32_t aHeight) MOZ_EXCLUDES(api_lock_);

  // Registers the current thread with the profiler if not already registered.
  void MaybeRegisterCallbackThread();

 private:
  SequenceChecker mChecker;
  AVCaptureDevice* _Nonnull mDevice RTC_GUARDED_BY(mChecker);
  VideoCaptureAdapter* _Nonnull mAdapter RTC_GUARDED_BY(mChecker);
  RTC_OBJC_TYPE(RTCCameraVideoCapturer) * _Nullable mCapturer RTC_GUARDED_BY(mChecker);
  mozilla::Maybe<VideoCaptureCapability> mCapability MOZ_GUARDED_BY(api_lock_);
  mozilla::Maybe<nsCString> mTrackingId MOZ_GUARDED_BY(api_lock_);
  mozilla::PerformanceRecorderMulti<mozilla::CaptureStage> mPerformanceRecorder;
  std::atomic<ProfilerThreadId> mCallbackThreadId;
};

}  // namespace webrtc::videocapturemodule

@interface VideoCaptureAdapter : NSObject <RTC_OBJC_TYPE (RTCVideoCapturerDelegate)>
@property(nonatomic) webrtc::videocapturemodule::VideoCaptureAvFoundation* _Nullable capturer;
@end

#endif
