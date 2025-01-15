/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_MAIN_SOURCE_DESKTOP_CAPTURE_IMPL_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_MAIN_SOURCE_DESKTOP_CAPTURE_IMPL_H_

/*
 * video_capture_impl.h
 */

#include <memory>
#include <set>
#include <string>

#include "MediaEventSource.h"
#include "PerformanceRecorder.h"
#include "api/sequence_checker.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/video_capture/video_capture.h"
#include "mozilla/DataMutex.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"

class nsIThread;
class nsITimer;

namespace mozilla::camera {
enum class CaptureDeviceType;
}

namespace webrtc {

class VideoCaptureEncodeInterface;

// Reuses the video engine pipeline for screen sharing.
// As with video, DesktopCaptureImpl is a proxy for screen sharing
// and follows the video pipeline design
class DesktopCaptureImpl : public DesktopCapturer::Callback,
                           public VideoCaptureModule {
 public:
  /* Create a screen capture modules object
   */
  static DesktopCaptureImpl* Create(
      const int32_t aModuleId, const char* aUniqueId,
      const mozilla::camera::CaptureDeviceType aType);

  [[nodiscard]] static std::shared_ptr<VideoCaptureModule::DeviceInfo>
  CreateDeviceInfo(const int32_t aId,
                   const mozilla::camera::CaptureDeviceType aType);

  // mControlThread only.
  void RegisterCaptureDataCallback(
      rtc::VideoSinkInterface<VideoFrame>* aCallback) override;
  void RegisterCaptureDataCallback(
      RawVideoSinkInterface* dataCallback) override {}
  void DeRegisterCaptureDataCallback(
      rtc::VideoSinkInterface<VideoFrame>* aCallback) override;
  int32_t StopCaptureIfAllClientsClose() override;

  int32_t SetCaptureRotation(VideoRotation aRotation) override;
  bool SetApplyRotation(bool aEnable) override;
  bool GetApplyRotation() override { return true; }

  const char* CurrentDeviceName() const override;

  int32_t StartCapture(const VideoCaptureCapability& aCapability) override;
  virtual bool FocusOnSelectedSource() override;
  int32_t StopCapture() override;
  bool CaptureStarted() override;
  int32_t CaptureSettings(VideoCaptureCapability& aSettings) override;

  void CaptureFrameOnThread();
  mozilla::MediaEventSource<void>* CaptureEndedEvent();

  const int32_t mModuleId;
  const mozilla::TrackingId mTrackingId;
  const std::string mDeviceUniqueId;
  const mozilla::camera::CaptureDeviceType mDeviceType;

 protected:
  DesktopCaptureImpl(const int32_t aId, const char* aUniqueId,
                     const mozilla::camera::CaptureDeviceType aType);
  virtual ~DesktopCaptureImpl();

 private:
  // Maximum CPU usage in %.
  static constexpr uint32_t kMaxDesktopCaptureCpuUsage = 50;
  void InitOnThread(std::unique_ptr<DesktopCapturer> aCapturer, int aFramerate);
  void ShutdownOnThread();
  // DesktopCapturer::Callback interface.
  void OnCaptureResult(DesktopCapturer::Result aResult,
                       std::unique_ptr<DesktopFrame> aFrame) override;

  // Notifies all mCallbacks of OnFrame(). mCaptureThread only.
  void NotifyOnFrame(const VideoFrame& aFrame);

  // Control thread on which the public API is called.
  const nsCOMPtr<nsISerialEventTarget> mControlThread;
  // Set in StartCapture.
  mozilla::Maybe<VideoCaptureCapability> mRequestedCapability
      RTC_GUARDED_BY(mControlThreadChecker);
  // The DesktopCapturer is created on mControlThread but assigned and accessed
  // only on mCaptureThread.
  std::unique_ptr<DesktopCapturer> mCapturer
      RTC_GUARDED_BY(mCaptureThreadChecker);
  // Dedicated thread that does the capturing.
  nsCOMPtr<nsIThread> mCaptureThread RTC_GUARDED_BY(mControlThreadChecker);
  // Checks that API methods are called on mControlThread.
  webrtc::SequenceChecker mControlThreadChecker;
  // Checks that frame delivery only happens on mCaptureThread.
  webrtc::SequenceChecker mCaptureThreadChecker;
  // Timer that triggers frame captures. Only used on mCaptureThread.
  // TODO(Bug 1806646): Drive capture with vsync instead.
  nsCOMPtr<nsITimer> mCaptureTimer RTC_GUARDED_BY(mCaptureThreadChecker);
  // Interval between captured frames, based on the framerate in
  // mRequestedCapability. mCaptureThread only.
  mozilla::Maybe<mozilla::TimeDuration> mRequestedCaptureInterval
      RTC_GUARDED_BY(mCaptureThreadChecker);
  // Used to make sure incoming timestamp is increasing for every frame.
  webrtc::Timestamp mNextFrameMinimumTime RTC_GUARDED_BY(mCaptureThreadChecker);
  // Callbacks for captured frames. Mutated on mControlThread, callbacks happen
  // on mCaptureThread.
  mozilla::DataMutex<std::set<rtc::VideoSinkInterface<VideoFrame>*>> mCallbacks;
  // Subscribers to this event will be notified when the capture has ended.
  mozilla::MediaEventProducer<void> mCaptureEndedEvent;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_MAIN_SOURCE_DESKTOP_CAPTURE_IMPL_H_
