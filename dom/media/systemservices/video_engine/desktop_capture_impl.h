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

#include "api/video/video_frame.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_config.h"
#include "modules/video_coding/event_wrapper.h"
#include "modules/desktop_capture/shared_memory.h"
#include "modules/desktop_capture/desktop_and_cursor_composer.h"
#include "rtc_base/deprecated/recursive_critical_section.h"

#include "desktop_device_info.h"
#include "VideoEngine.h"

#if !defined(_WIN32)
#  include "rtc_base/platform_thread.h"
#endif

using namespace webrtc::videocapturemodule;
using namespace mozilla::camera;  // for mozilla::camera::CaptureDeviceType

namespace rtc {
#if defined(_WIN32)
class PlatformUIThread;
#endif
}  // namespace rtc

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

  // Call backs
  void RegisterCaptureDataCallback(
      rtc::VideoSinkInterface<VideoFrame>* aCallback) override;
  void DeRegisterCaptureDataCallback(
      rtc::VideoSinkInterface<VideoFrame>* aCallback) override;
  int32_t StopCaptureIfAllClientsClose() override;

  int32_t SetCaptureRotation(VideoRotation aRotation) override;
  bool SetApplyRotation(bool aEnable) override;
  bool GetApplyRotation() override { return true; }

  const char* CurrentDeviceName() const override;

  int32_t IncomingFrame(uint8_t* videoFrame, size_t videoFrameLength,
                        size_t widthWithPadding,
                        const VideoCaptureCapability& frameInfo);

  // Platform dependent
  int32_t StartCapture(const VideoCaptureCapability& aCapability) override;
  virtual bool FocusOnSelectedSource() override;
  int32_t StopCapture() override;
  bool CaptureStarted() override;
  int32_t CaptureSettings(VideoCaptureCapability& aSettings) override;

 protected:
  DesktopCaptureImpl(const int32_t aId, const char* aUniqueId,
                     const mozilla::camera::CaptureDeviceType aType);
  virtual ~DesktopCaptureImpl();
  int32_t DeliverCapturedFrame(webrtc::VideoFrame& aCaptureFrame);

  static const uint32_t kMaxDesktopCaptureCpuUsage =
      50;  // maximum CPU usage in %

  int32_t mModuleId;  // Module ID
  const mozilla::TrackingId
      mTrackingId;              // Allows tracking of this video frame source
  std::string mDeviceUniqueId;  // current Device unique name;
  CaptureDeviceType mDeviceType;

 private:
  void LazyInitCaptureThread();
  int32_t LazyInitDesktopCapturer();

  // DesktopCapturer::Callback interface.
  void OnCaptureResult(DesktopCapturer::Result result,
                       std::unique_ptr<DesktopFrame> frame) override;

 public:
  static void Run(void* aObj) {
    static_cast<DesktopCaptureImpl*>(aObj)->process();
  };
  void process();
  void ProcessIter();

 private:
  rtc::RecursiveCriticalSection mApiCs;
  std::atomic<uint32_t> mMaxFPSNeeded = {0};
  // Set in StartCapture.
  VideoCaptureCapability mRequestedCapability;
  // This is created on the main thread and accessed on both the main thread
  // and the capturer thread. It is created prior to the capturer thread
  // starting and is destroyed after it is stopped.
  std::unique_ptr<DesktopCapturer> mCapturer;
  bool mCapturerStarted = false;
  std::unique_ptr<EventWrapper> mTimeEvent;
#if defined(_WIN32)
  std::unique_ptr<rtc::PlatformUIThread> mCaptureThread;
#else
  std::unique_ptr<rtc::PlatformThread> mCaptureThread;
#endif
  // Used to make sure incoming timestamp is increasing for every frame.
  int64_t mLastFrameTimeMs;
  // True once fully started and not fully stopped.
  std::atomic<bool> mRunning;
  // Callbacks for captured frames.
  mozilla::DataMutex<std::set<rtc::VideoSinkInterface<VideoFrame>*>> mCallbacks;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_MAIN_SOURCE_DESKTOP_CAPTURE_IMPL_H_
