/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_SYSTEMSERVICES_OBJC_VIDEO_CAPTURE_VIDEO_CAPTURE2_H_
#define DOM_MEDIA_SYSTEMSERVICES_OBJC_VIDEO_CAPTURE_VIDEO_CAPTURE2_H_

#import "components/capturer/RTCCameraVideoCapturer.h"

#include "api/scoped_refptr.h"
#include "api/sequence_checker.h"
#include "modules/video_capture/video_capture_impl.h"
#include "mozilla/Maybe.h"
#include "PerformanceRecorder.h"

@class VideoCaptureAdapter;

namespace webrtc::videocapturemodule {

/**
 * VideoCaptureImpl implementation of the libwebrtc ios/mac sdk camera backend.
 * Single threaded except for OnFrame() that happens on a platform callback
 * thread.
 */
class VideoCaptureAvFoundation : public VideoCaptureImpl {
 public:
  VideoCaptureAvFoundation(AVCaptureDevice* _Nonnull aDevice);
  virtual ~VideoCaptureAvFoundation();

  static rtc::scoped_refptr<VideoCaptureModule> Create(
      const char* _Nullable aDeviceUniqueIdUTF8);

  // Implementation of VideoCaptureImpl. Single threaded.

  // Starts capturing synchronously. Idempotent. If an existing capture is live
  // and another capability is requested we'll restart the underlying backend
  // with the new capability.
  int32_t StartCapture(const VideoCaptureCapability& aCapability)
      MOZ_EXCLUDES(api_lock_) override;
  // Stops capturing synchronously. Idempotent.
  int32_t StopCapture() MOZ_EXCLUDES(api_lock_) override;
  bool CaptureStarted() MOZ_EXCLUDES(api_lock_) override;
  int32_t CaptureSettings(VideoCaptureCapability& aSettings) override;

  // Callback. This can be called on any thread.
  int32_t OnFrame(__strong RTCVideoFrame* _Nonnull aFrame)
      MOZ_EXCLUDES(api_lock_);

  void SetTrackingId(uint32_t aTrackingIdProcId)
      MOZ_EXCLUDES(api_lock_) override;

  // Registers the current thread with the profiler if not already registered.
  void MaybeRegisterCallbackThread();

 private:
  // Control thread checker.
  SequenceChecker mChecker;
  AVCaptureDevice* _Nonnull const mDevice RTC_GUARDED_BY(mChecker);
  VideoCaptureAdapter* _Nonnull const mAdapter RTC_GUARDED_BY(mChecker);
  RTCCameraVideoCapturer* _Nonnull const mCapturer RTC_GUARDED_BY(mChecker);
  // If capture has started, this is the capability it was started for. Written
  // on the mChecker thread only.
  mozilla::Maybe<VideoCaptureCapability> mCapability MOZ_GUARDED_BY(api_lock_);
  // The image type that mCapability maps to. Set in lockstep with mCapability.
  mozilla::Maybe<mozilla::CaptureStage::ImageType> mImageType
      MOZ_GUARDED_BY(api_lock_);
  // Id string uniquely identifying this capture source. Written on the mChecker
  // thread only.
  mozilla::Maybe<mozilla::TrackingId> mTrackingId MOZ_GUARDED_BY(api_lock_);
  // Adds frame specific markers to the profiler while mTrackingId is set.
  // Callback thread only.
  mozilla::PerformanceRecorderMulti<mozilla::CaptureStage> mCaptureRecorder;
  mozilla::PerformanceRecorderMulti<mozilla::CopyVideoStage>
      mConversionRecorder;
  std::atomic<ProfilerThreadId> mCallbackThreadId;
};

}  // namespace webrtc::videocapturemodule

@interface VideoCaptureAdapter : NSObject <RTCVideoCapturerDelegate> {
  webrtc::Mutex _mutex;
  webrtc::videocapturemodule::VideoCaptureAvFoundation* _Nullable _capturer
      RTC_GUARDED_BY(_mutex);
}
- (void)setCapturer:
    (webrtc::videocapturemodule::VideoCaptureAvFoundation* _Nullable)capturer;
@end

#endif
