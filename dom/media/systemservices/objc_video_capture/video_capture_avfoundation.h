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
#include "MediaEventSource.h"
#include "modules/video_capture/video_capture_impl.h"
#include "mozilla/Maybe.h"
#include "mozilla/StateWatching.h"
#include "PerformanceRecorder.h"

@interface VideoCaptureAdapter : NSObject <RTCVideoCapturerDelegate> {
 @public
  mozilla::MediaEventProducerExc<__strong RTCVideoFrame* _Nullable> frameEvent;
}
@end

namespace webrtc::videocapturemodule {

/**
 * VideoCaptureImpl implementation of the libwebrtc ios/mac sdk camera backend.
 * Single threaded API. Callbacks run on the API thread.
 *
 * Internally callbacks come on a dedicated dispatch queue. We bounce them via a MediaEvent to the
 * API thread.
 */
class VideoCaptureAvFoundation : public VideoCaptureImpl {
 public:
  VideoCaptureAvFoundation(AVCaptureDevice* _Nonnull aDevice);
  virtual ~VideoCaptureAvFoundation();

  static rtc::scoped_refptr<VideoCaptureModule> Create(const char* _Nullable aDeviceUniqueIdUTF8);

  // Implementation of VideoCaptureImpl. Single threaded.

  // Starts capturing synchronously. Idempotent. If an existing capture is live and another
  // capability is requested we'll restart the underlying backend with the new capability.
  int32_t StartCapture(const VideoCaptureCapability& aCapability) override;
  // Stops capturing synchronously. Idempotent.
  int32_t StopCapture() override;
  bool CaptureStarted() override;
  int32_t CaptureSettings(VideoCaptureCapability& aSettings) override;

  // Callback on the mChecker thread.
  void OnFrame(__strong RTCVideoFrame* _Nonnull aFrame);

  void SetTrackingId(uint32_t aTrackingIdProcId) override;

 private:
  // Convert mNextFrameToProcess and pass it to callbacks.
  void ProcessNextFrame();

  // Control thread checker.
  SequenceChecker mChecker;
  const RefPtr<mozilla::AbstractThread> mCallbackThread;
  AVCaptureDevice* _Nonnull const mDevice RTC_GUARDED_BY(mChecker);
  VideoCaptureAdapter* _Nonnull const mAdapter RTC_GUARDED_BY(mChecker);
  RTCCameraVideoCapturer* _Nonnull const mCapturer RTC_GUARDED_BY(mChecker);
  mozilla::WatchManager<VideoCaptureAvFoundation> mWatchManager RTC_GUARDED_BY(mChecker);
  // The next frame to be processed. If processing is slow this is always the most recent frame.
  mozilla::Watchable<__strong RTCVideoFrame* _Nullable> mNextFrameToProcess
      RTC_GUARDED_BY(mChecker);
  // If capture has started, this is the capability it was started for.
  mozilla::Maybe<VideoCaptureCapability> mCapability RTC_GUARDED_BY(mChecker);
  // The image type that mCapability maps to. Set in lockstep with mCapability.
  mozilla::Maybe<mozilla::CaptureStage::ImageType> mImageType RTC_GUARDED_BY(mChecker);
  // Id string uniquely identifying this capture source.
  mozilla::Maybe<mozilla::TrackingId> mTrackingId RTC_GUARDED_BY(mChecker);
  // Handle for the mAdapter frame event. mChecker thread only.
  mozilla::MediaEventListener mFrameListener RTC_GUARDED_BY(mChecker);
  // Adds frame specific markers to the profiler while mTrackingId is set.
  mozilla::PerformanceRecorderMulti<mozilla::CaptureStage> mCaptureRecorder
      RTC_GUARDED_BY(mChecker);
  mozilla::PerformanceRecorderMulti<mozilla::CopyVideoStage> mConversionRecorder
      RTC_GUARDED_BY(mChecker);
};

}  // namespace webrtc::videocapturemodule

#endif
