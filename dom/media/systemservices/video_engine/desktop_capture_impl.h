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

#include "api/sequence_checker.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/video_capture/video_capture.h"

#include "desktop_device_info.h"
#include "mozilla/DataMutex.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "nsCOMPtr.h"
#include "PerformanceRecorder.h"

class nsIThread;
class nsITimer;

namespace mozilla::camera {
enum class CaptureDeviceType;
}

namespace webrtc {

class VideoCaptureEncodeInterface;

// simulate deviceInfo interface for video engine, bridge screen/application and
// real screen/application device info

class ScreenDeviceInfoImpl : public VideoCaptureModule::DeviceInfo {
 public:
  ScreenDeviceInfoImpl(int32_t aId) : mId(aId) {}
  virtual ~ScreenDeviceInfoImpl() = default;

  int32_t Init();
  int32_t Refresh();

  virtual uint32_t NumberOfDevices();
  virtual int32_t GetDeviceName(uint32_t aDeviceNumber, char* aDeviceNameUTF8,
                                uint32_t aDeviceNameUTF8Size,
                                char* aDeviceUniqueIdUTF8,
                                uint32_t aDeviceUniqueIdUTF8Size,
                                char* aProductUniqueIdUTF8,
                                uint32_t aProductUniqueIdUTF8Size, pid_t* aPid);

  virtual int32_t DisplayCaptureSettingsDialogBox(
      const char* aDeviceUniqueIdUTF8, const char* aDialogTitleUTF8,
      void* aParentWindow, uint32_t aPositionX, uint32_t aPositionY);
  virtual int32_t NumberOfCapabilities(const char* aDeviceUniqueIdUTF8);
  virtual int32_t GetCapability(const char* aDeviceUniqueIdUTF8,
                                uint32_t aDeviceCapabilityNumber,
                                VideoCaptureCapability& aCapability);

  virtual int32_t GetBestMatchedCapability(
      const char* aDeviceUniqueIdUTF8, const VideoCaptureCapability& aRequested,
      VideoCaptureCapability& aResulting);
  virtual int32_t GetOrientation(const char* aDeviceUniqueIdUTF8,
                                 VideoRotation& aOrientation);

 protected:
  int32_t mId;
  std::unique_ptr<DesktopDeviceInfo> mDesktopDeviceInfo;
};

class WindowDeviceInfoImpl : public VideoCaptureModule::DeviceInfo {
 public:
  WindowDeviceInfoImpl(int32_t aId) : mId(aId){};
  virtual ~WindowDeviceInfoImpl() = default;

  int32_t Init();
  int32_t Refresh();

  virtual uint32_t NumberOfDevices();
  virtual int32_t GetDeviceName(uint32_t aDeviceNumber, char* aDeviceNameUTF8,
                                uint32_t aDeviceNameUTF8Size,
                                char* aDeviceUniqueIdUTF8,
                                uint32_t aDeviceUniqueIdUTF8Size,
                                char* aProductUniqueIdUTF8,
                                uint32_t aProductUniqueIdUTF8Size, pid_t* aPid);

  virtual int32_t DisplayCaptureSettingsDialogBox(
      const char* aDeviceUniqueIdUTF8, const char* aDialogTitleUTF8,
      void* aParentWindow, uint32_t aPositionX, uint32_t aPositionY);
  virtual int32_t NumberOfCapabilities(const char* aDeviceUniqueIdUTF8);
  virtual int32_t GetCapability(const char* aDeviceUniqueIdUTF8,
                                uint32_t aDeviceCapabilityNumber,
                                VideoCaptureCapability& aCapability);

  virtual int32_t GetBestMatchedCapability(
      const char* aDeviceUniqueIdUTF8, const VideoCaptureCapability& aRequested,
      VideoCaptureCapability& aResulting);
  virtual int32_t GetOrientation(const char* aDeviceUniqueIdUTF8,
                                 VideoRotation& aOrientation);

 protected:
  int32_t mId;
  std::unique_ptr<DesktopDeviceInfo> mDesktopDeviceInfo;
};

class BrowserDeviceInfoImpl : public VideoCaptureModule::DeviceInfo {
 public:
  BrowserDeviceInfoImpl(int32_t aId) : mId(aId){};
  virtual ~BrowserDeviceInfoImpl() = default;

  int32_t Init();
  int32_t Refresh();

  virtual uint32_t NumberOfDevices();
  virtual int32_t GetDeviceName(uint32_t aDeviceNumber, char* aDeviceNameUTF8,
                                uint32_t aDeviceNameUTF8Size,
                                char* aDeviceUniqueIdUTF8,
                                uint32_t aDeviceUniqueIdUTF8Size,
                                char* aProductUniqueIdUTF8,
                                uint32_t aProductUniqueIdUTF8Size, pid_t* aPid);

  virtual int32_t DisplayCaptureSettingsDialogBox(
      const char* aDeviceUniqueIdUTF8, const char* aDialogTitleUTF8,
      void* aParentWindow, uint32_t aPositionX, uint32_t aPositionY);
  virtual int32_t NumberOfCapabilities(const char* aDeviceUniqueIdUTF8);
  virtual int32_t GetCapability(const char* aDeviceUniqueIdUTF8,
                                uint32_t aDeviceCapabilityNumber,
                                VideoCaptureCapability& aCapability);

  virtual int32_t GetBestMatchedCapability(
      const char* aDeviceUniqueIdUTF8, const VideoCaptureCapability& aRequested,
      VideoCaptureCapability& aResulting);
  virtual int32_t GetOrientation(const char* aDeviceUniqueIdUTF8,
                                 VideoRotation& aOrientation);

 protected:
  int32_t mId;
  std::unique_ptr<DesktopDeviceInfo> mDesktopDeviceInfo;
};

// Reuses the video engine pipeline for screen sharing.
// As with video, DesktopCaptureImpl is a proxy for screen sharing
// and follows the video pipeline design
class DesktopCaptureImpl : public DesktopCapturer::Callback,
                           public VideoCaptureModule {
 public:
  /* Create a screen capture modules object
   */
  static VideoCaptureModule* Create(
      const int32_t aModuleId, const char* aUniqueId,
      const mozilla::camera::CaptureDeviceType aType);

  [[nodiscard]] static std::shared_ptr<VideoCaptureModule::DeviceInfo>
  CreateDeviceInfo(const int32_t aId,
                   const mozilla::camera::CaptureDeviceType aType);

  // mControlThread only.
  void RegisterCaptureDataCallback(
      rtc::VideoSinkInterface<VideoFrame>* aCallback) override;
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
  int32_t EnsureCapturer();
  void InitOnThread(int aFramerate);
  void ShutdownOnThread();
  // DesktopCapturer::Callback interface.
  void OnCaptureResult(DesktopCapturer::Result aResult,
                       std::unique_ptr<DesktopFrame> aFrame) override;

  // Notifies all mCallbacks of OnFrame(). mCaptureThread only.
  void NotifyOnFrame(const VideoFrame& aFrame);

  // Control thread on which the public API is called.
  const nsCOMPtr<nsISerialEventTarget> mControlThread;
  // Set in StartCapture. mControlThread only.
  VideoCaptureCapability mRequestedCapability;
  // This is created on mControlThread and accessed on both mControlThread and
  // mCaptureThread. It is created prior to mCaptureThread starting and is
  // destroyed after it is stopped.
  std::unique_ptr<DesktopCapturer> mCapturer;
  // Dedicated thread that does the capturing. Only used on mControlThread.
  nsCOMPtr<nsIThread> mCaptureThread;
  // Checks that frame delivery only happens on mCaptureThread.
  webrtc::SequenceChecker mCaptureThreadChecker;
  // Timer that triggers frame captures. Only used on mCaptureThread.
  // TODO(Bug 1806646): Drive capture with vsync instead.
  nsCOMPtr<nsITimer> mCaptureTimer;
  // Interval between captured frames, based on the framerate in
  // mRequestedCapability. mCaptureThread only.
  mozilla::Maybe<mozilla::TimeDuration> mRequestedCaptureInterval;
  // Used to make sure incoming timestamp is increasing for every frame.
  // mCaptureThread only.
  webrtc::Timestamp mNextFrameMinimumTime;
  // True once fully started and not fully stopped. Only accessed on
  // mControlThread.
  bool mRunning;
  // Callbacks for captured frames. Mutated on mControlThread, callbacks happen
  // on mCaptureThread.
  mozilla::DataMutex<std::set<rtc::VideoSinkInterface<VideoFrame>*>> mCallbacks;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_MAIN_SOURCE_DESKTOP_CAPTURE_IMPL_H_
