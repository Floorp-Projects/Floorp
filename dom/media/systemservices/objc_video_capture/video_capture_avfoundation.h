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
  int32_t StartCapture(const VideoCaptureCapability& aCapability) override;
  int32_t StopCapture() override;
  bool CaptureStarted() override;
  int32_t CaptureSettings(VideoCaptureCapability& aSettings) override;

  // Callback. This can be called on any thread.
  int32_t OnFrame(webrtc::VideoFrame& aFrame) MOZ_EXCLUDES(api_lock_);

 private:
  SequenceChecker mChecker;
  AVCaptureDevice* _Nonnull mDevice RTC_GUARDED_BY(mChecker);
  VideoCaptureAdapter* _Nonnull mAdapter RTC_GUARDED_BY(mChecker);
  RTC_OBJC_TYPE(RTCCameraVideoCapturer) * _Nullable mCapturer RTC_GUARDED_BY(mChecker);
  mozilla::Maybe<VideoCaptureCapability> mCapability RTC_GUARDED_BY(mChecker);
};

}  // namespace webrtc::videocapturemodule

@interface VideoCaptureAdapter : NSObject <RTC_OBJC_TYPE (RTCVideoCapturerDelegate)>
@property(nonatomic) webrtc::videocapturemodule::VideoCaptureAvFoundation* _Nullable capturer;
@end

#endif
